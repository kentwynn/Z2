bool initAudio()
{
    const i2s_config_t config = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = kSpeakerSampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        // WebSocket/TLS sends can briefly occupy the application loop. Keep
        // enough RX DMA history that those network bursts cannot drop a word
        // from the INMP441 stream before updateMicrophone() reads it.
        .dma_buf_count = 12,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
    };
    const i2s_pin_config_t pins = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = kSpeakerBclkPin,
        .ws_io_num = kSpeakerWsPin,
        .data_out_num = kSpeakerDataPin,
        .data_in_num = kMicrophoneDataPin,
    };

    if (i2s_driver_install(kSpeakerI2sPort, &config, 0, nullptr) != ESP_OK) return false;
    if (i2s_set_pin(kSpeakerI2sPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kSpeakerI2sPort);
        return false;
    }
    i2s_zero_dma_buffer(kSpeakerI2sPort);
    gVoiceVad = WebRtcVad_Create();
    gVoiceVadReady = gVoiceVad && WebRtcVad_Init(gVoiceVad) == 0 &&
                     WebRtcVad_set_mode(gVoiceVad, 3) == 0;
    Serial.printf("[Z2 MIC] WebRTC VAD %s mode=3 frame=10ms\n",
                  gVoiceVadReady ? "ready" : "unavailable");
    return true;
}

void resetVoiceVad()
{
    gVoiceVadFrameSamples = 0;
    if (gVoiceVadReady) {
        WebRtcVad_Init(gVoiceVad);
        WebRtcVad_set_mode(gVoiceVad, 3);
    }
}

void updateMicrophone()
{
    if (!gMicrophoneReady || gRealtimeSpeakerActive || gPhoneSessionActive ||
        millis() - gLastMicrophoneReadMs < 10) return;
    gLastMicrophoneReadMs = millis();

    // Handling exposed hardware is a safety condition. Do not interpret
    // chassis/wire contact noise as a wake utterance while caution is active.
    if (gProximityHazardActive) {
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        return;
    }

    int32_t samples[256 * 2];
    size_t bytesRead = 0;
    if (i2s_read(kSpeakerI2sPort, samples, sizeof(samples), &bytesRead, 0) != ESP_OK ||
        bytesRead == 0) return;

    // Drain but ignore the amplifier's acoustic/electrical tail after TTS.
    // Without this guard, the robot can transcribe its own final word and
    // immediately start another turn.
    if (static_cast<int32_t>(gRealtimeMicResumeAtMs - millis()) > 0) {
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        return;
    }

    const size_t sampleCount = bytesRead / sizeof(samples[0]);
    if (sampleCount < 4) return;

    // L/R is grounded, so INMP441 data is always in the left I2S slot. With
    // this ESP32-S3/core combination exposes that slot at DMA index 0. Do not select
    // the louder slot dynamically: the unused slot can contain digital noise
    // and switching slots corrupts the PCM stream sent to Whisper.
    uint32_t channelRms[2] = {0, 0};
    uint32_t channelPeak[2] = {0, 0};
    for (size_t channel = 0; channel < 2; ++channel) {
        double sum = 0.0;
        size_t count = 0;
        for (size_t i = channel; i < sampleCount; i += 2) {
            sum += samples[i] >> 8;
            ++count;
        }
        const double mean = count ? sum / count : 0.0;
        double squaredSum = 0.0;
        for (size_t i = channel; i < sampleCount; i += 2) {
            const int32_t normalized = samples[i] >> 8;
            const double centered = normalized - mean;
            squaredSum += centered * centered;
            const uint32_t magnitude = static_cast<uint32_t>(abs(normalized));
            if (magnitude > channelPeak[channel]) channelPeak[channel] = magnitude;
        }
        if (count) channelRms[channel] = static_cast<uint32_t>(sqrt(squaredSum / count));
    }
    constexpr size_t activeChannel = 0;
    gMicrophoneRms = channelRms[activeChannel];
    gMicrophonePeak = channelPeak[activeChannel];

    if (gMicrophoneCalibrationFrames < kMicrophoneCalibrationFrameCount) {
        gMicrophoneCalibrationSum += gMicrophoneRms;
        ++gMicrophoneCalibrationFrames;
        if (gMicrophoneCalibrationFrames == kMicrophoneCalibrationFrameCount) {
            gMicrophoneFloor = static_cast<uint32_t>(
                gMicrophoneCalibrationSum / kMicrophoneCalibrationFrameCount);
        }
        return;
    }

    bool vadSpeech = false;
    bool vadDecisionReady = false;
    if (gVoiceVadReady) {
        for (size_t i = activeChannel; i < sampleCount; i += 2) {
            const int32_t raw = samples[i] >> 16;
            gVoiceVadFrame[gVoiceVadFrameSamples++] =
                static_cast<int16_t>(constrain(
                    raw * Z2_MIC_PCM_GAIN, -32768L, 32767L));
            if (gVoiceVadFrameSamples == 160) {
                const int decision = WebRtcVad_Process(
                    gVoiceVad, Z2_INPUT_AUDIO_SAMPLE_RATE,
                    gVoiceVadFrame, gVoiceVadFrameSamples);
                if (decision >= 0) {
                    vadDecisionReady = true;
                    vadSpeech = vadSpeech || decision == 1;
                }
                gVoiceVadFrameSamples = 0;
            }
        }
    }

    const uint32_t speechOn = voiceSpeechOnThreshold();
    const uint32_t speechOff = max<uint32_t>(
        (kVoiceMinimumRms * 3U) / 5U,
        (gMicrophoneFloor * 4U) / 3U);
    const unsigned long now = millis();
    const bool voiceWasDetected = gVoiceDetected;
    // Idle wake listening must use the strict adaptive energy threshold as
    // well as WebRTC VAD. The microphone's sustained room/electrical noise can
    // sit around 28-35k RMS and WebRTC occasionally labels it as speech; the
    // former 1.2x-floor gate therefore opened empty four-second wake turns.
    // Once a wake has already been confirmed, retain the more permissive gate
    // so quiet first syllables of the actual request are not clipped.
    const uint32_t followUpEnergyGate = max<uint32_t>(
        (kVoiceMinimumRms * 3U) / 5U,
        (gMicrophoneFloor * 6U) / 5U);
    const uint32_t vadEnergyGate = gRealtimeFollowUpAttack
        ? followUpEnergyGate
        : speechOn;
    gVoiceVadEnergyGate = vadEnergyGate;
    const bool speechFrame = gVoiceVadReady
        ? (vadDecisionReady && vadSpeech && gMicrophoneRms >= vadEnergyGate)
        : gMicrophoneRms >= speechOn;
    const uint8_t requiredAttackFrames =
        gRealtimeFollowUpAttack
            ? kFollowUpVoiceAttackRequiredFrames
            : kVoiceAttackRequiredFrames;
    if (speechFrame) {
        if (gVoiceAttackFrames < requiredAttackFrames) ++gVoiceAttackFrames;
        if (gVoiceAttackFrames >= requiredAttackFrames) {
            gVoiceDetected = true;
            gLastVoiceMs = now;
        }
    } else {
        gVoiceAttackFrames = 0;
        if (!gVoiceVadReady && gVoiceDetected && gMicrophoneRms >= speechOff)
            gLastVoiceMs = now;
        if (gVoiceDetected && now - gLastVoiceMs >= kVoiceHangoverMs) gVoiceDetected = false;
    }
    if (!gVoiceDetected && gMicrophoneRms < speechOn) {
        gMicrophoneFloor = (gMicrophoneFloor * 63U + gMicrophoneRms) / 64U;
    }
    if (!voiceWasDetected && gVoiceDetected) {
        addRealtimeLog(String("Local speech detected: RMS ") + gMicrophoneRms +
                       " · floor " + gMicrophoneFloor +
                       " · gate " + gVoiceVadEnergyGate +
                       " · WebRTC");
    }
    handleRealtimeMicrophoneFrame(samples, sampleCount, activeChannel);
}

void playRealtimePcm(const uint8_t *pcm, size_t length)
{
    if (!gSpeakerReady || !pcm || length < 2) return;
    int32_t stereoSamples[256 * 2];
    size_t offset = 0;
    while (offset + 1 < length) {
        const size_t sampleCount = min<size_t>(256, (length - offset) / 2);
        for (size_t i = 0; i < sampleCount; ++i) {
            const int16_t sample = static_cast<int16_t>(
                pcm[offset + i * 2] | (pcm[offset + i * 2 + 1] << 8));
            const int32_t scaled =
                (static_cast<int32_t>(sample) * gSpeakerVolumePercent) / 100;
            const int32_t aligned = scaled * 65536;
            stereoSamples[i * 2] = aligned;
            stereoSamples[i * 2 + 1] = aligned;
        }
        size_t written = 0;
        i2s_write(kSpeakerI2sPort, stereoSamples, sampleCount * 8,
                  &written, pdMS_TO_TICKS(100));
        offset += sampleCount * 2;
    }
}

void outputTone(uint16_t frequencyHz, uint16_t durationMs)
{
    if (!gSpeakerReady) return;
    int32_t samples[256 * 2];
    float phase = 0.0f;
    const float step = 2.0f * M_PI * frequencyHz / kSpeakerSampleRate;
    const unsigned long untilMs = millis() + durationMs;

    while (millis() < untilMs) {
        for (size_t i = 0; i < 256; ++i) {
            const int16_t sample = frequencyHz
                ? static_cast<int16_t>(
                    sinf(phase) * 15000 * gSpeakerVolumePercent / 100)
                : 0;
            const int32_t alignedSample = static_cast<int32_t>(sample) * 65536;
            samples[i * 2] = alignedSample;
            samples[i * 2 + 1] = alignedSample;
            phase += step;
            if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        size_t written = 0;
        i2s_write(kSpeakerI2sPort, samples, sizeof(samples), &written, pdMS_TO_TICKS(100));
    }
}

uint32_t readLe32(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

void playVoiceWav(const uint8_t *wav, size_t wavLength)
{
    size_t offset = 12;
    size_t dataOffset = 0;
    size_t dataBytes = 0;
    while (offset + 8 <= wavLength) {
        const uint32_t chunkBytes = readLe32(wav + offset + 4);
        if (memcmp(wav + offset, "data", 4) == 0) {
            dataOffset = offset + 8;
            dataBytes = min<size_t>(chunkBytes, wavLength - dataOffset);
            break;
        }
        offset += 8 + chunkBytes + (chunkBytes & 1U);
    }

    int32_t stereoSamples[256 * 2];
    for (size_t position = 0; dataOffset && position + 1 < dataBytes; position += 512) {
        const size_t monoBytes = min<size_t>(512, dataBytes - position);
        const size_t sampleCount = monoBytes / 2;
        for (size_t i = 0; i < sampleCount; ++i) {
            const size_t at = dataOffset + position + i * 2;
            const int16_t sample = static_cast<int16_t>(
                wav[at] | (wav[at + 1] << 8));
            const int32_t scaledSample =
                (static_cast<int32_t>(sample) * gSpeakerVolumePercent) / 100;
            const int32_t alignedSample = scaledSample * 65536;
            stereoSamples[i * 2] = alignedSample;
            stereoSamples[i * 2 + 1] = alignedSample;
        }
        size_t written = 0;
        i2s_write(kSpeakerI2sPort, stereoSamples, sampleCount * 8,
                  &written, pdMS_TO_TICKS(100));
    }
}

void playHelloVoice()
{
    outputTone(1050, 55);
    outputTone(1450, 65);
    outputTone(0, 35);
    playVoiceWav(kZ2HelloWav, kZ2HelloWav_len);
}

void playWakeAcknowledgement()
{
    // Instant local feedback: short enough not to delay the user's question.
    outputTone(1050, 45);
    outputTone(0, 20);
    outputTone(1450, 55);
    i2s_zero_dma_buffer(kSpeakerI2sPort);
}

void playWakeRejection()
{
    outputTone(850, 55);
    outputTone(0, 20);
    outputTone(520, 70);
    i2s_zero_dma_buffer(kSpeakerI2sPort);
}

void playSound(const String &requestedName)
{
    String name = requestedName;
    name.toUpperCase();
    // Presentation audio must not own locomotion. Safety recovery and control
    // cues are allowed to play while the motor state machine remains active;
    // callers that explicitly need a stop already do so before requesting
    // audio (for example the local /sound endpoint).

    if (name == "HELLO") {
        playHelloVoice();
    } else if (name == "BEEP") {
        outputTone(700, 220);
    } else if (name == "DOUBLE") {
        outputTone(750, 150); outputTone(0, 100); outputTone(750, 150);
    } else if (name == "CHIRP") {
        for (uint16_t frequency = 350; frequency <= 1400; frequency += 150) outputTone(frequency, 45);
    } else if (name == "HELP") {
        for (uint8_t group = 0; group < 2; ++group) {
            for (uint8_t beep = 0; beep < 3; ++beep) {
                outputTone(group ? 880 : 660, group ? 220 : 100);
                outputTone(0, 80);
            }
            outputTone(0, 140);
        }
    } else if (name == "ALARM") {
        for (uint8_t repeat = 0; repeat < 3; ++repeat) {
            outputTone(520, 180); outputTone(880, 180);
        }
    }
    i2s_zero_dma_buffer(kSpeakerI2sPort);
    Serial.printf("[Z2 SPEAKER] sound=%s\n", name.c_str());
}

void playCollisionWarning()
{
    gCollisionWarningLatched = true;
    if (gCollisionSound == "OFF") return;
    if (gCollisionSound == "DOUBLE") {
        outputTone(750, 150); outputTone(0, 100); outputTone(750, 150);
    } else if (gCollisionSound == "CHIRP") {
        for (uint16_t frequency = 350; frequency <= 1400; frequency += 150)
            outputTone(frequency, 45);
    } else if (gCollisionSound == "ALARM") {
        for (uint8_t repeat = 0; repeat < 2; ++repeat) {
            outputTone(520, 180); outputTone(880, 180);
        }
    } else {
        outputTone(1150, 90); outputTone(0, 40); outputTone(850, 130);
    }
    i2s_zero_dma_buffer(kSpeakerI2sPort);
}

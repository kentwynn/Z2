enum class RealtimeChatState : uint8_t {
    WakeListening, Connecting, Recording, Waiting, Speaking, FollowUp
};

websockets::WebsocketsClient gRealtimeSocket;
constexpr char kKentWynnRootCa[] PROGMEM = R"CERT(-----BEGIN CERTIFICATE-----
MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG
A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv
b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw
MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i
YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT
aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ
jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp
xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp
1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG
snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ
U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8
9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E
BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B
AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz
yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE
38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP
AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad
DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME
HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==
-----END CERTIFICATE-----
)CERT";
RealtimeChatState gRealtimeState = RealtimeChatState::WakeListening;
String gRealtimeHeaders;
uint8_t *gWakeAudio = nullptr;
size_t gWakeAudioCapacity = 0;
size_t gWakeAudioWrite = 0;
size_t gWakeAudioSize = 0;
bool gWakeAudioCaptureActive = false;
unsigned long gRealtimeRecordingStartedMs = 0;
unsigned long gRealtimeLastSpeechMs = 0;
unsigned long gRealtimeFollowUpStartedMs = 0;
unsigned long gRealtimeConnectingStartedMs = 0;
bool gRealtimePreviousVoice = false;
bool gRealtimeSessionPending = false;
bool gRealtimeVoicePending = false;
bool gRealtimeStartRecordingPending = false;
bool gRealtimeExpectedClose = false;
bool gRealtimeRecordingIsWake = false;
bool gRealtimeWakeAcknowledgementPending = false;
bool gRealtimeWakeRejectionPending = false;
unsigned long gAiMotionStopAtMs = 0;
String gRealtimeReplyEmotion = "HAPPY";
uint8_t gRealtimeSpeechFrames = 0;
size_t gRealtimeReplyAudioBytes = 0;
unsigned long gRealtimeLastConnectAttemptMs = 0;
unsigned long gRealtimeLastKeepaliveMs = 0;
unsigned long gRealtimeLastPingMs = 0;
bool gRealtimeFollowUpNeedsSilence = false;
unsigned long gRealtimeFollowUpSilenceStartedMs = 0;
struct RealtimePresentationNote {
    uint16_t frequencyHz = 0;
    uint16_t durationMs = 0;
    uint16_t gapMs = 0;
};
RealtimePresentationNote gRealtimePresentationNotes[4];
volatile uint8_t gRealtimePresentationNoteCount = 0;
volatile bool gRealtimePresentationSoundReady = false;
uint8_t gRealtimePresentationSoundVolume = 0;
constexpr uint8_t kRealtimeLogCount = 16;
String gRealtimeLog[kRealtimeLogCount];
uint8_t gRealtimeLogWrite = 0;
uint8_t gRealtimeLogSize = 0;
constexpr uint8_t kCloudDiagnosticQueueCount = 16;
String gCloudDiagnosticQueue[kCloudDiagnosticQueueCount];
uint8_t gCloudDiagnosticQueueHead = 0;
uint8_t gCloudDiagnosticQueueSize = 0;

void setRealtimeState(RealtimeChatState state, const char *reason);

void realtimePlaybackTask(void *)
{
    constexpr size_t kPlaybackPrebufferBytes = 12000; // 250 ms at 24 kHz PCM16
    for (;;) {
        if (!gRealtimeSpeakerActive || !gRealtimeAudioBuffer) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        const size_t buffered = gRealtimeAudioBufferCapacity -
            xRingbufferGetCurFreeSize(gRealtimeAudioBuffer);
        if (buffered < kPlaybackPrebufferBytes && !gRealtimeAudioEnded) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        size_t received = 0;
        void *item = xRingbufferReceiveUpTo(
            gRealtimeAudioBuffer, &received, pdMS_TO_TICKS(20), 4096);
        if (item && received) {
            playRealtimePcm(static_cast<const uint8_t *>(item), received);
            vRingbufferReturnItem(gRealtimeAudioBuffer, item);
            continue;
        }

        if (gRealtimeAudioEnded &&
            xRingbufferGetCurFreeSize(gRealtimeAudioBuffer) ==
                gRealtimeAudioBufferCapacity) {
            i2s_set_clk(kSpeakerI2sPort, Z2_INPUT_AUDIO_SAMPLE_RATE,
                        I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
            i2s_zero_dma_buffer(kSpeakerI2sPort);
            gRealtimeSpeakerActive = false;
            gRealtimeAudioEnded = false;
            gRealtimeMicResumeAtMs = millis() + kPostPlaybackMicGuardMs;
            gVoiceDetected = false;
            gVoiceAttackFrames = 0;
            gRealtimePreviousVoice = false;
            gWakeAudioSize = 0;
            gWakeAudioWrite = 0;
            gWakeAudioCaptureActive = false;
            gRealtimeFollowUpStartedMs = millis();
            setRealtimeState(RealtimeChatState::FollowUp,
                             "listening for follow-up");
        }
    }
}

void realtimePresentationSoundTask(void *)
{
    constexpr unsigned long kPresentationMicGuardMs = 150;
    for (;;) {
        if (!gRealtimePresentationSoundReady || gRealtimeSpeakerActive) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        const uint8_t queuedCount = gRealtimePresentationNoteCount;
        const uint8_t count = queuedCount > 4 ? 4 : queuedCount;
        const uint8_t volume = gRealtimePresentationSoundVolume;
        RealtimePresentationNote notes[4];
        unsigned long soundDurationMs = 0;
        for (uint8_t index = 0; index < count; ++index)
        {
            notes[index] = gRealtimePresentationNotes[index];
            soundDurationMs += notes[index].durationMs + notes[index].gapMs;
        }
        gRealtimePresentationSoundReady = false;
        gRealtimePresentationNoteCount = 0;

        // Do not let the microphone interpret the acknowledgement/rejection
        // notes as a new utterance. Clear both VAD and edge state before the
        // first tone, then rearm after the acoustic tail has drained.
        gRealtimeMicResumeAtMs =
            millis() + soundDurationMs + kPresentationMicGuardMs;
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        resetVoiceVad();
        gRealtimePreviousVoice = false;
        const uint8_t savedVolume = gSpeakerVolumePercent;
        gSpeakerVolumePercent = volume;
        for (uint8_t index = 0; index < count; ++index) {
            outputTone(notes[index].frequencyHz, notes[index].durationMs);
            if (notes[index].gapMs) outputTone(0, notes[index].gapMs);
        }
        gSpeakerVolumePercent = savedVolume;
        i2s_zero_dma_buffer(kSpeakerI2sPort);
        gRealtimeMicResumeAtMs = millis() + kPresentationMicGuardMs;
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        resetVoiceVad();
        gRealtimePreviousVoice = false;
        if (gRealtimeState == RealtimeChatState::FollowUp) {
            // The acknowledgement note itself is the ready cue. Do not make
            // the user provide another silence transition after hearing it.
            gRealtimeFollowUpNeedsSilence = false;
            gRealtimeFollowUpSilenceStartedMs = 0;
        }
    }
}

void startRealtimePlaybackTask()
{
    // TTS is generated faster than realtime and commonly arrives in bursts.
    // Keep enough PSRAM queued for a complete concise reply so the WebSocket
    // producer cannot overrun the 48 KB/s speaker consumer.
    constexpr size_t kAudioBufferBytes = 512 * 1024;
    static StaticRingbuffer_t ringBufferState;
    uint8_t *storage = static_cast<uint8_t *>(heap_caps_malloc(
        kAudioBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (storage)
        gRealtimeAudioBuffer = xRingbufferCreateStatic(
            kAudioBufferBytes, RINGBUF_TYPE_BYTEBUF, storage,
            &ringBufferState);
    if (!gRealtimeAudioBuffer) {
        Serial.println("[Z2 SPEAKER] PSRAM audio ring buffer allocation failed");
        return;
    }
    gRealtimeAudioBufferCapacity =
        xRingbufferGetCurFreeSize(gRealtimeAudioBuffer);
    xTaskCreatePinnedToCore(realtimePlaybackTask, "z2-audio", 4096, nullptr,
                            2, nullptr, 0);
    xTaskCreatePinnedToCore(realtimePresentationSoundTask, "z2-ai-sound", 3072,
                            nullptr, 1, nullptr, 0);
    Serial.printf("[Z2 SPEAKER] %u KB PSRAM realtime ring buffer ready\n",
                  static_cast<unsigned>(gRealtimeAudioBufferCapacity / 1024));
}

void addRealtimeLog(const String &message)
{
    const unsigned long seconds = millis() / 1000;
    gRealtimeLog[gRealtimeLogWrite] = String(seconds / 60) + ":" +
        (seconds % 60 < 10 ? "0" : "") + String(seconds % 60) + "  " + message;
    gRealtimeLogWrite = (gRealtimeLogWrite + 1) % kRealtimeLogCount;
    if (gRealtimeLogSize < kRealtimeLogCount) ++gRealtimeLogSize;
    if (gCloudDiagnosticQueueSize == kCloudDiagnosticQueueCount) {
        gCloudDiagnosticQueue[gCloudDiagnosticQueueHead] = "";
        gCloudDiagnosticQueueHead =
            (gCloudDiagnosticQueueHead + 1) % kCloudDiagnosticQueueCount;
        --gCloudDiagnosticQueueSize;
    }
    const uint8_t tail =
        (gCloudDiagnosticQueueHead + gCloudDiagnosticQueueSize) %
        kCloudDiagnosticQueueCount;
    gCloudDiagnosticQueue[tail] = message.substring(0, 220);
    ++gCloudDiagnosticQueueSize;
}

const char *realtimeStateName()
{
    switch (gRealtimeState) {
        case RealtimeChatState::Connecting: return "CONNECTING";
        case RealtimeChatState::Recording: return "RECORDING";
        case RealtimeChatState::Waiting: return "WAITING";
        case RealtimeChatState::Speaking: return "SPEAKING";
        case RealtimeChatState::FollowUp: return "FOLLOW_UP";
        default: return "WAKE_LISTENING";
    }
}

void setRealtimeState(RealtimeChatState state, const char *reason)
{
    gRealtimeState = state;
    gOledConsoleUntilMs = millis() + 15000UL;
    switch (state) {
    case RealtimeChatState::Connecting:
        gOledLiveStatus = "CONNECT";
        break;
    case RealtimeChatState::Recording:
        gOledLiveStatus = "LISTEN";
        gOledLiveDetail = "";
        gOledLastHeard = "";
        gOledLastReply = "";
        gOledReplyMood = "";
        break;
    case RealtimeChatState::Waiting:
        gOledLiveStatus = "THINK";
        break;
    case RealtimeChatState::Speaking:
        gOledLiveStatus = "SPEAK";
        break;
    case RealtimeChatState::FollowUp:
        gOledLiveStatus = "FOLLOW-UP";
        break;
    default:
        gOledLiveStatus = "READY";
        clearVoiceRingOverlay();
        break;
    }
    gConversationListeningVisual =
        state == RealtimeChatState::Recording || state == RealtimeChatState::FollowUp;
    gConversationThinkingVisual = state == RealtimeChatState::Waiting;
    gConversationSpeakingVisual = state == RealtimeChatState::Speaking;
    // Voice-chat presentation is supplied by Qwen's reply.presentation.
    // Legacy local presets must not override the AI-authored plan.
    clearPresentation(PresentationOwner::Voice);
    clearPresentation(PresentationOwner::System);
    Serial.printf("[Z2 CHAT] state=%s reason=%s\n", realtimeStateName(), reason);
    addRealtimeLog(String(realtimeStateName()) + " - " + reason);
}

void closeRealtimeConversation(const char *reason)
{
    setRealtimeState(RealtimeChatState::WakeListening, reason);
    gRealtimeSocket.close();
    if (gRealtimeSpeakerActive) {
        i2s_set_clk(kSpeakerI2sPort, Z2_INPUT_AUDIO_SAMPLE_RATE,
                    I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
        gRealtimeSpeakerActive = false;
    }
    gWakeAudioSize = 0;
    gWakeAudioWrite = 0;
    gWakeAudioCaptureActive = false;
    gRealtimeSessionPending = false;
    gRealtimeVoicePending = false;
    gRealtimeStartRecordingPending = false;
    gRealtimeSpeechFrames = 0;
    gRealtimePresentationSoundReady = false;
    gRealtimePresentationNoteCount = 0;
    // Leave a short socket-free maintenance window for configuration/status
    // HTTPS before the proven preconnected voice channel is re-armed.
    gRealtimeLastConnectAttemptMs = millis();
}

bool controlLeaseActive()
{
    return gControlLeaseUntilMs != 0 &&
        static_cast<int32_t>(gControlLeaseUntilMs - millis()) > 0;
}

void activateControlLease(bool stopping)
{
    const uint16_t seconds = stopping
        ? min<uint16_t>(5, gControlVoiceSuspendSeconds)
        : gControlVoiceSuspendSeconds;
    const bool wasActive = controlLeaseActive();
    gControlLeaseUntilMs = millis() + static_cast<unsigned long>(seconds) * 1000UL;
    if (gRealtimeSocket.available() ||
        gRealtimeState != RealtimeChatState::WakeListening)
        closeRealtimeConversation("Control owns execution");
    if (!wasActive) {
        Serial.printf("[Z2 CONTROL] voice suspended for %u seconds\n", seconds);
        addRealtimeLog(String("CONTROL_ACTIVE - voice suspended for ") + seconds + "s");
    }
}

void sendRealtimeSession()
{
    // Keep the first frame below 126 bytes. The ESP32 WebSockets build used by
    // this board produces an invalid extended-length client frame; server
    // defaults already select PCM16 16 kHz input, PCM16 24 kHz output, and a
    // 20-second follow-up timeout.
    // The scoped credential identifies the robot. The server applies every
    // conversational setting from the owner's current API configuration.
    const String payload = String("{\"type\":\"session.start\",\"robot_id\":\"") +
                           Z2_ROBOT_ID + "\"}";
    Serial.printf("[Z2 CHAT] sending session.start bytes=%u\n",
                  static_cast<unsigned>(payload.length()));
    const bool sent = gRealtimeSocket.send(payload.c_str(), payload.length());
    Serial.printf("[Z2 CHAT] session.start sent=%s\n", sent ? "yes" : "no");
}

bool sendRealtimeBinary(const uint8_t *data, size_t length)
{
    // ArduinoWebsockets 0.5.4 produces an invalid extended-length frame on
    // this ESP32/core combination. Keep every PCM frame below 126 bytes.
    constexpr size_t kSafeBinaryFrameBytes = 120;
    bool sent = true;
    for (size_t offset = 0; offset < length; offset += kSafeBinaryFrameBytes) {
        const size_t chunk = min(kSafeBinaryFrameBytes, length - offset);
        sent = gRealtimeSocket.sendBinary(
            reinterpret_cast<const char *>(data + offset), chunk) && sent;
        if ((offset / kSafeBinaryFrameBytes) % 32U == 31U) delay(1);
    }
    return sent;
}

void sendWakeAudio()
{
    if (!gWakeAudio || !gWakeAudioSize) return;
    const size_t start = (gWakeAudioWrite + gWakeAudioCapacity - gWakeAudioSize) %
                         gWakeAudioCapacity;
    const size_t first = min(gWakeAudioSize, gWakeAudioCapacity - start);
    sendRealtimeBinary(gWakeAudio + start, first);
    if (first < gWakeAudioSize)
        sendRealtimeBinary(gWakeAudio, gWakeAudioSize - first);
    gWakeAudioSize = 0;
    gWakeAudioWrite = 0;
    gWakeAudioCaptureActive = false;
}

void startRealtimeRecording(bool includeWakeBuffer)
{
    // Capture which phase opened the recorder before changing the state.
    // Wake phrases are short and must not inherit the much longer request cap.
    gRealtimeRecordingIsWake =
        gRealtimeState == RealtimeChatState::WakeListening;
    gRealtimeSocket.send("{\"type\":\"audio.start\"}");
    if (includeWakeBuffer) sendWakeAudio();
    gRealtimeRecordingStartedMs = millis();
    gRealtimeLastSpeechMs = millis();
    gRealtimeSpeechFrames = 0;
    setRealtimeState(RealtimeChatState::Recording, "audio started");
}

void cancelRealtimeRecordingForProximity()
{
    if (gRealtimeState != RealtimeChatState::Recording ||
        millis() - gRealtimeRecordingStartedMs > 1000UL) return;
    gRealtimeSocket.send("{\"type\":\"audio.cancel\"}");
    gWakeAudioSize = 0;
    gWakeAudioWrite = 0;
    gWakeAudioCaptureActive = false;
    gRealtimeSpeechFrames = 0;
    gRealtimePreviousVoice = false;
    setRealtimeState(RealtimeChatState::WakeListening,
                     "recording cancelled by proximity safety");
}

void handleRealtimeSocketEvent(websockets::WebsocketsEvent type, String detail)
{
    if (type == websockets::WebsocketsEvent::ConnectionOpened) {
        gRealtimeLastKeepaliveMs = millis();
        gRealtimeLastPingMs = gRealtimeLastKeepaliveMs;
        Serial.println("[Z2 CHAT] secure WebSocket connected");
        addRealtimeLog("API WebSocket connected securely");
        return;
    }
    if (type == websockets::WebsocketsEvent::ConnectionClosed) {
        const bool wasExpectedClose = gRealtimeExpectedClose;
        Serial.printf("[Z2 CHAT] disconnected detail=%s\n", detail.c_str());
        addRealtimeLog(wasExpectedClose
            ? "API session closed normally"
            : "API WebSocket disconnected");
        gRealtimeExpectedClose = false;
        if (gRealtimeState != RealtimeChatState::WakeListening)
            closeRealtimeConversation("WebSocket disconnected");
        // Unexpected transport loss should not create a five-second deaf
        // period. Reconnect on the next application-loop iteration.
        if (!wasExpectedClose && millis() >= 5000UL)
            gRealtimeLastConnectAttemptMs = millis() - 5000UL;
    }
}

void handleRealtimeMessage(websockets::WebsocketsMessage message)
{
    if (message.isBinary()) {
        if (gRealtimeSpeakerActive) {
            const bool firstAudio = gRealtimeReplyAudioBytes == 0;
            gRealtimeReplyAudioBytes += message.length();
            gOledReplyAudioBytes = gRealtimeReplyAudioBytes;
            if (firstAudio)
                addRealtimeLog(String("Speaker PCM received: ") +
                               message.length() + " bytes");
            if (!gRealtimeAudioBuffer ||
                xRingbufferSend(gRealtimeAudioBuffer, message.c_str(),
                                message.length(), 0) != pdTRUE)
                addRealtimeLog("Speaker PCM buffer overflow");
        }
        return;
    }
    if (!message.isText()) return;

    JsonDocument event;
    if (deserializeJson(event, message.c_str(), message.length())) return;
    const char *eventType = event["type"] | "";
    if (!strcmp(eventType, "device.command")) {
        String payload;
        serializeJson(event["command"], payload);
        enqueueCloudCommand(payload);
        const char *command = event["command"]["command"] | "unknown";
        const char *value = event["command"]["value"] | "";
        addRealtimeLog(String("Control queued [") + gCloudCommandQueueSize +
                       "]: " + command + (value[0] ? String(" ") + value : "") +
                       (gRobotConfigSynced ? "" : " · config sync pending"));
        return;
    }
    Serial.printf("[Z2 CHAT] event=%s\n", eventType);
    // Text arrives as many tiny streaming deltas. Processing them is required,
    // but logging each one pushes useful wake, transcript, audio, and error
    // events out of the dashboard's compact history.
    if (strcmp(eventType, "reply.text.delta"))
        addRealtimeLog(String("API event: ") + eventType);
    if (!strcmp(eventType, "transcript.final")) {
        const char *text = event["text"] | "";
        gOledLastHeard = text;
        addRealtimeLog(String("Heard: ") + (text[0] ? text : "(no speech)"));
    } else if (!strcmp(eventType, "toolbox.used")) {
        const char *toolbox = event["toolbox"] | "unknown";
        gOledLiveDetail = toolbox;
        gOledLiveDetail.replace("_", " ");
        gOledLiveDetail.toUpperCase();
        addRealtimeLog(String("AI toolbox: ") + toolbox);
    } else if (!strcmp(eventType, "toolbox.display")) {
        JsonObject display = event["display"].as<JsonObject>();
        gOledAiTitle = String(display["title"] | "AI TOOLBOX");
        gOledToolPrimary = String(display["primary"] | "WORKING");
        gOledToolSecondary = String(display["secondary"] | "");
        gOledToolDetail = String(display["detail"] | "");
        gOledConsoleUntilMs = millis() + 15000UL;
    } else if (!strcmp(eventType, "speaker.verified")) {
        const char *name = event["name"] | "Owner";
        const char *role = event["role"] | "owner";
        const float confidence = event["confidence"] | 0.0f;
        addRealtimeLog(
            String(!strcmp(role, "owner") ? "Owner verified: " : "Speaker verified: ") +
            name + " · confidence " + String(confidence, 3) +
            " · AI designing reaction"
        );
        // Qwen receives the verified identity before generating reply.presentation;
        // firmware deliberately does not hard-code colors, faces, or sounds here.
    } else if (!strcmp(eventType, "speaker.deferred")) {
        const unsigned long minimumMs = event["minimum_audio_ms"] | 1000UL;
        addRealtimeLog(String("Voice verification deferred: need ") +
                       minimumMs + " ms · retrying on next speech");
    } else if (!strcmp(eventType, "speaker.retry")) {
        const int remaining = event["attempts_remaining"] | 0;
        String detail = "Voice not confirmed yet";
        if (!event["confidence"].isNull())
            detail += String(": confidence ") +
                      String(event["confidence"].as<float>(), 3);
        detail += String(" · ") + remaining + " retries left";
        addRealtimeLog(detail);
    } else if (!strcmp(eventType, "speaker.unknown") || !strcmp(eventType, "speaker.unavailable")) {
        if (!event["confidence"].isNull()) {
            addRealtimeLog(
                String("Voice match: ") +
                String(event["confidence"].as<float>(), 3) +
                " (needs " + String(event["threshold"].as<float>(), 2) + ")"
            );
        }
    } else if (!strcmp(eventType, "speaker.enrollment.ready")) {
        requestPresentation(PresentationOwner::Identity, gPresetIdentityEnrollment, "enrollment");
        const int saved = event["sample_count"] | 0;
        const int required = event["required_samples"] | 3;
        const char *wakeWord = event["wake_word"] | gManagedWakeWord.c_str();
        addRealtimeLog(String("Owner enrollment armed: say ") + wakeWord + " (" + saved +
                       "/" + required + ")");
    } else if (!strcmp(eventType, "speaker.enrollment.sample")) {
        const int saved = event["sample_count"] | 0;
        const int required = event["required_samples"] | 3;
        addRealtimeLog(String("Owner voice sample saved: ") + saved + "/" +
                       required + " - say " + gManagedWakeWord + " again");
        gRealtimePreviousVoice = false;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        setRealtimeState(RealtimeChatState::WakeListening,
                         "owner enrollment - next sample");
    } else if (!strcmp(eventType, "speaker.enrollment.complete")) {
        requestPresentation(PresentationOwner::Identity, gPresetIdentityVerified, "verified");
        addRealtimeLog("Owner voice enrollment complete");
        gRealtimePreviousVoice = false;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        setRealtimeState(RealtimeChatState::WakeListening,
                         "owner enrollment complete");
    } else if (!strcmp(eventType, "speaker.enrollment.error")) {
        const char *message = event["message"] | "Enrollment failed";
        addRealtimeLog(String("Owner enrollment error: ") + message);
    } else if (!strcmp(eventType, "wake.rejected")) {
        const char *text = event["transcript"] | "";
        addRealtimeLog(String("Wake rejected. Heard: ") +
                       (text[0] ? text : "(no speech)"));
        gRealtimeWakeRejectionPending = true;
    } else if (!strcmp(eventType, "speech.rejected")) {
        addRealtimeLog("Ignored uncertain audio - listening again");
    } else if (!strcmp(eventType, "error")) {
        requestPresentation(PresentationOwner::System, gPresetSystemError, "error");
        const char *message = event["message"] | "";
        if (message[0]) addRealtimeLog(String("API error: ") + message);
    } else if (!strcmp(eventType, "reply.complete")) {
        String reply = event["reply"] | "";
        gOledLastReply = reply;
        if (reply.length() > 180) reply = reply.substring(0, 177) + "...";
        if (reply.length()) addRealtimeLog(String("Z2: ") + reply);
        gRealtimePresentationSoundReady = gRealtimePresentationNoteCount > 0;
    }
    if (!strcmp(eventType, "connected")) {
        // The callback runs inside WebSocketsClient::loop(). Queue the outbound
        // frame for the next application-loop iteration to avoid re-entering
        // the client while it is still parsing the server frame.
        gRealtimeSessionPending = true;
    } else if (!strcmp(eventType, "session.started")) {
        if (gRealtimeVoicePending) {
            gRealtimeVoicePending = false;
            // Defer the large PSRAM-backed send out of this receive callback.
            gRealtimeStartRecordingPending = true;
        } else {
            const String wakePrompt = String("API armed - say ") + gManagedWakeWord;
            setRealtimeState(RealtimeChatState::WakeListening,
                             wakePrompt.c_str());
        }
    } else if (!strcmp(eventType, "session.timeout")) {
        gRealtimeExpectedClose = true;
        closeRealtimeConversation(eventType);
    } else if (!strcmp(eventType, "wake.rejected")) {
        gRealtimePreviousVoice = true;
        gRealtimeFollowUpNeedsSilence = false;
        gRealtimeFollowUpSilenceStartedMs = 0;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        setRealtimeState(RealtimeChatState::WakeListening,
                         "wake rejected - listening again");
    } else if (!strcmp(eventType, "wake.detected")) {
        setRealtimeState(RealtimeChatState::Waiting,
                         "wake accepted - waiting for API reply");
    } else if (!strcmp(eventType, "wake.ready")) {
        gRealtimeFollowUpStartedMs = millis();
        // The wake phrase may still be inside the VAD hangover when this event
        // arrives. Require a clean silence/release before accepting a new
        // rising edge, otherwise the tail of "Hey Z2" becomes a second turn.
        gRealtimeFollowUpNeedsSilence = true;
        gRealtimeFollowUpSilenceStartedMs = 0;
        gRealtimePreviousVoice = true;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        gRealtimeWakeAcknowledgementPending = true;
        setRealtimeState(RealtimeChatState::FollowUp,
                         "wake accepted - listening for request");
    } else if (!strcmp(eventType, "speech.rejected")) {
        gRealtimeFollowUpStartedMs = millis();
        gRealtimeFollowUpNeedsSilence = true;
        gRealtimeFollowUpSilenceStartedMs = 0;
        gRealtimePreviousVoice = true;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        setRealtimeState(RealtimeChatState::FollowUp,
                         "uncertain audio ignored - listening again");
    } else if (!strcmp(eventType, "reply.emotion")) {
        gRealtimeReplyEmotion = String(event["emotion"] | "neutral");
        gRealtimeReplyEmotion.toUpperCase();
        gOledReplyMood = gRealtimeReplyEmotion;
    } else if (!strcmp(eventType, "reply.presentation")) {
        JsonObject presentation = event["presentation"].as<JsonObject>();
        JsonObject led = presentation["led"].as<JsonObject>();
        JsonObject sound = presentation["sound"].as<JsonObject>();
        JsonObject display = presentation["display"].as<JsonObject>();
        gOledReplyMood = String(presentation["mood"] | "neutral");
        gFaceEmotion = presentationEmotion(gOledReplyMood);
        gOledAiTitle = String(display["title"] | "Z2");
        gOledAiAccent = String(display["accent"] | "neutral");
        gOledAiSymbol = String(display["symbol"] | "*");
        setVoiceRingOverlay(
            led["enabled"] | false,
            constrain(led["red"] | 0, 0, 255),
            constrain(led["green"] | 0, 0, 255),
            constrain(led["blue"] | 0, 0, 255),
            constrain(led["brightness"] | 0, 0, 180),
            String(led["animation"] | "solid"),
            constrain(led["speed_ms"] | 900, 120, 4000));
        gRealtimePresentationSoundReady = false;
        gRealtimePresentationNoteCount = 0;
        if ((sound["enabled"] | false) && gCapabilitySpeaker) {
            gRealtimePresentationSoundVolume =
                constrain(sound["volume"] | 0, 0, 60);
            JsonArray notes = sound["notes"].as<JsonArray>();
            uint8_t queued = 0;
            for (JsonObject note : notes) {
                if (queued >= 4) break;
                gRealtimePresentationNotes[queued].frequencyHz =
                    constrain(note["frequency_hz"] | 440, 120, 2400);
                gRealtimePresentationNotes[queued].durationMs =
                    constrain(note["duration_ms"] | 80, 20, 400);
                gRealtimePresentationNotes[queued].gapMs =
                    constrain(note["gap_ms"] | 0, 0, 250);
                ++queued;
            }
            gRealtimePresentationNoteCount = queued;
        }
        addRealtimeLog(String("AI presentation: ") + gOledReplyMood + " · " +
                       gVoiceRingAnimation);
        Serial.printf("[Z2 CHAT] reply emotion=%s\n", gRealtimeReplyEmotion.c_str());
        addRealtimeLog(String("Reply emotion: ") + gRealtimeReplyEmotion);
    } else if (!strcmp(eventType, "reply.audio.start")) {
        // Return from temporary toolbox information to the AI-designed face
        // when Alpha begins speaking.
        gOledLiveDetail = "";
        gRealtimeReplyAudioBytes = 0;
        gOledReplyAudioBytes = 0;
        gRealtimeAudioEnded = false;
        gRealtimeSpeakerActive = true;
        i2s_zero_dma_buffer(kSpeakerI2sPort);
        i2s_set_clk(kSpeakerI2sPort, Z2_OUTPUT_AUDIO_SAMPLE_RATE,
                    I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
        setRealtimeState(RealtimeChatState::Speaking, "reply audio");
    } else if (!strcmp(eventType, "reply.audio.end")) {
        addRealtimeLog(String("Speaker PCM complete: ") +
                       gRealtimeReplyAudioBytes + " bytes");
        gRealtimeAudioEnded = true;
    } else if (!strcmp(eventType, "stage.timing")) {
        const char *stage = event["stage"] | "unknown";
        const unsigned long durationMs = event["duration_ms"] | 0UL;
        addRealtimeLog(String("Timing ") + stage + ": " + durationMs + " ms");
    } else if (!strcmp(eventType, "error")) {
        const char *code = event["code"] | "unknown";
        Serial.printf("[Z2 CHAT] API error=%s\n", code);
        closeRealtimeConversation("API error");
    }
}

void beginRealtimeConnection()
{
    if (!gToolboxVoiceEnabled || !Z2_REALTIME_ENABLED || !hasConfiguredRealtimeToken() ||
        WiFi.status() != WL_CONNECTED || controlLeaseActive()) return;
    disableAutoDrive("voice conversation");
    stopMotors();
    gRealtimeHeaders = String("X-AI-Token: ") + activeRobotToken() + "\r\n";
    gRealtimeConnectingStartedMs = millis();
    gRealtimeLastConnectAttemptMs = gRealtimeConnectingStartedMs;
    setRealtimeState(RealtimeChatState::Connecting, "voice detected");
    gRealtimeSocket = websockets::WebsocketsClient();
    gRealtimeSocket.addHeader("X-AI-Token", activeRobotToken());
    gRealtimeSocket.setCACert(kKentWynnRootCa);
    gRealtimeSocket.onMessage(handleRealtimeMessage);
    gRealtimeSocket.onEvent(handleRealtimeSocketEvent);
    if (!gRealtimeSocket.connectSecure(Z2_REALTIME_HOST, Z2_REALTIME_PORT,
                                       Z2_REALTIME_PATH))
        closeRealtimeConversation("WebSocket connection failed");
}

void storeWakeAudio(const uint8_t *pcm, size_t length)
{
    if (!gWakeAudio || !gWakeAudioCapacity) return;
    for (size_t i = 0; i < length; ++i) {
        gWakeAudio[gWakeAudioWrite] = pcm[i];
        gWakeAudioWrite = (gWakeAudioWrite + 1) % gWakeAudioCapacity;
        if (gWakeAudioSize < gWakeAudioCapacity) ++gWakeAudioSize;
    }
}

void handleRealtimeMicrophoneFrame(const int32_t *samples, size_t sampleCount,
                                   size_t activeChannel)
{
    if (!Z2_REALTIME_ENABLED || gRealtimeSpeakerActive || controlLeaseActive()) return;
    uint8_t pcm[512];
    size_t pcmBytes = 0;
    for (size_t i = activeChannel; i < sampleCount && pcmBytes + 1 < sizeof(pcm); i += 2) {
        const int32_t raw = samples[i] >> 16;
        const int16_t sample = static_cast<int16_t>(constrain(
            raw * Z2_MIC_PCM_GAIN, -32768L, 32767L));
        pcm[pcmBytes++] = sample & 0xff;
        pcm[pcmBytes++] = (sample >> 8) & 0xff;
    }
    if (!pcmBytes) return;

    if (gRealtimeState == RealtimeChatState::FollowUp &&
        gRealtimeFollowUpNeedsSilence) {
        if (gVoiceDetected) {
            gRealtimeFollowUpSilenceStartedMs = 0;
            gRealtimePreviousVoice = true;
        } else {
            if (!gRealtimeFollowUpSilenceStartedMs)
                gRealtimeFollowUpSilenceStartedMs = millis();
            if (millis() - gRealtimeFollowUpSilenceStartedMs >= 250UL) {
                gRealtimeFollowUpNeedsSilence = false;
                gRealtimePreviousVoice = false;
                gWakeAudioSize = 0;
                gWakeAudioWrite = 0;
                gWakeAudioCaptureActive = false;
            }
        }
        return;
    }

    const bool voiceStarted = gVoiceDetected && !gRealtimePreviousVoice;
    gRealtimePreviousVoice = gVoiceDetected;
    if (voiceStarted && (gRealtimeState == RealtimeChatState::WakeListening ||
                         gRealtimeState == RealtimeChatState::FollowUp))
        gWakeAudioCaptureActive = true;

    if (gRealtimeState == RealtimeChatState::WakeListening ||
        gRealtimeState == RealtimeChatState::FollowUp) {
        storeWakeAudio(pcm, pcmBytes);
        if (!gWakeAudioCaptureActive) {
            const size_t preRollBytes =
                (Z2_INPUT_AUDIO_SAMPLE_RATE * sizeof(int16_t) *
                 Z2_WAKE_AUDIO_PREROLL_MS) / 1000U;
            if (gWakeAudioSize > preRollBytes) gWakeAudioSize = preRollBytes;
        }
    } else if (gRealtimeState == RealtimeChatState::Connecting &&
               gWakeAudioCaptureActive) {
        // Preserve the complete utterance while TLS/session setup completes.
        // Do not stop at the first VAD gap: configured wake phrases may contain
        // a natural pause, and the PSRAM ring bounds connection-time silence.
        storeWakeAudio(pcm, pcmBytes);
    }
    if (gRealtimeState == RealtimeChatState::Recording) {
        sendRealtimeBinary(pcm, pcmBytes);
        // Refresh end-of-speech only for a currently strong frame. The VAD
        // state intentionally has hangover, and using that latched state here
        // can let steady room noise keep a recording open until the 15-second
        // hard limit.
        // Use the sensitive speech threshold for continuation so quiet words
        // are retained. The four-frame debounce below rejects isolated room
        // noise spikes without requiring later syllables to be unusually loud.
        const uint32_t speechOn = voiceSpeechOnThreshold();
        if (gMicrophoneRms >= speechOn) {
            if (gRealtimeSpeechFrames < 4) ++gRealtimeSpeechFrames;
            if (gRealtimeSpeechFrames >= 4)
                gRealtimeLastSpeechMs = millis();
        } else {
            gRealtimeSpeechFrames = 0;
        }
    }

    if (voiceStarted && gRealtimeState == RealtimeChatState::WakeListening) {
        if (gRealtimeSocket.available()) startRealtimeRecording(true);
        else {
            gRealtimeVoicePending = true;
            beginRealtimeConnection();
        }
    } else if (voiceStarted && gRealtimeState == RealtimeChatState::FollowUp) {
        startRealtimeRecording(true);
    }
}

void setupRealtimeConversation()
{
    // The preconnected voice socket streams the utterance live. Keep only the
    // short attack pre-roll needed to preserve the first consonant.
    const size_t requested =
        (Z2_INPUT_AUDIO_SAMPLE_RATE * sizeof(int16_t) *
         Z2_WAKE_AUDIO_PREROLL_MS) / 1000U;
    gWakeAudio = static_cast<uint8_t *>(ps_malloc(requested));
    if (gWakeAudio) gWakeAudioCapacity = requested;
    else {
        // Internal-RAM fallback still preserves one second of speech.
        const size_t fallback = Z2_INPUT_AUDIO_SAMPLE_RATE * sizeof(int16_t);
        gWakeAudio = static_cast<uint8_t *>(malloc(fallback));
        if (gWakeAudio) gWakeAudioCapacity = fallback;
    }
    Serial.printf("[Z2 CHAT] PSRAM=%s size=%u pre-roll=%u bytes\n",
                  psramFound() ? "ready" : "unavailable",
                  static_cast<unsigned>(ESP.getPsramSize()),
                  static_cast<unsigned>(gWakeAudioCapacity));
    addRealtimeLog(Z2_REALTIME_ENABLED && hasConfiguredRealtimeToken()
        ? String("Ready - say ") + gManagedWakeWord
        : "Disabled - API token missing");
    if (Z2_REALTIME_ENABLED && hasConfiguredRealtimeToken() &&
        WiFi.status() == WL_CONNECTED)
        beginRealtimeConnection();
}

void updateRealtimeConversation()
{
    if (!Z2_REALTIME_ENABLED) return;
    gRealtimeSocket.poll();
    if (controlLeaseActive()) {
        if (gRealtimeSocket.available() ||
            gRealtimeState != RealtimeChatState::WakeListening)
            closeRealtimeConversation("Control owns execution");
        return;
    }
    if (gRealtimeSessionPending) {
        gRealtimeSessionPending = false;
        sendRealtimeSession();
    }
    if (gRealtimeStartRecordingPending) {
        gRealtimeStartRecordingPending = false;
        startRealtimeRecording(true);
    }
    if (gRealtimeWakeAcknowledgementPending) {
        gRealtimeWakeAcknowledgementPending = false;
        playWakeAcknowledgement();
        gRealtimeMicResumeAtMs = millis() + 150UL;
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        resetVoiceVad();
        gRealtimePreviousVoice = false;
        gRealtimeFollowUpNeedsSilence = false;
        gRealtimeFollowUpSilenceStartedMs = 0;
        addRealtimeLog("Follow-up microphone armed");
    }
    if (gRealtimeWakeRejectionPending) {
        gRealtimeWakeRejectionPending = false;
        playWakeRejection();
        delay(80);
        playWakeAcknowledgement();
        gRealtimeMicResumeAtMs = millis() + 150UL;
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        resetVoiceVad();
        gRealtimePreviousVoice = false;
        gRealtimeFollowUpNeedsSilence = false;
        gRealtimeFollowUpSilenceStartedMs = 0;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        addRealtimeLog(String("Wake microphone rearmed - say ") +
                       gManagedWakeWord);
    }
    const unsigned long now = millis();
    // Keep the transport alive while the cloud is doing STT, model, toolbox,
    // or TTS work. Application keepalives are only appropriate while the API
    // is awaiting a message; WebSocket ping frames are safe during an active
    // turn and do not create noisy diagnostic events.
    if (gRealtimeSocket.available() &&
        gRealtimeState != RealtimeChatState::Recording &&
        now - gRealtimeLastPingMs >= 8000UL) {
        if (gRealtimeSocket.ping("z2")) gRealtimeLastPingMs = now;
    }
    if (gRealtimeState == RealtimeChatState::WakeListening &&
        gRealtimeSocket.available() &&
        now - gRealtimeLastKeepaliveMs >= 15000UL) {
        if (gRealtimeSocket.send("{\"type\":\"session.keepalive\"}"))
            gRealtimeLastKeepaliveMs = now;
    }
    if (gRealtimeState == RealtimeChatState::WakeListening &&
        !gRealtimeSocket.available() &&
        now - gRealtimeLastConnectAttemptMs >= 5000) {
        beginRealtimeConnection();
        return;
    }
    if (gRealtimeState == RealtimeChatState::Connecting &&
        now - gRealtimeConnectingStartedMs >= 15000) {
        closeRealtimeConversation("connection timeout");
        return;
    }
    const unsigned long recordingDurationMs =
        now - gRealtimeRecordingStartedMs;
    const bool silenceEndedRecording =
        now - gRealtimeLastSpeechMs >= Z2_SILENCE_END_MS &&
        (!gRealtimeRecordingIsWake ||
         recordingDurationMs >= Z2_MIN_WAKE_RECORDING_MS);
    const bool recordingLimitReached =
        recordingDurationMs >=
        (gRealtimeRecordingIsWake
             ? Z2_MAX_WAKE_RECORDING_MS
             : Z2_MAX_RECORDING_MS);
    if (gRealtimeState == RealtimeChatState::Recording &&
        (silenceEndedRecording || recordingLimitReached)) {
        gRealtimeSocket.send("{\"type\":\"audio.end\"}");
        addRealtimeLog(String("Recording complete: ") +
                       (now - gRealtimeRecordingStartedMs) + " ms");
        setRealtimeState(RealtimeChatState::Waiting, "audio ended");
    }
    if (gRealtimeState == RealtimeChatState::FollowUp &&
        now - gRealtimeFollowUpStartedMs >=
            static_cast<unsigned long>(gManagedFollowUpSeconds) * 1000UL) {
        gRealtimeExpectedClose = true;
        closeRealtimeConversation("follow-up timeout");
    }
}

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
bool gRealtimeSpeechRetryPending = false;
unsigned long gAiMotionStopAtMs = 0;
String gAiMotionCommandId;
String gAiMotionCommandDetail;
char gAiMotionExpectedDirection = 'S';
unsigned long gAiMotionStartedAtMs = 0;
unsigned long gAiMotionDurationMs = 0;
float gAiMotionRequestedDistanceCm = 0;
float gAiMotionRequestedDegrees = 0;
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
volatile bool gRealtimeEndAfterPlayback = false;
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

void interruptRealtimePlaybackForSafety()
{
    if (!gRealtimeSpeakerActive) return;
    gRealtimeSpeakerActive = false;
    gRealtimeAudioEnded = false;
    if (gRealtimeAudioBuffer) {
        size_t discarded = 0;
        while (void *item = xRingbufferReceiveUpTo(
                   gRealtimeAudioBuffer, &discarded, 0, 4096))
            vRingbufferReturnItem(gRealtimeAudioBuffer, item);
    }
    i2s_set_clk(kSpeakerI2sPort, Z2_INPUT_AUDIO_SAMPLE_RATE,
                I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
    i2s_zero_dma_buffer(kSpeakerI2sPort);
    setRealtimeState(gPhoneSessionActive
        ? RealtimeChatState::FollowUp
        : RealtimeChatState::WakeListening,
        "reply interrupted by local safety");
    addRealtimeLog("Local safety interrupted reply audio");
}

void realtimePlaybackTask(void *)
{
    // Buffer once before playback, then drain continuously.  The old code
    // reapplied its 250 ms threshold after every 4096-byte write, creating a
    // stop/start cadence even when audio was arriving normally.
    constexpr size_t kPlaybackInitialBufferBytes = 36000; // 750 ms PCM16
    constexpr size_t kPlaybackRebufferBytes = 24000;      // 500 ms PCM16
    bool playbackPrimed = false;
    bool recoveringUnderflow = false;
    for (;;) {
        if (!gRealtimeSpeakerActive || !gRealtimeAudioBuffer) {
            playbackPrimed = false;
            recoveringUnderflow = false;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        const size_t buffered = gRealtimeAudioBufferCapacity -
            xRingbufferGetCurFreeSize(gRealtimeAudioBuffer);
        const size_t target = recoveringUnderflow
            ? kPlaybackRebufferBytes
            : kPlaybackInitialBufferBytes;
        if (!playbackPrimed && buffered < target && !gRealtimeAudioEnded) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (!playbackPrimed) {
            playbackPrimed = true;
            recoveringUnderflow = false;
        }

        size_t received = 0;
        void *item = xRingbufferReceiveUpTo(
            gRealtimeAudioBuffer, &received, pdMS_TO_TICKS(20), 4096);
        if (item && received) {
            playRealtimePcm(static_cast<const uint8_t *>(item), received);
            vRingbufferReturnItem(gRealtimeAudioBuffer, item);
            continue;
        }

        if (!gRealtimeAudioEnded) {
            // Do not feed tiny late chunks directly to I2S. Pause once and
            // rebuild a useful cushion so recovery is clean rather than
            // producing repeated broken syllables.
            playbackPrimed = false;
            recoveringUnderflow = true;
            addRealtimeLog("Speaker stream underrun - rebuffering");
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
            gLastVoiceMs = 0;
            resetVoiceVad();
            gRealtimePreviousVoice = false;
            gWakeAudioSize = 0;
            gWakeAudioWrite = 0;
            gWakeAudioCaptureActive = false;
            gRealtimeFollowUpStartedMs = millis();
            // Playback ended cleanly and the DMA queue was cleared above. Do
            // not require another silence edge: the first real user phrase
            // after the short amplifier-tail guard should open recording.
            gRealtimeFollowUpNeedsSilence = false;
            gRealtimeFollowUpSilenceStartedMs = 0;
            if (gRealtimeEndAfterPlayback) {
                // The API intentionally ended the conversational session.
                // Socket teardown stays in the main loop to avoid racing the
                // WebSocket poller from this audio playback task.
            } else {
                // Shared-room default: every user turn must start with the
                // wake phrase. Ambient conversation after a reply must never
                // become an unlimited chain of STT/model requests.
                gRealtimeFollowUpAttack = false;
                gRealtimeFollowUpNeedsSilence = false;
                gRealtimeFollowUpSilenceStartedMs = 0;
                setRealtimeState(RealtimeChatState::WakeListening,
                                 "reply complete - wake phrase required");
            }
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
    gRealtimeFollowUpAttack = state == RealtimeChatState::FollowUp;
    gOledConsoleUntilMs = millis() + 15000UL;
    switch (state) {
    case RealtimeChatState::Connecting:
        gOledLiveStatus = "CONNECT";
        gFaceEmotion = FaceEmotion::Curious;
        gFaceAnimation = "PULSE";
        break;
    case RealtimeChatState::Recording:
        gOledLiveStatus = "LISTEN";
        gOledLiveDetail = "";
        gOledLastHeard = "";
        gOledLastReply = "";
        gOledReplyMood = "";
        gFaceEmotion = FaceEmotion::Alert;
        gFaceAnimation = "LISTENING";
        gVoiceEventUntilMs = 0;
        break;
    case RealtimeChatState::Waiting:
        gOledLiveStatus = "THINK";
        gFaceEmotion = FaceEmotion::Focused;
        gFaceAnimation = "PROCESSING";
        break;
    case RealtimeChatState::Speaking:
        gOledLiveStatus = "SPEAK";
        // reply.presentation owns the emotion; firmware only supplies motion.
        gFaceAnimation = "TALKING";
        break;
    case RealtimeChatState::FollowUp:
        gOledLiveStatus = "FOLLOW-UP";
        gFaceEmotion = FaceEmotion::Curious;
        gFaceAnimation = "LISTENING";
        break;
    default:
        gOledLiveStatus = "READY";
        // Wake listening is a quiet resting state. Do not carry a temporary
        // PULSE/wink reaction from rejection, connection, or the prior turn
        // into idle; that produced a strange alternating one-eye face.
        gVoiceEventUntilMs = 0;
        gFaceEmotion = FaceEmotion::Sleepy;
        gFaceAnimation = "STATIC";
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

void showRealtimeVoiceEvent(FaceEmotion emotion, const char *animation,
                            unsigned long durationMs)
{
    gVoiceEventEmotion = emotion;
    gVoiceEventAnimation = animation;
    gVoiceEventUntilMs = millis() + durationMs;
    gLastOledUpdateMs = 0;
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
    gRealtimeSpeechRetryPending = false;
    gRealtimeSpeechFrames = 0;
    gRealtimePresentationSoundReady = false;
    gRealtimePresentationNoteCount = 0;
    gRealtimeEndAfterPlayback = false;
    // Leave a short socket-free maintenance window for configuration/status
    // HTTPS before the proven preconnected voice channel is re-armed.
    gRealtimeLastConnectAttemptMs = millis();
}

bool controlLeaseActive()
{
    // Autonomous drive owns the robot until it is explicitly stopped or local
    // safety disables it. A timed voice lease must never expire underneath an
    // active autonomous run and let microphone noise cancel the motors.
    return gAutoDriveEnabled || (gControlLeaseUntilMs != 0 &&
        static_cast<int32_t>(gControlLeaseUntilMs - millis()) > 0);
}

void activateControlLease(bool stopping)
{
    const uint16_t seconds = stopping
        ? min<uint16_t>(5, gControlVoiceSuspendSeconds)
        : gControlVoiceSuspendSeconds;
    const bool wasActive = controlLeaseActive();
    gControlLeaseUntilMs = millis() + static_cast<unsigned long>(seconds) * 1000UL;
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
        if (gPhoneSessionActive) {
            // The robot chat socket is the phone session's only audio output.
            // If it disappears, Phone Connect cannot continue; fail open to
            // the onboard microphone even if the control disconnect event was
            // lost at the same time.
            gPhoneSessionActive = false;
            gPhonePairingVisible = false;
            gPhonePairingUrl = "";
            gPhonePairingUntilMs = 0;
            resetVoiceVad();
            addRealtimeLog("Phone output lost · onboard microphone restored");
        }
        if (!wasExpectedClose)
            showRealtimeVoiceEvent(FaceEmotion::Worried, "ALERT", 2200UL);
        if (gRealtimeState != RealtimeChatState::WakeListening)
            closeRealtimeConversation("WebSocket disconnected");
        // Unexpected transport loss should not create a five-second deaf
        // period. Reconnect on the next application-loop iteration.
        if (!wasExpectedClose && millis() >= 1500UL)
            gRealtimeLastConnectAttemptMs = millis() - 1500UL;
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
    // session.alive is an internal transport heartbeat, not robot activity.
    if (strcmp(eventType, "reply.text.delta") && strcmp(eventType, "session.alive"))
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
    } else if (!strcmp(eventType, "execution.plan")) {
        const char *state = event["state"] | "running";
        const char *active = event["active"] | "";
        const int step = event["step"] | 0;
        const int total = event["total"] | 0;
        gOledAiTitle = "EXECUTION PLAN";
        gOledToolPrimary = !strcmp(state, "complete")
            ? "PLAN COMPLETE" : String("STEP ") + step + " / " + total;
        gOledToolSecondary = active[0] ? String(active) : "SELECTING TOOLS";
        gOledToolSecondary.replace("_", " ");
        gOledToolSecondary.toUpperCase();
        gOledToolDetail = !strcmp(state, "step_complete")
            ? "STEP COMPLETE" : "AI ORCHESTRATION";
        gOledConsoleUntilMs = millis() + (!strcmp(state, "complete") ? 2500UL : 15000UL);
    } else if (!strcmp(eventType, "wake.rejected")) {
        const char *text = event["transcript"] | "";
        addRealtimeLog(String("Wake rejected. Heard: ") +
                       (text[0] ? text : "(no speech)"));
        gRealtimeWakeRejectionPending = true;
    } else if (!strcmp(eventType, "speech.rejected")) {
        addRealtimeLog("Ignored uncertain audio - listening again");
    } else if (!strcmp(eventType, "speaker.verified")) {
        const char *name = event["name"] | "Owner";
        const float confidence = event["confidence"] | 0.0f;
        const bool firstInSession = event["first_in_session"] | true;
        if (firstInSession) {
            showRealtimeVoiceEvent(FaceEmotion::Love, "PULSE", 1000UL);
            playSound("CHIRP");
        }
        addRealtimeLog(String("Voice verified: ") + name + " · confidence " +
                       String(confidence, 3) +
                       (firstInSession ? " · welcomed" : ""));
    } else if (!strcmp(eventType, "speaker.unknown")) {
        const float confidence = event["confidence"] | 0.0f;
        showRealtimeVoiceEvent(FaceEmotion::Confused, "PULSE", 900UL);
        addRealtimeLog(String("Phone voice unknown · confidence ") +
                       String(confidence, 3));
    } else if (!strcmp(eventType, "error")) {
        const char *message = event["message"] | "";
        if (message[0]) addRealtimeLog(String("API error: ") + message);
    } else if (!strcmp(eventType, "reply.complete")) {
        String reply = event["reply"] | "";
        gOledLastReply = reply;
        if (reply.length() > 180) reply = reply.substring(0, 177) + "...";
        if (reply.length()) addRealtimeLog(String("Z2: ") + reply);
        gRealtimePresentationSoundReady = gRealtimePresentationNoteCount > 0;
    } else if (!strcmp(eventType, "session.end")) {
        showRealtimeVoiceEvent(FaceEmotion::Happy, "PULSE", 1600UL);
        gRealtimeEndAfterPlayback = true;
        addRealtimeLog("Conversation ending after reply playback");
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
        showRealtimeVoiceEvent(FaceEmotion::Bored, "PULSE", 1800UL);
        gRealtimeExpectedClose = true;
        closeRealtimeConversation(eventType);
    } else if (!strcmp(eventType, "wake.rejected")) {
        showRealtimeVoiceEvent(FaceEmotion::Confused, "PULSE", 1400UL);
        gRealtimePreviousVoice = true;
        gRealtimeFollowUpNeedsSilence = true;
        gRealtimeFollowUpSilenceStartedMs = 0;
        gWakeAudioSize = 0;
        gWakeAudioWrite = 0;
        gWakeAudioCaptureActive = false;
        setRealtimeState(RealtimeChatState::WakeListening,
                         "wake rejected - listening again");
    } else if (!strcmp(eventType, "wake.detected")) {
        showRealtimeVoiceEvent(FaceEmotion::Surprised, "PULSE", 700UL);
        setRealtimeState(RealtimeChatState::Waiting,
                         "wake accepted - waiting for API reply");
    } else if (!strcmp(eventType, "wake.ready")) {
        showRealtimeVoiceEvent(FaceEmotion::Excited, "PULSE", 900UL);
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
        // speech.rejected is immediately followed by wake.ready. On that
        // recovery path retain the required clean-silence edge and stay
        // silent; otherwise residual VAD/noise opens another recording loop.
        if (gRealtimeSpeechRetryPending) {
            gRealtimeSpeechRetryPending = false;
            gRealtimeWakeAcknowledgementPending = false;
        } else {
            gRealtimeWakeAcknowledgementPending = true;
        }
        setRealtimeState(RealtimeChatState::FollowUp,
                         "wake accepted - listening for request");
    } else if (!strcmp(eventType, "speech.rejected")) {
        showRealtimeVoiceEvent(FaceEmotion::Worried, "PULSE", 1400UL);
        gRealtimeSpeechRetryPending = true;
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
        showRealtimeVoiceEvent(FaceEmotion::Despair, "ALERT", 2500UL);
        closeRealtimeConversation("API error");
        // Keep the failure visible after closeRealtimeConversation resets the
        // voice state. Reconnect loops must never produce warning sounds.
        requestPresentation(PresentationOwner::System, gPresetSystemError,
                            "error", false);
    }
}

void beginRealtimeConnection()
{
    if (!gToolboxVoiceEnabled || !Z2_REALTIME_ENABLED || !hasConfiguredRealtimeToken() ||
        WiFi.status() != WL_CONNECTED) return;
    // A task may own the motors while Phone Connect uses this authenticated
    // socket for conversation output. Keep transport online without touching
    // the drivetrain. The onboard microphone remains suspended below while
    // the control lease is active, preventing motor noise from becoming input.
    if (!controlLeaseActive()) {
        disableAutoDrive("voice conversation");
        stopMotors();
    }
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

    if ((gRealtimeState == RealtimeChatState::FollowUp ||
         gRealtimeState == RealtimeChatState::WakeListening) &&
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
    if (gRealtimeEndAfterPlayback && !gRealtimeSpeakerActive) {
        closeRealtimeConversation("conversation ended");
        return;
    }
    if (controlLeaseActive()) {
        // Preserve the WebSocket for Phone Connect and server events. Only
        // local microphone capture is suspended by
        // handleRealtimeMicrophoneFrame() during physical execution.
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
        // Idle wake listening stays silent. Play one acknowledgement only
        // after the API has accepted the wake phrase and armed follow-up.
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
        // A rejected wake is routine, not an alarm. Keep the state/face/log
        // feedback but rearm silently and without an audio-tail delay.
        gRealtimeMicResumeAtMs = millis();
        gVoiceDetected = false;
        gVoiceAttackFrames = 0;
        gLastVoiceMs = 0;
        resetVoiceVad();
        gRealtimePreviousVoice = false;
        gRealtimeFollowUpNeedsSilence = true;
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
    // Phone Connect drives turns through its own WebSocket while this
    // authenticated robot socket remains in FOLLOW_UP. Keep the robot channel
    // alive too; otherwise its short follow-up timer can expire mid phone turn
    // and tear down playback plus the verified phone identity.
    if ((gRealtimeState == RealtimeChatState::WakeListening ||
         gPhoneSessionActive) &&
        gRealtimeSocket.available() &&
        now - gRealtimeLastKeepaliveMs >= 15000UL) {
        if (gRealtimeSocket.send("{\"type\":\"session.keepalive\"}"))
            gRealtimeLastKeepaliveMs = now;
    }
    if (gRealtimeState == RealtimeChatState::WakeListening &&
        !gRealtimeSocket.available() &&
        now - gRealtimeLastConnectAttemptMs >= 1500) {
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
        !gPhoneSessionActive &&
        now - gRealtimeFollowUpStartedMs >=
            static_cast<unsigned long>(gManagedFollowUpSeconds) * 1000UL) {
        gRealtimeExpectedClose = true;
        closeRealtimeConversation("follow-up timeout");
    }
}

websockets::WebsocketsClient gControlSocket;
unsigned long gControlLastConnectAttemptMs = 0;
unsigned long gControlConnectingStartedMs = 0;
bool gControlConnecting = false;

void enqueueCloudCommand(const String &payload)
{
    if (payload.isEmpty()) return;
    if (gCloudCommandQueueSize == kCloudCommandQueueCapacity) {
        gCloudCommandQueue[gCloudCommandQueueHead] = "";
        gCloudCommandQueueHead =
            (gCloudCommandQueueHead + 1) % kCloudCommandQueueCapacity;
        --gCloudCommandQueueSize;
        addRealtimeLog("Control queue full - oldest command dropped");
    }
    const uint8_t tail =
        (gCloudCommandQueueHead + gCloudCommandQueueSize) %
        kCloudCommandQueueCapacity;
    gCloudCommandQueue[tail] = payload;
    ++gCloudCommandQueueSize;
    Serial.printf("[Z2 CONTROL] enqueued size=%u head=%u bytes=%u\n",
                  gCloudCommandQueueSize, gCloudCommandQueueHead,
                  static_cast<unsigned>(payload.length()));
}

bool peekCloudCommand(String &payload)
{
    if (!gCloudCommandQueueSize) return false;
    payload = gCloudCommandQueue[gCloudCommandQueueHead];
    return true;
}

void discardCloudCommand()
{
    if (!gCloudCommandQueueSize) return;
    gCloudCommandQueue[gCloudCommandQueueHead] = "";
    gCloudCommandQueueHead =
        (gCloudCommandQueueHead + 1) % kCloudCommandQueueCapacity;
    --gCloudCommandQueueSize;
}

void sendDeviceEvent(const char *tool, const char *event, const String &detail)
{
    if (!gControlSocket.available()) return;
    JsonDocument message;
    message["type"] = "device.event";
    message["id"] = String(millis(), HEX);
    message["tool"] = tool;
    message["event"] = event;
    message["detail"] = detail;
    String payload;
    serializeJson(message, payload);
    gControlSocket.send(payload);
}

void handleControlMessage(websockets::WebsocketsMessage message)
{
    if (message.isBinary()) {
        if (gRealtimeSpeakerActive && gRealtimeAudioBuffer) {
            const bool firstAudio = gRealtimeReplyAudioBytes == 0;
            gRealtimeReplyAudioBytes += message.length();
            if (firstAudio)
                addRealtimeLog(String("Task speaker PCM received: ") + message.length() + " bytes");
            if (xRingbufferSend(gRealtimeAudioBuffer, message.c_str(), message.length(), 0) != pdTRUE)
                addRealtimeLog("Task speaker PCM buffer overflow");
        }
        return;
    }
    if (!message.isText()) return;
    JsonDocument event;
    if (deserializeJson(event, message.c_str(), message.length())) return;
    const char *eventType = event["type"] | "";
    if (!strcmp(eventType, "control.ping")) {
        gControlSocket.send("{\"type\":\"control.pong\"}");
        return;
    }
    if (!strcmp(eventType, "control.connected")) {
        Serial.println("[Z2 CONTROL] secure command channel ready");
        addRealtimeLog("Control channel connected");
        return;
    }
    if (!strcmp(eventType, "config.updated")) {
        const uint32_t version = event["config_version"] | 0;
        if (version > gManagedConfigVersion) {
            gRobotConfigSyncRequested = true;
            gLastRobotConfigPollMs = 0;
            closeRealtimeConversation("API configuration updated");
            addRealtimeLog(String("Config v") + version + " update received");
        }
        return;
    }
    if (!strcmp(eventType, "phone.pairing.ready")) {
        gPhonePairingUrl = String(event["pairing_url"] | "");
        gPhonePairingVisible = !gPhonePairingUrl.isEmpty();
        gPhonePairingUntilMs = millis() + 60000UL;
        gPhoneSessionActive = false;
        clearVoiceRingOverlay();
        gRing.clear(); gRing.show();
        showRealtimeVoiceEvent(FaceEmotion::Curious, "PULSE", 1200UL);
        playSound("CHIRP");
        addRealtimeLog("Phone pairing QR ready · RGB ring off");
        return;
    }
    if (!strcmp(eventType, "phone.session.connected")) {
        gPhonePairingVisible = false;
        gPhoneSessionActive = true;
        gPhonePairingUrl = "";
        gPhonePairingUntilMs = 0;
        const char *purpose = event["purpose"] | "phone_chat";
        showRealtimeVoiceEvent(FaceEmotion::Excited, "PULSE", 1400UL);
        playSound("BEEP");
        addRealtimeLog(String(!strcmp(purpose, "owner_voice_enrollment")
            ? "Owner enrollment phone connected"
            : "Phone microphone connected · onboard microphone suspended"));
        return;
    }
    if (!strcmp(eventType, "owner.enrollment.progress")) {
        const int saved = event["sample_count"] | 0;
        showRealtimeVoiceEvent(FaceEmotion::Happy, "PULSE", 900UL);
        playSound("BEEP");
        addRealtimeLog(String("Owner voice sample saved: ") + saved + "/3");
        return;
    }
    if (!strcmp(eventType, "owner.enrollment.complete")) {
        showRealtimeVoiceEvent(FaceEmotion::Love, "PULSE", 2400UL);
        playSound("DOUBLE");
        addRealtimeLog("Owner voice enrolled: 3/3 samples · recognition enabled");
        return;
    }
    if (!strcmp(eventType, "phone.session.disconnected")) {
        gPhonePairingVisible = false;
        gPhoneSessionActive = false;
        gPhonePairingUrl = "";
        gPhonePairingUntilMs = 0;
        resetVoiceVad();
        addRealtimeLog("Phone session ended · onboard microphone restored");
        return;
    }
    if (!strcmp(eventType, "device.event.ack")) return;
    if (!strcmp(eventType, "task.speech.start")) {
        gRealtimeReplyAudioBytes = 0;
        gRealtimeAudioEnded = false;
        gRealtimeSpeakerActive = true;
        i2s_zero_dma_buffer(kSpeakerI2sPort);
        i2s_set_clk(kSpeakerI2sPort, Z2_OUTPUT_AUDIO_SAMPLE_RATE,
                    I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
        showRealtimeVoiceEvent(FaceEmotion::Focused, "PULSE", 1800UL);
        setRealtimeState(RealtimeChatState::Speaking, "task delivery audio");
        addRealtimeLog("Task voice delivery started");
        return;
    }
    if (!strcmp(eventType, "task.speech.end")) {
        gRealtimeAudioEnded = true;
        JsonDocument acknowledgement;
        acknowledgement["type"] = "task.delivery.ack";
        acknowledgement["delivery_id"] = event["delivery_id"] | "";
        acknowledgement["status"] = "executed";
        String payload;
        serializeJson(acknowledgement, payload);
        gControlSocket.send(payload);
        addRealtimeLog(String("Task speaker PCM complete: ") + gRealtimeReplyAudioBytes + " bytes");
        return;
    }
    if (!strcmp(eventType, "task.speech.error")) {
        gRealtimeAudioEnded = true;
        showRealtimeVoiceEvent(FaceEmotion::Worried, "PULSE", 1800UL);
        addRealtimeLog("Task voice delivery failed");
        return;
    }
    if (!strcmp(eventType, "device.command")) {
        String payload;
        serializeJson(event["command"], payload);
        enqueueCloudCommand(payload);
        const char *command = event["command"]["command"] | "unknown";
        const char *value = event["command"]["value"] | "";
        addRealtimeLog(String("Control queued [") + gCloudCommandQueueSize +
                       "]: " + command + (value[0] ? String(" ") + value : "") +
                       (gRobotConfigSynced ? "" : " · config sync pending"));
    }
}

void handleControlSocketEvent(websockets::WebsocketsEvent type, String detail)
{
    if (type == websockets::WebsocketsEvent::ConnectionOpened) {
        gControlConnecting = false;
        Serial.println("[Z2 CONTROL] WebSocket connected securely");
    } else if (type == websockets::WebsocketsEvent::ConnectionClosed) {
        gControlConnecting = false;
        Serial.printf("[Z2 CONTROL] WebSocket disconnected detail=%s\n",
                      detail.c_str());
        addRealtimeLog(String("Control channel disconnected") +
                       (detail.length() ? String(" · ") + detail : ""));
    }
}

void beginControlConnection()
{
    if (!hasConfiguredRealtimeToken() || WiFi.status() != WL_CONNECTED) return;
    gControlLastConnectAttemptMs = millis();
    gControlConnectingStartedMs = gControlLastConnectAttemptMs;
    gControlConnecting = true;
    gControlSocket = websockets::WebsocketsClient();
    gControlSocket.addHeader("X-AI-Token", activeRobotToken());
    gControlSocket.setCACert(kKentWynnRootCa);
    gControlSocket.onMessage(handleControlMessage);
    gControlSocket.onEvent(handleControlSocketEvent);
    if (!gControlSocket.connectSecure(Z2_REALTIME_HOST, Z2_REALTIME_PORT,
                                      Z2_CONTROL_PATH)) {
        gControlConnecting = false;
        addRealtimeLog("Control channel connection failed");
    }
}

void setupControlChannel()
{
    if (hasConfiguredRealtimeToken() && WiFi.status() == WL_CONNECTED)
        beginControlConnection();
}

void updateControlChannel()
{
    gControlSocket.poll();
    const unsigned long now = millis();
    if (gControlSocket.available()) {
        // Flush outside WebSocket callbacks to avoid re-entering the client.
        // The bounded queue keeps diagnostics useful without affecting control.
        if (gCloudDiagnosticQueueSize) {
            const String detail = gCloudDiagnosticQueue[gCloudDiagnosticQueueHead];
            sendDeviceEvent("Diagnostics", "Device log", detail);
            gCloudDiagnosticQueue[gCloudDiagnosticQueueHead] = "";
            gCloudDiagnosticQueueHead =
                (gCloudDiagnosticQueueHead + 1) % kCloudDiagnosticQueueCount;
            --gCloudDiagnosticQueueSize;
        }
        // A compact one-second snapshot lets server-side AI tools answer from
        // Redis without opening a request socket or interrupting voice audio.
        static unsigned long lastTelemetryMs = 0;
        if (now - lastTelemetryMs >= 1000) {
            lastTelemetryMs = now;
            JsonDocument message;
            message["type"] = "device.telemetry";
            JsonObject telemetry = message["telemetry"].to<JsonObject>();
            telemetry["wifi_connected"] = WiFi.status() == WL_CONNECTED;
            telemetry["motors_ready"] =
                gVirtualPowerOn && !gSafetyStopActive && !gCliffRecoveryActive;
            telemetry["sonar_cm"] = gUltrasonicValid ? gLatestDistanceCm : -1;
            telemetry["sonar_valid"] = gUltrasonicValid;
            JsonArray tof = telemetry["tof_mm"].to<JsonArray>();
            for (uint8_t index = 0; index < kTofCount; ++index)
                tof.add(gTofValid[index] ? gTofDistanceMm[index] : -1);
            JsonArray tofReady = telemetry["tof_ready"].to<JsonArray>();
            JsonArray tofValid = telemetry["tof_valid"].to<JsonArray>();
            JsonArray tofType = telemetry["tof_type"].to<JsonArray>();
            for (uint8_t index = 0; index < kTofCount; ++index) {
                tofReady.add(gTofReady[index]);
                tofValid.add(gTofValid[index]);
                tofType.add(gTofType[index] == 1 ? "L0X" :
                            gTofType[index] == 2 ? "L1X" : "unknown");
            }
            JsonArray cliff = telemetry["cliff"].to<JsonArray>();
            cliff.add(gCliffDetected[0]); cliff.add(gCliffDetected[1]);
            telemetry["light_lux"] = gAmbientLux;
            telemetry["light_sensor_ready"] = gLightSensorReady;
            telemetry["microphone_ready"] = gMicrophoneReady;
            telemetry["speaker_ready"] = gSpeakerReady;
            telemetry["display_ready"] = gOledReady;
            telemetry["led_ring_mode"] = ringModeName();
            telemetry["led_ring_brightness"] = gRing.getBrightness();
            telemetry["moving"] = gMoving;
            telemetry["direction"] = String(gCommand);
            telemetry["motor_speed_pwm"] = gMoving ? gSpeed : 0;
            telemetry["auto_drive"] = gAutoDriveEnabled;
            telemetry["auto_state"] = autoDriveStateName();
            telemetry["auto_stop_reason"] = gAutoDriveStopReason;
            telemetry["auto_drive_silent"] = gAutoDriveSilent;
            telemetry["motion_control"] = "open_loop_timed";
            telemetry["distance_control"] = false;
            telemetry["wheel_encoders"] = false;
            JsonObject drivetrain = telemetry["drivetrain"].to<JsonObject>();
            drivetrain["wheel_diameter_mm"] = kWheelDiameterMm;
            drivetrain["wheel_center_spacing_mm"] = kWheelCenterSpacingMm;
            drivetrain["rated_rpm"] = kGearMotorRatedRpm;
            drivetrain["rated_pwm"] = kDrivetrainRatedPwm;
            drivetrain["configured_motion_pwm"] = gAutoCruiseSpeed;
            drivetrain["linear_cm_per_s"] = kDrivetrainLinearCmPerSecond;
            drivetrain["angular_deg_per_s"] = kDrivetrainAngularDegPerSecond;
            drivetrain["calibration"] = kDrivetrainUsesMeasuredCalibration
                ? "measured" : "rated_theoretical";
            telemetry["virtual_power"] = gVirtualPowerOn;
            telemetry["safety_stop"] = gSafetyStopActive;
            telemetry["cliff_recovery"] = gCliffRecoveryActive;
            telemetry["automatic_safety_recovery"] = gAutomaticSafetyRecovery;
            telemetry["local_recovery_active"] = gLocalRecoveryActive;
            telemetry["local_recovery_attempted"] = gLocalRecoveryAttempted;
            JsonObject safety = telemetry["safety_profile"].to<JsonObject>();
            safety["proximity_stop_mm"] = kSafetyProximityStopMm;
            safety["proximity_reset_mm"] = kSafetyProximityResetMm;
            safety["source"] = "hardware_profile";
            JsonObject capabilities = telemetry["capabilities"].to<JsonObject>();
            capabilities["movement"] = gCapabilityMovement && gToolboxControlEnabled;
            capabilities["autonomous_drive"] = gCapabilityAutonomousDrive;
            capabilities["display"] = gCapabilityDisplay;
            capabilities["speaker"] = gCapabilitySpeaker && gSpeakerReady;
            capabilities["led_ring"] = gCapabilityLedRing;
            capabilities["traffic_light"] = true;
            telemetry["all_systems_good"] =
                WiFi.status() == WL_CONNECTED && gVirtualPowerOn &&
                gUltrasonicValid && gTofReady[0] && gTofReady[1] &&
                gTofReady[2] && !gCliffDetected[0] && !gCliffDetected[1] &&
                gLightSensorReady && gMicrophoneReady && gSpeakerReady &&
                gOledReady;
            String payload;
            serializeJson(message, payload);
            gControlSocket.send(payload);
        }
        return;
    }
    if (gControlConnecting && now - gControlConnectingStartedMs < 15000) return;
    gControlConnecting = false;
    if (now - gControlLastConnectAttemptMs >= 5000) beginControlConnection();
}

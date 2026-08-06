const char *activeRobotToken()
{
    return gRobotDeviceToken.c_str();
}

bool hasConfiguredRealtimeToken()
{
    return gRobotConfigSynced && gRobotDeviceToken.startsWith("rk_");
}

bool syncManagedOtaCredential()
{
    if (WiFi.status() != WL_CONNECTED || !gRobotDeviceToken.startsWith("rk_"))
        return false;
    const String localPassword = gRobotPreferences.getString("ota-pass", "");
    WiFiClientSecure client;
    client.setCACert(kKentWynnRootCa);
    HTTPClient request;
    request.setConnectTimeout(800);
    request.setTimeout(1500);
    if (!request.begin(client, "https://api.kentwynn.com/v1/robot/ota-credential"))
        return false;
    request.addHeader("X-AI-Token", activeRobotToken());
    request.addHeader("Content-Type", "application/json");
    JsonDocument body;
    if (!localPassword.isEmpty()) body["ota_password"] = localPassword;
    String serialized;
    serializeJson(body, serialized);
    const int status = localPassword.isEmpty() ? request.GET() : request.POST(serialized);
    if (status != HTTP_CODE_OK) {
        request.end();
        return false;
    }
    JsonDocument response;
    const DeserializationError error = deserializeJson(response, request.getString());
    request.end();
    const String managedPassword = response["ota_password"] | "";
    if (error || managedPassword.length() < 24) return false;
    if (managedPassword != localPassword) {
        gRobotPreferences.putString("ota-pass", managedPassword);
        Serial.println("[Z2 OTA] managed credential synchronized; active after reboot");
    } else {
        Serial.println("[Z2 OTA] managed credential synchronized");
    }
    return true;
}

void loadRobotOnboardingState()
{
    gRobotPreferences.begin("z2-robot", false);
    gRobotDeviceToken = gRobotPreferences.getString("device-token", "");
    gManagedRobotName = "";
    gManagedWakeWord = "";
    gManagedLanguage = "";
    gManagedVoice = "";
    gManagedThinkingEnabled = false;
    gManagedFollowUpSeconds = 0;
    gManagedConfigVersion = 0;
    gRobotConfigSynced = false;
}

bool syncManagedRobotConfig()
{
    if (WiFi.status() != WL_CONNECTED || !gRobotDeviceToken.startsWith("rk_"))
        return false;
    WiFiClientSecure client;
    client.setCACert(kKentWynnRootCa);
    HTTPClient request;
    request.setConnectTimeout(600);
    request.setTimeout(1000);
    if (!request.begin(client, "https://api.kentwynn.com/v1/robot/config")) return false;
    request.addHeader("X-AI-Token", activeRobotToken());
    const int status = request.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("[Z2 ONBOARD] config sync failed HTTP %d\n", status);
        request.end();
        return false;
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, request.getString());
    request.end();
    if (error) return false;

    if (document["name"].is<const char *>())
        gManagedRobotName = document["name"].as<const char *>();
    if (document["wake_word"].is<const char *>())
        gManagedWakeWord = document["wake_word"].as<const char *>();
    if (document["language"].is<const char *>())
        gManagedLanguage = document["language"].as<const char *>();
    if (document["voice"].is<const char *>())
        gManagedVoice = document["voice"].as<const char *>();
    if (!document["thinking_enabled"].is<bool>() ||
        !document["follow_up_timeout_seconds"].is<uint16_t>() ||
        gManagedRobotName.isEmpty() || gManagedWakeWord.isEmpty() ||
        gManagedLanguage.isEmpty() || gManagedVoice.isEmpty())
        return false;
    gManagedThinkingEnabled = document["thinking_enabled"].as<bool>();
    gManagedFollowUpSeconds = document["follow_up_timeout_seconds"].as<uint16_t>();
    JsonObject capabilities = document["capabilities"].as<JsonObject>();
    if (!capabilities.isNull()) {
        gCapabilityDisplay = capabilities["display"] | true;
        gCapabilitySpeaker = capabilities["speaker"] | true;
        gCapabilityLedRing = capabilities["led_ring"] | true;
        gCapabilityMovement = capabilities["movement"] | false;
        gCapabilityAutonomousDrive = capabilities["autonomous_drive"] | false;
    }
    JsonObject device = document["device_settings"].as<JsonObject>();
    if (!device.isNull()) {
        gSpeakerVolumePercent = constrain(device["speaker_volume"] | 60, 0, 100);
        gLightSensitivity = constrain(device["light_sensitivity"] | 7, 1, 10);
        gAutoOffLux = constrain(device["light_off_lux"] | 300, 50, 1000);
        gVoiceRingBrightness = constrain(
            device["voice_ring_brightness"] | 32, 1, 180);
        gObstacleWarningDistanceCm = constrain(device["obstacle_warning_cm"] | 25, 15, 60);
        gAutoFrontClearMm = constrain(device["auto_front_clear_mm"] | 300, 200, 800);
        gAutoRearClearMm = constrain(device["auto_rear_clear_mm"] | 250, 150, 800);
        gAutoCruiseSpeed = constrain(device["auto_cruise_speed"] | 180, 100, 230);
        gControlVoiceSuspendSeconds = constrain(
            device["control_voice_suspend_seconds"] | 20, 5, 120);
        gReverseSound = String(device["reverse_sound"] | "DOUBLE");
        gCollisionSound = String(device["collision_sound"] | "BEEP");
        JsonObject presentation = device["presentation"].as<JsonObject>();
        JsonObject toolboxes = device["toolboxes"].as<JsonObject>();
        if (!toolboxes.isNull()) {
            gToolboxControlEnabled = toolboxes["control"] | true;
            gToolboxVoiceEnabled = toolboxes["voice"] | true;
            gToolboxIdentityEnabled = toolboxes["identity"] | true;
        }
        auto loadPreset = [](JsonObject source, PresentationPreset &target) {
            if (source.isNull()) return;
            target.oled = String(source["oled"] | target.oled.c_str());
            target.sound = String(source["sound"] | target.sound.c_str());
            target.animation = String(source["animation"] | target.animation.c_str());
        };
        if (!presentation.isNull()) {
            loadPreset(presentation["idle"].as<JsonObject>(), gPresetIdle);
            loadPreset(presentation["voice_listening"].as<JsonObject>(), gPresetVoiceListening);
            loadPreset(presentation["voice_thinking"].as<JsonObject>(), gPresetVoiceThinking);
            loadPreset(presentation["voice_speaking"].as<JsonObject>(), gPresetVoiceSpeaking);
            loadPreset(presentation["control_forward"].as<JsonObject>(), gPresetControlForward);
            loadPreset(presentation["control_reverse"].as<JsonObject>(), gPresetControlReverse);
            loadPreset(presentation["control_turn"].as<JsonObject>(), gPresetControlTurn);
            loadPreset(presentation["control_left"].as<JsonObject>(), gPresetControlLeft);
            loadPreset(presentation["control_right"].as<JsonObject>(), gPresetControlRight);
            loadPreset(presentation["control_auto"].as<JsonObject>(), gPresetControlAuto);
            loadPreset(presentation["safety_cliff"].as<JsonObject>(), gPresetSafetyCliff);
            loadPreset(presentation["safety_obstacle"].as<JsonObject>(), gPresetSafetyObstacle);
            loadPreset(presentation["safety_proximity"].as<JsonObject>(), gPresetSafetyProximity);
            loadPreset(presentation["system_connecting"].as<JsonObject>(), gPresetSystemConnecting);
            loadPreset(presentation["system_error"].as<JsonObject>(), gPresetSystemError);
            loadPreset(presentation["identity_verified"].as<JsonObject>(), gPresetIdentityVerified);
            loadPreset(presentation["identity_unknown"].as<JsonObject>(), gPresetIdentityUnknown);
            loadPreset(presentation["identity_enrollment"].as<JsonObject>(), gPresetIdentityEnrollment);
            loadPreset(presentation["interaction_touch"].as<JsonObject>(), gPresetInteractionTouch);
            gReverseSound = gPresetControlReverse.sound;
            gCollisionSound = gPresetSafetyObstacle.sound;
            requestPresentation(PresentationOwner::Idle, gPresetIdle, "idle", false);
        }
        gVoiceExpressionsEnabled = device["voice_expressions"] | true;
        auto parseEmotion = [](String value, FaceEmotion fallback) {
            value.toUpperCase();
            if (value == "HAPPY") return FaceEmotion::Happy;
            if (value == "CURIOUS") return FaceEmotion::Curious;
            if (value == "SLEEPY") return FaceEmotion::Sleepy;
            if (value == "SURPRISED") return FaceEmotion::Surprised;
            if (value == "EXCITED") return FaceEmotion::Excited;
            if (value == "SAD") return FaceEmotion::Sad;
            if (value == "LOVE") return FaceEmotion::Love;
            if (value == "CONFUSED") return FaceEmotion::Confused;
            if (value == "THINKING") return FaceEmotion::Thinking;
            if (value == "ANGRY") return FaceEmotion::Angry;
            if (value == "FACE") return FaceEmotion::Neutral;
            return fallback;
        };
        gListeningEmotion = parseEmotion(String(device["listening_expression"] | "CURIOUS"), FaceEmotion::Curious);
        gThinkingEmotion = parseEmotion(String(device["thinking_expression"] | "CURIOUS"), FaceEmotion::Curious);
        gSpeakingEmotion = parseEmotion(String(device["speaking_expression"] | "HAPPY"), FaceEmotion::Happy);
        // The OLED is always the robot face except during setup and temporary
        // AI-toolbox activity. Legacy cloud display modes are ignored.
        gDisplayMode = DisplayMode::Face;
        gPreferences.putUChar("sensitivity", gLightSensitivity);
        gPreferences.putUShort("off-lux", gAutoOffLux);

        if (presentation.isNull()) {
            String displayMode = device["display_mode"] | "FACE";
            displayMode.toUpperCase();
            gDisplayMode = DisplayMode::Face;
            gIdleEmotion = parseEmotion(displayMode, FaceEmotion::Neutral);
            gFaceEmotion = gIdleEmotion;
            Serial.printf("[Z2 ONBOARD] legacy display=%s\n", displayMode.c_str());
        }
        String ledMode = String(device["led_mode"] | "AUTO");
        ledMode.toUpperCase();
        if (ledMode != "OFF") ledMode = "AUTO";
        setRingMode(ledMode, gAutoRingBrightness);
        gLastOledUpdateMs = 0;
        Serial.printf("[Z2 ONBOARD] device volume=%u presentation=%s sensitivity=%u off-lux=%u\n",
                      gSpeakerVolumePercent, presentation.isNull() ? "legacy" : "managed",
                      gLightSensitivity, gAutoOffLux);
    }
    const uint32_t receivedVersion = document["config_version"] | 0;
    if (!receivedVersion) return false;
    gRobotConfigChanged = gManagedConfigVersion != 0 &&
                          receivedVersion != gManagedConfigVersion;
    gManagedConfigVersion = receivedVersion;
    gRobotConfigSynced = true;
    syncManagedOtaCredential();
    Serial.printf("[Z2 ONBOARD] config synced name=%s wake=%s language=%s\n",
                  gManagedRobotName.c_str(), gManagedWakeWord.c_str(),
                  gManagedLanguage.c_str());
    return true;
}

void pollManagedRobotConfig()
{
    constexpr unsigned long kConfigPollIntervalMs = 30000;
    const unsigned long now = millis();
    if ((!gRobotConfigSyncRequested &&
         now - gLastRobotConfigPollMs < kConfigPollIntervalMs) || gMoving ||
        gRealtimeSocket.available() || gVoiceDetected ||
        gRealtimeState != RealtimeChatState::WakeListening)
        return;
    gLastRobotConfigPollMs = now;
    if (syncManagedRobotConfig()) {
        gRobotConfigSyncRequested = false;
    }
    if (gRobotConfigChanged) {
        addRealtimeLog(String("Config v") + gManagedConfigVersion +
                       " contact=" + gPresetInteractionTouch.oled + "/" +
                       gPresetInteractionTouch.animation + "/" +
                       gPresetInteractionTouch.sound + " cliff=" +
                       gPresetSafetyCliff.oled + "/" +
                       gPresetSafetyCliff.animation + "/" +
                       gPresetSafetyCliff.sound);
        closeRealtimeConversation("API configuration updated");
        gRobotConfigChanged = false;
    }
}

void acknowledgeCloudCommand(const String &commandId, bool executed, String detail)
{
    if (commandId.isEmpty()) return;
    if (gControlSocket.available()) {
        JsonDocument acknowledgement;
        acknowledgement["type"] = "device.command.ack";
        acknowledgement["id"] = commandId;
        acknowledgement["status"] = executed ? "executed" : "rejected";
        acknowledgement["detail"] = detail;
        String payload;
        serializeJson(acknowledgement, payload);
        if (gControlSocket.send(payload)) {
            Serial.printf("[Z2 CONTROL] ack=%s WebSocket\n",
                          executed ? "executed" : "rejected");
            return;
        }
    }
    detail.replace("\\", "\\\\"); detail.replace("\"", "\\\"");
    const String body = String("{\"status\":\"") +
        (executed ? "executed" : "rejected") + "\",\"detail\":\"" + detail + "\"}";
    const String ackUrl = String("https://api.kentwynn.com/v1/robot/commands/") + commandId + "/ack";
    int ackStatus = -1;
    for (uint8_t attempt = 1; attempt <= 3 && ackStatus != HTTP_CODE_OK; ++attempt) {
        WiFiClientSecure ackClient;
        ackClient.setCACert(kKentWynnRootCa);
        HTTPClient ackRequest;
        ackRequest.setConnectTimeout(1500);
        ackRequest.setTimeout(2500);
        if (ackRequest.begin(ackClient, ackUrl)) {
            ackRequest.addHeader("X-AI-Token", activeRobotToken());
            ackRequest.addHeader("Content-Type", "application/json");
            ackStatus = ackRequest.POST(body);
            ackRequest.end();
        }
        if (ackStatus != HTTP_CODE_OK && attempt < 3) delay(150);
    }
    Serial.printf("[Z2 CONTROL] ack=%s HTTP %d\n", executed ? "executed" : "rejected", ackStatus);
}

void pollRobotCommands()
{
    if (gAiMotionStopAtMs &&
        static_cast<int32_t>(millis() - gAiMotionStopAtMs) >= 0) {
        gAiMotionStopAtMs = 0;
        stopMotors();
        addRealtimeLog("AI motion deadline reached - stopped locally");
        sendDeviceEvent("Safety", "AI motion stopped", "Local duration limit reached");
    }
    static unsigned long lastPollMs = 0;
    String realtimeCommand;
    const bool hasRealtimeCommand = peekCloudCommand(realtimeCommand);
    if (hasRealtimeCommand) {
        Serial.printf("[Z2 CONTROL] poll queued=%u synced=%d wifi=%d voice=%s speaker=%d bytes=%u\n",
                      gCloudCommandQueueSize, gRobotConfigSynced,
                      WiFi.status() == WL_CONNECTED, realtimeStateName(),
                      gRealtimeSpeakerActive,
                      static_cast<unsigned>(realtimeCommand.length()));
    }
    // The persistent control socket is authoritative. HTTPS polling is only
    // a recovery path and must never become a third concurrent TLS client.
    if (!hasRealtimeCommand && gControlSocket.available()) return;
    const bool voiceBusy = gRealtimeSpeakerActive ||
        gRealtimeState == RealtimeChatState::Connecting ||
        gRealtimeState == RealtimeChatState::Recording ||
        gRealtimeState == RealtimeChatState::Waiting ||
        gRealtimeState == RealtimeChatState::Speaking;
    // Commands already delivered over the authenticated control socket are
    // safe to execute with firmware defaults even while a config refresh is
    // pending. Only the HTTPS recovery poll depends on managed config sync.
    if (WiFi.status() != WL_CONNECTED ||
        (!hasRealtimeCommand && !gRobotConfigSynced) ||
        (!hasRealtimeCommand && gRealtimeSocket.available()) ||
        (!hasRealtimeCommand &&
         (millis() - lastPollMs < 5000 || voiceBusy))) return;
    if (!hasRealtimeCommand) lastPollMs = millis();
    JsonDocument document;
    DeserializationError error;
    if (hasRealtimeCommand) {
        error = deserializeJson(document, realtimeCommand);
    } else {
        WiFiClientSecure client;
        client.setCACert(kKentWynnRootCa);
        HTTPClient request;
        request.setConnectTimeout(600);
        request.setTimeout(1000);
        if (!request.begin(client, "https://api.kentwynn.com/v1/robot/commands/next")) return;
        request.addHeader("X-AI-Token", activeRobotToken());
        const int status = request.GET();
        if (status != HTTP_CODE_OK) { request.end(); return; }
        error = deserializeJson(document, request.getString());
        request.end();
    }
    if (error) {
        Serial.printf("[Z2 CONTROL] JSON rejected: %s payload=%s\n",
                      error.c_str(), realtimeCommand.c_str());
        if (hasRealtimeCommand) discardCloudCommand();
        addRealtimeLog("Control rejected: invalid command payload");
        return;
    }
    // WebSocket delivery stores the command itself as a flat object, e.g.
    // {"command":"drive","value":"F"}. The HTTPS recovery response wraps
    // that same object in {"command": {...}}. Only unwrap an actual object;
    // a flat command's string-valued "command" field is not an envelope.
    JsonObject item = document["command"].is<JsonObject>()
        ? document["command"].as<JsonObject>() : document.as<JsonObject>();
    if (item.isNull() || item["command"].isNull()) {
        Serial.println("[Z2 CONTROL] rejected: command object or name missing");
        if (hasRealtimeCommand) discardCloudCommand();
        return;
    }
    String commandId = item["id"] | "";
    String command = item["command"] | "";
    String value = item["value"] | "";
    const bool aiToolboxCommand = String(item["source"] | "") == "ai_toolbox";
    command.toLowerCase(); value.toUpperCase();
    const int speed = constrain(item["speed"] | 160, 70, 255);
    const unsigned long durationMs = constrain(item["duration_ms"] | 1000, 100, 5000);
    // Never mix a test sound into streamed TTS. Keep it queued and execute it
    // as soon as reply playback finishes; FollowUp/listening is safe now.
    if (hasRealtimeCommand && command == "sound" &&
        gRealtimeState == RealtimeChatState::Speaking)
        return;
    if (hasRealtimeCommand) discardCloudCommand();
    Serial.printf("[Z2 CONTROL] command=%s value=%s speed=%d\n",
                  command.c_str(), value.c_str(), speed);
    bool executed = false;
    String detail = command + (value.isEmpty() ? "" : String(" ") + value);
    if (!commandId.isEmpty() && commandId == gLastCloudCommandId) {
        addRealtimeLog(String("Control duplicate ignored: ") + detail);
        acknowledgeCloudCommand(commandId, gLastCloudCommandExecuted,
                                gLastCloudCommandDetail);
        return;
    }
    if (command == "stop") {
        gAiMotionStopAtMs = 0;
        if (!aiToolboxCommand) activateControlLease(true);
        disableAutoDrive("cloud emergency stop");
        gCliffRecoveryActive = false;
        clearPresentation(PresentationOwner::Control);
        executed = true;
    } else if (command == "drive" && gToolboxControlEnabled && gCapabilityMovement && value.length() == 1) {
        if (!aiToolboxCommand) activateControlLease(false);
        disableAutoDrive("cloud manual drive");
        executed = applyDrive(value[0], speed);
        if (executed && aiToolboxCommand)
            gAiMotionStopAtMs = millis() + durationMs;
        if (!executed) {
            if (!gVirtualPowerOn) detail += " blocked: virtual power is off";
            else if (gCliffRecoveryActive) detail += " blocked: cliff recovery is active";
            else if (gCliffDetected[0] || gCliffDetected[1]) detail += " blocked: cliff sensor detects an edge";
            else if (gUltrasonicValid && gLatestDistanceCm <= kEmergencyStopDistanceCm)
                detail += String(" blocked: sonar obstacle at ") + String(gLatestDistanceCm, 1) + " cm";
            else detail += " blocked: ToF obstacle in requested direction";
        }
    } else if (command == "auto" && gToolboxControlEnabled && gCapabilityAutonomousDrive) {
        if (!aiToolboxCommand) activateControlLease(value == "OFF");
        if (value == "OFF") { disableAutoDrive("cloud auto stop"); executed = true; }
        else if (gVirtualPowerOn && autoSensorsReady() &&
                 !gCliffDetected[0] && !gCliffDetected[1]) {
            stopMotors(); gAutoDriveEnabled = true;
            gCollisionWarningLatched = false; gSafetyStopActive = false;
            requestPresentation(PresentationOwner::Control, gPresetControlAuto, "auto");
            startAutoState(AutoDriveState::Cruise);
            executed = true;
        } else {
            detail += " blocked because power, sensors, or cliff state is unsafe";
        }
    } else if (command == "display" && gCapabilityDisplay) {
        if (value == "OFF") gDisplayMode = DisplayMode::Off;
        else if (value == "INFO") gDisplayMode = DisplayMode::Info;
        else if (value == "SENSORS") gDisplayMode = DisplayMode::Sensors;
        else { gDisplayMode = DisplayMode::Face;
            gFaceEmotion = presentationEmotion(value); }
        gLastOledUpdateMs = 0;
        executed = true;
    } else if (command == "sound" && gCapabilitySpeaker && gVirtualPowerOn && gSpeakerReady) {
        playSound(value);
        executed = true;
    } else if (command == "ring" && gCapabilityLedRing) {
        setRingMode(value, 80);
        executed = true;
    } else if (command == "traffic") {
        if (value == "AUTO") {
            gTrafficLightOverride = "AUTO";
            gTrafficLightOverrideUntilMs = 0;
            executed = true;
        } else if (value == "OFF" || value == "RED" ||
                   value == "YELLOW" || value == "GREEN") {
            gTrafficLightOverride = value;
            gTrafficLightOverrideUntilMs = millis() + durationMs;
            executed = true;
        }
    }
    if (!executed && detail.indexOf("blocked") < 0) detail += " unavailable or disabled";
    addRealtimeLog(String("Control ") + (executed ? "executed: " : "rejected: ") + detail);
    gLastCloudCommandId = commandId;
    gLastCloudCommandExecuted = executed;
    gLastCloudCommandDetail = detail;
    acknowledgeCloudCommand(commandId, executed, detail);
}

void saveRobotDeviceToken(const String &token)
{
    gRobotDeviceToken = token;
    gRobotDeviceToken.trim();
    gRobotPreferences.putString("device-token", gRobotDeviceToken);
    gRobotConfigSynced = false;
}

bool reportRobotStatus()
{
    if (WiFi.status() != WL_CONNECTED || !gRobotDeviceToken.startsWith("rk_"))
        return false;
    WiFiClientSecure client;
    client.setCACert(kKentWynnRootCa);
    HTTPClient request;
    request.setConnectTimeout(600);
    request.setTimeout(1000);
    if (!request.begin(client, "https://api.kentwynn.com/v1/robot/status"))
        return false;
    request.addHeader("X-AI-Token", activeRobotToken());
    request.addHeader("Content-Type", "application/json");
    const String payload = String("{\"firmware_version\":\"") +
                           kFirmwareVersion + "\"}";
    const int status = request.POST(payload);
    request.end();
    Serial.printf("[Z2 ONBOARD] status report HTTP %d\n", status);
    return status >= 200 && status < 300;
}

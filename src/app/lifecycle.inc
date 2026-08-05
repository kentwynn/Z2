void setup()
{
    Serial.begin(115200);
    delay(300);
    loadRobotOnboardingState();
    Serial.printf("\n%s two-wheel controller starting\n", kProductName);
    Serial.printf("[Z2 CHAT] realtime=%s credentials=%s endpoint=wss://%s%s\n",
                  Z2_REALTIME_ENABLED ? "enabled" : "disabled",
                  hasConfiguredRealtimeToken() ? "configured" : "missing",
                  Z2_REALTIME_HOST, Z2_REALTIME_PATH);
    Serial.println("[Z2 WAKE] phrase is managed by Kent Wynn API");
    gPreferences.begin("z2-light", false);
    gAutoOffLux = constrain(
        static_cast<int>(gPreferences.getUShort("off-lux", 300)), 50, 1000);
    gLightSensitivity = constrain(
        static_cast<int>(gPreferences.getUChar("sensitivity", 7)), 1, 10);

    pinMode(kStandbyPin, OUTPUT);
    pinMode(kAin1Pin, OUTPUT);
    pinMode(kAin2Pin, OUTPUT);
    pinMode(kBin1Pin, OUTPUT);
    pinMode(kBin2Pin, OUTPUT);
    ledcSetup(kRightPwmChannel, kPwmFrequencyHz, kPwmResolutionBits);
    ledcSetup(kLeftPwmChannel, kPwmFrequencyHz, kPwmResolutionBits);
    ledcAttachPin(kPwmAPin, kRightPwmChannel);
    ledcAttachPin(kPwmBPin, kLeftPwmChannel);
    digitalWrite(kStandbyPin, HIGH);
    stopMotors();

    pinMode(kTrafficGreenPin, OUTPUT);
    pinMode(kTrafficYellowPin, OUTPUT);
    pinMode(kTrafficRedPin, OUTPUT);
    gTrafficLightStartedMs = millis();
    setTrafficLight(true, false, false);

    if (kCliffSensorsEnabled) {
        pinMode(kCliffLeftPin, INPUT);
        pinMode(kCliffRightPin, INPUT);
        Serial.printf("[Z2 CLIFF] left GPIO%d right GPIO%d detected-level=%s\n",
                      kCliffLeftPin, kCliffRightPin,
                      kCliffDetectedLevel == HIGH ? "HIGH" : "LOW");
    } else {
        Serial.println("[Z2 CLIFF] disabled: GPIO35/36 are reserved by octal PSRAM");
    }

    pinMode(kUltrasonicTriggerPin, OUTPUT);
    pinMode(kUltrasonicEchoPin, INPUT);
    digitalWrite(kUltrasonicTriggerPin, LOW);
    Serial.printf("[Z2 SONAR] HY-SRF05 TRIGGER GPIO%d ECHO GPIO%d waiting for echo\n",
                  kUltrasonicTriggerPin, kUltrasonicEchoPin);

    for (uint8_t index = 0; index < kTofCount; ++index) {
        pinMode(kTofXshutPins[index], OUTPUT);
        digitalWrite(kTofXshutPins[index], LOW);
    }

    gRing.begin();
    gRing.setBrightness(kRingDefaultBrightness);
    gRing.clear();
    gRing.show();
    Serial.printf("[Z2 RING] NeoPixel GPIO%d pixels=%d ready\n",
                  kRingDataPin, kRingPixelCount);

    pinMode(kSpeakerShutdownPin, OUTPUT);
    digitalWrite(kSpeakerShutdownPin, HIGH);
    gSpeakerReady = initAudio();
    gMicrophoneReady = gSpeakerReady;
    if (gSpeakerReady) startRealtimePlaybackTask();
    Serial.printf("[Z2 AUDIO] WS/LRC GPIO%d SCK/BCLK GPIO%d speaker DIN GPIO%d mic SD GPIO%d amp SD GPIO%d init=%s\n",
                  static_cast<int>(kSpeakerWsPin), static_cast<int>(kSpeakerBclkPin),
                  static_cast<int>(kSpeakerDataPin), static_cast<int>(kMicrophoneDataPin),
                  kSpeakerShutdownPin,
                  gSpeakerReady ? "OK" : "FAILED");

    startNetwork();
    startOta();
    syncManagedRobotConfig();
    reportRobotStatus();
    setupRealtimeConversation();
    setupControlChannel();

    Wire.begin(kOledSdaPin, kOledSclPin);
    gOledReady = gDisplay.begin(SSD1306_SWITCHCAPVCC, kOledAddress);
    Serial.printf("[Z2 OLED] SDA GPIO%d SCL GPIO%d address=0x%02X init=%s\n",
                  kOledSdaPin, kOledSclPin, kOledAddress,
                  gOledReady ? "OK" : "FAILED");
    gLightSensorReady = gLightSensor.begin(&Wire);
    if (gLightSensorReady) {
        gLightSensor.setGain(VEML7700_GAIN_1);
        gLightSensor.setIntegrationTime(VEML7700_IT_100MS);
    } else {
        gRingMode = RingMode::Off;
        refreshRing();
    }
    Serial.printf("[Z2 LIGHT] VEML7700 SDA GPIO%d SCL GPIO%d address=0x%02X init=%s\n",
                  kOledSdaPin, kOledSclPin, kLightSensorAddress,
                  gLightSensorReady ? "OK" : "FAILED");
    initTofSensors();
    updateOled();

    gServer.on("/", HTTP_GET, [] { gServer.send_P(200, "text/html", kControlPage); });
    gServer.on("/drive", HTTP_GET, handleDrive);
    gServer.on("/power", HTTP_GET, [] {
        const bool turnOn = gServer.hasArg("state") && gServer.arg("state") == "on";
        setVirtualPowerState(turnOn, turnOn ? "dashboard All On" : "dashboard All Off");
        gServer.send(200, "application/json",
                     String("{\"power\":") + (gVirtualPowerOn ? "true" : "false") + "}");
    });
    gServer.on("/auto", HTTP_GET, [] {
        const bool enable = gServer.hasArg("enabled") && gServer.arg("enabled") == "1";
        if (!enable) {
            noteDashboardActivity();
            disableAutoDrive("dashboard Stop auto");
            gServer.send(200, "application/json",
                         "{\"enabled\":false,\"state\":\"OFF\",\"reason\":\"Auto drive stopped\"}");
            return;
        }
        if (!gVirtualPowerOn) {
            gServer.send(409, "application/json",
                         "{\"enabled\":false,\"state\":\"OFF\",\"reason\":\"Turn All On first\"}");
            return;
        }
        noteDashboardActivity();
        if (!autoSensorsReady()) {
            disableAutoDrive("cannot start; front sensor or sonar unavailable");
            gServer.send(503, "application/json",
                         "{\"enabled\":false,\"state\":\"OFF\",\"reason\":\"Front sensors or sonar not ready\"}");
            return;
        }
        if (gCliffDetected[0] || gCliffDetected[1]) {
            disableAutoDrive("cannot start; cliff detected");
            gServer.send(409, "application/json",
                         "{\"enabled\":false,\"state\":\"OFF\",\"reason\":\"Cliff detected\"}");
            return;
        }
        stopMotors();
        gAutoDriveEnabled = true;
        gCollisionWarningLatched = false;
        gSafetyStopActive = false;
        startAutoState(AutoDriveState::Cruise);
        gServer.send(200, "application/json",
                     "{\"enabled\":true,\"state\":\"CRUISE\"}");
    });
    gServer.on("/stop", HTTP_ANY, [] {
        disableAutoDrive("emergency Stop");
        gCliffRecoveryActive = false;
        gServer.send(200, "text/plain", "stopped");
    });
    gServer.on("/sound", HTTP_GET, [] {
        if (!gVirtualPowerOn || !gSpeakerReady) {
            gServer.send(503, "text/plain", "speaker unavailable");
            return;
        }
        noteDashboardActivity();
        disableAutoDrive("sound requested");
        playSound(gServer.arg("name"));
        gServer.send(200, "text/plain", "played");
    });
    gServer.on("/volume", HTTP_GET, [] {
        noteDashboardActivity();
        gSpeakerVolumePercent = constrain(
            gServer.hasArg("value") ? gServer.arg("value").toInt() : 60,
            0, 100);
        Serial.printf("[Z2 SPEAKER] volume=%u%%\n", gSpeakerVolumePercent);
        gServer.send(200, "application/json",
                     String("{\"volume\":") + gSpeakerVolumePercent + "}");
    });
    gServer.on("/display", HTTP_GET, [] {
        if (!gVirtualPowerOn) {
            gServer.send(409, "text/plain", "Turn All On first");
            return;
        }
        noteDashboardActivity();
        String mode = gServer.hasArg("mode") ? gServer.arg("mode") : "FACE";
        mode.toUpperCase();
        if (mode == "OFF") gDisplayMode = DisplayMode::Off;
        else if (mode == "INFO") gDisplayMode = DisplayMode::Info;
        else if (mode == "SENSORS") gDisplayMode = DisplayMode::Sensors;
        else {
            gDisplayMode = DisplayMode::Face;
            gFaceEmotion = presentationEmotion(mode);
        }
        gLastOledUpdateMs = 0;
        Serial.printf("[Z2 DISPLAY] mode=%s\n", mode.c_str());
        gServer.send(200, "text/plain", "display updated");
    });
    gServer.on("/health", HTTP_GET, [] {
        const bool wifiReady =
            WiFi.status() == WL_CONNECTED || WiFi.getMode() == WIFI_AP;
        gServer.send(200, "application/json",
                     String("{\"wifi\":") + (wifiReady ? "true" : "false") +
                     ",\"power\":" + (gVirtualPowerOn ? "true" : "false") +
                     ",\"chatConfigured\":" +
                     (Z2_REALTIME_ENABLED && hasConfiguredRealtimeToken() ? "true" : "false") +
                     ",\"chatState\":\"" + realtimeStateName() + "\"" +
                     ",\"mic\":" + (gMicrophoneReady ? "true" : "false") +
                     ",\"light\":" + (gLightSensorReady ? "true" : "false") +
                     ",\"audio\":" + (gSpeakerReady ? "true" : "false") +
                     ",\"sonar\":" + (gUltrasonicValid ? "true" : "false") +
                     ",\"distance\":" + String(gDistanceCm, 1) +
                     ",\"tofLeft\":" + (gTofValid[0] ? "true" : "false") +
                     ",\"tofLeftReady\":" + (gTofReady[0] ? "true" : "false") +
                     ",\"tofRight\":" + (gTofValid[1] ? "true" : "false") +
                     ",\"tofRightReady\":" + (gTofReady[1] ? "true" : "false") +
                     ",\"tofBack\":" + (gTofValid[2] ? "true" : "false") +
                     ",\"tofBackReady\":" + (gTofReady[2] ? "true" : "false") +
                     ",\"leftMm\":" + gTofDistanceMm[0] +
                     ",\"rightMm\":" + gTofDistanceMm[1] +
                     ",\"backMm\":" + gTofDistanceMm[2] +
                     ",\"leftType\":\"" + (gTofType[0] == 1 ? "L0X" : gTofType[0] == 2 ? "L1X" : "--") + "\"" +
                     ",\"rightType\":\"" + (gTofType[1] == 1 ? "L0X" : gTofType[1] == 2 ? "L1X" : "--") + "\"" +
                     ",\"backType\":\"" + (gTofType[2] == 1 ? "L0X" : gTofType[2] == 2 ? "L1X" : "--") + "\"" +
                     ",\"lux\":" + String(gAmbientLux, 1) +
                     ",\"cliffLeft\":" + (gCliffDetected[0] ? "true" : "false") +
                     ",\"cliffRight\":" + (gCliffDetected[1] ? "true" : "false") +
                     ",\"cliffLeftRaw\":" + (gCliffRawHigh[0] ? "1" : "0") +
                     ",\"cliffRightRaw\":" + (gCliffRawHigh[1] ? "1" : "0") +
                     ",\"cliffRecovery\":" + (gCliffRecoveryActive ? "true" : "false") +
                     ",\"autoDrive\":" + (gAutoDriveEnabled ? "true" : "false") +
                     ",\"autoState\":\"" + autoDriveStateName() + "\"" +
                     ",\"ringMode\":\"" + ringModeName() + "\"" +
                     ",\"ringBrightness\":" + gRing.getBrightness() +
                     ",\"moving\":" + (gMoving ? "true" : "false") + "}");
    });
    gServer.on("/chatlog", HTTP_GET, [] {
        JsonDocument document;
        document["configured"] = Z2_REALTIME_ENABLED && hasConfiguredRealtimeToken();
        document["state"] = realtimeStateName();
        document["voice"] = microphoneSpeechNow();
        document["name"] = gManagedRobotName;
        document["robot_id"] = Z2_ROBOT_ID;
        document["wake_word"] = gManagedWakeWord;
        document["config_synced"] = gRobotConfigSynced;
        JsonArray entries = document["entries"].to<JsonArray>();
        const uint8_t start =
            (gRealtimeLogWrite + kRealtimeLogCount - gRealtimeLogSize) % kRealtimeLogCount;
        for (uint8_t index = 0; index < gRealtimeLogSize; ++index)
            entries.add(gRealtimeLog[(start + index) % kRealtimeLogCount]);
        String payload;
        serializeJson(document, payload);
        gServer.send(200, "application/json", payload);
    });
    gServer.on("/mic", HTTP_GET, [] {
        gServer.send(200, "application/json",
                     String("{\"ready\":") + (gMicrophoneReady ? "true" : "false") +
                     ",\"calibrated\":" +
                     (gMicrophoneCalibrationFrames >= kMicrophoneCalibrationFrameCount ? "true" : "false") +
                     ",\"voice\":" + (microphoneSpeechNow() ? "true" : "false") +
                     ",\"level\":" + gMicrophoneRms +
                     ",\"peak\":" + gMicrophonePeak +
                     ",\"floor\":" + gMicrophoneFloor + "}");
    });
    gServer.on("/light", HTTP_GET, [] {
        gServer.send(200, "application/json",
                     String("{\"ready\":") + (gLightSensorReady ? "true" : "false") +
                     ",\"lux\":" + String(gAmbientLux, 1) +
                     ",\"smoothedLux\":" + String(gSmoothedLux, 1) +
                     ",\"condition\":\"" + ambientConditionName() +
                     "\",\"auto\":" + (gRingMode == RingMode::Auto ? "true" : "false") +
                     ",\"output\":\"" + autoLightOutputName() +
                     "\",\"brightness\":" + gAutoRingBrightness +
                     ",\"sensitivity\":" + gLightSensitivity +
                     ",\"offLux\":" + gAutoOffLux + "}");
    });
    gServer.on("/lightconfig", HTTP_GET, [] {
        noteDashboardActivity();
        if (gServer.hasArg("sensitivity")) {
            gLightSensitivity = constrain(gServer.arg("sensitivity").toInt(), 1, 10);
        }
        if (gServer.hasArg("offlux")) {
            gAutoOffLux = constrain(gServer.arg("offlux").toInt(), 50, 1000);
        }
        gPreferences.putUChar("sensitivity", gLightSensitivity);
        gPreferences.putUShort("off-lux", gAutoOffLux);
        Serial.printf("[Z2 LIGHT] sensitivity=%u off-above=%u lux\n",
                      gLightSensitivity, gAutoOffLux);
        gServer.send(200, "application/json",
                     String("{\"sensitivity\":") + gLightSensitivity +
                     ",\"offLux\":" + gAutoOffLux + "}");
    });
    gServer.on("/ring", HTTP_GET, [] {
        if (!gVirtualPowerOn) {
            gServer.send(409, "text/plain", "Turn All On first");
            return;
        }
        noteDashboardActivity();
        const String mode = gServer.hasArg("mode") ? gServer.arg("mode") : "OFF";
        const int brightness = gServer.hasArg("brightness")
            ? gServer.arg("brightness").toInt()
            : kRingDefaultBrightness;
        setRingMode(mode, brightness);
        gServer.send(200, "text/plain", "ring updated");
    });
    gServer.on("/status", HTTP_GET, [] {
        gServer.send(200, "application/json",
                     String("{\"robot\":\"Z2\",\"moving\":") + (gMoving ? "true" : "false") +
                     ",\"command\":\"" + gCommand + "\",\"speed\":" + gSpeed +
                     ",\"power\":" + (gVirtualPowerOn ? "true" : "false") +
                     ",\"autoDrive\":" + (gAutoDriveEnabled ? "true" : "false") +
                     ",\"autoState\":\"" + autoDriveStateName() + "\"}");
    });
    gServer.on("/robot/credential", HTTP_POST, [] {
        JsonDocument document;
        if (deserializeJson(document, gServer.arg("plain"))) {
            gServer.send(400, "application/json", "{\"detail\":\"Invalid JSON\"}");
            return;
        }
        const String token = document["device_token"] | "";
        if (!token.startsWith("rk_") || token.indexOf('.') < 4) {
            gServer.send(400, "application/json", "{\"detail\":\"Invalid robot device token\"}");
            return;
        }
        closeRealtimeConversation("installing device credential");
        saveRobotDeviceToken(token);
        const bool synced = syncManagedRobotConfig();
        if (synced) reportRobotStatus();
        gServer.send(synced ? 200 : 202, "application/json",
                     synced ? "{\"status\":\"installed\",\"config_synced\":true}"
                            : "{\"status\":\"installed\",\"config_synced\":false}");
    });
    gServer.on("/robot/config/sync", HTTP_POST, [] {
        const bool synced = syncManagedRobotConfig();
        if (synced) {
            reportRobotStatus();
            // A realtime session snapshots name, voice, language, and prompt
            // when session.start is accepted. Close any older session so the
            // next wake reconnects with the newly synchronized API settings.
            closeRealtimeConversation("configuration synchronized");
        }
        gServer.send(synced ? 200 : 503, "application/json",
                     synced ? "{\"status\":\"synced\"}" : "{\"detail\":\"Sync failed\"}");
    });
    gServer.onNotFound([] { gServer.send(404, "text/plain", "Z2: not found"); });
    gServer.begin();
    gLastDashboardActivityMs = millis();
    Serial.println("[Z2] web controller ready");
}

void loop()
{
    // Audio capture is latency-sensitive. Service the I2S RX DMA before any
    // HTTP or WebSocket polling so the always-on control channel cannot delay
    // VAD long enough to miss a short wake phrase.
    if (gVirtualPowerOn) updateMicrophone();

    ArduinoOTA.handle();
    gServer.handleClient();
    updateRealtimeConversation();
    updateControlChannel();
    pollManagedRobotConfig();
    pollRobotCommands();
    if (gVirtualPowerOn) {
        updateAmbientLight();
        updateUltrasonic();
        updateTofSensors();
        updateCliffSensors();
        updateTouchReaction();
        if (touchReactionActive()) cancelRealtimeRecordingForProximity();
        if (gEmergencyStopRequested) {
            gEmergencyStopRequested = false;
            const float stoppedAtCm = gDistanceCm;
            stopMotors();
            gSafetyStopActive = true;
            Serial.printf("[Z2 SAFETY] emergency stop at %.1f cm\n", stoppedAtCm);
            if (!gAutoDriveEnabled) playCollisionWarning();
            else gCollisionWarningLatched = true;
        }
        if (gCollisionWarningLatched && allSafetyZonesClear()) {
            gCollisionWarningLatched = false;
            gSafetyStopActive = false;
        }
        updateAutoDrive();
        updateRingAnimation();
        updateOled();
        if (gMoving && millis() - gLastDriveCommandMs > kCommandTimeoutMs) {
            stopMotors();
            Serial.println("[Z2] command timeout; motors stopped");
        }
        updateTrafficLight();
    }
    delay(2);
}

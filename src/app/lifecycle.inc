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

    Wire.begin(kOledSdaPin, kOledSclPin);
    gOledReady = gDisplay.begin(SSD1306_SWITCHCAPVCC, kOledAddress);
    Serial.printf("[Z2 OLED] SDA GPIO%d SCL GPIO%d address=0x%02X init=%s\n",
                  kOledSdaPin, kOledSclPin, kOledAddress,
                  gOledReady ? "OK" : "FAILED");
    startNetwork();
    if (gWifiSetupMode) {
        gRing.clear();
        gRing.show();
        showWifiSetupQr();
    }
    startOta();
    syncManagedRobotConfig();
    reportRobotStatus();
    setupRealtimeConversation();
    setupControlChannel();
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

    gServer.on("/", HTTP_GET, [] {
        if (!gWifiSetupMode) {
            gServer.send_P(200, "text/html", kControlPage);
            return;
        }
        static const char kWifiSetupPage[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Connect Z2</title><style>body{font:16px system-ui;background:#0b0b0b;color:#f4f4f1;margin:0}.box{max-width:430px;margin:auto;padding:28px 20px}h1{font-size:28px}p{color:#aaa;line-height:1.5}label{display:block;margin:18px 0 7px}select,input,button{box-sizing:border-box;width:100%;padding:14px;border-radius:12px;border:1px solid #444;background:#171717;color:#fff;font:inherit}button{margin-top:20px;background:#fff;color:#111;font-weight:700}.note{font-size:13px}.hidden{display:none}.signal{color:#8fd3a3}</style></head><body><main class="box"><h1>Connect Z2</h1><p>Select your home Wi-Fi. Z2 will test it before saving it.</p><form method="post" action="/setup/wifi"><label>Nearby Wi-Fi</label><select id="network"><option value="">Scanning nearby networks…</option></select><div id="manual" class="hidden"><label>Wi-Fi name</label><input id="manualName" maxlength="32" autocomplete="off"></div><input id="ssid" name="ssid" type="hidden"><label>Password</label><input name="password" type="password" maxlength="63" autocomplete="off"><button id="connect" disabled>Connect Z2</button></form><p id="scanStatus" class="note">Looking for nearby networks…</p><p class="note">Your Wi-Fi password stays on this robot and is never sent to Kent Wynn.</p></main><script>const list=document.querySelector('#network'),ssid=document.querySelector('#ssid'),manual=document.querySelector('#manual'),manualName=document.querySelector('#manualName'),button=document.querySelector('#connect'),status=document.querySelector('#scanStatus');function choose(){const other=list.value==='__other__';manual.classList.toggle('hidden',!other);ssid.value=other?manualName.value:list.value;button.disabled=!ssid.value}list.onchange=choose;manualName.oninput=choose;fetch('/setup/networks',{cache:'no-store'}).then(r=>r.json()).then(data=>{list.innerHTML='';data.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' · '+(n.rssi>-60?'Strong':n.rssi>-75?'Good':'Weak');list.append(o)});const other=document.createElement('option');other.value='__other__';other.textContent='Other network…';list.append(other);status.textContent=data.networks.length+' networks found';choose()}).catch(()=>{list.innerHTML='<option value="__other__">Enter network manually…</option>';status.textContent='Scan unavailable';choose()});</script></body></html>)HTML";
        gServer.send_P(200, "text/html", kWifiSetupPage);
    });
    gServer.on("/setup/networks", HTTP_GET, [] {
        if (!gWifiSetupMode) {
            gServer.send(409, "application/json", "{\"detail\":\"Setup mode is not active\"}");
            return;
        }
        const int found = WiFi.scanNetworks(false, true);
        JsonDocument document;
        JsonArray networks = document["networks"].to<JsonArray>();
        for (int index = 0; index < found; ++index) {
            const String ssid = WiFi.SSID(index);
            if (ssid.isEmpty()) continue;
            bool duplicate = false;
            for (JsonObject item : networks)
                if (String(item["ssid"] | "") == ssid) { duplicate = true; break; }
            if (duplicate) continue;
            JsonObject item = networks.add<JsonObject>();
            item["ssid"] = ssid;
            item["rssi"] = WiFi.RSSI(index);
            item["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        }
        WiFi.scanDelete();
        String payload;
        serializeJson(document, payload);
        gServer.send(200, "application/json", payload);
    });
    gServer.on("/setup/wifi", HTTP_POST, [] {
        if (!gWifiSetupMode) {
            gServer.send(409, "text/plain", "Wi-Fi setup mode is not active");
            return;
        }
        String ssid = gServer.arg("ssid");
        const String password = gServer.arg("password");
        ssid.trim();
        if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63) {
            gServer.send(400, "text/plain", "Invalid Wi-Fi details");
            return;
        }
        gRobotPreferences.putString("wifi-next-s", ssid);
        gRobotPreferences.putString("wifi-next-p", password);
        gServer.send(200, "text/html", "<!doctype html><meta name=viewport content='width=device-width'><style>body{font:18px system-ui;padding:30px;background:#0b0b0b;color:white}</style><h1>Z2 is connecting</h1><p>You can close this page. If the new network fails, Z2 will return to the previous network or setup mode.</p>");
        delay(350);
        ESP.restart();
    });
    gServer.on("/setup/start", HTTP_POST, [] {
        if (gMoving) stopMotors();
        closeRealtimeConversation("Wi-Fi setup requested");
        gRobotPreferences.putBool("wifi-setup", true);
        gServer.send(202, "application/json", "{\"status\":\"restarting-in-setup-mode\"}");
        delay(350);
        ESP.restart();
    });
    gServer.on("/setup/factory-reset", HTTP_POST, [] {
        if (gMoving) stopMotors();
        closeRealtimeConversation("factory reset requested");
        gRobotPreferences.clear();
        gPreferences.clear();
        gServer.send(202, "application/json", "{\"status\":\"factory-reset\"}");
        delay(350);
        ESP.restart();
    });
    gServer.on("/generate_204", HTTP_GET, [] { gServer.sendHeader("Location", "http://192.168.4.1/", true); gServer.send(302, "text/plain", ""); });
    gServer.on("/hotspot-detect.html", HTTP_GET, [] { gServer.sendHeader("Location", "http://192.168.4.1/", true); gServer.send(302, "text/plain", ""); });
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
        requestPresentation(PresentationOwner::Control, gPresetControlAuto, "auto");
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
                     ",\"wifiSetup\":" + (gWifiSetupMode ? "true" : "false") +
                     ",\"otaReady\":" + (gOtaReady ? "true" : "false") +
                     ",\"firmwareVersion\":\"" + kFirmwareVersion + "\"" +
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
                     ",\"automaticSafetyRecovery\":" +
                     (gAutomaticSafetyRecovery ? "true" : "false") +
                     ",\"localRecoveryActive\":" +
                     (gLocalRecoveryActive ? "true" : "false") +
                     ",\"localRecoveryAttempted\":" +
                     (gLocalRecoveryAttempted ? "true" : "false") +
                     ",\"localRecoveryCommand\":\"" + String(gLocalRecoveryCommand) + "\"" +
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
        // Match the debounced WebRTC decision used by the conversation state
        // machine. Raw RMS crosses the adaptive gate frame-by-frame and made
        // the dashboard indicator flicker.
        document["voice"] = gVoiceDetected;
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
                     ",\"voice\":" + (gVoiceDetected ? "true" : "false") +
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
    // OTA credentials are accepted only over the physical USB serial link and
    // only while no credential exists. The secret is never echoed.
    handleUsbOtaProvisioning();
    // Audio capture is latency-sensitive. Service the I2S RX DMA before any
    // HTTP or WebSocket polling so the always-on control channel cannot delay
    // VAD long enough to miss a short wake phrase.
    if (gVirtualPowerOn) updateMicrophone();

    ArduinoOTA.handle();
    updateNetworkConnection();
    if (gWifiSetupMode) gSetupDns.processNextRequest();
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
        updateLocalSafetyRecovery();
        if (touchReactionActive()) cancelRealtimeRecordingForProximity();
        if (gEmergencyStopRequested) {
            gEmergencyStopRequested = false;
            const float stoppedAtCm = gDistanceCm;
            const char interruptedCommand = gCommand;
            stopMotors();
            gSafetyStopActive = true;
            requestPresentation(PresentationOwner::Safety,
                                gPresetSafetyObstacle, "obstacle", false);
            Serial.printf("[Z2 SAFETY] emergency stop at %.1f cm\n", stoppedAtCm);
            if (!gAutoDriveEnabled) playCollisionWarning();
            else gCollisionWarningLatched = true;
            if (!gCliffRecoveryActive && !gLocalRecoveryActive)
                tryStartLocalSafetyRecovery(interruptedCommand,
                                            "distance boundary");
        }
        if (gCollisionWarningLatched && allSafetyZonesClear()) {
            gCollisionWarningLatched = false;
            gSafetyStopActive = false;
            gLocalRecoveryAttempted = false;
            gLastRecoveryBlockedNoticeMs = 0;
        }
        updateAutoDrive();
        if (!gWifiSetupMode) updateRingAnimation();
        updateOled();
        // AI metric/angle motion has its own bounded local deadline. Do not let
        // the generic held-control watchdog truncate a calibrated 360-degree
        // turn at 1.2 seconds. Cliff/proximity safety still interrupts through
        // the normal sensor paths, and the AI deadline is capped locally.
        // pollRobotCommands() exclusively owns the terminal boundary for a
        // timed AI motion. Testing whether the deadline is still in the
        // future here creates a one-loop race: it can expire after the poll
        // check, this watchdog stops the motors, and the next poll falsely
        // reports an interrupted motion. Safety paths remain independent.
        const bool boundedAiMotionPending = gAiMotionStopAtMs != 0;
        if (gMoving && !boundedAiMotionPending &&
            millis() - gLastDriveCommandMs > kCommandTimeoutMs) {
            stopMotors();
            Serial.println("[Z2] command timeout; motors stopped");
        }
        updateTrafficLight();
    }
    delay(2);
}

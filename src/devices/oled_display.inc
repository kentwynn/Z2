void drawFace()
{
    // Animation behavior is cloud/toolbox-owned. A STATIC face must remain
    // static; only the explicitly configured IDLE animation adds a blink.
    const bool blinking =
        gFaceAnimation == "IDLE" && millis() % 4200UL > 4000UL;
    int8_t eyeOffset = 0;
    if (gCommand == 'L' || gCommand == 'X') eyeOffset = -4;
    else if (gCommand == 'R' || gCommand == 'Y') eyeOffset = 4;
    else if (gFaceAnimation == "LISTENING") {
        // Gently scan toward the speaker while listening.
        const uint8_t phase = (millis() / 260UL) % 8U;
        eyeOffset = phase < 4 ? static_cast<int8_t>(phase) - 2
                              : 5 - static_cast<int8_t>(phase);
    } else if (gFaceAnimation == "PROCESSING") {
        // Slower gaze movement makes model processing visibly alive.
        eyeOffset = static_cast<int8_t>((millis() / 520UL) % 3U) * 3 - 3;
    }

    if (gFaceEmotion == FaceEmotion::Surprised) {
        gDisplay.drawRoundRect(15 + eyeOffset, 6, 40, 52, 19, SSD1306_WHITE);
        gDisplay.drawRoundRect(73 + eyeOffset, 6, 40, 52, 19, SSD1306_WHITE);
        gDisplay.fillCircle(35 + eyeOffset, 32, 11, SSD1306_WHITE);
        gDisplay.fillCircle(93 + eyeOffset, 32, 11, SSD1306_WHITE);
        gDisplay.fillCircle(31 + eyeOffset, 27, 3, SSD1306_BLACK);
        gDisplay.fillCircle(89 + eyeOffset, 27, 3, SSD1306_BLACK);
    } else if (blinking) {
        for (int8_t thickness = -2; thickness <= 2; ++thickness) {
            gDisplay.drawLine(18 + eyeOffset, 31 + thickness,
                              50 + eyeOffset, 31 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(78 + eyeOffset, 31 + thickness,
                              110 + eyeOffset, 31 + thickness, SSD1306_WHITE);
        }
    } else if (gFaceEmotion == FaceEmotion::Love) {
        gDisplay.fillCircle(27 + eyeOffset, 22, 8, SSD1306_WHITE);
        gDisplay.fillCircle(41 + eyeOffset, 22, 8, SSD1306_WHITE);
        gDisplay.fillTriangle(19 + eyeOffset, 24, 49 + eyeOffset, 24,
                              34 + eyeOffset, 49, SSD1306_WHITE);
        gDisplay.fillCircle(85 + eyeOffset, 22, 8, SSD1306_WHITE);
        gDisplay.fillCircle(99 + eyeOffset, 22, 8, SSD1306_WHITE);
        gDisplay.fillTriangle(77 + eyeOffset, 24, 107 + eyeOffset, 24,
                              92 + eyeOffset, 49, SSD1306_WHITE);
    } else if (gFaceEmotion == FaceEmotion::Excited) {
        gDisplay.fillTriangle(34 + eyeOffset, 8, 39 + eyeOffset, 27,
                              29 + eyeOffset, 27, SSD1306_WHITE);
        gDisplay.fillTriangle(18 + eyeOffset, 18, 50 + eyeOffset, 18,
                              34 + eyeOffset, 43, SSD1306_WHITE);
        gDisplay.fillTriangle(92 + eyeOffset, 8, 97 + eyeOffset, 27,
                              87 + eyeOffset, 27, SSD1306_WHITE);
        gDisplay.fillTriangle(76 + eyeOffset, 18, 108 + eyeOffset, 18,
                              92 + eyeOffset, 43, SSD1306_WHITE);
    } else if (gFaceEmotion == FaceEmotion::Sad) {
        gDisplay.drawLine(18 + eyeOffset, 16, 49 + eyeOffset, 26, SSD1306_WHITE);
        gDisplay.drawLine(79 + eyeOffset, 26, 110 + eyeOffset, 16, SSD1306_WHITE);
        gDisplay.fillRoundRect(22 + eyeOffset, 26, 23, 27, 10, SSD1306_WHITE);
        gDisplay.fillRoundRect(83 + eyeOffset, 26, 23, 27, 10, SSD1306_WHITE);
        gDisplay.fillCircle(29 + eyeOffset, 34, 3, SSD1306_BLACK);
        gDisplay.fillCircle(90 + eyeOffset, 34, 3, SSD1306_BLACK);
    } else if (gFaceEmotion == FaceEmotion::Confused) {
        gDisplay.fillRoundRect(18 + eyeOffset, 13, 28, 39, 13, SSD1306_WHITE);
        gDisplay.fillCircle(36 + eyeOffset, 23, 4, SSD1306_BLACK);
        for (uint8_t thickness = 0; thickness < 4; ++thickness)
            gDisplay.drawLine(79 + eyeOffset, 31 + thickness,
                              109 + eyeOffset, 31 + thickness, SSD1306_WHITE);
        gDisplay.drawLine(78 + eyeOffset, 15, 108 + eyeOffset, 10, SSD1306_WHITE);
    } else if (gFaceEmotion == FaceEmotion::Thinking) {
        gDisplay.fillRoundRect(20 + eyeOffset, 16, 25, 30, 11, SSD1306_WHITE);
        gDisplay.fillRoundRect(83 + eyeOffset, 16, 25, 30, 11, SSD1306_WHITE);
        gDisplay.fillCircle(36 + eyeOffset, 24, 4, SSD1306_BLACK);
        gDisplay.fillCircle(99 + eyeOffset, 24, 4, SSD1306_BLACK);
        const uint8_t activeDot = (millis() / 260UL) % 3U;
        for (uint8_t dot = 0; dot < 3; ++dot) {
            if (dot == activeDot)
                gDisplay.fillCircle(56 + dot * 8, 56, 3, SSD1306_WHITE);
            else
                gDisplay.drawCircle(56 + dot * 8, 56, 2, SSD1306_WHITE);
        }
    } else if (gFaceEmotion == FaceEmotion::Angry) {
        gDisplay.fillTriangle(18 + eyeOffset, 19, 50 + eyeOffset, 29,
                              22 + eyeOffset, 48, SSD1306_WHITE);
        gDisplay.fillTriangle(78 + eyeOffset, 29, 110 + eyeOffset, 19,
                              106 + eyeOffset, 48, SSD1306_WHITE);
        gDisplay.fillCircle(31 + eyeOffset, 31, 3, SSD1306_BLACK);
        gDisplay.fillCircle(97 + eyeOffset, 31, 3, SSD1306_BLACK);
    } else if (gFaceEmotion == FaceEmotion::Sleepy) {
        for (uint8_t thickness = 0; thickness < 3; ++thickness) {
            gDisplay.drawLine(18 + eyeOffset, 28 + thickness,
                              34 + eyeOffset, 34 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(34 + eyeOffset, 34 + thickness,
                              50 + eyeOffset, 28 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(78 + eyeOffset, 28 + thickness,
                              94 + eyeOffset, 34 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(94 + eyeOffset, 34 + thickness,
                              110 + eyeOffset, 28 + thickness, SSD1306_WHITE);
        }
    } else if (gFaceEmotion == FaceEmotion::Happy) {
        for (uint8_t thickness = 0; thickness < 4; ++thickness) {
            gDisplay.drawLine(18 + eyeOffset, 34 + thickness,
                              34 + eyeOffset, 24 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(34 + eyeOffset, 24 + thickness,
                              50 + eyeOffset, 34 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(78 + eyeOffset, 34 + thickness,
                              94 + eyeOffset, 24 + thickness, SSD1306_WHITE);
            gDisplay.drawLine(94 + eyeOffset, 24 + thickness,
                              110 + eyeOffset, 34 + thickness, SSD1306_WHITE);
        }
    } else if (gFaceEmotion == FaceEmotion::Curious) {
        gDisplay.fillRoundRect(18 + eyeOffset, 10, 25, 43, 12, SSD1306_WHITE);
        gDisplay.fillRoundRect(82 + eyeOffset, 20, 19, 25, 9, SSD1306_WHITE);
        gDisplay.fillCircle(35 + eyeOffset, 19, 4, SSD1306_BLACK);
        gDisplay.fillCircle(95 + eyeOffset, 26, 3, SSD1306_BLACK);
    } else {
        const uint8_t eyeHeight = gVoiceDetected ? 42 : 36;
        const int16_t eyeTop = (64 - eyeHeight) / 2;
        gDisplay.fillRoundRect(20 + eyeOffset, eyeTop, 23, eyeHeight, 11,
                               SSD1306_WHITE);
        gDisplay.fillRoundRect(85 + eyeOffset, eyeTop, 23, eyeHeight, 11,
                               SSD1306_WHITE);
        // Small highlights create the luminous eye style from the reference.
        gDisplay.fillCircle(28 + eyeOffset, eyeTop + 9, 3, SSD1306_BLACK);
        gDisplay.fillCircle(93 + eyeOffset, eyeTop + 9, 3, SSD1306_BLACK);
    }

    if (gFaceAnimation == "LISTENING") {
        // Animated sound-level bars indicate active listening without LEDs.
        const uint8_t phase = (millis() / 120UL) % 4U;
        for (uint8_t bar = 0; bar < 3; ++bar) {
            const uint8_t height = 2 + ((phase + bar) % 4U) * 2;
            gDisplay.fillRoundRect(58 + bar * 5, 63 - height, 3, height,
                                   1, SSD1306_WHITE);
        }
    } else if (gFaceAnimation == "TALKING") {
        // Mouth movement follows playback time, giving every selected emotion
        // a living speaking animation instead of a frozen face.
        const uint8_t phase = (millis() / 105UL) % 5U;
        const uint8_t height = 2 + (phase <= 2 ? phase : 4 - phase) * 3;
        gDisplay.fillRoundRect(52, 62 - height, 24, height, 3, SSD1306_WHITE);
        if (height >= 5) gDisplay.drawLine(57, 60, 71, 60, SSD1306_BLACK);
    }
}

void drawRobotInfo()
{
    gDisplay.setTextColor(SSD1306_WHITE);
    gDisplay.setTextSize(2);
    gDisplay.setCursor(0, 0);
    gDisplay.print("Z2");
    gDisplay.setTextSize(1);
    gDisplay.setCursor(34, 3);
    gDisplay.print(gMoving ? "MOVING" : "READY");
    gDisplay.setCursor(0, 19);
    gDisplay.printf("IP %s", WiFi.localIP().toString().c_str());
    gDisplay.setCursor(0, 33);
    gDisplay.printf("Drive: %c   Speed: %d", gCommand, gSpeed);
    gDisplay.setCursor(0, 47);
    gDisplay.printf("Ring: %s", ringModeName());
}

void drawHealthTile(int16_t x, int16_t y, const char *label, bool healthy,
                    const char *detail)
{
    gDisplay.drawRoundRect(x, y, 40, 27, 4, SSD1306_WHITE);
    gDisplay.setTextSize(1);
    gDisplay.setCursor(x + 3, y + 3);
    gDisplay.print(label);
    if (healthy) {
        gDisplay.fillCircle(x + 33, y + 7, 3, SSD1306_WHITE);
    } else {
        gDisplay.setCursor(x + 31, y + 2);
        gDisplay.print("!");
    }
    gDisplay.setCursor(x + 3, y + 15);
    gDisplay.print(detail);
}

void drawSensorInfo()
{
    const uint8_t page = (millis() / 4000UL) % 3;
    gDisplay.setTextColor(SSD1306_WHITE);
    if (page == 0) {
        const bool wifiReady =
            WiFi.status() == WL_CONNECTED || WiFi.getMode() == WIFI_AP;
        drawHealthTile(1, 2, "WIFI", wifiReady, wifiReady ? "OK" : "OFF");
        drawHealthTile(44, 2, "MIC", gMicrophoneReady,
                       gMicrophoneReady ? "OK" : "FAIL");
        drawHealthTile(87, 2, "LUX", gLightSensorReady,
                       gLightSensorReady ? "OK" : "FAIL");
        drawHealthTile(1, 34, "SPKR", gSpeakerReady,
                       gSpeakerReady ? "OK" : "FAIL");
        drawHealthTile(44, 34, "SONAR", gUltrasonicValid,
                       gUltrasonicValid ? "OK" : "NO ECHO");
        drawHealthTile(87, 34, "LED", true, "READY");
    } else if (page == 1) {
        gDisplay.setTextSize(1);
        gDisplay.setCursor(0, 0);
        gDisplay.print("DIRECTIONAL RANGE");
        gDisplay.drawLine(0, 10, 127, 10, SSD1306_WHITE);
        gDisplay.setCursor(0, 17);
        if (gTofValid[0]) gDisplay.printf("Front L  %4u mm", gTofDistanceMm[0]);
        else gDisplay.print(gTofReady[0] ? "Front L  OUT RANGE" : "Front L  INIT FAIL");
        gDisplay.setCursor(0, 31);
        if (gTofValid[1]) gDisplay.printf("Front R  %4u mm", gTofDistanceMm[1]);
        else gDisplay.print(gTofReady[1] ? "Front R  OUT RANGE" : "Front R  INIT FAIL");
        gDisplay.setCursor(0, 45);
        if (gTofValid[2]) gDisplay.printf("Back     %4u mm", gTofDistanceMm[2]);
        else gDisplay.print(gTofReady[2] ? "Back     OUT RANGE" : "Back     INIT FAIL");
        gDisplay.setCursor(0, 57);
        gDisplay.print("STOP <= 100 mm");
    } else {
        gDisplay.setTextSize(1);
        gDisplay.setCursor(0, 0);
        gDisplay.print("Z2 LIVE SENSORS");
        gDisplay.drawLine(0, 10, 127, 10, SSD1306_WHITE);
        gDisplay.setCursor(0, 17);
        gDisplay.printf("Light   %6.1f lux", gAmbientLux);
        gDisplay.setCursor(0, 31);
        gDisplay.printf("Mic RMS %6lu", static_cast<unsigned long>(gMicrophoneRms));
        gDisplay.setCursor(0, 45);
        if (gUltrasonicValid) gDisplay.printf("Range   %6.1f cm", gDistanceCm);
        else gDisplay.print("Range   NO ECHO");
        gDisplay.setCursor(0, 57);
        gDisplay.printf("LED     %u/180", gAutoRingBrightness);
    }
}

String oledCompactLine(String value, uint8_t width = 20)
{
    value.replace("\n", " ");
    value.replace("\r", " ");
    value.trim();
    if (value.length() > width) value = value.substring(0, width - 1) + "~";
    return value;
}

void drawConversationConsole()
{
    gDisplay.setTextColor(SSD1306_WHITE);
    gDisplay.setTextSize(1);
    gDisplay.fillCircle(4, 4, millis() % 700UL < 350UL ? 2 : 1,
                        SSD1306_WHITE);
    gDisplay.setCursor(10, 0);
    gDisplay.printf("%s  %s", oledCompactLine(gOledAiTitle, 8).c_str(),
                    gOledLiveStatus.c_str());
    gDisplay.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    gDisplay.setCursor(0, 14);
    gDisplay.print("HEARD ");
    gDisplay.print(oledCompactLine(gOledLastHeard, 14));

    gDisplay.setCursor(0, 27);
    if (!gOledLiveDetail.isEmpty()) {
        gDisplay.print("TOOL  ");
        gDisplay.print(oledCompactLine(gOledLiveDetail, 14));
    } else if (!gOledReplyMood.isEmpty()) {
        gDisplay.print(oledCompactLine(gOledAiSymbol, 2));
        gDisplay.print(" ");
        gDisplay.print(oledCompactLine(gOledAiAccent, 17));
    } else {
        gDisplay.print("TOOL  --");
    }

    gDisplay.setCursor(0, 40);
    if (!gOledLastReply.isEmpty()) {
        gDisplay.print("Z2    ");
        gDisplay.print(oledCompactLine(gOledLastReply, 14));
    } else if (gOledLiveStatus == "SPEAK") {
        gDisplay.printf("AUDIO %u KB",
                        static_cast<unsigned>(gOledReplyAudioBytes / 1024));
    } else {
        gDisplay.print("Z2    waiting");
    }

    gDisplay.setCursor(0, 53);
    gDisplay.print(gOledLiveStatus == "FOLLOW-UP"
                       ? "Listening for reply"
                       : "Live conversation");
}

void updateOled()
{
    if (!gOledReady || millis() - gLastOledUpdateMs < 100) return;
    gLastOledUpdateMs = millis();
    if (gWifiSetupMode) {
        static bool qrVisible = true;
        const bool wantQr = millis() % 15000UL < 10000UL;
        if (wantQr != qrVisible) {
            qrVisible = wantQr;
            if (qrVisible) showWifiSetupQr();
            else showWifiSetupInstructions();
        }
        return;
    }
    gDisplay.clearDisplay();
    if (!gOledLiveStatus.isEmpty() &&
        static_cast<int32_t>(gOledConsoleUntilMs - millis()) > 0) {
        drawConversationConsole();
        gDisplay.display();
        return;
    }
    if (gOledLiveStatus == "READY") gOledLiveStatus = "";
    if (gDisplayMode == DisplayMode::Off) { gDisplay.display(); return; }
    // Legacy face presets remain protocol-compatible, but the OLED now uses
    // that slot for useful robot information instead of an animated face.
    if (gDisplayMode == DisplayMode::Face) drawRobotInfo();
    else if (gDisplayMode == DisplayMode::Info) drawRobotInfo();
    else drawSensorInfo();
    gDisplay.display();
}

void showWifiSetupInstructions()
{
    if (!gOledReady || !gWifiSetupMode) return;
    gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
    gDisplay.ssd1306_command(0x7F);
    gDisplay.clearDisplay();
    gDisplay.setTextSize(1);
    gDisplay.setTextColor(SSD1306_WHITE);
    gDisplay.setCursor(0, 0);  gDisplay.print("Z2 WI-FI SETUP");
    gDisplay.setCursor(0, 11); gDisplay.print("Network:");
    gDisplay.setCursor(0, 21); gDisplay.print(gWifiSetupSsid);
    gDisplay.setCursor(0, 33); gDisplay.print("Password:");
    gDisplay.setCursor(0, 43); gDisplay.print(gWifiSetupPassword);
    gDisplay.setCursor(0, 55); gDisplay.print("Open 192.168.4.1");
    gDisplay.display();
}

void drawLargeWifiQr(esp_qrcode_handle_t code)
{
    const int modules = esp_qrcode_get_size(code);
    constexpr int quiet = 2;
    constexpr int pixels = 60;
    const int left = (128 - pixels) / 2;
    const int top = (64 - pixels) / 2;
    gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
    gDisplay.ssd1306_command(0x25);
    gDisplay.clearDisplay();
    gDisplay.fillRect(left - quiet, top - quiet, pixels + quiet * 2,
                      pixels + quiet * 2, SSD1306_WHITE);
    for (int y = 0; y < modules; ++y) {
        const int y0 = top + (y * pixels) / modules;
        const int y1 = top + ((y + 1) * pixels) / modules;
        for (int x = 0; x < modules; ++x) {
            if (!esp_qrcode_get_module(code, x, y)) continue;
            const int x0 = left + (x * pixels) / modules;
            const int x1 = left + ((x + 1) * pixels) / modules;
            gDisplay.fillRect(x0, y0, x1 - x0, y1 - y0, SSD1306_BLACK);
        }
    }
    gDisplay.display();
}

void showWifiSetupQr()
{
    if (!gOledReady || !gWifiSetupMode) return;
    const String payload = String("WIFI:T:WPA;S:") + gWifiSetupSsid +
                           ";P:" + gWifiSetupPassword + ";;";
    esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
    config.display_func = drawLargeWifiQr;
    config.max_qrcode_version = 2;
    config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
    if (esp_qrcode_generate(&config, payload.c_str()) != ESP_OK)
        showWifiSetupInstructions();
}

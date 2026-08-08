void drawLegacyFace()
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
    } else if (gFaceAnimation == "TALKING") {
        // A tiny gaze pulse keeps speech alive without adding a mouth.
        eyeOffset = static_cast<int8_t>((millis() / 180UL) % 3U) - 1;
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

    // The OLED character is deliberately eyes-only. Listening, thinking, and
    // speaking remain visible through gaze, blink, and eye-shape animation;
    // extra meters, status dots, or a mouth make the tiny display look like a
    // diagnostic panel instead of a robot face.
}

const unsigned char *irisBitmapForEmotion(FaceEmotion emotion)
{
    switch (emotion) {
    case FaceEmotion::Happy: return Irisoled::happy;
    case FaceEmotion::Curious: return Irisoled::alert;
    case FaceEmotion::Sleepy: return Irisoled::sleepy;
    case FaceEmotion::Surprised: return Irisoled::surprised;
    case FaceEmotion::Excited: return Irisoled::excited;
    case FaceEmotion::Sad: return Irisoled::sad;
    case FaceEmotion::Love: return Irisoled::happy;
    case FaceEmotion::Confused: return Irisoled::disoriented;
    case FaceEmotion::Thinking: return Irisoled::focused;
    case FaceEmotion::Angry: return Irisoled::angry;
    case FaceEmotion::Alert: return Irisoled::alert;
    case FaceEmotion::Focused: return Irisoled::focused;
    case FaceEmotion::Furious: return Irisoled::furious;
    case FaceEmotion::Scared: return Irisoled::scared;
    case FaceEmotion::Worried: return Irisoled::worried;
    case FaceEmotion::Bored: return Irisoled::bored;
    case FaceEmotion::Despair: return Irisoled::despair;
    default: return Irisoled::normal;
    }
}

void drawFace()
{
    const unsigned long now = millis();
    const unsigned char *target = irisBitmapForEmotion(gFaceEmotion);
    if (gCommand == 'L' || gCommand == 'X') target = Irisoled::look_left;
    else if (gCommand == 'R' || gCommand == 'Y') target = Irisoled::look_right;
    else if (gFaceAnimation == "LISTENING") target = Irisoled::alert;
    else if (gFaceAnimation == "PROCESSING") target = Irisoled::focused;

    // Bitmap expressions cannot geometrically interpolate, so transition
    // through IrisOLED's eyelid frames. State entry always starts at phase 0
    // instead of selecting an arbitrary frame from the global boot clock.
    static const unsigned char *shown = nullptr;
    static const unsigned char *pending = nullptr;
    static unsigned long transitionStartedMs = 0;
    if (!shown) shown = target;
    if (pending && target == shown) pending = nullptr;
    if (target != shown && target != pending) {
        pending = target;
        transitionStartedMs = now;
    }
    const unsigned char *bitmap = shown;
    if (pending) {
        const unsigned long elapsed = now - transitionStartedMs;
        if (elapsed < 100UL) bitmap = shown;
        else if (elapsed < 200UL) bitmap = Irisoled::blink_down;
        else if (elapsed < 300UL) bitmap = Irisoled::blink;
        else if (elapsed < 400UL) bitmap = Irisoled::blink_up;
        else {
            shown = pending;
            pending = nullptr;
            bitmap = shown;
        }
    } else if (gFaceAnimation == "PULSE") {
        const unsigned long phase = now % 1200UL;
        if (phase < 140UL) bitmap = Irisoled::wink_left;
        else if (phase >= 600UL && phase < 740UL) bitmap = Irisoled::wink_right;
    } else if (gFaceAnimation == "ALERT") {
        if (now % 700UL < 120UL) bitmap = Irisoled::surprised;
    } else if (gFaceAnimation == "TALKING") {
        if (now % 360UL < 100UL) bitmap = Irisoled::blink_up;
    } else {
        // One calm natural blink every five seconds.
        const unsigned long blinkPhase = now % 5000UL;
        if (blinkPhase < 100UL) bitmap = Irisoled::blink_down;
        else if (blinkPhase < 200UL) bitmap = Irisoled::blink;
        else if (blinkPhase < 300UL) bitmap = Irisoled::blink_up;
    }
    gDisplay.drawBitmap(0, 0, bitmap, 128, 64, SSD1306_WHITE);
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

void drawToolboxActivity()
{
    gDisplay.setTextColor(SSD1306_WHITE);
    gDisplay.setTextSize(1);
    gDisplay.fillCircle(4, 4, millis() % 700UL < 350UL ? 2 : 1,
                        SSD1306_WHITE);
    gDisplay.setCursor(10, 0);
    gDisplay.print(oledCompactLine(gOledAiTitle, 20));
    gDisplay.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    gDisplay.setCursor(0, 14);
    gDisplay.setTextSize(2);
    gDisplay.print(oledCompactLine(gOledToolPrimary, 10));

    gDisplay.setCursor(0, 27);
    gDisplay.setTextSize(1);
    gDisplay.print(oledCompactLine(gOledToolSecondary, 20));

    gDisplay.setCursor(0, 40);
    gDisplay.print(oledCompactLine(gOledToolDetail, 20));

    gDisplay.setCursor(0, 53);
    gDisplay.print(oledCompactLine(gOledLiveDetail, 20));
}

void drawAiOledCanvas()
{
    gDisplay.setTextColor(SSD1306_WHITE);
    for (uint8_t index = 0; index < gOledPrimitiveCount; ++index) {
        const OledPrimitive &item = gOledPrimitives[index];
        if (item.kind == OledPrimitiveKind::Text) {
            gDisplay.setTextSize(item.size);
            gDisplay.setCursor(item.x, item.y);
            gDisplay.print(item.text);
        } else if (item.kind == OledPrimitiveKind::Line) {
            gDisplay.drawLine(item.x, item.y, item.x2, item.y2, SSD1306_WHITE);
        } else if (item.kind == OledPrimitiveKind::Rect) {
            if (item.fill)
                gDisplay.fillRect(item.x, item.y, item.width, item.height, SSD1306_WHITE);
            else
                gDisplay.drawRect(item.x, item.y, item.width, item.height, SSD1306_WHITE);
        } else if (item.kind == OledPrimitiveKind::Circle) {
            if (item.fill)
                gDisplay.fillCircle(item.x, item.y, item.radius, SSD1306_WHITE);
            else
                gDisplay.drawCircle(item.x, item.y, item.radius, SSD1306_WHITE);
        }
    }
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
    if (gPhonePairingVisible) {
        if (static_cast<int32_t>(gPhonePairingUntilMs - millis()) <= 0) {
            gPhonePairingVisible = false;
            gPhonePairingUrl = "";
        } else {
        showPhonePairingQr();
        return;
        }
    }
    gDisplay.clearDisplay();
    const bool safetyPresentationActive =
        gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)].active;
    if (!safetyPresentationActive && gOledPrimitiveCount > 0 &&
        static_cast<int32_t>(gOledCanvasUntilMs - millis()) > 0) {
        drawAiOledCanvas();
        gDisplay.display();
        return;
    }
    if (gOledPrimitiveCount > 0 &&
        static_cast<int32_t>(gOledCanvasUntilMs - millis()) <= 0) {
        gOledPrimitiveCount = 0;
        gOledCanvasUntilMs = 0;
    }
    if (!gOledLiveDetail.isEmpty() &&
        static_cast<int32_t>(gOledConsoleUntilMs - millis()) > 0) {
        drawToolboxActivity();
        gDisplay.display();
        return;
    }
    // The OLED is Alpha's face. Information appears only while an AI toolbox
    // is actively producing live data.
    gDisplayMode = DisplayMode::Face;
    // Conversation state temporarily owns the face. Otherwise preserve the
    // presentation selected by Interaction, Control, Safety, Identity or Idle;
    // overwriting it here used to erase touch/movement faces every 100 ms.
    const bool voiceEventActive =
        static_cast<int32_t>(gVoiceEventUntilMs - millis()) > 0;
    if (safetyPresentationActive) {
        // Safety owns the face absolutely, including during phone chat and
        // streamed AI speech. applyPresentation already selected its face and
        // animation; conversation visuals must not overwrite either value.
    } else if (voiceEventActive) {
        gFaceEmotion = gVoiceEventEmotion;
        gFaceAnimation = gVoiceEventAnimation;
    } else {
        if (gVoiceEventUntilMs) gVoiceEventUntilMs = 0;
        if (gConversationSpeakingVisual) gFaceAnimation = "TALKING";
        else if (gConversationThinkingVisual) gFaceAnimation = "PROCESSING";
        else if (gConversationListeningVisual) gFaceAnimation = "LISTENING";
        if (gConversationThinkingVisual && gOledReplyMood.isEmpty())
            gFaceEmotion = gThinkingEmotion;
        else if (gConversationListeningVisual && gOledReplyMood.isEmpty())
            gFaceEmotion = gListeningEmotion;
    }
    drawFace();
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

void showPhonePairingQr()
{
    if (!gOledReady || gPhonePairingUrl.isEmpty()) return;
    esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
    config.display_func = drawLargeWifiQr;
    config.max_qrcode_version = 10;
    config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
    esp_qrcode_generate(&config, gPhonePairingUrl.c_str());
}

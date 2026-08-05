uint32_t ringColorForMode()
{
    switch (gRingMode) {
    case RingMode::Auto:
        return gRing.Color(gAutoRed, gAutoGreen, gAutoBlue);
    case RingMode::White: return gRing.Color(255, 255, 255);
    case RingMode::Red: return gRing.Color(255, 0, 0);
    case RingMode::Green: return gRing.Color(0, 255, 0);
    case RingMode::Blue: return gRing.Color(0, 0, 255);
    default: return 0;
    }
}

const char *ringModeName()
{
    switch (gRingMode) {
    case RingMode::Auto: return "AUTO";
    case RingMode::White: return "WHITE";
    case RingMode::Red: return "RED";
    case RingMode::Green: return "GREEN";
    case RingMode::Blue: return "BLUE";
    case RingMode::Rainbow: return "RAINBOW";
    default: return "OFF";
    }
}

void refreshRing()
{
    if (gVoiceRingOverlayActive) {
        gRing.setBrightness(gVoiceRingBrightness);
        const uint32_t color = gRing.Color(
            gVoiceRingRed, gVoiceRingGreen, gVoiceRingBlue);
        for (uint8_t pixel = 0; pixel < kRingPixelCount; ++pixel) {
            gRing.setPixelColor(pixel, color);
        }
        gRing.show();
        return;
    }
    gRing.setBrightness(gRingMode == RingMode::Off ? 0 : gAutoRingBrightness);
    if (gRingMode == RingMode::Rainbow) return;
    const uint32_t color = ringColorForMode();
    for (uint8_t pixel = 0; pixel < kRingPixelCount; ++pixel) {
        gRing.setPixelColor(pixel, color);
    }
    gRing.show();
}

void setVoiceRingOverlay(bool enabled, uint8_t red, uint8_t green, uint8_t blue,
                         uint8_t brightness, String animation, uint16_t speedMs)
{
    gVoiceRingOverlayActive = enabled && brightness > 0;
    gVoiceRingRed = red; gVoiceRingGreen = green; gVoiceRingBlue = blue;
    gVoiceRingBrightness = constrain(brightness, 0, 180);
    animation.toLowerCase();
    gVoiceRingAnimation = animation;
    gVoiceRingSpeedMs = constrain(speedMs, 120, 4000);
    refreshRing();
}

void clearVoiceRingOverlay()
{
    if (!gVoiceRingOverlayActive) return;
    gVoiceRingOverlayActive = false;
    refreshRing();
}

void setRingMode(const String &requestedMode, int brightness)
{
    String mode = requestedMode;
    mode.toUpperCase();
    if (mode == "AUTO") gRingMode = RingMode::Auto;
    else if (mode == "WHITE") gRingMode = RingMode::White;
    else if (mode == "RED") gRingMode = RingMode::Red;
    else if (mode == "GREEN") gRingMode = RingMode::Green;
    else if (mode == "BLUE") gRingMode = RingMode::Blue;
    else if (mode == "RAINBOW") gRingMode = RingMode::Rainbow;
    else if (mode != "BRIGHTNESS") gRingMode = RingMode::Off;
    if (gRingMode != RingMode::Auto) {
        gRing.setBrightness(constrain(brightness, 0, 180));
    }
    refreshRing();
    Serial.printf("[Z2 RING] mode=%s brightness=%d\n", mode.c_str(),
                  constrain(brightness, 0, 180));
}

const char *ambientConditionName()
{
    switch (gAmbientCondition) {
    case AmbientCondition::VeryDark: return "VERY DARK";
    case AmbientCondition::Dark: return "DARK";
    case AmbientCondition::Dim: return "DIM";
    case AmbientCondition::Comfortable: return "COMFORTABLE";
    case AmbientCondition::Bright: return "BRIGHT";
    case AmbientCondition::VeryBright: return "VERY BRIGHT";
    default: return "UNAVAILABLE";
    }
}

const char *autoLightOutputName()
{
    switch (gAmbientCondition) {
    case AmbientCondition::VeryDark: return "DEEP AMBER";
    case AmbientCondition::Dark: return "AMBER";
    case AmbientCondition::Dim: return "WARM AMBER";
    case AmbientCondition::Comfortable: return "WARM WHITE";
    case AmbientCondition::Bright: return "SOFT WHITE";
    case AmbientCondition::VeryBright: return "OFF";
    default: return "OFF";
    }
}

void updateAmbientLight()
{
    if (!gLightSensorReady || millis() - gLastLightReadMs < kLightReadIntervalMs) return;
    gLastLightReadMs = millis();
    const float measuredLux = max(0.0f, gLightSensor.readLux());
    gAmbientLux = measuredLux;
    const float alpha = 0.12f + 0.07f * gLightSensitivity;
    gSmoothedLux = gSmoothedLux <= 0.0f
        ? measuredLux
        : gSmoothedLux * (1.0f - alpha) + measuredLux * alpha;

    // Conditions scale with the user-selected off point, so the complete
    // color range remains useful in different rooms and sensor positions.
    const float ratio = gAutoOffLux ? gSmoothedLux / gAutoOffLux : 1.0f;
    AmbientCondition condition;
    if (ratio < 0.02f) condition = AmbientCondition::VeryDark;
    else if (ratio < 0.07f) condition = AmbientCondition::Dark;
    else if (ratio < 0.18f) condition = AmbientCondition::Dim;
    else if (ratio < 0.40f) condition = AmbientCondition::Comfortable;
    else if (ratio < 0.75f) condition = AmbientCondition::Bright;
    else condition = AmbientCondition::VeryBright;

    // NeoPixels look bright even at modest numerical values. This steeper
    // perceptual curve keeps the ring useful in darkness but makes it subtle
    // in a normally lit room without introducing a hard cutoff.
    const uint8_t targetBrightness = ratio >= 1.0f
        ? 0
        : constrain(static_cast<int>(180.0f * expf(-10.0f * ratio)), 1, 180);
    const int brightnessDifference =
        static_cast<int>(targetBrightness) - gAutoRingBrightness;
    const int maximumBrightnessStep = 4 + gLightSensitivity * 2;
    const uint8_t brightness = static_cast<uint8_t>(
        gAutoRingBrightness + constrain(
            brightnessDifference, -maximumBrightnessStep, maximumBrightnessStep));

    // Interpolate color temperature continuously instead of jumping between
    // condition colors.
    struct ColorStop { float at; uint8_t red; uint8_t green; uint8_t blue; };
    constexpr ColorStop stops[] = {
        {0.00f, 255, 45, 0}, {0.15f, 255, 105, 8},
        {0.35f, 255, 170, 70}, {0.65f, 255, 230, 190},
        {1.00f, 210, 230, 255}
    };
    const float colorRatio = constrain(ratio, 0.0f, 1.0f);
    const ColorStop *low = &stops[0];
    const ColorStop *high = &stops[1];
    for (size_t index = 1; index < sizeof(stops) / sizeof(stops[0]); ++index) {
        if (colorRatio <= stops[index].at) {
            low = &stops[index - 1];
            high = &stops[index];
            break;
        }
    }
    const float blend = (colorRatio - low->at) / (high->at - low->at);
    const uint8_t targetRed = low->red + (high->red - low->red) * blend;
    const uint8_t targetGreen = low->green + (high->green - low->green) * blend;
    const uint8_t targetBlue = low->blue + (high->blue - low->blue) * blend;
    gAutoRed = (gAutoRed * 3U + targetRed) / 4U;
    gAutoGreen = (gAutoGreen * 3U + targetGreen) / 4U;
    gAutoBlue = (gAutoBlue * 3U + targetBlue) / 4U;

    const bool stateChanged =
        condition != gAmbientCondition || brightness != gAutoRingBrightness;
    if (stateChanged) {
        gAmbientCondition = condition;
        gAutoRingBrightness = brightness;
        Serial.printf("[Z2 LIGHT] %.1f lux condition=%s auto=%s brightness=%u\n",
                      gAmbientLux, ambientConditionName(), autoLightOutputName(),
                      gAutoRingBrightness);
    }
    if (gVoiceRingOverlayActive || gRingMode != RingMode::Off) refreshRing();
}

void updateRingAnimation()
{
    if (gVoiceRingOverlayActive && gVoiceRingAnimation != "solid") {
        if (millis() - gLastRingAnimationMs < 30) return;
        gLastRingAnimationMs = millis();
        const float phase = static_cast<float>(millis() % gVoiceRingSpeedMs) /
                            static_cast<float>(gVoiceRingSpeedMs);
        float level = 1.0f;
        if (gVoiceRingAnimation == "breathe")
            level = 0.18f + 0.82f * (0.5f - 0.5f * cosf(phase * 2.0f * PI));
        else if (gVoiceRingAnimation == "pulse")
            level = phase < 0.18f ? 1.0f - phase / 0.22f : 0.12f;
        gRing.setBrightness(static_cast<uint8_t>(gVoiceRingBrightness * level));
        for (uint8_t pixel = 0; pixel < kRingPixelCount; ++pixel) {
            bool lit = gVoiceRingAnimation != "sparkle" ||
                       ((pixel * 37U + millis() / 90U) % 7U < 2U);
            gRing.setPixelColor(pixel, lit ? gRing.Color(
                gVoiceRingRed, gVoiceRingGreen, gVoiceRingBlue) : 0);
        }
        gRing.show();
        return;
    }
    if (gVoiceRingOverlayActive || gRingMode != RingMode::Rainbow ||
        millis() - gLastRingAnimationMs < 30) return;
    gLastRingAnimationMs = millis();
    for (uint8_t pixel = 0; pixel < kRingPixelCount; ++pixel) {
        const uint16_t hue = gRingRainbowOffset +
            static_cast<uint16_t>((65536UL * pixel) / kRingPixelCount);
        gRing.setPixelColor(pixel, gRing.gamma32(gRing.ColorHSV(hue)));
    }
    gRing.show();
    gRingRainbowOffset += 256;
}

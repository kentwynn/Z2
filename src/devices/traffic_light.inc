void updateTrafficLight()
{
    const unsigned long now = millis();
    const unsigned long startupAge = now - gTrafficLightStartedMs;
    if (startupAge < 1800) {
        setTrafficLight(startupAge < 600, startupAge >= 600 && startupAge < 1200,
                        startupAge >= 1200);
        return;
    }
    if (gCliffHazardActive || gCliffRecoveryActive) {
        setTrafficLight(false, (now / 140) % 2, false);
    } else if (gLocalRecoveryActive) {
        // Steady yellow means Z2 is actively resolving the hazard. Blinking
        // yellow below means it is stopped and still needs attention.
        setTrafficLight(false, true, false);
    } else if (gSafetyStopActive) {
        setTrafficLight(false, (now / 180) % 2, false);
    } else if (touchReactionActive()) {
        setTrafficLight(false, (now / 180) % 2, false);
    } else if (gTrafficLightOverride != "AUTO" &&
               static_cast<long>(gTrafficLightOverrideUntilMs - now) > 0) {
        setTrafficLight(gTrafficLightOverride == "RED",
                        gTrafficLightOverride == "YELLOW",
                        gTrafficLightOverride == "GREEN");
    } else if (!gMoving) {
        if (gTrafficLightOverride != "AUTO") gTrafficLightOverride = "AUTO";
        setTrafficLight(true, false, false);
    } else if (directionalWarningForCommand(gCommand)) {
        setTrafficLight(false, (now / 150) % 2, false);
    } else if (gCommand == 'F') {
        setTrafficLight(false, false, true);
    } else {
        setTrafficLight(false, (now / 400) % 2, false);
    }
}

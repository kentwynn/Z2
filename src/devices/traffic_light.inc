void updateTrafficLight()
{
    const unsigned long now = millis();
    const unsigned long startupAge = now - gTrafficLightStartedMs;
    if (startupAge < 1800) {
        setTrafficLight(startupAge < 600, startupAge >= 600 && startupAge < 1200,
                        startupAge >= 1200);
        return;
    }
    if (gCliffRecoveryActive || gLocalRecoveryActive) {
        // Yellow is exclusively an active-recovery state: the motors are
        // currently trying to move Z2 away from a detected boundary.
        setTrafficLight(false, true, false);
    } else if (gCliffHazardActive || gSafetyStopActive) {
        // If recovery has stopped while a hazard remains, show an unambiguous
        // red stop. Never leave yellow active when the motors are stopped.
        setTrafficLight(true, false, false);
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

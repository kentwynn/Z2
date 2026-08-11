bool directionalWarningForCommand(char command)
{
    if (command != 'B' && gUltrasonicValid &&
        gLatestDistanceCm <= kObstacleWarningDistanceCm) return true;
    if ((command == 'F' || command == 'L' || command == 'X') &&
        gTofValid[0] && gTofDistanceMm[0] <= kTofWarningDistanceMm) return true;
    if ((command == 'F' || command == 'R' || command == 'Y') &&
        gTofValid[1] && gTofDistanceMm[1] <= kTofWarningDistanceMm) return true;
    if (command == 'B' && gTofValid[2] &&
        gTofDistanceMm[2] <= kTofWarningDistanceMm) return true;
    return false;
}

bool immediateObstacleHazardActive()
{
    return (gUltrasonicValid &&
            gLatestDistanceCm <= kEmergencyStopDistanceCm) ||
        (gTofValid[0] && gTofDistanceMm[0] <= kTofStopDistanceMm) ||
        (gTofValid[1] && gTofDistanceMm[1] <= kTofStopDistanceMm) ||
        (gTofValid[2] && gTofDistanceMm[2] <= kTofStopDistanceMm);
}

void updateProximitySafety()
{
    // Cliff danger has higher authority than proximity recovery.
    if (gCliffHazardActive || gCliffRecoveryActive) {
        gProximityHazardActive = false;
        return;
    }
    if (gMoving) return;
    const bool frontHazard =
        (gUltrasonicValid && gLatestDistanceCm <= kEmergencyStopDistanceCm) ||
        (gTofValid[0] && gTofDistanceMm[0] <= kTofStopDistanceMm) ||
        (gTofValid[1] && gTofDistanceMm[1] <= kTofStopDistanceMm);
    const bool rearHazard = gTofValid[2] &&
        gTofDistanceMm[2] <= kTofStopDistanceMm;
    const bool proximityHazard = frontHazard || rearHazard;
    const bool proximityClear =
        (!gUltrasonicValid || gLatestDistanceCm > kEmergencyResetDistanceCm) &&
        (!gTofValid[0] || gTofDistanceMm[0] > kTofResetDistanceMm) &&
        (!gTofValid[1] || gTofDistanceMm[1] > kTofResetDistanceMm) &&
        (!gTofValid[2] || gTofDistanceMm[2] > kTofResetDistanceMm);

    if (proximityHazard && !gProximityHazardActive) {
        gProximityHazardActive = true;
        gSafetyStopActive = true;
        requestPresentation(PresentationOwner::Safety,
                            gPresetSafetyObstacle, "obstacle", false);
        if (gAutomaticSafetyRecovery) {
            const char approachedFrom = rearHazard && !frontHazard ? 'B' : 'F';
            tryStartLocalSafetyRecovery(
                approachedFrom,
                frontHazard && rearHazard
                    ? "front and rear distance boundary"
                    : frontHazard ? "front distance boundary"
                                  : "rear distance boundary");
        }
    } else if (gProximityHazardActive && proximityClear &&
               !gLocalRecoveryActive) {
        gProximityHazardActive = false;
        gLocalRecoveryAttempted = false;
        gLastRecoveryBlockedNoticeMs = 0;
        gSafetyStopActive = false;
        auto &safetyPresentation =
            gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
        if (safetyPresentation.active && safetyPresentation.event == "obstacle")
            clearPresentation(PresentationOwner::Safety);
    }

    // Reconcile a previously blocked recovery once the source-of-truth reset
    // boundary is clear. Never leave a stopped robot showing an active hazard.
    if (gSafetyStopActive && !gCliffHazardActive &&
        !gCliffRecoveryActive && !gLocalRecoveryActive &&
        !gProximityHazardActive &&
        !immediateObstacleHazardActive()) {
        gSafetyStopActive = false;
        gLocalRecoveryAttempted = false;
        gLastRecoveryBlockedNoticeMs = 0;
        auto &safetyPresentation =
            gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
        if (safetyPresentation.active && safetyPresentation.event == "obstacle")
            clearPresentation(PresentationOwner::Safety);
    }
}

void updateCliffSensors()
{
    if (!kCliffSensorsEnabled) return;
    const unsigned long now = millis();
    if (now - gLastCliffReadMs < kCliffReadIntervalMs) return;
    gLastCliffReadMs = now;
    const uint8_t pins[2] = {kCliffLeftPin, kCliffRightPin};
    for (uint8_t index = 0; index < 2; ++index) {
        gCliffRawHigh[index] = digitalRead(pins[index]) == HIGH;
        const bool detected = gCliffRawHigh[index] ==
            (kCliffDetectedLevel == HIGH);
        if (detected && gCliffDebounce[index] < kCliffDebounceSamples) {
            ++gCliffDebounce[index];
        } else if (!detected && gCliffDebounce[index] > 0) {
            --gCliffDebounce[index];
        }
        gCliffDetected[index] =
            gCliffDebounce[index] >= kCliffDebounceSamples;
    }

    const bool anyCliff = gCliffDetected[0] || gCliffDetected[1];
    constexpr unsigned long kCliffWarningRepeatMs = 1200;
    if (anyCliff) {
        if (!gCliffHazardActive) {
            gCliffHazardActive = true;
            gLastCliffWarningMs = now;
            requestPresentation(PresentationOwner::Safety,
                                gPresetSafetyCliff, "cliff",
                                !(gAutoDriveEnabled && gAutoDriveSilent));
            Serial.printf("[Z2 CLIFF] hazard active left=%d right=%d\n",
                          gCliffDetected[0], gCliffDetected[1]);
        } else if (now - gLastCliffWarningMs >= kCliffWarningRepeatMs) {
            gLastCliffWarningMs = now;
            if (!(gAutoDriveEnabled && gAutoDriveSilent) &&
                !gCliffRecoveryActive && gCapabilitySpeaker &&
                gPresetSafetyCliff.sound != "OFF")
                playSound(gPresetSafetyCliff.sound);
        }
    } else if (gCliffHazardActive) {
        gCliffHazardActive = false;
        gLastCliffWarningMs = 0;
        Serial.println("[Z2 CLIFF] hazard clear");
        if (!gCliffRecoveryActive) {
            auto &safetyPresentation =
                gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
            if (safetyPresentation.active && safetyPresentation.event == "cliff")
                clearPresentation(PresentationOwner::Safety);
        }
    }
    if (!anyCliff) {
        if (gCliffClearStartedMs == 0) gCliffClearStartedMs = now;
        if (!gCliffRecoveryActive && now - gCliffClearStartedMs >= 300) {
            gCliffArmed = true;
        }
    } else {
        gCliffClearStartedMs = 0;
    }

    if (gCliffRecoveryActive) {
        const bool rearBlocked = gTofValid[2] &&
            gTofStopDebounce[2] >= 2;
        // Cliff recovery is feedback-controlled, not time-controlled. Keep
        // reversing until both cliff sensors see supported floor. A valid rear
        // hard boundary is the only condition allowed to stop recovery while
        // a cliff remains active.
        if (!anyCliff || rearBlocked) {
            stopMotors();
            gCliffRecoveryActive = false;
            const bool recovered = !anyCliff;
            gSafetyStopActive = !recovered;
            if (recovered) {
                auto &safetyPresentation =
                    gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
                if (safetyPresentation.active && safetyPresentation.event == "cliff")
                    clearPresentation(PresentationOwner::Safety);
                addRealtimeLog("Cliff recovery completed · supported floor restored");
                sendDeviceEvent("Safety", "Recovery completed",
                                "cliff sensors returned to supported floor");
            } else if (rearBlocked) {
                notifyRecoveryBlocked(
                    "cliff active · rear became blocked");
            }
            Serial.printf("[Z2 CLIFF] recovery stopped clear=%d rear=%s\n",
                          recovered, rearBlocked ? "BLOCKED" : "clear");
        } else {
            // Keep the motor watchdog alive only while every feedback check
            // still permits the same recovery direction.
            gLastDriveCommandMs = now;
        }
        return;
    }

    // A robot placed down at an edge must recover even when it was stationary;
    // cliff protection is local and cannot depend on a prior drive command.
    if (!gCliffArmed || !anyCliff || gCommand == 'B') return;

    stopMotors();
    gCliffArmed = false;
    const bool rearClear = !gTofValid[2] ||
        gTofDistanceMm[2] > kTofResetDistanceMm;
    if (!rearClear) {
        gSafetyStopActive = true;
        Serial.println("[Z2 CLIFF] edge detected; rear blocked, staying stopped");
        notifyRecoveryBlocked("cliff active · rear path blocked");
        return;
    }
    if (!gAutomaticSafetyRecovery) {
        gSafetyStopActive = true;
        addRealtimeLog("Cliff stopped locally · automatic recovery disabled");
        sendDeviceEvent("Safety", "Recovery not attempted",
                        "cliff stop · automatic recovery disabled");
        return;
    }

    gSafetyStopActive = false;
    gCliffRecoveryActive = true;
    gCliffRecoveryStartedMs = now;
    driveMotor(-kCliffRecoverySpeed, kAin1Pin, kAin2Pin, kRightPwmChannel);
    driveMotor(-kCliffRecoverySpeed, kBin1Pin, kBin2Pin, kLeftPwmChannel);
    gCommand = 'B';
    gMoving = true;
    gLastDriveCommandMs = now;
    setTrafficLight(false, true, false);
    addRealtimeLog(String("Cliff recovery: B · left=") +
                   gCliffDetected[0] + " right=" + gCliffDetected[1] +
                   " rear=" + gTofDistanceMm[2] + " mm");
    sendDeviceEvent("Safety", "Recovery started",
                    String("cliff · direction B · rear ") +
                    gTofDistanceMm[2] + " mm");
    Serial.printf("[Z2 CLIFF] edge left=%d right=%d; reversing\n",
                  gCliffDetected[0], gCliffDetected[1]);
}

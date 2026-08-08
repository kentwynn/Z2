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

bool touchReactionActive()
{
    return !gMoving && gLastTouchReactionMs != 0 &&
        millis() - gLastTouchReactionMs <= kTouchReactionHoldMs;
}

bool immediateObstacleHazardActive()
{
    return (gUltrasonicValid &&
            gLatestDistanceCm <= kEmergencyStopDistanceCm) ||
        (gTofValid[0] && gTofDistanceMm[0] <= kTofStopDistanceMm) ||
        (gTofValid[1] && gTofDistanceMm[1] <= kTofStopDistanceMm) ||
        (gTofValid[2] && gTofDistanceMm[2] <= kTofStopDistanceMm);
}

void updateTouchReaction()
{
    // Cliff danger is exclusive. Do not queue a lower-priority contact
    // reaction that could appear or sound immediately after the cliff clears.
    if (gCliffHazardActive || gCliffRecoveryActive) {
        gTouchContactActive = false;
        gLastTouchReactionMs = 0;
        auto &interactionPresentation =
            gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Interaction)];
        if (interactionPresentation.active && interactionPresentation.event == "near_presence")
            clearPresentation(PresentationOwner::Interaction);
        return;
    }
    if (gMoving) return;
    const bool frontClose =
        (gUltrasonicValid && gLatestDistanceCm <= kTouchReactionDistanceCm) ||
        (gTofValid[0] && gTofDistanceMm[0] <= kTouchReactionDistanceMm) ||
        (gTofValid[1] && gTofDistanceMm[1] <= kTouchReactionDistanceMm);
    const bool rearClose = gTofValid[2] &&
        gTofDistanceMm[2] <= kTouchReactionDistanceMm;
    const bool veryClose = frontClose || rearClose;
    if (veryClose) {
        const unsigned long now = millis();
        const bool firstContact = !gTouchContactActive;
        gLastTouchReactionMs = now;
        gTouchContactActive = true;
        auto &interactionPresentation =
            gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Interaction)];
        if (firstContact &&
            (!interactionPresentation.active || interactionPresentation.event == "near_presence"))
            requestPresentation(PresentationOwner::Interaction,
                                gPresetInteractionTouch, "near_presence");
        if (firstContact && gAutomaticSafetyRecovery) {
            gSafetyStopActive = true;
            requestPresentation(PresentationOwner::Safety,
                                gPresetSafetyObstacle, "obstacle", false);
            // An interrupted forward command chooses reverse; an interrupted
            // reverse command chooses forward. When both sides are close,
            // prefer neither blindly: the candidate validation will reject it.
            const char approachedFrom = rearClose && !frontClose ? 'B' : 'F';
            tryStartLocalSafetyRecovery(approachedFrom,
                                        frontClose && rearClose
                                            ? "front and rear proximity"
                                            : frontClose ? "front proximity"
                                                         : "rear proximity");
        }
    } else if (gTouchContactActive &&
               ((!gLocalRecoveryActive && gLocalRecoveryAttempted) ||
                millis() - gLastTouchReactionMs > kTouchReactionHoldMs)) {
        gTouchContactActive = false;
        auto &interactionPresentation =
            gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Interaction)];
        if (interactionPresentation.active && interactionPresentation.event == "near_presence")
            clearPresentation(PresentationOwner::Interaction);
        // A new deliberate proximity incident must be evaluated again even
        // when an unrelated direction remains blocked. The candidate planner
        // will still refuse every uncertified escape path.
        if (!gLocalRecoveryActive) {
            gLocalRecoveryAttempted = false;
            gLastRecoveryBlockedNoticeMs = 0;
        }
        const bool immediateHazard = immediateObstacleHazardActive();
        if (!gCliffHazardActive && !immediateHazard) {
            gSafetyStopActive = false;
            auto &safetyPresentation =
                gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
            if (safetyPresentation.active && safetyPresentation.event == "obstacle")
                clearPresentation(PresentationOwner::Safety);
        }
    }

    // Reconcile a previously blocked recovery continuously. The contact edge
    // may already have been consumed while another sensor was still inside
    // its hard boundary; once that boundary clears, do not leave yellow and
    // the motor lock latched indefinitely.
    if (gSafetyStopActive && !gCliffHazardActive &&
        !gCliffRecoveryActive && !gLocalRecoveryActive &&
        !veryClose && !immediateObstacleHazardActive()) {
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
            gTouchContactActive = false;
            gLastTouchReactionMs = 0;
            auto &interactionPresentation =
                gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Interaction)];
            if (interactionPresentation.active && interactionPresentation.event == "near_presence")
                clearPresentation(PresentationOwner::Interaction);
            requestPresentation(PresentationOwner::Safety,
                                gPresetSafetyCliff, "cliff");
            Serial.printf("[Z2 CLIFF] hazard active left=%d right=%d\n",
                          gCliffDetected[0], gCliffDetected[1]);
        } else if (now - gLastCliffWarningMs >= kCliffWarningRepeatMs) {
            gLastCliffWarningMs = now;
            if (!gCliffRecoveryActive && gCapabilitySpeaker &&
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
        const bool rearBlocked = !gTofValid[2] ||
            gTofDistanceMm[2] <= kTofStopDistanceMm;
        const bool timedOut =
            now - gCliffRecoveryStartedMs >= kCliffRecoveryMaximumMs;
        if (!anyCliff || rearBlocked || timedOut) {
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
                    "cliff active · rear became unavailable or blocked");
            } else {
                notifyRecoveryBlocked(
                    "cliff did not clear within bounded recovery");
            }
            Serial.printf("[Z2 CLIFF] recovery stopped clear=%d rear=%s timeout=%d\n",
                          recovered, rearBlocked ? "BLOCKED" : "clear", timedOut);
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
    const bool rearClear = gTofValid[2] &&
        gTofDistanceMm[2] > kTofResetDistanceMm;
    if (!rearClear) {
        gSafetyStopActive = true;
        Serial.println("[Z2 CLIFF] edge detected; rear unavailable or blocked, staying stopped");
        notifyRecoveryBlocked("cliff active · rear path not verified clear");
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

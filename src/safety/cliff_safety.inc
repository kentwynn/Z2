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
    bool veryClose = gUltrasonicValid &&
        gLatestDistanceCm <= kTouchReactionDistanceCm;
    for (uint8_t index = 0; index < kTofCount && !veryClose; ++index) {
        veryClose = gTofValid[index] &&
            gTofDistanceMm[index] <= kTouchReactionDistanceMm;
    }
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
    } else if (gTouchContactActive &&
               millis() - gLastTouchReactionMs > kTouchReactionHoldMs) {
        gTouchContactActive = false;
        auto &interactionPresentation =
            gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Interaction)];
        if (interactionPresentation.active && interactionPresentation.event == "near_presence")
            clearPresentation(PresentationOwner::Interaction);
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
            if (gCapabilitySpeaker && gPresetSafetyCliff.sound != "OFF")
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
            gTofDistanceMm[2] <= kTofStopDistanceMm;
        if (rearBlocked || now - gCliffRecoveryStartedMs >= kCliffRecoveryMs) {
            stopMotors();
            gCliffRecoveryActive = false;
            gSafetyStopActive = rearBlocked;
            if (!gCliffHazardActive && !rearBlocked) {
                auto &safetyPresentation =
                    gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
                if (safetyPresentation.active && safetyPresentation.event == "cliff")
                    clearPresentation(PresentationOwner::Safety);
            }
            Serial.printf("[Z2 CLIFF] recovery complete rear=%s\n",
                          rearBlocked ? "BLOCKED" : "clear");
        }
        return;
    }

    if (!gCliffArmed || !anyCliff || !gMoving || gCommand == 'B') return;

    stopMotors();
    gCliffArmed = false;
    const bool rearBlocked = gTofValid[2] &&
        gTofDistanceMm[2] <= kTofStopDistanceMm;
    if (rearBlocked) {
        gSafetyStopActive = true;
        Serial.println("[Z2 CLIFF] edge detected; rear blocked, staying stopped");
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
    Serial.printf("[Z2 CLIFF] edge left=%d right=%d; reversing\n",
                  gCliffDetected[0], gCliffDetected[1]);
}

bool applyDrive(char command, int speed)
{
    if (!gVirtualPowerOn && command != 'S') {
        stopMotors();
        Serial.printf("[Z2 POWER] command=%c blocked while virtual power is off\n", command);
        return false;
    }
    if (gCliffRecoveryActive || gLocalRecoveryActive) {
        Serial.printf("[Z2 SAFETY] command=%c ignored during recovery\n", command);
        return false;
    }
    const bool cliffBlocked = (gCliffDetected[0] || gCliffDetected[1]) &&
        command != 'B' && command != 'S';
    if (cliffBlocked) {
        requestPresentation(PresentationOwner::Safety, gPresetSafetyCliff, "cliff", false);
        stopMotors();
        gSafetyStopActive = true;
        Serial.printf("[Z2 SAFETY] command=%c blocked by cliff left=%d right=%d\n",
                      command, gCliffDetected[0], gCliffDetected[1]);
        if (!gAutoDriveEnabled && !gCollisionWarningLatched) playCollisionWarning();
        else if (gAutoDriveEnabled) gCollisionWarningLatched = true;
        return false;
    }
    const bool ultrasonicBlocked =
        command != 'B' && command != 'S' && gUltrasonicValid &&
        gLatestDistanceCm <= kEmergencyStopDistanceCm;
    uint8_t blockedTof = 0;
    const bool tofBlocked = tofBlocksCommand(command, blockedTof);
    if (ultrasonicBlocked || tofBlocked) {
        requestPresentation(PresentationOwner::Safety, gPresetSafetyObstacle, "obstacle", false);
        stopMotors();
        gSafetyStopActive = true;
        if (tofBlocked) {
            const char *names[] = {"front-left", "front-right", "back"};
            Serial.printf("[Z2 SAFETY] command=%c blocked by %s at %u mm\n",
                          command, names[blockedTof], gTofDistanceMm[blockedTof]);
        } else {
            Serial.printf("[Z2 SAFETY] command=%c blocked by sonar at %.1f cm\n",
                          command, gLatestDistanceCm);
        }
        if (!gAutoDriveEnabled) playCollisionWarning();
        else gCollisionWarningLatched = true;
        return false;
    }

    // A previous block in another direction must not keep the traffic light
    // red when this newly requested direction is clear and accepted.
    gSafetyStopActive = false;
    if (!gCliffHazardActive)
        clearPresentation(PresentationOwner::Safety);

    const int full = constrain(speed, 0, 255);
    int left = 0;
    int right = 0;

    switch (command) {
    case 'F': left = full; right = full; break;
    case 'B': left = -full; right = -full; break;
    case 'L': left = -full; right = full; break;
    case 'R': left = full; right = -full; break;
    case 'X': left = -full; right = full; break;
    case 'Y': left = full; right = -full; break;
    default:
        stopMotors();
        clearPresentation(PresentationOwner::Control);
        Serial.println("[Z2] STOP");
        return true;
    }

    driveMotor(right, kAin1Pin, kAin2Pin, kRightPwmChannel);
    driveMotor(left, kBin1Pin, kBin2Pin, kLeftPwmChannel);
    gSpeed = full;
    gCommand = command;
    gMoving = true;
    gLastDriveCommandMs = millis();
    if (command == 'F') requestPresentation(PresentationOwner::Control, gPresetControlForward, "forward");
    else if (command == 'B') requestPresentation(PresentationOwner::Control, gPresetControlReverse, "reverse");
    else if (command == 'L' || command == 'X')
        requestPresentation(PresentationOwner::Control, gPresetControlLeft, "left");
    else if (command == 'R' || command == 'Y')
        requestPresentation(PresentationOwner::Control, gPresetControlRight, "right");
    else requestPresentation(PresentationOwner::Control, gPresetControlTurn, "turn");
    Serial.printf("[Z2] command=%c speed=%d left=%d right=%d\n",
                  command, full, left, right);
    return true;
}

const char *autoDriveStateName()
{
    switch (gAutoDriveState) {
    case AutoDriveState::Cruise: return "CRUISE";
    case AutoDriveState::TurnLeft: return "TURN LEFT";
    case AutoDriveState::TurnRight: return "TURN RIGHT";
    case AutoDriveState::Reverse: return "REVERSE";
    default: return "OFF";
    }
}

void disableAutoDrive(const char *reason)
{
    if (gAutoDriveEnabled) Serial.printf("[Z2 AUTO] stopped: %s\n", reason);
    gAutoDriveEnabled = false;
    gAutoDriveState = AutoDriveState::Off;
    stopMotors();
}

void noteDashboardActivity()
{
    if (gVirtualPowerOn) gLastDashboardActivityMs = millis();
}

void setVirtualPowerState(bool turnOn, const char *reason)
{
    if (turnOn) {
        gVirtualPowerOn = true;
        gLastDashboardActivityMs = millis();
        digitalWrite(kSpeakerShutdownPin, HIGH);
        if (gOledReady) gDisplay.ssd1306_command(SSD1306_DISPLAYON);
        gRingMode = RingMode::Auto;
        refreshRing();
        setTrafficLight(true, false, false);
        gLastLightReadMs = gLastUltrasonicReadMs = gLastTofReadMs = 0;
        gLastOledUpdateMs = 0;
        Serial.printf("[Z2 POWER] virtual power ON: %s\n", reason);
        return;
    }

    disableAutoDrive(reason);
    gCliffRecoveryActive = false;
    gLocalRecoveryActive = false;
    gLocalRecoveryAttempted = false;
    gEmergencyStopRequested = false;
    gVirtualPowerOn = false;
    gRingMode = RingMode::Off;
    refreshRing();
    if (gOledReady) gDisplay.ssd1306_command(SSD1306_DISPLAYOFF);
    setTrafficLight(false, false, false);
    i2s_zero_dma_buffer(kSpeakerI2sPort);
    digitalWrite(kSpeakerShutdownPin, LOW);
    Serial.printf("[Z2 POWER] manually off: %s; network remains available\n", reason);
}

bool autoSensorsReady()
{
    return gUltrasonicValid && gTofReady[0] && gTofReady[1];
}

uint16_t autoRangeMm(uint8_t index)
{
    if (!gTofReady[index]) return 0;
    return gTofValid[index] ? gTofDistanceMm[index] : 2000;
}

void startAutoState(AutoDriveState state)
{
    gAutoDriveState = state;
    gAutoStateStartedMs = millis();
    Serial.printf("[Z2 AUTO] state=%s\n", autoDriveStateName());
}

void updateAutoDrive()
{
    if (!gAutoDriveEnabled || millis() - gLastAutoDecisionMs < kAutoDecisionIntervalMs) return;
    gLastAutoDecisionMs = millis();

    if (!autoSensorsReady()) {
        disableAutoDrive("front sensor or sonar unavailable");
        return;
    }
    if (gCliffDetected[0] || gCliffDetected[1] || gCliffRecoveryActive) {
        if (!gCliffRecoveryActive) stopMotors();
        return;
    }

    const unsigned long stateAge = millis() - gAutoStateStartedMs;
    const uint16_t leftMm = autoRangeMm(0);
    const uint16_t rightMm = autoRangeMm(1);
    const uint16_t backMm = autoRangeMm(2);
    const bool leftBlocked = leftMm < gAutoFrontClearMm;
    const bool rightBlocked = rightMm < gAutoFrontClearMm;
    const bool centerBlocked = gLatestDistanceCm < gObstacleWarningDistanceCm;
    const bool rearClear = gTofReady[2] && backMm >= gAutoRearClearMm;

    if (gAutoDriveState == AutoDriveState::Reverse) {
        if (!rearClear) {
            disableAutoDrive("rear path blocked");
            return;
        }
        if (stateAge < kAutoReverseMs) {
            applyDrive('B', kAutoReverseSpeed);
            return;
        }
        startAutoState(gAutoTurnAfterReverse == 'L'
            ? AutoDriveState::TurnLeft : AutoDriveState::TurnRight);
        return;
    }

    if (gAutoDriveState == AutoDriveState::TurnLeft ||
        gAutoDriveState == AutoDriveState::TurnRight) {
        const char turn = gAutoDriveState == AutoDriveState::TurnLeft ? 'L' : 'R';
        const bool turnBlocked = turn == 'L' ? leftBlocked : rightBlocked;
        if (stateAge < kAutoTurnMs && !turnBlocked) {
            applyDrive(turn, kAutoTurnSpeed);
            return;
        }
        startAutoState(AutoDriveState::Cruise);
        return;
    }

    if (!centerBlocked && !leftBlocked && !rightBlocked) {
        applyDrive('F', gAutoCruiseSpeed);
        return;
    }

    stopMotors();
    gCollisionWarningLatched = true;
    gAutoTurnAfterReverse = rightMm > leftMm ? 'R' : 'L';
    const bool chosenSideBlocked = gAutoTurnAfterReverse == 'L' ? leftBlocked : rightBlocked;
    if (!chosenSideBlocked) {
        startAutoState(gAutoTurnAfterReverse == 'L'
            ? AutoDriveState::TurnLeft : AutoDriveState::TurnRight);
    } else if (rearClear) {
        startAutoState(AutoDriveState::Reverse);
    } else {
        disableAutoDrive("surrounded or rear path unavailable");
    }
}

void handleDrive()
{
    if (!gServer.hasArg("cmd")) {
        gServer.send(400, "text/plain", "Missing cmd");
        return;
    }
    noteDashboardActivity();
    disableAutoDrive("manual control");
    const String command = gServer.arg("cmd");
    const int speed = gServer.hasArg("speed") ? gServer.arg("speed").toInt() : gSpeed;
    const bool accepted = applyDrive(command.length() ? command[0] : 'S', speed);
    gServer.send(200, "application/json",
                 String("{\"robot\":\"Z2\",\"command\":\"") + gCommand +
                 "\",\"speed\":" + gSpeed +
                 ",\"blocked\":" + (accepted ? "false" : "true") + "}");
}

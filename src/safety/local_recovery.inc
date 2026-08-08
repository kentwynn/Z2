constexpr unsigned long kLocalRecoveryDurationMs = 320;
constexpr unsigned long kLocalRecoveryPivotDurationMs = 220;
constexpr unsigned long kLocalRecoveryMaximumMs = 900;
// Stay below normal cruise speed (180), but above the measured low-speed
// region where the drivetrain may not overcome static friction.
constexpr int kLocalRecoverySpeed = 150;
constexpr int kLocalRecoveryPivotSpeed = 145;
constexpr int kLocalRecoveryStartSpeed = 120;

void driveLocalRecovery(char command, int speed)
{
    const int translationMotor = command == 'B' ? -speed : speed;
    const int rightMotor = command == 'R' ? -speed
                         : command == 'L' ? speed : translationMotor;
    const int leftMotor = command == 'R' ? speed
                        : command == 'L' ? -speed : translationMotor;
    driveMotor(rightMotor, kAin1Pin, kAin2Pin, kRightPwmChannel);
    driveMotor(leftMotor, kBin1Pin, kBin2Pin, kLeftPwmChannel);
}

bool localRecoveryTriggerStillActive(char command)
{
    if (command == 'B')
        return (gUltrasonicValid &&
                gLatestDistanceCm <= kTouchReactionDistanceCm) ||
            (gTofValid[0] &&
             gTofDistanceMm[0] <= kTouchReactionDistanceMm) ||
            (gTofValid[1] &&
             gTofDistanceMm[1] <= kTouchReactionDistanceMm);
    if (command == 'F')
        return gTofValid[2] &&
            gTofDistanceMm[2] <= kTouchReactionDistanceMm;
    if (command == 'R')
        return gTofValid[0] &&
            gTofDistanceMm[0] <= kTouchReactionDistanceMm;
    if (command == 'L')
        return gTofValid[1] &&
            gTofDistanceMm[1] <= kTouchReactionDistanceMm;
    return false;
}

void notifyRecoveryBlocked(const String &detail)
{
    const unsigned long now = millis();
    if (gLastRecoveryBlockedNoticeMs &&
        now - gLastRecoveryBlockedNoticeMs < 1500) return;
    gLastRecoveryBlockedNoticeMs = now;
    gSafetyStopActive = true;
    requestPresentation(PresentationOwner::Safety,
                        gCliffHazardActive ? gPresetSafetyCliff
                                           : gPresetSafetyObstacle,
                        gCliffHazardActive ? "cliff" : "obstacle", false);
    addRealtimeLog(String("Safety recovery blocked: ") + detail);
    sendDeviceEvent("Safety", "Recovery blocked", detail);
}

bool recoveryDirectionClear(char command)
{
    // A cliff dominates every other hazard. The only bounded recovery that
    // follows the robot's known travelled floor is backward from a front edge.
    if (gCliffDetected[0] || gCliffDetected[1]) {
        return command == 'B' && gTofValid[2] &&
            gTofDistanceMm[2] > kTofResetDistanceMm;
    }
    // For an ordinary front-proximity escape, use every millimetre that the
    // valid rear sensor can safely supervise. The loop rechecks at the ToF
    // cadence and stops at the hard boundary; the wider reset margin is for
    // hysteresis, not a reason to refuse all backward motion.
    if (command == 'B')
        return gTofValid[2] &&
            gTofDistanceMm[2] > kTofStopDistanceMm;
    if (command == 'F')
        return gUltrasonicValid &&
            gLatestDistanceCm > kEmergencyResetDistanceCm &&
            gTofValid[0] && gTofDistanceMm[0] > kTofResetDistanceMm &&
            gTofValid[1] && gTofDistanceMm[1] > kTofResetDistanceMm;
    // A short in-place pivot does not translate the chassis into the blocked
    // front/rear path. It is allowed only away from a single close front zone,
    // with the destination-side sensor valid and positively clear.
    if (command == 'R')
        return !gCliffDetected[0] && !gCliffDetected[1] &&
            gTofValid[1] && gTofDistanceMm[1] > kTofResetDistanceMm;
    if (command == 'L')
        return !gCliffDetected[0] && !gCliffDetected[1] &&
            gTofValid[0] && gTofDistanceMm[0] > kTofResetDistanceMm;
    return false;
}

bool tryStartLocalSafetyRecovery(char interruptedCommand, const char *hazard)
{
    if (!gAutomaticSafetyRecovery || gLocalRecoveryActive ||
        gLocalRecoveryAttempted) return false;
    gLocalRecoveryAttempted = true;

    const bool leftBlocked = gTofValid[0] &&
        gTofDistanceMm[0] <= kTofStopDistanceMm;
    const bool rightBlocked = gTofValid[1] &&
        gTofDistanceMm[1] <= kTofStopDistanceMm;
    const bool rearBlocked = gTofValid[2] &&
        gTofDistanceMm[2] <= kTofStopDistanceMm;
    const bool sonarBlocked = gUltrasonicValid &&
        gLatestDistanceCm <= kEmergencyStopDistanceCm;
    const bool frontBlocked = sonarBlocked || leftBlocked || rightBlocked;
    char candidate = 'S';

    if (gCliffDetected[0] || gCliffDetected[1]) {
        candidate = 'B';
    } else if (leftBlocked && !rightBlocked) {
        // Move the occupied front-left corner away from the obstacle. This is
        // preferred over a generic reverse when the side geometry is known.
        candidate = 'R';
    } else if (rightBlocked && !leftBlocked) {
        candidate = 'L';
    } else if (rearBlocked && !frontBlocked) {
        candidate = 'F';
    } else if (frontBlocked && !rearBlocked) {
        candidate = 'B';
    } else if (frontBlocked && rearBlocked &&
               gTofValid[0] && gTofValid[1]) {
        // Translation is unavailable in both directions; pivot toward the
        // verified side with more measured clearance.
        candidate = gTofDistanceMm[1] >= gTofDistanceMm[0] ? 'R' : 'L';
    } else if (interruptedCommand == 'B') {
        candidate = 'F';
    } else {
        candidate = 'B';
    }

    // Central sonar contact can leave both front ToFs clear. If reverse is
    // unavailable, use their measured clearance to choose the safer pivot.
    if (!recoveryDirectionClear(candidate) && sonarBlocked &&
        gTofValid[0] && gTofValid[1]) {
        const char roomier = gTofDistanceMm[1] >= gTofDistanceMm[0]
            ? 'R' : 'L';
        if (recoveryDirectionClear(roomier)) candidate = roomier;
    }
    if (!recoveryDirectionClear(candidate)) {
        String detail = String(hazard) + " · no verified-safe " +
            (candidate == 'B' ? "rear" : candidate == 'F' ? "front"
                                                       : "pivot") + " path";
        if (gCliffDetected[0] || gCliffDetected[1]) detail += " · cliff active";
        if (gTofValid[2] && gTofDistanceMm[2] <= kTofResetDistanceMm)
            detail += " · rear blocked";
        notifyRecoveryBlocked(detail);
        return false;
    }

    gLocalRecoveryCommand = candidate;
    gLocalRecoveryStartedMs = millis();
    gLocalRecoveryActive = true;
    driveLocalRecovery(candidate, kLocalRecoveryStartSpeed);
    gCommand = candidate;
    gMoving = true;
    gLastDriveCommandMs = millis();
    setTrafficLight(false, true, false);
    addRealtimeLog(String("Local safety recovery: ") + candidate +
                   " · " + hazard);
    sendDeviceEvent("Safety", "Recovery started",
                    String(hazard) + " · direction " + candidate);
    return true;
}

void updateLocalSafetyRecovery()
{
    if (!gLocalRecoveryActive) return;
    if (!recoveryDirectionClear(gLocalRecoveryCommand)) {
        stopMotors();
        gLocalRecoveryActive = false;
        notifyRecoveryBlocked("escape path became unsafe");
        return;
    }
    const unsigned long elapsed = millis() - gLocalRecoveryStartedMs;
    const int targetSpeed =
        (gLocalRecoveryCommand == 'L' || gLocalRecoveryCommand == 'R')
            ? kLocalRecoveryPivotSpeed : kLocalRecoverySpeed;
    const int rampedSpeed = min(targetSpeed,
        kLocalRecoveryStartSpeed + static_cast<int>(elapsed / 8));
    driveLocalRecovery(gLocalRecoveryCommand, rampedSpeed);
    const unsigned long duration =
        (gLocalRecoveryCommand == 'L' || gLocalRecoveryCommand == 'R')
            ? kLocalRecoveryPivotDurationMs : kLocalRecoveryDurationMs;
    if (elapsed < duration) return;
    if (localRecoveryTriggerStillActive(gLocalRecoveryCommand) &&
        elapsed < kLocalRecoveryMaximumMs) return;

    const bool escaped =
        !localRecoveryTriggerStillActive(gLocalRecoveryCommand);
    stopMotors();
    gLocalRecoveryActive = false;
    if (!escaped) {
        notifyRecoveryBlocked("avoidance limit reached · hazard still active");
        return;
    }

    gSafetyStopActive = false;
    gLocalRecoveryAttempted = false;
    gLastRecoveryBlockedNoticeMs = 0;
    gTouchContactActive = false;
    gLastTouchReactionMs = 0;
    auto &safetyPresentation =
        gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Safety)];
    if (safetyPresentation.active && safetyPresentation.event == "obstacle")
        clearPresentation(PresentationOwner::Safety);
    auto &interactionPresentation =
        gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Interaction)];
    if (interactionPresentation.active &&
        interactionPresentation.event == "near_presence")
        clearPresentation(PresentationOwner::Interaction);
    sendDeviceEvent("Safety", "Recovery completed",
                    "local hazard cleared");
    addRealtimeLog("Local safety recovery completed · hazard cleared");
}

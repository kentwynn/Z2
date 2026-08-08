constexpr unsigned long kLocalRecoveryDurationMs = 320;
constexpr unsigned long kLocalRecoveryPivotDurationMs = 220;
// A person or moving object can follow the robot and keep a proximity reading
// inside the reset boundary forever. Recovery is therefore closed-loop but
// bounded: stop earlier when clear, or stop after this short escape budget and
// remain visibly blocked until the hazard is removed.
constexpr unsigned long kLocalRecoveryMaximumMs = 900;
// Stay below normal cruise speed (180), but above the measured low-speed
// region where the drivetrain may not overcome static friction.
constexpr int kLocalRecoverySpeed = 150;
constexpr int kLocalRecoveryPivotSpeed = 145;
constexpr int kLocalRecoveryStartSpeed = 120;

void driveLocalRecovery(char command, int speed)
{
    const int translationMotor = command == 'B' ? -speed : speed;
    const int rightMotor = command == 'Q' ? speed
                         : command == 'E' ? speed / 2
                         : command == 'R' ? -speed
                         : command == 'L' ? speed : translationMotor;
    const int leftMotor = command == 'Q' ? speed / 2
                        : command == 'E' ? speed
                        : command == 'R' ? speed
                        : command == 'L' ? -speed : translationMotor;
    driveMotor(rightMotor, kAin1Pin, kAin2Pin, kRightPwmChannel);
    driveMotor(leftMotor, kBin1Pin, kBin2Pin, kLeftPwmChannel);
}

bool localRecoveryTriggerStillActive(char command)
{
    if (command == 'B')
        return (gUltrasonicValid &&
                gLatestDistanceCm <= kEmergencyResetDistanceCm) ||
            (gTofValid[0] &&
             gTofDistanceMm[0] <= kTofResetDistanceMm) ||
            (gTofValid[1] &&
             gTofDistanceMm[1] <= kTofResetDistanceMm);
    if (command == 'F')
        return gTofValid[2] &&
            gTofDistanceMm[2] <= kTofResetDistanceMm;
    if (command == 'R')
        return gTofValid[1] &&
            gTofDistanceMm[1] <= kTofResetDistanceMm;
    if (command == 'L')
        return gTofValid[0] &&
            gTofDistanceMm[0] <= kTofResetDistanceMm;
    if (command == 'Q')
        return (gTofValid[2] &&
                gTofDistanceMm[2] <= kTofResetDistanceMm) ||
            (gTofValid[1] &&
             gTofDistanceMm[1] <= kTofResetDistanceMm);
    if (command == 'E')
        return (gTofValid[2] &&
                gTofDistanceMm[2] <= kTofResetDistanceMm) ||
            (gTofValid[0] &&
             gTofDistanceMm[0] <= kTofResetDistanceMm);
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
        return command == 'B' &&
            (!gTofValid[2] || gTofDistanceMm[2] > kTofResetDistanceMm);
    }
    // For an ordinary front-proximity escape, use every millimetre that the
    // valid rear sensor can safely supervise. The loop rechecks at the ToF
    // cadence and stops at the hard boundary; the wider reset margin is for
    // hysteresis, not a reason to refuse all backward motion.
    if (command == 'B')
        return !gTofValid[2] ||
            gTofDistanceMm[2] > kTofStopDistanceMm;
    if (command == 'F') {
        // Rear-proximity recovery must not be trapped merely because a
        // forward sensor has no echo or is temporarily out of range. Every
        // available reading remains authoritative: a known close boundary
        // blocks forward motion, while an unavailable reading contributes no
        // restriction. The normal moving safety loop keeps sampling and stops
        // immediately if any sensor subsequently reports the hard boundary.
        const bool sonarClear = !gUltrasonicValid ||
            gLatestDistanceCm > kEmergencyResetDistanceCm;
        const bool leftClear = !gTofValid[0] ||
            gTofDistanceMm[0] > kTofResetDistanceMm;
        const bool rightClear = !gTofValid[1] ||
            gTofDistanceMm[1] > kTofResetDistanceMm;
        return sonarClear && leftClear && rightClear;
    }
    // A short in-place pivot does not translate the chassis into the blocked
    // front/rear path. It is allowed only away from a single close front zone,
    // unless an available destination-side sensor reports a close boundary.
    // During a left pivot the right-front corner advances and the left-front
    // corner retreats; during a right pivot the opposite is true. Validate
    // the advancing corner, not the side named by the turn command.
    if (command == 'R')
        return !gCliffDetected[0] && !gCliffDetected[1] &&
            (!gTofValid[0] || gTofDistanceMm[0] > kTofResetDistanceMm);
    if (command == 'L')
        return !gCliffDetected[0] && !gCliffDetected[1] &&
            (!gTofValid[1] || gTofDistanceMm[1] > kTofResetDistanceMm);
    // Q/E are forward-left/forward-right escape arcs. Validate the target
    // front quadrant; the occupied rear/opposite side is the source we are
    // moving away from.
    if (command == 'Q')
        return !gCliffDetected[0] && !gCliffDetected[1] &&
            (!gTofValid[0] || gTofDistanceMm[0] > kTofResetDistanceMm);
    if (command == 'E')
        return !gCliffDetected[0] && !gCliffDetected[1] &&
            (!gTofValid[1] || gTofDistanceMm[1] > kTofResetDistanceMm);
    return false;
}

uint16_t recoveryDirectionScore(char command)
{
    if (!recoveryDirectionClear(command)) return 0;
    // Unknown/out-of-range is eligible by policy and receives a neutral open
    // score. A real available measurement still provides the exact score and
    // always wins or blocks according to its distance.
    if (command == 'B') return gTofValid[2] ? gTofDistanceMm[2] : 2000;
    if (command == 'L') return gTofValid[1] ? gTofDistanceMm[1] : 2000;
    if (command == 'R') return gTofValid[0] ? gTofDistanceMm[0] : 2000;
    if (command == 'Q') return gTofValid[0] ? gTofDistanceMm[0] : 2000;
    if (command == 'E') return gTofValid[1] ? gTofDistanceMm[1] : 2000;
    if (command == 'F') {
        const uint16_t sonarMm = gUltrasonicValid
            ? static_cast<uint16_t>(constrain(
                static_cast<int>(gLatestDistanceCm * 10.0f), 0, 4000))
            : 2000;
        const uint16_t leftMm = gTofValid[0] ? gTofDistanceMm[0] : 2000;
        const uint16_t rightMm = gTofValid[1] ? gTofDistanceMm[1] : 2000;
        return min(sonarMm,
                   min(leftMm, rightMm));
    }
    return 0;
}

bool tryStartLocalSafetyRecovery(char /*interruptedCommand*/, const char *hazard)
{
    if (!gAutomaticSafetyRecovery || gLocalRecoveryActive ||
        gLocalRecoveryAttempted) return false;
    gLocalRecoveryAttempted = true;

    char candidate = 'S';
    uint16_t candidateScore = 0;
    const bool sonarBlocked = gUltrasonicValid &&
        gLatestDistanceCm <= kEmergencyStopDistanceCm;
    const bool leftBlocked =
        (gTofValid[0] && gTofDistanceMm[0] <= kTofStopDistanceMm) ||
        (!gTofValid[0] && gTofReady[0] && sonarBlocked && gTofValid[1]);
    const bool rightBlocked =
        (gTofValid[1] && gTofDistanceMm[1] <= kTofStopDistanceMm) ||
        (!gTofValid[1] && gTofReady[1] && sonarBlocked && gTofValid[0]);
    const bool rearBlocked = gTofValid[2] &&
        gTofDistanceMm[2] <= kTofStopDistanceMm;
    const bool anyFrontBlocked = sonarBlocked || leftBlocked || rightBlocked;

    if (gCliffDetected[0] || gCliffDetected[1]) {
        candidate = 'B';
        candidateScore = recoveryDirectionScore(candidate);
    } else if (anyFrontBlocked && !rearBlocked) {
        // A front-boundary event is best resolved by translating the entire
        // chassis away. Do not let a large side range make a pivot beat a
        // verified rear escape; one front corner advances during any pivot.
        candidate = 'B';
        candidateScore = recoveryDirectionScore(candidate);
    } else if (rearBlocked && rightBlocked && !leftBlocked) {
        candidate = 'Q';
        candidateScore = recoveryDirectionScore(candidate);
    } else if (rearBlocked && leftBlocked && !rightBlocked) {
        candidate = 'E';
        candidateScore = recoveryDirectionScore(candidate);
    } else {
        // First apply kinematic eligibility: a candidate must move each known
        // occupied boundary away, not merely point toward a large range.
        // Then score the eligible candidates by verified clearance.
        constexpr char candidates[] = {'F', 'B', 'L', 'R'};
        for (char direction : candidates) {
            bool eligible = false;
            if (direction == 'F') {
                eligible = rearBlocked && !anyFrontBlocked;
            } else if (direction == 'B') {
                eligible = anyFrontBlocked && !rearBlocked;
            } else if (direction == 'L') {
                // Left pivot retreats the left-front corner. With a central
                // front obstacle, prefer straight reverse whenever available.
                eligible = leftBlocked && !rightBlocked &&
                    (!sonarBlocked || rearBlocked);
                if (sonarBlocked && rearBlocked &&
                    !leftBlocked && !rightBlocked) eligible = true;
            } else if (direction == 'R') {
                eligible = rightBlocked && !leftBlocked &&
                    (!sonarBlocked || rearBlocked);
                if (sonarBlocked && rearBlocked &&
                    !leftBlocked && !rightBlocked) eligible = true;
            }
            if (!eligible) continue;
            const uint16_t score = recoveryDirectionScore(direction);
            if (score > candidateScore) {
                candidate = direction;
                candidateScore = score;
            }
        }
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
                   " · clearance " + candidateScore + " mm · " + hazard);
    sendDeviceEvent("Safety", "Recovery started",
                    String(hazard) + " · direction " + candidate +
                    " · clearance " + candidateScore + " mm");
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
    gMoving = true;
    gLastDriveCommandMs = millis();
    const unsigned long duration =
        (gLocalRecoveryCommand == 'L' || gLocalRecoveryCommand == 'R')
            ? kLocalRecoveryPivotDurationMs : kLocalRecoveryDurationMs;
    if (elapsed < duration) return;
    // Closed-loop avoidance continues only within a bounded escape budget.
    // Direction validity above is re-evaluated every loop, so a new boundary
    // or cliff still stops immediately.
    if (localRecoveryTriggerStillActive(gLocalRecoveryCommand)) {
        if (elapsed < kLocalRecoveryMaximumMs) return;
        stopMotors();
        gLocalRecoveryActive = false;
        notifyRecoveryBlocked("bounded escape complete · hazard still present");
        return;
    }

    stopMotors();
    gLocalRecoveryActive = false;
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

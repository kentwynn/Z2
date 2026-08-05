void updateUltrasonic()
{
    if (millis() - gLastUltrasonicReadMs < kUltrasonicReadIntervalMs) return;
    gLastUltrasonicReadMs = millis();
    digitalWrite(kUltrasonicTriggerPin, LOW);
    delayMicroseconds(3);
    digitalWrite(kUltrasonicTriggerPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(kUltrasonicTriggerPin, LOW);

    const unsigned long pulseUs =
        pulseIn(kUltrasonicEchoPin, HIGH, kUltrasonicEchoTimeoutUs);
    if (pulseUs > 100 && pulseUs < kUltrasonicEchoTimeoutUs) {
        const float measuredDistance = pulseUs * 0.0343f * 0.5f;
        if (measuredDistance >= 2.0f && measuredDistance <= 450.0f) {
            gLatestDistanceCm = measuredDistance;
            gDistanceCm = gUltrasonicValid
                ? gDistanceCm * 0.65f + measuredDistance * 0.35f
                : measuredDistance;
            gUltrasonicValid = true;
            gLastValidEchoMs = millis();
            if (measuredDistance <= kEmergencyStopDistanceCm &&
                gMoving && gCommand != 'B') {
                gEmergencyStopRequested = true;
            }
        }
    }
    if (gUltrasonicValid && millis() - gLastValidEchoMs > kUltrasonicStaleMs) {
        gUltrasonicValid = false;
        Serial.println("[Z2 SONAR] no valid echo");
    }
}


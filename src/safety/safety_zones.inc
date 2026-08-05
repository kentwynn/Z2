bool allSafetyZonesClear()
{
    if (gCliffDetected[0] || gCliffDetected[1]) return false;
    if (gUltrasonicValid && gLatestDistanceCm < kEmergencyResetDistanceCm) return false;
    for (uint8_t index = 0; index < kTofCount; ++index) {
        if (gTofValid[index] && gTofDistanceMm[index] < kTofResetDistanceMm) return false;
    }
    return true;
}


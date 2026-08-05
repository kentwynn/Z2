void initTofSensors()
{
    for (uint8_t index = 0; index < kTofCount; ++index) {
        bool initialized = false;
        VL53L0X &l0 = *gTofL0Sensors[index];
        l0.setTimeout(60);
        for (uint8_t attempt = 1; attempt <= 2 && !initialized; ++attempt) {
            digitalWrite(kTofXshutPins[index], HIGH);
            delay(50);
            initialized = l0.init();
            if (!initialized) {
                digitalWrite(kTofXshutPins[index], LOW);
                delay(25);
                Serial.printf("[Z2 TOF] sensor %u VL53L0X attempt %u failed\n",
                              index + 1, attempt);
            }
        }

        if (initialized) {
            gTofType[index] = 1;
            l0.setAddress(kTofAddresses[index]);
            l0.setMeasurementTimingBudget(20000);
            l0.startContinuous(30);
        } else {
            VL53L1X &l1 = *gTofL1Sensors[index];
            l1.setTimeout(60);
            for (uint8_t attempt = 1; attempt <= 2 && !initialized; ++attempt) {
                digitalWrite(kTofXshutPins[index], HIGH);
                delay(50);
                initialized = l1.init();
                if (!initialized) {
                    digitalWrite(kTofXshutPins[index], LOW);
                    delay(25);
                    Serial.printf("[Z2 TOF] sensor %u VL53L1X attempt %u failed\n",
                                  index + 1, attempt);
                }
            }
            if (initialized) {
                gTofType[index] = 2;
                l1.setAddress(kTofAddresses[index]);
                l1.setDistanceMode(VL53L1X::Short);
                l1.setMeasurementTimingBudget(20000);
                l1.startContinuous(30);
            }
        }

        if (!initialized) {
            digitalWrite(kTofXshutPins[index], LOW);
            Serial.printf("[Z2 TOF] sensor %u XSHUT GPIO%u init=FAILED\n",
                          index + 1, kTofXshutPins[index]);
            continue;
        }
        gTofReady[index] = true;
        Serial.printf("[Z2 TOF] sensor %u type=VL53L%uX XSHUT GPIO%u address=0x%02X init=OK\n",
                      index + 1, gTofType[index] == 1 ? 0 : 1,
                      kTofXshutPins[index], kTofAddresses[index]);
    }
}

bool tofBlocksCommand(char command, uint8_t &blockedSensor)
{
    bool check[kTofCount] = {false, false, false};
    switch (command) {
    case 'F': check[0] = check[1] = true; break;
    case 'L': case 'X': check[0] = true; break;
    case 'R': case 'Y': check[1] = true; break;
    case 'B': check[2] = true; break;
    default: return false;
    }
    for (uint8_t index = 0; index < kTofCount; ++index) {
        if (check[index] && gTofValid[index] &&
            gTofDistanceMm[index] <= kTofStopDistanceMm) {
            blockedSensor = index;
            return true;
        }
    }
    return false;
}

void updateTofSensors()
{
    if (millis() - gLastTofReadMs < kTofReadIntervalMs) return;
    gLastTofReadMs = millis();
    const uint8_t index = gNextTofIndex;
    gNextTofIndex = (gNextTofIndex + 1) % kTofCount;
    if (gTofReady[index]) {
        uint16_t measuredMm = 0;
        bool timedOut = true;
        if (gTofType[index] == 1) {
            VL53L0X &sensor = *gTofL0Sensors[index];
            measuredMm = sensor.readRangeContinuousMillimeters();
            timedOut = sensor.timeoutOccurred();
        } else if (gTofType[index] == 2) {
            VL53L1X &sensor = *gTofL1Sensors[index];
            measuredMm = sensor.readRangeContinuousMillimeters();
            timedOut = sensor.timeoutOccurred();
        }
        const bool valid = !timedOut &&
            measuredMm >= 30 && measuredMm <= 2000;
        if (valid) {
            gTofDistanceMm[index] = gTofValid[index]
                ? static_cast<uint16_t>((gTofDistanceMm[index] * 2UL + measuredMm) / 3UL)
                : measuredMm;
        }
        gTofValid[index] = valid;
    }

    uint8_t blockedSensor = 0;
    if (gMoving && tofBlocksCommand(gCommand, blockedSensor)) {
        gEmergencyStopRequested = true;
    }
}


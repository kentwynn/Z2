void driveMotor(int value, uint8_t in1Pin, uint8_t in2Pin, uint8_t pwmChannel)
{
    value = constrain(value, -255, 255);
    digitalWrite(in1Pin, value > 0 ? HIGH : LOW);
    digitalWrite(in2Pin, value < 0 ? HIGH : LOW);
    ledcWrite(pwmChannel, abs(value));
}

void stopMotors()
{
    driveMotor(0, kAin1Pin, kAin2Pin, kRightPwmChannel);
    driveMotor(0, kBin1Pin, kBin2Pin, kLeftPwmChannel);
    gMoving = false;
    gCommand = 'S';
}

void setTrafficLight(bool red, bool yellow, bool green)
{
    digitalWrite(kTrafficRedPin, red ? HIGH : LOW);
    digitalWrite(kTrafficYellowPin, yellow ? HIGH : LOW);
    digitalWrite(kTrafficGreenPin, green ? HIGH : LOW);
}


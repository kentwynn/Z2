import json
import math
from pathlib import Path

Import("env")


project_dir = Path(env.subst("$PROJECT_DIR"))
source_path = project_dir / "config" / "hardware_pin_map.json"
output_path = project_dir / "include" / "generated_hardware_profile.h"

profile = json.loads(source_path.read_text())["drivetrain"]
wheel_mm = float(profile["wheelDiameterMm"])
track_mm = float(profile["wheelCenterSpacingMm"])
rpm = float(profile["gearMotorRatedRpm"])
rated_pwm = int(profile["ratedPwm"])
if wheel_mm <= 0 or track_mm <= 0 or rpm <= 0 or not 1 <= rated_pwm <= 255:
    raise RuntimeError("Invalid drivetrain values in config/hardware_pin_map.json")

theoretical_linear_cm_s = math.pi * (wheel_mm / 10.0) * rpm / 60.0
theoretical_angular_deg_s = math.degrees(
    2.0 * theoretical_linear_cm_s / (track_mm / 10.0)
)
linear_cm_s = profile.get("measuredLinearCmPerSecond") or theoretical_linear_cm_s
angular_deg_s = profile.get("measuredAngularDegPerSecond") or theoretical_angular_deg_s

generated = f"""// Generated from config/hardware_pin_map.json. Do not edit manually.
#pragma once
constexpr float kWheelDiameterMm = {wheel_mm:.3f}f;
constexpr float kWheelCenterSpacingMm = {track_mm:.3f}f;
constexpr float kGearMotorRatedRpm = {rpm:.3f}f;
constexpr uint8_t kDrivetrainRatedPwm = {rated_pwm};
constexpr float kDrivetrainLinearCmPerSecond = {linear_cm_s:.6f}f;
constexpr float kDrivetrainAngularDegPerSecond = {angular_deg_s:.6f}f;
constexpr bool kDrivetrainUsesMeasuredCalibration = {'true' if profile.get('measuredLinearCmPerSecond') and profile.get('measuredAngularDegPerSecond') else 'false'};
"""
if not output_path.exists() or output_path.read_text() != generated:
    output_path.write_text(generated)

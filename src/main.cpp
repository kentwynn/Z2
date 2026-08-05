// Z2 firmware composition root.
//
// Feature implementations are kept in focused fragments because they share
// one real-time application state and must compile as a single translation
// unit on Arduino. This keeps ownership visible without adding global
// cross-module coupling or changing proven hardware timing.

#include "app/context.inc"
#include "ui/control_page.inc"

#include "devices/motors.inc"
#include "safety/cliff_safety.inc"
#include "devices/traffic_light.inc"
#include "devices/ambient_ring.inc"
#include "sensors/ultrasonic.inc"
#include "sensors/time_of_flight.inc"
#include "safety/safety_zones.inc"
#include "devices/oled_display.inc"
#include "devices/audio.inc"
#include "domain/presentation_manager.inc"
#include "network/realtime_chat.inc"
#include "network/control_channel.inc"
#include "network/robot_onboarding.inc"

#include "domain/drive_and_autonomy.inc"
#include "network/network_and_ota.inc"
#include "app/lifecycle.inc"

# Z2

Z2 is an independent ESP32-S3-WROOM-1-N16R8 two-wheel robot project. It uses
one TB6612FNG motor driver and provides a small browser controller at
`http://z2.local`.

## Firmware structure

`src/main.cpp` is the composition root. Z2 is split by responsibility:

- `app/` — shared application state and Arduino lifecycle
- `domain/` — drive commands and autonomous-drive policy
- `sensors/` — ultrasonic and mixed VL53L0X/VL53L1X ranging
- `safety/` — cliff recovery and safety-zone rules
- `devices/` — motors, lights, OLED, audio, and ambient ring
- `network/` — Wi-Fi, mDNS, and OTA lifecycle
- `ui/` — responsive local command-center dashboard

The focused `.inc` fragments intentionally compile as one Arduino translation
unit. This preserves deterministic access to the shared real-time robot state
while keeping hardware and domain responsibilities easy to find.

The Z1 firmware is outside this directory and is not selected or modified when
building Z2.

## Realtime conversation configuration

Z2 contains no compiled API credential or conversational fallback. The tracked
`include/robot_config.h` contains only the device ID and technical transport and
audio parameters. Install the scoped `rk_` device credential from the Kent Wynn
account through `http://z2.local`; Z2 stores it in NVS. Name, owner, character,
instructions, wake word, language, voice, thinking mode, and follow-up duration
must be loaded successfully from the Kent Wynn API before voice chat is armed.

Z2's configured wake phrase is `Z2`. The intended conversation flow
is: listen locally for the wake phrase, open the authenticated WebSocket, send
the user's speech, play the reply, keep a short follow-up window, then close the
connection and return to local wake-word mode. A small microphone pre-roll must
be retained so speech immediately following “Z2” is not clipped. Wake-word
Without an embedded wake-word model, local VAD opens a short authenticated
session and sends the detected utterance to Kent Wynn speech-to-text. The server
calls Qwen and TTS only when the transcript begins with the configured phrase.
After a successful wake, the same socket accepts follow-up questions for 20
seconds before Z2 closes it and returns to wake listening.

The TLS connection pins the current `api.kentwynn.com` certificate fingerprint
to fit within the stable non-PSRAM Arduino build. Update the fingerprint before
the pinned certificate expires or whenever the API certificate rotates.

## Current scope

- One TB6612FNG
- Right N20 motor on AO1/AO2
- Left N20 motor on BO1/BO2
- Forward, reverse, pivot/spin left, pivot/spin right, and stop
- Adjustable motor speed
- Automatic stop after 700 ms without a fresh command
- Automatic stop when the control button is released
- `z2.local` via mDNS
- `Z2-Setup` fallback access point if home Wi-Fi cannot connect
- MAX98357A I2S speaker with Z2 sound and help controls
- Browser speaker-volume adjustment from 0 to 100 percent
- VEML7700 ambient-light sensing on the shared I2C bus
- Automatic LED-ring color and brightness based on ambient lux
- HY-SRF05 live distance sensing and health status
- 10 cm ultrasonic movement lockout and emergency stop with audible warning; reverse remains available
- Three mixed VL53L0X/VL53L1X directional sensors with independent health, range, and 10 cm movement lockouts
- Two TCRT-5000 cliff sensors with yellow warning and guarded automatic reverse recovery
- Opt-in Auto Drive with conservative cruising, clearer-side turning, guarded reversing, and manual Stop override
- Manual All On/All Off virtual power control; Z2 stays on until explicitly turned off
- INMP441 microphone with live browser level and peak readings
- 12-pixel Adafruit NeoPixel ring with color, rainbow, and brightness controls
- Three-color movement light: green for clear forward travel, blinking yellow for turns/reverse or a nearby obstacle, and red when stopped or blocked
- SSD1306 128x64 OLED status display

The complete pin assignment is in `config/hardware_pin_map.json`.

Current channel-A bring-up mapping:

- PWMA: GPIO10
- AIN1: GPIO8
- AIN2: GPIO7

Speaker mapping:

- LRC/WS: GPIO4
- BCLK: GPIO5
- DIN: GPIO6
- SD: GPIO3
- VIN: LM2596 OUT+
- GND: common ground

Microphone mapping:

- SD: GPIO9
- VDD: ESP32 3V3
- GND: common ground
- L/R: GND for left-channel mode
- WS: GPIO4, shared with speaker LRC
- SCK: GPIO5, shared with speaker BCLK

VEML7700:

- VIN: 3V3
- GND: common GND
- SDA: GPIO47, shared with OLED
- SCL: GPIO42, shared with OLED

HY-SRF05:

- VCC: regulated 5V LM2596 OUT+
- GND: LM2596 OUT- / common ground
- TRIGGER: GPIO18
- ECHO: GPIO40 through a 10k/20k 5V-to-3.3V divider

Time-of-flight sensors (shared SDA GPIO47 / SCL GPIO42, powered from 3V3):

- Front left: VL53L1X, XSHUT GPIO2, runtime address 0x30
- Front right: VL53L0X, XSHUT GPIO39, runtime address 0x31
- Back: VL53L1X, XSHUT GPIO1, runtime address 0x32

Cliff sensors:

- Both VCC: ESP32-S3 3V3
- Both GND: common ground
- Left D0: GPIO45
- Right D0: GPIO48
- Cliff signal: HIGH when floor reflection is missing; the module indicator LED normally means safe floor

NeoPixel ring mapping:

- IN/data: GPIO14
- VDD: regulated 5 V
- GND: common ground
- Pixel count: 12

Do not power the ring from the motor rail while it is adjusted to 6 V or 6.6 V.
Use a regulated 5 V rail for the NeoPixel VDD connection.

Traffic-light mapping:

- G: GPIO15
- Y: GPIO16
- R: GPIO17
- GND: common ground

The firmware runs a red-yellow-green startup test. Green means clear forward
movement, slow-blinking yellow means turning or reversing, fast-blinking yellow
means an obstacle is within 25 cm, and solid red means stopped or safety-blocked.

## Auto Drive

Open `http://z2.local` and press **Auto drive**. Auto mode starts only when
both front ToF sensors and the ultrasonic sensor are ready. It cruises at a
up to PWM 225, turns at PWM 200, and reverses briefly at PWM 180 only when
the rear ToF sensor confirms space. Cliff detection, obstacle lockouts, the
dashboard Stop button, and any manual drive command override Auto mode.
Auto mode always starts off after reboot.
Use a traffic-light module with onboard current-limiting resistors; the GPIO
control level is 3.3 V.

OLED mapping:

- VCC: ESP32 3V3
- GND: common ground
- SCL: GPIO42
- SDA: GPIO47
- I2C address: 0x3C

## Configure and flash

Wi-Fi is not compiled into the firmware. A blank Z2 opens its protected setup
network and displays a QR code on the OLED. The captive setup page stores the
verified network in device-local NVS and keeps the RGB ring off throughout
setup.

The first OTA-enabled firmware installation must use USB:

```bash
./z2.sh flash
```

The first USB flash installs a random device-scoped OTA credential over the
physical serial link and saves the matching secret in macOS Keychain. Update
wirelessly afterward without copying or exporting a password:

```bash
./z2.sh ota
```

OTA resolves `z2-001.local`, authenticates with the device-scoped password, stops both
motors before writing firmware, and restarts Z2 after a successful update.

```bash
chmod +x z2.sh
./z2.sh build
./z2.sh flash-monitor
```

After startup, open `http://z2.local`. If home Wi-Fi fails, connect to the
`Z2-Setup` network with password `z2robot01`, then open `http://192.168.4.1`.

## First motor test

Lift both wheels off the floor before pressing a direction button. If one wheel
runs backward while the other runs forward, do not change Z1. Correct only Z2
by swapping that motor's two output leads or by adding a Z2-only software
inversion after confirming which side is reversed.

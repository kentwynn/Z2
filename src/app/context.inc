#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_VEML7700.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Irisoled.h>
#include <Preferences.h>
#include <VL53L0X.h>
#include <VL53L1X.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <freertos/ringbuf.h>
#include <esp_heap_caps.h>
#include <qrcode.h>
#include <math.h>

extern "C" {
#include <webrtc/common_audio/vad/include/webrtc_vad.h>
}

#include "robot_config.h"
#include "z2_hello_audio.h"

namespace
{
constexpr char kProductName[] = "Z2";
constexpr char kFirmwareVersion[] = "0.2.0";
constexpr char kHostName[] = Z2_ROBOT_ID;
constexpr unsigned long kWifiTimeoutMs = 15000;
constexpr unsigned long kCommandTimeoutMs = 1200;

// Z2 has one TB6612FNG and two motors.
constexpr uint8_t kPwmAPin = 10;
constexpr uint8_t kAin1Pin = 8;
constexpr uint8_t kAin2Pin = 7;
constexpr uint8_t kStandbyPin = 12;
constexpr uint8_t kBin1Pin = 13;
constexpr uint8_t kBin2Pin = 41;
constexpr uint8_t kPwmBPin = 11;

constexpr uint8_t kRightPwmChannel = 0; // AO1/AO2
constexpr uint8_t kLeftPwmChannel = 1;  // BO1/BO2
constexpr uint16_t kPwmFrequencyHz = 20000;
constexpr uint8_t kPwmResolutionBits = 8;

constexpr gpio_num_t kSpeakerWsPin = GPIO_NUM_4;
constexpr gpio_num_t kSpeakerBclkPin = GPIO_NUM_5;
constexpr gpio_num_t kSpeakerDataPin = GPIO_NUM_6;
constexpr gpio_num_t kMicrophoneDataPin = GPIO_NUM_9;
constexpr uint8_t kSpeakerShutdownPin = 3;
constexpr i2s_port_t kSpeakerI2sPort = I2S_NUM_0;
constexpr uint32_t kSpeakerSampleRate = 16000;

constexpr uint8_t kRingDataPin = 14;
constexpr uint8_t kRingPixelCount = 12;
constexpr uint8_t kRingDefaultBrightness = 80;
constexpr uint8_t kTrafficGreenPin = 15;
constexpr uint8_t kTrafficYellowPin = 16;
constexpr uint8_t kTrafficRedPin = 17;
constexpr float kObstacleWarningDistanceCm = 25.0f;
constexpr uint16_t kTofWarningDistanceMm = 250;
// The stationary personality reaction should require a deliberate near touch;
// collision protection below remains at the wider 10 cm emergency threshold.
constexpr float kTouchReactionDistanceCm = 6.0f;
constexpr uint16_t kTouchReactionDistanceMm = 60;
constexpr unsigned long kTouchReactionHoldMs = 2500;

constexpr uint8_t kOledSclPin = 42;
constexpr uint8_t kOledSdaPin = 47;
constexpr uint8_t kOledAddress = 0x3C;
constexpr uint8_t kLightSensorAddress = 0x10;
constexpr unsigned long kLightReadIntervalMs = 250;
constexpr uint8_t kUltrasonicTriggerPin = 18;
constexpr uint8_t kUltrasonicEchoPin = 40;
constexpr uint32_t kUltrasonicEchoTimeoutUs = 30000;
constexpr unsigned long kUltrasonicReadIntervalMs = 180;
constexpr unsigned long kUltrasonicStaleMs = 1800;
constexpr float kEmergencyStopDistanceCm = 10.0f;
constexpr float kEmergencyResetDistanceCm = 14.0f;
constexpr uint8_t kTofCount = 3;
constexpr uint8_t kTofXshutPins[kTofCount] = {2, 39, 1};
constexpr uint8_t kTofAddresses[kTofCount] = {0x30, 0x31, 0x32};
constexpr unsigned long kTofReadIntervalMs = 25;
constexpr uint16_t kTofStopDistanceMm = 100;
constexpr uint16_t kTofResetDistanceMm = 140;
constexpr uint8_t kCliffLeftPin = 45;
constexpr uint8_t kCliffRightPin = 48;
// GPIO35-37 are reserved by octal PSRAM on the N16R8 module. The cliff sensor
// outputs are moved to exposed GPIO45 and GPIO48 so sensing can remain enabled
// without corrupting external RAM.
constexpr bool kCliffSensorsEnabled = true;
// The module pulls D0 LOW while it sees reflective floor. A missing
// reflection (edge/cliff) releases D0 HIGH.
constexpr uint8_t kCliffDetectedLevel = HIGH;
constexpr unsigned long kCliffReadIntervalMs = 10;
constexpr uint8_t kCliffDebounceSamples = 3;
constexpr unsigned long kCliffRecoveryMaximumMs = 1500;
// The measured drivetrain may not overcome static friction near PWM 115.
// Cliff recovery must produce real reverse movement immediately, so use the
// same proven speed as local proximity recovery.
constexpr int kCliffRecoverySpeed = 150;
constexpr int kAutoCruiseSpeed = 225;
constexpr int kAutoTurnSpeed = 200;
constexpr int kAutoReverseSpeed = 180;
constexpr uint16_t kAutoFrontClearMm = 300;
constexpr uint16_t kAutoRearClearMm = 250;
constexpr float kAutoSonarClearCm = 30.0f;
constexpr unsigned long kAutoDecisionIntervalMs = 120;
constexpr unsigned long kAutoTurnMs = 620;
constexpr unsigned long kAutoReverseMs = 480;

uint16_t gObstacleWarningDistanceCm = 25;
uint16_t gAutoFrontClearMm = 300;
uint16_t gAutoRearClearMm = 250;
uint8_t gAutoCruiseSpeed = 180;
String gReverseSound = "DOUBLE";
String gCollisionSound = "BEEP";
bool gVoiceExpressionsEnabled = true;
constexpr uint8_t kCloudCommandQueueCapacity = 6;
String gCloudCommandQueue[kCloudCommandQueueCapacity];
uint8_t gCloudCommandQueueHead = 0;
uint8_t gCloudCommandQueueSize = 0;
String gLastCloudCommandId;
bool gLastCloudCommandExecuted = false;
String gLastCloudCommandDetail;

enum class RingMode : uint8_t { Auto, Off, White, Red, Green, Blue, Rainbow };
enum class AmbientCondition : uint8_t {
    Unavailable, VeryDark, Dark, Dim, Comfortable, Bright, VeryBright
};
enum class DisplayMode : uint8_t { Off, Face, Info, Sensors };
enum class OledPrimitiveKind : uint8_t { Text, Line, Rect, Circle };
struct OledPrimitive {
    OledPrimitiveKind kind = OledPrimitiveKind::Text;
    int16_t x = 0, y = 0, x2 = 0, y2 = 0;
    uint8_t width = 1, height = 1, radius = 1, size = 1;
    bool fill = false;
    String text;
};
constexpr uint8_t kOledPrimitiveCapacity = 12;
OledPrimitive gOledPrimitives[kOledPrimitiveCapacity];
uint8_t gOledPrimitiveCount = 0;
unsigned long gOledCanvasUntilMs = 0;
enum class FaceEmotion : uint8_t {
    Neutral, Happy, Curious, Sleepy, Surprised, Excited,
    Sad, Love, Confused, Thinking, Angry, Alert, Focused,
    Furious, Scared, Worried, Bored, Despair
};
enum class AutoDriveState : uint8_t { Off, Cruise, TurnLeft, TurnRight, Reverse };
enum class PresentationOwner : uint8_t { Idle, Identity, Voice, Control, Interaction, System, Safety, Count };
struct PresentationPreset { String oled; String sound; String animation; };
struct PresentationSlot { bool active = false; PresentationPreset preset; String event; };
PresentationSlot gPresentationSlots[static_cast<uint8_t>(PresentationOwner::Count)];
PresentationPreset gPresetIdle{"FACE", "OFF", "IDLE"};
PresentationPreset gPresetVoiceListening{"ALERT", "OFF", "LISTENING"};
PresentationPreset gPresetVoiceThinking{"FOCUSED", "OFF", "PROCESSING"};
PresentationPreset gPresetVoiceSpeaking{"HAPPY", "OFF", "TALKING"};
PresentationPreset gPresetControlForward{"FOCUSED", "OFF", "STATIC"};
PresentationPreset gPresetControlReverse{"FOCUSED", "DOUBLE", "STATIC"};
PresentationPreset gPresetControlTurn{"FOCUSED", "OFF", "STATIC"};
PresentationPreset gPresetControlAuto{"FOCUSED", "OFF", "STATIC"};
PresentationPreset gPresetControlLeft{"FOCUSED", "OFF", "STATIC"};
PresentationPreset gPresetControlRight{"FOCUSED", "OFF", "STATIC"};
PresentationPreset gPresetSafetyCliff{"ALERT", "BEEP", "ALERT"};
PresentationPreset gPresetSafetyObstacle{"ALERT", "BEEP", "ALERT"};
PresentationPreset gPresetSafetyProximity{"ALERT", "BEEP", "ALERT"};
PresentationPreset gPresetSystemConnecting{"ALERT", "BEEP", "ALERT"};
PresentationPreset gPresetSystemError{"ALERT", "BEEP", "ALERT"};
// Legacy config name; shares the one contact-safety presentation.
PresentationPreset gPresetInteractionTouch{"ALERT", "BEEP", "ALERT"};
String gFaceAnimation = "IDLE";
// Fail closed until a successfully authenticated API configuration is loaded.
bool gToolboxControlEnabled = false;
bool gToolboxVoiceEnabled = false;
FaceEmotion gListeningEmotion = FaceEmotion::Curious;
FaceEmotion gThinkingEmotion = FaceEmotion::Curious;
FaceEmotion gSpeakingEmotion = FaceEmotion::Happy;
FaceEmotion gIdleEmotion = FaceEmotion::Neutral;
// Short-lived voice events (wake accepted/rejected, uncertainty, transport
// failure) temporarily override the steady conversation-state face.
FaceEmotion gVoiceEventEmotion = FaceEmotion::Neutral;
String gVoiceEventAnimation = "IDLE";
unsigned long gVoiceEventUntilMs = 0;

WebServer gServer(80);
DNSServer gSetupDns;
Adafruit_NeoPixel gRing(kRingPixelCount, kRingDataPin, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 gDisplay(128, 64, &Wire, -1);
Adafruit_VEML7700 gLightSensor;
VL53L0X gTofFrontLeft;
VL53L0X gTofFrontRight;
VL53L0X gTofBack;
VL53L0X *const gTofL0Sensors[kTofCount] = {
    &gTofFrontLeft, &gTofFrontRight, &gTofBack
};
VL53L1X gTofL1FrontLeft;
VL53L1X gTofL1FrontRight;
VL53L1X gTofL1Back;
VL53L1X *const gTofL1Sensors[kTofCount] = {
    &gTofL1FrontLeft, &gTofL1FrontRight, &gTofL1Back
};
Preferences gPreferences;
Preferences gRobotPreferences;
bool gWifiSetupMode = false;
bool gOtaReady = false;
String gWifiSetupPassword;
String gWifiSetupSsid;
String gWifiConnectedSsid;
String gWifiConnectionNote;
void showWifiSetupInstructions();
void showWifiSetupQr();
void showPhonePairingQr();
String gPhonePairingUrl;
bool gPhonePairingVisible = false;
bool gPhoneSessionActive = false;
unsigned long gPhonePairingUntilMs = 0;
String gRobotDeviceToken;
String gManagedRobotName;
String gManagedWakeWord;
String gManagedLanguage;
String gManagedVoice;
bool gManagedThinkingEnabled = false;
bool gCapabilityDisplay = true;
bool gCapabilitySpeaker = true;
bool gCapabilityLedRing = true;
bool gCapabilityMovement = false;
bool gCapabilityAutonomousDrive = false;
uint16_t gManagedFollowUpSeconds = 0;
uint16_t gControlVoiceSuspendSeconds = 20;
unsigned long gControlLeaseUntilMs = 0;
uint32_t gManagedConfigVersion = 0;
bool gRobotConfigChanged = false;
bool gRobotConfigSyncRequested = false;
unsigned long gLastRobotConfigPollMs = 0;

const char *activeRobotToken();
bool hasConfiguredRealtimeToken();
void loadRobotOnboardingState();
bool syncManagedRobotConfig();
bool reportRobotStatus();
void pollManagedRobotConfig();
void pollRobotCommands();
void playSound(const String &requestedName);
void interruptRealtimePlaybackForSafety();
void saveRobotDeviceToken(const String &token);
bool gRobotConfigSynced = false;
unsigned long gLastDriveCommandMs = 0;
bool gMoving = false;
bool gSpeakerReady = false;
bool gMicrophoneReady = false;
uint32_t gMicrophoneRms = 0;
uint32_t gMicrophonePeak = 0;
uint32_t gMicrophoneFloor = 0;
uint32_t gVoiceVadEnergyGate = 0;
bool gVoiceDetected = false;
VadInst *gVoiceVad = nullptr;
int16_t gVoiceVadFrame[160] = {0};
size_t gVoiceVadFrameSamples = 0;
bool gVoiceVadReady = false;
void resetVoiceVad();
volatile bool gRealtimeSpeakerActive = false;
RingbufHandle_t gRealtimeAudioBuffer = nullptr;
size_t gRealtimeAudioBufferCapacity = 0;
volatile bool gRealtimeAudioEnded = false;
volatile unsigned long gRealtimeMicResumeAtMs = 0;
uint64_t gMicrophoneCalibrationSum = 0;
uint16_t gMicrophoneCalibrationFrames = 0;
uint8_t gVoiceAttackFrames = 0;
unsigned long gLastVoiceMs = 0;
unsigned long gLastMicrophoneReadMs = 0;

void handleRealtimeMicrophoneFrame(const int32_t *samples, size_t sampleCount,
                                   size_t activeChannel);
void addRealtimeLog(const String &message);
void updateRealtimeConversation();
bool controlLeaseActive();
void activateControlLease(bool stopping = false);
void setupControlChannel();
void updateControlChannel();
void enqueueCloudCommand(const String &payload);
bool peekCloudCommand(String &payload);
void discardCloudCommand();
void playRealtimePcm(const uint8_t *pcm, size_t length);
void playWakeAcknowledgement();
void playWakeRejection();
void startRealtimePlaybackTask();
void disableAutoDrive(const char *reason);
bool applyDrive(char command, int speed);
void requestPresentation(PresentationOwner owner, const PresentationPreset &preset, const char *event, bool playEntrySound = true);
void clearPresentation(PresentationOwner owner);
void sendDeviceEvent(const char *tool, const char *event, const String &detail);
bool autoSensorsReady();
bool allSafetyZonesClear();
void startAutoState(AutoDriveState state);
bool tryStartLocalSafetyRecovery(char interruptedCommand, const char *hazard);
void updateLocalSafetyRecovery();
void notifyRecoveryBlocked(const String &detail);
unsigned long gLastRingAnimationMs = 0;
uint16_t gRingRainbowOffset = 0;
RingMode gRingMode = RingMode::Auto;
bool gVoiceRingOverlayActive = false;
uint8_t gVoiceRingBrightness = 32;
uint8_t gVoiceRingRed = 40;
uint8_t gVoiceRingGreen = 120;
uint8_t gVoiceRingBlue = 255;
String gVoiceRingAnimation = "solid";
uint16_t gVoiceRingSpeedMs = 900;
String gOledAiTitle = "Z2";
String gOledAiAccent;
String gOledAiSymbol = "*";
bool gOledReady = false;
unsigned long gLastOledUpdateMs = 0;
DisplayMode gDisplayMode = DisplayMode::Off;
String gOledHomeToolbox = "VOICE";
DisplayMode gControlDisplayMode = DisplayMode::Sensors;
DisplayMode gSystemDisplayMode = DisplayMode::Info;
FaceEmotion gFaceEmotion = FaceEmotion::Neutral;
String gOledLiveStatus;
String gOledLiveDetail;
String gOledToolPrimary;
String gOledToolSecondary;
String gOledToolDetail;
String gOledLastHeard;
String gOledLastReply;
String gOledReplyMood;
size_t gOledReplyAudioBytes = 0;
unsigned long gOledConsoleUntilMs = 0;
bool gConversationListeningVisual = false;
bool gConversationThinkingVisual = false;
bool gConversationSpeakingVisual = false;
bool gLightSensorReady = false;
float gAmbientLux = 0.0f;
float gSmoothedLux = 0.0f;
unsigned long gLastLightReadMs = 0;
AmbientCondition gAmbientCondition = AmbientCondition::Unavailable;
uint8_t gAutoRingBrightness = 0;
uint8_t gAutoRed = 255;
uint8_t gAutoGreen = 80;
uint8_t gAutoBlue = 5;
uint16_t gAutoOffLux = 300;
uint8_t gLightSensitivity = 7;
float gDistanceCm = 0.0f;
float gLatestDistanceCm = 0.0f;
bool gUltrasonicValid = false;
bool gCollisionWarningLatched = false;
bool gEmergencyStopRequested = false;
bool gSafetyStopActive = false;
bool gAutomaticSafetyRecovery = false;
bool gLocalRecoveryActive = false;
bool gLocalRecoveryAttempted = false;
char gLocalRecoveryCommand = 'S';
unsigned long gLocalRecoveryStartedMs = 0;
unsigned long gLastRecoveryBlockedNoticeMs = 0;
unsigned long gTrafficLightStartedMs = 0;
unsigned long gTrafficLightOverrideUntilMs = 0;
String gTrafficLightOverride = "AUTO";
unsigned long gLastTouchReactionMs = 0;
bool gTouchContactActive = false;
bool gCliffDetected[2] = {false, false};
bool gCliffRawHigh[2] = {false, false};
uint8_t gCliffDebounce[2] = {0, 0};
bool gCliffRecoveryActive = false;
bool gCliffHazardActive = false;
bool gCliffArmed = true;
unsigned long gCliffRecoveryStartedMs = 0;
unsigned long gCliffClearStartedMs = 0;
unsigned long gLastCliffWarningMs = 0;
unsigned long gLastCliffReadMs = 0;
unsigned long gLastUltrasonicReadMs = 0;
unsigned long gLastValidEchoMs = 0;
bool gTofReady[kTofCount] = {false, false, false};
uint8_t gTofType[kTofCount] = {0, 0, 0}; // 0=unknown, 1=VL53L0X, 2=VL53L1X
bool gTofValid[kTofCount] = {false, false, false};
uint16_t gTofDistanceMm[kTofCount] = {0, 0, 0};
unsigned long gLastTofReadMs = 0;
uint8_t gNextTofIndex = 0;
uint8_t gSpeakerVolumePercent = 60;
constexpr uint16_t kMicrophoneCalibrationFrameCount = 50;
// Require a sustained voice attack rather than allowing a short desk tap,
// motor click, Wi-Fi noise burst, or room transient to open an STT turn.
constexpr uint8_t kVoiceAttackRequiredFrames = 15;
// A follow-up begins only after an accepted wake or a completed reply, so it
// can use a shorter attack than idle wake listening. This preserves quick
// phrases such as "bye bye" without making ambient noise easier to wake Z2.
constexpr uint8_t kFollowUpVoiceAttackRequiredFrames = 7;
bool gRealtimeFollowUpAttack = false;
constexpr unsigned long kVoiceHangoverMs = 1000;
// The I2S DMA queue is explicitly cleared when playback ends. A short guard is
// enough for the amplifier tail; 700 ms made the UI say FOLLOW_UP while the
// microphone was still deaf to a natural immediate response.
constexpr unsigned long kPostPlaybackMicGuardMs = 250;
constexpr uint32_t kVoiceMeterFullScale = 300000;
// Z2 currently measures roughly 15k RMS at idle with a 22k calibrated floor.
// A 40k base combined with the adaptive floor gate starts near 33-34k at the
// configured sensitivity: comfortably above room noise, but reachable by
// normal speech after a rejected wake attempt.
constexpr uint32_t kVoiceMinimumRms = (kVoiceMeterFullScale * 40U) / 300U;

uint32_t voiceSpeechOnThreshold()
{
    const float sensitivity = constrain(Z2_WAKE_WORD_SENSITIVITY, 0.0f, 1.0f);
    // Higher sensitivity lowers both the absolute and noise-relative gates.
    // At the default 0.65 this remains safely above the measured 20-25k idle
    // floor while retaining quieter first syllables of the wake phrase.
    const float minimumScale = 1.20f - (0.50f * sensitivity);
    const float floorScale = 2.30f - (1.00f * sensitivity);
    return max<uint32_t>(
        static_cast<uint32_t>(kVoiceMinimumRms * minimumScale),
        static_cast<uint32_t>(gMicrophoneFloor * floorScale));
}

bool microphoneSpeechNow()
{
    return gMicrophoneRms >= voiceSpeechOnThreshold();
}

int gSpeed = 180;
char gCommand = 'S';
bool gAutoDriveEnabled = false;
bool gVirtualPowerOn = true;
unsigned long gLastDashboardActivityMs = 0;
AutoDriveState gAutoDriveState = AutoDriveState::Off;
char gAutoTurnAfterReverse = 'L';
unsigned long gAutoStateStartedMs = 0;
unsigned long gLastAutoDecisionMs = 0;

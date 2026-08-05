#pragma once

// Non-secret device identity and transport/audio parameters only.
// The scoped rk_ credential is installed through <robot-id>.local and stored in NVS.
// Name, owner, character, instructions, wake word, language, voice, thinking,
// and follow-up duration are managed exclusively by the Kent Wynn API.

#define Z2_ROBOT_ID "z2-001"

#define Z2_REALTIME_ENABLED true
#define Z2_REALTIME_HOST "api.kentwynn.com"
#define Z2_REALTIME_PORT 443
#define Z2_REALTIME_PATH "/v1/robot/chat"
#define Z2_CONTROL_PATH "/v1/robot/control"

#define Z2_WAKE_WORD_SENSITIVITY 0.65f
#define Z2_WAKE_AUDIO_PREROLL_MS 500
#define Z2_INPUT_AUDIO_SAMPLE_RATE 16000
#define Z2_OUTPUT_AUDIO_SAMPLE_RATE 24000
#define Z2_MIC_PCM_GAIN 2
#define Z2_SILENCE_END_MS 1400
#define Z2_MAX_RECORDING_MS 15000

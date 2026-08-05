websockets::WebsocketsClient gControlSocket;
unsigned long gControlLastConnectAttemptMs = 0;
unsigned long gControlConnectingStartedMs = 0;
bool gControlConnecting = false;

void enqueueCloudCommand(const String &payload)
{
    if (payload.isEmpty()) return;
    if (gCloudCommandQueueSize == kCloudCommandQueueCapacity) {
        gCloudCommandQueue[gCloudCommandQueueHead] = "";
        gCloudCommandQueueHead =
            (gCloudCommandQueueHead + 1) % kCloudCommandQueueCapacity;
        --gCloudCommandQueueSize;
        addRealtimeLog("Control queue full - oldest command dropped");
    }
    const uint8_t tail =
        (gCloudCommandQueueHead + gCloudCommandQueueSize) %
        kCloudCommandQueueCapacity;
    gCloudCommandQueue[tail] = payload;
    ++gCloudCommandQueueSize;
    Serial.printf("[Z2 CONTROL] enqueued size=%u head=%u bytes=%u\n",
                  gCloudCommandQueueSize, gCloudCommandQueueHead,
                  static_cast<unsigned>(payload.length()));
}

bool peekCloudCommand(String &payload)
{
    if (!gCloudCommandQueueSize) return false;
    payload = gCloudCommandQueue[gCloudCommandQueueHead];
    return true;
}

void discardCloudCommand()
{
    if (!gCloudCommandQueueSize) return;
    gCloudCommandQueue[gCloudCommandQueueHead] = "";
    gCloudCommandQueueHead =
        (gCloudCommandQueueHead + 1) % kCloudCommandQueueCapacity;
    --gCloudCommandQueueSize;
}

void sendDeviceEvent(const char *tool, const char *event, const String &detail)
{
    if (!gControlSocket.available()) return;
    JsonDocument message;
    message["type"] = "device.event";
    message["id"] = String(millis(), HEX);
    message["tool"] = tool;
    message["event"] = event;
    message["detail"] = detail;
    String payload;
    serializeJson(message, payload);
    gControlSocket.send(payload);
}

void handleControlMessage(websockets::WebsocketsMessage message)
{
    if (!message.isText()) return;
    JsonDocument event;
    if (deserializeJson(event, message.c_str(), message.length())) return;
    const char *eventType = event["type"] | "";
    if (!strcmp(eventType, "control.ping")) {
        gControlSocket.send("{\"type\":\"control.pong\"}");
        return;
    }
    if (!strcmp(eventType, "control.connected")) {
        Serial.println("[Z2 CONTROL] secure command channel ready");
        addRealtimeLog("Control channel connected");
        return;
    }
    if (!strcmp(eventType, "device.event.ack")) return;
    if (!strcmp(eventType, "device.command")) {
        String payload;
        serializeJson(event["command"], payload);
        enqueueCloudCommand(payload);
        const char *command = event["command"]["command"] | "unknown";
        const char *value = event["command"]["value"] | "";
        addRealtimeLog(String("Control queued [") + gCloudCommandQueueSize +
                       "]: " + command + (value[0] ? String(" ") + value : "") +
                       (gRobotConfigSynced ? "" : " · config sync pending"));
    }
}

void handleControlSocketEvent(websockets::WebsocketsEvent type, String detail)
{
    if (type == websockets::WebsocketsEvent::ConnectionOpened) {
        gControlConnecting = false;
        Serial.println("[Z2 CONTROL] WebSocket connected securely");
    } else if (type == websockets::WebsocketsEvent::ConnectionClosed) {
        gControlConnecting = false;
        Serial.printf("[Z2 CONTROL] WebSocket disconnected detail=%s\n",
                      detail.c_str());
        addRealtimeLog(String("Control channel disconnected") +
                       (detail.length() ? String(" · ") + detail : ""));
    }
}

void beginControlConnection()
{
    if (!hasConfiguredRealtimeToken() || WiFi.status() != WL_CONNECTED) return;
    gControlLastConnectAttemptMs = millis();
    gControlConnectingStartedMs = gControlLastConnectAttemptMs;
    gControlConnecting = true;
    gControlSocket = websockets::WebsocketsClient();
    gControlSocket.addHeader("X-AI-Token", activeRobotToken());
    gControlSocket.setCACert(kKentWynnRootCa);
    gControlSocket.onMessage(handleControlMessage);
    gControlSocket.onEvent(handleControlSocketEvent);
    if (!gControlSocket.connectSecure(Z2_REALTIME_HOST, Z2_REALTIME_PORT,
                                      Z2_CONTROL_PATH)) {
        gControlConnecting = false;
        addRealtimeLog("Control channel connection failed");
    }
}

void setupControlChannel()
{
    if (hasConfiguredRealtimeToken() && WiFi.status() == WL_CONNECTED)
        beginControlConnection();
}

void updateControlChannel()
{
    gControlSocket.poll();
    const unsigned long now = millis();
    if (gControlSocket.available()) {
        // A compact one-second snapshot lets server-side AI tools answer from
        // Redis without opening a request socket or interrupting voice audio.
        static unsigned long lastTelemetryMs = 0;
        if (now - lastTelemetryMs >= 1000) {
            lastTelemetryMs = now;
            JsonDocument message;
            message["type"] = "device.telemetry";
            JsonObject telemetry = message["telemetry"].to<JsonObject>();
            telemetry["sonar_cm"] = gUltrasonicValid ? gLatestDistanceCm : -1;
            telemetry["sonar_valid"] = gUltrasonicValid;
            JsonArray tof = telemetry["tof_mm"].to<JsonArray>();
            for (uint8_t index = 0; index < kTofCount; ++index)
                tof.add(gTofValid[index] ? gTofDistanceMm[index] : -1);
            JsonArray cliff = telemetry["cliff"].to<JsonArray>();
            cliff.add(gCliffDetected[0]); cliff.add(gCliffDetected[1]);
            telemetry["light_lux"] = gAmbientLux;
            telemetry["light_sensor_ready"] = gLightSensorReady;
            telemetry["moving"] = gMoving;
            telemetry["direction"] = String(gCommand);
            telemetry["virtual_power"] = gVirtualPowerOn;
            telemetry["safety_stop"] = gSafetyStopActive;
            telemetry["cliff_recovery"] = gCliffRecoveryActive;
            JsonObject capabilities = telemetry["capabilities"].to<JsonObject>();
            capabilities["movement"] = gCapabilityMovement && gToolboxControlEnabled;
            capabilities["autonomous_drive"] = gCapabilityAutonomousDrive;
            capabilities["display"] = gCapabilityDisplay;
            capabilities["speaker"] = gCapabilitySpeaker && gSpeakerReady;
            capabilities["led_ring"] = gCapabilityLedRing;
            capabilities["traffic_light"] = true;
            String payload;
            serializeJson(message, payload);
            gControlSocket.send(payload);
        }
        return;
    }
    if (gControlConnecting && now - gControlConnectingStartedMs < 15000) return;
    gControlConnecting = false;
    if (now - gControlLastConnectAttemptMs >= 5000) beginControlConnection();
}

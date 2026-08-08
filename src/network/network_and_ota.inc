String setupPassword()
{
    const uint64_t chip = ESP.getEfuseMac();
    char password[9];
    snprintf(password, sizeof(password), "Z2%06lX",
             static_cast<unsigned long>(chip) & 0xFFFFFFUL);
    return String(password);
}

String setupSsid()
{
    const uint64_t chip = ESP.getEfuseMac();
    char ssid[7];
    snprintf(ssid, sizeof(ssid), "Z2%04lX",
             static_cast<unsigned long>(chip) & 0xFFFFUL);
    return String(ssid);
}

bool connectToWifi(const String &ssid, const String &password, const char *source)
{
    if (ssid.isEmpty()) return false;
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    // Z2 is an always-powered realtime appliance. Modem sleep can leave long-
    // lived TLS WebSockets half-open after hours of inactivity.
    WiFi.setSleep(false);
    WiFi.setHostname(kHostName);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.printf("[Z2 WIFI] trying %s network \"%s\"", source, ssid.c_str());
    const unsigned long startedMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedMs < kWifiTimeoutMs) {
        delay(250);
        Serial.print('.');
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(" failed");
        WiFi.disconnect(true);
        return false;
    }
    gWifiConnectedSsid = ssid;
    gWifiConnectionNote = source;
    Serial.printf(" connected at %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void updateNetworkConnection()
{
    if (gWifiSetupMode) return;
    static unsigned long lastCheckMs = 0;
    static unsigned long lastBeginMs = 0;
    static bool previouslyConnected = true;
    const unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED) {
        if (!previouslyConnected) {
            previouslyConnected = true;
            gWifiConnectedSsid = WiFi.SSID();
            gWifiConnectionNote = "automatic recovery";
            Serial.printf("[Z2 WIFI] connection restored at %s\n",
                          WiFi.localIP().toString().c_str());
            addRealtimeLog("Wi-Fi connection restored");
        }
        return;
    }
    if (previouslyConnected) {
        previouslyConnected = false;
        Serial.printf("[Z2 WIFI] connection lost status=%d\n", WiFi.status());
        addRealtimeLog("Wi-Fi connection lost · recovering");
    }
    if (now - lastCheckMs < 5000UL) return;
    lastCheckMs = now;
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    // Let the ESP32 driver's inexpensive reconnect run first. If it remains
    // offline, restart association from the saved consumer credentials without
    // blocking audio/safety processing or opening setup mode.
    if (now - lastBeginMs < 30000UL) {
        WiFi.reconnect();
        return;
    }
    const String savedSsid = gRobotPreferences.getString("wifi-ssid", "");
    const String savedPassword = gRobotPreferences.getString("wifi-pass", "");
    if (savedSsid.isEmpty()) return;
    lastBeginMs = now;
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(kHostName);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    Serial.printf("[Z2 WIFI] retrying saved network \"%s\"\n",
                  savedSsid.c_str());
}

void startWifiSetupAccessPoint()
{
    gWifiSetupMode = true;
    gWifiSetupPassword = setupPassword();
    gWifiSetupSsid = setupSsid();
    // AP+STA keeps the captive setup network available while the station
    // radio scans nearby home networks for the phone's network picker.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(gWifiSetupSsid.c_str(), gWifiSetupPassword.c_str());
    gSetupDns.start(53, "*", WiFi.softAPIP());
    Serial.printf("[Z2 WIFI] setup AP \"%s\" at %s; password shown on OLED\n",
                  gWifiSetupSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

void startNetwork()
{
    const bool setupRequested = gRobotPreferences.getBool("wifi-setup", false);
    if (setupRequested) {
        gRobotPreferences.remove("wifi-setup");
        startWifiSetupAccessPoint();
    }
    const String pendingSsid = gRobotPreferences.getString("wifi-next-s", "");
    const String pendingPassword = gRobotPreferences.getString("wifi-next-p", "");
    const String savedSsid = gRobotPreferences.getString("wifi-ssid", "");
    const String savedPassword = gRobotPreferences.getString("wifi-pass", "");

    if (!gWifiSetupMode && !pendingSsid.isEmpty()) {
        if (connectToWifi(pendingSsid, pendingPassword, "new")) {
            gRobotPreferences.putString("wifi-ssid", pendingSsid);
            gRobotPreferences.putString("wifi-pass", pendingPassword);
            gRobotPreferences.remove("wifi-next-s");
            gRobotPreferences.remove("wifi-next-p");
            Serial.println("[Z2 WIFI] new network verified and promoted");
        } else {
            gRobotPreferences.remove("wifi-next-s");
            gRobotPreferences.remove("wifi-next-p");
            gWifiConnectionNote = "new network failed; rolled back";
        }
    }
    if (!gWifiSetupMode && WiFi.status() != WL_CONNECTED && !savedSsid.isEmpty())
        connectToWifi(savedSsid, savedPassword, "saved");
    if (!gWifiSetupMode && WiFi.status() != WL_CONNECTED) startWifiSetupAccessPoint();

    if (!gWifiSetupMode && MDNS.begin(kHostName)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[Z2] diagnostics: http://%s.local\n", kHostName);
    } else {
        Serial.println("[Z2] mDNS failed; use the IP address printed above");
    }
}

void startOta()
{
    gOtaReady = false;
    const String otaPassword = gRobotPreferences.getString("ota-pass", "");
    if (gWifiSetupMode || otaPassword.length() < 12) {
        Serial.println("[Z2 OTA] disabled until secure device onboarding completes");
        return;
    }
    ArduinoOTA.setHostname(kHostName);
    ArduinoOTA.setPassword(otaPassword.c_str());
    ArduinoOTA.onStart([] {
        disableAutoDrive("OTA update");
        gCliffRecoveryActive = false;
        setTrafficLight(true, false, false);
        Serial.println("[Z2 OTA] update starting; motors stopped");
    });
    ArduinoOTA.onEnd([] { Serial.println("\n[Z2 OTA] update complete"); });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[Z2 OTA] error=%u\n", error);
    });
    ArduinoOTA.begin();
    gOtaReady = true;
    Serial.printf("[Z2 OTA] ready at %s.local:3232\n", kHostName);
}

void handleUsbOtaProvisioning()
{
    static String command;
    while (Serial.available()) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\r') continue;
        if (value != '\n') {
            if (command.length() < 160) command += value;
            continue;
        }
        constexpr char prefix[] = "Z2_OTA_PROVISION ";
        if (command.startsWith(prefix)) {
            String password = command.substring(sizeof(prefix) - 1);
            password.trim();
            const String existing = gRobotPreferences.getString("ota-pass", "");
            bool valid = password.length() >= 24 && password.length() <= 96;
            for (size_t index = 0; valid && index < password.length(); ++index) {
                const char c = password[index];
                valid = isAlphaNumeric(c) || c == '-' || c == '_';
            }
            if (!existing.isEmpty()) {
                Serial.println("[Z2 OTA] OTA_ALREADY_PROVISIONED");
            } else if (!valid) {
                Serial.println("[Z2 OTA] OTA_PROVISION_REJECTED");
            } else if (gRobotPreferences.putString("ota-pass", password) > 0) {
                Serial.println("[Z2 OTA] OTA_PROVISIONED");
                startOta();
            } else {
                Serial.println("[Z2 OTA] OTA_PROVISION_FAILED");
            }
            password = "";
        }
        command = "";
    }
}
} // namespace

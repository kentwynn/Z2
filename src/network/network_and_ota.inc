void startNetwork()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(kHostName);
    WiFi.begin(Z2_WIFI_SSID, Z2_WIFI_PASSWORD);
    Serial.printf("[Z2] connecting to Wi-Fi \"%s\"", Z2_WIFI_SSID);

    const unsigned long startedMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedMs < kWifiTimeoutMs) {
        delay(250);
        Serial.print('.');
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[Z2] Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(kFallbackApSsid, kFallbackApPassword);
        Serial.printf("\n[Z2] Wi-Fi failed; AP \"%s\" started at %s\n",
                      kFallbackApSsid, WiFi.softAPIP().toString().c_str());
    }

    if (MDNS.begin(kHostName)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[Z2] diagnostics: http://%s.local\n", kHostName);
    } else {
        Serial.println("[Z2] mDNS failed; use the IP address printed above");
    }
}

void startOta()
{
    if (strcmp(Z2_OTA_PASSWORD, "SET_YOUR_OTA_PASSWORD") == 0 ||
        strlen(Z2_OTA_PASSWORD) < 8) {
        Serial.println("[Z2 OTA] disabled; set a password of at least 8 characters");
        return;
    }
    ArduinoOTA.setHostname(kHostName);
    ArduinoOTA.setPassword(Z2_OTA_PASSWORD);
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
    Serial.printf("[Z2 OTA] ready at %s.local:3232\n", kHostName);
}
} // namespace

/**
 * @file ConfigManager.cpp
 * @brief WiFi and configuration management implementation
 */

#include "ConfigManager.h"
#include <cstring>

#ifndef NATIVE_BUILD
#include <WiFi.h>
#include <WiFiManager.h>
#endif

ConfigManager::ConfigManager() : _port(1883) {
    memset(_broker, 0, sizeof(_broker));
    loadConfig();
}

ConfigManager::~ConfigManager() {
#ifndef NATIVE_BUILD
    _prefs.end();
#endif
}

bool ConfigManager::beginWiFiManager(const char* apName) {
#ifndef NATIVE_BUILD
    WiFiManager wm;
    
    // Custom parameters for MQTT broker
    WiFiManagerParameter mqttServer("mqtt", "MQTT Broker", _broker, 127);
    wm.addParameter(&mqttServer);
    
    // Auto-connect or start portal
    bool connected = wm.autoConnect(apName);
    
    if (connected) {
        // Save broker from portal
        strncpy(_broker, mqttServer.getValue(), sizeof(_broker) - 1);
        saveConfig();
    }
    
    return connected;
#else
    return true;
#endif
}

bool ConfigManager::beginWiFi(const char* ssid, const char* password,
                               unsigned long timeoutMs) {
#ifndef NATIVE_BUILD
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            return false;
        }
        delay(100);
    }
    
    return true;
#else
    return true;
#endif
}

bool ConfigManager::isWiFiConnected() const {
#ifndef NATIVE_BUILD
    return WiFi.status() == WL_CONNECTED;
#else
    return true;
#endif
}

void ConfigManager::setBroker(const char* host, uint16_t port) {
    strncpy(_broker, host, sizeof(_broker) - 1);
    _broker[sizeof(_broker) - 1] = '\0';
    _port = port;
    saveConfig();
}

void ConfigManager::saveHardwareEnabled(const char* hardwareKey, bool enabled) {
#ifndef NATIVE_BUILD
    _prefs.begin("iot-mesurable", false);
    char key[48];
    snprintf(key, sizeof(key), "en_%s", hardwareKey);
    _prefs.putBool(key, enabled);
    _prefs.end();
#endif
}

bool ConfigManager::loadHardwareEnabled(const char* hardwareKey, bool defaultValue) {
#ifndef NATIVE_BUILD
    _prefs.begin("iot-mesurable", true);
    char key[48];
    snprintf(key, sizeof(key), "en_%s", hardwareKey);
    bool value = _prefs.getBool(key, defaultValue);
    _prefs.end();
    return value;
#else
    return defaultValue;
#endif
}

void ConfigManager::saveInterval(const char* hardwareKey, int intervalMs) {
#ifndef NATIVE_BUILD
    _prefs.begin("iot-mesurable", false);
    char key[48];
    snprintf(key, sizeof(key), "iv_%s", hardwareKey);
    _prefs.putInt(key, intervalMs);
    _prefs.end();
#endif
}

int ConfigManager::loadInterval(const char* hardwareKey, int defaultValue) {
#ifndef NATIVE_BUILD
    _prefs.begin("iot-mesurable", true);
    char key[48];
    snprintf(key, sizeof(key), "iv_%s", hardwareKey);
    int value = _prefs.getInt(key, defaultValue);
    _prefs.end();
    return value;
#else
    return defaultValue;
#endif
}

void ConfigManager::loadConfig() {
#ifndef NATIVE_BUILD
    _prefs.begin("iot-mesurable", true);
    
    if (_prefs.isKey("broker")) {
        size_t len = _prefs.getString("broker", _broker, sizeof(_broker));
        _broker[len] = '\0';
    }
    _port = _prefs.getUShort("port", 1883);
    
    _prefs.end();
#endif
}

void ConfigManager::saveConfig() {
#ifndef NATIVE_BUILD
    _prefs.begin("iot-mesurable", false);
    _prefs.putString("broker", _broker);
    _prefs.putUShort("port", _port);
    _prefs.end();
#endif
}

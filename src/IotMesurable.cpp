/**
 * @file IotMesurable.cpp
 * @brief Main library implementation
 */

#include "IotMesurable.h"
#include "core/SensorRegistry.h"
#include "core/MqttClient.h"
#include "core/ConfigManager.h"

#include <cstring>
#include <cstdio>

#ifndef NATIVE_BUILD
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_system.h>
#endif

// =============================================================================
// Constructor / Destructor
// =============================================================================

IotMesurable::IotMesurable(const char* moduleId) 
    : _port(1883), _lastStatusPublish(0), _lastSystemPublish(0), _lastConfigPublish(0) {
    
    strncpy(_moduleId, moduleId, sizeof(_moduleId) - 1);
    _moduleId[sizeof(_moduleId) - 1] = '\0';
    
    _moduleType[0] = '\0';
    memset(_broker, 0, sizeof(_broker));
    
    _registry = new SensorRegistry();
    _mqtt = new MqttClient();
    _config = new ConfigManager();
    
    _mqtt->setClientId(_moduleId);
}

IotMesurable::~IotMesurable() {
    delete _registry;
    delete _mqtt;
    delete _config;
}

// =============================================================================
// Initialization
// =============================================================================

bool IotMesurable::begin() {
    _config->loadConfig();
    
    // Use WiFiManager with module ID as AP name
    if (!_config->beginWiFiManager(_moduleId)) {
        return false;
    }
    
    // Get broker from config
    if (strlen(_config->getBroker()) > 0) {
        setBroker(_config->getBroker(), _config->getPort());
    }
    
    // Setup MQTT callbacks and connect
    _mqtt->onConnect([this](bool connected) {
        if (connected) {
            setupSubscriptions();
        }
        if (_onConnect) {
            _onConnect(connected);
        }
    });
    
    _mqtt->onMessage([this](const char* topic, const char* payload) {
        handleMqttMessage(topic, payload);
    });

#ifndef NATIVE_BUILD
    // Start OTA
    ArduinoOTA.setHostname(_moduleId);
    ArduinoOTA.begin();
#endif
    
    return _mqtt->connect();
}

bool IotMesurable::begin(const char* ssid, const char* password) {
    _config->loadConfig();
    
    if (!_config->beginWiFi(ssid, password)) {
        return false;
    }
    
    // Get broker from config or use default
    if (strlen(_broker) == 0 && strlen(_config->getBroker()) > 0) {
        setBroker(_config->getBroker(), _config->getPort());
    }
    
    _mqtt->onConnect([this](bool connected) {
        if (connected) {
            setupSubscriptions();
        }
        if (_onConnect) {
            _onConnect(connected);
        }
    });
    
    _mqtt->onMessage([this](const char* topic, const char* payload) {
        handleMqttMessage(topic, payload);
    });
    
#ifndef NATIVE_BUILD
    // Start OTA
    ArduinoOTA.setHostname(_moduleId);
    ArduinoOTA.begin();
#endif

    return _mqtt->connect();
}

bool IotMesurable::begin(const char* ssid, const char* password,
                          const char* broker, uint16_t port) {
    setBroker(broker, port);
    return begin(ssid, password);
}

// =============================================================================
// Configuration
// =============================================================================

void IotMesurable::setBroker(const char* host, uint16_t port) {
    strncpy(_broker, host, sizeof(_broker) - 1);
    _broker[sizeof(_broker) - 1] = '\0';
    _port = port;
    
    _mqtt->setBroker(host, port);
    _config->setBroker(host, port);
}

void IotMesurable::setModuleType(const char* type) {
    if (!type) return;
    strncpy(_moduleType, type, sizeof(_moduleType) - 1);
    _moduleType[sizeof(_moduleType) - 1] = '\0';
}

// =============================================================================
// Sensor Registration
// =============================================================================

void IotMesurable::registerHardware(const char* key, const char* name) {
    _registry->registerHardware(key, name);
    
    // Load persisted enabled state
    bool enabled = _config->loadHardwareEnabled(key, true);
    _registry->setHardwareEnabled(key, enabled);
    
    // Load persisted interval
    int interval = _config->loadInterval(key, 60000);
    _registry->setHardwareInterval(key, interval);
}

void IotMesurable::addSensor(const char* hardwareKey, const char* sensorType) {
    _registry->addSensor(hardwareKey, sensorType);
}

// =============================================================================
// Publishing
// =============================================================================

void IotMesurable::publish(const char* hardwareKey, const char* sensorType, float value) {
    // Skip if hardware is disabled
    if (!_registry->isHardwareEnabled(hardwareKey)) {
        return;
    }
    
    // Update registry
    _registry->updateSensorValue(hardwareKey, sensorType, value);
    
    // Build topic: moduleId/hardware/sensor
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/%s/%s", _moduleId, hardwareKey, sensorType);
    
    // Build payload
    char payload[32];
    snprintf(payload, sizeof(payload), "%.2f", value);
    
    _mqtt->publish(topic, payload, false);
}

void IotMesurable::publish(const char* hardwareKey, const char* sensorType, int value) {
    publish(hardwareKey, sensorType, static_cast<float>(value));
}

// =============================================================================
// Main Loop
// =============================================================================

void IotMesurable::loop() {
    _mqtt->loop();
    
#ifndef NATIVE_BUILD
    ArduinoOTA.handle();
#endif

    // Publish status periodically
    unsigned long now = millis();
    if (now - _lastStatusPublish >= STATUS_INTERVAL) {
        _lastStatusPublish = now;
        publishStatus();
    }
    
    // Publish system info less frequently
    if (now - _lastSystemPublish >= SYSTEM_INTERVAL) {
        _lastSystemPublish = now;
        publishSystemInfo();
        publishHardwareInfo();
    }
    
    // Publish sensors config periodically (for storage projections)
    if (now - _lastConfigPublish >= CONFIG_INTERVAL) {
        _lastConfigPublish = now;
        publishConfig();
    }
}

// =============================================================================
// Callbacks
// =============================================================================

void IotMesurable::onConfigChange(ConfigCallback callback) {
    _onConfigChange = callback;
}

void IotMesurable::onEnableChange(EnableCallback callback) {
    _onEnableChange = callback;
}

void IotMesurable::onConnect(ConnectCallback callback) {
    _onConnect = callback;
}

// =============================================================================
// State
// =============================================================================

bool IotMesurable::isConnected() const {
    return _mqtt->isConnected();
}

bool IotMesurable::isHardwareEnabled(const char* hardwareKey) const {
    return _registry->isHardwareEnabled(hardwareKey);
}

const char* IotMesurable::getModuleId() const {
    return _moduleId;
}

// =============================================================================
// Private Methods
// =============================================================================

void IotMesurable::publishStatus() {
    if (!isConnected()) return;
    
    // Build sensors status JSON
    char sensorsBuffer[1024];
    _registry->buildStatusJson(sensorsBuffer, sizeof(sensorsBuffer));
    
    // Build complete status with metadata
    char fullBuffer[1536];
    snprintf(fullBuffer, sizeof(fullBuffer), 
        "{\"moduleId\":\"%s\",\"moduleType\":\"%s\",\"sensors\":%s}",
        _moduleId, _moduleType, sensorsBuffer);
    
    // Publish to moduleId/sensors/status
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/sensors/status", _moduleId);
    
    _mqtt->publish(topic, fullBuffer, true);
}

void IotMesurable::publishConfig() {
    if (!isConnected()) return;
    
    // Build sensors config JSON
    char configBuffer[1024];
    _registry->buildConfigJson(configBuffer, sizeof(configBuffer));
    
    // Publish to moduleId/sensors/config
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/sensors/config", _moduleId);
    
    _mqtt->publish(topic, configBuffer, true);
}

void IotMesurable::publishSystemInfo() {
#ifndef NATIVE_BUILD
    if (!isConnected()) return;
    
    // Get system info from ESP32
    char ip[16] = "0.0.0.0";
    char mac[18] = "00:00:00:00:00:00";
    unsigned long uptimeSeconds = millis() / 1000;
    uint32_t heapFree = ESP.getFreeHeap() / 1024;
    uint32_t heapTotal = ESP.getHeapSize() / 1024;
    uint32_t heapMinFree = ESP.getMinFreeHeap() / 1024;
    
    // Get WiFi info
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress localIP = WiFi.localIP();
        snprintf(ip, sizeof(ip), "%d.%d.%d.%d", 
            localIP[0], localIP[1], localIP[2], localIP[3]);
        
        uint8_t macAddr[6];
        WiFi.macAddress(macAddr);
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
            macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    }
    
    // Get RSSI
    int rssi = WiFi.RSSI();
    
    // Build JSON
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
        "{\"ip\":\"%s\",\"mac\":\"%s\",\"moduleType\":\"%s\",\"uptimeStart\":%lu,"
        "\"memory\":{\"heapTotalKb\":%lu,\"heapFreeKb\":%lu,\"heapMinFreeKb\":%lu},"
        "\"rssi\":%d}",
        ip, mac, _moduleType, uptimeSeconds,
        heapTotal, heapFree, heapMinFree, rssi);
    
    // Publish to moduleId/system/config
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/system/config", _moduleId);
    _mqtt->publish(topic, buffer, true);
#endif
}

void IotMesurable::publishHardwareInfo() {
#ifndef NATIVE_BUILD
    if (!isConnected()) return;
    
    // Get chip info
    esp_chip_info_t chipInfo;
    esp_chip_info(&chipInfo);
    
    const char* chipModel = "ESP32";
    if (chipInfo.model == CHIP_ESP32) chipModel = "ESP32";
    else if (chipInfo.model == CHIP_ESP32S2) chipModel = "ESP32-S2";
    else if (chipInfo.model == CHIP_ESP32S3) chipModel = "ESP32-S3";
    else if (chipInfo.model == CHIP_ESP32C3) chipModel = "ESP32-C3";
    
    int cpuFreq = ESP.getCpuFreqMHz();
    int flashKb = ESP.getFlashChipSize() / 1024;
    int cores = chipInfo.cores;
    int rev = chipInfo.revision;
    
    // Build JSON
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
        "{\"chip\":{\"model\":\"%s\",\"rev\":%d,\"cpuFreqMhz\":%d,\"flashKb\":%d,\"cores\":%d}}",
        chipModel, rev, cpuFreq, flashKb, cores);
    
    // Publish to moduleId/hardware/config
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/hardware/config", _moduleId);
    _mqtt->publish(topic, buffer, true);
#endif
}

void IotMesurable::handleMqttMessage(const char* topic, const char* payload) {
#ifndef NATIVE_BUILD
    // Parse topic to extract message type
    // Expected formats:
    //   moduleId/sensors/config  - configuration update
    //   moduleId/sensors/enable  - enable/disable hardware
    
    char expectedConfigTopic[128];
    char expectedEnableTopic[128];
    snprintf(expectedConfigTopic, sizeof(expectedConfigTopic), "%s/sensors/config", _moduleId);
    snprintf(expectedEnableTopic, sizeof(expectedEnableTopic), "%s/sensors/enable", _moduleId);
    
    if (strcmp(topic, expectedConfigTopic) == 0) {
        // Parse config message
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) return;
        
        // Look for interval updates per hardware
        JsonObject sensors = doc["sensors"];
        if (sensors) {
            for (const auto& hw : _registry->getAllHardware()) {
                if (sensors.containsKey(hw.key)) {
                    JsonObject hwConfig = sensors[hw.key];
                    if (hwConfig.containsKey("interval")) {
                        int interval = hwConfig["interval"];
                        _registry->setHardwareInterval(hw.key, interval * 1000);
                        _config->saveInterval(hw.key, interval * 1000);
                        
                        if (_onConfigChange) {
                            _onConfigChange(hw.key, interval * 1000);
                        }
                    }
                }
            }
        }
    }
    else if (strcmp(topic, expectedEnableTopic) == 0) {
        // Parse enable message: { "hardware": "dht22", "enabled": true }
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) return;
        
        const char* hardware = doc["hardware"];
        bool enabled = doc["enabled"];
        
        if (hardware) {
            _registry->setHardwareEnabled(hardware, enabled);
            _config->saveHardwareEnabled(hardware, enabled);
            
            if (_onEnableChange) {
                _onEnableChange(hardware, enabled);
            }
        }
    }
#endif
}

void IotMesurable::setupSubscriptions() {
    char topic[128];
    
    // Subscribe to config topic
    snprintf(topic, sizeof(topic), "%s/sensors/config", _moduleId);
    _mqtt->subscribe(topic);
    
    // Subscribe to enable topic
    snprintf(topic, sizeof(topic), "%s/sensors/enable", _moduleId);
    _mqtt->subscribe(topic);
}

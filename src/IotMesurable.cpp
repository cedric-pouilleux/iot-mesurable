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
#endif

// =============================================================================
// Constructor / Destructor
// =============================================================================

IotMesurable::IotMesurable(const char* moduleId) 
    : _port(1883), _lastStatusPublish(0) {
    
    strncpy(_moduleId, moduleId, sizeof(_moduleId) - 1);
    _moduleId[sizeof(_moduleId) - 1] = '\0';
    
    memset(_broker, 0, sizeof(_broker));
    
    _registry = new SensorRegistry();
    _mqtt = new MqttClient();
    _config = new ConfigManager();
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
    
    return _mqtt->connect();
}

bool IotMesurable::begin(const char* ssid, const char* password) {
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
    
    // Publish status periodically
    unsigned long now = millis();
    if (now - _lastStatusPublish >= STATUS_INTERVAL) {
        _lastStatusPublish = now;
        publishStatus();
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
    
    // Build status JSON
    char buffer[1024];
    _registry->buildStatusJson(buffer, sizeof(buffer));
    
    // Publish to moduleId/sensors/status
    char topic[128];
    snprintf(topic, sizeof(topic), "%s/sensors/status", _moduleId);
    
    _mqtt->publish(topic, buffer, true);
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

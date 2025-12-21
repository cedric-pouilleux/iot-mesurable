/**
 * @file MqttClient.cpp
 * @brief MQTT client wrapper implementation
 */

#include "MqttClient.h"
#include <cstring>

MqttClient::MqttClient() : _port(1883), _connected(false), _lastReconnectAttempt(0) {
    memset(_host, 0, sizeof(_host));
    memset(_clientId, 0, sizeof(_clientId));
#ifndef NATIVE_BUILD
    setupCallbacks();
#endif
}

MqttClient::~MqttClient() {
    disconnect();
}

void MqttClient::setBroker(const char* host, uint16_t port) {
    strncpy(_host, host, sizeof(_host) - 1);
    _host[sizeof(_host) - 1] = '\0';
    _port = port;
    
#ifndef NATIVE_BUILD
    _client.setServer(_host, _port);
#endif
}

void MqttClient::setClientId(const char* clientId) {
    strncpy(_clientId, clientId, sizeof(_clientId) - 1);
    _clientId[sizeof(_clientId) - 1] = '\0';
    
#ifndef NATIVE_BUILD
    _client.setClientId(_clientId);
#endif
}

bool MqttClient::connect() {
    if (strlen(_host) == 0) return false;
    
#ifndef NATIVE_BUILD
    if (!_client.connected()) {
        _client.connect();
    }
#endif
    return true;
}

void MqttClient::disconnect() {
#ifndef NATIVE_BUILD
    _client.disconnect();
#endif
    _connected = false;
}

bool MqttClient::isConnected() const {
#ifndef NATIVE_BUILD
    return _client.connected();
#else
    return _connected;
#endif
}

void MqttClient::subscribe(const char* topic) {
#ifndef NATIVE_BUILD
    if (_client.connected()) {
        _client.subscribe(topic, 0);
    }
#endif
}

void MqttClient::publish(const char* topic, const char* payload, bool retain) {
#ifndef NATIVE_BUILD
    if (_client.connected()) {
        _client.publish(topic, 0, retain, payload);
    }
#endif
}

void MqttClient::onMessage(MqttMessageCallback callback) {
    _onMessage = callback;
}

void MqttClient::onConnect(MqttConnectCallback callback) {
    _onConnect = callback;
}

void MqttClient::loop() {
#ifndef NATIVE_BUILD
    // Auto-reconnect logic
    if (!_client.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > RECONNECT_INTERVAL) {
            _lastReconnectAttempt = now;
            connect();
        }
    }
#endif
}

#ifndef NATIVE_BUILD
void MqttClient::setupCallbacks() {
    _client.onConnect([this](bool sessionPresent) {
        _connected = true;
        if (_onConnect) {
            _onConnect(true);
        }
    });
    
    _client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
        _connected = false;
        Serial.printf("[MQTT] Disconnected (reason: %d)\n", (int)reason);
        if (_onConnect) {
            _onConnect(false);
        }
    });
    
    _client.onMessage([this](char* topic, char* payload, 
                              AsyncMqttClientMessageProperties properties,
                              size_t len, size_t index, size_t total) {
        if (_onMessage && payload) {
            // Create null-terminated copy
            char buffer[512];
            size_t copyLen = len < sizeof(buffer) - 1 ? len : sizeof(buffer) - 1;
            memcpy(buffer, payload, copyLen);
            buffer[copyLen] = '\0';
            
            _onMessage(topic, buffer);
        }
    });
}
#else
void MqttClient::setupCallbacks() {
    // Native build - no-op
}
#endif

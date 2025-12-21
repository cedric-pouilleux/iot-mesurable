/**
 * @file ConfigManager.h
 * @brief WiFi and configuration management
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

#ifndef NATIVE_BUILD
#include <Preferences.h>
#endif

/**
 * @brief Manages WiFi connection and persistent configuration
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    /**
     * @brief Initialize WiFi with WiFiManager (captive portal)
     * @param apName Access point name for portal
     * @return true if connected
     */
    bool beginWiFiManager(const char* apName);
    
    /**
     * @brief Initialize WiFi with direct credentials
     * @param ssid Network name
     * @param password Network password
     * @param timeoutMs Connection timeout in ms
     * @return true if connected
     */
    bool beginWiFi(const char* ssid, const char* password, 
                   unsigned long timeoutMs = 30000);
    
    /**
     * @brief Check WiFi connection
     */
    bool isWiFiConnected() const;
    
    /**
     * @brief Get stored MQTT broker
     */
    const char* getBroker() const { return _broker; }
    
    /**
     * @brief Get stored MQTT port
     */
    uint16_t getPort() const { return _port; }
    
    /**
     * @brief Set and persist broker config
     */
    void setBroker(const char* host, uint16_t port);
    
    /**
     * @brief Save hardware enabled state
     */
    void saveHardwareEnabled(const char* hardwareKey, bool enabled);
    
    /**
     * @brief Load hardware enabled state
     */
    bool loadHardwareEnabled(const char* hardwareKey, bool defaultValue = true);
    
    /**
     * @brief Save hardware interval
     */
    void saveInterval(const char* hardwareKey, int intervalMs);
    
    /**
     * @brief Load hardware interval
     */
    int loadInterval(const char* hardwareKey, int defaultValue = 60000);

private:
#ifndef NATIVE_BUILD
    Preferences _prefs;
#endif
    char _broker[128];
    uint16_t _port;
    
    void loadConfig();
    void saveConfig();
};

#endif // CONFIG_MANAGER_H

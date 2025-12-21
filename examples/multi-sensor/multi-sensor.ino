/**
 * @file multi-sensor.ino
 * @brief Multi-sensor example with callbacks
 * 
 * This example shows how to integrate multiple sensors
 * and use callbacks for configuration changes.
 */

#include <IotMesurable.h>

IotMesurable brain("greenhouse-01");

// Simulated sensor pins
const int SOIL_PIN = 32;
const int LIGHT_PIN = 34;
const int WATER_FLOW_PIN = 35;

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== iot-mesurable Multi-Sensor Example ===\n");
    
    // Initialize with direct WiFi credentials
    if (!brain.begin("MyWiFi", "MyPassword", "192.168.1.100", 1883)) {
        Serial.println("Connection failed!");
        while (true) delay(1000);
    }
    
    // Register multiple hardware
    brain.registerHardware("soil-moisture", "Capacitive Soil Sensor");
    brain.addSensor("soil-moisture", "moisture");
    
    brain.registerHardware("ldr", "Light Sensor");
    brain.addSensor("ldr", "light");
    
    brain.registerHardware("water-flow", "Flow Meter");
    brain.addSensor("water-flow", "flow_rate");
    brain.addSensor("water-flow", "total_volume");
    
    // Set up callbacks
    brain.onConfigChange([](const char* hardware, int intervalMs) {
        Serial.printf("Config changed: %s = %d ms\n", hardware, intervalMs);
    });
    
    brain.onEnableChange([](const char* hardware, bool enabled) {
        Serial.printf("Hardware %s: %s\n", hardware, enabled ? "ENABLED" : "DISABLED");
    });
    
    brain.onConnect([](bool connected) {
        Serial.printf("MQTT %s\n", connected ? "Connected" : "Disconnected");
    });
    
    Serial.println("Setup complete!");
}

// State for water flow calculation
volatile unsigned long pulseCount = 0;
float totalLiters = 0;

void loop() {
    // Read soil moisture
    int soilRaw = analogRead(SOIL_PIN);
    float moisture = map(soilRaw, 0, 4095, 0, 100);
    brain.publish("soil-moisture", "moisture", moisture);
    
    // Read light level
    int lightRaw = analogRead(LIGHT_PIN);
    float light = map(lightRaw, 0, 4095, 0, 100);
    brain.publish("ldr", "light", light);
    
    // Calculate water flow (simplified)
    float flowRate = pulseCount * 2.25; // mL per pulse
    pulseCount = 0;
    totalLiters += flowRate / 1000.0;
    
    brain.publish("water-flow", "flow_rate", flowRate);
    brain.publish("water-flow", "total_volume", totalLiters);
    
    // Must call loop()
    brain.loop();
    
    delay(5000);
}

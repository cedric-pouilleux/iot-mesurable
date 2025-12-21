/**
 * @file basic.ino
 * @brief Basic example - minimal sensor integration
 * 
 * This example shows the minimal code needed to integrate
 * a sensor with the IoT Grow Brain ecosystem.
 */

#include <IotMesurable.h>
#include <DHT.h>

// Create brain instance with your module ID
IotMesurable brain("my-growbox");

// Your sensor
DHT dht(4, DHT22);

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== iot-mesurable Basic Example ===\n");
    
    // Initialize (will start WiFiManager portal if not configured)
    if (!brain.begin()) {
        Serial.println("Failed to initialize!");
        while (true) delay(1000);
    }
    
    Serial.println("Connected!");
    
    // Register your hardware and sensors
    brain.registerHardware("dht22", "DHT22 Sensor");
    brain.addSensor("dht22", "temperature");
    brain.addSensor("dht22", "humidity");
    
    // Initialize your sensor
    dht.begin();
}

void loop() {
    // Read your sensor
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    
    // Publish values (library handles MQTT formatting, intervals, etc.)
    if (!isnan(temperature)) {
        brain.publish("dht22", "temperature", temperature);
    }
    if (!isnan(humidity)) {
        brain.publish("dht22", "humidity", humidity);
    }
    
    // Let the library handle MQTT, status, etc.
    brain.loop();
    
    delay(5000);
}

# iot-mesurable

ESP32 library for easy sensor integration with the IoT Grow Brain ecosystem.

Register your own sensors and publish data with minimal code. The library handles WiFi, MQTT, configuration, and enable/disable features automatically.

## Features

- 🔌 **WiFiManager** - Captive portal for easy WiFi setup
- 📡 **Auto MQTT** - Connection, reconnection, and message handling
- 📊 **Status Publishing** - Automatic sensor status updates
- ⏸️ **Enable/Disable** - Pause sensors from the dashboard
- ⚙️ **Config Sync** - Receive interval updates from backend
- 💾 **Persistence** - Settings survive reboots

## Installation

### PlatformIO (recommended)

Add to your `platformio.ini`:

```ini
lib_deps = 
    cedric-pouilleux/iot-mesurable@^1.0.0
```

### Manual

Download and place in your `lib/` folder.

## Quick Start

```cpp
#include <IotMesurable.h>
#include <DHT.h>

IotMesurable brain("my-module");
DHT dht(4, DHT22);

void setup() {
    brain.begin();  // WiFiManager portal if not configured
    
    brain.registerHardware("dht22", "DHT22");
    brain.addSensor("dht22", "temperature");
    brain.addSensor("dht22", "humidity");
    
    dht.begin();
}

void loop() {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    
    brain.publish("dht22", "temperature", temp);
    brain.publish("dht22", "humidity", hum);
    
    brain.loop();
    delay(5000);
}
```

## API Reference

### Initialization

```cpp
IotMesurable brain("module-id");

// Option 1: WiFiManager (captive portal)
brain.begin();

// Option 2: Direct credentials
brain.begin("ssid", "password");

// Option 3: Full config
brain.begin("ssid", "password", "mqtt-broker", 1883);
```

### Register Sensors

```cpp
brain.registerHardware("key", "Display Name");
brain.addSensor("hardware-key", "sensor-type");
```

### Publish Values

```cpp
brain.publish("hardware", "sensor", 23.5);  // float
brain.publish("hardware", "sensor", 100);   // int
```

### Callbacks

```cpp
brain.onConfigChange([](const char* hw, int intervalMs) {
    Serial.printf("New interval for %s: %d ms\n", hw, intervalMs);
});

brain.onEnableChange([](const char* hw, bool enabled) {
    Serial.printf("%s is now %s\n", hw, enabled ? "ON" : "OFF");
});

brain.onConnect([](bool connected) {
    Serial.println(connected ? "MQTT Connected" : "MQTT Disconnected");
});
```

### State

```cpp
brain.isConnected();                    // MQTT connection
brain.isHardwareEnabled("hardware");    // Check if paused
brain.getModuleId();                    // Get module ID
```

## MQTT Topics

### Published by Library

| Topic | Description |
|-------|-------------|
| `{moduleId}/{hw}/{sensor}` | Sensor values |
| `{moduleId}/sensors/status` | JSON status (retained) |

### Subscribed by Library

| Topic | Description |
|-------|-------------|
| `{moduleId}/sensors/config` | Interval configuration |
| `{moduleId}/sensors/enable` | Enable/disable hardware |

## Examples

See the `examples/` folder:
- `basic/` - Minimal setup
- `multi-sensor/` - Multiple sensors with callbacks

## Testing

Run unit tests:

```bash
pio test -e native
```

## Publishing

1. Create account on [PlatformIO Registry](https://registry.platformio.org)
2. Generate access token
3. Login: `pio account login`
4. Pack: `pio package pack`
5. Publish: `pio package publish`

## License

MIT License - See [LICENSE](LICENSE)

## Author

Cédric Pouilleux

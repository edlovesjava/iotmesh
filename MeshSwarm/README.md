# MeshSwarm

A self-organizing ESP32 mesh network library with distributed shared state synchronization.

## Features

- **Self-organizing mesh**: Nodes automatically discover and connect using painlessMesh
- **Coordinator election**: Lowest node ID automatically becomes coordinator
- **Self-healing**: Network recovers from node failures and re-elects coordinators
- **Distributed shared state**: Key-value store synchronized across all nodes
- **State watchers**: Register callbacks to react to state changes
- **Conflict resolution**: Version numbers + origin ID ensure deterministic convergence
- **OTA updates**: Receive firmware updates via mesh from gateway
- **Telemetry**: Send metrics to server via gateway node
- **OLED display**: Built-in support for SSD1306 128x64 displays
- **Serial console**: Built-in command interface for debugging and control

## Installation

### Arduino IDE

Copy this `MeshSwarm` folder to your Arduino libraries directory:

```bash
# macOS
cp -r MeshSwarm ~/Documents/Arduino/libraries/

# Linux
cp -r MeshSwarm ~/Arduino/libraries/

# Windows
# Copy to: Documents\Arduino\libraries\
```

Restart Arduino IDE after copying.

### PlatformIO

Add to your `platformio.ini`:

```ini
lib_deps =
    painlessMesh
    ArduinoJson
    Adafruit SSD1306
    Adafruit GFX Library
```

Then copy `MeshSwarm/` to your project's `lib/` directory.

## Dependencies

Install via Arduino Library Manager:

- painlessMesh
- ArduinoJson
- Adafruit SSD1306
- Adafruit GFX Library

## Quick Start

```cpp
#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin();

  // React to state changes from other nodes
  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    digitalWrite(2, value == "1" ? HIGH : LOW);
  });
}

void loop() {
  swarm.update();
}
```

## Hardware

### Required
- ESP32 (dual-core recommended)
- SSD1306 OLED 128x64 (I2C)

### Default Pin Configuration

| Component | ESP32 Pin |
|-----------|-----------|
| OLED SDA  | GPIO 21   |
| OLED SCL  | GPIO 22   |

Pins can be overridden by defining before including:

```cpp
#define I2C_SDA 21
#define I2C_SCL 22
#include <MeshSwarm.h>
```

## API Reference

### Initialization

```cpp
MeshSwarm swarm;

// Default network settings
swarm.begin();

// Custom network settings
swarm.begin("mynetwork", "password123", 5555);

// With custom node name
swarm.begin("mynetwork", "password123", 5555, "sensor1");
```

### Main Loop

```cpp
void loop() {
  swarm.update();  // Must be called frequently
}
```

### State Management

```cpp
// Set state (broadcasts to all nodes)
swarm.setState("temperature", "23.5");

// Get state
String temp = swarm.getState("temperature");
String humid = swarm.getState("humidity", "0");  // With default

// Watch for changes
swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("%s changed: %s -> %s\n", key.c_str(), oldValue.c_str(), value.c_str());
});

// Watch all changes (wildcard)
swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
  // Called for any state change
});

// Manual sync
swarm.broadcastFullState();  // Send all state to peers
swarm.requestStateSync();    // Request state from peers
```

### Node Information

```cpp
uint32_t id = swarm.getNodeId();      // Unique mesh ID
String name = swarm.getNodeName();    // Human-readable name
String role = swarm.getRole();        // "COORD" or "NODE"
bool coord = swarm.isCoordinator();   // Am I coordinator?
int peers = swarm.getPeerCount();     // Connected peer count
```

### Customization Hooks

```cpp
// Add custom logic to each loop iteration
swarm.onLoop([]() {
  // Poll sensors, update state, etc.
});

// Add custom serial commands
swarm.onSerialCommand([](const String& input) -> bool {
  if (input == "mycommand") {
    Serial.println("Handled!");
    return true;  // Command was handled
  }
  return false;  // Not our command
});

// Add custom display section
swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
  display.println("---------------------");
  display.println("Custom data here");
});

// Set custom status line
swarm.setStatusLine("Sensor OK");

// Add data to heartbeat messages
swarm.setHeartbeatData("battery", 85);
```

### Advanced Access

```cpp
// Direct access to painlessMesh
painlessMesh& mesh = swarm.getMesh();

// Direct access to display
Adafruit_SSD1306& display = swarm.getDisplay();

// Access peer map
std::map<uint32_t, Peer>& peers = swarm.getPeers();
```

## Configuration

Override defaults before including the library:

```cpp
// Network settings
#define MESH_PREFIX     "myswarm"
#define MESH_PASSWORD   "secret123"
#define MESH_PORT       5555

// Display settings
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C

// I2C pins
#define I2C_SDA         21
#define I2C_SCL         22

// Timing (milliseconds)
#define HEARTBEAT_INTERVAL   5000
#define STATE_SYNC_INTERVAL  10000
#define DISPLAY_INTERVAL     500

#include <MeshSwarm.h>
```

## Built-in Serial Commands

| Command | Description |
|---------|-------------|
| `status` | Show node info (ID, role, peers, heap) |
| `peers` | List all known peers |
| `state` | Show all shared state entries |
| `set <key> <value>` | Set a shared state value |
| `get <key>` | Get a shared state value |
| `sync` | Broadcast full state to all nodes |
| `reboot` | Restart the node |

## State Conflict Resolution

When multiple nodes update the same key simultaneously:

1. Higher version number wins
2. If versions match, lower origin node ID wins

This ensures all nodes converge to the same state without a central authority.

## OTA Updates

### Receiving OTA Updates (Nodes)

```cpp
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

void setup() {
  // Mark current firmware as valid (enables auto-rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin("PIR");
  swarm.enableOTAReceive("pir");  // Register to receive OTA for this role
}
```

### Distributing OTA Updates (Gateway)

```cpp
void setup() {
  esp_ota_mark_app_valid_cancel_rollback();
  swarm.begin("Gateway");
  swarm.connectToWiFi(ssid, password);
  swarm.setGatewayMode(true);
  swarm.enableOTADistribution(true);
}

void loop() {
  swarm.update();
  swarm.checkForOTAUpdates();  // Poll server every 60 seconds
}
```

### OTA Methods

| Method | Description |
|--------|-------------|
| `enableOTAReceive(role)` | Enable node to receive OTA for given role |
| `enableOTADistribution(bool)` | Enable gateway OTA distribution |
| `checkForOTAUpdates()` | Poll server for pending updates (gateway only) |

## Telemetry

### Sending Telemetry via Gateway

```cpp
void setup() {
  swarm.begin("Sensor");
  swarm.enableTelemetry(true);  // Sends via mesh to gateway
}
```

### Gateway Mode

```cpp
void setup() {
  swarm.begin("Gateway");
  swarm.connectToWiFi(ssid, password);
  swarm.setGatewayMode(true);
  swarm.setTelemetryServer("http://server:8000");
  swarm.enableTelemetry(true);
}
```

### Telemetry Methods

| Method | Description |
|--------|-------------|
| `enableTelemetry(bool)` | Enable/disable telemetry |
| `setTelemetryServer(url)` | Set server URL (gateway only) |
| `setTelemetryInterval(ms)` | Set push interval (default 30s) |
| `setGatewayMode(bool)` | Enable gateway mode |
| `connectToWiFi(ssid, pass)` | Connect to WiFi (gateway only) |

## Examples

This library includes the following example sketches (File → Examples → MeshSwarm):

| Example | Description |
|---------|-------------|
| **BasicNode** | Minimal mesh node with OLED display |
| **ButtonNode** | Button input that publishes to mesh |
| **LedNode** | LED controlled by mesh state |
| **SensorNode** | DHT11 temperature/humidity sensor |
| **WatcherNode** | Observer that logs all state changes |
| **GatewayNode** | WiFi bridge with telemetry and OTA |

## Related Projects

- [iotmesh](https://github.com/edlovesjava/iotmesh) - Complete IoT mesh application with server, dashboard, and OTA system

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

MIT License - see [LICENSE](LICENSE) file for details.

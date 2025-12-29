# MeshSwarm API Reference

Complete API reference for the MeshSwarm library.

## Core Methods

| Method | Description |
|--------|-------------|
| `begin()` | Initialize mesh with default settings |
| `begin(prefix, password, port)` | Initialize with custom network settings |
| `begin(prefix, password, port, nodeName)` | Initialize with custom name |
| `update()` | Process mesh events (call in `loop()`) |

### Example

```cpp
#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  // Default settings
  swarm.begin();

  // Or with custom network
  swarm.begin("mynet", "password123", 5555);

  // Or with custom name
  swarm.begin("mynet", "password123", 5555, "SensorNode");
}

void loop() {
  swarm.update();
}
```

## State Management

| Method | Description |
|--------|-------------|
| `setState(key, value)` | Set a shared state value, broadcasts to mesh |
| `setStates({...})` | Batch update multiple states |
| `getState(key)` | Get current value for a key |
| `getState(key, default)` | Get value with default if not found |
| `watchState(key, callback)` | Register callback for state changes |
| `watchState("*", callback)` | Watch all state changes (wildcard) |
| `broadcastFullState()` | Send full state to all peers |
| `requestStateSync()` | Request state from peers |

### Examples

```cpp
// Set single state
swarm.setState("temp", "72.5");

// Batch update
swarm.setStates({
  {"temp", "72.5"},
  {"humidity", "45"}
});

// Get state
String temp = swarm.getState("temp");
String mode = swarm.getState("mode", "auto");  // with default

// Watch specific key
swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("LED changed: %s -> %s\n", oldValue.c_str(), value.c_str());
  digitalWrite(LED_PIN, value == "1" ? HIGH : LOW);
});

// Watch all changes
swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("State: %s = %s\n", key.c_str(), value.c_str());
});
```

## Node Information

| Method | Description |
|--------|-------------|
| `getNodeId()` | Get this node's unique mesh ID (uint32_t) |
| `getNodeName()` | Get human-readable node name |
| `getRole()` | Get role ("COORD" or "NODE") |
| `isCoordinator()` | Check if this node is coordinator |
| `getPeerCount()` | Get number of connected peers |
| `getPeers()` | Get map of all known peers |

### Example

```cpp
Serial.printf("Node ID: %u\n", swarm.getNodeId());
Serial.printf("Name: %s\n", swarm.getNodeName().c_str());
Serial.printf("Role: %s\n", swarm.getRole().c_str());
Serial.printf("Peers: %d\n", swarm.getPeerCount());

if (swarm.isCoordinator()) {
  Serial.println("I am the coordinator!");
}
```

## Customization Hooks

| Method | Description |
|--------|-------------|
| `onLoop(callback)` | Register function to call each loop iteration |
| `onSerialCommand(handler)` | Add custom serial command handler |
| `onDisplayUpdate(handler)` | Add custom OLED display section |
| `setStatusLine(text)` | Set custom status line on display |
| `setHeartbeatData(key, value)` | Add custom data to heartbeat messages |

### Examples

```cpp
// Custom loop logic
swarm.onLoop([]() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 5000) {
    lastRead = millis();
    float temp = readTemperature();
    swarm.setState("temp", String(temp));
  }
});

// Custom serial command
swarm.onSerialCommand([](const String& input) -> bool {
  if (input == "mycommand") {
    Serial.println("My command executed!");
    return true;  // Command handled
  }
  return false;  // Not our command
});

// Custom display section
swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
  display.println("---------------------");
  display.println("My custom data:");
  display.printf("Value: %d\n", myValue);
});

// Custom status line
swarm.setStatusLine("Temp: 72.5F");

// Custom heartbeat data
swarm.setHeartbeatData("battery", 85);
```

## Remote Command Protocol

| Method | Description |
|--------|-------------|
| `sendCommand(target, command)` | Send command (fire and forget) |
| `sendCommand(target, command, args)` | Send command with arguments |
| `sendCommand(target, command, args, callback, timeout)` | Send with response callback |
| `onCommand(command, handler)` | Register custom command handler |

### Sending Commands

```cpp
// Fire and forget
swarm.sendCommand("Node1", "reboot");

// With arguments
JsonDocument doc;
doc["key"] = "temp";
JsonObject args = doc.as<JsonObject>();
swarm.sendCommand("Node1", "get", args);

// With callback
swarm.sendCommand("Node1", "status", args, [](bool success, const String& node, JsonObject& result) {
  if (success) {
    Serial.printf("Response from %s: heap=%d\n", node.c_str(), result["heap"].as<int>());
  } else {
    Serial.println("Command failed or timed out");
  }
}, 5000);  // 5 second timeout

// Broadcast to all nodes
swarm.sendCommand("*", "ping", args, callback);
```

### Registering Command Handlers

```cpp
swarm.onCommand("sensor", [](const String& sender, JsonObject& args) {
  JsonDocument response;
  response["temperature"] = readTemperature();
  response["humidity"] = readHumidity();
  return response;
});
```

### Built-in Commands

All nodes support these commands:

| Command | Arguments | Description |
|---------|-----------|-------------|
| `status` | - | Node info (ID, role, heap, uptime) |
| `peers` | - | List connected peers |
| `state` | - | Get all shared state |
| `get` | `key` | Get specific state key |
| `set` | `key`, `value` | Set state key/value |
| `sync` | - | Force state broadcast |
| `ping` | - | Connectivity test |
| `info` | - | Node capabilities |
| `reboot` | - | Restart node |

## Telemetry (Gateway Mode)

Requires `MESHSWARM_ENABLE_TELEMETRY=1`

| Method | Description |
|--------|-------------|
| `setTelemetryServer(url, apiKey)` | Configure telemetry endpoint |
| `setTelemetryInterval(ms)` | Set push interval |
| `enableTelemetry(bool)` | Enable/disable telemetry |
| `isTelemetryEnabled()` | Check if enabled |
| `pushTelemetry()` | Force immediate push |
| `connectToWiFi(ssid, password)` | Connect to WiFi |
| `isWiFiConnected()` | Check WiFi status |
| `setGatewayMode(bool)` | Enable gateway mode |
| `isGateway()` | Check if gateway |

## OTA Updates

Requires `MESHSWARM_ENABLE_OTA=1`

| Method | Description |
|--------|-------------|
| `enableOTADistribution(bool)` | Enable OTA distribution (gateway) |
| `isOTADistributionEnabled()` | Check if distribution enabled |
| `checkForOTAUpdates()` | Poll for pending updates |
| `enableOTAReceive(role)` | Enable OTA reception for role |

## HTTP Server (Gateway)

Requires `MESHSWARM_ENABLE_HTTP_SERVER=1`

| Method | Description |
|--------|-------------|
| `startHTTPServer(port)` | Start HTTP API server |
| `stopHTTPServer()` | Stop HTTP server |
| `isHTTPServerRunning()` | Check if running |

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/nodes` | GET | List all mesh nodes |
| `/api/state` | GET | Get mesh state |
| `/api/command` | POST | Send command to node |
| `/api/result` | GET | Poll for command result |

## Advanced Access

| Method | Description |
|--------|-------------|
| `getMesh()` | Access underlying painlessMesh object |
| `getDisplay()` | Access Adafruit_SSD1306 display object |
| `getPeers()` | Access peer map directly |

### Example

```cpp
// Direct painlessMesh access
painlessMesh& mesh = swarm.getMesh();
mesh.sendBroadcast("custom message");

// Direct display access
Adafruit_SSD1306& display = swarm.getDisplay();
display.clearDisplay();
display.println("Custom screen");
display.display();
```

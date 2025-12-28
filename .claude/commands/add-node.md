# Add New MeshSwarm Node

You are an expert in ESP32 development and the MeshSwarm framework. Your task is to help create a new node type for the IoT Mesh network.

## Context

This project uses:
- **MeshSwarm library**: Provides mesh networking, shared state, OLED display, telemetry, and OTA updates
- **PlatformIO**: Build system with environment-based node configuration
- **painlessMesh**: Underlying mesh networking protocol

## Information Gathering

First, gather the following information from the user. Ask questions one at a time if not provided:

1. **Node Type ID** (lowercase, no spaces): e.g., `relay`, `servo`, `rgb`, `soil`
2. **Node Display Name**: e.g., "Relay", "Servo Motor", "RGB LED", "Soil Moisture"
3. **Node Category**: sensor, actuator, display, or infrastructure
4. **Brief Description**: What does this node do?
5. **Hardware Components**: What sensors/actuators/displays does it use?
6. **GPIO Pins**: Which pins are used for what?
7. **Required Libraries**: Any additional PlatformIO libraries needed?
8. **State Keys**: What mesh state keys will this node publish/watch?

## Implementation Steps

Once you have the information, implement the following:

### Step 1: Create Node Directory

Create `firmware/nodes/{node_type}/main.cpp` with the standard structure:

```cpp
/**
 * {Node Display Name} Node
 *
 * {Brief Description}
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - {List hardware components and their connections}
 *
 * State Keys:
 *   - {List state keys this node publishes or watches}
 *
 * Serial Commands:
 *   - {node_type}: Show node-specific status
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "{Node Display Name}"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "{node_type}"
#endif

// ============== HARDWARE PINS ==============
// {Define pins with clear comments}

// ============== CONFIGURATION ==============
// {Define timing, thresholds, zones, etc.}

// ============== GLOBALS ==============
MeshSwarm swarm;

// {Node-specific state variables}

// ============== NODE-SPECIFIC FUNCTIONS ==============
// {Polling function for sensors, or state watchers for actuators}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

  // {Initialize hardware}

  // {Register callbacks with swarm.onLoop(), swarm.watchState(), etc.}

  // {Register serial command}
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "{node_type}") {
      Serial.println("\n--- {NODE DISPLAY NAME} ---");
      // {Print status information}
      Serial.println();
      return true;
    }
    return false;
  });

  // {Optional: Register display update}
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    // {Custom display content}
  });

  // Add node type to heartbeat
  swarm.setHeartbeatData("{node_type}", 1);
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}
```

### Step 2: Add PlatformIO Environment

Add to `firmware/platformio.ini`:

```ini
[env:{node_type}]
build_src_filter = +<{node_type}/>
lib_deps =
    ${env.lib_deps}
    ; {Add any additional libraries here}
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"{node_type}\"
    -DNODE_NAME=\"{Node Display Name}\"
    ; {Add any feature flags}
```

### Step 3: Update Documentation

Update `CLAUDE.md` to include:
1. Add to "Available Environments" table
2. Add to appropriate "Node Types" section (Sensor/Actuator/Display/Infrastructure)
3. Add hardware connections if using non-standard pins
4. Add serial command to "Node-specific commands" list

## Node Type Templates

Use these patterns based on node category:

### Sensor Node Pattern
- Use `swarm.onLoop(pollFunction)` for periodic reading
- Publish state with `swarm.setState(key, value)`
- Include debouncing/change threshold to avoid spam
- Example: PIR, DHT, Light

### Actuator Node Pattern
- Use `swarm.watchState(key, callback)` to react to state changes
- Set initial state on boot
- Provide manual control via serial commands
- Example: LED, Relay, Servo

### Display Node Pattern
- Disable built-in OLED: `#define MESHSWARM_ENABLE_DISPLAY 0`
- Watch multiple state keys for data to display
- Handle button input for navigation
- Example: Clock, Watcher

### Infrastructure Node Pattern
- May need WiFi: `swarm.connectToWiFi(ssid, password)`
- May run HTTP server: `swarm.startHTTPServer(port)`
- Example: Gateway

## Code Quality Checklist

Before completing, verify:
- [ ] All GPIO pins are defined with `#ifndef` guards for override
- [ ] Configuration values have sensible defaults
- [ ] Serial debug output uses consistent `[{NODE_TYPE}]` prefix
- [ ] State keys follow naming convention (lowercase, underscores)
- [ ] OTA receive is enabled with correct node type
- [ ] Telemetry is enabled
- [ ] Custom serial command is registered
- [ ] Display callback is registered (if using OLED)
- [ ] Heartbeat data includes node type indicator

## Example Invocations

**Simple Actuator:**
```
/add-node relay
```
Then answer: GPIO5, active-high relay module, controls outlet

**Sensor with Library:**
```
/add-node soil
```
Then answer: Capacitive soil moisture on GPIO34, publishes soil moisture percentage

## After Creation

1. Build the new node: `pio run -e {node_type}`
2. Flash to device: `pio run -e {node_type} --target upload`
3. Monitor output: `pio device monitor -e {node_type}`
4. Test mesh integration with existing nodes

## User Request

$ARGUMENTS

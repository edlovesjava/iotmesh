# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 mesh networking project using painlessMesh for self-organizing, self-healing networks with distributed shared state. Nodes automatically discover peers, elect coordinators, and synchronize key-value state across the network.

## Repository Structure (Monorepo)

```
iotmesh/
├── firmware/                    # ESP32/Arduino sketches
│   ├── mesh_shared_state/       # Full-featured reference node
│   ├── mesh_shared_state_button/
│   ├── mesh_shared_state_led/
│   ├── mesh_shared_state_watcher/
│   ├── mesh_shared_state_pir/
│   ├── mesh_shared_state_dht11/
│   ├── mesh_swarm_base/
│   ├── pir_nano/               # Arduino Nano PIR module (legacy)
│   └── pir_nano_test/
├── server/                      # Telemetry server
│   ├── api/                     # FastAPI backend
│   └── dashboard/               # React frontend
├── scripts/                     # Build and deploy scripts
├── docs/                        # Documentation
└── README.md
```

**Note**: The MeshSwarm library is maintained in a separate repository:
https://github.com/edlovesjava/MeshSwarm

## Development Environment

- **IDE**: Arduino IDE
- **ESP32 Board**: Select "ESP32 Dev Module" (Tools → Board → ESP32 Arduino)
- **Nano Board**: Select "Arduino Nano" with ATmega328P processor
- **Serial Baud**: 115200 for all sketches
- **Upload Speed**: 921600 (ESP32)

### Required Libraries (Arduino Library Manager)
- painlessMesh
- ArduinoJson
- Adafruit SSD1306
- Adafruit GFX Library
- DHT sensor library (by Adafruit) - for DHT11 node
- MeshSwarm - install from https://github.com/edlovesjava/MeshSwarm

## Architecture

### Two-Tier Hardware Design

**ESP32 Nodes** (mesh_shared_state_* sketches):
- Run painlessMesh network
- I2C master for OLED display (0x3C) and sensor modules
- Publish/subscribe to distributed shared state

**Sensor Modules** (Arduino Nano as I2C slave):
- Dedicated signal processing (e.g., PIR debouncing)
- Communicate with ESP32 via I2C register interface
- Example: pir_nano at address 0x42

### Shared State System

State is stored as key-value pairs with conflict resolution:
```cpp
struct StateEntry {
  String value;
  uint32_t version;    // Increments on change
  uint32_t origin;     // Node ID that set it
  unsigned long timestamp;
};
```

**Conflict Resolution**: Higher version wins; if equal, lower origin node ID wins.

**State Watchers**: Register callbacks to react to state changes:
```cpp
watchState("led", [](const String& key, const String& value, const String& oldValue) {
  digitalWrite(LED_PIN, value == "1" ? HIGH : LOW);
});
```

### Message Protocol

JSON messages over painlessMesh with type field:
- `MSG_HEARTBEAT` (1): Health check with role/uptime
- `MSG_STATE_SET` (2): Single key-value update
- `MSG_STATE_SYNC` (3): Full state dump
- `MSG_STATE_REQ` (4): Request state from peers
- `MSG_COMMAND` (5): Custom commands
- `MSG_TELEMETRY` (6): Node telemetry to gateway

## Sketch Variants

Located in `firmware/`, the mesh_shared_state_* sketches share ~90% code with different hardware configs:
- `mesh_shared_state`: Full features (button + LED)
- `mesh_shared_state_button`: Button input only
- `mesh_shared_state_led`: LED output only
- `mesh_shared_state_watcher`: Observer, no I/O
- `mesh_shared_state_pir`: PIR motion sensor (direct GPIO4)
- `mesh_shared_state_dht11`: DHT11 temperature/humidity sensor (GPIO4)
- `mesh_gateway`: Telemetry gateway (bridges mesh to server via WiFi)

Enable/disable features via defines:
```cpp
#define ENABLE_BUTTON
// #define ENABLE_LED
```

## I2C Bus Configuration

ESP32 I2C pins: GPIO21 (SDA), GPIO22 (SCL)

Devices on bus:
- 0x3C: SSD1306 OLED display

Use `scan` serial command to enumerate I2C devices.

## PIR Module I2C Protocol (Legacy)

> **Note**: The main `mesh_shared_state_pir` sketch now reads PIR directly from GPIO4.
> This I2C protocol is only used by the legacy `pir_nano` sketch.

Nano acts as I2C slave at 0x42 with register interface:
- 0x00 STATUS: Bit0=motion, Bit1=ready
- 0x01 MOTION_COUNT: Events since last read (auto-clears)
- 0x02 LAST_MOTION: Seconds since motion (0-255)
- 0x03 CONFIG: Bit0=enable, Bit1=auto-clear
- 0x04 HOLD_TIME: Motion hold seconds
- 0x05 VERSION: Firmware version

## Serial Commands (ESP32 nodes)

All nodes support: `status`, `peers`, `state`, `set <key> <value>`, `get <key>`, `sync`, `scan`, `reboot`

Telemetry-enabled nodes add: `telem`, `push`

PIR node adds: `pir`

DHT11 node adds: `dht`

## Hardware Connections

### ESP32 Standard
- OLED SDA: GPIO21
- OLED SCL: GPIO22
- Button: GPIO0
- LED: GPIO2

### PIR Sensor (mesh_shared_state_pir)

Supports two sensor types (select via `#define` in sketch):

**AM312 (default)** - Mini 3.3V sensor, low power (~60μA), 5s warmup
- VCC: 3.3V (native, no regulator needed)
- GND: GND
- OUT: GPIO4

**HC-SR501** - Standard adjustable sensor, 30s warmup
- VCC: 5V (has onboard regulator)
- GND: GND
- OUT: GPIO4

### DHT11 Sensor (mesh_shared_state_dht11)
- DHT DATA: GPIO4 (with 10k pull-up to VCC)
- DHT VCC: 3.3V
- DHT GND: GND

### Nano PIR Module (Legacy)
- PIR sensor OUT: D2 (interrupt-capable)
- I2C SDA: A4
- I2C SCL: A5
- Use level shifter between 5V Nano and 3.3V ESP32

## Code Patterns

### Adding a new sensor node
1. Copy mesh_shared_state_watcher as template
2. Add I2C polling for sensor module
3. Call `setState("key", "value")` to publish to mesh
4. Other nodes receive via state watchers

### State change propagation
```cpp
// On sensor node:
setState("motion", "1");  // Broadcasts to mesh

// On actuator node:
watchState("motion", [](const String& key, const String& value, const String& oldValue) {
  // React to motion state
});
```

### Telemetry Architecture

**Gateway Pattern**: A dedicated gateway node bridges mesh to server.
- Only the gateway needs WiFi credentials
- Other nodes send telemetry via mesh to the gateway
- Gateway pushes all telemetry to the server via HTTP

**Gateway node setup** (firmware/mesh_gateway):
```cpp
swarm.begin("Gateway");
swarm.connectToWiFi("SSID", "password");
swarm.setGatewayMode(true);
swarm.setTelemetryServer("http://192.168.1.100:8000");
swarm.enableTelemetry(true);
```

**Regular node setup** (no WiFi needed):
```cpp
swarm.begin();
swarm.enableTelemetry(true);  // Sends via mesh to gateway
```

Telemetry pushes:
- Every 30 seconds (configurable)
- Immediately on state change
- Includes: uptime, heap_free, peer_count, role, firmware, and all shared state

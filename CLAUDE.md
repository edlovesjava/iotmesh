# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 mesh networking project using painlessMesh for self-organizing, self-healing networks with distributed shared state. Nodes automatically discover peers, elect coordinators, and synchronize key-value state across the network.

## Repository Structure

```
mesh/
├── firmware/                    # PlatformIO project
│   ├── nodes/                   # Node source code
│   │   ├── button/              # Button input node
│   │   ├── clock/               # Round TFT clock display
│   │   ├── dht/                 # DHT11 temperature/humidity sensor
│   │   ├── gateway/             # WiFi bridge for telemetry/OTA
│   │   ├── led/                 # LED output node
│   │   ├── light/               # Light sensor (LDR)
│   │   ├── pir/                 # PIR motion sensor
│   │   └── watcher/             # Observer node (no I/O)
│   ├── include/                 # Shared headers
│   ├── lib/                     # Local libraries
│   ├── test/                    # Unit tests
│   ├── platformio.ini           # Build configuration
│   └── credentials.h            # WiFi credentials (gitignored)
├── server/                      # Telemetry server (future)
│   ├── api/                     # FastAPI backend
│   └── dashboard/               # React frontend
├── docs/                        # Documentation
└── README.md
```

## Development Environment

- **Toolchain**: PlatformIO (preferred)
- **Board**: ESP32 Dev Module (esp32dev)
- **Serial Baud**: 115200
- **Upload Speed**: 921600

### PlatformIO Commands

```bash
# Build specific node
pio run -e pir
pio run -e clock
pio run -e gateway

# Build all default nodes
pio run

# Upload to connected device
pio run -e pir --target upload

# Monitor serial output
pio device monitor -e pir

# Build and upload
pio run -e pir --target upload && pio device monitor -e pir
```

### Available Environments

| Environment | Description |
|-------------|-------------|
| `pir` | PIR motion sensor node |
| `led` | LED output node |
| `button` | Button input node |
| `button2` | Second button node (GPIO4) |
| `dht` | DHT11 temperature/humidity |
| `watcher` | Observer node (no I/O) |
| `clock` | Round TFT clock display |
| `light` | Light sensor (LDR) |
| `gateway` | WiFi bridge for telemetry |
| `pir_debug` | PIR with debug logging |
| `pir_s3` | PIR for ESP32-S3 |
| `gateway_s3` | Gateway for ESP32-S3 |

## Architecture

### ESP32 Nodes

All nodes use the MeshSwarm library which provides:
- painlessMesh network management
- Automatic coordinator election
- Distributed shared state synchronization
- Optional OLED display (SSD1306)
- Optional telemetry and OTA updates

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
swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
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

## Node Types

### Sensor Nodes
- **pir**: PIR motion sensor on GPIO4 (AM312 or HC-SR501)
- **dht**: DHT11 temperature/humidity on GPIO4
- **light**: LDR light sensor (analog)
- **button**: Button input on GPIO0

### Actuator Nodes
- **led**: LED output on GPIO2

### Display Nodes
- **clock**: 1.28" round TFT (GC9A01) with analog clock, sensor gauges
- **watcher**: OLED display showing all mesh state

### Infrastructure Nodes
- **gateway**: WiFi bridge for telemetry server and OTA distribution

## Hardware Connections

### ESP32 Standard (most nodes)
- OLED SDA: GPIO21
- OLED SCL: GPIO22
- Button: GPIO0
- LED: GPIO2

### PIR Sensor
- PIR OUT: GPIO4
- Supports AM312 (3.3V) or HC-SR501 (5V)

### DHT11 Sensor
- DHT DATA: GPIO4 (with 10k pull-up)

### Clock Node (GC9A01 TFT)
- TFT RST: GPIO27
- TFT DC: GPIO25
- TFT CS: GPIO26
- TFT SCL: GPIO18 (SPI clock)
- TFT SDA: GPIO23 (SPI MOSI)
- Left button: GPIO32
- Right button: GPIO33

## Code Patterns

### Adding a new node type

1. Create directory: `firmware/nodes/mynode/`
2. Create `main.cpp` with MeshSwarm setup
3. Add environment to `platformio.ini`:

```ini
[env:mynode]
build_src_filter = +<mynode/>
lib_deps =
    ${env.lib_deps}
    ; Add sensor/display libraries here
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"mynode\"
    -DNODE_NAME=\"MyNode\"
```

### Publishing sensor data
```cpp
swarm.setState("motion", "1");  // Broadcasts to all nodes
```

### Reacting to state changes
```cpp
swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
  // React to motion state
});
```

### Using custom displays
```cpp
#define MESHSWARM_ENABLE_DISPLAY 0  // Disable built-in OLED
#include <MeshSwarm.h>
#include <YourDisplayLibrary.h>
```

## Serial Commands

All nodes support: `status`, `peers`, `state`, `set <key> <value>`, `get <key>`, `sync`, `scan`, `reboot`

Node-specific commands:
- PIR: `pir`
- DHT: `dht`
- Clock: `clock`, `settime HH:MM`
- Gateway: `telem`, `push`

## Telemetry Architecture

**Gateway Pattern**: A dedicated gateway node bridges mesh to server.
- Only the gateway needs WiFi credentials
- Other nodes send telemetry via mesh to the gateway
- Gateway pushes all telemetry to the server via HTTP

**Gateway node setup**:
```cpp
swarm.begin("Gateway");
swarm.connectToWiFi("SSID", "password");
swarm.setGatewayMode(true);
swarm.setTelemetryServer("http://192.168.1.100:8000");
swarm.enableTelemetry(true);
```

**Regular node setup** (no WiFi needed):
```cpp
swarm.begin("NodeName");
swarm.enableTelemetry(true);  // Sends via mesh to gateway
```

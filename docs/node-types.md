# Node Types

This document describes the available node types, their hardware requirements, and configuration options.

## Hardware Requirements

### Common Components

- ESP32 (original dual-core recommended)
- SSD1306 OLED 128x64 display (I2C) - optional but recommended

### Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| OLED SDA  | GPIO 21   |
| OLED SCL  | GPIO 22   |
| Button    | GPIO 0    |
| LED       | GPIO 2    |
| PIR OUT   | GPIO 4    |
| DHT11 DATA| GPIO 4    |

## Node Types

### PIR (Motion Sensor)

Detects motion and publishes `motion` and `motion_<zone>` states to the mesh.

**Hardware**: PIR sensor OUT connected to GPIO4

**Supported Sensors** (select via `#define` in sketch):

| Sensor | Voltage | Warmup | Power | Size |
|--------|---------|--------|-------|------|
| AM312 (default) | 3.3V | 5 sec | ~60μA | Mini |
| HC-SR501 | 5V | 30 sec | ~65mA | Large |

**Features**:
- Automatic sensor-specific timing configuration
- Debounce for reliable edge detection
- Configurable hold time after motion trigger
- Zone-based state keys for multi-sensor deployments

**Serial command**: `pir` - show sensor status and model

**OTA Role**: `pir`

---

### LED (Output Controller)

Watches the shared `led` state and turns the LED on/off accordingly.

**Hardware**: LED on GPIO2 enabled

**OTA Role**: `led`

---

### Button (Input Controller)

Press the button to toggle the shared `led` state across the entire mesh network.

**Hardware**: Button on GPIO0

**OTA Role**: `button`

---

### DHT (Temperature/Humidity Sensor)

Reads DHT11 sensor and publishes `temp`, `humidity`, and zone-specific variants to the mesh.

**Hardware**: DHT11 sensor on GPIO4 (with 10k pull-up resistor)

**Features**:
- Reads every 5 seconds
- Only publishes when values change (≥0.5°C or ≥1% humidity)
- Zone-based state keys for multi-sensor deployments

**Serial command**: `dht` - show sensor status

**OTA Role**: `dht`

---

### Watcher (Network Observer)

Observer node with no hardware I/O. Monitors and displays state changes across the network without controlling any actuators.

**Hardware**: Both button and LED disabled

**OTA Role**: `watcher`

---

### Gateway (Network Bridge)

Bridges the mesh network to external services. Provides WiFi connectivity for telemetry and OTA distribution.

**Features**:
- WiFi station mode for internet connectivity
- HTTP telemetry to server
- OTA firmware distribution to mesh nodes
- HTTP API server for remote commands

**OTA Role**: Distributor (does not receive OTA updates via mesh)

---

### Clock (Time Display)

Displays current time on a round TFT display. Syncs time via NTP when WiFi is available.

**Hardware**: DIYables TFT Round display

**Build flags**:
- `MESHSWARM_ENABLE_DISPLAY=0` (uses TFT instead of OLED)
- `GMT_OFFSET_SEC` - timezone offset in seconds
- `DAYLIGHT_OFFSET` - daylight saving offset

---

### Light (Light Sensor)

Reads ambient light levels using an LDR (Light Dependent Resistor).

**Hardware**: LDR with voltage divider on analog pin

**OTA Role**: `light`

---

### Remote (Touch Controller)

Touch-screen remote control using the ESP32-2432S028R "Cheap Yellow Display".

**Hardware**: ILI9341 TFT with resistive touch

**Build flags**: See `platformio.ini` for TFT_eSPI configuration

## Building Nodes

### PlatformIO

```bash
cd firmware

# Build specific node
pio run -e pir
pio run -e led
pio run -e gateway

# Build and upload
pio run -e pir -t upload

# Build all nodes
pio run
```

### Arduino IDE

1. Open the node's `.ino` file from `firmware/nodes/<type>/`
2. Select "ESP32 Dev Module" as board
3. Compile and upload

## Configuration

### Network Settings

Default settings (modify in sketch or via build flags):

```cpp
#define MESH_PREFIX     "swarm"
#define MESH_PASSWORD   "swarmnet123"
#define MESH_PORT       5555
```

### Zone Configuration

See [Zones Documentation](zones.md) for multi-sensor deployments.

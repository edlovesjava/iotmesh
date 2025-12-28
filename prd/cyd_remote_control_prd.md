# CYD Remote Control Node PRD

## Overview

This document specifies a touchscreen remote control node built on the ESP32-2432S028R "Cheap Yellow Display" (CYD) board. The remote provides a central interface to monitor and control all nodes in the MeshSwarm network through an intuitive touch UI and the Remote Command Protocol (RCP).

## Problem Statement

While the mesh network enables distributed sensor/actuator nodes to communicate, there's no unified way to:

1. **Visualize** all active nodes at a glance
2. **Monitor** real-time status and state from any node
3. **Control** actuators (LED, relays) without serial access
4. **Navigate** between different node views intuitively

The CYD Remote solves this by providing a dedicated touchscreen controller that discovers nodes automatically and presents node-specific control interfaces.

## Use Cases

### UC1: Monitor Network Status
- User powers on CYD Remote
- Home screen displays tiles for each active node type (PIR, LED, DHT, etc.)
- Each tile shows node count and key status indicator
- User sees at a glance: 2 PIR nodes, 1 LED node, 1 DHT node online

### UC2: Control LED Node
- User taps "LED" tile on home screen
- LED control screen appears with:
  - Current LED state (ON/OFF)
  - Toggle button to change state
  - List of all LED nodes if multiple exist
- User taps toggle → LED state changes across mesh

### UC3: View Sensor Data
- User taps "DHT" tile on home screen
- Sensor screen shows:
  - Current temperature and humidity
  - Historical trend (if available)
  - Node health status
- Data updates in real-time via mesh state

### UC4: Check Motion Events
- User taps "PIR" tile on home screen
- Motion screen shows:
  - Current motion state (active/clear)
  - Last motion timestamp
  - Motion count since last reset

## Hardware

### ESP32-2432S028R "Cheap Yellow Display" (CYD)

| Component | Specification | Notes |
|-----------|---------------|-------|
| MCU | ESP32-WROOM-32 | Dual-core 240MHz, 4MB flash |
| Display | ILI9341 2.8" TFT | 320x240 pixels, SPI interface |
| Touch | XPT2046 resistive | SPI interface, shared bus |
| LDR | Onboard | Ambient light sensing |
| RGB LED | WS2812-style | Single addressable LED |
| SD Card | Micro SD slot | Optional storage |
| Speaker | Onboard | PWM audio output |

### Pin Configuration

| ESP32 Pin | Function | Notes |
|-----------|----------|-------|
| GPIO2 | TFT DC | Data/Command select |
| GPIO12 | TFT SDO (MISO) | Display data out |
| GPIO13 | TFT SDI (MOSI) | Display data in |
| GPIO14 | TFT SCK | SPI clock |
| GPIO15 | TFT CS | Display chip select |
| GPIO21 | TFT Backlight | PWM brightness control |
| GPIO25 | Touch CS | XPT2046 chip select |
| GPIO33 | Touch IRQ | Touch interrupt (active LOW) |
| GPIO34 | LDR | Analog light sensor |
| GPIO4 | RGB LED | WS2812 data |
| GPIO22 | Speaker | PWM audio |

### Display Configuration

```cpp
// ILI9341 in landscape orientation (320x240)
#define TFT_WIDTH   320
#define TFT_HEIGHT  240
#define TFT_ROTATION 1  // Landscape, USB on right

// SPI pins
#define TFT_MOSI    13
#define TFT_MISO    12
#define TFT_SCLK    14
#define TFT_CS      15
#define TFT_DC      2
#define TFT_BL      21

// Touch pins (shared SPI bus)
#define TOUCH_CS    25
#define TOUCH_IRQ   33
```

## Software Architecture

### Library Dependencies

| Library | Purpose |
|---------|---------|
| MeshSwarm | Mesh networking and shared state |
| TFT_eSPI | ILI9341 display driver (optimized) |
| XPT2046_Touchscreen | Touch input handling |
| ArduinoJson | RCP message parsing |

### Application Structure

```
┌─────────────────────────────────────────────────────────────────┐
│                    CYD Remote Control Node                       │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    Display Manager                           │ │
│  │   - Screen rendering (TFT_eSPI)                             │ │
│  │   - Touch input (XPT2046)                                    │ │
│  │   - UI component library                                     │ │
│  └──────────────────────┬──────────────────────────────────────┘ │
│                         │                                        │
│  ┌──────────────────────┴──────────────────────────────────────┐ │
│  │                    Screen Manager                            │ │
│  │   - Home screen (node type tiles)                           │ │
│  │   - Node-specific control screens                           │ │
│  │   - Navigation stack                                         │ │
│  └──────────────────────┬──────────────────────────────────────┘ │
│                         │                                        │
│  ┌──────────────────────┴──────────────────────────────────────┐ │
│  │                    Node Discovery                            │ │
│  │   - Track active nodes by type                              │ │
│  │   - Monitor node health/status                              │ │
│  │   - Cache node metadata                                      │ │
│  └──────────────────────┬──────────────────────────────────────┘ │
│                         │                                        │
│  ┌──────────────────────┴──────────────────────────────────────┐ │
│  │                    MeshSwarm + RCP                           │ │
│  │   - Shared state monitoring                                 │ │
│  │   - HTTP client for RCP commands                            │ │
│  │   - Mesh message handling                                   │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## User Interface Design

### Home Screen (Node Grid)

The home screen displays a grid of tiles, one for each active node type discovered on the mesh. Tiles are arranged in a 2-column grid, scrollable if needed.

```
┌────────────────────────────────────────┐
│  IoT Mesh Remote           ⚡ 5 nodes  │  <- Header with network status
├────────────────────────────────────────┤
│                                        │
│  ┌─────────────┐    ┌─────────────┐   │
│  │     🔴      │    │     💡      │   │
│  │    PIR      │    │    LED      │   │
│  │   2 nodes   │    │   1 node    │   │
│  │  motion: 1  │    │  state: ON  │   │
│  └─────────────┘    └─────────────┘   │
│                                        │
│  ┌─────────────┐    ┌─────────────┐   │
│  │     🌡️      │    │     ☀️      │   │
│  │    DHT      │    │   Light     │   │
│  │   1 node    │    │   1 node    │   │
│  │  24°C 65%   │    │  dim (45%)  │   │
│  └─────────────┘    └─────────────┘   │
│                                        │
│  ┌─────────────┐    ┌─────────────┐   │
│  │     🕐      │    │     🌐      │   │
│  │   Clock     │    │  Gateway    │   │
│  │   1 node    │    │   1 node    │   │
│  │  14:32:05   │    │  IP: .42    │   │
│  └─────────────┘    └─────────────┘   │
│                                        │
└────────────────────────────────────────┘
```

### Tile Design

Each tile (140x90 pixels) contains:
- **Icon** (32x32): Visual identifier for node type
- **Type name**: "PIR", "LED", "DHT", etc.
- **Node count**: "2 nodes" or "1 node"
- **Status summary**: Key value from shared state

### Navigation

- **Tap tile**: Navigate to node-specific control screen
- **Swipe down**: Refresh node discovery
- **Long press tile**: Show node list if multiple nodes of same type

### Node Control Screens

Each node type has a custom control screen:

#### LED Control Screen

```
┌────────────────────────────────────────┐
│  ← Back          LED Control           │
├────────────────────────────────────────┤
│                                        │
│              ┌─────────┐               │
│              │         │               │
│              │   💡    │               │
│              │         │               │
│              └─────────┘               │
│                                        │
│         Current State: ON              │
│                                        │
│      ┌─────────────────────────┐       │
│      │                         │       │
│      │      TOGGLE LED         │       │
│      │                         │       │
│      └─────────────────────────┘       │
│                                        │
│  Node: LED-12345678                    │
│  Last update: 2 sec ago                │
│                                        │
└────────────────────────────────────────┘
```

#### PIR Monitor Screen

```
┌────────────────────────────────────────┐
│  ← Back          PIR Motion            │
├────────────────────────────────────────┤
│                                        │
│         ┌───────────────────┐          │
│         │                   │          │
│         │    🔴 MOTION!     │          │  <- Pulses red when active
│         │                   │          │
│         └───────────────────┘          │
│                                        │
│      Last motion: 14:32:05             │
│      Duration: 3 sec                   │
│                                        │
│      ─────── Today ───────             │
│      Motion events: 47                 │
│      Total duration: 12m 34s           │
│                                        │
│  Nodes:                                │
│   • PIR-12345678 (active)              │
│   • PIR-87654321 (clear)               │
│                                        │
└────────────────────────────────────────┘
```

#### DHT Sensor Screen

```
┌────────────────────────────────────────┐
│  ← Back         Temperature            │
├────────────────────────────────────────┤
│                                        │
│     ┌─────────────────────────────┐    │
│     │          24.5°C             │    │
│     │    ████████████░░░░░░░░    │    │  <- Temperature bar
│     │          (75°F)             │    │
│     └─────────────────────────────┘    │
│                                        │
│     ┌─────────────────────────────┐    │
│     │           65%               │    │
│     │    ████████████████░░░░    │    │  <- Humidity bar
│     │        Humidity             │    │
│     └─────────────────────────────┘    │
│                                        │
│  Node: DHT-12345678                    │
│  Last update: 5 sec ago                │
│                                        │
└────────────────────────────────────────┘
```

#### Light Sensor Screen

```
┌────────────────────────────────────────┐
│  ← Back         Light Level            │
├────────────────────────────────────────┤
│                                        │
│              ┌─────────┐               │
│              │         │               │
│              │   ☀️    │               │
│              │         │               │
│              └─────────┘               │
│                                        │
│            Level: 72%                  │
│            State: BRIGHT               │
│                                        │
│     ░░░░████████████████████████      │
│     Dark                    Bright     │
│                                        │
│  Node: Light-12345678                  │
│  Last update: 1 sec ago                │
│                                        │
└────────────────────────────────────────┘
```

## Remote Command Protocol (RCP)

The CYD Remote uses RCP to send commands through the Gateway node's HTTP API.

### Gateway API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/nodes` | GET | List all nodes with metadata |
| `/api/state` | GET | Get all shared state key-values |
| `/api/state/{key}` | GET | Get specific state value |
| `/api/command` | POST | Send command to node(s) |

### Command Message Format

```json
{
  "target": "all" | "type:led" | "node:12345678",
  "action": "setState" | "sendCommand",
  "key": "led",
  "value": "1"
}
```

### Example: Toggle LED

```cpp
// HTTP POST to gateway at /api/command
{
  "target": "type:led",
  "action": "setState",
  "key": "led",
  "value": "1"
}
```

### RCP Client Implementation

```cpp
class RCPClient {
public:
  // Initialize with gateway IP
  void begin(const String& gatewayIP);

  // Discover gateway on mesh
  bool discoverGateway();

  // Get all shared state
  bool getState(JsonDocument& state);

  // Get specific state value
  String getState(const String& key);

  // Set state (sends to gateway, propagates to mesh)
  bool setState(const String& key, const String& value);

  // Send command to specific node type or ID
  bool sendCommand(const String& target, const String& action,
                   const String& key, const String& value);

  // Get list of nodes
  bool getNodes(JsonDocument& nodes);

private:
  String gatewayIP;
  WiFiClient client;
};
```

## Node Discovery

### Discovery via Mesh State

Nodes publish their type in heartbeats and state. The remote tracks:

```cpp
struct NodeInfo {
  uint32_t nodeId;
  String type;           // "pir", "led", "dht", etc.
  String name;           // User-friendly name
  unsigned long lastSeen;
  bool online;
  JsonObject lastState;  // Cached state values
};

class NodeDiscovery {
public:
  // Get all discovered nodes
  std::vector<NodeInfo> getAllNodes();

  // Get nodes by type
  std::vector<NodeInfo> getNodesByType(const String& type);

  // Get unique node types
  std::vector<String> getNodeTypes();

  // Check if node is online (seen in last 30 seconds)
  bool isOnline(uint32_t nodeId);

  // Update from heartbeat/state
  void updateNode(uint32_t nodeId, const String& type, const JsonObject& data);

private:
  std::map<uint32_t, NodeInfo> nodes;
};
```

### State Keys to Watch

| Key Pattern | Node Type | Description |
|-------------|-----------|-------------|
| `motion` | PIR | Motion detected (0/1) |
| `led` | LED | LED state (0/1) |
| `temp`, `humidity` | DHT | Sensor readings |
| `light`, `light_state` | Light | Light level and state |
| `time` | Gateway/Clock | Network time |

## Implementation Phases

### Phase 1: Basic Display and Touch
- [ ] Create `firmware/nodes/remote/` directory
- [ ] Configure TFT_eSPI for CYD (User_Setup.h or build flags)
- [ ] Initialize ILI9341 display with correct rotation
- [ ] Add serial debug output for display dimensions
- [ ] Initialize XPT2046 touch with calibration
- [ ] Basic touch detection and coordinate mapping
- [ ] Simple test screen with touch feedback

### Phase 2: Home Screen
- [ ] Design tile component (icon, text, status)
- [ ] Implement 2-column grid layout
- [ ] Node discovery via mesh state watching
- [ ] Dynamic tile generation based on discovered node types
- [ ] Tap detection on tiles
- [ ] Pull-to-refresh gesture

### Phase 3: Navigation Framework
- [ ] Screen manager with navigation stack
- [ ] Back button handling
- [ ] Screen transition animations (optional)
- [ ] Header bar component with title and back button

### Phase 4: Node Control Screens
- [ ] LED control screen with toggle button
- [ ] PIR monitor screen with motion indicator
- [ ] DHT sensor screen with temperature/humidity display
- [ ] Light sensor screen with level indicator
- [ ] Gateway info screen

### Phase 5: RCP Integration
- [ ] RCP client implementation
- [ ] Gateway discovery (via mesh or mDNS)
- [ ] HTTP client for API calls
- [ ] State update via RCP
- [ ] Command sending via RCP

### Phase 6: Polish
- [ ] Loading indicators
- [ ] Error handling and offline states
- [ ] Auto-brightness via LDR
- [ ] Sound feedback via speaker
- [ ] Settings screen (brightness, sound, calibration)

## Configuration

### Compile-Time Options

```cpp
// Display
#define TFT_WIDTH       320
#define TFT_HEIGHT      240
#define TFT_ROTATION    1       // 0-3, landscape options: 1 or 3

// Touch calibration (adjust after calibration routine)
#define TOUCH_MIN_X     200
#define TOUCH_MAX_X     3800
#define TOUCH_MIN_Y     200
#define TOUCH_MAX_Y     3800

// UI
#define TILE_WIDTH      140
#define TILE_HEIGHT     90
#define TILE_MARGIN     10
#define HEADER_HEIGHT   40

// Behavior
#define NODE_TIMEOUT_MS     30000   // Consider node offline after 30s
#define REFRESH_INTERVAL_MS 1000    // UI refresh rate
#define TOUCH_DEBOUNCE_MS   200     // Touch debounce

// RCP
#define RCP_TIMEOUT_MS      5000    // HTTP request timeout
#define GATEWAY_PORT        80      // Gateway HTTP server port
```

### PlatformIO Environment

```ini
[env:remote]
build_src_filter = +<remote/>
lib_deps =
    ${env.lib_deps}
    bodmer/TFT_eSPI @ ^2.5.0
    paulstoffregen/XPT2046_Touchscreen @ ^1.4
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"remote\"
    -DNODE_NAME=\"Remote\"
    -DMESHSWARM_ENABLE_DISPLAY=0
    ; TFT_eSPI configuration for CYD
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=-1
    -DTFT_BL=21
    -DTOUCH_CS=25
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=16000000
    -DSPI_TOUCH_FREQUENCY=2500000
```

## Display Debugging

Add serial output to verify display configuration:

```cpp
void debugDisplayConfig() {
  Serial.println("\n=== DISPLAY CONFIGURATION ===");
  Serial.printf("TFT Width:  %d\n", TFT_WIDTH);
  Serial.printf("TFT Height: %d\n", TFT_HEIGHT);
  Serial.printf("Rotation:   %d\n", TFT_ROTATION);
  Serial.printf("TFT_MOSI:   %d\n", TFT_MOSI);
  Serial.printf("TFT_MISO:   %d\n", TFT_MISO);
  Serial.printf("TFT_SCLK:   %d\n", TFT_SCLK);
  Serial.printf("TFT_CS:     %d\n", TFT_CS);
  Serial.printf("TFT_DC:     %d\n", TFT_DC);
  Serial.printf("TFT_BL:     %d\n", TFT_BL);
  Serial.printf("TOUCH_CS:   %d\n", TOUCH_CS);
  Serial.println();

  // Query actual display dimensions after init
  Serial.printf("Display width():  %d\n", tft.width());
  Serial.printf("Display height(): %d\n", tft.height());
  Serial.printf("Display rotation: %d\n", tft.getRotation());
  Serial.println("==============================\n");
}
```

## Testing Plan

### Unit Tests

1. **Display Init**: Verify correct width/height after rotation
2. **Touch Calibration**: Map touch coordinates to screen pixels
3. **Tile Layout**: Grid calculates correct positions
4. **RCP Client**: HTTP requests succeed to gateway

### Integration Tests

1. **Node Discovery**: Tiles appear as nodes come online
2. **State Updates**: UI reflects mesh state changes
3. **Command Sending**: LED toggles via touch
4. **Navigation**: Screen stack works correctly

### End-to-End Tests

1. **Full Flow**: Discover nodes → tap tile → control → verify state change
2. **Offline Handling**: Nodes go offline → tiles update → graceful degradation
3. **Gateway Failover**: Gateway restarts → remote reconnects

## File Structure

```
firmware/nodes/remote/
├── main.cpp              # Entry point, setup, loop
├── DisplayManager.h      # TFT and touch handling
├── DisplayManager.cpp
├── ScreenManager.h       # Screen navigation
├── ScreenManager.cpp
├── screens/
│   ├── HomeScreen.h      # Node grid home screen
│   ├── HomeScreen.cpp
│   ├── LEDScreen.h       # LED control
│   ├── LEDScreen.cpp
│   ├── PIRScreen.h       # Motion monitor
│   ├── PIRScreen.cpp
│   ├── DHTScreen.h       # Temperature/humidity
│   ├── DHTScreen.cpp
│   └── LightScreen.h     # Light level
│   └── LightScreen.cpp
├── NodeDiscovery.h       # Track active nodes
├── NodeDiscovery.cpp
├── RCPClient.h           # Remote Command Protocol
├── RCPClient.cpp
└── ui/
    ├── Tile.h            # Tile component
    ├── Button.h          # Touch button component
    └── Colors.h          # Color definitions
```

## Open Questions

1. **Touch Calibration**: Store calibration in NVS or require recalibration on boot?
2. **Multiple Gateways**: Support failover if primary gateway goes offline?
3. **Screen Sleep**: Auto-dim/sleep after inactivity to save power?
4. **Custom Screens**: Allow user-defined screens for automation scenarios?
5. **Haptic Feedback**: Use speaker for touch feedback sounds?

## References

- [ESP32-2432S028R (CYD) Schematic](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [XPT2046 Touchscreen Library](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
- [ILI9341 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ILI9341.pdf)

# ESP32 Mesh Swarm

A self-organizing, self-healing mesh network for ESP32 microcontrollers with distributed shared state synchronization.

## Overview

This project implements a mesh network where multiple ESP32 nodes can discover each other, elect a coordinator, and share state across the entire network. When any node updates a value, all other nodes automatically sync to reflect the change.

## Features

- **Self-organizing mesh**: Nodes automatically discover and connect to each other
- **Coordinator election**: Lowest node ID automatically becomes coordinator
- **Self-healing**: Network recovers from node failures and re-elects coordinators
- **Distributed shared state**: Key-value store synchronized across all nodes
- **State watchers**: Register callbacks to react to state changes
- **Conflict resolution**: Version numbers + origin ID ensure deterministic state convergence
- **OLED display**: Real-time status display on each node
- **Serial console**: Debug and control nodes via serial commands

## Hardware Requirements

- ESP32 (original dual-core recommended)
- SSD1306 OLED 128x64 display (I2C)
- Optional: Button on GPIO0, LED on GPIO2, PIR sensor, DHT11

### Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| OLED SDA  | GPIO 21   |
| OLED SCL  | GPIO 22   |
| Button    | GPIO 0    |
| LED       | GPIO 2    |
| PIR OUT   | GPIO 4    |
| DHT11 DATA| GPIO 4    |

## Required Libraries

Install via Arduino Library Manager:

- painlessMesh
- ArduinoJson
- Adafruit SSD1306
- Adafruit GFX Library
- DHT sensor library (by Adafruit) - for DHT11 node

## Sketches

### mesh_swarm_base

Basic mesh network implementation with:
- Auto peer discovery
- Coordinator election (lowest node ID wins)
- Heartbeat health monitoring
- OLED status display
- Extensible message system

This is the foundation sketch demonstrating core mesh functionality without shared state.

### mesh_shared_state

Full-featured shared state implementation. All hardware features (button + LED) enabled. Use this as a reference or for nodes that need both input and output capabilities.

Features:
- Distributed key-value state store
- State watcher callbacks
- Version-based conflict resolution
- Periodic state synchronization

### mesh_shared_state_button

Specialized node configured as a **button input**. Press the button to toggle the shared `led` state across the entire mesh network.

Hardware: Button on GPIO0 enabled, LED disabled.

### mesh_shared_state_led

Specialized node configured as an **LED output**. Watches the shared `led` state and turns the LED on/off accordingly.

Hardware: LED on GPIO2 enabled, button disabled.

### mesh_shared_state_watcher

Observer node with no hardware I/O. Monitors and displays state changes across the network without controlling any actuators.

Hardware: Both button and LED disabled.

### mesh_shared_state_pir

PIR motion sensor node. Detects motion and publishes `motion` and `motion_<zone>` states to the mesh.

Hardware: PIR sensor (HC-SR501 or similar) on GPIO4.

Features:
- 30-second warmup period for sensor stabilization
- 50ms debounce for reliable edge detection
- 3-second hold time after last motion trigger
- Zone-based state keys for multi-sensor deployments

Serial command: `pir` - show sensor status

### mesh_shared_state_dht11

Temperature and humidity sensor node. Reads DHT11 sensor and publishes `temp`, `humidity`, and zone-specific variants to the mesh.

Hardware: DHT11 sensor on GPIO4 (with 10k pull-up resistor).

Features:
- Reads every 5 seconds
- Only publishes when values change (≥0.5°C or ≥1% humidity)
- Zone-based state keys for multi-sensor deployments

Serial command: `dht` - show sensor status

## Zones

Zones allow multiple sensors of the same type to coexist on the mesh without overwriting each other's data. Each sensor node publishes both generic and zone-specific state keys.

### How Zones Work

A PIR sensor in "zone1" publishes:
- `motion` - generic key (last sensor to update wins)
- `motion_zone1` - zone-specific key (unique to this sensor)

A DHT11 sensor in "zone2" publishes:
- `temp` and `humidity` - generic keys
- `temp_zone2` and `humidity_zone2` - zone-specific keys

### Configuration

Set the zone in each sketch's configuration section:

```cpp
#define MOTION_ZONE  "zone1"   // PIR node
#define SENSOR_ZONE  "zone1"   // DHT11 node
```

### Example Multi-Zone Setup

| Node | Zone | State Keys |
|------|------|------------|
| PIR (front door) | `entrance` | `motion`, `motion_entrance` |
| PIR (backyard) | `backyard` | `motion`, `motion_backyard` |
| DHT11 (living room) | `living` | `temp`, `humidity`, `temp_living`, `humidity_living` |
| DHT11 (garage) | `garage` | `temp`, `humidity`, `temp_garage`, `humidity_garage` |

### Watching Zone-Specific State

```cpp
// Watch all motion events (any zone)
swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("Motion somewhere: %s\n", value.c_str());
});

// Watch specific zone
swarm.watchState("motion_entrance", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("Front door motion: %s\n", value.c_str());
});

// Watch all state changes and filter by prefix
swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
  if (key.startsWith("temp_")) {
    Serial.printf("Temperature in %s: %s\n", key.c_str() + 5, value.c_str());
  }
});
```

## Quick Start

1. Install required libraries in Arduino IDE
2. Select "ESP32 Dev Module" as your board
3. Flash `mesh_shared_state_button` to one ESP32
4. Flash `mesh_shared_state_led` to another ESP32
5. Power on both nodes - they will auto-discover each other
6. Press the button on the button node - the LED on the LED node will toggle

## Serial Commands

Connect via serial monitor (115200 baud):

| Command | Description |
|---------|-------------|
| `status` | Show node info (ID, role, peers, heap) |
| `peers` | List all known peers |
| `state` | Show all shared state entries |
| `set <key> <value>` | Set a shared state value |
| `get <key>` | Get a shared state value |
| `sync` | Broadcast full state to all nodes |
| `reboot` | Restart the node |
| `pir` | PIR sensor status (PIR node only) |
| `dht` | DHT11 sensor status (DHT11 node only) |

## Network Configuration

Default settings in all sketches:

```cpp
#define MESH_PREFIX     "swarm"
#define MESH_PASSWORD   "swarmnet123"
#define MESH_PORT       5555
```

Modify these values to create separate mesh networks or improve security.

## Architecture

### Message Types

| Type | Purpose |
|------|---------|
| `MSG_HEARTBEAT` | Periodic health check, carries role/uptime/heap |
| `MSG_STATE_SET` | Single key-value state update |
| `MSG_STATE_SYNC` | Full state dump (on join or periodic) |
| `MSG_STATE_REQ` | Request state sync from peers |
| `MSG_COMMAND` | Custom commands (extensible) |

### State Conflict Resolution

When multiple nodes update the same key:
1. Higher version number wins
2. If versions match, lower origin node ID wins (deterministic)

This ensures all nodes converge to the same state without requiring a central authority.

## Extending the Project

### Adding a New State Watcher

```cpp
watchState("mykey", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("mykey changed: %s -> %s\n", oldValue.c_str(), value.c_str());
  // React to the change
});
```

### Adding Sensor Data

```cpp
// In loop(), periodically:
float temp = readTemperature();
setState("temp", String(temp));
```

### Wildcard Watcher

Watch all state changes:

```cpp
watchState("*", [](const String& key, const String& value, const String& oldValue) {
  Serial.printf("Any state: %s = %s\n", key.c_str(), value.c_str());
});
```

## License

MIT License - feel free to use and modify for your projects.

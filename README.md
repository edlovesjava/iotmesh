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
- Optional: Button on GPIO0, LED on GPIO2

### Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| OLED SDA  | GPIO 21   |
| OLED SCL  | GPIO 22   |
| Button    | GPIO 0    |
| LED       | GPIO 2    |

## Required Libraries

Install via Arduino Library Manager:

- painlessMesh
- ArduinoJson
- Adafruit SSD1306
- Adafruit GFX Library

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

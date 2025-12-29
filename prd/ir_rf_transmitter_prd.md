# Remote Control IR/RF Actuator Node PRD

## Overview

This document specifies a remote control actuator node that can both **receive** (learn) and **transmit** (control) infrared (IR) and/or radio frequency (RF) signals. The node integrates with the MeshSwarm network, enabling mesh-wide control of external devices like TVs, air conditioners, fans, lights, and RF-controlled outlets.

## Problem Statement

Home automation often requires controlling legacy devices that use IR remotes (TVs, AC units, audio equipment) or RF remotes (wireless outlets, garage doors, fans). Currently, the IoT Mesh network can only interact with directly-wired devices. This node bridges that gap by:

1. **Learning** codes from existing remotes
2. **Storing** learned codes persistently
3. **Transmitting** codes on demand via mesh state changes
4. **Receiving** and republishing codes for mesh-wide event triggering

## Use Cases

### UC1: Control TV via Motion Detection
- PIR node detects motion → sets `motion=1`
- Remote control node watches `motion` state
- On motion detected → transmits "TV Power On" IR code

### UC2: Learn and Replay AC Commands
- User puts node in "learn mode" via serial or mesh command
- User presses button on AC remote
- Node captures and stores the IR code as "ac_cool_22"
- Later: `set remote_tx ac_cool_22` → node transmits stored code

### UC3: RF Outlet Control
- Node has learned RF codes for wireless outlets
- Button node toggles `outlet_1` state
- Remote node watches `outlet_1` → transmits corresponding RF code

### UC4: Doorbell Detection
- Node receives RF signal from wireless doorbell
- Publishes `doorbell=1` to mesh
- LED nodes can flash, watcher nodes can log event

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32 Remote Control Node                     │
│                                                                  │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────────┐    │
│  │  IR Receiver │     │  RF Receiver │     │   MeshSwarm     │    │
│  │   (TSOP384x) │     │   (433MHz)   │     │   Library       │    │
│  └──────┬──────┘     └──────┬──────┘     └────────┬────────┘    │
│         │                    │                     │             │
│         ▼                    ▼                     │             │
│  ┌──────────────────────────────────────┐         │             │
│  │         Signal Decoder               │         │             │
│  │   (IRremoteESP8266 / RCSwitch)       │         │             │
│  └──────────────────┬───────────────────┘         │             │
│                     │                              │             │
│                     ▼                              ▼             │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    Code Manager                           │   │
│  │   - Store learned codes (Preferences/NVS)                │   │
│  │   - Map names to codes                                    │   │
│  │   - Trigger on mesh state changes                         │   │
│  └──────────────────┬───────────────────────────────────────┘   │
│                     │                                            │
│         ┌───────────┴───────────┐                               │
│         ▼                       ▼                                │
│  ┌─────────────┐         ┌─────────────┐                        │
│  │ IR Transmit │         │ RF Transmit │                        │
│  │  (IR LED)   │         │ (433MHz TX) │                        │
│  └─────────────┘         └─────────────┘                        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
                    ┌──────────────────┐
                    │   Mesh Network   │
                    │     (swarm)      │
                    └──────────────────┘
```

## Hardware

### Components

| Component | Model | Description |
|-----------|-------|-------------|
| ESP32 | ESP32 DevKit | Main controller |
| IR Receiver | TSOP38438 / VS1838B | 38kHz IR receiver |
| IR Transmitter | 940nm IR LED + transistor | High-power IR transmission |
| RF Receiver | MX-RM-5V (433MHz) | 433MHz ASK receiver |
| RF Transmitter | FS1000A (433MHz) | 433MHz ASK transmitter |
| OLED Display | SSD1306 128x64 | Status display |

### Pin Configuration

| ESP32 Pin | Function | Notes |
|-----------|----------|-------|
| GPIO21 | I2C SDA | OLED display |
| GPIO22 | I2C SCL | OLED display |
| GPIO15 | IR Receiver | TSOP data out |
| GPIO4 | IR Transmitter | Via NPN transistor (2N2222) |
| GPIO13 | RF Receiver | 433MHz RX data |
| GPIO12 | RF Transmitter | 433MHz TX data |
| GPIO0 | Boot Button | Enter learn mode |
| GPIO2 | Status LED | Activity indicator |

### IR Transmitter Circuit

```
ESP32 GPIO4 ───┬──── 1kΩ ──── Base (2N2222)
               │
               │              Collector ───┬─── IR LED (+)
               │                           │
              GND                         3.3V
                                           │
                              Emitter ─────┴─── IR LED (-) via 47Ω
```

Note: For longer range, use multiple IR LEDs in series or a dedicated driver IC (e.g., TSAL6100).

### RF Module Connections

```
RF Receiver (MX-RM-5V):
  VCC  → 5V
  GND  → GND
  DATA → GPIO13

RF Transmitter (FS1000A):
  VCC  → 3.3V (or 5V for more range)
  GND  → GND
  DATA → GPIO12
```

## Software Design

### Required Libraries

| Library | Purpose |
|---------|---------|
| MeshSwarm | Mesh networking and shared state |
| IRremoteESP8266 | IR encode/decode (supports most protocols) |
| RCSwitch | 433MHz RF encode/decode |
| Preferences | ESP32 NVS for persistent code storage |

### Shared State Keys

| Key | Values | Description |
|-----|--------|-------------|
| `remote_tx` | code_name | Trigger transmission of named code |
| `remote_rx` | protocol:code | Last received code (auto-published) |
| `remote_mode` | "idle", "learn_ir", "learn_rf" | Current operating mode |
| `remote_learn_name` | string | Name to assign to next learned code |
| `remote_last_tx` | code_name | Last transmitted code name |
| `remote_codes` | count | Number of stored codes |

### Operating Modes

#### 1. Idle Mode (Default)
- Listens for `remote_tx` state changes
- On change, looks up code by name and transmits
- Optionally receives and publishes incoming signals

#### 2. IR Learn Mode
- Entered via: `set remote_mode learn_ir` or serial command `learn ir <name>`
- Waits for IR signal from user's remote
- Stores captured code with specified name
- Returns to idle mode after capture

#### 3. RF Learn Mode
- Entered via: `set remote_mode learn_rf` or serial command `learn rf <name>`
- Waits for RF signal
- Stores captured code with specified name
- Returns to idle mode after capture

### Code Storage Format

Codes are stored in ESP32 NVS (Preferences library) with structure:

```cpp
// Key format: "ir_<name>" or "rf_<name>"
// Value format: JSON string

// IR Code Example:
{
  "type": "ir",
  "protocol": "NEC",
  "code": "0x20DF10EF",
  "bits": 32,
  "raw": []  // Optional: raw timing data for unknown protocols
}

// RF Code Example:
{
  "type": "rf",
  "protocol": 1,  // RCSwitch protocol number
  "code": 5393,
  "bits": 24,
  "pulse": 320   // Pulse length
}
```

### Code Manager API

```cpp
class RemoteCodeManager {
public:
  // Store a learned code
  bool storeCode(const String& name, const CodeData& code);

  // Retrieve a stored code
  bool getCode(const String& name, CodeData& code);

  // Delete a stored code
  bool deleteCode(const String& name);

  // List all stored code names
  std::vector<String> listCodes();

  // Get code count
  int getCodeCount();

  // Clear all codes
  void clearAll();
};
```

### MeshSwarm Integration

```cpp
void setup() {
  swarm.begin("Remote");
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive("remote");

  // Initialize IR/RF hardware
  irReceiver.enableIRIn();
  rfReceiver.enableReceive(RF_RX_PIN);

  // Watch for transmit commands
  swarm.watchState("remote_tx", [](const String& key, const String& value, const String& oldValue) {
    if (value.length() > 0 && value != oldValue) {
      transmitCode(value);
    }
  });

  // Watch for mode changes
  swarm.watchState("remote_mode", [](const String& key, const String& value, const String& oldValue) {
    setMode(value);
  });

  // Register polling for receive
  swarm.onLoop(pollReceivers);
}
```

### Serial Commands

| Command | Description |
|---------|-------------|
| `remote` | Show remote control status |
| `codes` | List all stored codes |
| `learn ir <name>` | Enter IR learn mode |
| `learn rf <name>` | Enter RF learn mode |
| `tx <name>` | Transmit stored code |
| `delete <name>` | Delete stored code |
| `clear` | Clear all stored codes |
| `export` | Export codes as JSON |
| `import <json>` | Import codes from JSON |

### Display Layout

```
┌─────────────────────┐
│ Node: Remote        │
│ ID: 12345678        │
│ Role: NODE  Peers:3 │
│---------------------│
│ Mode: IDLE          │
│ Codes: 12 stored    │
│ Last TX: tv_power   │
│ Last RX: NEC:20DF   │
└─────────────────────┘
```

## Configuration

### Compile-Time Options

```cpp
// Enable/disable IR functionality
#define ENABLE_IR_RX
#define ENABLE_IR_TX

// Enable/disable RF functionality
#define ENABLE_RF_RX
#define ENABLE_RF_TX

// Pin assignments
#define IR_RX_PIN     15
#define IR_TX_PIN     4
#define RF_RX_PIN     13
#define RF_TX_PIN     12

// Behavior
#define AUTO_PUBLISH_RX     true   // Publish received codes to mesh
#define LEARN_TIMEOUT_MS    30000  // Learn mode timeout
#define TX_REPEAT_COUNT     3      // Times to repeat transmission
```

### Runtime Configuration via Mesh State

| State Key | Default | Description |
|-----------|---------|-------------|
| `remote_auto_rx` | "1" | Auto-publish received codes |
| `remote_tx_repeat` | "3" | Transmission repeat count |
| `remote_zone` | "living" | Zone identifier |

## Automation Rules

The node supports simple automation rules stored in NVS:

```cpp
// Rule format: "when <state_key>=<value> then tx <code_name>"

// Examples:
"when motion=1 then tx tv_power"
"when temp>28 then tx ac_cool"
"when outlet_1=1 then tx outlet1_on"
```

### Rule Management Commands

| Command | Description |
|---------|-------------|
| `rule add <rule>` | Add automation rule |
| `rule list` | List all rules |
| `rule delete <id>` | Delete rule by ID |
| `rule clear` | Clear all rules |

## Implementation Phases

### Phase 1: Basic IR Transmit/Receive
- [ ] Create `mesh_shared_state_remote` sketch
- [ ] Implement IR receiver using IRremoteESP8266
- [ ] Implement IR transmitter with basic protocols (NEC, Sony, Samsung)
- [ ] Store codes in Preferences
- [ ] Mesh state integration for `remote_tx`
- [ ] Serial commands: `learn`, `tx`, `codes`

### Phase 2: RF Support
- [ ] Add RCSwitch for 433MHz
- [ ] RF receive and learn mode
- [ ] RF transmit
- [ ] Support common RF protocols (PT2262, EV1527)

### Phase 3: Advanced Features
- [ ] Raw IR capture for unknown protocols
- [ ] Automation rules engine
- [ ] Code import/export via mesh
- [ ] Multi-code sequences (macros)
- [ ] Scheduled transmissions

### Phase 4: Web Integration
- [ ] Server API endpoints for code management
- [ ] Dashboard UI for learning and triggering
- [ ] Code library sharing between nodes

## File Structure

```
firmware/
├── mesh_shared_state_remote/
│   ├── mesh_shared_state_remote.ino   # Main sketch
│   ├── RemoteCodeManager.h            # Code storage management
│   ├── RemoteCodeManager.cpp
│   ├── IRHandler.h                    # IR transmit/receive
│   ├── IRHandler.cpp
│   ├── RFHandler.h                    # RF transmit/receive
│   └── RFHandler.cpp
└── ...

prd/
└── remote_control_node_prd.md         # This document
```

## Testing Plan

### Unit Tests

1. **Code Storage**: Store, retrieve, delete, list operations
2. **IR Decode**: Test with various remotes (TV, AC, etc.)
3. **IR Encode**: Verify transmission matches original codes
4. **RF Decode**: Test with RF outlets, doorbells
5. **RF Encode**: Verify RF device responds to transmitted codes

### Integration Tests

1. **Mesh State Trigger**: Set `remote_tx` from another node, verify transmission
2. **Learn Flow**: Complete learn cycle via mesh commands
3. **Auto-Publish**: Verify received codes appear in mesh state
4. **Persistence**: Reboot node, verify codes persist

### End-to-End Tests

1. **PIR → TV Power**: Motion triggers TV power on
2. **Button → Outlet**: Button press toggles RF outlet
3. **Multi-Node**: Learn on one node, trigger from another
4. **Gateway Integration**: Control via server API

## Hardware Considerations

### IR Range Optimization

- Use high-power IR LED (TSAL6100, 100mA)
- Add 2-3 LEDs for wider coverage
- Consider lens/reflector for directional boost
- Transistor driver essential for high current

### RF Range Optimization

- Use 5V supply for RF transmitter (increases range)
- Add 17.3cm antenna wire to both TX and RX
- Position away from ESP32 WiFi antenna
- Consider SYN115/SYN480R for better performance

### Interference

- Keep IR receiver away from fluorescent lights
- Shield RF modules from ESP32 if interference occurs
- Use shielded cables for longer wire runs

## Supported Protocols

### IR Protocols (via IRremoteESP8266)

| Protocol | Common Devices |
|----------|----------------|
| NEC | LG, Samsung, many others |
| Sony | Sony TVs, audio |
| RC5/RC6 | Philips, some EU brands |
| Samsung | Samsung TVs |
| Panasonic | Panasonic devices |
| Daikin | AC units |
| Mitsubishi | AC units |
| Raw | Unknown protocols |

### RF Protocols (via RCSwitch)

| Protocol | Common Devices |
|----------|----------------|
| Protocol 1 | Most 433MHz outlets |
| Protocol 2 | Some remotes |
| PT2262 | Wireless doorbells, sensors |
| EV1527 | Learning code devices |
| HT6P20B | Some Asian market devices |

## Security Considerations

1. **Code Replay**: Anyone on the mesh can trigger transmissions
2. **RF Vulnerabilities**: 433MHz has no encryption
3. **Recommendations**:
   - Use mesh authentication
   - Implement rate limiting on transmissions
   - Log all remote operations
   - Consider code obfuscation for sensitive devices

## Open Questions

1. **Multiple Frequencies**: Support 315MHz in addition to 433MHz?
2. **IR Blaster Range**: External IR blaster module for multiple rooms?
3. **AC Protocol Complexity**: Full AC remote emulation (temp, mode, fan)?
4. **Code Sharing**: Sync learned codes between multiple remote nodes?
5. **Scheduling**: Built-in time-based triggers or defer to server?

## References

- [IRremoteESP8266 Library](https://github.com/crankyoldgit/IRremoteESP8266)
- [RCSwitch Library](https://github.com/sui77/rc-switch)
- [ESP32 Preferences](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html)
- [TSOP38438 Datasheet](https://www.vishay.com/docs/82491/tsop382.pdf)
- [433MHz RF Module Guide](https://randomnerdtutorials.com/rf-433mhz-transmitter-receiver-module-with-arduino/)

# PIR Motion Sensor Module Specification

## Overview

This document specifies a motion detection module using a Lafvin Nano (Arduino Nano compatible) as a dedicated signal processor for a PIR sensor. The Nano communicates with an ESP32 mesh node over I2C, allowing motion events to propagate across the mesh network.

## Architecture

```
┌─────────────────┐      I2C       ┌─────────────────┐
│   Lafvin Nano   │◄──────────────►│   ESP32 Node    │
│  (I2C Slave)    │   SDA/SCL      │  (I2C Master)   │
│                 │                │                 │
│  ┌───────────┐  │                │  ┌───────────┐  │
│  │PIR Filter │  │                │  │Mesh State │  │
│  │ & Debounce│  │                │  │  "motion" │  │
│  └───────────┘  │                │  └───────────┘  │
└────────┬────────┘                └─────────────────┘
         │                                  │
    ┌────┴────┐                      ┌──────┴──────┐
    │   PIR   │                      │ Mesh Network│
    │ Sensor  │                      │  (swarm)    │
    └─────────┘                      └─────────────┘
```

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| Lafvin Nano | Arduino Nano compatible (ATmega328P, 5V logic) |
| PIR Sensor | HC-SR501 or similar (typically 3.3V-5V, digital output) |
| ESP32 Node | Existing mesh node (mesh_shared_state variant) |
| Level Shifter | Optional - for 5V Nano to 3.3V ESP32 I2C (recommended) |

### Wiring

#### Lafvin Nano Connections

| Nano Pin | Connection | Notes |
|----------|------------|-------|
| D2 | PIR OUT | Interrupt-capable pin |
| A4 (SDA) | ESP32 GPIO21 | Via level shifter if used |
| A5 (SCL) | ESP32 GPIO22 | Via level shifter if used |
| GND | Common ground | Shared with ESP32 and PIR |
| 5V | PIR VCC | Power to PIR sensor |

#### ESP32 Connections

| ESP32 Pin | Connection | Notes |
|-----------|------------|-------|
| GPIO21 (SDA) | Nano A4 | I2C data |
| GPIO22 (SCL) | Nano A5 | I2C clock |
| GND | Common ground | |

### Level Shifting Consideration

The Lafvin Nano operates at 5V logic while ESP32 uses 3.3V. Options:

1. **Bidirectional level shifter** (recommended): Use a module like BSS138 or TXS0102
2. **Direct connection**: May work since ESP32 pins are somewhat 5V tolerant for input, but not recommended for reliability
3. **Resistor divider**: For SCL/SDA lines (less ideal for I2C)

## Software Design

### Lafvin Nano Firmware

#### Responsibilities

1. **PIR Signal Acquisition**: Read digital output from PIR sensor
2. **Signal Filtering**: Debounce and validate motion events
3. **False Positive Reduction**: Require sustained signal before confirming motion
4. **I2C Slave Communication**: Respond to ESP32 master requests
5. **Motion Timeout**: Auto-clear motion state after configurable period

#### I2C Protocol

| Address | Default: 0x42 (configurable) |
|---------|------------------------------|

**Register Map:**

| Register | Name | R/W | Description |
|----------|------|-----|-------------|
| 0x00 | STATUS | R | Bit 0: Motion detected (1=yes), Bit 1: Sensor ready |
| 0x01 | MOTION_COUNT | R | Number of motion events since last read (auto-clears) |
| 0x02 | LAST_MOTION_SEC | R | Seconds since last motion (0-255, saturates at 255) |
| 0x03 | CONFIG | R/W | Bit 0: Enable, Bit 1: Auto-clear enable |
| 0x04 | DEBOUNCE_MS | R/W | Debounce time in 10ms units (default: 5 = 50ms) |
| 0x05 | HOLD_TIME_SEC | R/W | Motion hold time in seconds (default: 5) |
| 0x06 | VERSION | R | Firmware version (major.minor in nibbles) |

**Read Transaction:**
```
Master: [START] [ADDR+W] [REG] [RESTART] [ADDR+R] [DATA...] [STOP]
```

**Write Transaction:**
```
Master: [START] [ADDR+W] [REG] [DATA] [STOP]
```

#### Signal Filtering Algorithm

```
PIR Raw Signal
      │
      ▼
┌─────────────────┐
│ Hardware IRQ on │
│ rising edge     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Debounce check  │  Reject if < DEBOUNCE_MS since last edge
│ (50ms default)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Validation      │  Require signal HIGH for >100ms
│ (confirm real)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Set MOTION=1    │
│ Reset hold timer│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Hold timer      │  Keep MOTION=1 for HOLD_TIME_SEC
│ (5 sec default) │  after last valid trigger
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Clear MOTION=0  │
└─────────────────┘
```

### ESP32 Mesh Node Modifications

#### New Sketch: mesh_shared_state_pir

Based on `mesh_shared_state_watcher` with additions:

1. **I2C Master Initialization**: Configure I2C for polling the Nano
2. **Polling Loop**: Read PIR module status at configurable interval (default 100ms)
3. **State Publication**: Set shared state `motion` = "1" or "0"
4. **Optional**: Publish `motion_count` for activity level

#### Shared State Keys

| Key | Values | Description |
|-----|--------|-------------|
| `motion` | "0", "1" | Current motion state |
| `motion_zone` | String | Zone identifier (e.g., "kitchen", "hallway") |
| `motion_ts` | Unix timestamp | Last motion detection time |

## Configuration

### Nano Defaults

```cpp
#define I2C_ADDRESS       0x42
#define PIR_PIN           2       // Interrupt-capable
#define DEBOUNCE_MS       50
#define VALIDATION_MS     100
#define HOLD_TIME_SEC     5
#define LED_PIN           13      // Status LED
```

### ESP32 Defaults

```cpp
#define PIR_I2C_ADDRESS   0x42
#define PIR_POLL_MS       100     // Poll interval
#define MOTION_ZONE       "default"
```

## Implementation Phases

### Phase 1: Basic Functionality

- [ ] Nano firmware with PIR reading and I2C slave
- [ ] Simple debounce filtering
- [ ] ESP32 I2C master polling
- [ ] Mesh state publication

### Phase 2: Enhanced Filtering

- [ ] Validation window (sustained signal check)
- [ ] Configurable hold time
- [ ] Motion event counting
- [ ] Status LED feedback on Nano

### Phase 3: Advanced Features

- [ ] Multiple PIR sensors on single Nano
- [ ] Sensitivity adjustment via I2C
- [ ] Sleep mode for Nano (wake on PIR interrupt)
- [ ] Diagnostic data (false positive rate, etc.)

## File Structure

```
mesh/
├── pir_nano/
│   └── pir_nano.ino           # Lafvin Nano firmware
├── mesh_shared_state_pir/
│   └── mesh_shared_state_pir.ino  # ESP32 PIR mesh node
└── docs/
    └── pir_motion_module_spec.md  # This document
```

## Testing Plan

1. **Nano Standalone**: Verify PIR reading and filtering via Serial monitor
2. **I2C Communication**: Use I2C scanner to verify address, then test register reads
3. **Integration**: Connect to ESP32, verify motion state propagation
4. **Mesh Test**: Multiple nodes, verify motion state syncs across network
5. **False Positive Test**: Observe over extended period, tune filtering parameters

## Open Questions

1. **Level shifter**: Use dedicated module or rely on ESP32's input tolerance?
2. **Polling vs Interrupt**: Should ESP32 poll, or should Nano signal via additional GPIO?
3. **Multiple zones**: Support multiple PIR modules per ESP32 node?
4. **Power**: Power Nano from ESP32's 5V or separate supply?

## References

- [HC-SR501 PIR Sensor Datasheet](https://www.epitran.it/ebayDrive/datasheet/44.pdf)
- [Arduino Wire Library (I2C)](https://www.arduino.cc/en/Reference/Wire)
- [ESP32 I2C Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)

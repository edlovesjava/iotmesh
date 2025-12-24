# Clock Node

A mesh-connected clock with a 1.28" round TFT display (GC9A01), showing analog/digital time, date, and sensor data from the mesh network.

## Features

- **Analog clock face** with hour, minute, and second hands
- **Digital time display** at bottom (HH:MM:SS)
- **Date display** at top (Mon DD format)
- **Temperature and humidity** from mesh DHT sensors
- **Two-button interface** for navigation and time setting
- **Sensor screen** with arc gauges for temp/humidity
- **Auto set-time mode** if no gateway time received

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| ESP32 | Original dual-core dev module |
| GC9A01 TFT | 1.28" round 240x240 display |
| 2x Momentary buttons | For navigation and settings |

### Wiring

**Display (SPI):**
| Pin | GPIO | Description |
|-----|------|-------------|
| RST | 27 | Reset |
| DC | 25 | Data/Command |
| CS | 26 | Chip Select |
| SCL | 18 | SPI Clock |
| SDA | 23 | SPI MOSI |
| VCC | 3.3V | Power |
| GND | GND | Ground |

**Buttons (active LOW with internal pullup):**
| Button | GPIO | Description |
|--------|------|-------------|
| Left | 32 | Previous screen / Decrement |
| Right | 33 | Next screen / Increment |

Wire each button between GPIO and GND. Internal pullups are enabled.

## Usage

### Normal Operation

1. **Power on** - Clock shows "Waiting..." until time is set
2. **Time sync** - Automatically syncs from gateway `time` state if available
3. **View clock** - Shows analog clock with digital time at bottom
4. **View sensors** - Press right button to see sensor screen with arc gauges

### Button Controls

| Action | Normal Mode | Set Time Mode |
|--------|-------------|---------------|
| Left button | Previous screen | Decrement hour/minute |
| Right button | Next screen | Increment hour/minute |
| Both buttons (hold 300ms) | Enter set time mode | Next field / Save & exit |
| Hold left/right | - | Auto-repeat adjustment |

### Setting the Time

1. **Enter set time mode:**
   - Press and hold both buttons for 300ms, OR
   - Wait 10 seconds after power-on (auto-enters if no gateway time)

2. **Set the hour:**
   - Hour blinks in yellow
   - Press left to decrement, right to increment
   - Hold button for fast scrolling

3. **Set the minute:**
   - Press both buttons to advance to minutes
   - Minute blinks in yellow
   - Press left to decrement, right to increment

4. **Save and exit:**
   - Press both buttons to save time and return to clock

### Serial Commands

| Command | Description |
|---------|-------------|
| `clock` | Show current time, date, and sensor values |
| `settime HH:MM` | Manually set time (e.g., `settime 14:30`) |
| `settime HH:MM:SS` | Set time with seconds |

## Screens

### Clock Screen
- Analog clock face with hour ticks
- Hour hand (white, thick)
- Minute hand (white, medium)
- Second hand (red, thin)
- Digital time at bottom (HH:MM:SS)
- Date at top (Mon DD)
- Temperature on left side (cyan)
- Humidity on right side (green)

### Sensor Screen
- Title "SENSORS"
- Temperature arc gauge (left, cyan, 0-40°C)
- Humidity arc gauge (right, green, 0-100%)
- Large numeric values with units

### Set Time Screen
- Title "SET TIME" in yellow
- Large blinking time display (HH:MM)
- Current field indicator
- Instructions for button usage

## State Machine

```mermaid
stateDiagram-v2
    [*] --> Waiting: Power on

    Waiting --> SetHour: No gateway time (10s timeout)
    Waiting --> SetHour: Both buttons pressed
    Waiting --> ClockScreen: Time received from mesh

    ClockScreen --> SensorScreen: Right button
    ClockScreen --> SetHour: Both buttons (300ms)

    SensorScreen --> ClockScreen: Left button
    SensorScreen --> ClockScreen: Right button (wraps)
    SensorScreen --> SetHour: Both buttons (300ms)

    SetHour --> SetMinute: Both buttons
    SetHour --> SetHour: Left/Right button (adjust)

    SetMinute --> ClockScreen: Both buttons (save)
    SetMinute --> SetMinute: Left/Right button (adjust)

    state ClockScreen {
        [*] --> UpdateHands
        UpdateHands --> UpdateDigital: Every second
        UpdateDigital --> UpdateDate: Every minute
        UpdateDate --> UpdateSensors: If data available
        UpdateSensors --> UpdateHands
    }

    state SetHour {
        [*] --> BlinkHour
        BlinkHour --> ShowHour: 500ms
        ShowHour --> BlinkHour: 500ms
    }

    state SetMinute {
        [*] --> BlinkMinute
        BlinkMinute --> ShowMinute: 500ms
        ShowMinute --> BlinkMinute: 500ms
    }
```

## Implementation Details

### Button Handling

Buttons use hardware interrupts for responsive input:

```cpp
void IRAM_ATTR onLeftButtonPress() {
  if (millis() - btnLeftEventTime > BTN_DEBOUNCE_MS) {
    btnLeftEvent = true;
    btnLeftEventTime = millis();
  }
}
```

Both-button detection prevents accidental single-button triggers:
- Events are cleared when both buttons detected
- Single-button events ignored if other button is pressed
- 300ms hold required for both-button actions

### Display Updates

Efficient redraw strategy to avoid flicker:
- **Clock hands:** Only erase/redraw when angle changes
- **Digital time:** Separate functions for hours, minutes, seconds
- **Set time:** Only redraw the field being edited
- **Sensor arcs:** Only update when value changes

### Time Management

Time can come from multiple sources:
1. **Mesh state** - Gateway publishes `time` as Unix timestamp
2. **Manual setting** - Via buttons or `settime` serial command
3. **NTP** - If gateway provides WiFi (not typical for mesh nodes)

Time is stored as Unix timestamp with local offset applied:
```cpp
time_t currentTime = meshTimeBase + elapsed + GMT_OFFSET_SEC + DAYLIGHT_OFFSET;
```

### Mesh Integration

Watches for sensor data from other nodes:
```cpp
swarm.watchState("temp", [](const String& key, const String& value, ...) {
  meshTemp = value;
  hasSensorData = true;
});

swarm.watchState("humid", [](const String& key, const String& value, ...) {
  meshHumid = value;
  hasSensorData = true;
});
```

## Build

```bash
# Build
pio run -e clock

# Upload
pio run -e clock --target upload

# Monitor
pio device monitor -e clock
```

## Configuration

Build-time configuration via `platformio.ini`:

```ini
build_flags =
    -DNODE_NAME=\"Clock\"
    -DMESHSWARM_ENABLE_DISPLAY=0
    -DGMT_OFFSET_SEC=-18000      ; EST = UTC-5
    -DDAYLIGHT_OFFSET=3600       ; DST = +1 hour
```

Pin overrides (if needed):
```ini
build_flags =
    -DTFT_RST=27
    -DTFT_DC=25
    -DTFT_CS=26
    -DBTN_LEFT=32
    -DBTN_RIGHT=33
```

## Dependencies

- DIYables TFT Round library (`diyables/DIYables TFT Round`)
- MeshSwarm library
- painlessMesh
- ArduinoJson

# Mesh Simulator Proposal

**Document**: Mesh Data Simulator for Touch169 Development
**Version**: 1.0
**Date**: December 2025
**Status**: Proposal

## 1. Overview

### 1.1 Purpose

This document proposes a mesh network simulator to support Touch169 development without requiring a full mesh hardware deployment. The simulator provides mock sensor data, state changes, and network events to enable:

- UI development and testing without physical sensors
- Rapid iteration on display layouts and transitions
- Automated testing of edge cases
- Demo capabilities for stakeholders

### 1.2 Problem Statement

Current development requires:
- Multiple ESP32 nodes (PIR, DHT, Light, LED)
- Physical mesh network formation
- Real sensor readings (motion, temperature, humidity, light)

This creates friction for:
- UI/UX iteration (need full hardware setup)
- Testing edge cases (hard to simulate sensor failures)
- Development away from lab environment
- Automated CI/CD testing

## 2. Simulator Architecture

### 2.1 Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Touch169 Firmware                         │
│  ┌─────────────────────────────────────────────────────────┐│
│  │                    MeshSwarm API                         ││
│  │   getState() / watchState() / setState()                ││
│  └─────────────────────────────────────────────────────────┘│
│                           │                                  │
│              ┌────────────┴────────────┐                    │
│              ▼                         ▼                    │
│  ┌─────────────────────┐   ┌─────────────────────┐         │
│  │   Real Mesh Mode    │   │  Simulator Mode     │         │
│  │   (painlessMesh)    │   │  (MockMeshData)     │         │
│  └─────────────────────┘   └─────────────────────┘         │
│                                      │                      │
│                            ┌─────────┴─────────┐           │
│                            ▼                   ▼           │
│                   ┌──────────────┐   ┌──────────────┐      │
│                   │ Static Data  │   │ Dynamic Data │      │
│                   │ (Patterns)   │   │ (Random/Time)│      │
│                   └──────────────┘   └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Operating Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `MESH_MODE_REAL` | Normal mesh operation | Production, integration testing |
| `MESH_MODE_STATIC` | Fixed mock values | UI layout testing, screenshots |
| `MESH_MODE_DYNAMIC` | Time-varying values | Demo mode, behavior testing |
| `MESH_MODE_SCENARIO` | Scripted sequences | Edge case testing, automation |

### 2.3 Build Configuration

```ini
# platformio.ini

[env:touch169]
# Production build - real mesh
build_flags =
    ${env.build_flags}
    -DMESH_MODE=MESH_MODE_REAL

[env:touch169_sim]
# Simulator build - mock data
build_flags =
    ${env.build_flags}
    -DMESH_MODE=MESH_MODE_DYNAMIC
    -DSIM_ENABLE_SERIAL_CONTROL=1

[env:touch169_demo]
# Demo build - attractive cycling values
build_flags =
    ${env.build_flags}
    -DMESH_MODE=MESH_MODE_DYNAMIC
    -DSIM_DEMO_MODE=1
```

## 3. Mock Data Specification

### 3.1 Simulated State Keys

| Key | Real Source | Mock Value Range | Update Frequency |
|-----|-------------|------------------|------------------|
| `temp` | DHT11 node | 18.0 - 28.0 °C | 5 seconds |
| `humidity` | DHT11 node | 30 - 70 % | 5 seconds |
| `motion` | PIR node | "0" / "1" | Random events |
| `lux` | Light node | 0 - 1000 lux | 2 seconds |
| `led` | LED node | "0" / "1" | User triggered |
| `zone_temp` | Multiple DHTs | "living", "bedroom", etc. | Static |
| `zone_motion` | Multiple PIRs | "front", "back", etc. | Static |

### 3.2 Static Mode Values

```cpp
// MockMeshData.h

struct StaticMockData {
    float temperature = 22.5;
    float humidity = 45.0;
    int lux = 350;
    bool motion = false;
    bool led = false;
    String tempZone = "living";
    String motionZone = "front";
};
```

### 3.3 Dynamic Mode Patterns

```cpp
// Time-based patterns for realistic simulation

class DynamicMockData {
public:
    float getTemperature() {
        // Sinusoidal daily pattern: cooler at night, warmer midday
        float hourAngle = (hour() * 15.0) * DEG_TO_RAD;
        float base = 22.0;
        float amplitude = 3.0;
        float noise = random(-50, 50) / 100.0;  // ±0.5°C noise
        return base + amplitude * sin(hourAngle - PI/2) + noise;
    }

    float getHumidity() {
        // Inverse of temperature pattern
        float hourAngle = (hour() * 15.0) * DEG_TO_RAD;
        float base = 50.0;
        float amplitude = 10.0;
        return base - amplitude * sin(hourAngle - PI/2) + random(-2, 2);
    }

    int getLux() {
        // Daylight pattern: 0 at night, peaks at noon
        int hour = hour();
        if (hour < 6 || hour > 20) return random(0, 5);  // Night
        if (hour < 8 || hour > 18) return random(50, 150);  // Dawn/dusk
        return random(300, 800);  // Daytime
    }

    bool getMotion() {
        // Random motion events, more frequent during day
        int hour = hour();
        int probability = (hour >= 8 && hour <= 22) ? 5 : 1;  // % per check
        return random(100) < probability;
    }
};
```

### 3.4 Scenario Mode Scripts

```cpp
// Predefined test scenarios

enum SimScenario {
    SCENARIO_NORMAL,           // Typical day operation
    SCENARIO_HOT_DAY,          // High temperature values
    SCENARIO_MOTION_STORM,     // Rapid motion triggers
    SCENARIO_SENSOR_DROPOUT,   // Sensors go offline
    SCENARIO_ZONE_SWITCH,      // Cycle through zones
    SCENARIO_NIGHT_MODE,       // Low light, no motion
};

class ScenarioRunner {
public:
    void runScenario(SimScenario scenario);
    void advanceStep();
    bool isComplete();

private:
    struct ScenarioStep {
        unsigned long delayMs;
        String stateKey;
        String stateValue;
    };

    std::vector<ScenarioStep> steps;
    size_t currentStep = 0;
};

// Example: Motion storm scenario
const ScenarioStep motionStormSteps[] = {
    {0,    "motion", "1"},
    {500,  "motion", "0"},
    {200,  "motion", "1"},
    {300,  "motion", "0"},
    {100,  "motion", "1"},
    {1000, "motion", "0"},
    {50,   "motion", "1"},
    {50,   "motion", "0"},
    // ... rapid fire motion events
};
```

## 4. Implementation

### 4.1 MockMeshData Class

```cpp
// lib/MockMeshData/MockMeshData.h

#pragma once

#include <Arduino.h>
#include <map>
#include <functional>

#ifndef MESH_MODE
#define MESH_MODE MESH_MODE_REAL
#endif

#define MESH_MODE_REAL     0
#define MESH_MODE_STATIC   1
#define MESH_MODE_DYNAMIC  2
#define MESH_MODE_SCENARIO 3

class MockMeshData {
public:
    static MockMeshData& getInstance();

    // Initialize simulator
    void begin();

    // Update mock values (call in loop)
    void update();

    // Get current mock state value
    String getState(const String& key);

    // Set mock state (for serial control)
    void setState(const String& key, const String& value);

    // Register state change callback (mirrors MeshSwarm API)
    void watchState(const String& key,
        std::function<void(const String&, const String&, const String&)> callback);

    // Scenario control
    void loadScenario(SimScenario scenario);
    void startScenario();
    void stopScenario();

    // Zone simulation
    void setActiveZone(const String& sensorType, const String& zoneName);
    String getActiveZone(const String& sensorType);
    std::vector<String> getAvailableZones(const String& sensorType);

private:
    MockMeshData() = default;

    std::map<String, String> mockState;
    std::map<String, std::vector<std::function<void(const String&, const String&, const String&)>>> watchers;

    unsigned long lastTempUpdate = 0;
    unsigned long lastLuxUpdate = 0;
    unsigned long lastMotionCheck = 0;

    void updateDynamicValues();
    void notifyWatchers(const String& key, const String& newValue, const String& oldValue);
};
```

### 4.2 Integration with MeshSwarm

```cpp
// MeshSwarm.cpp modifications (or wrapper)

#if MESH_MODE != MESH_MODE_REAL

#include "MockMeshData.h"

String MeshSwarm::getState(const String& key) {
    return MockMeshData::getInstance().getState(key);
}

void MeshSwarm::watchState(const String& key, StateCallback callback) {
    MockMeshData::getInstance().watchState(key, callback);
}

void MeshSwarm::update() {
    MockMeshData::getInstance().update();
    // ... rest of update logic (display, touch, etc.)
}

#endif
```

### 4.3 Serial Control Interface

```cpp
// Enable runtime control of mock data via serial

void processSerialCommand(const String& cmd) {
    if (cmd.startsWith("sim set ")) {
        // sim set temp 25.5
        String rest = cmd.substring(8);
        int spaceIdx = rest.indexOf(' ');
        String key = rest.substring(0, spaceIdx);
        String value = rest.substring(spaceIdx + 1);
        MockMeshData::getInstance().setState(key, value);
        Serial.printf("Mock: %s = %s\n", key.c_str(), value.c_str());
    }
    else if (cmd.startsWith("sim scenario ")) {
        // sim scenario motion_storm
        String name = cmd.substring(13);
        // Map name to enum and load
    }
    else if (cmd == "sim status") {
        // Print all current mock values
    }
    else if (cmd == "sim mode static") {
        // Switch to static mode
    }
    else if (cmd == "sim mode dynamic") {
        // Switch to dynamic mode
    }
}
```

## 5. Zone Simulation

### 5.1 Multi-Zone Mock Data

```cpp
// Simulate multiple sensor zones

struct ZoneConfig {
    String name;
    String displayName;
    float tempOffset;      // Offset from base temperature
    float humidityOffset;  // Offset from base humidity
    int luxMultiplier;     // Multiplier for light (100 = 1x)
    int motionProbability; // Motion probability modifier
};

const ZoneConfig temperatureZones[] = {
    {"living",   "Living Room",  0.0,  0.0, 100, 100},
    {"bedroom",  "Bedroom",     -1.5,  5.0,  50,  30},
    {"kitchen",  "Kitchen",      2.0, 10.0, 120, 150},
    {"garage",   "Garage",      -3.0, -5.0,  80,  20},
    {"outdoor",  "Outdoor",      5.0,-10.0, 200,  50},
};

const ZoneConfig motionZones[] = {
    {"front",    "Front Door",   0, 0, 100, 100},
    {"back",     "Back Door",    0, 0, 100,  80},
    {"hallway",  "Hallway",      0, 0, 100, 200},
    {"garage",   "Garage",       0, 0, 100,  40},
};
```

### 5.2 Zone Switching UI Test

The simulator enables testing zone switching without multiple physical nodes:

```cpp
// Test zone switch flow
void testZoneSwitching() {
    MockMeshData& mock = MockMeshData::getInstance();

    // Simulate user switching temperature zone
    mock.setActiveZone("temp", "bedroom");
    // UI should update to show "Bedroom" and adjusted values

    delay(2000);

    mock.setActiveZone("temp", "kitchen");
    // UI should update to show "Kitchen" with higher temp
}
```

## 6. Testing Scenarios

### 6.1 Automated Test Cases

| Test | Scenario | Expected Behavior |
|------|----------|-------------------|
| T1 | Static display | All values render correctly |
| T2 | Value update | Display updates when mock changes |
| T3 | Motion trigger | LED icon responds to motion=1 |
| T4 | Zone switch | Zone name and values update |
| T5 | Extreme values | Display handles temp=99.9, lux=9999 |
| T6 | Rapid updates | No display flicker or crashes |
| T7 | Sensor offline | Graceful handling of missing data |

### 6.2 Demo Sequences

```cpp
// Attractive demo for stakeholders

void runDemoSequence() {
    // Cycle through features every 5 seconds

    // 1. Show main clock with live sensor corners
    showScreen(SCREEN_CLOCK);
    delay(5000);

    // 2. Tap humidity - show detail
    simulateTouch(20, 20);  // Top-left
    delay(3000);

    // 3. Show zone switching
    mock.setActiveZone("humidity", "bedroom");
    delay(2000);
    mock.setActiveZone("humidity", "kitchen");
    delay(2000);

    // 4. Return to clock, show motion trigger
    navigateBack();
    delay(1000);
    mock.setState("motion", "1");
    delay(2000);
    mock.setState("motion", "0");

    // 5. Open calendar
    simulateTouch(120, 20);  // Top-center date
    delay(3000);

    // ... continue demo
}
```

## 7. Development Workflow

### 7.1 Local Development

```bash
# Build simulator version
pio run -e touch169_sim

# Upload and monitor
pio run -e touch169_sim --target upload && pio device monitor

# In serial monitor, control mock data:
# > sim set temp 30.5
# > sim set motion 1
# > sim scenario hot_day
# > sim status
```

### 7.2 CI/CD Integration

```yaml
# .github/workflows/test.yml

jobs:
  firmware-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build simulator firmware
        run: pio run -e touch169_sim

      - name: Run scenario tests
        run: |
          # Use QEMU or hardware-in-loop testing
          # Run predefined scenarios and validate outputs
```

## 8. File Structure

```
firmware/
├── lib/
│   └── MockMeshData/
│       ├── MockMeshData.h
│       ├── MockMeshData.cpp
│       ├── DynamicPatterns.h
│       ├── ScenarioRunner.h
│       ├── ScenarioRunner.cpp
│       └── scenarios/
│           ├── normal_day.h
│           ├── hot_day.h
│           ├── motion_storm.h
│           └── sensor_dropout.h
├── nodes/
│   └── touch169/
│       └── main.cpp  # Uses MockMeshData when MESH_MODE != REAL
└── platformio.ini    # Defines touch169_sim environment
```

## 9. Implementation Phases

### Phase 1: Basic Mock (Week 1)
- [ ] Create MockMeshData class with static values
- [ ] Add build flag for simulator mode
- [ ] Integrate with touch169 main.cpp
- [ ] Test basic display with mock data

### Phase 2: Dynamic Patterns (Week 2)
- [ ] Add time-based temperature/humidity patterns
- [ ] Add daylight-aware lux simulation
- [ ] Add random motion events
- [ ] Serial control for manual overrides

### Phase 3: Zone Support (Week 3)
- [ ] Define zone configurations
- [ ] Implement zone switching in mock
- [ ] Add zone-specific value offsets
- [ ] Test zone UI flows

### Phase 4: Scenarios & Demo (Week 4)
- [ ] Create ScenarioRunner class
- [ ] Define test scenarios
- [ ] Build demo sequence
- [ ] Document all serial commands

## 10. Future Enhancements

### 10.1 Web-Based Control Panel

A simple web UI to control mock data remotely:

```
┌─────────────────────────────────────────┐
│  Mesh Simulator Control Panel           │
├─────────────────────────────────────────┤
│  Temperature: [====|====] 22.5°C        │
│  Humidity:    [====|====] 45%           │
│  Light:       [====|====] 350 lux       │
│  Motion:      [ Off ] [ Trigger ]       │
│  LED:         [ Off ] [ On ]            │
├─────────────────────────────────────────┤
│  Zone: [Living Room ▼]                  │
├─────────────────────────────────────────┤
│  Scenarios:                             │
│  [Normal Day] [Hot Day] [Night Mode]    │
└─────────────────────────────────────────┘
```

### 10.2 Record and Playback

Record real mesh data and play it back:

```cpp
// Record actual mesh traffic
void startRecording(const String& filename);
void stopRecording();

// Playback recorded data
void playRecording(const String& filename);
```

### 10.3 Hardware-in-Loop Testing

Connect simulator to real mesh via gateway:

```
[Touch169 Sim] <--serial--> [Test PC] <--HTTP--> [Gateway] <--mesh--> [Real Nodes]
```

## 11. Summary

The Mesh Simulator enables efficient Touch169 development by:

1. **Eliminating hardware dependency** for UI work
2. **Enabling edge case testing** with scripted scenarios
3. **Supporting demos** with attractive data patterns
4. **Facilitating CI/CD** with automated test scenarios
5. **Accelerating iteration** with serial control

Integration is transparent - the same UI code works with both real mesh and simulator, controlled by build flags.

---

**Next Steps:**
1. Review and approve this proposal
2. Create MockMeshData library structure
3. Add touch169_sim environment to platformio.ini
4. Implement Phase 1 basic mock

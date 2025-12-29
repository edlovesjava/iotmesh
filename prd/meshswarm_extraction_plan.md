# MeshSwarm Library Extraction Plan

DONE

Plan for extracting MeshSwarm as an independent, publicly distributable Arduino library.

## Goal

Create a standalone `MeshSwarm` library repository that can be:
- Installed via Arduino Library Manager
- Used independently of the iotmesh project
- Published to PlatformIO registry

## Current State

- Library code: `MeshSwarm/src/MeshSwarm.h`, `MeshSwarm/src/MeshSwarm.cpp`
- Metadata: `MeshSwarm/library.properties`
- README: `MeshSwarm/README.md`
- Examples: Embedded in parent sketches (mesh_shared_state_*)

## Extraction Steps

### Phase 1: Repository Setup

1. Create new GitHub repository: `MeshSwarm`
2. Initialize with MIT license
3. Copy library structure:
   ```
   MeshSwarm/
   ├── src/
   │   ├── MeshSwarm.h
   │   └── MeshSwarm.cpp
   ├── examples/
   │   ├── BasicNode/
   │   │   └── BasicNode.ino
   │   ├── ButtonNode/
   │   │   └── ButtonNode.ino
   │   ├── LedNode/
   │   │   └── LedNode.ino
   │   ├── SensorNode/
   │   │   └── SensorNode.ino
   │   └── WatcherNode/
   │       └── WatcherNode.ino
   ├── library.properties
   ├── keywords.txt
   ├── README.md
   └── LICENSE
   ```

### Phase 2: Create Example Sketches

Extract minimal examples from existing sketches:

#### examples/BasicNode/BasicNode.ino
```cpp
// Minimal mesh node - join network and sync state
#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin();
}

void loop() {
  swarm.update();
}
```

#### examples/ButtonNode/ButtonNode.ino
```cpp
// Button input node - toggles shared "led" state
#include <MeshSwarm.h>

#define BUTTON_PIN 0

MeshSwarm swarm;
bool lastButton = HIGH;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  swarm.begin();

  swarm.onLoop([]() {
    bool button = digitalRead(BUTTON_PIN);
    if (button == LOW && lastButton == HIGH) {
      String current = swarm.getState("led", "0");
      swarm.setState("led", current == "1" ? "0" : "1");
    }
    lastButton = button;
  });
}

void loop() {
  swarm.update();
}
```

#### examples/LedNode/LedNode.ino
```cpp
// LED output node - reacts to shared "led" state
#include <MeshSwarm.h>

#define LED_PIN 2

MeshSwarm swarm;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  swarm.begin();

  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    digitalWrite(LED_PIN, value == "1" ? HIGH : LOW);
  });
}

void loop() {
  swarm.update();
}
```

#### examples/SensorNode/SensorNode.ino
```cpp
// Sensor node - publishes readings to mesh
#include <MeshSwarm.h>

#define SENSOR_PIN 34

MeshSwarm swarm;

void setup() {
  swarm.begin();

  swarm.onLoop([]() {
    static unsigned long lastRead = 0;
    if (millis() - lastRead > 5000) {
      lastRead = millis();
      int value = analogRead(SENSOR_PIN);
      swarm.setState("sensor", String(value));
    }
  });
}

void loop() {
  swarm.update();
}
```

#### examples/WatcherNode/WatcherNode.ino
```cpp
// Observer node - monitors all state changes
#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  swarm.begin();

  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    Serial.printf("[STATE] %s: %s -> %s\n", key.c_str(), oldValue.c_str(), value.c_str());
  });
}

void loop() {
  swarm.update();
}
```

### Phase 3: Create keywords.txt

For Arduino IDE syntax highlighting:

```
# MeshSwarm keywords

# Classes (KEYWORD1)
MeshSwarm	KEYWORD1
StateEntry	KEYWORD1
Peer	KEYWORD1

# Methods (KEYWORD2)
begin	KEYWORD2
update	KEYWORD2
setState	KEYWORD2
getState	KEYWORD2
watchState	KEYWORD2
broadcastFullState	KEYWORD2
requestStateSync	KEYWORD2
getNodeId	KEYWORD2
getNodeName	KEYWORD2
getRole	KEYWORD2
isCoordinator	KEYWORD2
getPeerCount	KEYWORD2
getPeers	KEYWORD2
getMesh	KEYWORD2
getDisplay	KEYWORD2
onLoop	KEYWORD2
onSerialCommand	KEYWORD2
onDisplayUpdate	KEYWORD2
setStatusLine	KEYWORD2
setHeartbeatData	KEYWORD2

# Constants (LITERAL1)
MSG_HEARTBEAT	LITERAL1
MSG_STATE_SET	LITERAL1
MSG_STATE_SYNC	LITERAL1
MSG_STATE_REQ	LITERAL1
MSG_COMMAND	LITERAL1
```

### Phase 4: Update library.properties

```properties
name=MeshSwarm
version=1.0.0
author=edlovesjava
maintainer=edlovesjava
sentence=Self-organizing ESP32 mesh network with distributed shared state
paragraph=Build mesh networks where nodes automatically discover each other, elect coordinators, and synchronize key-value state. Features state watchers, conflict resolution, OLED display support, and serial command interface.
category=Communication
url=https://github.com/edlovesjava/MeshSwarm
architectures=esp32
depends=painlessMesh,ArduinoJson,Adafruit SSD1306,Adafruit GFX Library
includes=MeshSwarm.h
```

### Phase 5: Code Improvements for Public Release

1. **Add header guards and documentation**
   - Ensure all public methods have doc comments
   - Add @param and @return annotations

2. **Make OLED optional**
   - Add `#define MESHSWARM_NO_DISPLAY` option
   - Conditionally compile display code

3. **Error handling**
   - Add return codes or error callbacks
   - Handle mesh initialization failures gracefully

4. **Configuration validation**
   - Validate mesh credentials length
   - Check I2C address validity

5. **Memory optimization**
   - Review String usage, consider char arrays for fixed strings
   - Add state entry limit configuration

### Phase 6: Testing

1. **Compile tests**
   - Verify all examples compile with Arduino IDE
   - Test with PlatformIO

2. **Integration tests**
   - 2-node basic connectivity
   - 3+ node state sync
   - Coordinator failover
   - State conflict resolution

3. **Hardware matrix**
   - ESP32 Dev Module
   - ESP32-S2
   - ESP32-S3
   - ESP32-C3 (if supported by painlessMesh)

### Phase 7: Documentation

1. **README.md** (done)
2. **CHANGELOG.md** - Version history
3. **CONTRIBUTING.md** - Contribution guidelines
4. **API documentation** - Consider Doxygen
5. **Wiring diagrams** - Fritzing or similar

### Phase 8: Publication

1. **GitHub Release**
   - Tag v1.0.0
   - Create release with zip archive
   - Add release notes

2. **Arduino Library Manager**
   - Submit to arduino/library-registry
   - Requires: valid library.properties, examples/, correct folder structure

3. **PlatformIO Registry**
   - Create library.json
   - Register with PlatformIO

## File Checklist

| File | Status | Notes |
|------|--------|-------|
| src/MeshSwarm.h | Exists | Review documentation |
| src/MeshSwarm.cpp | Exists | Review error handling |
| library.properties | Exists | Update for standalone |
| README.md | Exists | Complete |
| LICENSE | Create | MIT |
| keywords.txt | Create | For syntax highlighting |
| examples/BasicNode/ | Create | From mesh_swarm_base |
| examples/ButtonNode/ | Create | From mesh_shared_state_button |
| examples/LedNode/ | Create | From mesh_shared_state_led |
| examples/SensorNode/ | Create | New generic example |
| examples/WatcherNode/ | Create | From mesh_shared_state_watcher |
| CHANGELOG.md | Create | Version history |

## Future Enhancements

After initial release:

1. **ESP-NOW support** - Alternative to WiFi mesh for lower latency
2. **Encryption** - End-to-end state encryption
3. **Persistence** - SPIFFS/LittleFS state backup
4. **Web interface** - Optional HTTP API for state access
5. **MQTT bridge** - Connect mesh to MQTT broker
6. **OTA updates** - Coordinate firmware updates across mesh

# IoTMesh Project Roadmap

Comprehensive plan for evolving the iotmesh project with telemetry, OTA updates, and library publication.

## Overview

Three major initiatives:
1. **Telemetry Platform** - Web dashboard and API for collecting/visualizing mesh data
2. **OTA Updates** - Over-the-air firmware updates coordinated across the mesh
3. **Library Publication** - Extract and publish MeshSwarm as standalone Arduino library

---

## 1. Telemetry Platform

### Goals
- Centralized dashboard showing all mesh nodes and their state
- REST API for nodes to push telemetry data
- Historical data storage and visualization
- Alerting on state changes or node failures

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Telemetry Server                        │
├─────────────┬─────────────┬─────────────┬──────────────────┤
│   REST API  │  WebSocket  │   Web UI    │   Database       │
│   (ingest)  │  (realtime) │  (dashboard)│   (TimescaleDB)  │
└──────▲──────┴──────┬──────┴─────────────┴──────────────────┘
       │             │
       │ HTTP POST   │ WS push
       │             ▼
┌──────┴─────────────────────────────────────────────────────┐
│                      WiFi Network                           │
└──────▲──────────────▲──────────────▲──────────────▲────────┘
       │              │              │              │
   ┌───┴───┐      ┌───┴───┐      ┌───┴───┐      ┌───┴───┐
   │ ESP32 │◄────►│ ESP32 │◄────►│ ESP32 │◄────►│ ESP32 │
   │ Node  │ mesh │ Node  │ mesh │ Node  │ mesh │ Node  │
   └───────┘      └───────┘      └───────┘      └───────┘
```

### Phase 1.1: REST API Server

**Tech Stack Options:**

| Option | Pros | Cons |
|--------|------|------|
| Node.js + Express | Fast dev, JS everywhere | Memory usage |
| Python + FastAPI | Easy, async, auto-docs | Slower than Go |
| Go + Gin | Fast, low memory, single binary | More verbose |
| Rust + Actix | Fastest, safest | Steeper learning curve |

**Recommended: Python + FastAPI** (quick iteration, good for IoT prototyping)

**API Endpoints:**

```
POST /api/v1/nodes/{node_id}/telemetry
  Body: { "state": {...}, "peers": [...], "heap": 12345, "uptime": 3600 }

GET  /api/v1/nodes
  Response: List of all known nodes

GET  /api/v1/nodes/{node_id}
  Response: Node details and recent telemetry

GET  /api/v1/nodes/{node_id}/history?from=&to=&keys=
  Response: Historical state values

POST /api/v1/nodes/{node_id}/command
  Body: { "command": "reboot" }
  (Queue command for node to pick up)

GET  /api/v1/state
  Response: Current merged state from all nodes

WebSocket /ws/live
  Real-time state updates pushed to dashboard
```

**Data Model:**

```sql
-- Nodes table
CREATE TABLE nodes (
  id VARCHAR(10) PRIMARY KEY,  -- Mesh node ID (hex)
  name VARCHAR(50),
  first_seen TIMESTAMP,
  last_seen TIMESTAMP,
  firmware_version VARCHAR(20),
  ip_address INET
);

-- Telemetry time-series (TimescaleDB hypertable)
CREATE TABLE telemetry (
  time TIMESTAMP NOT NULL,
  node_id VARCHAR(10),
  key VARCHAR(50),
  value TEXT,
  version INT
);
SELECT create_hypertable('telemetry', 'time');

-- State snapshots
CREATE TABLE state_snapshots (
  time TIMESTAMP NOT NULL,
  node_id VARCHAR(10),
  state JSONB,
  heap_free INT,
  uptime_sec INT,
  peer_count INT
);
```

**Tasks:**
- [ ] Set up FastAPI project structure
- [ ] Implement node registration endpoint
- [ ] Implement telemetry ingestion endpoint
- [ ] Add TimescaleDB for time-series storage
- [ ] Implement query endpoints with filtering
- [ ] Add WebSocket for real-time updates
- [ ] Add authentication (API keys per node)
- [ ] Dockerize the server

### Phase 1.2: ESP32 Telemetry Client

Add telemetry reporting to MeshSwarm library.

**New MeshSwarm Methods:**

```cpp
// Configuration
void setTelemetryServer(const char* url, const char* apiKey);
void setTelemetryInterval(unsigned long ms);  // Default: 30000

// Control
void enableTelemetry(bool enable);
bool isTelemetryEnabled();

// Manual push (auto-push happens on interval)
void pushTelemetry();
```

**Implementation Approach:**

```cpp
// In MeshSwarm.cpp
void MeshSwarm::pushTelemetry() {
  if (!telemetryEnabled || telemetryUrl.isEmpty()) return;

  // Only coordinator pushes to avoid duplicates
  // Or: each node pushes its own data

  HTTPClient http;
  http.begin(telemetryUrl + "/api/v1/nodes/" + String(myId, HEX) + "/telemetry");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", telemetryApiKey);

  StaticJsonDocument<1024> doc;
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["peers"] = getPeerCount();
  doc["role"] = myRole;

  JsonObject state = doc.createNestedObject("state");
  for (auto& entry : sharedState) {
    state[entry.first] = entry.second.value;
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  http.end();
}
```

**Tasks:**
- [ ] Add HTTPClient integration to MeshSwarm
- [ ] Implement telemetry configuration methods
- [ ] Add periodic telemetry push in update()
- [ ] Handle WiFi station mode alongside mesh
- [ ] Add telemetry example sketch
- [ ] Test with mock server

### Phase 1.3: Web Dashboard

**Tech Stack:** React + Vite + TailwindCSS + Recharts

**Dashboard Features:**
- Node list with status indicators (online/offline)
- Network topology visualization (mesh graph)
- Real-time state table
- Historical charts for numeric values
- Node detail view (uptime, heap, peers, firmware)
- Alert configuration and history

**Pages:**
- `/` - Dashboard overview with node grid
- `/nodes` - Node list table with sorting/filtering
- `/nodes/:id` - Node detail page with history
- `/state` - Live state table with all key-values
- `/graph` - Mesh topology visualization
- `/alerts` - Alert rules and history
- `/settings` - Server configuration

**Tasks:**
- [ ] Set up React + Vite project
- [ ] Create node grid component with status badges
- [ ] Implement real-time WebSocket updates
- [ ] Add state table with live updates
- [ ] Create time-series charts with Recharts
- [ ] Build mesh topology graph (D3.js or vis.js)
- [ ] Add responsive mobile layout
- [ ] Deploy with Docker/nginx

---

## 2. OTA Updates

### Goals
- Update firmware on ESP32 nodes without physical access
- Coordinate updates across mesh (rolling update)
- Rollback capability on failure
- Version tracking and update history

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     OTA Server                              │
├─────────────────────────────────────────────────────────────┤
│  - Firmware storage (.bin files)                            │
│  - Version management                                       │
│  - Update scheduling                                        │
│  - Rollout coordination                                     │
└──────────────────────────▲──────────────────────────────────┘
                           │
                    HTTP GET /firmware
                           │
┌──────────────────────────┴──────────────────────────────────┐
│                      Mesh Network                           │
├─────────────────────────────────────────────────────────────┤
│  Coordinator checks for updates, notifies nodes             │
│  Nodes download individually to avoid mesh disruption       │
│  Rolling update: one node at a time                         │
└─────────────────────────────────────────────────────────────┘
```

### Phase 2.1: OTA Server Endpoints

Extend telemetry server or create separate service.

**API Endpoints:**

```
GET  /api/v1/firmware/latest?board=esp32
  Response: { "version": "1.2.0", "url": "/firmware/esp32/1.2.0.bin", "checksum": "sha256:..." }

GET  /api/v1/firmware/{version}.bin
  Response: Binary firmware file

POST /api/v1/firmware/upload
  Body: multipart form with .bin file and metadata

GET  /api/v1/nodes/{node_id}/update-status
  Response: { "current": "1.1.0", "available": "1.2.0", "status": "pending" }

POST /api/v1/rollout
  Body: { "version": "1.2.0", "strategy": "rolling", "batch_size": 1 }
```

**Tasks:**
- [ ] Add firmware storage (S3 or local filesystem)
- [ ] Implement version comparison logic
- [ ] Add firmware upload endpoint with validation
- [ ] Create rollout scheduling system
- [ ] Track update status per node

### Phase 2.2: ESP32 OTA Client

**New MeshSwarm Methods:**

```cpp
// Configuration
void setOtaServer(const char* url);
void setOtaCheckInterval(unsigned long ms);  // Default: 3600000 (1 hour)

// Control
void checkForUpdate();  // Manual check
bool updateAvailable();
String getAvailableVersion();
void performUpdate();   // Download and flash

// Callbacks
void onUpdateAvailable(std::function<void(String version)> callback);
void onUpdateProgress(std::function<void(int percent)> callback);
void onUpdateComplete(std::function<void(bool success)> callback);
```

**Implementation using ESP32 OTA:**

```cpp
#include <HTTPUpdate.h>

void MeshSwarm::performUpdate() {
  if (!updateAvailable()) return;

  // Notify mesh we're updating
  broadcastMessage(MSG_COMMAND, "updating", currentVersion);

  WiFiClient client;
  t_httpUpdate_return ret = httpUpdate.update(client, otaFirmwareUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update failed: %s\n", httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No update available");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("Update successful, rebooting...");
      ESP.restart();
      break;
  }
}
```

**Rolling Update Coordination:**

```cpp
// Coordinator manages update sequence
void MeshSwarm::coordinateRollout() {
  if (!isCoordinator()) return;

  // Get list of nodes needing update
  // Update one at a time
  // Wait for node to rejoin mesh
  // Verify new version
  // Continue to next node
}
```

**Tasks:**
- [ ] Integrate ESP32 HTTPUpdate library
- [ ] Add version checking logic
- [ ] Implement update progress tracking
- [ ] Add pre-update state backup
- [ ] Implement coordinator-based rolling updates
- [ ] Add rollback on consecutive boot failures
- [ ] Create OTA example sketch

### Phase 2.3: Firmware Versioning

**Version Scheme:** Semantic versioning (MAJOR.MINOR.PATCH)

**Build Process:**
```bash
# platformio.ini
[env:esp32]
build_flags =
  -DFIRMWARE_VERSION=\"1.2.0\"
  -DBUILD_TIME=\"${UNIX_TIME}\"
```

**In Code:**
```cpp
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif

const char* MeshSwarm::getFirmwareVersion() {
  return FIRMWARE_VERSION;
}
```

**Tasks:**
- [ ] Add version define to all sketches
- [ ] Create build script to inject version
- [ ] Add version to telemetry payload
- [ ] Display version on OLED

---

## 3. Repository Refactoring

### Goals
- Publish MeshSwarm as standalone Arduino library
- Keep iotmesh as example/application repository
- Enable others to use MeshSwarm in their projects

### New Repository Structure

**Repository 1: MeshSwarm (library)**
```
MeshSwarm/
├── src/
│   ├── MeshSwarm.h
│   └── MeshSwarm.cpp
├── examples/
│   ├── BasicNode/
│   ├── ButtonNode/
│   ├── LedNode/
│   ├── SensorNode/
│   ├── WatcherNode/
│   └── TelemetryNode/
├── library.properties
├── keywords.txt
├── README.md
├── LICENSE
├── CHANGELOG.md
└── CONTRIBUTING.md
```

**Repository 2: iotmesh (application)**
```
iotmesh/
├── firmware/
│   ├── button_node/
│   ├── led_node/
│   ├── pir_node/
│   ├── dht11_node/
│   └── gateway_node/
├── server/
│   ├── api/
│   ├── dashboard/
│   └── docker-compose.yml
├── hardware/
│   ├── schematics/
│   └── 3d-prints/
├── docs/
│   ├── setup-guide.md
│   ├── api-reference.md
│   └── architecture.md
├── README.md
└── LICENSE
```

### Phase 3.1: Library Extraction

**Tasks:**
- [ ] Create new GitHub repository: `MeshSwarm`
- [ ] Copy library files (src/, library.properties, README.md)
- [ ] Create example sketches from existing mesh_shared_state_*
- [ ] Add keywords.txt for syntax highlighting
- [ ] Add MIT LICENSE file
- [ ] Create CHANGELOG.md
- [ ] Tag v1.0.0 release
- [ ] Submit to Arduino Library Manager registry
- [ ] Register with PlatformIO

### Phase 3.2: Application Repository Refactor

**Tasks:**
- [ ] Restructure iotmesh with firmware/ and server/ directories
- [ ] Update sketches to use published MeshSwarm library
- [ ] Add platformio.ini for easier building
- [ ] Create server/ with telemetry API
- [ ] Create dashboard/ with web UI
- [ ] Add docker-compose.yml for easy deployment
- [ ] Update documentation for new structure

### Phase 3.3: CI/CD Pipeline

**GitHub Actions for MeshSwarm:**

```yaml
# .github/workflows/build.yml
name: Build Examples

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: arduino/compile-sketches@v1
        with:
          fqbn: esp32:esp32:esp32
          sketch-paths: |
            - examples/BasicNode
            - examples/ButtonNode
            - examples/LedNode
          libraries: |
            - source-path: ./
            - name: painlessMesh
            - name: ArduinoJson
            - name: Adafruit SSD1306
            - name: Adafruit GFX Library
```

**GitHub Actions for iotmesh:**

```yaml
# .github/workflows/build.yml
name: Build and Test

on: [push, pull_request]

jobs:
  firmware:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/cache@v3
        with:
          path: ~/.platformio
          key: ${{ runner.os }}-pio
      - uses: actions/setup-python@v4
        with:
          python-version: '3.x'
      - run: pip install platformio
      - run: cd firmware && pio run

  server:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - run: cd server/api && pip install -r requirements.txt
      - run: cd server/api && pytest

  dashboard:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-node@v3
        with:
          node-version: '18'
      - run: cd server/dashboard && npm ci
      - run: cd server/dashboard && npm run build
```

**Tasks:**
- [ ] Set up GitHub Actions for MeshSwarm library
- [ ] Set up GitHub Actions for iotmesh application
- [ ] Add automated releases on version tags
- [ ] Configure PlatformIO CI
- [ ] Add code coverage for server

---

## Implementation Timeline

### Milestone 1: Foundation
- [ ] Extract and publish MeshSwarm library v1.0.0
- [ ] Basic telemetry API server (FastAPI)
- [ ] ESP32 telemetry client in MeshSwarm

### Milestone 2: Dashboard
- [ ] Web dashboard with node list and state view
- [ ] Real-time WebSocket updates
- [ ] Historical data charts

### Milestone 3: OTA
- [ ] OTA server with firmware storage
- [ ] ESP32 OTA client in MeshSwarm
- [ ] Rolling update coordination

### Milestone 4: Production Ready
- [ ] Authentication and security
- [ ] Docker deployment
- [ ] Documentation and tutorials
- [ ] CI/CD pipelines

---

## Open Questions

1. **Telemetry transport**: HTTP polling vs MQTT vs WebSocket from nodes?
2. **Mesh + Station mode**: Can ESP32 maintain mesh while connecting to WiFi AP?
3. **Database choice**: TimescaleDB vs InfluxDB vs SQLite for small deployments?
4. **Update strategy**: Coordinator-orchestrated vs each-node-independent?
5. **Security**: How to secure OTA without adding too much complexity?

---

## References

- [painlessMesh documentation](https://gitlab.com/painlessMesh/painlessMesh)
- [ESP32 OTA Updates](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [Arduino Library Specification](https://arduino.github.io/arduino-cli/latest/library-specification/)
- [FastAPI documentation](https://fastapi.tiangolo.com/)
- [TimescaleDB](https://www.timescale.com/)

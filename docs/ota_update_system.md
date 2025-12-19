# OTA Update System Implementation Plan

## Overview

Server-push OTA updates via gateway distribution to mesh nodes, with per-node-type firmware binaries.

## Architecture

```
Server (FastAPI)          Gateway (ESP32)           Mesh Nodes
┌─────────────────┐      ┌─────────────────┐      ┌──────────────┐
│ firmware table  │      │ Poll /pending   │      │ PIR node     │
│ ota_updates     │─────>│ Download binary │      │ role="pir"   │
│ ota_node_status │      │ initOTASend()   │─────>│ initOTARecv()│
└─────────────────┘      │ offerOTA()      │      └──────────────┘
                         └─────────────────┘
```

### Flow

1. Admin uploads firmware binary to server via API
2. Admin creates OTA update job targeting node type (e.g., "pir")
3. Gateway polls `/api/v1/ota/updates/pending` every 60 seconds
4. Gateway downloads firmware binary to memory
5. Gateway uses painlessMesh OTA to distribute to mesh nodes
6. Nodes with matching role receive and apply update
7. Gateway reports progress per node back to server

## Implementation Phases

### Phase 1: Server Firmware Storage (MVP)

**Goal**: Upload, store, and download firmware binaries

#### Database Schema

Add to `server/init.sql`:

```sql
CREATE TABLE firmware (
    id SERIAL PRIMARY KEY,
    node_type VARCHAR(30) NOT NULL,      -- pir, led, button, gateway, etc.
    version VARCHAR(20) NOT NULL,
    hardware VARCHAR(10) DEFAULT 'ESP32',
    filename VARCHAR(100) NOT NULL,
    size_bytes INTEGER NOT NULL,
    md5_hash VARCHAR(32) NOT NULL,
    binary_data BYTEA NOT NULL,
    release_notes TEXT,
    is_stable BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    UNIQUE(node_type, version, hardware)
);
```

#### New Files

- `server/api/app/models/firmware.py` - SQLAlchemy model
- `server/api/app/schemas/firmware.py` - Pydantic schemas
- `server/api/app/routers/firmware.py` - API endpoints

#### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/firmware/upload` | POST | Upload firmware binary (multipart form) |
| `/api/v1/firmware` | GET | List all firmware versions |
| `/api/v1/firmware/{id}` | GET | Get firmware metadata |
| `/api/v1/firmware/{id}/download` | GET | Download binary |
| `/api/v1/firmware/{id}` | DELETE | Delete firmware |

---

### Phase 2: OTA Update Jobs & Gateway Polling

**Goal**: Server can create update jobs, gateway polls and downloads

#### Database Schema

Add to `server/init.sql`:

```sql
CREATE TABLE ota_updates (
    id SERIAL PRIMARY KEY,
    firmware_id INTEGER REFERENCES firmware(id),
    target_node_id VARCHAR(10),          -- NULL = all nodes of type
    target_node_type VARCHAR(30),
    status VARCHAR(20) DEFAULT 'pending', -- pending/distributing/completed/failed
    force_update BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ
);

CREATE TABLE ota_node_status (
    id SERIAL PRIMARY KEY,
    update_id INTEGER REFERENCES ota_updates(id) ON DELETE CASCADE,
    node_id VARCHAR(10) NOT NULL,
    status VARCHAR(20) DEFAULT 'pending',
    current_part INTEGER DEFAULT 0,
    total_parts INTEGER,
    error_message TEXT,
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    UNIQUE(update_id, node_id)
);

CREATE INDEX idx_firmware_type_version ON firmware(node_type, version DESC);
CREATE INDEX idx_ota_updates_status ON ota_updates(status);
CREATE INDEX idx_ota_node_status_update ON ota_node_status(update_id);
```

#### New Files

- `server/api/app/models/ota.py` - OTAUpdate, OTANodeStatus models
- `server/api/app/schemas/ota.py` - Pydantic schemas
- `server/api/app/routers/ota.py` - API endpoints

#### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/ota/updates` | POST | Create update job |
| `/api/v1/ota/updates/pending` | GET | Gateway polls this |
| `/api/v1/ota/updates/{id}/start` | POST | Mark update as started |
| `/api/v1/ota/updates/{id}/node/{node}/progress` | POST | Report node progress |
| `/api/v1/ota/updates/{id}/complete` | POST | Mark update complete |
| `/api/v1/ota/updates/{id}/status` | GET | Get detailed status |

---

### Phase 3: Gateway OTA Distribution

**Goal**: Gateway downloads firmware and distributes via painlessMesh OTA

#### MeshSwarm Library Changes

Add to `MeshSwarm.h`:

```cpp
#define OTA_POLL_INTERVAL 60000
#define OTA_PART_SIZE 1024

struct OTAUpdateInfo {
  int updateId;
  int firmwareId;
  String nodeType;
  String version;
  String md5;
  int numParts;
  bool force;
  bool active;
};

// Public methods
void enableOTADistribution(bool enable);
void checkForUpdates();

// Private members
bool otaDistributionEnabled = false;
unsigned long lastOTACheck = 0;
OTAUpdateInfo currentUpdate;
uint8_t* firmwareBuffer = nullptr;
size_t firmwareSize = 0;

void downloadFirmware(int firmwareId);
void startDistribution(OTAUpdateInfo& update);
void reportProgress(const String& nodeId, int part, int total, const String& status);
```

#### Implementation Details

1. **checkForUpdates()**: HTTP GET `/ota/updates/pending`, parse JSON response
2. **downloadFirmware()**: HTTP GET `/firmware/{id}/download`, store in memory buffer
3. **startDistribution()**: Call painlessMesh `initOTASend()` and `offerOTA()`
4. **reportProgress()**: HTTP POST progress to server per node

#### Gateway Sketch Changes

```cpp
void setup() {
  // ... existing code ...
  swarm.enableOTADistribution(true);
}

void loop() {
  swarm.update();
  swarm.checkForUpdates();  // Poll for OTA updates
}
```

---

### Phase 4: Node OTA Reception

**Goal**: Nodes receive and apply firmware updates

#### MeshSwarm Library Changes

Add to `MeshSwarm.h`:

```cpp
void enableOTAReceive(const String& role);
```

Add to `MeshSwarm.cpp`:

```cpp
void MeshSwarm::enableOTAReceive(const String& role) {
  mesh.initOTAReceive(role.c_str());
  Serial.printf("[OTA] Receiver enabled for role: %s\n", role.c_str());
}
```

#### Node Sketch Changes

Add after `swarm.begin()` in each sketch:

| Sketch | Code to Add |
|--------|-------------|
| `mesh_shared_state_pir.ino` | `swarm.enableOTAReceive("pir");` |
| `mesh_shared_state_led.ino` | `swarm.enableOTAReceive("led");` |
| `mesh_shared_state_button.ino` | `swarm.enableOTAReceive("button");` |
| `mesh_shared_state_watcher.ino` | `swarm.enableOTAReceive("watcher");` |
| `mesh_shared_state_dht11.ino` | `swarm.enableOTAReceive("dht11");` |
| `mesh_gateway.ino` | `swarm.enableOTAReceive("gateway");` |

---

## Safety Features

### 1. Version Matching
painlessMesh uses MD5 hash comparison to skip already-installed firmware. Nodes only accept updates with different MD5.

### 2. Node Type Matching
The `role` parameter in painlessMesh OTA ensures type safety:
- Gateway announces firmware with specific role (e.g., "pir")
- Only nodes registered with matching role accept the firmware

### 3. Force Flag
Override version check when needed (e.g., rollback to previous version).

### 4. Dual Partitions
ESP32 maintains two OTA partitions. On boot failure, automatically rolls back to previous partition.

Add to node setup for explicit rollback marking:
```cpp
#include <esp_ota_ops.h>

void setup() {
  esp_ota_mark_app_valid_cancel_rollback();
  // ... rest of setup
}
```

### 5. Progress Tracking
Server tracks per-node update status enabling:
- Real-time progress monitoring
- Failure detection and alerting
- Audit trail of updates

---

## Files Summary

### Files to Create

| File | Description |
|------|-------------|
| `server/api/app/models/firmware.py` | Firmware SQLAlchemy model |
| `server/api/app/models/ota.py` | OTAUpdate, OTANodeStatus models |
| `server/api/app/schemas/firmware.py` | Firmware Pydantic schemas |
| `server/api/app/schemas/ota.py` | OTA Pydantic schemas |
| `server/api/app/routers/firmware.py` | Firmware API endpoints |
| `server/api/app/routers/ota.py` | OTA API endpoints |

### Files to Modify

| File | Changes |
|------|---------|
| `server/init.sql` | Add firmware, ota_updates, ota_node_status tables |
| `server/api/app/main.py` | Register firmware and OTA routers |
| `server/api/app/models/__init__.py` | Export new models |
| `server/api/app/schemas/__init__.py` | Export new schemas |
| `server/api/app/routers/__init__.py` | Export new routers |
| `MeshSwarm/src/MeshSwarm.h` | Add OTA structs and method declarations |
| `MeshSwarm/src/MeshSwarm.cpp` | Implement OTA distribution and reception |
| `firmware/mesh_gateway/mesh_gateway.ino` | Enable OTA distribution |
| `firmware/mesh_shared_state_*.ino` | Enable OTA reception per node type |

---

## API Reference

### Firmware Upload

```bash
curl -X POST http://localhost:8000/api/v1/firmware/upload \
  -F "file=@firmware.bin" \
  -F "node_type=pir" \
  -F "version=1.1.0" \
  -F "release_notes=Bug fixes"
```

### Create OTA Update

```bash
curl -X POST http://localhost:8000/api/v1/ota/updates \
  -H "Content-Type: application/json" \
  -d '{"firmware_id": 1, "target_node_type": "pir"}'
```

### Check Update Status

```bash
curl http://localhost:8000/api/v1/ota/updates/1/status
```

Response:
```json
{
  "id": 1,
  "firmware": {"id": 1, "node_type": "pir", "version": "1.1.0"},
  "status": "distributing",
  "nodes": [
    {"node_id": "abc123", "status": "completed", "progress": "100/100"},
    {"node_id": "def456", "status": "downloading", "progress": "45/100"}
  ]
}
```

---

## painlessMesh OTA Integration

The system leverages painlessMesh's built-in OTA capabilities:

- **Package Types**: Uses internal message types 10-12 for OTA protocol
- **Chunking**: Default 1024 bytes per chunk
- **Reliability**: 30-second timeout, 10 retries per chunk
- **Verification**: MD5 hash verification before applying update

Key painlessMesh methods:
- `mesh.initOTASend(callback, partSize)` - Initialize as OTA sender
- `mesh.offerOTA(role, hardware, md5, numParts, forced)` - Announce firmware
- `mesh.initOTAReceive(role)` - Register to receive OTA for specific role

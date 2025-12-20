# OTA Update System

Server-push OTA updates via gateway distribution to mesh nodes, with per-node-type firmware binaries.

**Status: ✅ Fully Implemented**

## Architecture

```
Server (FastAPI)          Gateway (ESP32)           Mesh Nodes
┌─────────────────┐      ┌─────────────────┐      ┌──────────────┐
│ firmware table  │      │ Poll /pending   │      │ PIR node     │
│ ota_updates     │─────>│ Stream chunks   │      │ role="pir"   │
│ ota_node_status │      │ initOTASend()   │─────>│ initOTARecv()│
└─────────────────┘      │ offerOTA()      │      └──────────────┘
                         └─────────────────┘
```

## How It Works

1. Admin uploads firmware binary to server via API
2. Admin creates OTA update job targeting node type (e.g., "pir")
3. Gateway polls `/api/v1/ota/updates/pending` every 60 seconds
4. Gateway streams firmware chunks on-demand via HTTP Range headers
5. Gateway uses painlessMesh OTA to distribute to mesh nodes
6. Nodes with matching role receive and apply update
7. Gateway reports completion to server

## Node Types & OTA Roles

| Node Sketch | OTA Role | Description |
|-------------|----------|-------------|
| `mesh_shared_state_pir.ino` | `pir` | PIR motion sensor |
| `mesh_shared_state_led.ino` | `led` | LED output controller |
| `mesh_shared_state_button.ino` | `button` | Button input |
| `mesh_shared_state_button2.ino` | `button2` | Second button variant |
| `mesh_shared_state_dht11.ino` | `dht` | Temperature/humidity sensor |
| `mesh_shared_state_watcher.ino` | `watcher` | Network observer |
| `mesh_gateway.ino` | (distributor) | OTA distribution gateway |

## API Reference

### Upload Firmware

```bash
curl -X POST http://localhost:8000/api/v1/firmware/upload \
  -F "file=@firmware.bin" \
  -F "node_type=pir" \
  -F "version=1.1.0" \
  -F "release_notes=Bug fixes"
```

### List Firmware

```bash
curl http://localhost:8000/api/v1/firmware
```

### Create OTA Update Job

```bash
curl -X POST http://localhost:8000/api/v1/ota/updates \
  -H "Content-Type: application/json" \
  -d '{"firmware_id": 1, "force_update": false}'
```

Set `force_update: true` to push even if nodes already have firmware with same MD5.

### Check Update Status

```bash
curl http://localhost:8000/api/v1/ota/updates/1
```

Response:
```json
{
  "id": 1,
  "firmware_id": 1,
  "node_type": "pir",
  "version": "1.1.0",
  "status": "completed",
  "nodes": [
    {"node_id": "abc123", "status": "completed", "current_part": 100, "total_parts": 100}
  ]
}
```

### List All Updates

```bash
curl http://localhost:8000/api/v1/ota/updates
curl http://localhost:8000/api/v1/ota/updates?status=pending
```

### Cancel Pending Update

```bash
curl -X DELETE http://localhost:8000/api/v1/ota/updates/1
```

## Safety Features

### 1. Version Matching (MD5)
painlessMesh uses MD5 hash comparison. Nodes only accept updates with different MD5 than currently running firmware.

### 2. Node Type Matching
The `role` parameter ensures type safety:
- Gateway announces firmware with specific role (e.g., "pir")
- Only nodes registered with matching role accept the firmware

### 3. Force Flag
Override MD5 check when needed (e.g., rollback to previous version or re-flash same version).

### 4. Automatic Rollback
ESP32 maintains two OTA partitions. All nodes call `esp_ota_mark_app_valid_cancel_rollback()` at startup:
- If new firmware boots successfully and reaches this call, it's marked valid
- If new firmware crashes before this call, ESP32 automatically rolls back to previous firmware

### 5. Streaming Chunks
Gateway streams firmware chunks on-demand using HTTP Range headers instead of buffering entire firmware in memory. This allows OTA of large firmware files on memory-constrained ESP32.

### 6. Progress Tracking
Server tracks per-node update status:
- Real-time progress monitoring
- Failure detection
- Audit trail of updates

## Implementation Files

### Server

| File | Description |
|------|-------------|
| `server/api/app/models/firmware.py` | Firmware SQLAlchemy model |
| `server/api/app/models/ota.py` | OTAUpdate, OTANodeStatus models |
| `server/api/app/schemas/firmware.py` | Firmware Pydantic schemas |
| `server/api/app/schemas/ota.py` | OTA Pydantic schemas |
| `server/api/app/routers/firmware.py` | Firmware API (upload, download, list) |
| `server/api/app/routers/ota.py` | OTA API (create, status, progress) |

### MeshSwarm Library

| Method | Description |
|--------|-------------|
| `enableOTADistribution(bool)` | Enable gateway OTA distribution |
| `checkForOTAUpdates()` | Poll server for pending updates (call in loop) |
| `enableOTAReceive(role)` | Enable node to receive OTA for given role |

### Firmware Sketches

All node sketches include:
```cpp
#include <esp_ota_ops.h>

void setup() {
  esp_ota_mark_app_valid_cancel_rollback();  // Enable auto-rollback
  swarm.begin("NodeName");
  swarm.enableOTAReceive("role");  // Register for OTA
  // ...
}
```

Gateway includes:
```cpp
void setup() {
  esp_ota_mark_app_valid_cancel_rollback();
  swarm.begin("Gateway");
  swarm.enableOTADistribution(true);
  // ...
}

void loop() {
  swarm.update();
  swarm.checkForOTAUpdates();  // Poll every 60 seconds
}
```

## Database Schema

### firmware table
```sql
CREATE TABLE firmware (
    id SERIAL PRIMARY KEY,
    node_type VARCHAR(30) NOT NULL,
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

### ota_updates table
```sql
CREATE TABLE ota_updates (
    id SERIAL PRIMARY KEY,
    firmware_id INTEGER REFERENCES firmware(id),
    target_node_id VARCHAR(10),
    target_node_type VARCHAR(30),
    status VARCHAR(20) DEFAULT 'pending',
    force_update BOOLEAN DEFAULT false,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ
);
```

### ota_node_status table
```sql
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
```

## painlessMesh OTA Integration

The system leverages painlessMesh's built-in OTA capabilities:

- **Chunking**: 1024 bytes per chunk (configurable via `OTA_PART_SIZE`)
- **Reliability**: 30-second timeout, 10 retries per chunk
- **Verification**: MD5 hash verification before applying update

Key painlessMesh methods used:
- `mesh.initOTASend(callback, partSize)` - Initialize as OTA sender with data callback
- `mesh.offerOTA(role, hardware, md5, numParts, forced)` - Announce firmware to nodes
- `mesh.initOTAReceive(role)` - Register to receive OTA for specific role

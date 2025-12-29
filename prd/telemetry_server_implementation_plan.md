# Telemetry Dashboard & Mesh Manager Server - Implementation Plan

This document outlines the implementation plan for building an internal telemetry dashboard and mesh manager server for the IoTMesh project. Each phase is fully self-contained and deployable before moving to the next.

## Overview

A self-hosted server that:
- Collects telemetry data from ESP32 mesh nodes
- Provides a web dashboard for real-time monitoring
- Enables remote management of mesh nodes
- Stores historical data for analysis

## Technology Stack

| Component | Technology | Justification |
|-----------|------------|---------------|
| Backend API | Python + FastAPI | Fast development, async support, auto-docs |
| Database | PostgreSQL + TimescaleDB | Time-series optimized, SQL familiar |
| Web Dashboard | React + Vite + TailwindCSS | Modern, fast builds, responsive |
| Real-time | WebSocket (native) | Low latency, bidirectional |
| Deployment | Docker Compose | Single-command deployment |
| Reverse Proxy | Nginx | SSL termination, static serving |

---

## Phase 1: Backend API Server + Simple Test UI

**Goal:** Deployable API server with database that can receive telemetry via curl/Postman, with a minimal HTML test page for verification.

### Architecture (Phase 1)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Mesh Manager Server (Phase 1)                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐ │
│  │   FastAPI    │  │   Test HTML  │  │   PostgreSQL           │ │
│  │   REST API   │  │   Page       │  │   + TimescaleDB        │ │
│  │   :8000      │  │   (built-in) │  │   :5432                │ │
│  └──────────────┘  └──────────────┘  └────────────────────────┘ │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
                              │
                    Test with curl/Postman
                              │
                    (No ESP32 changes yet)
```

### 1.1 Project Structure

```
server/
├── api/
│   ├── app/
│   │   ├── __init__.py
│   │   ├── main.py              # FastAPI app + test UI route
│   │   ├── config.py            # Environment config
│   │   ├── database.py          # DB connection
│   │   ├── models/
│   │   │   ├── __init__.py
│   │   │   ├── node.py
│   │   │   └── telemetry.py
│   │   ├── routers/
│   │   │   ├── __init__.py
│   │   │   ├── nodes.py
│   │   │   ├── telemetry.py
│   │   │   └── state.py
│   │   ├── schemas/
│   │   │   ├── __init__.py
│   │   │   ├── node.py
│   │   │   └── telemetry.py
│   │   └── templates/
│   │       └── test.html        # Simple test UI
│   ├── requirements.txt
│   ├── Dockerfile
│   └── alembic/
├── docker-compose.yml           # API + DB only
└── .env.example
```

### 1.2 API Endpoints (Phase 1)

**Node Management:**
```
GET    /api/v1/nodes                    # List all nodes
GET    /api/v1/nodes/{node_id}          # Get node details
DELETE /api/v1/nodes/{node_id}          # Remove node
PUT    /api/v1/nodes/{node_id}/name     # Update node name
```

**Telemetry Ingestion:**
```
POST   /api/v1/nodes/{node_id}/telemetry
       Body: {
         "uptime": 3600,
         "heap_free": 245000,
         "peer_count": 3,
         "role": "COORD",
         "firmware": "1.0.0",
         "state": {
           "led": "1",
           "motion": "0",
           "temp": "23.5"
         }
       }

       - Auto-registers node if not exists
       - Updates last_seen timestamp
       - Stores telemetry in time-series table
       - Updates current_state table
```

**State & History:**
```
GET    /api/v1/state                    # Current merged state from all nodes
GET    /api/v1/nodes/{node_id}/state    # Current state for one node
GET    /api/v1/nodes/{node_id}/history  # Historical telemetry
       Query: ?hours=24 (default)
```

**Health & Test:**
```
GET    /health                          # Health check
GET    /test                            # Simple HTML test page
GET    /docs                            # Auto-generated Swagger UI
```

### 1.3 Database Schema

```sql
-- Enable TimescaleDB
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Nodes table
CREATE TABLE nodes (
    id VARCHAR(10) PRIMARY KEY,       -- Mesh node ID (hex)
    name VARCHAR(50),                  -- Human-friendly name
    firmware_version VARCHAR(20),
    ip_address VARCHAR(45),            -- IPv4 or IPv6
    first_seen TIMESTAMPTZ DEFAULT NOW(),
    last_seen TIMESTAMPTZ DEFAULT NOW(),
    is_online BOOLEAN DEFAULT true,
    role VARCHAR(10) DEFAULT 'NODE',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Telemetry time-series
CREATE TABLE telemetry (
    time TIMESTAMPTZ NOT NULL,
    node_id VARCHAR(10) NOT NULL,
    heap_free INTEGER,
    uptime_sec INTEGER,
    peer_count INTEGER,
    role VARCHAR(10),
    CONSTRAINT telemetry_pkey PRIMARY KEY (time, node_id)
);
SELECT create_hypertable('telemetry', 'time');

-- Current state (latest values per node/key)
CREATE TABLE current_state (
    node_id VARCHAR(10) NOT NULL,
    key VARCHAR(50) NOT NULL,
    value TEXT,
    version INTEGER DEFAULT 1,
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    PRIMARY KEY (node_id, key)
);

-- State history (for tracking changes over time)
CREATE TABLE state_history (
    time TIMESTAMPTZ NOT NULL,
    node_id VARCHAR(10) NOT NULL,
    key VARCHAR(50) NOT NULL,
    value TEXT,
    version INTEGER
);
SELECT create_hypertable('state_history', 'time');

-- Indexes
CREATE INDEX idx_telemetry_node_time ON telemetry (node_id, time DESC);
CREATE INDEX idx_state_history_node_key ON state_history (node_id, key, time DESC);
CREATE INDEX idx_nodes_online ON nodes (is_online);

-- Retention: 30 days telemetry, 90 days state history
SELECT add_retention_policy('telemetry', INTERVAL '30 days');
SELECT add_retention_policy('state_history', INTERVAL '90 days');
```

### 1.4 Docker Compose (Phase 1)

```yaml
# docker-compose.yml
version: '3.8'

services:
  db:
    image: timescale/timescaledb:latest-pg14
    environment:
      POSTGRES_DB: meshmanager
      POSTGRES_USER: mesh
      POSTGRES_PASSWORD: ${DB_PASSWORD:-meshpass123}
    volumes:
      - pgdata:/var/lib/postgresql/data
      - ./init.sql:/docker-entrypoint-initdb.d/init.sql
    ports:
      - "5432:5432"
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U mesh -d meshmanager"]
      interval: 5s
      timeout: 5s
      retries: 5

  api:
    build: ./api
    environment:
      DATABASE_URL: postgresql://mesh:${DB_PASSWORD:-meshpass123}@db:5432/meshmanager
      API_KEY: ${API_KEY:-dev-api-key}
    depends_on:
      db:
        condition: service_healthy
    ports:
      - "8000:8000"
    volumes:
      - ./api/app:/app/app  # Hot reload in dev

volumes:
  pgdata:
```

### 1.5 Test UI (Built into API)

Simple HTML page served at `/test` for manual verification:

```html
<!-- templates/test.html -->
<!DOCTYPE html>
<html>
<head>
    <title>Mesh Manager - Test Console</title>
    <style>
        body { font-family: monospace; max-width: 800px; margin: 40px auto; padding: 20px; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ccc; }
        button { padding: 10px 20px; margin: 5px; cursor: pointer; }
        pre { background: #f5f5f5; padding: 10px; overflow-x: auto; }
        input, textarea { width: 100%; padding: 8px; margin: 5px 0; }
        .status { padding: 5px 10px; border-radius: 3px; }
        .online { background: #90EE90; }
        .offline { background: #FFB6C1; }
    </style>
</head>
<body>
    <h1>Mesh Manager - Test Console</h1>

    <div class="section">
        <h2>Push Test Telemetry</h2>
        <input type="text" id="nodeId" placeholder="Node ID (e.g., abc123)" value="test001">
        <textarea id="telemetryData" rows="10">{
  "uptime": 3600,
  "heap_free": 245000,
  "peer_count": 2,
  "role": "NODE",
  "firmware": "1.0.0",
  "state": {
    "led": "1",
    "temp": "23.5"
  }
}</textarea>
        <button onclick="pushTelemetry()">Push Telemetry</button>
        <pre id="pushResult"></pre>
    </div>

    <div class="section">
        <h2>Nodes</h2>
        <button onclick="loadNodes()">Refresh Nodes</button>
        <div id="nodeList"></div>
    </div>

    <div class="section">
        <h2>Current State</h2>
        <button onclick="loadState()">Refresh State</button>
        <pre id="stateData"></pre>
    </div>

    <script>
        const API = '';

        async function pushTelemetry() {
            const nodeId = document.getElementById('nodeId').value;
            const data = JSON.parse(document.getElementById('telemetryData').value);
            try {
                const resp = await fetch(`${API}/api/v1/nodes/${nodeId}/telemetry`, {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(data)
                });
                const result = await resp.json();
                document.getElementById('pushResult').textContent = JSON.stringify(result, null, 2);
                loadNodes();
                loadState();
            } catch (e) {
                document.getElementById('pushResult').textContent = 'Error: ' + e.message;
            }
        }

        async function loadNodes() {
            try {
                const resp = await fetch(`${API}/api/v1/nodes`);
                const nodes = await resp.json();
                let html = '<table border="1" cellpadding="8"><tr><th>ID</th><th>Name</th><th>Role</th><th>Status</th><th>Last Seen</th><th>Peers</th></tr>';
                for (const node of nodes) {
                    const status = node.is_online ? '<span class="status online">Online</span>' : '<span class="status offline">Offline</span>';
                    html += `<tr><td>${node.id}</td><td>${node.name || '-'}</td><td>${node.role}</td><td>${status}</td><td>${node.last_seen}</td><td>${node.peer_count || 0}</td></tr>`;
                }
                html += '</table>';
                document.getElementById('nodeList').innerHTML = html;
            } catch (e) {
                document.getElementById('nodeList').textContent = 'Error: ' + e.message;
            }
        }

        async function loadState() {
            try {
                const resp = await fetch(`${API}/api/v1/state`);
                const state = await resp.json();
                document.getElementById('stateData').textContent = JSON.stringify(state, null, 2);
            } catch (e) {
                document.getElementById('stateData').textContent = 'Error: ' + e.message;
            }
        }

        // Load on start
        loadNodes();
        loadState();
    </script>
</body>
</html>
```

### 1.6 Implementation Tasks (Phase 1)

**Setup:**
- [ ] Create `server/` directory structure
- [ ] Initialize FastAPI project with requirements.txt
- [ ] Create docker-compose.yml for API + TimescaleDB
- [ ] Create init.sql with database schema
- [ ] Create .env.example

**Core API:**
- [ ] Implement database connection (SQLAlchemy async)
- [ ] Create Pydantic schemas for request/response
- [ ] Implement `POST /api/v1/nodes/{node_id}/telemetry`
  - Auto-register node on first telemetry
  - Store telemetry in time-series table
  - Update current_state table
  - Update node last_seen
- [ ] Implement `GET /api/v1/nodes` (list all nodes)
- [ ] Implement `GET /api/v1/nodes/{node_id}` (node details)
- [ ] Implement `GET /api/v1/state` (current merged state)
- [ ] Implement `GET /api/v1/nodes/{node_id}/history`

**Testing & Deployment:**
- [ ] Add `/health` endpoint
- [ ] Add `/test` HTML page for manual testing
- [ ] Create Dockerfile for API
- [ ] Test with docker-compose up
- [ ] Document curl examples for testing
- [ ] Add node online/offline detection (mark offline if no telemetry for 2 minutes)

### 1.7 Testing Phase 1

**Manual curl tests:**
```bash
# Push telemetry
curl -X POST http://localhost:8000/api/v1/nodes/test001/telemetry \
  -H "Content-Type: application/json" \
  -d '{"uptime":3600,"heap_free":245000,"peer_count":2,"role":"NODE","state":{"led":"1"}}'

# List nodes
curl http://localhost:8000/api/v1/nodes

# Get state
curl http://localhost:8000/api/v1/state

# Get node history
curl http://localhost:8000/api/v1/nodes/test001/history?hours=1
```

**Test UI:**
- Open http://localhost:8000/test in browser
- Push test telemetry from multiple "nodes"
- Verify nodes appear in list
- Verify state updates

### 1.8 Phase 1 Success Criteria

- [ ] `docker-compose up` starts API and database
- [ ] Push telemetry via curl creates node automatically
- [ ] Multiple telemetry pushes create time-series data
- [ ] `/api/v1/nodes` returns list of all nodes with status
- [ ] `/api/v1/state` returns merged state from all nodes
- [ ] `/test` page allows manual telemetry submission
- [ ] Swagger docs available at `/docs`
- [ ] Node marked offline after 2 minutes of no telemetry

---

## Phase 2: ESP32 Telemetry Client

**Goal:** ESP32 nodes can push telemetry to the server. Verified by seeing real node data in Phase 1 test UI.

**Prerequisite:** Phase 1 complete and deployed.

### Architecture (Phase 2)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Mesh Manager Server                           │
│                    (from Phase 1)                                │
└──────────────────────────▲──────────────────────────────────────┘
                           │
                    HTTP POST /telemetry
                           │
┌──────────────────────────┴──────────────────────────────────────┐
│                      WiFi Network                                │
└──────▲──────────────▲──────────────▲──────────────▲─────────────┘
       │              │              │              │
   ┌───┴───┐      ┌───┴───┐      ┌───┴───┐      ┌───┴───┐
   │ ESP32 │◄────►│ ESP32 │◄────►│ ESP32 │◄────►│ ESP32 │
   │ Node  │ mesh │ Node  │ mesh │ Node  │ mesh │ Node  │
   └───────┘      └───────┘      └───────┘      └───────┘
```

### 2.1 MeshSwarm Library Additions

**New methods in MeshSwarm.h:**
```cpp
// Telemetry configuration
void setTelemetryServer(const char* url, const char* apiKey = nullptr);
void setTelemetryInterval(unsigned long ms);  // Default: 30000
void enableTelemetry(bool enable);
bool isTelemetryEnabled();

// WiFi station mode for telemetry
void connectToWiFi(const char* ssid, const char* password);
bool isWiFiConnected();

// Manual control
void pushTelemetry();
```

### 2.2 Implementation

**Telemetry push (MeshSwarm.cpp):**
```cpp
#include <HTTPClient.h>
#include <WiFi.h>

void MeshSwarm::pushTelemetry() {
    if (!telemetryEnabled || telemetryUrl.isEmpty()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = telemetryUrl + "/api/v1/nodes/" + String(myId, HEX) + "/telemetry";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    if (telemetryApiKey.length() > 0) {
        http.addHeader("X-API-Key", telemetryApiKey);
    }
    http.setTimeout(5000);  // 5 second timeout

    StaticJsonDocument<1024> doc;
    doc["uptime"] = millis() / 1000;
    doc["heap_free"] = ESP.getFreeHeap();
    doc["peer_count"] = getPeerCount();
    doc["role"] = myRole;
    doc["firmware"] = FIRMWARE_VERSION;

    JsonObject state = doc.createNestedObject("state");
    for (auto& entry : sharedState) {
        state[entry.first] = entry.second.value;
    }

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);
    if (httpCode == 200 || httpCode == 201) {
        Serial.println("[TELEM] Push OK");
    } else {
        Serial.printf("[TELEM] Push failed: %d\n", httpCode);
    }
    http.end();
}
```

**WiFi + Mesh coexistence:**
```cpp
void MeshSwarm::connectToWiFi(const char* ssid, const char* password) {
    // painlessMesh supports station mode alongside mesh
    mesh.stationManual(ssid, password);
    Serial.printf("[WIFI] Connecting to %s...\n", ssid);
}

bool MeshSwarm::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}
```

**Auto-push in update():**
```cpp
void MeshSwarm::update() {
    mesh.update();

    // Telemetry push
    if (telemetryEnabled && (millis() - lastTelemetryPush >= telemetryInterval)) {
        lastTelemetryPush = millis();
        pushTelemetry();
    }

    // ... rest of update
}
```

### 2.3 Example Sketch: Telemetry Node

```cpp
/*
 * ESP32 Mesh Swarm - Telemetry Example
 *
 * Demonstrates pushing telemetry to server while maintaining mesh.
 */

#include <MeshSwarm.h>

// WiFi credentials for telemetry server access
#define WIFI_SSID     "YourWiFi"
#define WIFI_PASSWORD "YourPassword"

// Telemetry server
#define TELEMETRY_URL "http://192.168.1.100:8000"
#define TELEMETRY_KEY "dev-api-key"  // Optional

MeshSwarm swarm;

void setup() {
    swarm.begin();

    // Connect to WiFi for telemetry (runs alongside mesh)
    swarm.connectToWiFi(WIFI_SSID, WIFI_PASSWORD);

    // Configure telemetry
    swarm.setTelemetryServer(TELEMETRY_URL, TELEMETRY_KEY);
    swarm.setTelemetryInterval(30000);  // Every 30 seconds
    swarm.enableTelemetry(true);

    Serial.println("[SETUP] Telemetry enabled");
}

void loop() {
    swarm.update();
}
```

### 2.4 Implementation Tasks (Phase 2)

- [ ] Add HTTPClient include to MeshSwarm
- [ ] Add telemetry config members to MeshSwarm class
- [ ] Implement `setTelemetryServer()`, `setTelemetryInterval()`, `enableTelemetry()`
- [ ] Implement `connectToWiFi()` using mesh.stationManual()
- [ ] Implement `pushTelemetry()` with JSON payload
- [ ] Add telemetry push timing to `update()` loop
- [ ] Add `telem` serial command to show telemetry status
- [ ] Create telemetry example sketch
- [ ] Test mesh + WiFi station coexistence
- [ ] Update library.properties

### 2.5 Testing Phase 2

1. Deploy Phase 1 server on local network
2. Flash telemetry example to ESP32
3. Verify node appears in `/test` page
4. Verify telemetry updates every 30 seconds
5. Verify mesh still works between nodes
6. Test with multiple telemetry-enabled nodes

### 2.6 Phase 2 Success Criteria

- [ ] ESP32 connects to WiFi while maintaining mesh
- [ ] Telemetry pushes to server every 30 seconds
- [ ] Node appears in server's node list
- [ ] State values visible in `/api/v1/state`
- [ ] Mesh communication unaffected by telemetry
- [ ] Multiple nodes can push telemetry simultaneously

---

## Phase 3: Web Dashboard

**Goal:** Professional web dashboard replacing the Phase 1 test UI.

**Prerequisite:** Phase 1 + Phase 2 complete. Real nodes pushing telemetry.

### Architecture (Phase 3)

```
┌─────────────────────────────────────────────────────────────────┐
│                    Mesh Manager Server                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐ │
│  │   FastAPI    │  │   React      │  │   PostgreSQL           │ │
│  │   REST API   │  │   Dashboard  │  │   + TimescaleDB        │ │
│  │   :8000      │  │   :3000      │  │   :5432                │ │
│  └──────┬───────┘  └──────┬───────┘  └────────────────────────┘ │
│         │                 │                                      │
│         └────────┬────────┘                                      │
│                  │                                               │
│         ┌────────▼────────┐                                      │
│         │    WebSocket    │                                      │
│         │   (real-time)   │                                      │
│         └─────────────────┘                                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### 3.1 Dashboard Pages

| Route | Description |
|-------|-------------|
| `/` | Dashboard overview - node grid with status |
| `/nodes` | Node list table with sorting/filtering |
| `/nodes/:id` | Node detail with history charts |
| `/state` | Live state table from all nodes |
| `/settings` | Configuration |

### 3.2 WebSocket for Real-time Updates

**Add to API (FastAPI):**
```python
# routers/websocket.py
from fastapi import WebSocket, WebSocketDisconnect
from typing import List

class ConnectionManager:
    def __init__(self):
        self.connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in self.connections:
            await connection.send_json(message)

manager = ConnectionManager()

@router.websocket("/ws/live")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            await websocket.receive_text()  # Keep alive
    except WebSocketDisconnect:
        manager.disconnect(websocket)
```

**Broadcast on telemetry receive:**
```python
# In telemetry router
@router.post("/nodes/{node_id}/telemetry")
async def push_telemetry(node_id: str, data: TelemetryIn):
    # ... save telemetry ...

    # Broadcast to connected dashboards
    await manager.broadcast({
        "type": "telemetry",
        "node_id": node_id,
        "data": data.dict()
    })
```

### 3.3 Implementation Tasks (Phase 3)

**API Additions:**
- [ ] Add WebSocket endpoint `/ws/live`
- [ ] Broadcast telemetry updates to WebSocket clients
- [ ] Broadcast node online/offline events

**Dashboard:**
- [ ] Initialize Vite + React + TypeScript
- [ ] Configure TailwindCSS
- [ ] Set up React Query
- [ ] Create layout with navigation
- [ ] Build node grid component
- [ ] Build node list table
- [ ] Build node detail page with Recharts
- [ ] Build state table with live updates
- [ ] Implement WebSocket hook
- [ ] Add dark mode
- [ ] Create Dockerfile for dashboard

**Deployment:**
- [ ] Update docker-compose.yml with dashboard service
- [ ] Add nginx for routing

### 3.4 Phase 3 Success Criteria

- [ ] Dashboard shows all nodes in real-time
- [ ] Node status updates without refresh
- [ ] Historical charts display telemetry data
- [ ] State table shows all keys from all nodes
- [ ] Responsive on mobile devices

---

## Phase 4: Production Deployment

**Goal:** Production-ready deployment with nginx, authentication, and monitoring.

**Prerequisite:** Phases 1-3 complete.

### 4.1 Final Docker Compose

```yaml
version: '3.8'

services:
  db:
    image: timescale/timescaledb:latest-pg14
    environment:
      POSTGRES_DB: meshmanager
      POSTGRES_USER: mesh
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    volumes:
      - pgdata:/var/lib/postgresql/data
    restart: unless-stopped

  api:
    build: ./api
    environment:
      DATABASE_URL: postgresql://mesh:${DB_PASSWORD}@db:5432/meshmanager
      API_KEY: ${API_KEY}
      SECRET_KEY: ${SECRET_KEY}
    depends_on:
      - db
    restart: unless-stopped

  dashboard:
    build: ./dashboard
    restart: unless-stopped

  nginx:
    image: nginx:alpine
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf:ro
      - ./certs:/etc/nginx/certs:ro  # Optional TLS
    ports:
      - "80:80"
      - "443:443"
    depends_on:
      - api
      - dashboard
    restart: unless-stopped

volumes:
  pgdata:
```

### 4.2 Implementation Tasks (Phase 4)

- [ ] Add API key authentication middleware
- [ ] Add dashboard login (basic auth or session)
- [ ] Configure nginx reverse proxy
- [ ] Add TLS support (optional)
- [ ] Create backup script for database
- [ ] Add health checks to all services
- [ ] Write deployment documentation
- [ ] Test 7-day stability run

### 4.3 Phase 4 Success Criteria

- [ ] Single `docker-compose up -d` deploys everything
- [ ] Authentication required for dashboard
- [ ] API key required for telemetry push
- [ ] System stable for 7+ days
- [ ] Database backup/restore tested

---

## Quick Reference: Testing Each Phase

### Phase 1 Test
```bash
cd server
docker-compose up -d
# Open http://localhost:8000/test
# Push telemetry, verify nodes appear
```

### Phase 2 Test
```bash
# With Phase 1 running:
# Flash telemetry sketch to ESP32
# Check http://localhost:8000/test for real node data
```

### Phase 3 Test
```bash
cd server
docker-compose up -d
# Open http://localhost:3000 (dashboard)
# Verify real-time updates from ESP32 nodes
```

### Phase 4 Test
```bash
docker-compose -f docker-compose.prod.yml up -d
# Verify authentication required
# Run for 7 days, check stability
```

---

## Summary

| Phase | Deliverable | Test Method |
|-------|-------------|-------------|
| 1 | API + DB + Test UI | curl, browser /test |
| 2 | ESP32 telemetry client | Real nodes in test UI |
| 3 | React dashboard | Full UI with real data |
| 4 | Production deployment | 7-day stability test |

Each phase builds on the previous and can be deployed/tested independently before moving forward.

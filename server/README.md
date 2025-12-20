# IoT Mesh Telemetry Server

Backend infrastructure for collecting telemetry from mesh nodes and visualizing network state.

## Components

| Component | Description | Port |
|-----------|-------------|------|
| [API](api/README.md) | FastAPI REST server for telemetry collection | 8000 |
| [Dashboard](dashboard/README.md) | React web UI for monitoring nodes and state | 3000 |
| Database | TimescaleDB (PostgreSQL) for time-series storage | 5432 |

## Quick Start (Docker)

The easiest way to run the full stack:

```bash
cd server

# Start all services
docker-compose up -d

# View logs
docker-compose logs -f
```

Services will be available at:
- API: http://localhost:8000
- API Docs: http://localhost:8000/docs
- Test UI: http://localhost:8000/test
- Dashboard: http://localhost:3000

## Configuration

Create a `.env` file in the `server/` directory (see `.env.example`):

```bash
DB_PASSWORD=your_secure_password
API_KEY=your_api_key
```

## Architecture

```
ESP32 Nodes                    Server
┌─────────────┐               ┌─────────────────────────────────┐
│ PIR Node    │               │                                 │
│ LED Node    │──telemetry──> │  API (:8000)                    │
│ Button Node │               │    │                            │
│ Gateway     │               │    v                            │
└─────────────┘               │  TimescaleDB (:5432)            │
                              │    │                            │
                              │    v                            │
                              │  Dashboard (:3000)              │
                              └─────────────────────────────────┘
```

## Development

### Running Components Individually

**Database only:**
```bash
docker-compose up db -d
```

**API (local development):**
```bash
cd api
python -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate
pip install -r requirements.txt
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

**Dashboard (local development):**
```bash
cd dashboard
npm install
npm run dev  # Opens on http://localhost:5173
```

### Hot Reload

The docker-compose configuration mounts the `api/app` directory for hot reload during development. Changes to Python files are reflected immediately.

## API Endpoints

### Telemetry & State

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/nodes` | GET | List all registered nodes |
| `/api/v1/nodes/{id}` | GET | Get node details |
| `/api/v1/nodes/{id}/telemetry` | POST | Submit telemetry data |
| `/api/v1/state` | GET | Get current shared state |
| `/health` | GET | Health check |

### Firmware & OTA Updates

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/firmware/upload` | POST | Upload firmware binary |
| `/api/v1/firmware` | GET | List all firmware versions |
| `/api/v1/firmware/{id}` | GET | Get firmware metadata |
| `/api/v1/firmware/{id}/download` | GET | Download firmware binary |
| `/api/v1/ota/updates` | POST | Create OTA update job |
| `/api/v1/ota/updates` | GET | List all update jobs |
| `/api/v1/ota/updates/pending` | GET | Gateway polls this |
| `/api/v1/ota/updates/{id}` | GET | Get update status |

See [API Documentation](http://localhost:8000/docs) for full details.

## Database Schema

- `nodes` - Registered mesh nodes with firmware version, online status
- `telemetry` - Time-series metrics (heap, uptime, peer count)
- `current_state` - Latest shared state per node/key
- `state_history` - Historical state changes
- `firmware` - Uploaded firmware binaries with metadata
- `ota_updates` - OTA update jobs and their status
- `ota_node_status` - Per-node progress for each update

See [init.sql](init.sql) for full schema.

## OTA Update Workflow

1. **Upload firmware**: `POST /api/v1/firmware/upload` with binary and metadata
2. **Create update job**: `POST /api/v1/ota/updates` with firmware_id and target node type
3. **Gateway polls**: Gateway checks `/api/v1/ota/updates/pending` every 60 seconds
4. **Distribution**: Gateway streams firmware chunks to matching nodes
5. **Completion**: Gateway reports `/api/v1/ota/updates/{id}/complete`

See [OTA Documentation](../docs/ota_update_system.md) for details.

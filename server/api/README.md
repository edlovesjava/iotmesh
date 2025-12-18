# Telemetry API Server

FastAPI-based REST API for collecting and serving mesh telemetry data.

## Setup

See [docs/telemetry_server_implementation_plan.md](../../docs/telemetry_server_implementation_plan.md) for implementation details.

## Quick Start

```bash
# Create virtual environment
python -m venv venv
source venv/bin/activate  # or venv\Scripts\activate on Windows

# Install dependencies
pip install -r requirements.txt

# Run development server
uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

## Endpoints

- `POST /api/v1/nodes/{node_id}/telemetry` - Submit telemetry
- `GET /api/v1/nodes` - List all nodes
- `GET /api/v1/nodes/{node_id}` - Get node details
- `GET /api/v1/state` - Get current merged state
- `WebSocket /ws/live` - Real-time updates

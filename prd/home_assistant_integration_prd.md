# Home Assistant Integration PRD

## Summary
Integrate IoTMesh with the open-source Home Assistant (HA) ecosystem so mesh nodes and shared state can appear as entities in Home Assistant. Provide bidirectional state sync, device discovery, and a clear setup path for users running HA locally.

## Motivation
- Allow users to monitor and control mesh nodes from a widely used smart home platform.
- Reduce custom dashboard effort by leveraging Home Assistant’s UI and automation engine.
- Standardize telemetry and state exposure for non-technical users.

## Goals
- Provide a reliable bridge between IoTMesh shared state and Home Assistant entities.
- Support bidirectional state updates (HA → IoTMesh and IoTMesh → HA).
- Enable device discovery and metadata mapping (node type, capabilities, firmware).
- Secure, configurable integration with minimal setup for local HA users.

## Non-Goals
- Replacing the existing MeshSwarm dashboard.
- Providing Home Assistant Cloud or external access features.
- Supporting every Home Assistant platform on day one.

## Target Users
- Home Assistant users who want ESP32 mesh nodes as native entities.
- Makers who want automations using HA for MeshSwarm nodes.
- Developers building node types that should show up in HA.

## User Stories
- As a HA user, I can see each mesh node as a device with sensors and controls.
- As a HA user, I can toggle a mesh node state (e.g., LED) and have it propagate to the mesh.
- As a developer, I can declare node capabilities so HA gets correct entity types.
- As an admin, I can revoke or rotate credentials used by the HA integration.

## Requirements

### Functional
1. **Entity mapping**
   - Map MeshSwarm state keys to Home Assistant entities (sensor, switch, binary_sensor, number, etc.).
2. **Discovery and registry**
   - Expose nodes and metadata to HA’s device registry.
3. **Bidirectional sync**
   - IoTMesh → HA: state updates update HA entities.
   - HA → IoTMesh: HA service calls update mesh state.
4. **Configuration**
   - Users can enable/disable HA integration and set credentials in server config.
5. **Authentication**
   - Require an API key or token for HA to connect.

### Non-Functional
- Low latency for state updates (< 2s target for LAN).
- Robust reconnect and backoff handling.
- Clear logging and diagnostics in the server for integration failures.

## Proposed Architecture

### Option A: Home Assistant MQTT Discovery (Recommended)
- Use HA’s MQTT discovery to auto-create entities.
- Server publishes discovery payloads and state to an MQTT broker.
- HA sends commands back via MQTT topics.

**Pros:**
- Native HA support, stable ecosystem.
- Simple data model and tooling.

**Cons:**
- Requires MQTT broker in the stack.

### Option B: Home Assistant REST API + Webhooks
- Server pushes state to HA REST API.
- HA calls back to server endpoints to update mesh state.

**Pros:**
- No MQTT broker required.

**Cons:**
- More custom glue, less HA-native entity discovery.

### Option C: Custom Home Assistant Integration
- Build a custom HA integration (Python) that connects to the IoTMesh server API.

**Pros:**
- Deepest integration, full control.

**Cons:**
- Higher maintenance, more work to distribute updates.

## Recommended Direction
Start with **Option A (MQTT Discovery)** for the first iteration. It provides the lowest integration effort for HA users, and leverages existing HA patterns.

## Packaging & Deployment Options
- **Home Assistant Add-on (preferred for HA users)**: Package the IoTMesh server bridge as an HA add-on so users can install from the HA UI with built-in configuration options.
- **Standalone Docker Compose**: Keep the existing `server/docker-compose` as the default for non-HA deployments; add an optional MQTT broker service or document integration with an external broker.
- **Manual install**: Document a minimal setup path for users running HA on bare metal who want to configure MQTT and the bridge manually.

## Data Model

### Node → HA Device Mapping
- **Device identifiers:** `node_id`, `node_type`
- **Name:** `MeshSwarm {node_type} {node_id}`
- **Manufacturer:** IoTMesh / MeshSwarm
- **SW version:** firmware version from telemetry

### State → Entity Mapping
| Mesh State Key | Suggested HA Entity Type | Example |
|---|---|---|
| `temperature` | sensor | `sensor.meshswarm_node_1_temperature` |
| `humidity` | sensor | `sensor.meshswarm_node_1_humidity` |
| `motion` | binary_sensor | `binary_sensor.meshswarm_node_2_motion` |
| `led` | switch | `switch.meshswarm_node_3_led` |

## API/Integration Surface

### MQTT Topics (Option A)
- Discovery: `homeassistant/<component>/<unique_id>/config`
- State: `meshswarm/<node_id>/<key>/state`
- Command: `meshswarm/<node_id>/<key>/set`

### Server Endpoints (Optional)
- `POST /api/v1/ha/register` – register new HA integration
- `POST /api/v1/ha/state` – push state (if not using MQTT)

## Security & Privacy
- Require API key or MQTT credentials.
- Local-only by default; avoid internet exposure.
- Consider per-node access control for command topics.

## Success Metrics
- Time-to-first-entity in HA < 10 minutes.
- 99% state update delivery on LAN.
- At least 5 node types supported in v1.

## Milestones
1. **Discovery prototype** (MQTT): basic entity registration for nodes.
2. **State sync**: bi-directional update for a small set of keys.
3. **Config UX**: server-side config + docs.
4. **Stability**: reconnect handling, logging, metrics.

## Risks
- Mesh node connectivity can be intermittent; HA expects stability.
- Entity explosion if every state key creates an HA entity.
- MQTT broker availability and configuration complexity.

## Open Questions
- Should we bundle an MQTT broker in `server/docker-compose`?
- How should node capabilities be described (static config vs dynamic reporting)?
- Which node types should be supported in the v1 integration?

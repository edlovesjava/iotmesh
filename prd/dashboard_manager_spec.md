# Dashboard & Manager UI Specification

## Overview

Extend the existing dashboard with a tabbed interface:
- **Dashboard** tab: Current monitoring UI (nodes, state)
- **Manager** tab: Administrative UI for firmware and OTA operations

This provides a UI alternative to direct API calls for managing the mesh network.

## Current State

### Existing Dashboard Features
- Node grid with online/offline status, role, firmware version
- Shared state table with real-time updates
- 5-second auto-refresh
- Dark theme with TailwindCSS

### Existing API Endpoints (Manager will use)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/firmware/upload` | POST | Upload firmware binary |
| `/api/v1/firmware` | GET | List all firmware versions |
| `/api/v1/firmware/{id}` | GET | Get firmware metadata |
| `/api/v1/firmware/{id}` | DELETE | Delete firmware |
| `/api/v1/firmware/{id}/stable` | PATCH | Mark stable/unstable |
| `/api/v1/ota/updates` | POST | Create OTA update job |
| `/api/v1/ota/updates` | GET | List all update jobs |
| `/api/v1/ota/updates/{id}` | GET | Get update status |
| `/api/v1/ota/updates/{id}` | DELETE | Cancel pending update |
| `/api/v1/nodes/{id}` | DELETE | Remove node |
| `/api/v1/nodes/{id}/name` | PUT | Rename node |

---

## UI Architecture

### Tab Navigation

```
┌─────────────────────────────────────────────────────────────────┐
│  IoT Mesh Network                              Last: 12:34:56   │
├─────────────┬─────────────┬─────────────────────────────────────┤
│  Dashboard  │   Manager   │                                     │
├─────────────┴─────────────┴─────────────────────────────────────┤
│                                                                 │
│  [Tab content here]                                             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Component Structure

```
App.jsx
├── TabNavigation.jsx
├── Dashboard/ (existing)
│   ├── NodeGrid.jsx
│   └── StateTable.jsx
└── Manager/
    ├── ManagerLayout.jsx
    ├── FirmwarePanel.jsx
    │   ├── FirmwareUpload.jsx
    │   └── FirmwareList.jsx
    ├── OTAPanel.jsx
    │   ├── OTACreateForm.jsx
    │   ├── OTAUpdateList.jsx
    │   └── OTAProgressCard.jsx
    └── NodesPanel.jsx
        ├── NodeList.jsx
        └── NodeActions.jsx
```

---

## Manager Tab Sections

### 1. Firmware Management

#### Layout
```
┌─────────────────────────────────────────────────────────────────┐
│  Firmware Management                                            │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Upload New Firmware                                     │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  File: [Choose file...]  firmware.bin                   │   │
│  │  Node Type: [pir    ▼]   Version: [1.2.0    ]           │   │
│  │  Hardware:  [ESP32  ▼]   Release Notes: [___________]   │   │
│  │  [ ] Mark as Stable                                     │   │
│  │                                    [Upload Firmware]     │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Firmware Library                        Filter: [All ▼]│   │
│  ├──────┬───────┬─────────┬────────┬────────┬──────┬───────┤   │
│  │ Type │Version│ Hardware│  Size  │ Stable │ Date │Actions│   │
│  ├──────┼───────┼─────────┼────────┼────────┼──────┼───────┤   │
│  │ pir  │ 1.2.0 │ ESP32   │ 845 KB │   ★    │ 12/19│ ⬇ ✕  │   │
│  │ pir  │ 1.1.0 │ ESP32   │ 842 KB │        │ 12/15│ ⬇ ✕  │   │
│  │ led  │ 1.0.0 │ ESP32   │ 838 KB │   ★    │ 12/10│ ⬇ ✕  │   │
│  └──────┴───────┴─────────┴────────┴────────┴──────┴───────┘   │
└─────────────────────────────────────────────────────────────────┘
```

#### Features
- **Upload Form**
  - File picker with drag-and-drop support
  - Node type dropdown (populated from known types: pir, led, button, button2, dht, watcher, gateway)
  - Version input with semantic versioning hint
  - Hardware dropdown (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)
  - Release notes textarea
  - Stable checkbox
  - Upload progress indicator
  - Success/error toast notifications

- **Firmware List**
  - Filterable by node type
  - Sortable by version/date
  - Actions per row:
    - Download firmware binary
    - Toggle stable status
    - Delete (with confirmation modal)
  - Visual indicator for stable versions (star icon)
  - Size formatted (KB/MB)

#### API Calls
```javascript
// Upload firmware
POST /api/v1/firmware/upload (multipart/form-data)

// List firmware
GET /api/v1/firmware?node_type=pir

// Download firmware
GET /api/v1/firmware/{id}/download

// Toggle stable
PATCH /api/v1/firmware/{id}/stable
Body: { "is_stable": true }

// Delete firmware
DELETE /api/v1/firmware/{id}
```

---

### 2. OTA Updates

#### Layout
```
┌─────────────────────────────────────────────────────────────────┐
│  OTA Updates                                                    │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Create Update                                           │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Target Type: [pir ▼]  Firmware: [v1.2.0 (stable) ▼]    │   │
│  │  [ ] Force update (ignore MD5 match)                    │   │
│  │                                    [Create Update Job]   │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Active Updates                                          │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  ┌───────────────────────────────────────────────────┐  │   │
│  │  │ Update #3: pir → v1.2.0              DISTRIBUTING │  │   │
│  │  │ ████████████████████░░░░  3/4 nodes (75%)         │  │   │
│  │  │ ─────────────────────────────────────────────────  │  │   │
│  │  │ PIR-Kitchen    ████████████████████ 100% ✓        │  │   │
│  │  │ PIR-Garage     ████████████████████ 100% ✓        │  │   │
│  │  │ PIR-Bedroom    ██████████░░░░░░░░░░  45%          │  │   │
│  │  │ PIR-Backyard   ░░░░░░░░░░░░░░░░░░░░ pending       │  │   │
│  │  └───────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Update History                          Filter: [All ▼]│   │
│  ├──────┬───────┬─────────┬────────┬────────┬──────────────┤   │
│  │  ID  │ Type  │ Version │ Status │ Nodes  │   Created    │   │
│  ├──────┼───────┼─────────┼────────┼────────┼──────────────┤   │
│  │  #2  │ led   │ 1.0.0   │   ✓    │  2/2   │ 12/18 14:30  │   │
│  │  #1  │ pir   │ 1.1.0   │   ✓    │  4/4   │ 12/15 09:15  │   │
│  └──────┴───────┴─────────┴────────┴────────┴──────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

#### Features
- **Create Update Form**
  - Target type dropdown (auto-populated from firmware types)
  - Firmware dropdown (filtered by selected type, shows version + stable indicator)
  - Force update checkbox with tooltip explanation
  - Validation: requires both selections
  - Success creates job and shows in Active Updates

- **Active Updates Panel**
  - Real-time progress (poll every 3 seconds for active updates)
  - Overall progress bar with percentage
  - Per-node breakdown:
    - Node name
    - Progress bar (current_part / total_parts)
    - Status icon (pending, downloading, completed, failed)
  - Cancel button for pending updates
  - Expands/collapses for multiple active updates

- **Update History**
  - Filterable by status (all, completed, failed, cancelled)
  - Clickable rows to view full details
  - Status badges (green=completed, red=failed, gray=cancelled)
  - Success rate display

#### API Calls
```javascript
// Create update job
POST /api/v1/ota/updates
Body: { "firmware_id": 1, "force_update": false }

// List updates
GET /api/v1/ota/updates?status=distributing

// Get update details with per-node status
GET /api/v1/ota/updates/{id}

// Cancel pending update
DELETE /api/v1/ota/updates/{id}
```

---

### 3. Node Management

#### Layout
```
┌─────────────────────────────────────────────────────────────────┐
│  Node Management                                                │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Registered Nodes                                   [↻] │   │
│  ├────────────┬──────────┬───────────┬──────────┬──────────┤   │
│  │    Name    │  Status  │  Firmware │   Role   │  Actions │   │
│  ├────────────┼──────────┼───────────┼──────────┼──────────┤   │
│  │ PIR-Kitchen│  ● Online│   v1.2.0  │   NODE   │  ✏ 🗑    │   │
│  │ LED-Living │  ● Online│   v1.0.0  │   COORD  │  ✏ 🗑    │   │
│  │ Gateway    │  ● Online│   v1.0.0  │   NODE   │  ✏       │   │
│  │ PIR-Garage │  ○ Offline│  v1.1.0  │   NODE   │  ✏ 🗑    │   │
│  └────────────┴──────────┴───────────┴──────────┴──────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Node Details: PIR-Kitchen                          [✕] │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │  Node ID:    abc123def                                  │   │
│  │  IP Address: 192.168.1.45                               │   │
│  │  Firmware:   v1.2.0                                     │   │
│  │  Role:       NODE                                       │   │
│  │  Uptime:     2d 5h 32m                                  │   │
│  │  Heap Free:  124,532 bytes                              │   │
│  │  Peer Count: 3                                          │   │
│  │  First Seen: Dec 10, 2024 08:15:32                      │   │
│  │  Last Seen:  Dec 19, 2024 14:32:15                      │   │
│  │                                                         │   │
│  │  Current State:                                         │   │
│  │  ┌────────────┬────────┬─────────┐                      │   │
│  │  │    Key     │ Value  │ Version │                      │   │
│  │  ├────────────┼────────┼─────────┤                      │   │
│  │  │ motion     │   0    │   42    │                      │   │
│  │  │ motion_kit │   0    │   42    │                      │   │
│  │  └────────────┴────────┴─────────┘                      │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

#### Features
- **Node List**
  - All registered nodes with status indicators
  - Online/offline status (green/gray dot)
  - Current firmware version
  - Role badge (COORD highlighted)
  - Actions:
    - Edit name (inline or modal)
    - Delete node (with confirmation, disabled for gateway)
  - Click row to expand details

- **Node Details Panel**
  - Full node information
  - Current state entries from this node
  - Telemetry snapshot (heap, uptime, peers)
  - Rename inline editing
  - Delete with cascade warning

#### API Calls
```javascript
// Get node details
GET /api/v1/nodes/{node_id}

// Rename node
PUT /api/v1/nodes/{node_id}/name
Body: { "name": "PIR-Kitchen" }

// Delete node
DELETE /api/v1/nodes/{node_id}

// Get node state
GET /api/v1/nodes/{node_id}/state
```

---

## New Components

### File Structure
```
server/dashboard/src/
├── App.jsx                    # Add tab state, router
├── api/
│   ├── mesh.js               # Existing
│   ├── firmware.js           # NEW: firmware API calls
│   └── ota.js                # NEW: OTA API calls
├── components/
│   ├── NodeGrid.jsx          # Existing
│   ├── StateTable.jsx        # Existing
│   ├── TabNavigation.jsx     # NEW: tab switcher
│   ├── common/
│   │   ├── Modal.jsx         # NEW: confirmation dialogs
│   │   ├── Toast.jsx         # NEW: notifications
│   │   ├── ProgressBar.jsx   # NEW: reusable progress bar
│   │   ├── Badge.jsx         # NEW: status badges
│   │   └── Dropdown.jsx      # NEW: select component
│   └── manager/
│       ├── ManagerLayout.jsx # NEW: manager tab container
│       ├── FirmwarePanel.jsx # NEW: firmware section
│       ├── FirmwareUpload.jsx# NEW: upload form
│       ├── FirmwareList.jsx  # NEW: firmware table
│       ├── OTAPanel.jsx      # NEW: OTA section
│       ├── OTACreateForm.jsx # NEW: create update form
│       ├── OTAUpdateList.jsx # NEW: update history
│       ├── OTAProgressCard.jsx# NEW: active update display
│       ├── NodesPanel.jsx    # NEW: node management
│       └── NodeDetails.jsx   # NEW: node detail view
└── utils/
    ├── time.js               # Existing
    └── format.js             # NEW: additional formatters
```

### API Client Extensions

#### `api/firmware.js`
```javascript
const API_BASE = '/api/v1';

export async function uploadFirmware(formData) {
  const response = await fetch(`${API_BASE}/firmware/upload`, {
    method: 'POST',
    body: formData, // FormData with file, node_type, version, etc.
  });
  if (!response.ok) throw new Error(await response.text());
  return response.json();
}

export async function fetchFirmwareList(nodeType = null) {
  const params = nodeType ? `?node_type=${nodeType}` : '';
  const response = await fetch(`${API_BASE}/firmware${params}`);
  if (!response.ok) throw new Error('Failed to fetch firmware');
  return response.json();
}

export async function deleteFirmware(firmwareId) {
  const response = await fetch(`${API_BASE}/firmware/${firmwareId}`, {
    method: 'DELETE',
  });
  if (!response.ok) throw new Error('Failed to delete firmware');
  return response.json();
}

export async function toggleFirmwareStable(firmwareId, isStable) {
  const response = await fetch(`${API_BASE}/firmware/${firmwareId}/stable`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ is_stable: isStable }),
  });
  if (!response.ok) throw new Error('Failed to update firmware');
  return response.json();
}

export function getFirmwareDownloadUrl(firmwareId) {
  return `${API_BASE}/firmware/${firmwareId}/download`;
}
```

#### `api/ota.js`
```javascript
const API_BASE = '/api/v1';

export async function createOTAUpdate(firmwareId, forceUpdate = false) {
  const response = await fetch(`${API_BASE}/ota/updates`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ firmware_id: firmwareId, force_update: forceUpdate }),
  });
  if (!response.ok) throw new Error('Failed to create update');
  return response.json();
}

export async function fetchOTAUpdates(status = null) {
  const params = status ? `?status=${status}` : '';
  const response = await fetch(`${API_BASE}/ota/updates${params}`);
  if (!response.ok) throw new Error('Failed to fetch updates');
  return response.json();
}

export async function fetchOTAUpdateDetails(updateId) {
  const response = await fetch(`${API_BASE}/ota/updates/${updateId}`);
  if (!response.ok) throw new Error('Failed to fetch update details');
  return response.json();
}

export async function cancelOTAUpdate(updateId) {
  const response = await fetch(`${API_BASE}/ota/updates/${updateId}`, {
    method: 'DELETE',
  });
  if (!response.ok) throw new Error('Failed to cancel update');
  return response.json();
}
```

---

## State Management

### App-Level State
```javascript
// App.jsx
const [activeTab, setActiveTab] = useState('dashboard'); // 'dashboard' | 'manager'
const [nodes, setNodes] = useState([]);
const [state, setState] = useState([]);
const [firmware, setFirmware] = useState([]);
const [otaUpdates, setOTAUpdates] = useState([]);
const [activeOTAUpdates, setActiveOTAUpdates] = useState([]);
```

### Refresh Strategy
- Dashboard tab: 5-second auto-refresh (existing)
- Manager tab: Manual refresh + on-action refresh
- Active OTA updates: 3-second polling when updates are distributing

---

## UI/UX Considerations

### Responsive Design
- Mobile: Single column, collapsible panels
- Tablet: 2-column layout where appropriate
- Desktop: Full 3-column manager layout

### Loading States
- Skeleton loaders for tables
- Spinner overlays for actions
- Disabled buttons during operations

### Error Handling
- Toast notifications for errors
- Inline validation for forms
- Retry buttons for failed fetches

### Confirmations
- Delete firmware: Modal with warning
- Delete node: Modal with cascade info
- Cancel OTA: Modal confirmation
- Force update: Checkbox requires explicit action

### Accessibility
- Keyboard navigation for tabs
- ARIA labels on interactive elements
- Focus management in modals

---

## Implementation Phases

### Phase 1: Foundation
- [ ] Add TabNavigation component
- [ ] Create manager layout structure
- [ ] Add common components (Modal, Toast, ProgressBar)
- [ ] Set up API clients for firmware and OTA

### Phase 2: Firmware Management
- [ ] FirmwareUpload with drag-and-drop
- [ ] FirmwareList with filtering
- [ ] Download/delete/stable toggle actions
- [ ] Integration testing with API

### Phase 3: OTA Updates
- [ ] OTACreateForm with firmware selection
- [ ] OTAProgressCard for active updates
- [ ] OTAUpdateList for history
- [ ] Real-time polling for active updates

### Phase 4: Node Management
- [ ] NodesPanel with node list
- [ ] NodeDetails expandable panel
- [ ] Rename and delete actions
- [ ] Per-node state display

### Phase 5: Polish
- [ ] Responsive breakpoints
- [ ] Loading/error states everywhere
- [ ] Keyboard navigation
- [ ] Toast notification system
- [ ] Final styling pass

---

## Future Enhancements

### Potential Additions
- Telemetry charts (heap, uptime over time)
- State history timeline
- Batch node operations
- Firmware comparison view
- OTA rollback quick action
- Node health alerts
- WebSocket for real-time updates
- Dark/light theme toggle
- Export telemetry data

### API Additions Needed
None - current API fully supports all proposed features.

---

## Testing Strategy

### Unit Tests
- Component rendering tests
- API client mock tests
- Utility function tests

### Integration Tests
- Full flow: upload firmware → create OTA → monitor progress
- Error handling: network failures, validation errors
- Edge cases: empty states, concurrent updates

### E2E Tests
- Playwright/Cypress for critical paths
- Visual regression testing

---

## Dependencies

### New Packages (Optional)
- `@headlessui/react` - Accessible UI components
- `react-dropzone` - File upload drag-and-drop
- `react-hot-toast` - Toast notifications

### Existing (Already Available)
- TailwindCSS - Styling
- React 19 - UI framework
- Vite - Build tool

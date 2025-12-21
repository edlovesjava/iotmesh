# Node Provisioning Specification

## Problem Statement

When flashing firmware to nodes, configuration data is lost. Each node needs persistent identity and configuration:
- **Node name/zone** (e.g., "Kitchen", "Garage", "Bedroom")
- **Role-specific settings** (e.g., motion timeout, LED color, sensor thresholds)
- **Network identity** (preserved across reflash)

## Requirements

1. Configuration survives OTA updates
2. Configuration survives manual reflash (ideally)
3. Easy provisioning of new nodes
4. Configuration editable via serial, mesh, or server
5. Minimal flash wear

---

## Storage Options

### Option 1: ESP32 Preferences (NVS) ✅ Recommended

ESP32's Non-Volatile Storage in a separate flash partition.

**Pros:**
- Survives OTA updates (different partition)
- Survives most reflashes (unless you erase all flash)
- Built into ESP-IDF, no extra libraries
- Wear-leveling handled automatically
- Key-value store, easy to use

**Cons:**
- ~20KB partition size by default
- Lost if full flash erase (`esptool.py erase_flash`)

**Usage:**
```cpp
#include <Preferences.h>
Preferences prefs;

void loadConfig() {
  prefs.begin("meshswarm", false);  // namespace, readonly=false
  nodeName = prefs.getString("name", "Node");
  nodeZone = prefs.getString("zone", "default");
  motionTimeout = prefs.getInt("motion_timeout", 30);
  prefs.end();
}

void saveConfig() {
  prefs.begin("meshswarm", false);
  prefs.putString("name", nodeName);
  prefs.putString("zone", nodeZone);
  prefs.putInt("motion_timeout", motionTimeout);
  prefs.end();
}
```

### Option 2: SPIFFS/LittleFS File

Store config as JSON file on flash filesystem.

**Pros:**
- Human-readable config file
- Can store complex nested config
- Easy to backup/restore

**Cons:**
- Separate partition, may be erased on reflash
- More flash space needed
- Filesystem overhead

**Usage:**
```cpp
#include <SPIFFS.h>
#include <ArduinoJson.h>

void loadConfig() {
  File file = SPIFFS.open("/config.json", "r");
  JsonDocument doc;
  deserializeJson(doc, file);
  nodeName = doc["name"] | "Node";
  nodeZone = doc["zone"] | "default";
  file.close();
}
```

### Option 3: EEPROM Emulation

Legacy approach using flash to emulate EEPROM.

**Pros:**
- Simple API
- Familiar to Arduino users

**Cons:**
- Limited wear-leveling
- Fixed size
- Deprecated in favor of Preferences

### Option 4: Server-Side Provisioning

Store config on telemetry server, download on boot.

**Pros:**
- Centralized management
- Easy backup/restore
- Works across reflash

**Cons:**
- Requires network connectivity
- Chicken-and-egg: need some ID to fetch config
- Latency on boot

---

## Recommended Architecture

**Hybrid approach: NVS (Preferences) + Server backup**

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32 Node                              │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐ │
│  │   Firmware  │    │     NVS     │    │   Chip ID (MAC)     │ │
│  │  (OTA slot) │    │  (config)   │    │   (permanent)       │ │
│  └─────────────┘    └─────────────┘    └─────────────────────┘ │
│       OTA             Preserved          Always preserved      │
│      updates           across             unique ID            │
│                        reflash                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Sync on boot / on change
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Telemetry Server                           │
├─────────────────────────────────────────────────────────────────┤
│  Node Registry:                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ chip_id    │ name      │ zone     │ config              │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │ f4a50215   │ PIR-Kit   │ kitchen  │ {timeout: 30, ...}  │  │
│  │ f4a4ce39   │ LED-Liv   │ living   │ {brightness: 80}    │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Configuration Schema

### Common Config (all nodes)
```json
{
  "name": "PIR-Kitchen",
  "zone": "kitchen",
  "floor": 1,
  "tags": ["motion", "security"]
}
```

### Role-Specific Config

**PIR Node:**
```json
{
  "motion_timeout_sec": 30,
  "sensitivity": "high",
  "report_interval_sec": 5
}
```

**LED Node:**
```json
{
  "default_brightness": 80,
  "color_rgb": [255, 255, 255],
  "fade_duration_ms": 500
}
```

**DHT Node:**
```json
{
  "read_interval_sec": 10,
  "temp_offset": -1.5,
  "humidity_offset": 0
}
```

**Button Node:**
```json
{
  "debounce_ms": 50,
  "long_press_ms": 1000,
  "target_state_key": "led"
}
```

---

## Implementation Plan

### Phase 1: NVS Config Storage

Add to MeshSwarm library:

```cpp
// MeshSwarm.h additions
class MeshSwarm {
public:
  // Configuration
  void loadConfig();
  void saveConfig();
  String getConfigValue(const String& key, const String& defaultValue = "");
  int getConfigInt(const String& key, int defaultValue = 0);
  void setConfigValue(const String& key, const String& value);
  void setConfigInt(const String& key, int value);

  // Node identity
  String getChipId();  // Permanent hardware ID

private:
  Preferences prefs;
};
```

**Serial commands:**
```
config                    # Show all config
config get <key>          # Get value
config set <key> <value>  # Set value
config reset              # Reset to defaults
config export             # Export as JSON
```

### Phase 2: Provisioning Mode

First-boot or triggered provisioning:

```cpp
void MeshSwarm::enterProvisioningMode() {
  // Start WiFi AP: "MeshSwarm-XXXX"
  // Serve simple web page for config
  // Accept config via HTTP POST
  // Save to NVS
  // Reboot into normal mode
}
```

**Provisioning triggers:**
- Button held on boot (GPIO0 for 5 seconds)
- Serial command: `provision`
- No config exists (first boot)

### Phase 3: Server-Side Backup

**API Endpoints:**
```
GET  /api/v1/nodes/{chip_id}/config     # Get config from server
PUT  /api/v1/nodes/{chip_id}/config     # Update config on server
POST /api/v1/nodes/{chip_id}/provision  # Provision new node
```

**Node behavior:**
1. On boot: Load config from NVS
2. If no config: Check server by chip_id
3. If server has config: Apply and save to NVS
4. On config change: Sync to server

### Phase 4: Dashboard Integration

Manager UI additions:
- Node config editor
- Bulk provisioning
- Config templates by role
- Export/import configs

---

## Provisioning Workflows

### New Node (First Flash)

```
1. Flash firmware via USB
2. Node boots, no config in NVS
3. Node uses default name "Node-XXXX" (from chip ID)
4. Via serial or mesh:
   > config set name "PIR-Kitchen"
   > config set zone "kitchen"
5. Config saved to NVS
6. If gateway online: syncs to server
```

### OTA Update (Config Preserved)

```
1. Server pushes OTA update
2. Node downloads, flashes new firmware
3. Node reboots
4. NVS partition untouched
5. Config loaded from NVS
6. Node continues with same identity
```

### Manual Reflash (Config Preserved)

```
1. Arduino IDE: Upload sketch
2. Only firmware partition erased
3. NVS partition preserved (unless Tools > Erase Flash: All)
4. Node reboots with config intact
```

### Full Erase Recovery

```
1. User runs: esptool.py erase_flash
2. All flash erased including NVS
3. User reflashes firmware
4. Node boots with no config
5. If server has backup:
   - Node requests config by chip_id
   - Server responds with saved config
   - Node applies and saves to NVS
6. If no server backup:
   - Manual provisioning required
```

---

## Serial Commands Reference

```
# Identity
status              # Shows chip_id, name, zone, role

# Config management
config              # List all config values
config get <key>    # Get single value
config set <key> <value>  # Set value (saves immediately)
config reset        # Reset to defaults
config export       # Print JSON
config import       # Enter JSON input mode

# Provisioning
provision           # Enter provisioning mode (WiFi AP)
factory-reset       # Erase config and reboot
```

---

## Mesh Commands

Config can be updated via mesh messages:

```cpp
// MSG_COMMAND type with config subcommand
{
  "t": 5,
  "cmd": "config",
  "action": "set",
  "key": "zone",
  "value": "bedroom"
}
```

Only coordinator or gateway can send config commands (security).

---

## Implementation Priority

1. **MVP (Phase 1):** NVS storage + serial commands
   - `Preferences.h` integration
   - `config get/set` serial commands
   - Load on boot, save on change

2. **Usability (Phase 2):** Provisioning mode
   - Button-triggered AP mode
   - Simple web config page

3. **Robustness (Phase 3):** Server backup
   - API endpoints for config
   - Sync on boot and change

4. **Convenience (Phase 4):** Dashboard UI
   - Config editor in Manager tab
   - Bulk operations

---

## Partition Table Consideration

Ensure NVS partition exists in partition table:

```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x1E0000,
app1,     app,  ota_1,   0x1F0000,0x1E0000,
spiffs,   data, spiffs,  0x3D0000,0x30000,
```

The default ESP32 partition includes NVS, so no changes needed for most users.

---

## Questions to Resolve

1. **Config schema versioning?** How to handle schema changes across firmware versions
2. **Encryption?** Should sensitive config (WiFi passwords) be encrypted in NVS?
3. **Config validation?** Validate values on set or allow any key-value?
4. **Conflict resolution?** If server and NVS differ, which wins?

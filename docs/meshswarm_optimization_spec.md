# MeshSwarm Library Optimization Specification

**Status:** Phase 1 & 2 COMPLETE

## Completed Work

### Phase 1: Log Levels ✅ (Commit: 9accafc)
- Added compile-time log level macros to MeshSwarmConfig.h
- Levels: NONE, ERROR, WARN, INFO (default), DEBUG
- Subsystem-specific macros: MESH_LOG, STATE_LOG, TELEM_LOG, OTA_LOG, GATEWAY_LOG
- Documented in README.md
- **Savings:** 4-8 KB flash when using ERROR or NONE

### Phase 2: HTTP Request Consolidation ✅ (Commit: 6f55a1c)
- Created MeshSwarmHTTP.inc with shared helpers (httpPost, httpGet, httpGetRange)
- Refactored MeshSwarmTelemetry.inc (2 callsites)
- Refactored MeshSwarmOTA.inc (6 callsites)
- Reduced ~80 lines of duplicated HTTP boilerplate
- **Savings:** 3-5 KB flash

---

## Overview

This specification proposes optimizations to reduce the MeshSwarm library's memory footprint while maintaining full functionality and backward compatibility.

### Current Footprint

| Configuration | Flash Usage | RAM Usage |
|--------------|-------------|-----------|
| Full build (all features) | 58-84 KB | ~12-15 KB |
| Minimal (core only) | 15-20 KB | ~4-6 KB |

### Target Goals

- **10-15 KB flash reduction** in full builds
- **3-5 KB RAM reduction** through better allocation patterns
- **Zero breaking changes** to public API

---

## Current Architecture Analysis

### File Structure

```
src/
├── MeshSwarm.h              (384 lines, 9.6 KB)
├── MeshSwarm.cpp            (586 lines, 15 KB)
├── MeshSwarmConfig.h        (73 lines, 2.4 KB)
└── features/
    ├── MeshSwarmDisplay.inc     (95 lines, 2.4 KB)
    ├── MeshSwarmSerial.inc      (129 lines, 3.9 KB)
    ├── MeshSwarmTelemetry.inc   (180 lines, 4.9 KB)
    ├── MeshSwarmOTA.inc         (392 lines, 12 KB)  ← Largest
    └── MeshSwarmCallbacks.inc   (29 lines, 0.7 KB)

Total: 1,868 lines, ~50 KB source
```

### Memory Consumers

| Component | Instances | Impact |
|-----------|-----------|--------|
| String literals (debug/serial) | 223 | ~8-12 KB flash |
| HTTP client instances | 8 | ~3-5 KB flash |
| JsonDocument allocations | 24 | ~2-3 KB RAM per call |
| STL containers (map/vector) | 8 | Variable |
| Serial.printf calls | 96 | String constants |

---

## Optimization Phases

### Phase 1: PROGMEM String Constants (High Impact)

**Problem**: 223 string literals compiled into RAM-consuming flash.

**Solution**: Move static strings to program memory.

#### Implementation

**Step 1: Create string table**

```cpp
// MeshSwarmStrings.h
#ifndef MESHSWARM_STRINGS_H
#define MESHSWARM_STRINGS_H

#include <avr/pgmspace.h>

// Message prefixes
const char STR_MESH[] PROGMEM = "[MESH] ";
const char STR_STATE[] PROGMEM = "[STATE] ";
const char STR_TELEM[] PROGMEM = "[TELEM] ";
const char STR_OTA[] PROGMEM = "[OTA] ";

// Common messages
const char STR_NODE_ID[] PROGMEM = "Node ID: %u";
const char STR_CONNECTED[] PROGMEM = "Connected to mesh";
const char STR_DISCONNECTED[] PROGMEM = "Node %u disconnected";

// Serial commands
const char STR_CMD_STATUS[] PROGMEM = "status";
const char STR_CMD_PEERS[] PROGMEM = "peers";
const char STR_CMD_STATE[] PROGMEM = "state";
const char STR_CMD_SET[] PROGMEM = "set";
const char STR_CMD_GET[] PROGMEM = "get";
const char STR_CMD_SYNC[] PROGMEM = "sync";
const char STR_CMD_REBOOT[] PROGMEM = "reboot";

#endif
```

**Step 2: Use PROGMEM-aware printing**

```cpp
// Before
Serial.printf("[MESH] Node ID: %u\n", myId);

// After
Serial.printf_P(PSTR("[MESH] Node ID: %u\n"), myId);

// Or with string table
char buf[64];
strcpy_P(buf, STR_MESH);
Serial.print(buf);
Serial.printf_P(STR_NODE_ID, myId);
```

**Step 3: ESP32-specific optimization**

ESP32 stores strings in flash by default, but explicit PROGMEM ensures consistency:

```cpp
// ESP32-optimized
#ifdef ESP32
  #define PROGMEM_STR(s) s  // Already in flash
#else
  #define PROGMEM_STR(s) PSTR(s)
#endif

Serial.printf(PROGMEM_STR("[MESH] Node ID: %u\n"), myId);
```

**Expected Savings**: 8-12 KB flash (ESP8266), 2-4 KB consistency improvement (ESP32)

---

### Phase 2: HTTP Request Consolidation (Medium Impact)

**Problem**: Duplicated HTTP request patterns across Telemetry and OTA modules.

**Current Pattern** (repeated 8 times):
```cpp
HTTPClient http;
http.begin(url);
http.addHeader("Content-Type", "application/json");
http.setTimeout(5000);
int code = http.POST(payload);
String response = http.getString();
http.end();
```

**Solution**: Create shared HTTP helper.

#### Implementation

**Add to MeshSwarm.cpp:**

```cpp
// Private HTTP helper (inline to avoid call overhead)
int MeshSwarm::httpPost(const String& url, const String& payload,
                        String* response, int timeout) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (!apiKey.isEmpty()) {
    http.addHeader("X-API-Key", apiKey);
  }
  http.setTimeout(timeout);

  int code = http.POST(payload);
  if (response && code > 0) {
    *response = http.getString();
  }
  http.end();
  return code;
}

int MeshSwarm::httpGet(const String& url, String* response,
                       int timeout, int rangeStart, int rangeEnd) {
  HTTPClient http;
  http.begin(url);
  if (!apiKey.isEmpty()) {
    http.addHeader("X-API-Key", apiKey);
  }
  if (rangeStart >= 0 && rangeEnd >= rangeStart) {
    http.addHeader("Range", "bytes=" + String(rangeStart) + "-" + String(rangeEnd));
  }
  http.setTimeout(timeout);

  int code = http.GET();
  if (response && code > 0) {
    *response = http.getString();
  }
  http.end();
  return code;
}
```

**Update Telemetry module:**

```cpp
// Before
HTTPClient http;
http.begin(telemetryUrl + "/api/v1/telemetry");
http.addHeader("Content-Type", "application/json");
// ... 6 more lines

// After
String response;
int code = httpPost(telemetryUrl + "/api/v1/telemetry", payload, &response);
```

**Expected Savings**: 3-5 KB flash (code deduplication)

---

### Phase 3: JSON Document Reuse (Medium Impact)

**Problem**: Creating new JsonDocument on every message, causing stack churn.

**Current Pattern:**
```cpp
JsonDocument createMsg(int type) {
  JsonDocument doc;  // Stack allocation each call
  doc["t"] = type;
  doc["from"] = mesh.getNodeId();
  return doc;  // Copy on return
}
```

**Solution**: Reuse class-level document with clear pattern.

#### Implementation

**Option A: Static document with mutex (thread-safe)**

```cpp
// MeshSwarm.h
class MeshSwarm {
private:
  StaticJsonDocument<512> msgDoc;  // Reusable

  JsonDocument& prepareMsg(int type) {
    msgDoc.clear();
    msgDoc["t"] = type;
    msgDoc["from"] = mesh.getNodeId();
    return msgDoc;
  }
};
```

**Option B: Caller-provided document**

```cpp
void MeshSwarm::broadcastState(const String& key, const StateEntry& entry) {
  StaticJsonDocument<256> doc;
  doc["t"] = MSG_STATE_SET;
  doc["from"] = mesh.getNodeId();
  doc["key"] = key;
  doc["val"] = entry.value;
  doc["ver"] = entry.version;
  doc["orig"] = entry.origin;

  String msg;
  serializeJson(doc, msg);
  mesh.sendBroadcast(msg);
}
```

**Option C: StaticJsonDocument with exact sizing**

Replace dynamic sizing with calculated static sizes:

```cpp
// Message type sizes (calculated)
#define JSON_MSG_HEARTBEAT_SIZE 192
#define JSON_MSG_STATE_SET_SIZE 256
#define JSON_MSG_STATE_SYNC_SIZE 1024
#define JSON_MSG_TELEMETRY_SIZE 512

void sendHeartbeat() {
  StaticJsonDocument<JSON_MSG_HEARTBEAT_SIZE> doc;
  // ...
}
```

**Expected Savings**: 2-3 KB RAM (reduced stack pressure), ~1 KB flash

---

### Phase 4: OTA Module Optimization (Medium Impact)

**Problem**: OTA module is 392 lines (40% of feature code) with redundancy.

#### Optimizations

**A. Consolidate progress reporting**

```cpp
// Before: 3 separate methods
void reportOTAProgress(int percent);
void reportOTAStatus(const String& status);
void sendOTAResult(bool success, const String& message);

// After: Single unified method
void reportOTA(int percent, const String& status, bool complete = false);
```

**B. Extract URL building**

```cpp
// Before (repeated)
String url = telemetryUrl + "/api/v1/ota/updates/" + String(updateId);

// After
String buildOtaUrl(const char* endpoint, int id = -1) {
  String url = telemetryUrl + "/api/v1/ota/";
  url += endpoint;
  if (id >= 0) {
    url += "/";
    url += String(id);
  }
  return url;
}
```

**C. Combine firmware check methods**

```cpp
// Before: Separate poll and check
void pollForUpdates();
void checkFirmwareVersion();
bool hasNewFirmware();

// After: Single entry point
OTAUpdateInfo* checkForUpdate();  // Returns null if no update
```

**Expected Savings**: 2-3 KB flash

---

### Phase 5: Conditional Serial Logging Levels

**Problem**: Even with MESHSWARM_ENABLE_SERIAL, all 96 log statements are included.

**Solution**: Add log levels to reduce compiled strings.

#### Implementation

```cpp
// MeshSwarmConfig.h
#define MESHSWARM_LOG_NONE    0
#define MESHSWARM_LOG_ERROR   1
#define MESHSWARM_LOG_WARN    2
#define MESHSWARM_LOG_INFO    3
#define MESHSWARM_LOG_DEBUG   4

#ifndef MESHSWARM_LOG_LEVEL
  #define MESHSWARM_LOG_LEVEL MESHSWARM_LOG_INFO
#endif

// Logging macros
#if MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_ERROR
  #define LOG_ERROR(fmt, ...) Serial.printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_ERROR(fmt, ...)
#endif

#if MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_INFO
  #define LOG_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_INFO(fmt, ...)
#endif

#if MESHSWARM_LOG_LEVEL >= MESHSWARM_LOG_DEBUG
  #define LOG_DEBUG(fmt, ...) Serial.printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_DEBUG(fmt, ...)
#endif
```

**Usage:**

```cpp
// Before
Serial.printf("[MESH] Node ID: %u\n", myId);
Serial.printf("[MESH] Received state: %s\n", key.c_str());

// After
LOG_INFO("Node ID: %u", myId);
LOG_DEBUG("Received state: %s", key.c_str());
```

**Build with minimal logging:**
```cpp
#define MESHSWARM_LOG_LEVEL MESHSWARM_LOG_ERROR
#include <MeshSwarm.h>
```

**Expected Savings**: 4-8 KB flash (depending on log level)

---

### Phase 6: Fixed-Size Alternatives (Low Impact)

**Problem**: STL containers (std::map, std::vector) have overhead.

**Solution**: Offer compile-time fixed-size alternatives.

#### Implementation

```cpp
// MeshSwarmConfig.h
#define MESHSWARM_MAX_PEERS 16
#define MESHSWARM_MAX_STATE_ENTRIES 32
#define MESHSWARM_MAX_WATCHERS 8

#ifdef MESHSWARM_USE_FIXED_CONTAINERS
  // Fixed-size peer array
  struct PeerList {
    Peer peers[MESHSWARM_MAX_PEERS];
    uint8_t count = 0;

    Peer* find(uint32_t id);
    bool add(const Peer& p);
    void remove(uint32_t id);
  };

  // Fixed-size state map
  struct StateMap {
    struct Entry {
      char key[24];
      StateEntry value;
      bool used = false;
    };
    Entry entries[MESHSWARM_MAX_STATE_ENTRIES];

    StateEntry* get(const char* key);
    bool set(const char* key, const StateEntry& val);
  };
#else
  // Default: Use STL containers
  typedef std::map<uint32_t, Peer> PeerList;
  typedef std::map<String, StateEntry> StateMap;
#endif
```

**Trade-offs:**
- Fixed containers: Smaller code, predictable memory, limited capacity
- STL containers: Dynamic sizing, more code, heap fragmentation risk

**Expected Savings**: 1-2 KB flash, more predictable RAM

---

## Summary: Expected Savings

| Phase | Optimization | Flash Savings | RAM Savings |
|-------|-------------|---------------|-------------|
| 1 | PROGMEM strings | 8-12 KB | - |
| 2 | HTTP consolidation | 3-5 KB | - |
| 3 | JSON reuse | 1 KB | 2-3 KB |
| 4 | OTA refactoring | 2-3 KB | - |
| 5 | Log levels | 4-8 KB | - |
| 6 | Fixed containers | 1-2 KB | 1-2 KB |
| **Total** | | **19-31 KB** | **3-5 KB** |

**Realistic Target**: 10-15 KB flash reduction (phases 1, 2, 5)

---

## Implementation Priority

### Must Have (Phase 1)
1. **Log levels** - Immediate impact, non-breaking
2. **PROGMEM strings** - Standard optimization pattern

### Should Have (Phase 2)
3. **HTTP consolidation** - Code cleanup + savings
4. **JSON document reuse** - RAM optimization

### Nice to Have (Phase 3)
5. **OTA refactoring** - Maintainability + minor savings
6. **Fixed containers** - Special use cases only

---

## Backward Compatibility

All optimizations maintain 100% API compatibility:

| Change | Compatibility |
|--------|--------------|
| PROGMEM strings | Transparent |
| HTTP helpers | Internal only |
| JSON reuse | Internal only |
| Log levels | Opt-in via define |
| Fixed containers | Opt-in via define |

**No sketch changes required** for default configuration.

---

## Configuration Options (After Optimization)

```cpp
// MeshSwarmConfig.h - Full configuration

// Feature toggles (existing)
#define MESHSWARM_ENABLE_DISPLAY    1
#define MESHSWARM_ENABLE_SERIAL     1
#define MESHSWARM_ENABLE_TELEMETRY  1
#define MESHSWARM_ENABLE_OTA        1
#define MESHSWARM_ENABLE_CALLBACKS  1

// NEW: Log level (0=none, 1=error, 2=warn, 3=info, 4=debug)
#define MESHSWARM_LOG_LEVEL         3

// NEW: Use fixed-size containers (optional)
// #define MESHSWARM_USE_FIXED_CONTAINERS

// NEW: Capacity limits (if using fixed containers)
#define MESHSWARM_MAX_PEERS         16
#define MESHSWARM_MAX_STATE_ENTRIES 32
#define MESHSWARM_MAX_WATCHERS      8
```

---

## Build Size Profiles

### Profile: Full Featured (Current)
```cpp
// All features enabled, info logging
#define MESHSWARM_LOG_LEVEL 3
```
**Expected**: 58-70 KB flash

### Profile: Production
```cpp
// All features, error-only logging
#define MESHSWARM_LOG_LEVEL 1
```
**Expected**: 45-55 KB flash

### Profile: Minimal Sensor Node
```cpp
#define MESHSWARM_ENABLE_DISPLAY    0
#define MESHSWARM_ENABLE_SERIAL     0
#define MESHSWARM_ENABLE_TELEMETRY  1
#define MESHSWARM_ENABLE_OTA        0
#define MESHSWARM_LOG_LEVEL         0
```
**Expected**: 25-30 KB flash

### Profile: Ultra Minimal
```cpp
#define MESHSWARM_ENABLE_DISPLAY    0
#define MESHSWARM_ENABLE_SERIAL     0
#define MESHSWARM_ENABLE_TELEMETRY  0
#define MESHSWARM_ENABLE_OTA        0
```
**Expected**: 15-20 KB flash

---

## Testing Strategy

### Compile Size Verification

```bash
# Before optimization
pio run -e minimal 2>&1 | grep "RAM\|Flash"

# After each phase
pio run -e minimal 2>&1 | grep "RAM\|Flash"
```

### Functional Verification

1. **State sync** - Verify key-value propagation
2. **Coordinator election** - Verify lowest-ID wins
3. **OTA updates** - Full update cycle
4. **Telemetry** - Gateway push to server
5. **Display** - OLED rendering
6. **Serial** - Command processing

### Memory Profiling

```cpp
// Add to sketch
void printMemory() {
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
  Serial.printf("Min free heap: %u\n", ESP.getMinFreeHeap());
  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free sketch: %u\n", ESP.getFreeSketchSpace());
}
```

---

## Questions to Resolve

1. **PROGMEM on ESP32**: Is explicit PROGMEM beneficial or redundant?
2. **Log level default**: Should production default be INFO or WARN?
3. **Fixed containers**: Worth the complexity for most users?
4. **HTTP keepalive**: Would connection reuse help telemetry frequency?

---

## Timeline

| Phase | Scope | Effort |
|-------|-------|--------|
| Phase 1 | Log levels + PROGMEM | Small |
| Phase 2 | HTTP consolidation | Small |
| Phase 3 | JSON reuse | Medium |
| Phase 4 | OTA refactor | Medium |
| Phase 5-6 | Fixed containers | Large (optional) |

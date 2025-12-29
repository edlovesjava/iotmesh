# PlatformIO Migration Specification

## Overview

Migrate from Arduino IDE/CLI to PlatformIO for a more professional, reproducible build system with better dependency management, multi-environment support, and CI/CD integration.

## Why PlatformIO?

| Feature | Arduino IDE/CLI | PlatformIO |
|---------|-----------------|------------|
| Dependency management | Manual library install | `platformio.ini` declares all deps |
| Multi-board builds | Run separately | Single `pio run` builds all |
| Version pinning | No | Yes, exact versions |
| Build flags per target | Difficult | Native support |
| Unit testing | External | Built-in `pio test` |
| CI/CD integration | Custom scripts | First-class support |
| IDE support | Arduino IDE only | VSCode, CLion, Atom, CLI |
| Library isolation | Global | Per-project |
| Upload + monitor | Separate | `pio run -t upload && pio device monitor` |

---

## Current State

```
iotmesh/
├── firmware/
│   ├── mesh_shared_state_pir/
│   │   └── mesh_shared_state_pir.ino
│   ├── mesh_shared_state_led/
│   ├── mesh_shared_state_button/
│   ├── mesh_shared_state_button2/
│   ├── mesh_shared_state_dht11/
│   ├── mesh_shared_state_watcher/
│   └── mesh_gateway/
├── MeshSwarm/                    # Library (separate repo now)
├── scripts/
│   ├── build.ps1                 # Arduino CLI wrapper
│   └── deploy.ps1
└── server/
```

**Current build:** Arduino CLI via PowerShell scripts

---

## Target State

```
iotmesh/
├── firmware/
│   ├── platformio.ini            # Main PIO config
│   ├── src/                      # Shared source (if any)
│   ├── lib/                      # Local libraries
│   │   └── MeshSwarm/            # Or reference from registry
│   ├── nodes/
│   │   ├── pir/
│   │   │   └── main.cpp
│   │   ├── led/
│   │   │   └── main.cpp
│   │   ├── button/
│   │   │   └── main.cpp
│   │   ├── dht/
│   │   │   └── main.cpp
│   │   ├── watcher/
│   │   │   └── main.cpp
│   │   └── gateway/
│   │       └── main.cpp
│   ├── include/
│   │   └── credentials.h         # WiFi creds (gitignored)
│   └── test/                     # Unit tests
├── scripts/
│   ├── build.ps1                 # Wrapper for pio run
│   ├── deploy.ps1                # Build + OTA upload
│   └── flash.ps1                 # Direct USB flash
└── server/
```

---

## PlatformIO Configuration

### `firmware/platformio.ini`

```ini
; PlatformIO Project Configuration
; https://docs.platformio.org/page/projectconf.html

[platformio]
default_envs = pir, led, button, dht, watcher, gateway
src_dir = nodes
lib_dir = lib
include_dir = include

; ============================================================
; Common settings for all ESP32 nodes
; ============================================================
[env]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600

; Partition scheme for OTA
board_build.partitions = min_spiffs.csv

; Common libraries
lib_deps =
    painlessmesh/painlessMesh @ ^1.5.0
    bblanchon/ArduinoJson @ ^7.0.0
    adafruit/Adafruit SSD1306 @ ^2.5.0
    adafruit/Adafruit GFX Library @ ^1.11.0
    ; MeshSwarm from GitHub or local
    https://github.com/edlovesjava/MeshSwarm.git#v1.1.0

; Build flags
build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DARDUINO_ARCH_ESP32

; ============================================================
; Node-specific environments
; ============================================================

[env:pir]
build_src_filter = +<pir/>
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"pir\"
    -DNODE_NAME=\"PIR\"
    -DENABLE_PIR_SENSOR

[env:led]
build_src_filter = +<led/>
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"led\"
    -DNODE_NAME=\"LED\"
    -DENABLE_LED_OUTPUT

[env:button]
build_src_filter = +<button/>
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"button\"
    -DNODE_NAME=\"Button\"
    -DENABLE_BUTTON_INPUT

[env:button2]
build_src_filter = +<button/>
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"button2\"
    -DNODE_NAME=\"Button2\"
    -DENABLE_BUTTON_INPUT
    -DBUTTON_PIN=4

[env:dht]
build_src_filter = +<dht/>
lib_deps =
    ${env.lib_deps}
    adafruit/DHT sensor library @ ^1.4.0
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"dht\"
    -DNODE_NAME=\"DHTNode\"
    -DENABLE_DHT_SENSOR

[env:watcher]
build_src_filter = +<watcher/>
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"watcher\"
    -DNODE_NAME=\"Watcher\"

[env:gateway]
build_src_filter = +<gateway/>
build_flags =
    ${env.build_flags}
    -DNODE_TYPE=\"gateway\"
    -DNODE_NAME=\"Gateway\"
    -DENABLE_GATEWAY_MODE
    -DENABLE_TELEMETRY
    -DENABLE_OTA_DISTRIBUTION

; ============================================================
; Development variants
; ============================================================

[env:pir_debug]
extends = env:pir
build_type = debug
build_flags =
    ${env:pir.build_flags}
    -DCORE_DEBUG_LEVEL=4
    -DDEBUG_MODE

; ============================================================
; Hardware variants
; ============================================================

[env:pir_s3]
extends = env:pir
board = esp32-s3-devkitc-1
build_flags =
    ${env:pir.build_flags}
    -DBOARD_ESP32S3

[env:gateway_s3]
extends = env:gateway
board = esp32-s3-devkitc-1
```

---

## Node Source Structure

### `firmware/nodes/pir/main.cpp`

```cpp
/**
 * PIR Motion Sensor Node
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// Build-time configuration from platformio.ini
#ifndef NODE_NAME
#define NODE_NAME "PIR"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "pir"
#endif

#define PIR_PIN 4

MeshSwarm swarm;

volatile bool motionDetected = false;
unsigned long lastMotionTime = 0;
const unsigned long motionTimeout = 30000;

void IRAM_ATTR onMotion() {
  motionDetected = true;
}

void setup() {
  Serial.begin(115200);

  esp_ota_mark_app_valid_cancel_rollback();

  pinMode(PIR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), onMotion, RISING);

  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

  swarm.onLoop([]() {
    if (motionDetected) {
      motionDetected = false;
      lastMotionTime = millis();
      swarm.setState("motion", "1");
    } else if (swarm.getState("motion") == "1" &&
               millis() - lastMotionTime > motionTimeout) {
      swarm.setState("motion", "0");
    }
  });
}

void loop() {
  swarm.update();
}
```

### `firmware/nodes/gateway/main.cpp`

```cpp
/**
 * Gateway Node - Bridges mesh to server
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// Include credentials (gitignored)
#if __has_include("credentials.h")
#include "credentials.h"
#else
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#define SERVER_URL "http://192.168.1.100:8000"
#endif

#ifndef NODE_NAME
#define NODE_NAME "Gateway"
#endif

MeshSwarm swarm;

void setup() {
  Serial.begin(115200);

  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin(NODE_NAME);
  swarm.connectToWiFi(WIFI_SSID, WIFI_PASS);
  swarm.setGatewayMode(true);
  swarm.setTelemetryServer(SERVER_URL);
  swarm.enableTelemetry(true);
  swarm.enableOTADistribution(true);
}

void loop() {
  swarm.update();
  swarm.checkForOTAUpdates();
}
```

### `firmware/include/credentials.h.example`

```cpp
// Copy to credentials.h and fill in your values
// credentials.h is gitignored

#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"
#define SERVER_URL "http://192.168.1.100:8000"
```

---

## Build Commands

### Basic Usage

```bash
# Build all environments
pio run

# Build specific node
pio run -e pir

# Build and upload
pio run -e pir -t upload

# Upload to specific port
pio run -e pir -t upload --upload-port COM3

# Monitor serial
pio device monitor

# Upload and monitor
pio run -e pir -t upload && pio device monitor

# Clean build
pio run -e pir -t clean

# List connected devices
pio device list
```

### Multi-Node Operations

```bash
# Build all node types
pio run -e pir -e led -e button -e gateway

# Build release binaries for OTA
pio run -e pir -e led -e button -e dht -e watcher -e gateway
# Binaries at: .pio/build/<env>/firmware.bin
```

---

## Scripts Integration

All scripts use Bash for portability across Linux, macOS, WSL, and Git Bash on Windows.

### `scripts/build.sh`

```bash
#!/usr/bin/env bash
set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FIRMWARE_DIR="$PROJECT_DIR/firmware"
BUILD_DIR="$PROJECT_DIR/build"

ENVS=(pir led button button2 dht watcher gateway)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [node_type|all] [--version VERSION]"
    echo ""
    echo "Node types: ${ENVS[*]}"
    echo ""
    echo "Examples:"
    echo "  $0 pir                    # Build PIR firmware"
    echo "  $0 all                    # Build all firmware"
    echo "  $0 pir --version 1.2.0    # Build with version tag"
    exit 1
}

# Parse arguments
NODE_TYPE=""
VERSION="dev"

while [[ $# -gt 0 ]]; do
    case $1 in
        --version|-v)
            VERSION="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            NODE_TYPE="$1"
            shift
            ;;
    esac
done

[[ -z "$NODE_TYPE" ]] && NODE_TYPE="all"

cd "$FIRMWARE_DIR"

echo -e "${CYAN}======================================${NC}"
echo -e "${CYAN}  PlatformIO Build${NC}"
echo -e "${CYAN}  Version: $VERSION${NC}"
echo -e "${CYAN}======================================${NC}"
echo ""

# Determine which environments to build
if [[ "$NODE_TYPE" == "all" ]]; then
    BUILD_ENVS=("${ENVS[@]}")
    pio run
else
    BUILD_ENVS=("$NODE_TYPE")
    pio run -e "$NODE_TYPE"
fi

# Copy binaries to build/ with version
echo ""
echo -e "${CYAN}Copying binaries...${NC}"

for env in "${BUILD_ENVS[@]}"; do
    src=".pio/build/$env/firmware.bin"
    if [[ -f "$src" ]]; then
        dest_dir="$BUILD_DIR/$env"
        mkdir -p "$dest_dir"
        dest="$dest_dir/${env}_${VERSION}.bin"
        cp "$src" "$dest"

        size=$(stat -f%z "$dest" 2>/dev/null || stat -c%s "$dest")
        size_kb=$(echo "scale=1; $size / 1024" | bc)
        echo -e "  ${GREEN}$env${NC} -> $dest ($size_kb KB)"
    fi
done

echo ""
echo -e "${GREEN}Build complete!${NC}"
```

### `scripts/flash.sh`

```bash
#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")/firmware"

usage() {
    echo "Usage: $0 <node_type> [port]"
    echo ""
    echo "Examples:"
    echo "  $0 pir              # Auto-detect port"
    echo "  $0 pir /dev/ttyUSB0 # Specify port"
    echo "  $0 led COM3         # Windows port"
    exit 1
}

[[ -z "$1" ]] && usage

NODE_TYPE="$1"
PORT="$2"

cd "$FIRMWARE_DIR"

if [[ -n "$PORT" ]]; then
    pio run -e "$NODE_TYPE" -t upload --upload-port "$PORT"
else
    pio run -e "$NODE_TYPE" -t upload
fi

echo ""
echo "Flash complete. Starting monitor..."
pio device monitor
```

### `scripts/deploy.sh`

```bash
#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

# Server URL from env or default
SERVER_URL="${MESH_SERVER_URL:-http://localhost:8000}"

usage() {
    echo "Usage: $0 <node_type> <version> [--force] [--notes 'Release notes']"
    echo ""
    echo "Examples:"
    echo "  $0 pir 1.2.0"
    echo "  $0 pir 1.2.0 --force --notes 'Bug fixes'"
    exit 1
}

# Parse arguments
NODE_TYPE=""
VERSION=""
FORCE="false"
NOTES=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --force)
            FORCE="true"
            shift
            ;;
        --notes)
            NOTES="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            if [[ -z "$NODE_TYPE" ]]; then
                NODE_TYPE="$1"
            elif [[ -z "$VERSION" ]]; then
                VERSION="$1"
            fi
            shift
            ;;
    esac
done

[[ -z "$NODE_TYPE" || -z "$VERSION" ]] && usage

BIN_FILE="$BUILD_DIR/$NODE_TYPE/${NODE_TYPE}_${VERSION}.bin"

# Step 1: Build if not exists
if [[ ! -f "$BIN_FILE" ]]; then
    echo "Binary not found, building..."
    "$SCRIPT_DIR/build.sh" "$NODE_TYPE" --version "$VERSION"
fi

[[ ! -f "$BIN_FILE" ]] && { echo "Build failed!"; exit 1; }

echo ""
echo "=========================================="
echo "  Deploying: $NODE_TYPE v$VERSION"
echo "  Server:    $SERVER_URL"
echo "=========================================="

# Step 2: Upload firmware
echo ""
echo "Uploading firmware..."

UPLOAD_RESPONSE=$(curl -s -X POST "$SERVER_URL/api/v1/firmware/upload" \
    -F "file=@$BIN_FILE" \
    -F "node_type=$NODE_TYPE" \
    -F "version=$VERSION" \
    -F "hardware=ESP32" \
    -F "release_notes=$NOTES" \
    -F "is_stable=true")

FIRMWARE_ID=$(echo "$UPLOAD_RESPONSE" | grep -o '"id":[0-9]*' | grep -o '[0-9]*')

if [[ -z "$FIRMWARE_ID" ]]; then
    echo "Upload failed: $UPLOAD_RESPONSE"
    exit 1
fi

echo "  Uploaded firmware ID: $FIRMWARE_ID"

# Step 3: Create OTA update
echo ""
echo "Creating OTA update job..."

OTA_RESPONSE=$(curl -s -X POST "$SERVER_URL/api/v1/ota/updates" \
    -H "Content-Type: application/json" \
    -d "{\"firmware_id\": $FIRMWARE_ID, \"force_update\": $FORCE}")

UPDATE_ID=$(echo "$OTA_RESPONSE" | grep -o '"id":[0-9]*' | grep -o '[0-9]*')

if [[ -z "$UPDATE_ID" ]]; then
    echo "OTA job creation failed: $OTA_RESPONSE"
    exit 1
fi

echo "  Created OTA update ID: $UPDATE_ID"

echo ""
echo "=========================================="
echo "  Deployment Complete!"
echo "  Monitor: $SERVER_URL/api/v1/ota/updates/$UPDATE_ID"
echo "=========================================="
```

### `scripts/test.sh`

```bash
#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")/firmware"

ENVS=(pir led button button2 dht watcher gateway)

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

cd "$FIRMWARE_DIR"

echo -e "${CYAN}======================================${NC}"
echo -e "${CYAN}  MeshSwarm Compile Test${NC}"
echo -e "${CYAN}======================================${NC}"
echo ""

passed=0
failed=0

for env in "${ENVS[@]}"; do
    printf "  Compiling: %-12s ... " "$env"

    if pio run -e "$env" -s > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((passed++))
    else
        echo -e "${RED}FAIL${NC}"
        ((failed++))
    fi
done

echo ""
echo -e "${CYAN}======================================${NC}"
echo -e "  Passed: ${GREEN}$passed${NC}"
[[ $failed -gt 0 ]] && echo -e "  Failed: ${RED}$failed${NC}"
echo -e "${CYAN}======================================${NC}"

[[ $failed -gt 0 ]] && exit 1
exit 0
```

---

## CI/CD Integration

### `.github/workflows/build.yml`

```yaml
name: Build Firmware

on:
  push:
    branches: [main]
    paths:
      - 'firmware/**'
  pull_request:
    branches: [main]
  workflow_dispatch:
    inputs:
      version:
        description: 'Version tag'
        required: true
        default: 'dev'

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install PlatformIO
        run: pip install platformio

      - name: Build all firmware
        run: |
          cd firmware
          pio run

      - name: Prepare artifacts
        run: |
          mkdir -p artifacts
          for env in pir led button button2 dht watcher gateway; do
            if [ -f firmware/.pio/build/$env/firmware.bin ]; then
              cp firmware/.pio/build/$env/firmware.bin artifacts/${env}_${{ github.event.inputs.version || 'dev' }}.bin
            fi
          done

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware-${{ github.sha }}
          path: artifacts/*.bin
          retention-days: 30
```

---

## VSCode Integration

### `.vscode/extensions.json`

```json
{
  "recommendations": [
    "platformio.platformio-ide"
  ]
}
```

### `.vscode/settings.json`

```json
{
  "platformio-ide.customPATH": "",
  "platformio-ide.defaultToolbarBuildAction": "none",
  "files.associations": {
    "platformio.ini": "ini"
  },
  "C_Cpp.default.configurationProvider": "platformio.platformio-ide"
}
```

---

## Migration Steps

### Phase 1: Setup (1-2 hours)

1. **Install PlatformIO**
   ```bash
   pip install platformio
   # Or install VSCode extension
   ```

2. **Create directory structure**
   ```bash
   mkdir -p firmware/nodes/{pir,led,button,dht,watcher,gateway}
   mkdir -p firmware/include
   mkdir -p firmware/lib
   ```

3. **Create `platformio.ini`**
   - Copy from spec above
   - Adjust board if not esp32dev

4. **Create `credentials.h.example`**
   - Add to `.gitignore`: `credentials.h`

### Phase 2: Convert Sketches (2-3 hours)

1. **Rename `.ino` to `main.cpp`**
   ```bash
   mv mesh_shared_state_pir.ino main.cpp
   ```

2. **Add `#include <Arduino.h>`** at top of each file

3. **Move to `nodes/<type>/main.cpp`**

4. **Update includes**
   - Remove Arduino IDE relative paths
   - Use standard includes

5. **Extract build flags**
   - Move `#define` to `platformio.ini` build_flags

### Phase 3: Test Builds (1 hour)

```bash
cd firmware

# Test single build
pio run -e pir

# Test all builds
pio run

# Test upload
pio run -e pir -t upload
```

### Phase 4: Update Scripts (1 hour)

1. Update `build.ps1` to use `pio run`
2. Update `deploy.ps1` to use PIO output path
3. Test OTA flow end-to-end

### Phase 5: CI/CD (1 hour)

1. Add GitHub Actions workflow
2. Test on PR
3. Add artifact upload

---

## Rollback Plan

Keep Arduino CLI scripts as backup:

```
scripts/
├── build.sh            # PlatformIO (primary)
├── build-arduino.sh    # Arduino CLI (backup)
├── deploy.sh
├── flash.sh
└── test.sh
```

If PlatformIO has issues, can revert to Arduino CLI by renaming scripts.

---

## Platform Compatibility

All scripts use `#!/usr/bin/env bash` and work on:

| Platform | Shell | Notes |
|----------|-------|-------|
| Linux | Native bash | Works out of box |
| macOS | Native bash/zsh | Works out of box |
| Windows + WSL | WSL bash | Full Linux environment |
| Windows + Git Bash | MINGW bash | Included with Git for Windows |
| Windows + Cygwin | Cygwin bash | Alternative option |

### Windows Users

Recommended: Use **Git Bash** (comes with Git for Windows) or **WSL**.

```bash
# Git Bash - run from project root
./scripts/build.sh pir

# WSL - same commands
./scripts/build.sh pir
```

### Make Scripts Executable

```bash
chmod +x scripts/*.sh
```

---

## Benefits After Migration

1. **Reproducible builds** - Same deps on all machines
2. **Faster iteration** - Better caching, parallel builds
3. **Multi-board support** - Easy to add ESP32-S3, etc.
4. **Version pinning** - No "it worked yesterday" issues
5. **IDE integration** - VSCode IntelliSense works properly
6. **CI/CD ready** - GitHub Actions just works
7. **Unit testing** - `pio test` built-in
8. **Library management** - No manual installs

---

## Questions to Resolve

1. **MeshSwarm source location?**
   - Option A: Git submodule in `lib/`
   - Option B: GitHub dependency in `lib_deps`
   - Option C: PlatformIO Registry (requires publishing)

2. **Credentials handling?**
   - Option A: `credentials.h` file (gitignored)
   - Option B: Environment variables
   - Option C: Build-time injection

3. **Keep Arduino compatibility?**
   - The `.ino` sketches can stay for Arduino IDE users
   - PlatformIO uses separate `main.cpp` files

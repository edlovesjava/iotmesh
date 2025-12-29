# CLI Build Automation Specification

## Overview

Automate firmware builds and OTA deployments using Arduino CLI and shell scripts, eliminating the need for Arduino IDE. This enables CI/CD pipelines, batch builds, and one-command deployments.

## Tools

### Arduino CLI

The official command-line interface for Arduino development.

**Installation:**
```bash
# Windows (PowerShell)
winget install ArduinoSA.ArduinoCLI

# macOS
brew install arduino-cli

# Linux
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Verify installation
arduino-cli version
```

**Initial Setup:**
```bash
# Initialize configuration
arduino-cli config init

# Add ESP32 board support
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Update board index
arduino-cli core update-index

# Install ESP32 core
arduino-cli core install esp32:esp32
```

### Alternative: PlatformIO CLI

More powerful build system with dependency management.

**Installation:**
```bash
# Via pip
pip install platformio

# Verify
pio --version
```

---

## Project Structure for CLI Builds

```
iotmesh/
├── firmware/
│   ├── mesh_shared_state_pir/
│   │   └── mesh_shared_state_pir.ino
│   ├── mesh_shared_state_led/
│   ├── mesh_shared_state_button/
│   ├── mesh_shared_state_dht11/
│   ├── mesh_shared_state_watcher/
│   ├── mesh_gateway/
│   └── credentials.h              # WiFi credentials (gitignored)
├── MeshSwarm/                      # Library
├── scripts/
│   ├── build.sh                   # Build all or specific firmware
│   ├── build.ps1                  # Windows PowerShell version
│   ├── deploy.sh                  # Build + upload to server + create OTA
│   ├── deploy.ps1                 # Windows PowerShell version
│   └── flash.sh                   # Direct USB flash (for initial setup)
├── build/                         # Output directory (gitignored)
│   ├── pir/
│   │   └── mesh_shared_state_pir.ino.bin
│   ├── led/
│   └── ...
└── arduino-cli.yaml               # CLI configuration
```

---

## Arduino CLI Configuration

### `arduino-cli.yaml`
```yaml
board_manager:
  additional_urls:
    - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

directories:
  data: .arduino15
  downloads: .arduino15/staging
  user: .

library:
  enable_unsafe_install: true

sketch:
  always_export_binaries: true

build_cache:
  path: .arduino15/cache
```

### Library Installation
```bash
# Install required libraries
arduino-cli lib install "painlessMesh"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "DHT sensor library"

# Link local MeshSwarm library
# Option 1: Symlink (Linux/macOS)
ln -s $(pwd)/MeshSwarm ~/Arduino/libraries/MeshSwarm

# Option 2: Copy (all platforms)
cp -r MeshSwarm ~/Arduino/libraries/
```

---

## Build Scripts

### `scripts/build.sh` (Linux/macOS)
```bash
#!/bin/bash
set -e

# Configuration
BOARD="esp32:esp32:esp32"
BUILD_DIR="build"
FIRMWARE_DIR="firmware"

# Node types and their sketch directories
declare -A SKETCHES=(
    ["pir"]="mesh_shared_state_pir"
    ["led"]="mesh_shared_state_led"
    ["button"]="mesh_shared_state_button"
    ["button2"]="mesh_shared_state_button2"
    ["dht"]="mesh_shared_state_dht11"
    ["watcher"]="mesh_shared_state_watcher"
    ["gateway"]="mesh_gateway"
)

# Build options
BUILD_PROPS="build.partitions=min_spiffs,upload.maximum_size=1966080"

usage() {
    echo "Usage: $0 [node_type|all] [--version VERSION]"
    echo ""
    echo "Node types: ${!SKETCHES[@]}"
    echo ""
    echo "Examples:"
    echo "  $0 pir                    # Build PIR firmware"
    echo "  $0 all                    # Build all firmware"
    echo "  $0 pir --version 1.2.0    # Build with version tag"
    exit 1
}

build_firmware() {
    local node_type=$1
    local version=${2:-"dev"}
    local sketch_name=${SKETCHES[$node_type]}

    if [ -z "$sketch_name" ]; then
        echo "Error: Unknown node type '$node_type'"
        usage
    fi

    local sketch_path="$FIRMWARE_DIR/$sketch_name/$sketch_name.ino"
    local output_dir="$BUILD_DIR/$node_type"

    echo "=========================================="
    echo "Building: $node_type ($sketch_name)"
    echo "Version:  $version"
    echo "=========================================="

    mkdir -p "$output_dir"

    # Compile
    arduino-cli compile \
        --fqbn "$BOARD" \
        --build-property "$BUILD_PROPS" \
        --output-dir "$output_dir" \
        --warnings default \
        "$sketch_path"

    # Rename output to include version
    local bin_file="$output_dir/$sketch_name.ino.bin"
    local versioned_file="$output_dir/${node_type}_${version}.bin"

    if [ -f "$bin_file" ]; then
        cp "$bin_file" "$versioned_file"

        # Calculate MD5
        local md5=$(md5sum "$versioned_file" | cut -d' ' -f1)
        local size=$(stat -f%z "$versioned_file" 2>/dev/null || stat -c%s "$versioned_file")

        echo ""
        echo "Output:   $versioned_file"
        echo "Size:     $size bytes"
        echo "MD5:      $md5"
        echo ""
    fi
}

# Parse arguments
NODE_TYPE=""
VERSION="dev"

while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
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

if [ -z "$NODE_TYPE" ]; then
    usage
fi

# Build
if [ "$NODE_TYPE" == "all" ]; then
    for type in "${!SKETCHES[@]}"; do
        build_firmware "$type" "$VERSION"
    done
    echo "All builds complete!"
else
    build_firmware "$NODE_TYPE" "$VERSION"
fi
```

### `scripts/build.ps1` (Windows PowerShell)
```powershell
param(
    [Parameter(Position=0)]
    [string]$NodeType,

    [string]$Version = "dev"
)

$ErrorActionPreference = "Stop"

# Configuration
$BOARD = "esp32:esp32:esp32"
$BUILD_DIR = "build"
$FIRMWARE_DIR = "firmware"

$SKETCHES = @{
    "pir"     = "mesh_shared_state_pir"
    "led"     = "mesh_shared_state_led"
    "button"  = "mesh_shared_state_button"
    "button2" = "mesh_shared_state_button2"
    "dht"     = "mesh_shared_state_dht11"
    "watcher" = "mesh_shared_state_watcher"
    "gateway" = "mesh_gateway"
}

$BUILD_PROPS = "build.partitions=min_spiffs,upload.maximum_size=1966080"

function Show-Usage {
    Write-Host "Usage: .\build.ps1 <node_type|all> [-Version VERSION]"
    Write-Host ""
    Write-Host "Node types: $($SKETCHES.Keys -join ', ')"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1 pir"
    Write-Host "  .\build.ps1 all -Version 1.2.0"
    exit 1
}

function Build-Firmware {
    param([string]$Type, [string]$Ver)

    $sketchName = $SKETCHES[$Type]
    if (-not $sketchName) {
        Write-Error "Unknown node type: $Type"
        Show-Usage
    }

    $sketchPath = "$FIRMWARE_DIR\$sketchName\$sketchName.ino"
    $outputDir = "$BUILD_DIR\$Type"

    Write-Host "=========================================="
    Write-Host "Building: $Type ($sketchName)"
    Write-Host "Version:  $Ver"
    Write-Host "=========================================="

    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

    # Compile
    arduino-cli compile `
        --fqbn $BOARD `
        --build-property $BUILD_PROPS `
        --output-dir $outputDir `
        --warnings default `
        $sketchPath

    # Rename with version
    $binFile = "$outputDir\$sketchName.ino.bin"
    $versionedFile = "$outputDir\${Type}_${Ver}.bin"

    if (Test-Path $binFile) {
        Copy-Item $binFile $versionedFile

        $md5 = (Get-FileHash -Algorithm MD5 $versionedFile).Hash.ToLower()
        $size = (Get-Item $versionedFile).Length

        Write-Host ""
        Write-Host "Output:   $versionedFile"
        Write-Host "Size:     $size bytes"
        Write-Host "MD5:      $md5"
        Write-Host ""
    }
}

# Main
if (-not $NodeType) { Show-Usage }

if ($NodeType -eq "all") {
    foreach ($type in $SKETCHES.Keys) {
        Build-Firmware -Type $type -Ver $Version
    }
    Write-Host "All builds complete!"
} else {
    Build-Firmware -Type $NodeType -Ver $Version
}
```

---

## Deployment Scripts

### `scripts/deploy.sh` (Linux/macOS)
```bash
#!/bin/bash
set -e

# Configuration
SERVER_URL="${MESH_SERVER_URL:-http://localhost:8000}"
BUILD_DIR="build"

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
FORCE=""
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
            if [ -z "$NODE_TYPE" ]; then
                NODE_TYPE="$1"
            elif [ -z "$VERSION" ]; then
                VERSION="$1"
            fi
            shift
            ;;
    esac
done

if [ -z "$NODE_TYPE" ] || [ -z "$VERSION" ]; then
    usage
fi

BIN_FILE="$BUILD_DIR/$NODE_TYPE/${NODE_TYPE}_${VERSION}.bin"

# Step 1: Build if not exists
if [ ! -f "$BIN_FILE" ]; then
    echo "Binary not found, building..."
    ./scripts/build.sh "$NODE_TYPE" --version "$VERSION"
fi

# Verify binary exists
if [ ! -f "$BIN_FILE" ]; then
    echo "Error: Build failed, binary not found: $BIN_FILE"
    exit 1
fi

echo ""
echo "=========================================="
echo "Deploying: $NODE_TYPE v$VERSION"
echo "Server:    $SERVER_URL"
echo "=========================================="

# Step 2: Upload firmware to server
echo ""
echo "Uploading firmware..."

UPLOAD_RESPONSE=$(curl -s -X POST "$SERVER_URL/api/v1/firmware/upload" \
    -F "file=@$BIN_FILE" \
    -F "node_type=$NODE_TYPE" \
    -F "version=$VERSION" \
    -F "hardware=ESP32" \
    -F "release_notes=$NOTES" \
    -F "is_stable=true")

FIRMWARE_ID=$(echo "$UPLOAD_RESPONSE" | jq -r '.id')

if [ "$FIRMWARE_ID" == "null" ] || [ -z "$FIRMWARE_ID" ]; then
    echo "Error: Upload failed"
    echo "$UPLOAD_RESPONSE"
    exit 1
fi

echo "Uploaded firmware ID: $FIRMWARE_ID"

# Step 3: Create OTA update job
echo ""
echo "Creating OTA update job..."

OTA_BODY="{\"firmware_id\": $FIRMWARE_ID, \"force_update\": ${FORCE:-false}}"

OTA_RESPONSE=$(curl -s -X POST "$SERVER_URL/api/v1/ota/updates" \
    -H "Content-Type: application/json" \
    -d "$OTA_BODY")

UPDATE_ID=$(echo "$OTA_RESPONSE" | jq -r '.id')

if [ "$UPDATE_ID" == "null" ] || [ -z "$UPDATE_ID" ]; then
    echo "Error: Failed to create OTA update"
    echo "$OTA_RESPONSE"
    exit 1
fi

echo "Created OTA update ID: $UPDATE_ID"

# Step 4: Monitor progress (optional)
echo ""
echo "=========================================="
echo "Deployment initiated!"
echo ""
echo "Monitor progress:"
echo "  curl $SERVER_URL/api/v1/ota/updates/$UPDATE_ID"
echo ""
echo "Or view in dashboard:"
echo "  $SERVER_URL"
echo "=========================================="
```

### `scripts/deploy.ps1` (Windows PowerShell)
```powershell
param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$NodeType,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$Version,

    [switch]$Force,

    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"

# Configuration
$SERVER_URL = $env:MESH_SERVER_URL
if (-not $SERVER_URL) { $SERVER_URL = "http://localhost:8000" }
$BUILD_DIR = "build"

$binFile = "$BUILD_DIR\$NodeType\${NodeType}_${Version}.bin"

# Step 1: Build if not exists
if (-not (Test-Path $binFile)) {
    Write-Host "Binary not found, building..."
    .\scripts\build.ps1 -NodeType $NodeType -Version $Version
}

if (-not (Test-Path $binFile)) {
    Write-Error "Build failed, binary not found: $binFile"
    exit 1
}

Write-Host ""
Write-Host "=========================================="
Write-Host "Deploying: $NodeType v$Version"
Write-Host "Server:    $SERVER_URL"
Write-Host "=========================================="

# Step 2: Upload firmware
Write-Host ""
Write-Host "Uploading firmware..."

$form = @{
    file = Get-Item $binFile
    node_type = $NodeType
    version = $Version
    hardware = "ESP32"
    release_notes = $Notes
    is_stable = "true"
}

$uploadResponse = Invoke-RestMethod -Uri "$SERVER_URL/api/v1/firmware/upload" `
    -Method Post -Form $form

$firmwareId = $uploadResponse.id
Write-Host "Uploaded firmware ID: $firmwareId"

# Step 3: Create OTA update
Write-Host ""
Write-Host "Creating OTA update job..."

$otaBody = @{
    firmware_id = $firmwareId
    force_update = $Force.IsPresent
} | ConvertTo-Json

$otaResponse = Invoke-RestMethod -Uri "$SERVER_URL/api/v1/ota/updates" `
    -Method Post -ContentType "application/json" -Body $otaBody

$updateId = $otaResponse.id
Write-Host "Created OTA update ID: $updateId"

Write-Host ""
Write-Host "=========================================="
Write-Host "Deployment initiated!"
Write-Host ""
Write-Host "Monitor: $SERVER_URL/api/v1/ota/updates/$updateId"
Write-Host "=========================================="
```

---

## Direct USB Flash Script

For initial node setup or recovery.

### `scripts/flash.sh`
```bash
#!/bin/bash
set -e

BOARD="esp32:esp32:esp32"
PORT="${1:-/dev/ttyUSB0}"
NODE_TYPE="${2:-}"

usage() {
    echo "Usage: $0 <port> <node_type>"
    echo ""
    echo "Examples:"
    echo "  $0 /dev/ttyUSB0 pir"
    echo "  $0 COM3 led          # Windows"
    exit 1
}

if [ -z "$PORT" ] || [ -z "$NODE_TYPE" ]; then
    usage
fi

# Map node types to sketches
declare -A SKETCHES=(
    ["pir"]="mesh_shared_state_pir"
    ["led"]="mesh_shared_state_led"
    ["button"]="mesh_shared_state_button"
    ["button2"]="mesh_shared_state_button2"
    ["dht"]="mesh_shared_state_dht11"
    ["watcher"]="mesh_shared_state_watcher"
    ["gateway"]="mesh_gateway"
)

SKETCH_NAME=${SKETCHES[$NODE_TYPE]}
if [ -z "$SKETCH_NAME" ]; then
    echo "Error: Unknown node type '$NODE_TYPE'"
    usage
fi

SKETCH_PATH="firmware/$SKETCH_NAME/$SKETCH_NAME.ino"

echo "=========================================="
echo "Flashing: $NODE_TYPE"
echo "Port:     $PORT"
echo "=========================================="

# Compile and upload
arduino-cli compile --fqbn "$BOARD" "$SKETCH_PATH"
arduino-cli upload --fqbn "$BOARD" --port "$PORT" "$SKETCH_PATH"

echo ""
echo "Flash complete!"
echo "Open serial monitor: arduino-cli monitor -p $PORT -c baudrate=115200"
```

---

## CI/CD Integration

### GitHub Actions Workflow

`.github/workflows/build.yml`
```yaml
name: Build Firmware

on:
  push:
    branches: [main]
    paths:
      - 'firmware/**'
      - 'MeshSwarm/**'
  pull_request:
    branches: [main]
  workflow_dispatch:
    inputs:
      node_type:
        description: 'Node type to build (or "all")'
        required: true
        default: 'all'
      version:
        description: 'Version tag'
        required: true
        default: 'dev'

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Setup Arduino CLI
        uses: arduino/setup-arduino-cli@v1

      - name: Install ESP32 core
        run: |
          arduino-cli config init
          arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
          arduino-cli core update-index
          arduino-cli core install esp32:esp32

      - name: Install libraries
        run: |
          arduino-cli lib install "painlessMesh"
          arduino-cli lib install "ArduinoJson"
          arduino-cli lib install "Adafruit SSD1306"
          arduino-cli lib install "Adafruit GFX Library"
          arduino-cli lib install "DHT sensor library"
          cp -r MeshSwarm ~/Arduino/libraries/

      - name: Build firmware
        run: |
          chmod +x scripts/build.sh
          ./scripts/build.sh ${{ github.event.inputs.node_type || 'all' }} \
            --version ${{ github.event.inputs.version || 'dev' }}

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware-binaries
          path: build/**/*.bin
          retention-days: 30

  deploy:
    needs: build
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main' && github.event_name == 'workflow_dispatch'

    steps:
      - uses: actions/checkout@v4

      - name: Download artifacts
        uses: actions/download-artifact@v4
        with:
          name: firmware-binaries
          path: build

      - name: Deploy to server
        env:
          MESH_SERVER_URL: ${{ secrets.MESH_SERVER_URL }}
        run: |
          chmod +x scripts/deploy.sh
          ./scripts/deploy.sh ${{ github.event.inputs.node_type }} \
            ${{ github.event.inputs.version }} \
            --notes "Automated deployment from GitHub Actions"
```

---

## PlatformIO Alternative

### `platformio.ini`
```ini
[platformio]
default_envs = pir, led, button, dht, watcher, gateway
src_dir = firmware
build_dir = .pio/build

[env]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps =
    painlessMesh
    ArduinoJson
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
lib_extra_dirs = .

[env:pir]
build_src_filter = +<mesh_shared_state_pir/>

[env:led]
build_src_filter = +<mesh_shared_state_led/>

[env:button]
build_src_filter = +<mesh_shared_state_button/>

[env:button2]
build_src_filter = +<mesh_shared_state_button2/>

[env:dht]
build_src_filter = +<mesh_shared_state_dht11/>
lib_deps =
    ${env.lib_deps}
    adafruit/DHT sensor library

[env:watcher]
build_src_filter = +<mesh_shared_state_watcher/>

[env:gateway]
build_src_filter = +<mesh_gateway/>
```

### PlatformIO Commands
```bash
# Build specific target
pio run -e pir

# Build all
pio run

# Upload to connected device
pio run -e pir -t upload

# Build and get binary path
pio run -e pir
# Binary at: .pio/build/pir/firmware.bin

# Monitor serial
pio device monitor
```

---

## Quick Reference

### Arduino CLI Commands
```bash
# List connected boards
arduino-cli board list

# Compile sketch
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/mesh_shared_state_pir/

# Upload to device
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 firmware/mesh_shared_state_pir/

# Serial monitor
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# List installed libraries
arduino-cli lib list

# Search for libraries
arduino-cli lib search painlessMesh
```

### Environment Variables
```bash
# Set server URL for deployment scripts
export MESH_SERVER_URL=http://192.168.1.100:8000

# Windows PowerShell
$env:MESH_SERVER_URL = "http://192.168.1.100:8000"
```

### Common Workflows

**Build single firmware:**
```bash
./scripts/build.sh pir --version 1.2.0
```

**Build and deploy:**
```bash
./scripts/deploy.sh pir 1.2.0 --notes "Added motion debouncing"
```

**Flash new node via USB:**
```bash
./scripts/flash.sh /dev/ttyUSB0 pir
```

**Build all for release:**
```bash
./scripts/build.sh all --version 2.0.0
```

---

## Troubleshooting

### Common Issues

**Board not found:**
```bash
# Update core
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

**Library not found:**
```bash
# Reinstall libraries
arduino-cli lib install "painlessMesh" --no-deps
```

**Permission denied on port:**
```bash
# Linux: Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

**Windows COM port issues:**
```powershell
# List COM ports
Get-WMIObject Win32_SerialPort | Select DeviceID, Description
```

**Upload timeout:**
```bash
# Hold BOOT button during upload
# Or reduce upload speed
arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 ...
```

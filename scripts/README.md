# Build & Deploy Scripts

Command-line tools for building and deploying ESP32 mesh node firmware without the Arduino IDE.

## Prerequisites

- **Arduino CLI** installed and in PATH
  ```powershell
  winget install ArduinoSA.ArduinoCLI
  ```

- **ESP32 core** installed
  ```bash
  arduino-cli core install esp32:esp32
  ```

- **Required libraries** installed
  ```bash
  arduino-cli lib install "painlessMesh" "ArduinoJson" "Adafruit SSD1306" "Adafruit GFX Library" "DHT sensor library"
  ```

## Scripts

### build.ps1 - Compile Firmware

Compiles a node sketch and produces a `.bin` file ready for OTA.

```powershell
# Build a single node type
.\build.ps1 button -Version 1.0.0
.\build.ps1 pir -Version 1.2.0
.\build.ps1 gateway -Version 2.0.0

# Build all node types
.\build.ps1 -All -Version 2.0.0
```

**Node types:** `pir`, `led`, `button`, `button2`, `dht`, `watcher`, `gateway`

**Output:** `build/<node_type>/<node_type>_<version>.bin`

Example:
```
build/
├── button/
│   └── button_1.0.0.bin    # Ready for OTA
├── pir/
│   └── pir_1.2.0.bin
└── led/
    └── led_1.0.0.bin
```

### deploy.ps1 - Build & Upload to Server

Compiles firmware, uploads to the mesh server, and optionally creates an OTA update job.

```powershell
# Build and upload (firmware only)
.\deploy.ps1 button -Version 1.0.0

# Build, upload, and create OTA update job
.\deploy.ps1 button -Version 1.0.0 -Deploy

# With release notes
.\deploy.ps1 button -Version 1.0.0 -Deploy -Notes "Fixed LED timing issue"

# Force update (even if nodes have same firmware)
.\deploy.ps1 pir -Version 2.0.0 -Deploy -Force
```

**Parameters:**
| Parameter | Required | Description |
|-----------|----------|-------------|
| `NodeType` | Yes | Node type to build |
| `-Version` | Yes | Semantic version (e.g., 1.2.0) |
| `-ServerUrl` | No | API server URL (default: `$env:MESH_SERVER_URL` or `http://localhost:8000`) |
| `-Notes` | No | Release notes |
| `-Deploy` | No | Create OTA update job after upload |
| `-Force` | No | Force update even if MD5 matches |

## Environment Variables

```powershell
# Set default server URL
$env:MESH_SERVER_URL = "http://192.168.1.100:8000"
```

## Workflow Examples

### Development Build
```powershell
# Quick build for testing
.\build.ps1 button
# Output: build/button/button_dev.bin
```

### Release Deployment
```powershell
# Build v1.2.0 and push to all button nodes
.\deploy.ps1 button -Version 1.2.0 -Deploy -Notes "Added debouncing"
```

### Build All for Release
```powershell
# Build all node types with same version
.\build.ps1 -All -Version 2.0.0

# Then upload each one
.\deploy.ps1 pir -Version 2.0.0 -Deploy
.\deploy.ps1 led -Version 2.0.0 -Deploy
.\deploy.ps1 button -Version 2.0.0 -Deploy
```

### Manual Upload (without script)
```powershell
# If you just want to upload a pre-built binary
curl -X POST http://localhost:8000/api/v1/firmware/upload `
  -F "file=@build/button/button_1.0.0.bin" `
  -F "node_type=button" `
  -F "version=1.0.0" `
  -F "hardware=ESP32" `
  -F "is_stable=true"
```

## Troubleshooting

### "arduino-cli not found"
Install Arduino CLI:
```powershell
winget install ArduinoSA.ArduinoCLI
# Restart terminal
```

### "Board not found"
Install ESP32 core:
```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

### "Library not found"
Install missing libraries:
```bash
arduino-cli lib install "painlessMesh"
```

### Build succeeds but upload fails
- Check server is running: `curl http://localhost:8000/health`
- Check `$env:MESH_SERVER_URL` is set correctly
- Verify network connectivity

### OneDrive path errors
The scripts handle OneDrive sync issues automatically. If you see warnings about `partitions.csv`, they can be ignored as long as the binary is produced.

## Output Files

After a successful build, these files are created:

| File | Description |
|------|-------------|
| `<node>_<version>.bin` | OTA-ready firmware binary |
| `*.ino.bin` | Raw compiled binary |
| `*.ino.elf` | Debug symbols |
| `*.ino.map` | Memory map |
| `*.ino.bootloader.bin` | Bootloader (for full flash) |
| `*.ino.partitions.bin` | Partition table |

Only the `<node>_<version>.bin` file is needed for OTA updates.

# Mesh Gateway Node

A dedicated gateway that bridges the mesh network to the telemetry server. This node connects to WiFi and receives telemetry from all other mesh nodes, then pushes it to the server via HTTP.

## Setup

### 1. Create credentials file

Copy the example credentials file and fill in your values:

```bash
cp ../credentials.h.example ../credentials.h
```

Then edit `firmware/credentials.h`:

```cpp
#define WIFI_SSID     "YourWiFiNetwork"
#define WIFI_PASSWORD "YourWiFiPassword"
#define TELEMETRY_URL "http://192.168.1.100:8000"
#define TELEMETRY_KEY ""  // Optional API key
```

**Note:** `credentials.h` is gitignored and will not be committed to the repository.

### 2. Flash the gateway

1. Open `mesh_gateway.ino` in Arduino IDE
2. Select "ESP32 Dev Module" as the board
3. Upload to your ESP32

### 3. Verify connection

Open Serial Monitor (115200 baud). You should see:

```
========================================
       MESH GATEWAY NODE
========================================

Waiting for WiFi connection...

========================================
[GATEWAY] WiFi Connected!
[GATEWAY] IP: 192.168.1.50
[GATEWAY] Server: http://192.168.1.100:8000
[GATEWAY] Ready to receive telemetry from mesh
========================================
```

## How It Works

- The gateway connects to your WiFi network
- Other mesh nodes do NOT need WiFi credentials
- Sensor nodes send telemetry via mesh broadcast (MSG_TELEMETRY)
- Gateway receives these messages and pushes to the server via HTTP POST
- Gateway also pushes its own telemetry every 30 seconds

## Serial Commands

| Command | Description |
|---------|-------------|
| `status` | Show node status |
| `peers` | List connected mesh peers |
| `state` | Show shared state |
| `telem` | Show telemetry/gateway status |
| `push` | Manual telemetry push |
| `reboot` | Restart node |

## Architecture

```
[Sensor Node] --mesh--> [Gateway] --HTTP--> [Telemetry Server]
[Sensor Node] --mesh--> [Gateway] --HTTP--> [Telemetry Server]
[Sensor Node] --mesh--> [Gateway] --HTTP--> [Telemetry Server]
```

Only the gateway needs internet access. All other nodes communicate through the mesh network.

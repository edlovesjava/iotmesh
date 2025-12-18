/*
 * ESP32 Mesh Swarm - Telemetry Example
 *
 * Demonstrates pushing telemetry to server while maintaining mesh network.
 *
 * Features:
 *   - Connects to WiFi for telemetry server access
 *   - Maintains mesh network alongside WiFi
 *   - Pushes telemetry every 30 seconds (configurable)
 *   - Pushes immediately on state changes
 *   - Includes all shared state in telemetry payload
 *
 * Hardware:
 *   - ESP32 Dev Module
 *   - OLED display (optional, SSD1306 at 0x3C)
 *
 * Serial Commands:
 *   - status: Show node status
 *   - peers: List connected peers
 *   - state: Show shared state
 *   - set <k> <v>: Set state value
 *   - telem: Show telemetry status
 *   - push: Manual telemetry push
 *   - reboot: Restart node
 */

#include <MeshSwarm.h>

// ============== CONFIGURATION ==============
// WiFi credentials for telemetry server access
#define WIFI_SSID     "YourWiFi"
#define WIFI_PASSWORD "YourPassword"

// Telemetry server (Phase 1 server address)
#define TELEMETRY_URL "http://192.168.1.100:8000"
#define TELEMETRY_KEY ""  // Optional API key

// Telemetry interval (milliseconds)
#define TELEMETRY_PUSH_INTERVAL 30000  // 30 seconds

// ============== GLOBALS ==============
MeshSwarm swarm;

void setup() {
  // Initialize mesh swarm
  swarm.begin();

  // Connect to WiFi for telemetry (runs alongside mesh)
  swarm.connectToWiFi(WIFI_SSID, WIFI_PASSWORD);

  // Configure telemetry
  swarm.setTelemetryServer(TELEMETRY_URL, TELEMETRY_KEY);
  swarm.setTelemetryInterval(TELEMETRY_PUSH_INTERVAL);
  swarm.enableTelemetry(true);

  Serial.println();
  Serial.println("[SETUP] Telemetry node ready");
  Serial.println("[SETUP] Waiting for WiFi connection...");
  Serial.println();
}

void loop() {
  swarm.update();

  // Optional: Show WiFi status on first connection
  static bool wifiReported = false;
  if (!wifiReported && swarm.isWiFiConnected()) {
    Serial.println("[WIFI] Connected!");
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
    wifiReported = true;
  }
}

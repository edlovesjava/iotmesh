/**
 * Mesh Gateway Node
 *
 * A dedicated gateway that bridges the mesh network to the telemetry server.
 * This node connects to WiFi and receives telemetry from all other mesh nodes,
 * then pushes it to the server via HTTP.
 *
 * Other nodes in the mesh do NOT need WiFi credentials - they send telemetry
 * through the mesh to this gateway.
 *
 * Features:
 *   - Connects to WiFi for server access
 *   - Maintains mesh network with other nodes
 *   - Receives MSG_TELEMETRY from other nodes
 *   - Pushes telemetry to server for all nodes
 *   - Also pushes its own telemetry
 *   - OTA firmware distribution to mesh nodes
 *
 * Hardware:
 *   - ESP32 Dev Module
 *   - OLED display (optional, SSD1306 at 0x3C)
 *
 * Serial Commands:
 *   - status: Show node status
 *   - peers: List connected peers
 *   - state: Show shared state
 *   - telem: Show telemetry/gateway status
 *   - push: Manual telemetry push
 *   - reboot: Restart node
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// Include credentials (gitignored)
#if __has_include("credentials.h")
#include "credentials.h"
#else
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
#define TELEMETRY_URL "http://192.168.1.100:8000"
#define TELEMETRY_KEY ""
#endif

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Gateway"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "gateway"
#endif

// Gateway's own telemetry interval (milliseconds)
#ifndef TELEMETRY_PUSH_INTERVAL
#define TELEMETRY_PUSH_INTERVAL 30000  // 30 seconds
#endif

// ============== GLOBALS ==============
MeshSwarm swarm;

void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  // Initialize mesh swarm with a name
  swarm.begin(NODE_NAME);

  // Connect to WiFi (required for gateway)
  swarm.connectToWiFi(WIFI_SSID, WIFI_PASSWORD);

  // Configure as gateway
  swarm.setGatewayMode(true);
  swarm.setTelemetryServer(TELEMETRY_URL, TELEMETRY_KEY);
  swarm.setTelemetryInterval(TELEMETRY_PUSH_INTERVAL);
  swarm.enableTelemetry(true);

  // Enable OTA distribution (gateway polls server and distributes to mesh)
  swarm.enableOTADistribution(true);

  // Start HTTP server for Remote Command Protocol API
  swarm.startHTTPServer(80);

  Serial.println();
  Serial.println("========================================");
  Serial.println("       MESH GATEWAY NODE");
  Serial.println("========================================");
  Serial.println();
  Serial.println("This node bridges mesh -> server");
  Serial.println("Other nodes send telemetry via mesh");
  Serial.println();
  Serial.println("Waiting for WiFi connection...");
  Serial.println();
}

void loop() {
  swarm.update();

  // Check for OTA updates (polls every OTA_POLL_INTERVAL)
  swarm.checkForOTAUpdates();

  // Show WiFi connection status once
  static bool wifiReported = false;
  if (!wifiReported && swarm.isWiFiConnected()) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("[GATEWAY] WiFi Connected!");
    Serial.printf("[GATEWAY] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[GATEWAY] Server: %s\n", TELEMETRY_URL);
    Serial.println("[GATEWAY] Ready to receive telemetry from mesh");
    Serial.println("[GATEWAY] OTA distribution enabled");
    Serial.println("[GATEWAY] HTTP API enabled on port 80");
    Serial.println("[GATEWAY]   GET  /api/nodes  - List nodes");
    Serial.println("[GATEWAY]   GET  /api/state  - Get state");
    Serial.println("[GATEWAY]   POST /api/command - Send command");
    Serial.println("========================================");
    Serial.println();

    // Show IP on OLED display
    swarm.setStatusLine("IP:" + WiFi.localIP().toString());

    wifiReported = true;
  }
}

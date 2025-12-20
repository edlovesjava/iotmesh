/**
 * GatewayNode - Bridge between mesh and external server
 *
 * Demonstrates:
 * - WiFi station mode alongside mesh
 * - Telemetry push to server
 * - OTA firmware distribution
 * - Gateway mode operation
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 *
 * Configuration:
 * - Set your WiFi credentials below
 * - Set your telemetry server URL
 */

#include <MeshSwarm.h>
#include <esp_ota_ops.h>

MeshSwarm swarm;

// WiFi credentials - UPDATE THESE
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Telemetry server - UPDATE THIS
const char* serverUrl = "http://192.168.1.100:8000";

void setup() {
  Serial.begin(115200);

  // Mark OTA partition valid (enables auto-rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin("Gateway");

  // Connect to WiFi (runs alongside mesh)
  swarm.connectToWiFi(ssid, password);

  // Enable gateway mode
  swarm.setGatewayMode(true);

  // Configure telemetry
  swarm.setTelemetryServer(serverUrl);
  swarm.enableTelemetry(true);

  // Enable OTA distribution to mesh nodes
  swarm.enableOTADistribution(true);

  Serial.println("GatewayNode started");
  Serial.printf("WiFi: %s\n", ssid);
  Serial.printf("Server: %s\n", serverUrl);
}

void loop() {
  swarm.update();

  // Check for OTA updates periodically
  swarm.checkForOTAUpdates();
}

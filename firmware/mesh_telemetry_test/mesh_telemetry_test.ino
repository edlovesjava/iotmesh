/*
 * Telemetry Test Node
 *
 * Test sketch for verifying telemetry server connectivity.
 * Update WIFI_SSID, WIFI_PASSWORD, and TELEMETRY_URL for your network.
 */

#include <MeshSwarm.h>

// ============== UPDATE THESE FOR YOUR NETWORK ==============
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Telemetry server running on your local machine
#define TELEMETRY_URL "http://10.0.0.71:8000"

// Push every 10 seconds for testing (faster feedback)
#define TELEMETRY_PUSH_INTERVAL 10000

// ============== GLOBALS ==============
MeshSwarm swarm;
unsigned long lastTestStateChange = 0;
int testCounter = 0;

void setup() {
  swarm.begin("TestNode");

  // Connect to WiFi for telemetry
  swarm.connectToWiFi(WIFI_SSID, WIFI_PASSWORD);

  // Configure telemetry
  swarm.setTelemetryServer(TELEMETRY_URL);
  swarm.setTelemetryInterval(TELEMETRY_PUSH_INTERVAL);
  swarm.enableTelemetry(true);

  // Set initial test state
  swarm.setState("test", "ready");

  Serial.println();
  Serial.println("========================================");
  Serial.println("       TELEMETRY TEST NODE");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Waiting for WiFi connection...");
  Serial.println("Once connected, telemetry will push every 10s");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  telem  - Show telemetry status");
  Serial.println("  push   - Force telemetry push");
  Serial.println("  test   - Change test state (triggers push)");
  Serial.println();
}

void loop() {
  swarm.update();

  // Show WiFi connection status once
  static bool wifiReported = false;
  if (!wifiReported && swarm.isWiFiConnected()) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("[WIFI] Connected!");
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Server: %s\n", TELEMETRY_URL);
    Serial.println("========================================");
    Serial.println();
    wifiReported = true;
  }

  // Handle custom "test" command
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "test") {
      testCounter++;
      String value = "count_" + String(testCounter);
      swarm.setState("test", value);
      Serial.printf("[TEST] State changed to: %s\n", value.c_str());
    }
  }
}

/*
 * ESP32 Mesh Swarm - Watcher Node
 *
 * Observer node with no hardware I/O.
 * Monitors and displays state changes across the network.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 */

#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== GLOBALS ==============
MeshSwarm swarm;

// ============== SETUP ==============
void setup() {
  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin("Watcher");
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive("watcher");  // Enable OTA updates for this node type

  Serial.println("[MODE] Watcher - monitoring state changes");

  // Watch all state changes
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    Serial.printf("[WATCH] %s: %s -> %s\n", key.c_str(), oldValue.c_str(), value.c_str());
  });

  // Default display shows all states - no custom handler needed
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

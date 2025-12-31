/**
 * Watcher Node
 *
 * Observer node with no hardware I/O.
 * Monitors and displays state changes across the network.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Watcher"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "watcher"
#endif

// ============== DISPLAY SLEEP CONFIG ==============
#define WATCHER_DISPLAY_SLEEP_MS  30000  // Sleep after 30 seconds of inactivity
#define BOOT_BUTTON_PIN  0

// ============== GLOBALS ==============
MeshSwarm swarm;

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

  // Enable display sleep with boot button wake
  swarm.enableDisplaySleep(WATCHER_DISPLAY_SLEEP_MS);
  swarm.addDisplayWakeButton(BOOT_BUTTON_PIN);

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

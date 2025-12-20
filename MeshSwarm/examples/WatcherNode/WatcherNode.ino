/**
 * WatcherNode - Observer that monitors all state changes
 *
 * Demonstrates:
 * - Wildcard state watching
 * - Logging all mesh activity
 * - Read-only observer pattern
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 */

#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  Serial.begin(115200);

  swarm.begin("Watcher");

  // Watch ALL state changes with wildcard
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    Serial.printf("[STATE] %s: %s -> %s\n", key.c_str(), oldValue.c_str(), value.c_str());
  });

  Serial.println("WatcherNode started - monitoring all state changes");
}

void loop() {
  swarm.update();
}

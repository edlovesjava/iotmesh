/**
 * BasicNode - Minimal MeshSwarm example
 *
 * Demonstrates:
 * - Mesh network auto-connection
 * - Coordinator election
 * - OLED display
 * - Serial commands
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 */

#include <MeshSwarm.h>

MeshSwarm swarm;

void setup() {
  Serial.begin(115200);

  // Initialize with default network settings
  // Uses: MESH_PREFIX="meshswarm", MESH_PASSWORD="meshpassword", MESH_PORT=5555
  swarm.begin();

  // Or use custom network settings:
  // swarm.begin("mynetwork", "password123", 5555);

  Serial.println("BasicNode started");
}

void loop() {
  // Must be called frequently to maintain mesh
  swarm.update();
}

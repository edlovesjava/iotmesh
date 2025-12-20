/**
 * LedNode - LED output controlled by mesh state
 *
 * Demonstrates:
 * - Watching state changes from other nodes
 * - Controlling hardware based on shared state
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 * - LED on GPIO2 (built-in LED on most ESP32 boards)
 */

#include <MeshSwarm.h>

MeshSwarm swarm;

#define LED_PIN 2

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  swarm.begin("LED");

  // Watch for button state changes from other nodes
  swarm.watchState("button", [](const String& key, const String& value, const String& oldValue) {
    bool ledOn = (value == "1");
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    Serial.printf("LED: %s (from button state)\n", ledOn ? "ON" : "OFF");
  });

  // Also watch for direct LED commands
  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    bool ledOn = (value == "1");
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    Serial.printf("LED: %s (from led state)\n", ledOn ? "ON" : "OFF");
  });

  Serial.println("LedNode started");
}

void loop() {
  swarm.update();
}

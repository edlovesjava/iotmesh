/*
 * ESP32 Mesh Swarm - LED Output Node
 *
 * Watches shared LED state and controls LED accordingly.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - LED on GPIO2 (onboard LED)
 */

#include <MeshSwarm.h>

// ============== LED CONFIGURATION ==============
#define LED_PIN         2         // Onboard LED on most ESP32 boards

// ============== GLOBALS ==============
MeshSwarm swarm;

// ============== SETUP ==============
void setup() {
  swarm.begin();

  // LED setup
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.printf("[HW] LED enabled on GPIO%d\n", LED_PIN);

  // Watch for LED state changes
  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    bool ledOn = (value == "1" || value == "on" || value == "true");
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    Serial.printf("[LED] State changed: %s -> %s (LED %s)\n",
                  oldValue.c_str(), value.c_str(), ledOn ? "ON" : "OFF");
  });

  // Custom display
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    String ledState = swarm.getState("led", "0");
    bool ledOn = (ledState == "1");

    display.println("Mode: LED OUTPUT");
    display.println("---------------------");
    display.printf("led=%s\n", ledState.c_str());
    display.println();
    display.printf("LED is: %s\n", ledOn ? "ON" : "OFF");
  });
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

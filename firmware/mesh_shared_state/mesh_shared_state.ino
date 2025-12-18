/*
 * ESP32 Mesh Swarm - Full Featured Node
 *
 * All hardware features enabled (button + LED).
 * Use as reference or for nodes needing both input and output.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - Button on GPIO0, LED on GPIO2
 */

#include <MeshSwarm.h>

// ============== HARDWARE CONFIGURATION ==============
#define BUTTON_PIN      0         // Boot button
#define LED_PIN         2         // Onboard LED
#define DEBOUNCE_MS     50

// ============== GLOBALS ==============
MeshSwarm swarm;

bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;

// ============== BUTTON HANDLING ==============
void handleButton() {
  bool buttonState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (buttonState != lastButtonState && (now - lastButtonChange > DEBOUNCE_MS)) {
    lastButtonChange = now;
    lastButtonState = buttonState;

    if (buttonState == LOW) {
      String currentLed = swarm.getState("led", "0");
      String newLed = (currentLed == "1") ? "0" : "1";
      swarm.setState("led", newLed);
      Serial.printf("[BUTTON] Pressed! LED state: %s -> %s\n", currentLed.c_str(), newLed.c_str());
    }
  }
}

// ============== SETUP ==============
void setup() {
  swarm.begin();

  // Button setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("[HW] Button enabled on GPIO%d\n", BUTTON_PIN);

  // LED setup
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.printf("[HW] LED enabled on GPIO%d\n", LED_PIN);

  // Register button polling
  swarm.onLoop(handleButton);

  // Watch for LED state changes
  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    bool ledOn = (value == "1" || value == "on" || value == "true");
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    Serial.printf("[LED] State changed: %s -> %s (LED %s)\n",
                  oldValue.c_str(), value.c_str(), ledOn ? "ON" : "OFF");
  });

  // Watch temperature example
  swarm.watchState("temp", [](const String& key, const String& value, const String& oldValue) {
    Serial.printf("[WATCH] Temperature: %s -> %s\n", oldValue.c_str(), value.c_str());
  });
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

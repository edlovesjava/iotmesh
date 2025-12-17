/*
 * ESP32 Mesh Swarm - Button Input Node
 *
 * Press button to toggle shared LED state across mesh network.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - Button on GPIO0 (boot button)
 */

#include <MeshSwarm.h>

// ============== BUTTON CONFIGURATION ==============
#define BUTTON_PIN      0         // Boot button on most ESP32 boards
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

    // Button pressed (LOW because INPUT_PULLUP)
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

  // Register button polling in loop
  swarm.onLoop(handleButton);

  // Custom display
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    display.println("Mode: BUTTON");
    display.println("------------------------");
    display.printf("led=%s\n", swarm.getState("led", "0").c_str());
    display.println();
    display.println("Press button to toggle");
  });
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

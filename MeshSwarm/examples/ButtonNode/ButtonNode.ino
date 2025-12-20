/**
 * ButtonNode - Button input with shared state
 *
 * Demonstrates:
 * - Reading button input
 * - Publishing state to mesh
 * - Using onLoop callback
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 * - Momentary button on GPIO0 (pulled high, active low)
 */

#include <MeshSwarm.h>

MeshSwarm swarm;

#define BUTTON_PIN 0

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  swarm.begin("Button");

  // Register custom loop logic
  swarm.onLoop([]() {
    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
      static bool buttonState = HIGH;
      if (reading != buttonState) {
        buttonState = reading;

        // Publish button state to mesh
        swarm.setState("button", buttonState == LOW ? "1" : "0");
        Serial.printf("Button: %s\n", buttonState == LOW ? "pressed" : "released");
      }
    }

    lastButtonState = reading;
  });

  Serial.println("ButtonNode started");
}

void loop() {
  swarm.update();
}

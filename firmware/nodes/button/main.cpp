/**
 * Button Input Node
 *
 * Press button to toggle shared LED state across mesh network.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - Boot button on GPIO0
 *   - External button on GPIO5 (optional)
 *     - Wire: GPIO5 -> Button -> GND
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Button"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "button"
#endif

// ============== BUTTON CONFIGURATION ==============
#ifndef BOOT_BUTTON_PIN
#define BOOT_BUTTON_PIN    0      // Boot button on most ESP32 boards
#endif

#ifndef EXT_BUTTON_PIN
#define EXT_BUTTON_PIN     5      // External button
#endif

#ifndef DEBOUNCE_MS
#define DEBOUNCE_MS        50
#endif

// ============== DISPLAY SLEEP CONFIG ==============
// Note: Use a local name to avoid conflict with library default
#define BUTTON_DISPLAY_SLEEP_MS  15000  // Sleep after 15 seconds of inactivity

// ============== GLOBALS ==============
MeshSwarm swarm;

bool lastBootButtonState = HIGH;
bool lastExtButtonState = HIGH;
unsigned long lastBootButtonChange = 0;
unsigned long lastExtButtonChange = 0;
unsigned long buttonPressCount = 0;

// ============== BUTTON HANDLING ==============
void toggleLed(const char* source) {
  // Wake display on any button press (handled by DisplayPowerManager)
  swarm.getPowerManager().resetActivity();

  String currentLed = swarm.getState("led", "0");
  String newLed = (currentLed == "1") ? "0" : "1";
  swarm.setState("led", newLed);
  buttonPressCount++;
  Serial.printf("[BUTTON] %s pressed! LED: %s -> %s (count: %lu)\n",
                source, currentLed.c_str(), newLed.c_str(), buttonPressCount);
}

void handleButtons() {
  unsigned long now = millis();

  // Check boot button (GPIO0)
  bool bootState = digitalRead(BOOT_BUTTON_PIN);
  if (bootState != lastBootButtonState && (now - lastBootButtonChange > DEBOUNCE_MS)) {
    lastBootButtonChange = now;
    lastBootButtonState = bootState;
    if (bootState == LOW) {
      toggleLed("Boot");
    }
  }

  // Check external button
  bool extState = digitalRead(EXT_BUTTON_PIN);
  if (extState != lastExtButtonState && (now - lastExtButtonChange > DEBOUNCE_MS)) {
    lastExtButtonChange = now;
    lastExtButtonState = extState;
    if (extState == LOW) {
      toggleLed("External");
    }
  }
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

  // Button setup - both use internal pull-up
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(EXT_BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("[HW] Boot button on GPIO%d\n", BOOT_BUTTON_PIN);
  Serial.printf("[HW] External button on GPIO%d\n", EXT_BUTTON_PIN);

  // Enable display sleep with timeout
  swarm.enableDisplaySleep(BUTTON_DISPLAY_SLEEP_MS);

  // Add both buttons as wake sources
  swarm.addDisplayWakeButton(BOOT_BUTTON_PIN);
  swarm.addDisplayWakeButton(EXT_BUTTON_PIN);

  // Register button polling in loop
  swarm.onLoop(handleButtons);

  // Custom display
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    display.println("Mode: BUTTON");
    display.println("---------------------");
    display.printf("led=%s\n", swarm.getState("led", "0").c_str());
    display.printf("presses=%lu\n", buttonPressCount);
    display.println("Press to toggle LED");
  });
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

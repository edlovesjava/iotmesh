/*
 * ESP32 Mesh Swarm - Button Input Node
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

#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== BUTTON CONFIGURATION ==============
#define BOOT_BUTTON_PIN    0      // Boot button on most ESP32 boards
#define EXT_BUTTON_PIN     5      // External button
#define DEBOUNCE_MS        50

// ============== GLOBALS ==============
MeshSwarm swarm;

bool lastBootButtonState = HIGH;
bool lastExtButtonState = HIGH;
unsigned long lastBootButtonChange = 0;
unsigned long lastExtButtonChange = 0;
unsigned long buttonPressCount = 0;

// ============== BUTTON HANDLING ==============
void toggleLed(const char* source) {
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

  // Check external button (GPIO5)
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
  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin("Button");
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive("button");  // Enable OTA updates for this node type

  // Button setup - both use internal pull-up
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(EXT_BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("[HW] Boot button on GPIO%d\n", BOOT_BUTTON_PIN);
  Serial.printf("[HW] External button on GPIO%d\n", EXT_BUTTON_PIN);

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

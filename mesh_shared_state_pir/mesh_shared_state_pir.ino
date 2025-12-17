/*
 * ESP32 Mesh Swarm - PIR Motion Sensor Node
 *
 * Reads PIR sensor directly on GPIO and publishes motion state
 * to the mesh network.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - PIR sensor (HC-SR501 or similar)
 *     - VCC -> 3.3V or 5V
 *     - GND -> GND
 *     - OUT -> GPIO4 (D4)
 */

#include <MeshSwarm.h>

// ============== PIR CONFIGURATION ==============
#define PIR_PIN         4         // GPIO4 for PIR sensor output
#define DEBOUNCE_MS     50        // Debounce time
#define HOLD_TIME_MS    3000      // Keep motion active after last trigger
#define WARMUP_SEC      30        // PIR warmup time
#define MOTION_ZONE     "zone1"   // Zone identifier for this sensor

// ============== GLOBALS ==============
MeshSwarm swarm;

// PIR state
bool pirReady = false;
bool motionActive = false;
bool lastPirReading = LOW;
unsigned long lastTriggerTime = 0;
unsigned long lastMotionTime = 0;
unsigned long motionCount = 0;
unsigned long bootTime = 0;

// ============== PIR HANDLING ==============
void pollPir() {
  if (!pirReady) return;

  unsigned long now = millis();
  bool pirReading = digitalRead(PIR_PIN);

  // Debounce - detect rising edge
  if (pirReading == HIGH && lastPirReading == LOW) {
    if (now - lastTriggerTime > DEBOUNCE_MS) {
      lastTriggerTime = now;

      if (!motionActive) {
        // New motion event
        motionActive = true;
        motionCount++;
        lastMotionTime = now;

        swarm.setState("motion", "1");
        String zoneKey = String("motion_") + MOTION_ZONE;
        swarm.setState(zoneKey, "1");

        Serial.printf("[PIR] Motion DETECTED (#%lu)\n", motionCount);
      } else {
        // Extend hold time
        lastMotionTime = now;
      }
    }
  }

  lastPirReading = pirReading;

  // Hold time expired - clear motion
  if (motionActive && (now - lastMotionTime >= HOLD_TIME_MS)) {
    motionActive = false;

    swarm.setState("motion", "0");
    String zoneKey = String("motion_") + MOTION_ZONE;
    swarm.setState(zoneKey, "0");

    Serial.println("[PIR] Motion cleared");
  }
}

// ============== SETUP ==============
void setup() {
  swarm.begin();
  bootTime = millis();

  // PIR pin setup
  pinMode(PIR_PIN, INPUT);
  Serial.printf("[PIR] Sensor on GPIO%d\n", PIR_PIN);
  Serial.printf("[PIR] Zone: %s\n", MOTION_ZONE);

  // PIR warmup
  Serial.printf("[PIR] Warming up (%d seconds)...\n", WARMUP_SEC);
  for (int i = WARMUP_SEC; i > 0; i--) {
    Serial.printf("%d ", i);
    if (i % 10 == 0 && i != WARMUP_SEC) Serial.println();
    delay(1000);
  }
  Serial.println();
  pirReady = true;
  Serial.println("[PIR] Ready!");
  Serial.println();

  // Register PIR polling in loop
  swarm.onLoop(pollPir);

  // Register custom serial command
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "pir") {
      unsigned long now = millis();
      unsigned long secSinceMotion = motionActive ? 0 : (now - lastMotionTime) / 1000;

      Serial.println("\n--- PIR SENSOR ---");
      Serial.printf("GPIO: %d\n", PIR_PIN);
      Serial.printf("Ready: %s\n", pirReady ? "YES" : "NO");
      Serial.printf("Motion: %s\n", motionActive ? "ACTIVE" : "none");
      Serial.printf("Event count: %lu\n", motionCount);
      Serial.printf("Last motion: %lu sec ago\n", secSinceMotion);
      Serial.printf("Zone: %s\n", MOTION_ZONE);
      Serial.printf("Raw pin: %s\n", digitalRead(PIR_PIN) ? "HIGH" : "LOW");
      Serial.println();
      return true;
    }
    return false;
  });

  // Register custom display section
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    unsigned long now = millis();
    unsigned long secSinceMotion = motionActive ? 0 : (now - lastMotionTime) / 1000;
    if (secSinceMotion > 999) secSinceMotion = 999;

    // PIR Status line
    display.print("PIR:");
    if (!pirReady) {
      display.println("WARMING UP");
    } else if (motionActive) {
      display.println("MOTION!");
    } else {
      display.printf("idle %lus ago\n", secSinceMotion);
    }

    display.println("------------------------");

    // Show state
    display.printf("motion=%s\n", motionActive ? "1" : "0");
    display.printf("zone=%s\n", MOTION_ZONE);
    display.printf("events=%lu\n", motionCount);
  });

  // Add PIR status to heartbeat
  swarm.setHeartbeatData("pir", 1);
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

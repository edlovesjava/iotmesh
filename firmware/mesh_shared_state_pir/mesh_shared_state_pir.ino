/*
 * ESP32 Mesh Swarm - PIR Motion Sensor Node
 *
 * Reads PIR sensor directly on GPIO and publishes motion state
 * to the mesh network.
 *
 * Supported sensors:
 *   - HC-SR501: Adjustable sensitivity/delay, 5V, 30s warmup
 *   - AM312: Mini sensor, 3.3V native, 5s warmup, low power
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - PIR sensor:
 *     - VCC -> 3.3V (AM312) or 5V (HC-SR501)
 *     - GND -> GND
 *     - OUT -> GPIO4
 */

#include <MeshSwarm.h>

// ============== PIR SENSOR TYPE ==============
// Uncomment ONE of these to select your sensor:
// #define PIR_SENSOR_AM312      // Mini 3.3V sensor, low power
#define PIR_SENSOR_HC_SR501   // Standard adjustable sensor

// ============== PIR CONFIGURATION ==============
#define PIR_PIN         4         // GPIO4 for PIR sensor output
#define MOTION_ZONE     "zone1"   // Zone identifier for this sensor

// Sensor-specific defaults
#ifdef PIR_SENSOR_AM312
  #define PIR_MODEL       "AM312"
  #define DEBOUNCE_MS     30        // AM312 has cleaner signal
  #define HOLD_TIME_MS    2500      // AM312 has ~2s fixed output
  #define WARMUP_SEC      5         // AM312 stabilizes quickly
#else
  #define PIR_MODEL       "HC-SR501"
  #define DEBOUNCE_MS     50        // HC-SR501 may need more debounce
  #define HOLD_TIME_MS    3000      // Extend beyond sensor's delay
  #define WARMUP_SEC      30        // HC-SR501 needs longer warmup
#endif

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
  Serial.printf("[PIR] Model: %s\n", PIR_MODEL);
  Serial.printf("[PIR] GPIO: %d\n", PIR_PIN);
  Serial.printf("[PIR] Zone: %s\n", MOTION_ZONE);
  Serial.printf("[PIR] Hold time: %dms\n", HOLD_TIME_MS);

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
      Serial.printf("Model: %s\n", PIR_MODEL);
      Serial.printf("GPIO: %d\n", PIR_PIN);
      Serial.printf("Ready: %s\n", pirReady ? "YES" : "NO");
      Serial.printf("Motion: %s\n", motionActive ? "ACTIVE" : "none");
      Serial.printf("Event count: %lu\n", motionCount);
      Serial.printf("Last motion: %lu sec ago\n", secSinceMotion);
      Serial.printf("Hold time: %dms\n", HOLD_TIME_MS);
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

    // PIR Status line with model
    display.printf("%s:", PIR_MODEL);
    if (!pirReady) {
      display.println("WARMING UP");
    } else if (motionActive) {
      display.println("MOTION!");
    } else {
      display.printf("idle %lus\n", secSinceMotion);
    }

    display.println("---------------------");

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

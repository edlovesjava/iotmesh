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
  #define WARMUP_SEC      5         // AM312 stabilizes quickly
#else
  #define PIR_MODEL       "HC-SR501"
  #define WARMUP_SEC      30        // HC-SR501 needs longer warmup
#endif

// Time window for motion detection
#define MOTION_WINDOW_MS  10000     // 10 second evaluation window

// ============== GLOBALS ==============
MeshSwarm swarm;

// PIR state - time window approach
bool pirReady = false;
bool motionDetectedInWindow = false;  // Any motion detected this window?
bool lastReportedMotion = false;      // Last state sent to mesh
unsigned long lastWindowCheck = 0;    // Last window evaluation time
unsigned long motionCount = 0;
unsigned long bootTime = 0;

// ============== PIR HANDLING ==============
void pollPir() {
  if (!pirReady) return;

  unsigned long now = millis();

  // Track any motion in current window
  if (digitalRead(PIR_PIN) == HIGH) {
    motionDetectedInWindow = true;
  }

  // Evaluate window every MOTION_WINDOW_MS
  if (now - lastWindowCheck >= MOTION_WINDOW_MS) {
    lastWindowCheck = now;

    if (motionDetectedInWindow && !lastReportedMotion) {
      // Transition: no motion → motion
      lastReportedMotion = true;
      motionCount++;

      String zoneKey = String("motion_") + MOTION_ZONE;
      swarm.setStates({{"motion", "1"}, {zoneKey, "1"}});

      Serial.printf("[PIR] Motion DETECTED (#%lu)\n", motionCount);
    }
    else if (!motionDetectedInWindow && lastReportedMotion) {
      // Transition: motion → no motion
      lastReportedMotion = false;

      String zoneKey = String("motion_") + MOTION_ZONE;
      swarm.setStates({{"motion", "0"}, {zoneKey, "0"}});

      Serial.println("[PIR] Motion cleared");
    }

    // Reset window
    motionDetectedInWindow = false;
  }
}

// ============== SETUP ==============
void setup() {
  swarm.begin("PIR");
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive("pir");  // Enable OTA updates for this node type
  bootTime = millis();

  // PIR pin setup
  pinMode(PIR_PIN, INPUT);
  Serial.printf("[PIR] Model: %s\n", PIR_MODEL);
  Serial.printf("[PIR] GPIO: %d\n", PIR_PIN);
  Serial.printf("[PIR] Zone: %s\n", MOTION_ZONE);
  Serial.printf("[PIR] Window: %dms\n", MOTION_WINDOW_MS);

  // PIR warmup
  Serial.printf("[PIR] Warming up (%d seconds)...\n", WARMUP_SEC);
  for (int i = WARMUP_SEC; i > 0; i--) {
    Serial.printf("%d ", i);
    if (i % 10 == 0 && i != WARMUP_SEC) Serial.println();
    delay(1000);
  }
  Serial.println();
  pirReady = true;
  lastWindowCheck = millis();  // Start first window now
  Serial.println("[PIR] Ready!");
  Serial.println();

  // Send initial state (no motion)
  String zoneKey = String("motion_") + MOTION_ZONE;
  swarm.setStates({{"motion", "0"}, {zoneKey, "0"}});
  Serial.println("[PIR] Initial state: no motion");

  // Register PIR polling in loop
  swarm.onLoop(pollPir);

  // Register custom serial command
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "pir") {
      unsigned long now = millis();
      unsigned long secSinceCheck = (now - lastWindowCheck) / 1000;

      Serial.println("\n--- PIR SENSOR ---");
      Serial.printf("Model: %s\n", PIR_MODEL);
      Serial.printf("GPIO: %d\n", PIR_PIN);
      Serial.printf("Ready: %s\n", pirReady ? "YES" : "NO");
      Serial.printf("Motion state: %s\n", lastReportedMotion ? "ACTIVE" : "none");
      Serial.printf("Window activity: %s\n", motionDetectedInWindow ? "YES" : "no");
      Serial.printf("Event count: %lu\n", motionCount);
      Serial.printf("Last check: %lu sec ago\n", secSinceCheck);
      Serial.printf("Window: %dms\n", MOTION_WINDOW_MS);
      Serial.printf("Zone: %s\n", MOTION_ZONE);
      Serial.printf("Raw pin: %s\n", digitalRead(PIR_PIN) ? "HIGH" : "LOW");
   //   Serial.printf("Uptime: %lu sec\n", (now - bootTime) / 1000);
      Serial.println();
      return true;
    }
    return false;
  });

  // Register custom display section
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    // PIR Status line with model
    display.printf("%s:", PIR_MODEL);
    if (!pirReady) {
      display.println("WARMING UP");
    } else if (lastReportedMotion) {
      display.println("MOTION!");
    } else {
      display.println("idle");
    }

    display.println("---------------------");

    // Show state
    display.printf("motion=%s\n", lastReportedMotion ? "1" : "0");
    display.printf("zone=%s\n", MOTION_ZONE);
    display.printf("events=%lu\n", motionCount);
    display.printf("window=%s\n", motionDetectedInWindow ? "active" : "-");
  });

  // Add PIR status to heartbeat
  swarm.setHeartbeatData("pir", 1);
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

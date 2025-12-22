/**
 * Light Sensor Node
 *
 * Reads ambient light level and publishes to the mesh network.
 *
 * Supports two sensor types:
 *   - LDR (photoresistor): Analog reading, cheap, easy wiring
 *   - BH1750: Digital I2C sensor, accurate lux readings
 *
 * Hardware (LDR):
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - LDR circuit:
 *     - LDR one leg -> 3.3V
 *     - LDR other leg -> GPIO34 (ADC) + 10k resistor to GND
 *
 * Hardware (BH1750):
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - BH1750 on same I2C bus (address 0x23 or 0x5C)
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Light"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "light"
#endif

// ============== SENSOR TYPE ==============
// Define ONE of these in platformio.ini or uncomment here:
// -DLIGHT_SENSOR_LDR    (Analog photoresistor)
// -DLIGHT_SENSOR_BH1750 (Digital I2C lux sensor)
#if !defined(LIGHT_SENSOR_LDR) && !defined(LIGHT_SENSOR_BH1750)
#define LIGHT_SENSOR_LDR        // Default to LDR
#endif

// ============== LDR CONFIGURATION ==============
#ifdef LIGHT_SENSOR_LDR

#ifndef LDR_PIN
#define LDR_PIN         34        // GPIO34 (ADC1_CH6) - ADC capable pin
#endif

// Set to 1 if your LDR module reads HIGH when dark (common for modules)
#ifndef LDR_INVERTED
#define LDR_INVERTED          1     // 1 = inverted (high=dark), 0 = normal (high=bright)
#endif

// Thresholds for light levels (adjust based on your LDR + resistor)
// These are for the RAW value after inversion is applied
#ifndef LDR_DARK_THRESHOLD
#define LDR_DARK_THRESHOLD    500   // Below this = dark
#endif

#ifndef LDR_BRIGHT_THRESHOLD
#define LDR_BRIGHT_THRESHOLD  3000  // Above this = bright
#endif

#define SENSOR_MODEL "LDR"

#endif // LIGHT_SENSOR_LDR

// ============== BH1750 CONFIGURATION ==============
#ifdef LIGHT_SENSOR_BH1750
#include <Wire.h>

#ifndef BH1750_ADDR
#define BH1750_ADDR     0x23      // Default address (ADDR pin LOW)
#endif

#define SENSOR_MODEL "BH1750"

// BH1750 commands
#define BH1750_POWER_ON     0x01
#define BH1750_RESET        0x07
#define BH1750_CONT_H_RES   0x10  // Continuous high-res mode (1 lux resolution)

#endif // LIGHT_SENSOR_BH1750

// ============== COMMON CONFIGURATION ==============
#ifndef READ_INTERVAL
#define READ_INTERVAL   2000      // Read every 2 seconds
#endif

#ifndef SENSOR_ZONE
#define SENSOR_ZONE     "zone1"   // Zone identifier for this sensor
#endif

// Change threshold to trigger mesh update
#ifndef LIGHT_CHANGE_THRESHOLD
#define LIGHT_CHANGE_THRESHOLD  5  // Percent change to trigger update
#endif

// ============== GLOBALS ==============
MeshSwarm swarm;

// Sensor state
int lightLevel = 0;           // 0-100 percentage or lux value
int lastReportedLevel = -1;   // Last value sent to mesh
String lightState = "unknown"; // "dark", "dim", "bright"
String lastReportedState = "";
bool sensorReady = false;
unsigned long lastReadTime = 0;
unsigned long readCount = 0;
unsigned long errorCount = 0;

// ============== BH1750 FUNCTIONS ==============
#ifdef LIGHT_SENSOR_BH1750

bool bh1750Init() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(BH1750_POWER_ON);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delay(10);

  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(BH1750_CONT_H_RES);
  return Wire.endTransmission() == 0;
}

int bh1750ReadLux() {
  Wire.requestFrom((uint8_t)BH1750_ADDR, (uint8_t)2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read() << 8;
    raw |= Wire.read();
    // Convert to lux (divide by 1.2 per datasheet)
    return raw / 1.2;
  }
  return -1;
}

#endif // LIGHT_SENSOR_BH1750

// ============== LIGHT READING ==============
void pollLight() {
  unsigned long now = millis();

  if (now - lastReadTime < READ_INTERVAL) return;
  lastReadTime = now;

  int newLevel = 0;
  String newState = "unknown";

#ifdef LIGHT_SENSOR_LDR
  // Read analog value (0-4095 on ESP32)
  int rawValue = analogRead(LDR_PIN);

  // Invert if needed (some modules read high when dark)
#if LDR_INVERTED
  rawValue = 4095 - rawValue;
#endif

  // Convert to percentage (0-100)
  newLevel = map(rawValue, 0, 4095, 0, 100);

  // Determine state based on thresholds
  if (rawValue < LDR_DARK_THRESHOLD) {
    newState = "dark";
  } else if (rawValue > LDR_BRIGHT_THRESHOLD) {
    newState = "bright";
  } else {
    newState = "dim";
  }

  sensorReady = true;
  readCount++;
#endif

#ifdef LIGHT_SENSOR_BH1750
  int lux = bh1750ReadLux();
  if (lux < 0) {
    errorCount++;
    Serial.printf("[LIGHT] Read error (#%lu)\n", errorCount);
    return;
  }

  newLevel = lux;

  // Determine state based on lux levels
  if (lux < 10) {
    newState = "dark";
  } else if (lux < 200) {
    newState = "dim";
  } else {
    newState = "bright";
  }

  sensorReady = true;
  readCount++;
#endif

  lightLevel = newLevel;
  lightState = newState;

  // Check if level changed significantly
  bool levelChanged = (lastReportedLevel < 0) ||
                      (abs(newLevel - lastReportedLevel) >= LIGHT_CHANGE_THRESHOLD);
  bool stateChanged = (newState != lastReportedState);

  // Update mesh state if changed
  if (levelChanged) {
    lastReportedLevel = newLevel;

    swarm.setState("light", String(newLevel));

    String zoneKey = String("light_") + SENSOR_ZONE;
    swarm.setState(zoneKey, String(newLevel));

#ifdef LIGHT_SENSOR_LDR
    Serial.printf("[LIGHT] Level: %d%%\n", newLevel);
#else
    Serial.printf("[LIGHT] Level: %d lux\n", newLevel);
#endif
  }

  if (stateChanged) {
    lastReportedState = newState;

    swarm.setState("light_state", newState);

    String zoneKey = String("light_state_") + SENSOR_ZONE;
    swarm.setState(zoneKey, newState);

    Serial.printf("[LIGHT] State: %s\n", newState.c_str());
  }
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

#ifdef LIGHT_SENSOR_LDR
  // Configure ADC
  analogReadResolution(12);  // 12-bit (0-4095)
  analogSetAttenuation(ADC_11db);  // Full 0-3.3V range
  pinMode(LDR_PIN, INPUT);
  Serial.printf("[LIGHT] LDR on GPIO%d\n", LDR_PIN);
#endif

#ifdef LIGHT_SENSOR_BH1750
  // Initialize BH1750
  Wire.begin();
  if (bh1750Init()) {
    Serial.println("[LIGHT] BH1750 initialized");
    sensorReady = true;
  } else {
    Serial.println("[LIGHT] BH1750 init FAILED!");
  }
#endif

  Serial.printf("[LIGHT] Sensor: %s\n", SENSOR_MODEL);
  Serial.printf("[LIGHT] Zone: %s\n", SENSOR_ZONE);
  Serial.printf("[LIGHT] Read interval: %dms\n", READ_INTERVAL);
  Serial.println();

  // Register light polling in loop
  swarm.onLoop(pollLight);

  // Register custom serial command
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "light") {
      Serial.println("\n--- LIGHT SENSOR ---");
      Serial.printf("Model: %s\n", SENSOR_MODEL);
#ifdef LIGHT_SENSOR_LDR
      Serial.printf("GPIO: %d\n", LDR_PIN);
      Serial.printf("Raw ADC: %d\n", analogRead(LDR_PIN));
#endif
#ifdef LIGHT_SENSOR_BH1750
      Serial.printf("Address: 0x%02X\n", BH1750_ADDR);
#endif
      Serial.printf("Ready: %s\n", sensorReady ? "YES" : "NO");
      if (sensorReady) {
#ifdef LIGHT_SENSOR_LDR
        Serial.printf("Light level: %d%%\n", lightLevel);
#else
        Serial.printf("Light level: %d lux\n", lightLevel);
#endif
        Serial.printf("State: %s\n", lightState.c_str());
      }
      Serial.printf("Read count: %lu\n", readCount);
      Serial.printf("Error count: %lu\n", errorCount);
      Serial.printf("Zone: %s\n", SENSOR_ZONE);
      Serial.println();
      return true;
    }
    return false;
  });

  // Register custom display section
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    display.print(SENSOR_MODEL);
    display.print(":");
    if (!sensorReady) {
      display.println("WAITING...");
    } else {
#ifdef LIGHT_SENSOR_LDR
      display.printf("%d%% %s\n", lightLevel, lightState.c_str());
#else
      display.printf("%dlux %s\n", lightLevel, lightState.c_str());
#endif
    }

    display.println("---------------------");

    if (sensorReady) {
      display.printf("light=%d\n", lightLevel);
      display.printf("state=%s\n", lightState.c_str());
    } else {
      display.println("light=--");
      display.println("state=--");
    }
    display.printf("zone=%s\n", SENSOR_ZONE);
    display.printf("reads=%lu\n", readCount);
  });

  // Add light sensor to heartbeat
  swarm.setHeartbeatData("light", 1);
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

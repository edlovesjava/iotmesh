/*
 * ESP32 Mesh Swarm - PIR Motion Sensor Node
 *
 * Polls a Lafvin Nano over I2C for PIR motion state and
 * publishes to the mesh network shared state.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - Lafvin Nano with PIR sensor (I2C slave at 0x42)
 *   - Level shifter between ESP32 (3.3V) and Nano (5V)
 */

#include <MeshSwarm.h>

// ============== PIR MODULE CONFIGURATION ==============
#define PIR_I2C_ADDR    0x42      // Lafvin Nano I2C address
#define PIR_POLL_MS     200       // Poll interval (ms)
#define MOTION_ZONE     "zone1"   // Zone identifier for this sensor

// PIR Nano Register Map
#define PIR_REG_STATUS        0x00
#define PIR_REG_MOTION_COUNT  0x01
#define PIR_REG_LAST_MOTION   0x02
#define PIR_REG_CONFIG        0x03
#define PIR_REG_HOLD_TIME     0x04
#define PIR_REG_VERSION       0x05

// Status bits
#define PIR_STATUS_MOTION     0x01
#define PIR_STATUS_READY      0x02

// ============== GLOBALS ==============
MeshSwarm swarm;

// PIR state
bool pirConnected = false;
bool pirReady = false;
bool lastMotionState = false;
uint8_t pirVersion = 0;
uint8_t motionCount = 0;
uint8_t lastMotionSec = 255;
unsigned long lastPirPoll = 0;

// ============== PIR I2C FUNCTIONS ==============

uint8_t pirReadRegister(uint8_t reg) {
  Wire.beginTransmission(PIR_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return 0xFF;
  }

  if (Wire.requestFrom(PIR_I2C_ADDR, (uint8_t)1) != 1) {
    return 0xFF;
  }

  return Wire.read();
}

bool pirCheckConnection() {
  Wire.beginTransmission(PIR_I2C_ADDR);
  return Wire.endTransmission() == 0;
}

void pollPirModule() {
  // Check connection
  if (!pirCheckConnection()) {
    if (pirConnected) {
      Serial.println("[PIR] Module disconnected!");
      pirConnected = false;
      pirReady = false;
    }
    return;
  }

  if (!pirConnected) {
    pirConnected = true;
    pirVersion = pirReadRegister(PIR_REG_VERSION);
    Serial.printf("[PIR] Module connected! Version: 0x%02X\n", pirVersion);
  }

  // Read status
  uint8_t status = pirReadRegister(PIR_REG_STATUS);
  if (status == 0xFF) return;

  pirReady = (status & PIR_STATUS_READY) != 0;
  bool motionDetected = (status & PIR_STATUS_MOTION) != 0;

  // Read additional info
  lastMotionSec = pirReadRegister(PIR_REG_LAST_MOTION);

  // State changed?
  if (motionDetected != lastMotionState) {
    lastMotionState = motionDetected;

    // Read motion count
    motionCount = pirReadRegister(PIR_REG_MOTION_COUNT);

    // Update mesh state
    String motionValue = motionDetected ? "1" : "0";
    swarm.setState("motion", motionValue);

    // Also publish zone-specific state
    String zoneKey = String("motion_") + MOTION_ZONE;
    swarm.setState(zoneKey, motionValue);

    Serial.printf("[PIR] Motion %s (count: %d)\n",
                  motionDetected ? "DETECTED" : "cleared", motionCount);
  }
}

// ============== SETUP ==============
void setup() {
  swarm.begin();

  // Check PIR module at startup
  Serial.print("[PIR] Checking I2C address 0x");
  Serial.print(PIR_I2C_ADDR, HEX);
  Serial.print("... ");

  if (pirCheckConnection()) {
    pirConnected = true;
    pirVersion = pirReadRegister(PIR_REG_VERSION);
    Serial.printf("Found! Version: 0x%02X\n", pirVersion);

    // Wait for PIR to be ready
    Serial.println("[PIR] Waiting for sensor ready...");
    unsigned long waitStart = millis();
    while (millis() - waitStart < 35000) {
      uint8_t status = pirReadRegister(PIR_REG_STATUS);
      if (status & PIR_STATUS_READY) {
        pirReady = true;
        Serial.println("[PIR] Sensor ready!");
        break;
      }
      delay(1000);
      Serial.print(".");
    }
    if (!pirReady) {
      Serial.println("\n[PIR] Warning: Sensor not ready yet");
    }
  } else {
    Serial.println("Not found!");
  }

  Serial.printf("[PIR] Zone: %s\n", MOTION_ZONE);
  Serial.println();

  // Register custom loop for PIR polling
  swarm.onLoop([]() {
    unsigned long now = millis();
    if (now - lastPirPoll >= PIR_POLL_MS) {
      pollPirModule();
      lastPirPoll = now;
    }
  });

  // Register custom serial command
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "pir") {
      Serial.println("\n--- PIR MODULE ---");
      Serial.printf("Connected: %s\n", pirConnected ? "YES" : "NO");
      Serial.printf("Ready: %s\n", pirReady ? "YES" : "NO");
      Serial.printf("Version: 0x%02X\n", pirVersion);
      Serial.printf("Motion: %s\n", lastMotionState ? "DETECTED" : "none");
      Serial.printf("Last motion: %d sec ago\n", lastMotionSec);
      Serial.printf("Zone: %s\n", MOTION_ZONE);
      Serial.println();
      return true;  // Command handled
    }
    return false;  // Not our command
  });

  // Register custom display section
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    // Line 3: PIR Status
    display.print("PIR:");
    if (!pirConnected) {
      display.println("DISCONNECTED");
    } else if (!pirReady) {
      display.println("WARMING UP");
    } else {
      display.printf("%s last:%ds\n",
                     lastMotionState ? "MOTION!" : "idle",
                     lastMotionSec);
    }

    display.println("------------------------");

    // Show motion state
    display.printf("motion=%s\n", lastMotionState ? "1" : "0");
    display.printf("zone=%s\n", MOTION_ZONE);
  });

  // Add PIR status to heartbeat
  swarm.setHeartbeatData("pir", pirConnected ? 1 : 0);
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

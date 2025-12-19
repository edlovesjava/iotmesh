/*
 * PIR Motion Sensor Module - Lafvin Nano I2C Slave
 *
 * Reads PIR sensor, filters signal, and exposes motion state
 * over I2C for an ESP32 mesh node to poll.
 *
 * Hardware:
 *   - Lafvin Nano (Arduino Nano compatible)
 *   - PIR sensor (HC-SR501 or similar)
 *     - VCC -> 5V
 *     - GND -> GND
 *     - OUT -> D2
 *   - I2C to ESP32:
 *     - A4 (SDA) -> ESP32 GPIO21 (via level shifter recommended)
 *     - A5 (SCL) -> ESP32 GPIO22 (via level shifter recommended)
 *     - GND -> Common ground
 *
 * I2C Address: 0x42 (configurable)
 *
 * Register Map:
 *   0x00 STATUS       (R)   Bit0: motion, Bit1: sensor ready
 *   0x01 MOTION_COUNT (R)   Events since last read (auto-clears)
 *   0x02 LAST_MOTION  (R)   Seconds since last motion (max 255)
 *   0x03 CONFIG       (R/W) Bit0: enable, Bit1: auto-clear
 *   0x04 HOLD_TIME    (R/W) Motion hold time in seconds (default 3)
 *   0x05 VERSION      (R)   Firmware version (0x10 = v1.0)
 *
 * Serial: 115200 baud (debug output)
 */

#include <Wire.h>

// ============== CONFIGURATION ==============
#define I2C_ADDRESS       0x42    // I2C slave address
#define PIR_PIN           2       // PIR digital output
#define LED_PIN           13      // Onboard LED

#define DEFAULT_HOLD_SEC  3       // Motion hold time (seconds)
#define DEBOUNCE_MS       50      // Debounce time
#define WARMUP_SEC        30      // PIR warmup time

#define FIRMWARE_VERSION  0x10    // v1.0

// ============== REGISTER ADDRESSES ==============
#define REG_STATUS        0x00
#define REG_MOTION_COUNT  0x01
#define REG_LAST_MOTION   0x02
#define REG_CONFIG        0x03
#define REG_HOLD_TIME     0x04
#define REG_VERSION       0x05
#define REG_COUNT         6

// ============== STATUS BITS ==============
#define STATUS_MOTION     0x01
#define STATUS_READY      0x02

// ============== CONFIG BITS ==============
#define CONFIG_ENABLE     0x01
#define CONFIG_AUTOCLEAR  0x02

// ============== STATE ==============
volatile uint8_t registers[REG_COUNT];
volatile uint8_t regPointer = 0;
volatile uint8_t motionCountRead = 0;  // For auto-clear

// PIR state
volatile bool pirTriggered = false;
bool motionActive = false;
unsigned long lastMotionTime = 0;
unsigned long lastTriggerTime = 0;
uint8_t motionCount = 0;

// Timing
unsigned long bootTime = 0;
bool sensorReady = false;

// ============== ISR: PIR Interrupt ==============
void pirISR() {
  unsigned long now = millis();
  if (now - lastTriggerTime > DEBOUNCE_MS) {
    pirTriggered = true;
    lastTriggerTime = now;
  }
}

// ============== ISR: I2C Receive ==============
void i2cReceive(int numBytes) {
  if (numBytes < 1) return;

  regPointer = Wire.read();
  if (regPointer >= REG_COUNT) {
    regPointer = 0;
  }

  // If there's more data, it's a write
  while (Wire.available()) {
    uint8_t value = Wire.read();

    // Only allow writes to CONFIG and HOLD_TIME
    if (regPointer == REG_CONFIG) {
      registers[REG_CONFIG] = value;
    }
    else if (regPointer == REG_HOLD_TIME) {
      registers[REG_HOLD_TIME] = value;
      if (registers[REG_HOLD_TIME] < 1) registers[REG_HOLD_TIME] = 1;
    }

    regPointer++;
    if (regPointer >= REG_COUNT) regPointer = 0;
  }
}

// ============== ISR: I2C Request ==============
void i2cRequest() {
  // Send register at current pointer
  Wire.write(registers[regPointer]);

  // Auto-clear motion count after read
  if (regPointer == REG_MOTION_COUNT) {
    motionCountRead = 1;  // Flag for main loop to clear
  }

  // Auto-increment pointer
  regPointer++;
  if (regPointer >= REG_COUNT) regPointer = 0;
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("╔═══════════════════════════════════╗"));
  Serial.println(F("║   PIR MODULE - I2C SLAVE v1.0     ║"));
  Serial.println(F("╚═══════════════════════════════════╝"));
  Serial.println();

  // Pin setup
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize registers
  registers[REG_STATUS] = 0;
  registers[REG_MOTION_COUNT] = 0;
  registers[REG_LAST_MOTION] = 255;
  registers[REG_CONFIG] = CONFIG_ENABLE | CONFIG_AUTOCLEAR;
  registers[REG_HOLD_TIME] = DEFAULT_HOLD_SEC;
  registers[REG_VERSION] = FIRMWARE_VERSION;

  Serial.print(F("I2C Address: 0x"));
  Serial.println(I2C_ADDRESS, HEX);
  Serial.print(F("PIR Pin: D"));
  Serial.println(PIR_PIN);
  Serial.print(F("Hold time: "));
  Serial.print(DEFAULT_HOLD_SEC);
  Serial.println(F("s"));
  Serial.println();

  // PIR warmup
  Serial.print(F("PIR warmup ("));
  Serial.print(WARMUP_SEC);
  Serial.println(F("s)..."));

  bootTime = millis();
  for (int i = WARMUP_SEC; i > 0; i--) {
    Serial.print(i);
    Serial.print(F(" "));
    if (i % 10 == 0 && i != WARMUP_SEC) Serial.println();

    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(900);
  }
  Serial.println();

  // Mark sensor ready
  sensorReady = true;
  registers[REG_STATUS] |= STATUS_READY;

  // Attach PIR interrupt
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), pirISR, RISING);

  // Initialize I2C slave
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(i2cReceive);
  Wire.onRequest(i2cRequest);

  Serial.println(F("Ready! I2C slave active."));
  Serial.println(F("─────────────────────────────────────"));
  Serial.println();
}

// ============== MAIN LOOP ==============
void loop() {
  unsigned long now = millis();

  // Check if enabled
  bool enabled = registers[REG_CONFIG] & CONFIG_ENABLE;

  // Handle PIR trigger
  if (pirTriggered && enabled) {
    pirTriggered = false;

    if (!motionActive) {
      // New motion event
      motionActive = true;
      motionCount++;
      registers[REG_MOTION_COUNT] = motionCount;
      registers[REG_STATUS] |= STATUS_MOTION;

      digitalWrite(LED_PIN, HIGH);

      Serial.print(F("["));
      Serial.print((now - bootTime) / 1000.0, 1);
      Serial.print(F("s] Motion #"));
      Serial.println(motionCount);
    }

    // Reset hold timer
    lastMotionTime = now;
  }

  // Hold time expired
  unsigned long holdMs = (unsigned long)registers[REG_HOLD_TIME] * 1000;
  if (motionActive && (now - lastMotionTime >= holdMs)) {
    motionActive = false;
    registers[REG_STATUS] &= ~STATUS_MOTION;

    digitalWrite(LED_PIN, LOW);

    Serial.print(F("["));
    Serial.print((now - bootTime) / 1000.0, 1);
    Serial.println(F("s] Motion cleared"));
  }

  // Update last motion time register
  if (lastMotionTime > 0) {
    unsigned long secSince = (now - lastMotionTime) / 1000;
    if (secSince > 255) secSince = 255;
    registers[REG_LAST_MOTION] = (uint8_t)secSince;
  }

  // Handle motion count auto-clear (set by I2C read)
  if (motionCountRead) {
    if (registers[REG_CONFIG] & CONFIG_AUTOCLEAR) {
      motionCount = 0;
      registers[REG_MOTION_COUNT] = 0;
    }
    motionCountRead = 0;
  }

  // Periodic status
  static unsigned long lastStatus = 0;
  if (now - lastStatus >= 10000) {
    lastStatus = now;

    Serial.print(F("["));
    Serial.print((now - bootTime) / 1000.0, 1);
    Serial.print(F("s] Status: motion="));
    Serial.print(motionActive ? F("YES") : F("NO"));
    Serial.print(F(" count="));
    Serial.print(motionCount);
    Serial.print(F(" last="));
    Serial.print(registers[REG_LAST_MOTION]);
    Serial.println(F("s"));
  }

  delay(10);
}

/*
 * PIR Sensor Test Sketch for Lafvin Nano
 *
 * DIAGNOSTIC VERSION - Simplified for troubleshooting
 *
 * Hardware:
 *   - Lafvin Nano (Arduino Nano compatible)
 *   - PIR sensor (HC-SR501 or similar)
 *     - VCC -> 5V
 *     - GND -> GND
 *     - OUT -> D2
 *
 * Serial: 115200 baud
 */

// ============== CONFIGURATION ==============
#define PIR_PIN           2      // PIR digital output
#define LED_PIN           13      // Onboard LED for visual feedback

// ============== STATE ==============
int lastPirState = -1;  // Track changes
unsigned long motionCount = 0;
unsigned long lastChangeTime = 0;

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println();
  Serial.println(F("╔═══════════════════════════════════╗"));
  Serial.println(F("║  PIR DIAGNOSTIC TEST - LAFVIN NANO║"));
  Serial.println(F("╚═══════════════════════════════════╝"));
  Serial.println();

  // Pin setup
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println(F("PIR Pin: D2"));
  Serial.println(F("This sketch simply reads the pin state every 100ms"));
  Serial.println(F("and reports any changes."));
  Serial.println();
  Serial.println(F("HC-SR501 Adjustment Pots:"));
  Serial.println(F("  - Sensitivity (Sx): Turn CLOCKWISE to increase range"));
  Serial.println(F("  - Time delay (Tx): Turn COUNTER-CLOCKWISE for shorter pulses"));
  Serial.println();
  Serial.println(F("Jumper setting:"));
  Serial.println(F("  - H: Repeatable trigger (recommended)"));
  Serial.println(F("  - L: Single trigger"));
  Serial.println();

  // Quick warmup
  Serial.println(F("Waiting 10 seconds for PIR warmup..."));
  for (int i = 10; i > 0; i--) {
    Serial.print(i); Serial.print(F(" "));
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(900);
  }
  Serial.println();
  Serial.println(F("─────────────────────────────────────"));
  Serial.println(F("Monitoring D2 pin state..."));
  Serial.println();

  // Read initial state
  lastPirState = digitalRead(PIR_PIN);
  Serial.print(F("Initial state: "));
  Serial.println(lastPirState ? F("HIGH") : F("LOW"));
  Serial.println();
}

// ============== MAIN LOOP ==============
void loop() {
  unsigned long now = millis();
  int pirState = digitalRead(PIR_PIN);

  // Update LED to match PIR state
  digitalWrite(LED_PIN, pirState);

  // Detect state change
  if (pirState != lastPirState) {
    unsigned long elapsed = now - lastChangeTime;
    lastChangeTime = now;

    if (pirState == HIGH) {
      motionCount++;
      Serial.print(F("["));
      Serial.print(now / 1000.0, 1);
      Serial.print(F("s] ▲ HIGH - Motion #"));
      Serial.print(motionCount);
      if (lastPirState != -1) {
        Serial.print(F(" (was LOW for "));
        Serial.print(elapsed);
        Serial.print(F("ms)"));
      }
      Serial.println();
    } else {
      Serial.print(F("["));
      Serial.print(now / 1000.0, 1);
      Serial.print(F("s] ▼ LOW  - Motion ended"));
      Serial.print(F(" (was HIGH for "));
      Serial.print(elapsed);
      Serial.print(F("ms)"));
      Serial.println();
    }

    lastPirState = pirState;
  }

  // Periodic status every 5 seconds
  static unsigned long lastStatus = 0;
  if (now - lastStatus >= 5000) {
    lastStatus = now;
    Serial.print(F("["));
    Serial.print(now / 1000.0, 1);
    Serial.print(F("s] Status: D3="));
    Serial.print(pirState ? F("HIGH") : F("LOW"));
    Serial.print(F(" | Events: "));
    Serial.print(motionCount);
    Serial.println();
  }

  delay(50);  // Poll every 50ms
}

/*
 * ESP32 Mesh Swarm - DHT11 Temperature/Humidity Node
 *
 * Reads DHT11 sensor and publishes temperature and humidity
 * to the mesh network.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - DHT11 sensor
 *     - VCC -> 3.3V
 *     - GND -> GND
 *     - DATA -> GPIO4 (with 10k pull-up to VCC)
 */

#include <MeshSwarm.h>
#include <DHT.h>

// ============== DHT CONFIGURATION ==============
#define DHT_PIN         4         // GPIO4 for DHT11 data
#define DHT_TYPE        DHT11     // DHT11 sensor type
#define READ_INTERVAL   5000      // Read every 5 seconds
#define SENSOR_ZONE     "zone1"   // Zone identifier for this sensor

// ============== GLOBALS ==============
MeshSwarm swarm;
DHT dht(DHT_PIN, DHT_TYPE);

// Sensor state
float temperature = NAN;
float humidity = NAN;
bool sensorReady = false;
unsigned long lastReadTime = 0;
unsigned long readCount = 0;
unsigned long errorCount = 0;

// ============== DHT HANDLING ==============
void pollDht() {
  unsigned long now = millis();

  if (now - lastReadTime < READ_INTERVAL) return;
  lastReadTime = now;

  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  // Check for read errors
  if (isnan(newTemp) || isnan(newHumidity)) {
    errorCount++;
    Serial.printf("[DHT] Read error (#%lu)\n", errorCount);
    return;
  }

  readCount++;
  sensorReady = true;

  // Check if values changed significantly (0.5 degree or 1% humidity)
  bool tempChanged = isnan(temperature) || abs(newTemp - temperature) >= 0.5;
  bool humidityChanged = isnan(humidity) || abs(newHumidity - humidity) >= 1.0;

  temperature = newTemp;
  humidity = newHumidity;

  // Update mesh state if changed
  if (tempChanged) {
    char tempStr[8];
    dtostrf(temperature, 4, 1, tempStr);
    swarm.setState("temp", tempStr);

    String zoneKey = String("temp_") + SENSOR_ZONE;
    swarm.setState(zoneKey, tempStr);

    Serial.printf("[DHT] Temperature: %s°C\n", tempStr);
  }

  if (humidityChanged) {
    char humStr[8];
    dtostrf(humidity, 4, 1, humStr);
    swarm.setState("humidity", humStr);

    String zoneKey = String("humidity_") + SENSOR_ZONE;
    swarm.setState(zoneKey, humStr);

    Serial.printf("[DHT] Humidity: %s%%\n", humStr);
  }
}

// ============== SETUP ==============
void setup() {
  swarm.begin();

  // DHT sensor setup
  dht.begin();
  Serial.printf("[DHT] Sensor on GPIO%d\n", DHT_PIN);
  Serial.printf("[DHT] Zone: %s\n", SENSOR_ZONE);
  Serial.printf("[DHT] Read interval: %dms\n", READ_INTERVAL);
  Serial.println();

  // Register DHT polling in loop
  swarm.onLoop(pollDht);

  // Register custom serial command
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "dht") {
      Serial.println("\n--- DHT11 SENSOR ---");
      Serial.printf("GPIO: %d\n", DHT_PIN);
      Serial.printf("Ready: %s\n", sensorReady ? "YES" : "NO");
      if (sensorReady) {
        Serial.printf("Temperature: %.1f°C\n", temperature);
        Serial.printf("Humidity: %.1f%%\n", humidity);
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
    display.print("DHT:");
    if (!sensorReady) {
      display.println("WAITING...");
    } else {
      display.printf("%.1fC %.0f%%\n", temperature, humidity);
    }

    display.println("------------------------");

    // Show state values
    if (sensorReady) {
      display.printf("temp=%.1f\n", temperature);
      display.printf("humidity=%.1f\n", humidity);
    } else {
      display.println("temp=--");
      display.println("humidity=--");
    }
    display.printf("zone=%s\n", SENSOR_ZONE);
    display.printf("reads=%lu err=%lu\n", readCount, errorCount);
  });

  // Add DHT status to heartbeat
  swarm.setHeartbeatData("dht", 1);
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

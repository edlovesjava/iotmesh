/**
 * SensorNode - DHT11 temperature/humidity sensor
 *
 * Demonstrates:
 * - Reading sensor data periodically
 * - Publishing sensor values to mesh
 * - Custom display updates
 * - Custom serial commands
 *
 * Hardware:
 * - ESP32
 * - SSD1306 OLED (I2C: SDA=21, SCL=22)
 * - DHT11 sensor on GPIO4
 *
 * Requires:
 * - DHT sensor library (Adafruit)
 */

#include <MeshSwarm.h>
#include <DHT.h>

MeshSwarm swarm;

#define DHT_PIN  4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

float lastTemp = 0;
float lastHumid = 0;
unsigned long lastRead = 0;
const unsigned long readInterval = 5000;  // Read every 5 seconds

void setup() {
  Serial.begin(115200);

  dht.begin();

  swarm.begin("Sensor");

  // Custom display section
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    display.println("---------------------");
    display.printf("Temp:  %.1f C\n", lastTemp);
    display.printf("Humid: %.1f %%\n", lastHumid);
  });

  // Custom serial command
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "dht" || input == "sensor") {
      Serial.printf("Temperature: %.1f C\n", lastTemp);
      Serial.printf("Humidity:    %.1f %%\n", lastHumid);
      return true;
    }
    return false;
  });

  // Periodic sensor reading
  swarm.onLoop([]() {
    if (millis() - lastRead < readInterval) return;
    lastRead = millis();

    float temp = dht.readTemperature();
    float humid = dht.readHumidity();

    if (isnan(temp) || isnan(humid)) {
      Serial.println("DHT read failed");
      return;
    }

    // Only publish if values changed significantly
    if (abs(temp - lastTemp) >= 0.5 || abs(humid - lastHumid) >= 1.0) {
      lastTemp = temp;
      lastHumid = humid;

      swarm.setState("temp", String(temp, 1));
      swarm.setState("humid", String(humid, 1));

      Serial.printf("Published: temp=%.1f, humid=%.1f\n", temp, humid);
    }
  });

  Serial.println("SensorNode started");
}

void loop() {
  swarm.update();
}

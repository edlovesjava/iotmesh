/**
 * Touch169 Node - ESP32-S3 1.69" Touch Display
 *
 * Waveshare ESP32-S3-Touch-LCD-1.69 with ST7789V2 display (240x280).
 * Shows analog clock with sensor data in corners:
 *   - Top-left: Humidity
 *   - Top-right: Temperature
 *   - Bottom-left: Light level
 *   - Bottom-right: Motion and LED states
 *
 * Hardware:
 *   - ESP32-S3R8 (dual-core, 8MB PSRAM)
 *   - 1.69" IPS LCD ST7789V2 (240x280)
 *   - CST816T capacitive touch controller
 *   - QMI8658 6-axis IMU (accelerometer + gyroscope)
 *   - PCF85063 RTC chip
 *
 * Display Pins (ST7789V2 SPI):
 *   - MOSI = GPIO15
 *   - SCK  = GPIO18
 *   - CS   = GPIO16
 *   - DC   = GPIO2
 *   - RST  = GPIO3
 *   - BL   = GPIO17
 *
 * I2C Bus (Touch, IMU, RTC):
 *   - SDA  = GPIO11
 *   - SCL  = GPIO10
 */

#include <Arduino.h>

// Disable MeshSwarm's built-in SSD1306 display - we use our own TFT
#define MESHSWARM_ENABLE_DISPLAY 0

#include <MeshSwarm.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <sys/time.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Touch169"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "touch169"
#endif

// ============== TIMEZONE CONFIG ==============
#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC  -18000  // EST = UTC-5
#endif

#ifndef DAYLIGHT_OFFSET
#define DAYLIGHT_OFFSET 3600    // 1 hour daylight saving
#endif

// ============== DISPLAY PINS (configured in platformio.ini) ==============
#ifndef TFT_BL
#define TFT_BL 15
#endif

// ============== DISPLAY GEOMETRY ==============
#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   280
#define CENTER_X        120
#define CENTER_Y        140      // Center of display
#define CLOCK_RADIUS    80       // Clock face radius
#define HOUR_HAND_LEN   40
#define MIN_HAND_LEN    55
#define SEC_HAND_LEN    65

// Corner positions for sensor data
#define CORNER_MARGIN   8
#define CORNER_TEXT_H   16

// ============== DISPLAY COLORS (RGB565) ==============
#define COLOR_BG        0x0000  // Black
#define COLOR_FACE      0x2104  // Dark gray
#define COLOR_HOUR      0xFFFF  // White
#define COLOR_MINUTE    0xFFFF  // White
#define COLOR_SECOND    0xF800  // Red
#define COLOR_TEXT      0xFFFF  // White
#define COLOR_TEMP      0xFD20  // Orange
#define COLOR_HUMID     0x07E0  // Green
#define COLOR_LIGHT     0xFFE0  // Yellow
#define COLOR_MOTION    0x07FF  // Cyan
#define COLOR_LED       0xF81F  // Magenta
#define COLOR_TICK      0x8410  // Gray
#define COLOR_DATE      0xAD55  // Light gray

// ============== GLOBALS ==============
MeshSwarm swarm;
TFT_eSPI tft = TFT_eSPI();

// Sensor data from mesh
String meshTemp = "--";
String meshHumid = "--";
String meshLight = "--";
String meshMotion = "0";
String meshLed = "0";
bool hasSensorData = false;

// Previous sensor values for efficient redraw
String prevTemp = "";
String prevHumid = "";
String prevLight = "";
String prevMotion = "";
String prevLed = "";

// Time tracking
int lastSec = -1;
int lastMin = -1;
int lastHour = -1;
bool timeValid = false;

// Mesh-based time
unsigned long meshTimeBase = 0;
unsigned long meshTimeMillis = 0;
bool hasMeshTime = false;

// Previous hand positions for efficient redraw
float prevSecAngle = -999;
float prevMinAngle = -999;
float prevHourAngle = -999;

// Screen state
bool screenChanged = true;
bool firstDraw = true;

// ============== FUNCTION DECLARATIONS ==============
void drawClockFace();
void drawHand(float angle, int length, uint16_t color, int width);
void eraseHand(float angle, int length, int width);
void updateClock();
void updateCorners();
void drawCornerLabels();
void setMeshTime(unsigned long unixTime);
bool getMeshTime(struct tm* timeinfo);

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[TOUCH169] Starting...");

  // Initialize backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize display
  tft.init();
  tft.setRotation(0);  // Portrait mode
  tft.fillScreen(COLOR_BG);

  Serial.println("[TOUCH169] Display initialized");

  // Show startup message
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(CENTER_X - 60, CENTER_Y - 10);
  tft.print("Starting...");

  // Initialize MeshSwarm
  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

  Serial.println("[TOUCH169] MeshSwarm initialized");

  // Watch for sensor data from mesh
  swarm.watchState("temp", [](const String& key, const String& value, const String& oldValue) {
    meshTemp = value;
    hasSensorData = true;
    Serial.printf("[TOUCH169] Temperature: %s C\n", value.c_str());
  });

  swarm.watchState("humidity", [](const String& key, const String& value, const String& oldValue) {
    meshHumid = value;
    hasSensorData = true;
    Serial.printf("[TOUCH169] Humidity: %s %%\n", value.c_str());
  });

  swarm.watchState("light", [](const String& key, const String& value, const String& oldValue) {
    meshLight = value;
    hasSensorData = true;
    Serial.printf("[TOUCH169] Light: %s\n", value.c_str());
  });

  swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
    meshMotion = value;
    Serial.printf("[TOUCH169] Motion: %s\n", value.c_str());
  });

  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    meshLed = value;
    Serial.printf("[TOUCH169] LED: %s\n", value.c_str());
  });

  // Watch for time sync from gateway
  swarm.watchState("time", [](const String& key, const String& value, const String& oldValue) {
    unsigned long unixTime = value.toInt();
    if (unixTime > 1700000000) {
      setMeshTime(unixTime);
      Serial.printf("[TOUCH169] Time synced: %lu\n", unixTime);
    }
  });

  // Wildcard watcher for zone-specific sensors
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    if (key.startsWith("temp_") && meshTemp == "--") {
      meshTemp = value;
      hasSensorData = true;
    }
    if (key.startsWith("humidity_") && meshHumid == "--") {
      meshHumid = value;
      hasSensorData = true;
    }
    if (key.startsWith("light_") && meshLight == "--") {
      meshLight = value;
      hasSensorData = true;
    }
    if (key.startsWith("motion_")) {
      meshMotion = value;
    }
  });

  // Clear startup message and draw clock
  delay(500);
  tft.fillScreen(COLOR_BG);
  drawClockFace();
  drawCornerLabels();

  Serial.println("[TOUCH169] Ready");
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
  updateClock();
  updateCorners();
}

// ============== CLOCK FUNCTIONS ==============

void drawClockFace() {
  // Draw outer ring (double thickness)
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS, COLOR_FACE);
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS - 1, COLOR_FACE);

  // Draw hour ticks
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * PI / 180;
    int x1 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 10);
    int y1 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 10);
    int x2 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 3);
    int y2 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 3);

    // Thicker ticks at 12, 3, 6, 9
    if (i % 3 == 0) {
      tft.drawLine(x1-1, y1, x2-1, y2, COLOR_TICK);
      tft.drawLine(x1, y1, x2, y2, COLOR_TICK);
      tft.drawLine(x1+1, y1, x2+1, y2, COLOR_TICK);
    } else {
      tft.drawLine(x1, y1, x2, y2, COLOR_TICK);
    }
  }

  // Draw center dot
  tft.fillCircle(CENTER_X, CENTER_Y, 6, COLOR_HOUR);
}

void drawHand(float angle, int length, uint16_t color, int width) {
  float rad = (angle - 90) * PI / 180;
  int x = CENTER_X + cos(rad) * length;
  int y = CENTER_Y + sin(rad) * length;

  if (width > 1) {
    // Draw thick line for hour/minute hands
    for (int w = -width/2; w <= width/2; w++) {
      tft.drawLine(CENTER_X + w, CENTER_Y, x + w, y, color);
      tft.drawLine(CENTER_X, CENTER_Y + w, x, y + w, color);
    }
  } else {
    tft.drawLine(CENTER_X, CENTER_Y, x, y, color);
  }
}

void eraseHand(float angle, int length, int width) {
  drawHand(angle, length, COLOR_BG, width + 2);
}

void updateClock() {
  struct tm timeinfo;

  // Handle screen change
  if (screenChanged) {
    screenChanged = false;
    tft.fillScreen(COLOR_BG);
    drawClockFace();
    drawCornerLabels();
    lastSec = -1;
    lastMin = -1;
    lastHour = -1;
    prevSecAngle = -999;
    prevMinAngle = -999;
    prevHourAngle = -999;
    firstDraw = true;
  }

  // Try mesh time first, then NTP
  if (!getMeshTime(&timeinfo) && !getLocalTime(&timeinfo)) {
    // Time not yet synced - show waiting message
    if (!timeValid) {
      static unsigned long lastDot = 0;
      static int dots = 0;
      if (millis() - lastDot > 500) {
        lastDot = millis();
        dots = (dots + 1) % 4;
        tft.fillRect(CENTER_X - 70, CENTER_Y - 20, 140, 40, COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.setCursor(CENTER_X - 65, CENTER_Y - 8);
        tft.print("Waiting");
        for (int i = 0; i < dots; i++) tft.print(".");
      }
    }
    return;
  }

  // Time is valid
  if (!timeValid) {
    timeValid = true;
    tft.fillScreen(COLOR_BG);
    drawClockFace();
    drawCornerLabels();
    lastSec = -1;
    lastMin = -1;
    lastHour = -1;
    firstDraw = true;
  }

  int sec = timeinfo.tm_sec;
  int min = timeinfo.tm_min;
  int hour = timeinfo.tm_hour % 12;

  // Only update if time changed
  if (sec == lastSec) return;

  // Calculate angles
  float secAngle = sec * 6;  // 360/60 = 6 degrees per second
  float minAngle = min * 6 + sec * 0.1;  // Smooth minute hand
  float hourAngle = hour * 30 + min * 0.5;  // 360/12 = 30 degrees per hour

  // Erase old hands (in reverse order: second, minute, hour)
  if (prevSecAngle != -999) {
    eraseHand(prevSecAngle, SEC_HAND_LEN, 1);
  }
  if (prevMinAngle != -999 && (min != lastMin || lastMin == -1)) {
    eraseHand(prevMinAngle, MIN_HAND_LEN, 3);
  }
  if (prevHourAngle != -999 && (hour != lastHour || min != lastMin || lastHour == -1)) {
    eraseHand(prevHourAngle, HOUR_HAND_LEN, 5);
  }

  // Draw new hands (hour, minute, second)
  drawHand(hourAngle, HOUR_HAND_LEN, COLOR_HOUR, 5);
  drawHand(minAngle, MIN_HAND_LEN, COLOR_MINUTE, 3);
  drawHand(secAngle, SEC_HAND_LEN, COLOR_SECOND, 1);

  // Redraw center dot
  tft.fillCircle(CENTER_X, CENTER_Y, 6, COLOR_SECOND);

  // Redraw clock face ticks (hands may have erased them)
  if (lastMin != min || firstDraw) {
    drawClockFace();
    firstDraw = false;
  }

  // Store previous angles
  prevSecAngle = secAngle;
  prevMinAngle = minAngle;
  prevHourAngle = hourAngle;

  // Update date display at top center (once per minute)
  if (min != lastMin || lastMin == -1) {
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    // Clear date area (between corner labels)
    tft.fillRect(60, CORNER_MARGIN, 120, 20, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_DATE, COLOR_BG);
    tft.setCursor(75, CORNER_MARGIN + 4);
    tft.printf("%s %s %d", days[timeinfo.tm_wday], months[timeinfo.tm_mon], timeinfo.tm_mday);
  }

  // Digital time at bottom center
  if (lastSec != sec) {
    // Clear digital time area (between corner labels)
    tft.fillRect(70, SCREEN_HEIGHT - 28, 100, 20, COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(72, SCREEN_HEIGHT - 26);
    tft.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  lastSec = sec;
  lastMin = min;
  lastHour = hour;
}

// ============== CORNER DISPLAY FUNCTIONS ==============

void drawCornerLabels() {
  tft.setTextSize(1);

  // Top-left: Humidity label
  tft.setTextColor(COLOR_HUMID, COLOR_BG);
  tft.setCursor(CORNER_MARGIN, CORNER_MARGIN);
  tft.print("HUM");

  // Top-right: Temperature label
  tft.setTextColor(COLOR_TEMP, COLOR_BG);
  tft.setCursor(SCREEN_WIDTH - CORNER_MARGIN - 30, CORNER_MARGIN);
  tft.print("TEMP");

  // Bottom-left: Light label
  tft.setTextColor(COLOR_LIGHT, COLOR_BG);
  tft.setCursor(CORNER_MARGIN, SCREEN_HEIGHT - CORNER_MARGIN - 8);
  tft.print("LUX");

  // Bottom-right: Motion/LED labels
  tft.setTextColor(COLOR_MOTION, COLOR_BG);
  tft.setCursor(SCREEN_WIDTH - CORNER_MARGIN - 30, SCREEN_HEIGHT - CORNER_MARGIN - 20);
  tft.print("PIR");
  tft.setTextColor(COLOR_LED, COLOR_BG);
  tft.setCursor(SCREEN_WIDTH - CORNER_MARGIN - 30, SCREEN_HEIGHT - CORNER_MARGIN - 8);
  tft.print("LED");
}

void updateCorners() {
  // Top-left: Humidity
  if (meshHumid != prevHumid) {
    prevHumid = meshHumid;
    tft.fillRect(CORNER_MARGIN, CORNER_MARGIN + 12, 50, 16, COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_HUMID, COLOR_BG);
    tft.setCursor(CORNER_MARGIN, CORNER_MARGIN + 12);
    if (meshHumid != "--") {
      tft.printf("%s%%", meshHumid.c_str());
    } else {
      tft.print("--");
    }
  }

  // Top-right: Temperature
  if (meshTemp != prevTemp) {
    prevTemp = meshTemp;
    tft.fillRect(SCREEN_WIDTH - CORNER_MARGIN - 55, CORNER_MARGIN + 12, 55, 16, COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEMP, COLOR_BG);
    tft.setCursor(SCREEN_WIDTH - CORNER_MARGIN - 55, CORNER_MARGIN + 12);
    if (meshTemp != "--") {
      tft.printf("%sC", meshTemp.c_str());
    } else {
      tft.print("--");
    }
  }

  // Bottom-left: Light level
  if (meshLight != prevLight) {
    prevLight = meshLight;
    tft.fillRect(CORNER_MARGIN, SCREEN_HEIGHT - CORNER_MARGIN - 26, 55, 16, COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_LIGHT, COLOR_BG);
    tft.setCursor(CORNER_MARGIN, SCREEN_HEIGHT - CORNER_MARGIN - 26);
    if (meshLight != "--") {
      int lightVal = meshLight.toInt();
      if (lightVal >= 1000) {
        tft.printf("%dk", lightVal / 1000);
      } else {
        tft.print(meshLight.c_str());
      }
    } else {
      tft.print("--");
    }
  }

  // Bottom-right: Motion indicator
  if (meshMotion != prevMotion) {
    prevMotion = meshMotion;
    bool motion = (meshMotion == "1");
    tft.fillCircle(SCREEN_WIDTH - CORNER_MARGIN - 8, SCREEN_HEIGHT - CORNER_MARGIN - 14, 5,
                   motion ? COLOR_MOTION : COLOR_BG);
    if (!motion) {
      tft.drawCircle(SCREEN_WIDTH - CORNER_MARGIN - 8, SCREEN_HEIGHT - CORNER_MARGIN - 14, 5, COLOR_TICK);
    }
  }

  // Bottom-right: LED indicator
  if (meshLed != prevLed) {
    prevLed = meshLed;
    bool ledOn = (meshLed == "1" || meshLed == "on" || meshLed == "true");
    tft.fillCircle(SCREEN_WIDTH - CORNER_MARGIN - 8, SCREEN_HEIGHT - CORNER_MARGIN - 2, 5,
                   ledOn ? COLOR_LED : COLOR_BG);
    if (!ledOn) {
      tft.drawCircle(SCREEN_WIDTH - CORNER_MARGIN - 8, SCREEN_HEIGHT - CORNER_MARGIN - 2, 5, COLOR_TICK);
    }
  }
}

// ============== MESH TIME FUNCTIONS ==============

void setMeshTime(unsigned long unixTime) {
  meshTimeBase = unixTime;
  meshTimeMillis = millis();
  hasMeshTime = true;

  // Also set the system time so getLocalTime() works
  struct timeval tv;
  tv.tv_sec = unixTime;
  tv.tv_usec = 0;
  settimeofday(&tv, NULL);
}

bool getMeshTime(struct tm* timeinfo) {
  if (!hasMeshTime) return false;

  // Calculate current time based on mesh time + elapsed millis
  unsigned long elapsed = (millis() - meshTimeMillis) / 1000;
  time_t currentTime = meshTimeBase + elapsed + GMT_OFFSET_SEC + DAYLIGHT_OFFSET;

  // Convert to struct tm
  struct tm* t = localtime(&currentTime);
  if (t) {
    memcpy(timeinfo, t, sizeof(struct tm));
    return true;
  }
  return false;
}

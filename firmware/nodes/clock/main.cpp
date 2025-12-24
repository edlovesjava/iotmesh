/**
 * Clock Node
 *
 * Round TFT display showing time, date, and mesh sensor data.
 * Watches temperature and humidity from other mesh nodes.
 * Two-button interface for screen navigation and time setting.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - 1.28" Round TFT GC9A01 (SPI)
 *     - RST = GPIO27
 *     - DC  = GPIO25
 *     - CS  = GPIO26
 *     - SCL = GPIO18 (SPI clock)
 *     - SDA = GPIO23 (SPI MOSI)
 *   - Left button: GPIO32 (active LOW, internal pullup)
 *   - Right button: GPIO33 (active LOW, internal pullup)
 *
 * Button Functions:
 *   - Both buttons pressed: Enter/exit set time mode
 *   - Left button (in clock/sensor mode): Previous screen
 *   - Right button (in clock/sensor mode): Next screen
 *   - Left button (in set time mode): Decrement hour/minute
 *   - Right button (in set time mode): Increment hour/minute
 *
 * Library:
 *   - DIYables_TFT_Round (install via Arduino Library Manager)
 */

#include <Arduino.h>

// Disable MeshSwarm's built-in SSD1306 display - we use our own TFT
#define MESHSWARM_ENABLE_DISPLAY 0

#include <MeshSwarm.h>
#include <esp_ota_ops.h>
#include <DIYables_TFT_Round.h>
#include <time.h>
#include <sys/time.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Clock"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "clock"
#endif

// ============== DISPLAY PINS ==============
#ifndef TFT_RST
#define TFT_RST 27
#endif

#ifndef TFT_DC
#define TFT_DC  25
#endif

#ifndef TFT_CS
#define TFT_CS  26
#endif

// ============== BUTTON PINS ==============
#ifndef BTN_LEFT
#define BTN_LEFT  32
#endif

#ifndef BTN_RIGHT
#define BTN_RIGHT 33
#endif

#define BTN_DEBOUNCE_MS     50
#define BTN_LONG_PRESS_MS   500
#define BTN_REPEAT_MS       150
#define BTN_BOTH_TIMEOUT_MS 300

// ============== DISPLAY COLORS ==============
#define COLOR_BG        0x0000  // Black
#define COLOR_FACE      0x2104  // Dark gray
#define COLOR_HOUR      0xFFFF  // White
#define COLOR_MINUTE    0xFFFF  // White
#define COLOR_SECOND    0xF800  // Red
#define COLOR_TEXT      0xFFFF  // White
#define COLOR_TEMP      0x07FF  // Cyan
#define COLOR_HUMID     0x07E0  // Green
#define COLOR_TICK      0x8410  // Gray
#define COLOR_SET_TIME  0xFFE0  // Yellow for set time mode
#define COLOR_ARC_BG    0x2104  // Dark gray for arc background
#define COLOR_ARC_TEMP  0x07FF  // Cyan arc
#define COLOR_ARC_HUMID 0x07E0  // Green arc

// ============== DISPLAY GEOMETRY ==============
#define SCREEN_SIZE     240
#define CENTER_X        120
#define CENTER_Y        110      // Shift clock up to make room for digital time at bottom
#define CLOCK_RADIUS    70       // Smaller clock face to leave room for text
#define HOUR_HAND_LEN   30       // Shorter hands for smaller clock
#define MIN_HAND_LEN    45
#define SEC_HAND_LEN    55

// ============== NTP CONFIG ==============
#ifndef NTP_SERVER
#define NTP_SERVER      "pool.ntp.org"
#endif

#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC  -18000  // EST = UTC-5 (adjust for your timezone)
#endif

#ifndef DAYLIGHT_OFFSET
#define DAYLIGHT_OFFSET 3600    // 1 hour daylight saving
#endif

// ============== SCREEN/MODE ENUMS ==============
enum ScreenMode {
  SCREEN_CLOCK = 0,
  SCREEN_SENSOR = 1,
  SCREEN_COUNT = 2
};

enum ClockMode {
  MODE_NORMAL = 0,
  MODE_SET_HOUR = 1,
  MODE_SET_MINUTE = 2
};

// ============== GLOBALS ==============
MeshSwarm swarm;
DIYables_TFT_GC9A01_Round tft(TFT_RST, TFT_DC, TFT_CS);

// Sensor data from mesh
String meshTemp = "--";
String meshHumid = "--";
bool hasSensorData = false;

// Time tracking - use mesh time or millis-based fallback
int lastSec = -1;
int lastMin = -1;
int lastHour = -1;
bool timeValid = false;

// Mesh-based time (received from gateway or set manually)
unsigned long meshTimeBase = 0;      // Unix timestamp when we got mesh time
unsigned long meshTimeMillis = 0;    // millis() when we got mesh time
bool hasMeshTime = false;

// Previous hand positions for efficient redraw
float prevSecAngle = -999;
float prevMinAngle = -999;
float prevHourAngle = -999;

// Screen and mode state
ScreenMode currentScreen = SCREEN_CLOCK;
ClockMode clockMode = MODE_NORMAL;
ScreenMode lastScreen = SCREEN_CLOCK;
bool screenChanged = true;

// Set time mode variables
int setHour = 12;
int setMinute = 0;
unsigned long lastSetTimeBlink = 0;
bool setTimeBlinkState = true;

// Button state (volatile for ISR access)
volatile bool btnLeftEvent = false;
volatile bool btnRightEvent = false;
volatile unsigned long btnLeftEventTime = 0;
volatile unsigned long btnRightEventTime = 0;
bool btnLeftHeld = false;
bool btnRightHeld = false;
unsigned long btnLeftPressTime = 0;
unsigned long btnRightPressTime = 0;
unsigned long btnLastRepeat = 0;
bool bothPressed = false;
unsigned long bothPressedStart = 0;

// Set time redraw flags
bool redrawHour = true;
bool redrawMinute = true;
int lastSetHour = -1;
int lastSetMinute = -1;

// Startup timeout for auto set-time mode
unsigned long startupTime = 0;
bool startupTimeoutChecked = false;
#define STARTUP_TIMEOUT_MS 10000  // 10 seconds to wait for gateway time

// ============== FORWARD DECLARATIONS ==============
void drawClockFace();
void drawHand(float angle, int length, uint16_t color, int width);
void eraseHand(float angle, int length, int width);
void updateClock();
void drawSensorData();
void drawDateDisplay();
void drawDigitalHours(int hour);
void drawDigitalMinutes(int min);
void drawDigitalSeconds(int sec);
void drawDigitalColons();
bool getMeshTime(struct tm* timeinfo);
void setMeshTime(unsigned long unixTime);

// Button and screen functions
void handleButtons();
void drawSetTimeScreen();
void updateSetTimeScreen();
void drawSetTimeHour();
void drawSetTimeMinute();
void drawSensorScreen();
void updateSensorScreen();
void drawArc(int cx, int cy, int r, int thickness, float startAngle, float endAngle, uint16_t color);
void enterSetTimeMode();
void exitSetTimeMode();
void switchScreen(ScreenMode screen);

// Button interrupt handlers
void IRAM_ATTR onLeftButtonPress();
void IRAM_ATTR onRightButtonPress();

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid
  esp_ota_mark_app_valid_cancel_rollback();

  // Initialize buttons with internal pullup and interrupts
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN_LEFT), onLeftButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_RIGHT), onRightButtonPress, FALLING);

  // Record startup time for auto set-time mode
  startupTime = millis();

  // Initialize TFT display
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(COLOR_BG);

  // Draw initial clock face
  drawClockFace();
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(CENTER_X - 60, CENTER_Y - 10);
  tft.print("Connecting...");

  // Initialize mesh
  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);

  // Watch for temperature updates
  swarm.watchState("temp", [](const String& key, const String& value, const String& oldValue) {
    meshTemp = value;
    hasSensorData = true;
    Serial.printf("[CLOCK] Temperature: %s C\n", value.c_str());
  });

  // Watch for humidity updates
  swarm.watchState("humid", [](const String& key, const String& value, const String& oldValue) {
    meshHumid = value;
    hasSensorData = true;
    Serial.printf("[CLOCK] Humidity: %s %%\n", value.c_str());
  });

  // Watch for time sync from gateway
  swarm.watchState("time", [](const String& key, const String& value, const String& oldValue) {
    unsigned long unixTime = value.toInt();
    if (unixTime > 1700000000) {  // Sanity check: after Nov 2023
      setMeshTime(unixTime);
      Serial.printf("[CLOCK] Time synced from mesh: %lu\n", unixTime);
    }
  });

  // Also watch zone-specific variants
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    if (key.startsWith("temp_") && meshTemp == "--") {
      meshTemp = value;
      hasSensorData = true;
    }
    if (key.startsWith("humid_") && meshHumid == "--") {
      meshHumid = value;
      hasSensorData = true;
    }
  });

  // Add custom serial commands
  swarm.onSerialCommand([](const String& input) -> bool {
    if (input == "clock") {
      struct tm timeinfo;
      if (getMeshTime(&timeinfo)) {
        Serial.printf("Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        Serial.printf("Date: %04d-%02d-%02d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
      } else {
        Serial.println("Time not synced - use 'settime HH:MM' to set manually");
      }
      Serial.printf("Temp: %s C, Humid: %s %%\n", meshTemp.c_str(), meshHumid.c_str());
      return true;
    }
    // Manual time set: "settime HH:MM" or "settime HH:MM:SS"
    if (input.startsWith("settime ")) {
      String timeStr = input.substring(8);
      int hour = 0, min = 0, sec = 0;
      if (sscanf(timeStr.c_str(), "%d:%d:%d", &hour, &min, &sec) >= 2) {
        // Create a fake unix timestamp for today with this time
        // Use a base date (Dec 23, 2024) + time offset
        unsigned long baseDate = 1734912000;  // Dec 23, 2024 00:00:00 UTC
        unsigned long unixTime = baseDate + (hour * 3600) + (min * 60) + sec - GMT_OFFSET_SEC;
        setMeshTime(unixTime);
        Serial.printf("Time set to %02d:%02d:%02d\n", hour, min, sec);
        return true;
      }
      Serial.println("Usage: settime HH:MM or settime HH:MM:SS");
      return true;
    }
    return false;
  });

  // Configure NTP (will sync when gateway provides WiFi time)
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);

  Serial.println("[CLOCK] Clock node started");

  // Redraw clean face after init
  delay(1000);
  tft.fillScreen(COLOR_BG);
  drawClockFace();
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
  handleButtons();

  // Check for startup timeout - auto enter set time mode if no gateway time
  if (!startupTimeoutChecked && !hasMeshTime && millis() - startupTime > STARTUP_TIMEOUT_MS) {
    startupTimeoutChecked = true;
    Serial.println("[CLOCK] No gateway time received, entering set time mode");
    enterSetTimeMode();
  }

  // Handle current screen/mode
  if (clockMode != MODE_NORMAL) {
    updateSetTimeScreen();
  } else {
    switch (currentScreen) {
      case SCREEN_CLOCK:
        updateClock();
        break;
      case SCREEN_SENSOR:
        updateSensorScreen();
        break;
    }
  }
}

// ============== CLOCK FUNCTIONS ==============

void drawClockFace() {
  // Draw outer ring
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS, COLOR_FACE);
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS - 1, COLOR_FACE);

  // Draw hour ticks
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * PI / 180;
    int x1 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 8);
    int y1 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 8);
    int x2 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 2);
    int y2 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 2);
    tft.drawLine(x1, y1, x2, y2, COLOR_TICK);
  }

  // Draw center dot
  tft.fillCircle(CENTER_X, CENTER_Y, 5, COLOR_HOUR);
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
    lastSec = -1;
    lastMin = -1;
    lastHour = -1;
    prevSecAngle = -999;
    prevMinAngle = -999;
    prevHourAngle = -999;
  }

  // Try mesh time first, then NTP
  if (!getMeshTime(&timeinfo) && !getLocalTime(&timeinfo)) {
    // Time not yet synced - show waiting message with instructions
    if (!timeValid) {
      static unsigned long lastDot = 0;
      static int dots = 0;
      if (millis() - lastDot > 500) {
        lastDot = millis();
        dots = (dots + 1) % 4;
        tft.fillRect(CENTER_X - 100, CENTER_Y - 30, 200, 70, COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.setCursor(CENTER_X - 95, CENTER_Y - 25);
        tft.print("Waiting");
        for (int i = 0; i < dots; i++) tft.print(".");
        tft.setTextSize(1);
        tft.setCursor(CENTER_X - 60, CENTER_Y + 10);
        tft.print("or press both");
        tft.setCursor(CENTER_X - 55, CENTER_Y + 22);
        tft.print("buttons to set");
      }
    }
    return;
  }

  // Time is valid
  if (!timeValid) {
    timeValid = true;
    tft.fillScreen(COLOR_BG);
    drawClockFace();
    lastSec = -1;
    lastMin = -1;
    lastHour = -1;
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
  tft.fillCircle(CENTER_X, CENTER_Y, 5, COLOR_SECOND);

  // Store previous angles
  prevSecAngle = secAngle;
  prevMinAngle = minAngle;
  prevHourAngle = hourAngle;

  // Update digital time display - only redraw changed parts to avoid flashing
  if (lastHour == -1) {
    drawDigitalColons();  // Draw colons once on first run
  }
  if (hour != lastHour || lastHour == -1) {
    drawDigitalHours(timeinfo.tm_hour);
  }
  if (min != lastMin || lastMin == -1) {
    drawDigitalMinutes(timeinfo.tm_min);
  }
  drawDigitalSeconds(timeinfo.tm_sec);  // Seconds always change

  // Update date display (top) - once per minute
  if (min != lastMin || lastMin == -1) {
    drawDateDisplay();
  }

  // Update sensor display - once per minute or when data first arrives
  static bool sensorDisplayed = false;
  if ((min != lastMin && hasSensorData) || (hasSensorData && !sensorDisplayed)) {
    drawSensorData();
    sensorDisplayed = hasSensorData;
  }

  lastSec = sec;
  lastMin = min;
  lastHour = hour;
}

void drawDateDisplay() {
  struct tm timeinfo;
  if (!getMeshTime(&timeinfo) && !getLocalTime(&timeinfo)) return;

  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  // Date at top of screen - just "Mon 23" format
  tft.fillRect(CENTER_X - 45, 8, 90, 20, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 42, 10);
  tft.printf("%s %d", months[timeinfo.tm_mon], timeinfo.tm_mday);
}

void drawSensorData() {
  // Temperature on left side (outside clock face) - "23C" format
  tft.fillRect(2, CENTER_Y - 8, 48, 18, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEMP, COLOR_BG);
  tft.setCursor(5, CENTER_Y - 6);
  tft.print(meshTemp + "C");

  // Humidity on right side (outside clock face) - "45%" format, same size
  tft.fillRect(SCREEN_SIZE - 50, CENTER_Y - 8, 48, 18, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_HUMID, COLOR_BG);
  tft.setCursor(SCREEN_SIZE - 48, CENTER_Y - 6);
  tft.print(meshHumid + "%");
}

// ============== DIGITAL TIME FUNCTIONS ==============

void drawDigitalHours(int hour) {
  tft.fillRect(CENTER_X - 48, 200, 24, 16, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 48, 200);
  tft.printf("%02d", hour);
}

void drawDigitalMinutes(int min) {
  tft.fillRect(CENTER_X - 12, 200, 24, 16, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 12, 200);
  tft.printf("%02d", min);
}

void drawDigitalSeconds(int sec) {
  tft.fillRect(CENTER_X + 24, 200, 24, 16, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X + 24, 200);
  tft.printf("%02d", sec);
}

void drawDigitalColons() {
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 24, 200);
  tft.print(":");
  tft.setCursor(CENTER_X + 12, 200);
  tft.print(":");
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

// ============== BUTTON INTERRUPT HANDLERS ==============

void IRAM_ATTR onLeftButtonPress() {
  unsigned long now = millis();
  if (now - btnLeftEventTime > BTN_DEBOUNCE_MS) {
    btnLeftEvent = true;
    btnLeftEventTime = now;
  }
}

void IRAM_ATTR onRightButtonPress() {
  unsigned long now = millis();
  if (now - btnRightEventTime > BTN_DEBOUNCE_MS) {
    btnRightEvent = true;
    btnRightEventTime = now;
  }
}

// ============== BUTTON HANDLING ==============

void handleButtons() {
  unsigned long now = millis();

  // Read current button states (active LOW)
  bool leftNow = !digitalRead(BTN_LEFT);
  bool rightNow = !digitalRead(BTN_RIGHT);

  // Detect both buttons pressed together
  if (leftNow && rightNow) {
    // Clear any pending single-button events when both are pressed
    btnLeftEvent = false;
    btnRightEvent = false;
    btnLeftHeld = false;
    btnRightHeld = false;

    if (!bothPressed) {
      bothPressed = true;
      bothPressedStart = now;
    } else if (now - bothPressedStart > BTN_BOTH_TIMEOUT_MS) {
      // Both held long enough - toggle set time mode or advance field
      bothPressed = false;
      bothPressedStart = 0;

      if (clockMode == MODE_NORMAL) {
        enterSetTimeMode();
      } else if (clockMode == MODE_SET_HOUR) {
        clockMode = MODE_SET_MINUTE;
        redrawHour = true;
        redrawMinute = true;
        Serial.println("[CLOCK] Now setting minutes");
      } else if (clockMode == MODE_SET_MINUTE) {
        exitSetTimeMode();
      }
      // Wait for buttons to be released before processing more events
      while (!digitalRead(BTN_LEFT) == LOW || !digitalRead(BTN_RIGHT) == LOW) {
        delay(10);
      }
      delay(100);  // Extra debounce after release
      btnLeftEvent = false;
      btnRightEvent = false;
      return;
    }
    return;  // Don't process individual buttons while both pressed
  }

  // If we were in both-pressed state but one was released, reset and wait
  if (bothPressed) {
    bothPressed = false;
    bothPressedStart = 0;
    // Clear events to avoid triggering single press action
    btnLeftEvent = false;
    btnRightEvent = false;
    btnLeftHeld = false;
    btnRightHeld = false;
    return;
  }

  // Process left button interrupt event (only if not recently in both-pressed mode)
  if (btnLeftEvent && !rightNow) {
    btnLeftEvent = false;
    btnLeftPressTime = now;
    btnLastRepeat = now;
    btnLeftHeld = true;

    if (clockMode == MODE_NORMAL) {
      // Previous screen
      currentScreen = (ScreenMode)((currentScreen + SCREEN_COUNT - 1) % SCREEN_COUNT);
      screenChanged = true;
      Serial.printf("[CLOCK] Screen: %d\n", currentScreen);
    } else if (clockMode == MODE_SET_HOUR) {
      setHour = (setHour + 23) % 24;  // Decrement hour
      redrawHour = true;
    } else if (clockMode == MODE_SET_MINUTE) {
      setMinute = (setMinute + 59) % 60;  // Decrement minute
      redrawMinute = true;
    }
  } else if (btnLeftEvent && rightNow) {
    // Other button is pressed, ignore this event (likely both-press attempt)
    btnLeftEvent = false;
  }

  // Process right button interrupt event (only if not recently in both-pressed mode)
  if (btnRightEvent && !leftNow) {
    btnRightEvent = false;
    btnRightPressTime = now;
    btnLastRepeat = now;
    btnRightHeld = true;

    if (clockMode == MODE_NORMAL) {
      // Next screen
      currentScreen = (ScreenMode)((currentScreen + 1) % SCREEN_COUNT);
      screenChanged = true;
      Serial.printf("[CLOCK] Screen: %d\n", currentScreen);
    } else if (clockMode == MODE_SET_HOUR) {
      setHour = (setHour + 1) % 24;  // Increment hour
      redrawHour = true;
    } else if (clockMode == MODE_SET_MINUTE) {
      setMinute = (setMinute + 1) % 60;  // Increment minute
      redrawMinute = true;
    }
  } else if (btnRightEvent && leftNow) {
    // Other button is pressed, ignore this event (likely both-press attempt)
    btnRightEvent = false;
  }

  // Handle button hold for auto-repeat in set time mode (only single button)
  if (leftNow && !rightNow && btnLeftHeld && clockMode != MODE_NORMAL) {
    if (now - btnLeftPressTime > BTN_LONG_PRESS_MS &&
        now - btnLastRepeat > BTN_REPEAT_MS) {
      btnLastRepeat = now;
      if (clockMode == MODE_SET_HOUR) {
        setHour = (setHour + 23) % 24;
        redrawHour = true;
      } else if (clockMode == MODE_SET_MINUTE) {
        setMinute = (setMinute + 59) % 60;
        redrawMinute = true;
      }
    }
  } else if (!leftNow) {
    btnLeftHeld = false;
  }

  if (rightNow && !leftNow && btnRightHeld && clockMode != MODE_NORMAL) {
    if (now - btnRightPressTime > BTN_LONG_PRESS_MS &&
        now - btnLastRepeat > BTN_REPEAT_MS) {
      btnLastRepeat = now;
      if (clockMode == MODE_SET_HOUR) {
        setHour = (setHour + 1) % 24;
        redrawHour = true;
      } else if (clockMode == MODE_SET_MINUTE) {
        setMinute = (setMinute + 1) % 60;
        redrawMinute = true;
      }
    }
  } else if (!rightNow) {
    btnRightHeld = false;
  }
}

// ============== SET TIME MODE ==============

void enterSetTimeMode() {
  if (clockMode != MODE_NORMAL) return;

  // Get current time as starting point
  struct tm timeinfo;
  if (getMeshTime(&timeinfo) || getLocalTime(&timeinfo)) {
    setHour = timeinfo.tm_hour;
    setMinute = timeinfo.tm_min;
  } else {
    setHour = 12;
    setMinute = 0;
  }

  clockMode = MODE_SET_HOUR;
  redrawHour = true;
  redrawMinute = true;
  lastSetHour = -1;
  lastSetMinute = -1;
  Serial.println("[CLOCK] Entering set time mode");

  tft.fillScreen(COLOR_BG);
  drawSetTimeScreen();

  // Draw initial time display
  tft.setTextSize(4);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 12, CENTER_Y - 15);
  tft.print(":");

  // Draw initial indicator
  tft.setTextSize(1);
  tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
  tft.setCursor(CENTER_X - 45, CENTER_Y + 32);
  tft.print("Setting HOUR");
}

void exitSetTimeMode() {
  if (clockMode == MODE_NORMAL) return;

  // Apply the set time
  unsigned long baseDate = 1734912000;  // Dec 23, 2024 00:00:00 UTC
  unsigned long unixTime = baseDate + (setHour * 3600) + (setMinute * 60) - GMT_OFFSET_SEC;
  setMeshTime(unixTime);

  Serial.printf("[CLOCK] Time set to %02d:%02d\n", setHour, setMinute);

  clockMode = MODE_NORMAL;
  timeValid = true;
  screenChanged = true;
  startupTimeoutChecked = true;  // Don't trigger auto set-time again

  // Redraw clock screen
  tft.fillScreen(COLOR_BG);
  drawClockFace();
  lastSec = -1;
  lastMin = -1;
  lastHour = -1;
}

void drawSetTimeScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
  tft.setCursor(CENTER_X - 55, 30);
  tft.print("SET TIME");

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 55, 55);
  tft.print("L/R: adjust value");
  tft.setCursor(CENTER_X - 55, 68);
  tft.print("Both: next/save");
}

void drawSetTimeHour() {
  // Clear hour area only
  tft.fillRect(CENTER_X - 62, CENTER_Y - 17, 50, 35, COLOR_BG);

  tft.setTextSize(4);
  if (clockMode == MODE_SET_HOUR && setTimeBlinkState) {
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
  } else if (clockMode == MODE_SET_HOUR && !setTimeBlinkState) {
    tft.setTextColor(COLOR_BG, COLOR_BG);  // Hide during blink off
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }
  tft.setCursor(CENTER_X - 60, CENTER_Y - 15);
  tft.printf("%02d", setHour);
}

void drawSetTimeMinute() {
  // Clear minute area only
  tft.fillRect(CENTER_X + 6, CENTER_Y - 17, 50, 35, COLOR_BG);

  tft.setTextSize(4);
  if (clockMode == MODE_SET_MINUTE && setTimeBlinkState) {
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
  } else if (clockMode == MODE_SET_MINUTE && !setTimeBlinkState) {
    tft.setTextColor(COLOR_BG, COLOR_BG);  // Hide during blink off
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }
  tft.setCursor(CENTER_X + 8, CENTER_Y - 15);
  tft.printf("%02d", setMinute);
}

void updateSetTimeScreen() {
  static ClockMode lastMode = MODE_NORMAL;

  // Handle blink timing - only affects the currently edited field
  bool needBlink = false;
  if (millis() - lastSetTimeBlink > 500) {
    lastSetTimeBlink = millis();
    setTimeBlinkState = !setTimeBlinkState;
    needBlink = true;
  }

  // Check if mode changed (hour -> minute)
  if (lastMode != clockMode) {
    lastMode = clockMode;
    // Redraw indicator
    tft.fillRect(CENTER_X - 60, CENTER_Y + 30, 120, 15, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
    tft.setCursor(CENTER_X - 45, CENTER_Y + 32);
    if (clockMode == MODE_SET_HOUR) {
      tft.print("Setting HOUR");
    } else {
      tft.print("Setting MINUTE");
    }
    // Force redraw of both fields when mode changes
    redrawHour = true;
    redrawMinute = true;
  }

  // Draw colon once if needed
  static bool colonDrawn = false;
  if (!colonDrawn) {
    tft.setTextSize(4);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(CENTER_X - 12, CENTER_Y - 15);
    tft.print(":");
    colonDrawn = true;
  }

  // Only redraw hour if it changed or needs blink update
  if (redrawHour || (needBlink && clockMode == MODE_SET_HOUR)) {
    redrawHour = false;
    drawSetTimeHour();
  }

  // Only redraw minute if it changed or needs blink update
  if (redrawMinute || (needBlink && clockMode == MODE_SET_MINUTE)) {
    redrawMinute = false;
    drawSetTimeMinute();
  }
}

// ============== SENSOR SCREEN ==============

void drawArc(int cx, int cy, int r, int thickness, float startAngle, float endAngle, uint16_t color) {
  // Draw arc using line segments
  float step = 2.0;  // Degrees per segment
  for (float angle = startAngle; angle <= endAngle; angle += step) {
    float rad = angle * PI / 180.0;
    for (int t = 0; t < thickness; t++) {
      int rr = r - t;
      int x = cx + cos(rad) * rr;
      int y = cy + sin(rad) * rr;
      tft.drawPixel(x, y, color);
    }
  }
}

void drawSensorScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 45, 15);
  tft.print("SENSORS");

  // Draw background arcs
  // Temperature arc: left side (200° to 340°, which is from bottom-left around to top)
  drawArc(CENTER_X, CENTER_Y + 10, 90, 8, 200, 340, COLOR_ARC_BG);

  // Humidity arc: right side (20° to 160°, which is from top around to bottom-right)
  drawArc(CENTER_X, CENTER_Y + 10, 90, 8, 20, 160, COLOR_ARC_BG);

  // Labels
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEMP, COLOR_BG);
  tft.setCursor(20, CENTER_Y + 60);
  tft.print("TEMP");

  tft.setTextColor(COLOR_HUMID, COLOR_BG);
  tft.setCursor(SCREEN_SIZE - 50, CENTER_Y + 60);
  tft.print("HUMID");
}

void updateSensorScreen() {
  static String lastTemp = "";
  static String lastHumid = "";
  static bool firstDraw = true;

  if (screenChanged || firstDraw) {
    screenChanged = false;
    firstDraw = false;
    drawSensorScreen();
    lastTemp = "";
    lastHumid = "";
  }

  // Update temperature display if changed
  if (meshTemp != lastTemp) {
    lastTemp = meshTemp;

    // Parse temperature value for arc
    float tempVal = meshTemp.toFloat();
    if (meshTemp == "--") tempVal = 0;

    // Map temperature (0-40°C) to arc angle (200° to 340°)
    float tempAngle = 200 + (tempVal / 40.0) * 140;
    if (tempAngle > 340) tempAngle = 340;
    if (tempAngle < 200) tempAngle = 200;

    // Redraw temperature arc
    drawArc(CENTER_X, CENTER_Y + 10, 90, 8, 200, 340, COLOR_ARC_BG);
    drawArc(CENTER_X, CENTER_Y + 10, 90, 8, 200, tempAngle, COLOR_ARC_TEMP);

    // Temperature value
    tft.fillRect(10, CENTER_Y - 15, 60, 35, COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_TEMP, COLOR_BG);
    tft.setCursor(15, CENTER_Y - 10);
    tft.print(meshTemp);
    tft.setTextSize(1);
    tft.setCursor(15, CENTER_Y + 18);
    tft.print("C");
  }

  // Update humidity display if changed
  if (meshHumid != lastHumid) {
    lastHumid = meshHumid;

    // Parse humidity value for arc
    float humidVal = meshHumid.toFloat();
    if (meshHumid == "--") humidVal = 0;

    // Map humidity (0-100%) to arc angle (20° to 160°)
    float humidAngle = 20 + (humidVal / 100.0) * 140;
    if (humidAngle > 160) humidAngle = 160;
    if (humidAngle < 20) humidAngle = 20;

    // Redraw humidity arc
    drawArc(CENTER_X, CENTER_Y + 10, 90, 8, 20, 160, COLOR_ARC_BG);
    drawArc(CENTER_X, CENTER_Y + 10, 90, 8, 20, humidAngle, COLOR_ARC_HUMID);

    // Humidity value
    tft.fillRect(SCREEN_SIZE - 70, CENTER_Y - 15, 60, 35, COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_HUMID, COLOR_BG);
    tft.setCursor(SCREEN_SIZE - 65, CENTER_Y - 10);
    tft.print(meshHumid);
    tft.setTextSize(1);
    tft.setCursor(SCREEN_SIZE - 65, CENTER_Y + 18);
    tft.print("%");
  }
}

void switchScreen(ScreenMode screen) {
  currentScreen = screen;
  screenChanged = true;

  tft.fillScreen(COLOR_BG);

  switch (screen) {
    case SCREEN_CLOCK:
      drawClockFace();
      lastSec = -1;
      lastMin = -1;
      lastHour = -1;
      break;
    case SCREEN_SENSOR:
      drawSensorScreen();
      break;
  }
}

/**
 * Clock Node
 *
 * Round TFT display showing time, date, and mesh sensor data.
 * Watches temperature and humidity from other mesh nodes.
 * Three-button interface with circular menu for screen navigation.
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
 *   - Mode button: GPIO4 (active LOW, internal pullup)
 *
 * Button Functions:
 *   - Mode button: Open/close circular menu, confirm selections
 *   - Left/Right buttons (menu open): Rotate selection around the ring
 *   - Left/Right buttons (screen-specific): Varies by screen
 *
 * Screens:
 *   - Clock: Analog clock with date and digital time
 *   - Sensors: Temperature and humidity from mesh with arc gauges
 *   - Settings: Set time (hour/minute)
 *   - Stopwatch: Start/stop, reset
 *   - Alarm: Toggle on/off (Left), set time (Right), visual feedback
 *   - Light: Mesh light sensor status
 *   - LED: Toggle mesh LED on/off
 *   - Motion: Mesh PIR motion sensor status
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

#ifndef BTN_MODE
#define BTN_MODE  4
#endif

#define BTN_DEBOUNCE_MS     50
#define BTN_LONG_PRESS_MS   500
#define BTN_REPEAT_MS       150

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
#define COLOR_MENU_BG   0x0000  // Menu background
#define COLOR_MENU_ICON 0x8410  // Gray for unselected icons
#define COLOR_MENU_SEL  0xFFE0  // Yellow for selected icon

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

// ============== MENU GEOMETRY ==============
#define MENU_RADIUS     95      // Distance from center to icon center
#define MENU_ICON_SIZE  16      // Icon size (radius for circular icons)
#define MENU_ITEM_COUNT 8       // Number of menu items

// ============== SCREEN/MODE ENUMS ==============
enum ScreenMode {
  SCREEN_CLOCK = 0,
  SCREEN_SENSOR,
  SCREEN_SETTINGS,
  SCREEN_STOPWATCH,
  SCREEN_ALARM,
  SCREEN_LIGHT,
  SCREEN_LED,
  SCREEN_MOTION,
  SCREEN_COUNT
};

enum ClockMode {
  MODE_NORMAL = 0,
  MODE_SET_HOUR = 1,
  MODE_SET_MINUTE = 2,
  MODE_SET_ALARM_HOUR = 3,
  MODE_SET_ALARM_MINUTE = 4
};

// Menu item indices (maps to screen modes)
enum MenuItem {
  MENU_CLOCK = 0,
  MENU_SENSORS,
  MENU_SETTINGS,
  MENU_STOPWATCH,
  MENU_ALARM,
  MENU_LIGHT,
  MENU_LED,
  MENU_MOTION
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

// Menu state
bool menuActive = false;
int menuSelection = 0;  // 0-7 for 8 menu items

// Set time mode variables
int setHour = 12;
int setMinute = 0;

// Button state (volatile for ISR access)
volatile bool btnLeftPressed = false;
volatile bool btnRightPressed = false;
volatile bool btnModePressed = false;
volatile unsigned long btnLeftPressTime = 0;
volatile unsigned long btnRightPressTime = 0;
volatile unsigned long btnModePressTime = 0;

// Button tracking for release detection and hold repeat
bool btnLeftWasPressed = false;
bool btnRightWasPressed = false;
bool btnModeWasPressed = false;
bool btnLeftActionDone = false;   // True if action was taken (via hold repeat)
bool btnRightActionDone = false;
unsigned long btnLastRepeat = 0;

// Set time redraw flags
bool redrawHour = true;
bool redrawMinute = true;
int lastSetHour = -1;
int lastSetMinute = -1;

// Startup timeout for auto set-time mode
unsigned long startupTime = 0;
bool startupTimeoutChecked = false;
#define STARTUP_TIMEOUT_MS 10000  // 10 seconds to wait for gateway time

// Stopwatch state
bool stopwatchRunning = false;
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchElapsed = 0;  // Accumulated time in ms

// Alarm state
bool alarmEnabled = false;
int alarmHour = 7;
int alarmMinute = 0;
bool alarmTriggered = false;

// Mesh device state
String meshLight = "--";
String meshLed = "0";
String meshMotion = "0";
unsigned long lastMotionTime = 0;

// ============== FORWARD DECLARATIONS ==============
void drawClockFace();
void drawHand(float angle, int length, uint16_t color, int width);
void eraseHand(float angle, int length, int width);
void updateClock();
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

// Menu functions
void drawMenu();
void hideMenu();
void getMenuIconPosition(int index, int* x, int* y);
void drawMenuIcon(int index, int cx, int cy, uint16_t color);
void drawClockIcon(int cx, int cy, uint16_t color);
void drawThermometerIcon(int cx, int cy, uint16_t color);
void drawGearIcon(int cx, int cy, uint16_t color);
void drawStopwatchIcon(int cx, int cy, uint16_t color);
void drawBellIcon(int cx, int cy, uint16_t color);
void drawLightbulbIcon(int cx, int cy, uint16_t color);
void drawLedIcon(int cx, int cy, uint16_t color);
void drawMotionIcon(int cx, int cy, uint16_t color);

// New screen functions
void drawStopwatchScreen();
void updateStopwatchScreen();
void drawAlarmScreen();
void updateAlarmScreen();
void drawLightScreen();
void updateLightScreen();
void drawLedScreen();
void updateLedScreen();
void drawMotionScreen();
void updateMotionScreen();
void checkAlarm();

// Button interrupt handlers
void IRAM_ATTR onLeftButtonPress();
void IRAM_ATTR onRightButtonPress();
void IRAM_ATTR onModeButtonPress();

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid
  esp_ota_mark_app_valid_cancel_rollback();

  // Initialize buttons with internal pullup and interrupts
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN_LEFT), onLeftButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_RIGHT), onRightButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN_MODE), onModeButtonPress, FALLING);

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
  swarm.watchState("humidity", [](const String& key, const String& value, const String& oldValue) {
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

      // If we're in set time mode or waiting, exit to clock display
      if (clockMode != MODE_NORMAL) {
        clockMode = MODE_NORMAL;
        timeValid = true;
        screenChanged = true;
        currentScreen = SCREEN_CLOCK;
        Serial.println("[CLOCK] Exiting set time mode - received gateway time");
      }
      startupTimeoutChecked = true;  // Don't auto-enter set time mode
    }
  });

  // Also watch zone-specific variants
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    if (key.startsWith("temp_") && meshTemp == "--") {
      meshTemp = value;
      hasSensorData = true;
    }
    if (key.startsWith("humidity_") && meshHumid == "--") {
      meshHumid = value;
      hasSensorData = true;
    }
    // Catch zone-specific motion keys (motion_zone1, etc.)
    if (key.startsWith("motion_")) {
      meshMotion = value;
      if (value == "1") {
        lastMotionTime = millis();
      }
      Serial.printf("[CLOCK] Motion (%s): %s\n", key.c_str(), value.c_str());
    }
  });

  // Watch for light sensor updates
  swarm.watchState("light", [](const String& key, const String& value, const String& oldValue) {
    meshLight = value;
    Serial.printf("[CLOCK] Light: %s\n", value.c_str());
  });

  // Watch for LED state updates
  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    meshLed = value;
    Serial.printf("[CLOCK] LED: %s\n", value.c_str());
  });

  // Watch for motion updates
  swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
    meshMotion = value;
    if (value == "1") {
      lastMotionTime = millis();
    }
    Serial.printf("[CLOCK] Motion: %s\n", value.c_str());
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

  // Check alarm
  checkAlarm();

  // Handle current screen/mode (skip updates if menu is open)
  if (menuActive) {
    // Menu is overlaid - don't update underlying screen
    return;
  }

  if (clockMode == MODE_SET_HOUR || clockMode == MODE_SET_MINUTE) {
    updateSetTimeScreen();
  } else if (clockMode == MODE_SET_ALARM_HOUR || clockMode == MODE_SET_ALARM_MINUTE) {
    // Alarm set mode - update alarm screen to show edit UI
    updateAlarmScreen();
  } else {
    switch (currentScreen) {
      case SCREEN_CLOCK:
        updateClock();
        break;
      case SCREEN_SENSOR:
        updateSensorScreen();
        break;
      case SCREEN_STOPWATCH:
        updateStopwatchScreen();
        break;
      case SCREEN_ALARM:
        updateAlarmScreen();
        break;
      case SCREEN_LIGHT:
        updateLightScreen();
        break;
      case SCREEN_LED:
        updateLedScreen();
        break;
      case SCREEN_MOTION:
        updateMotionScreen();
        break;
      case SCREEN_SETTINGS:
        // Settings screen is handled by set time mode
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
        tft.print("or press MODE");
        tft.setCursor(CENTER_X - 50, CENTER_Y + 22);
        tft.print("button to set");
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
  if (now - btnLeftPressTime > BTN_DEBOUNCE_MS) {
    btnLeftPressed = true;
    btnLeftPressTime = now;
  }
}

void IRAM_ATTR onRightButtonPress() {
  unsigned long now = millis();
  if (now - btnRightPressTime > BTN_DEBOUNCE_MS) {
    btnRightPressed = true;
    btnRightPressTime = now;
  }
}

void IRAM_ATTR onModeButtonPress() {
  unsigned long now = millis();
  if (now - btnModePressTime > BTN_DEBOUNCE_MS) {
    btnModePressed = true;
    btnModePressTime = now;
  }
}

// ============== BUTTON HANDLING ==============

// Helper to get menu index for current screen
int screenToMenuIndex(ScreenMode screen) {
  switch (screen) {
    case SCREEN_CLOCK:     return MENU_CLOCK;
    case SCREEN_SENSOR:    return MENU_SENSORS;
    case SCREEN_SETTINGS:  return MENU_SETTINGS;
    case SCREEN_STOPWATCH: return MENU_STOPWATCH;
    case SCREEN_ALARM:     return MENU_ALARM;
    case SCREEN_LIGHT:     return MENU_LIGHT;
    case SCREEN_LED:       return MENU_LED;
    case SCREEN_MOTION:    return MENU_MOTION;
    default:               return MENU_CLOCK;
  }
}

// Helper to get screen for menu index
ScreenMode menuIndexToScreen(int index) {
  switch (index) {
    case MENU_CLOCK:     return SCREEN_CLOCK;
    case MENU_SENSORS:   return SCREEN_SENSOR;
    case MENU_SETTINGS:  return SCREEN_SETTINGS;
    case MENU_STOPWATCH: return SCREEN_STOPWATCH;
    case MENU_ALARM:     return SCREEN_ALARM;
    case MENU_LIGHT:     return SCREEN_LIGHT;
    case MENU_LED:       return SCREEN_LED;
    case MENU_MOTION:    return SCREEN_MOTION;
    default:             return SCREEN_CLOCK;
  }
}

void handleButtons() {
  unsigned long now = millis();

  // Read current button states (active LOW)
  bool leftNow = !digitalRead(BTN_LEFT);
  bool rightNow = !digitalRead(BTN_RIGHT);
  bool modeNow = !digitalRead(BTN_MODE);

  // ===== MODE BUTTON - triggers on release =====
  if (btnModePressed && !btnModeWasPressed) {
    btnModeWasPressed = true;
  }
  if (btnModeWasPressed && !modeNow) {
    btnModeWasPressed = false;
    btnModePressed = false;

    if (menuActive) {
      // Menu is open - check if selection changed
      ScreenMode selectedScreen = menuIndexToScreen(menuSelection);
      if (selectedScreen == currentScreen) {
        // Same screen selected - just close menu
        hideMenu();
        Serial.println("[CLOCK] Menu closed (same screen)");
      } else {
        // Different screen selected - switch to it
        menuActive = false;
        switchScreen(selectedScreen);
        Serial.printf("[CLOCK] Switched to screen: %d\n", selectedScreen);
      }
    } else if (clockMode == MODE_SET_HOUR) {
      clockMode = MODE_SET_MINUTE;
      redrawHour = true;
      redrawMinute = true;
      Serial.println("[CLOCK] Now setting minutes");
    } else if (clockMode == MODE_SET_MINUTE) {
      exitSetTimeMode();
    } else if (clockMode == MODE_SET_ALARM_HOUR) {
      clockMode = MODE_SET_ALARM_MINUTE;
      Serial.println("[CLOCK] Now setting alarm minutes");
    } else if (clockMode == MODE_SET_ALARM_MINUTE) {
      // Exit alarm set mode - stay on alarm screen
      clockMode = MODE_NORMAL;
      screenChanged = true;  // Force redraw to show normal alarm UI
      Serial.printf("[CLOCK] Alarm set to %02d:%02d\n", alarmHour, alarmMinute);
    } else {
      // Normal mode - open menu (works for all screens including alarm)
      menuActive = true;
      menuSelection = screenToMenuIndex(currentScreen);
      drawMenu();
      Serial.println("[CLOCK] Menu opened");
    }
  }

  // ===== LEFT BUTTON =====
  if (btnLeftPressed && !btnLeftWasPressed) {
    btnLeftWasPressed = true;
    btnLeftActionDone = false;
    btnLastRepeat = now;
  }

  if (btnLeftWasPressed) {
    if (leftNow) {
      // Button still held - check for long press repeat
      bool canRepeat = (clockMode == MODE_SET_HOUR) || (clockMode == MODE_SET_MINUTE) ||
                       (clockMode == MODE_SET_ALARM_HOUR) || (clockMode == MODE_SET_ALARM_MINUTE);
      if (canRepeat &&
          now - btnLeftPressTime > BTN_LONG_PRESS_MS &&
          now - btnLastRepeat > BTN_REPEAT_MS) {
        btnLastRepeat = now;
        btnLeftActionDone = true;
        if (clockMode == MODE_SET_HOUR) {
          setHour = (setHour + 23) % 24;
          redrawHour = true;
        } else if (clockMode == MODE_SET_MINUTE) {
          setMinute = (setMinute + 59) % 60;
          redrawMinute = true;
        } else if (clockMode == MODE_SET_ALARM_HOUR) {
          alarmHour = (alarmHour + 23) % 24;
        } else if (clockMode == MODE_SET_ALARM_MINUTE) {
          alarmMinute = (alarmMinute + 59) % 60;
        }
      }
    } else {
      // Button released
      if (!btnLeftActionDone) {
        if (menuActive) {
          // Rotate menu selection counter-clockwise
          menuSelection = (menuSelection + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
          drawMenu();
          Serial.printf("[CLOCK] Menu selection: %d\n", menuSelection);
        } else if (clockMode == MODE_SET_HOUR) {
          setHour = (setHour + 23) % 24;
          redrawHour = true;
        } else if (clockMode == MODE_SET_MINUTE) {
          setMinute = (setMinute + 59) % 60;
          redrawMinute = true;
        } else if (clockMode == MODE_SET_ALARM_HOUR) {
          alarmHour = (alarmHour + 23) % 24;
        } else if (clockMode == MODE_SET_ALARM_MINUTE) {
          alarmMinute = (alarmMinute + 59) % 60;
        } else if (currentScreen == SCREEN_STOPWATCH) {
          // Start/Stop stopwatch
          if (stopwatchRunning) {
            stopwatchElapsed += millis() - stopwatchStartTime;
            stopwatchRunning = false;
          } else {
            stopwatchStartTime = millis();
            stopwatchRunning = true;
          }
        } else if (currentScreen == SCREEN_ALARM) {
          // Toggle alarm on/off
          alarmEnabled = !alarmEnabled;
          Serial.printf("[CLOCK] Alarm: %s\n", alarmEnabled ? "ON" : "OFF");
        } else if (currentScreen == SCREEN_LED) {
          // Toggle LED via mesh
          String newState = (meshLed == "1") ? "0" : "1";
          swarm.setState("led", newState);
          Serial.printf("[CLOCK] Set LED: %s\n", newState.c_str());
        }
      }
      btnLeftWasPressed = false;
      btnLeftPressed = false;
      btnLeftActionDone = false;
    }
  }

  // ===== RIGHT BUTTON =====
  if (btnRightPressed && !btnRightWasPressed) {
    btnRightWasPressed = true;
    btnRightActionDone = false;
    btnLastRepeat = now;
  }

  if (btnRightWasPressed) {
    if (rightNow) {
      // Button still held - check for long press repeat
      bool canRepeat = (clockMode == MODE_SET_HOUR) || (clockMode == MODE_SET_MINUTE) ||
                       (clockMode == MODE_SET_ALARM_HOUR) || (clockMode == MODE_SET_ALARM_MINUTE);
      if (canRepeat &&
          now - btnRightPressTime > BTN_LONG_PRESS_MS &&
          now - btnLastRepeat > BTN_REPEAT_MS) {
        btnLastRepeat = now;
        btnRightActionDone = true;
        if (clockMode == MODE_SET_HOUR) {
          setHour = (setHour + 1) % 24;
          redrawHour = true;
        } else if (clockMode == MODE_SET_MINUTE) {
          setMinute = (setMinute + 1) % 60;
          redrawMinute = true;
        } else if (clockMode == MODE_SET_ALARM_HOUR) {
          alarmHour = (alarmHour + 1) % 24;
        } else if (clockMode == MODE_SET_ALARM_MINUTE) {
          alarmMinute = (alarmMinute + 1) % 60;
        }
      }
    } else {
      // Button released
      if (!btnRightActionDone) {
        if (menuActive) {
          // Rotate menu selection clockwise
          menuSelection = (menuSelection + 1) % MENU_ITEM_COUNT;
          drawMenu();
          Serial.printf("[CLOCK] Menu selection: %d\n", menuSelection);
        } else if (clockMode == MODE_SET_HOUR) {
          setHour = (setHour + 1) % 24;
          redrawHour = true;
        } else if (clockMode == MODE_SET_MINUTE) {
          setMinute = (setMinute + 1) % 60;
          redrawMinute = true;
        } else if (clockMode == MODE_SET_ALARM_HOUR) {
          alarmHour = (alarmHour + 1) % 24;
        } else if (clockMode == MODE_SET_ALARM_MINUTE) {
          alarmMinute = (alarmMinute + 1) % 60;
        } else if (currentScreen == SCREEN_STOPWATCH) {
          // Reset stopwatch (when stopped)
          if (!stopwatchRunning) {
            stopwatchElapsed = 0;
          }
        } else if (currentScreen == SCREEN_ALARM) {
          // Enter alarm set mode
          clockMode = MODE_SET_ALARM_HOUR;
          Serial.println("[CLOCK] Entering alarm set mode");
        } else if (currentScreen == SCREEN_LED) {
          // Toggle LED via mesh
          String newState = (meshLed == "1") ? "0" : "1";
          swarm.setState("led", newState);
          Serial.printf("[CLOCK] Set LED: %s\n", newState.c_str());
        }
      }
      btnRightWasPressed = false;
      btnRightPressed = false;
      btnRightActionDone = false;
    }
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
  tft.print("MODE: next/save");
}

void drawSetTimeHour() {
  // Clear hour area including underline space
  tft.fillRect(CENTER_X - 62, CENTER_Y - 17, 50, 40, COLOR_BG);

  tft.setTextSize(4);
  if (clockMode == MODE_SET_HOUR) {
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }
  tft.setCursor(CENTER_X - 60, CENTER_Y - 15);
  tft.printf("%02d", setHour);

  // Draw underline if this field is selected (accessibility for color blind)
  if (clockMode == MODE_SET_HOUR) {
    tft.fillRect(CENTER_X - 60, CENTER_Y + 18, 48, 3, COLOR_SET_TIME);
  }
}

void drawSetTimeMinute() {
  // Clear minute area including underline space
  tft.fillRect(CENTER_X + 6, CENTER_Y - 17, 50, 40, COLOR_BG);

  tft.setTextSize(4);
  if (clockMode == MODE_SET_MINUTE) {
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
  } else {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
  }
  tft.setCursor(CENTER_X + 8, CENTER_Y - 15);
  tft.printf("%02d", setMinute);

  // Draw underline if this field is selected (accessibility for color blind)
  if (clockMode == MODE_SET_MINUTE) {
    tft.fillRect(CENTER_X + 8, CENTER_Y + 18, 48, 3, COLOR_SET_TIME);
  }
}

void updateSetTimeScreen() {
  static ClockMode lastMode = MODE_NORMAL;

  // Check if mode changed (hour -> minute)
  if (lastMode != clockMode) {
    lastMode = clockMode;
    // Redraw indicator
    tft.fillRect(CENTER_X - 60, CENTER_Y + 35, 120, 15, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
    tft.setCursor(CENTER_X - 45, CENTER_Y + 37);
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

  // Only redraw hour if it changed
  if (redrawHour) {
    redrawHour = false;
    drawSetTimeHour();
  }

  // Only redraw minute if it changed
  if (redrawMinute) {
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

  // Draw background arcs (thermometer style bars on sides)
  // Temperature arc: left side (200° to 340°)
  drawArc(CENTER_X, CENTER_Y + 20, 95, 12, 200, 340, COLOR_ARC_BG);

  // Humidity arc: right side (160° to 20°) - draws same arc, fills opposite direction
  drawArc(CENTER_X, CENTER_Y + 20, 95, 12, 20, 160, COLOR_ARC_BG);

  // Labels at bottom of arcs
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEMP, COLOR_BG);
  tft.setCursor(15, 210);
  tft.print("TEMP");

  tft.setTextColor(COLOR_HUMID, COLOR_BG);
  tft.setCursor(SCREEN_SIZE - 45, 210);
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

    // Map temperature (0-40°C) to arc angle (200° up to 340°) - bottom to top
    // At 0°C: starts at 200° (bottom-left), at 40°C: fills to 340° (top-left)
    float tempAngle = 200 + (tempVal / 40.0) * 140;
    if (tempAngle > 340) tempAngle = 340;
    if (tempAngle < 200) tempAngle = 200;

    // Redraw temperature arc (left side) - fill from 200° up to tempAngle
    drawArc(CENTER_X, CENTER_Y + 20, 95, 12, 200, 340, COLOR_ARC_BG);
    drawArc(CENTER_X, CENTER_Y + 20, 95, 12, 200, tempAngle, COLOR_ARC_TEMP);

    // Temperature value - centered, above humidity
    tft.fillRect(CENTER_X - 50, CENTER_Y - 25, 100, 40, COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_TEMP, COLOR_BG);
    // Center the temperature value
    int tempWidth = meshTemp.length() * 18 + 18;  // 18px per char + "C"
    int tempX = CENTER_X - tempWidth / 2;
    tft.setCursor(tempX, CENTER_Y - 20);
    tft.print(meshTemp);
    tft.setTextSize(2);
    tft.print("C");
  }

  // Update humidity display if changed
  if (meshHumid != lastHumid) {
    lastHumid = meshHumid;

    // Parse humidity value for arc
    float humidVal = meshHumid.toFloat();
    if (meshHumid == "--") humidVal = 0;

    // Map humidity (0-100%) to arc angle (160° down to 20°) - bottom to top
    // At 0%: starts at 160° (bottom-right), at 100%: fills to 20° (top-right)
    float humidAngle = 160 - (humidVal / 100.0) * 140;
    if (humidAngle < 20) humidAngle = 20;
    if (humidAngle > 160) humidAngle = 160;

    // Redraw humidity arc (right side) - fill from 160° up to humidAngle
    drawArc(CENTER_X, CENTER_Y + 20, 95, 12, 20, 160, COLOR_ARC_BG);
    drawArc(CENTER_X, CENTER_Y + 20, 95, 12, humidAngle, 160, COLOR_ARC_HUMID);

    // Humidity value - centered, below temperature
    tft.fillRect(CENTER_X - 50, CENTER_Y + 20, 100, 40, COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_HUMID, COLOR_BG);
    // Center the humidity value
    int humidWidth = meshHumid.length() * 18 + 18;  // 18px per char + "%"
    int humidX = CENTER_X - humidWidth / 2;
    tft.setCursor(humidX, CENTER_Y + 25);
    tft.print(meshHumid);
    tft.setTextSize(2);
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
    case SCREEN_SETTINGS:
      enterSetTimeMode();
      break;
    case SCREEN_STOPWATCH:
      drawStopwatchScreen();
      break;
    case SCREEN_ALARM:
      drawAlarmScreen();
      break;
    case SCREEN_LIGHT:
      drawLightScreen();
      break;
    case SCREEN_LED:
      drawLedScreen();
      break;
    case SCREEN_MOTION:
      drawMotionScreen();
      break;
  }
}

// ============== MENU SYSTEM ==============

void getMenuIconPosition(int index, int* x, int* y) {
  // 8 items, starting at top (270°), going clockwise
  // Angles: 270, 315, 0, 45, 90, 135, 180, 225
  float angle = (270.0 + index * 45.0) * PI / 180.0;
  *x = CENTER_X + cos(angle) * MENU_RADIUS;
  *y = CENTER_Y + sin(angle) * MENU_RADIUS;
}

// Draw clock icon - circle with two hands
void drawClockIcon(int cx, int cy, uint16_t color) {
  tft.drawCircle(cx, cy, 8, color);
  // Hour hand (short, pointing to 10)
  float hourAngle = (300 - 90) * PI / 180;
  tft.drawLine(cx, cy, cx + cos(hourAngle) * 4, cy + sin(hourAngle) * 4, color);
  // Minute hand (long, pointing to 2)
  float minAngle = (60 - 90) * PI / 180;
  tft.drawLine(cx, cy, cx + cos(minAngle) * 6, cy + sin(minAngle) * 6, color);
}

// Draw thermometer icon - vertical line with bulb at bottom
void drawThermometerIcon(int cx, int cy, uint16_t color) {
  // Tube
  tft.drawLine(cx, cy - 7, cx, cy + 3, color);
  tft.drawLine(cx - 1, cy - 7, cx - 1, cy + 3, color);
  tft.drawLine(cx + 1, cy - 7, cx + 1, cy + 3, color);
  // Bulb at bottom
  tft.fillCircle(cx, cy + 5, 3, color);
  // Top cap
  tft.drawPixel(cx - 1, cy - 8, color);
  tft.drawPixel(cx, cy - 8, color);
  tft.drawPixel(cx + 1, cy - 8, color);
}

// Draw gear icon - circle with teeth
void drawGearIcon(int cx, int cy, uint16_t color) {
  tft.drawCircle(cx, cy, 4, color);
  // Draw 6 teeth around the gear
  for (int i = 0; i < 6; i++) {
    float angle = i * 60 * PI / 180;
    int x1 = cx + cos(angle) * 5;
    int y1 = cy + sin(angle) * 5;
    int x2 = cx + cos(angle) * 8;
    int y2 = cy + sin(angle) * 8;
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

// Draw stopwatch icon - circle with button on top and single hand
void drawStopwatchIcon(int cx, int cy, uint16_t color) {
  tft.drawCircle(cx, cy + 1, 7, color);
  // Button on top
  tft.fillRect(cx - 1, cy - 9, 3, 3, color);
  // Single hand pointing up
  tft.drawLine(cx, cy + 1, cx, cy - 4, color);
}

// Draw bell icon - bell shape
void drawBellIcon(int cx, int cy, uint16_t color) {
  // Bell dome (arc)
  tft.drawCircle(cx, cy - 2, 6, color);
  tft.fillRect(cx - 6, cy - 2, 12, 6, COLOR_BG);  // Cut bottom half
  // Bell body
  tft.drawLine(cx - 6, cy + 2, cx - 4, cy - 4, color);
  tft.drawLine(cx + 6, cy + 2, cx + 4, cy - 4, color);
  tft.drawLine(cx - 6, cy + 2, cx + 6, cy + 2, color);
  // Clapper
  tft.fillCircle(cx, cy + 5, 2, color);
}

// Draw lightbulb icon - bulb outline with rays
void drawLightbulbIcon(int cx, int cy, uint16_t color) {
  // Bulb shape
  tft.drawCircle(cx, cy - 2, 5, color);
  // Base
  tft.drawLine(cx - 3, cy + 3, cx - 3, cy + 6, color);
  tft.drawLine(cx + 3, cy + 3, cx + 3, cy + 6, color);
  tft.drawLine(cx - 3, cy + 6, cx + 3, cy + 6, color);
  // Rays
  tft.drawLine(cx - 8, cy - 2, cx - 6, cy - 2, color);
  tft.drawLine(cx + 6, cy - 2, cx + 8, cy - 2, color);
  tft.drawLine(cx, cy - 9, cx, cy - 7, color);
}

// Draw LED icon - small circle with radiating lines
void drawLedIcon(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy, 4, color);
  // Radiating lines
  for (int i = 0; i < 8; i++) {
    float angle = i * 45 * PI / 180;
    int x1 = cx + cos(angle) * 5;
    int y1 = cy + sin(angle) * 5;
    int x2 = cx + cos(angle) * 8;
    int y2 = cy + sin(angle) * 8;
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

// Draw motion icon - 3 curved lines (wifi-style waves)
void drawMotionIcon(int cx, int cy, uint16_t color) {
  // Three arcs representing motion waves
  for (int r = 3; r <= 7; r += 2) {
    // Draw arc from -45 to 45 degrees (facing right)
    for (float a = -45; a <= 45; a += 10) {
      float rad = a * PI / 180;
      int x = cx + cos(rad) * r;
      int y = cy + sin(rad) * r;
      tft.drawPixel(x, y, color);
    }
  }
  // Motion source dot
  tft.fillCircle(cx - 4, cy, 2, color);
}

// Draw a specific menu icon at position
void drawMenuIcon(int index, int cx, int cy, uint16_t color) {
  switch (index) {
    case MENU_CLOCK:      drawClockIcon(cx, cy, color); break;
    case MENU_SENSORS:    drawThermometerIcon(cx, cy, color); break;
    case MENU_SETTINGS:   drawGearIcon(cx, cy, color); break;
    case MENU_STOPWATCH:  drawStopwatchIcon(cx, cy, color); break;
    case MENU_ALARM:      drawBellIcon(cx, cy, color); break;
    case MENU_LIGHT:      drawLightbulbIcon(cx, cy, color); break;
    case MENU_LED:        drawLedIcon(cx, cy, color); break;
    case MENU_MOTION:     drawMotionIcon(cx, cy, color); break;
  }
}

void drawMenu() {
  // Clear the outer ring area where menu icons will be drawn
  // This clears date (top), digital time (bottom), and arc gauges (sides)
  // Clear top area (date)
  tft.fillRect(0, 0, SCREEN_SIZE, 35, COLOR_BG);
  // Clear bottom area (digital time)
  tft.fillRect(0, 190, SCREEN_SIZE, 50, COLOR_BG);
  // Clear left arc area
  tft.fillRect(0, 35, 30, 155, COLOR_BG);
  // Clear right arc area
  tft.fillRect(210, 35, 30, 155, COLOR_BG);

  // Draw all 8 menu icons around the ring
  for (int i = 0; i < MENU_ITEM_COUNT; i++) {
    int x, y;
    getMenuIconPosition(i, &x, &y);

    // Determine color based on selection
    uint16_t color;
    if (i == menuSelection) {
      // Selected item - draw highlight arc behind it
      float startAngle = 270.0 + i * 45.0 - 20.0;
      float endAngle = 270.0 + i * 45.0 + 20.0;
      drawArc(CENTER_X, CENTER_Y, MENU_RADIUS + 12, 6, startAngle, endAngle, COLOR_MENU_SEL);
      color = COLOR_MENU_SEL;
    } else {
      color = COLOR_MENU_ICON;
    }

    drawMenuIcon(i, x, y, color);
  }
}

void hideMenu() {
  // Redraw current screen to clear menu overlay
  menuActive = false;
  screenChanged = true;
}

// ============== STOPWATCH SCREEN ==============

void drawStopwatchScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 50, 20);
  tft.print("STOPWATCH");

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TICK, COLOR_BG);
  tft.setCursor(CENTER_X - 55, 200);
  tft.print("L:Start/Stop R:Reset");
}

void updateStopwatchScreen() {
  static unsigned long lastDisplay = 0;

  if (screenChanged) {
    screenChanged = false;
    drawStopwatchScreen();
    lastDisplay = 0;
  }

  // Calculate current elapsed time
  unsigned long elapsed = stopwatchElapsed;
  if (stopwatchRunning) {
    elapsed += millis() - stopwatchStartTime;
  }

  // Only update display every 100ms to avoid flicker
  if (millis() - lastDisplay < 100) return;
  lastDisplay = millis();

  // Format time: MM:SS.d
  int totalSecs = elapsed / 1000;
  int mins = totalSecs / 60;
  int secs = totalSecs % 60;
  int tenths = (elapsed % 1000) / 100;

  // Clear and draw time
  tft.fillRect(CENTER_X - 70, CENTER_Y - 20, 140, 50, COLOR_BG);
  tft.setTextSize(4);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 65, CENTER_Y - 15);
  tft.printf("%02d:%02d", mins, secs);

  // Tenths in smaller text
  tft.setTextSize(3);
  tft.setCursor(CENTER_X + 50, CENTER_Y - 10);
  tft.printf(".%d", tenths);

  // Running indicator
  tft.setTextSize(1);
  tft.fillRect(CENTER_X - 25, CENTER_Y + 40, 50, 15, COLOR_BG);
  tft.setCursor(CENTER_X - 20, CENTER_Y + 45);
  if (stopwatchRunning) {
    tft.setTextColor(COLOR_HUMID, COLOR_BG);  // Green for running
    tft.print("RUNNING");
  } else if (elapsed > 0) {
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);  // Yellow for paused
    tft.print("PAUSED");
  }
}

// ============== ALARM SCREEN ==============

void drawAlarmScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 35, 20);
  tft.print("ALARM");

  // Instructions - vary based on mode
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TICK, COLOR_BG);
  if (clockMode == MODE_SET_ALARM_HOUR || clockMode == MODE_SET_ALARM_MINUTE) {
    tft.setCursor(CENTER_X - 55, 200);
    tft.print("L/R: Adjust  MODE:Next");
  } else {
    tft.setCursor(CENTER_X - 55, 200);
    tft.print("L:On/Off  R:Set Time");
  }
}

void updateAlarmScreen() {
  static int lastDisplayHour = -1;
  static int lastDisplayMin = -1;
  static bool lastEnabled = false;
  static ClockMode lastClockMode = MODE_NORMAL;

  if (screenChanged) {
    screenChanged = false;
    drawAlarmScreen();
    lastDisplayHour = -1;
    lastDisplayMin = -1;
    lastEnabled = false;
    lastClockMode = MODE_NORMAL;
  }

  // Redraw instructions if mode changed
  if (clockMode != lastClockMode) {
    lastClockMode = clockMode;
    // Redraw instructions area
    tft.fillRect(CENTER_X - 70, 195, 140, 25, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TICK, COLOR_BG);
    if (clockMode == MODE_SET_ALARM_HOUR || clockMode == MODE_SET_ALARM_MINUTE) {
      tft.setCursor(CENTER_X - 55, 200);
      tft.print("L/R: Adjust  MODE:Next");
    } else {
      tft.setCursor(CENTER_X - 55, 200);
      tft.print("L:On/Off  R:Set Time");
    }
    // Force redraw of time display
    lastDisplayHour = -1;
    lastDisplayMin = -1;
  }

  // Only redraw if changed
  if (alarmHour != lastDisplayHour || alarmMinute != lastDisplayMin || alarmEnabled != lastEnabled) {
    lastDisplayHour = alarmHour;
    lastDisplayMin = alarmMinute;
    lastEnabled = alarmEnabled;

    // Clear time area
    tft.fillRect(CENTER_X - 65, CENTER_Y - 25, 130, 60, COLOR_BG);

    // Draw hour
    tft.setTextSize(4);
    if (clockMode == MODE_SET_ALARM_HOUR) {
      tft.setTextColor(COLOR_SET_TIME, COLOR_BG);  // Yellow when editing
    } else if (alarmEnabled) {
      tft.setTextColor(COLOR_HUMID, COLOR_BG);  // Green when enabled
    } else {
      tft.setTextColor(COLOR_TICK, COLOR_BG);  // Gray when disabled
    }
    tft.setCursor(CENTER_X - 55, CENTER_Y - 20);
    tft.printf("%02d", alarmHour);

    // Draw colon
    if (alarmEnabled) {
      tft.setTextColor(COLOR_HUMID, COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TICK, COLOR_BG);
    }
    tft.setCursor(CENTER_X - 7, CENTER_Y - 20);
    tft.print(":");

    // Draw minute
    if (clockMode == MODE_SET_ALARM_MINUTE) {
      tft.setTextColor(COLOR_SET_TIME, COLOR_BG);  // Yellow when editing
    } else if (alarmEnabled) {
      tft.setTextColor(COLOR_HUMID, COLOR_BG);  // Green when enabled
    } else {
      tft.setTextColor(COLOR_TICK, COLOR_BG);  // Gray when disabled
    }
    tft.setCursor(CENTER_X + 10, CENTER_Y - 20);
    tft.printf("%02d", alarmMinute);

    // Draw underlines when in set mode
    if (clockMode == MODE_SET_ALARM_HOUR) {
      tft.fillRect(CENTER_X - 55, CENTER_Y + 15, 48, 3, COLOR_SET_TIME);
    } else if (clockMode == MODE_SET_ALARM_MINUTE) {
      tft.fillRect(CENTER_X + 10, CENTER_Y + 15, 48, 3, COLOR_SET_TIME);
    }

    // Status indicator (only in normal mode)
    tft.fillRect(CENTER_X - 40, CENTER_Y + 40, 80, 25, COLOR_BG);
    if (clockMode == MODE_SET_ALARM_HOUR) {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
      tft.setCursor(CENTER_X - 38, CENTER_Y + 47);
      tft.print("Setting HOUR");
    } else if (clockMode == MODE_SET_ALARM_MINUTE) {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
      tft.setCursor(CENTER_X - 38, CENTER_Y + 47);
      tft.print("Setting MINUTE");
    } else {
      tft.setTextSize(2);
      tft.setCursor(CENTER_X - 15, CENTER_Y + 45);
      if (alarmEnabled) {
        tft.setTextColor(COLOR_HUMID, COLOR_BG);
        tft.print("ON");
      } else {
        tft.setTextColor(COLOR_TICK, COLOR_BG);
        tft.print("OFF");
      }
    }
  }
}

void checkAlarm() {
  if (!alarmEnabled || alarmTriggered) return;

  struct tm timeinfo;
  if (getMeshTime(&timeinfo) || getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute) {
      alarmTriggered = true;
      Serial.println("[CLOCK] ALARM!");
      // Could add buzzer or visual alert here
    }
  }

  // Reset trigger flag when time moves past alarm time
  if (alarmTriggered) {
    struct tm timeinfo;
    if (getMeshTime(&timeinfo) || getLocalTime(&timeinfo)) {
      if (timeinfo.tm_hour != alarmHour || timeinfo.tm_min != alarmMinute) {
        alarmTriggered = false;
      }
    }
  }
}

// ============== LIGHT SCREEN ==============

void drawLightScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 35, 20);
  tft.print("LIGHT");

  // Icon
  drawLightbulbIcon(CENTER_X, CENTER_Y - 40, COLOR_SET_TIME);
}

void updateLightScreen() {
  static String lastLight = "";

  if (screenChanged) {
    screenChanged = false;
    drawLightScreen();
    lastLight = "";
  }

  if (meshLight != lastLight) {
    lastLight = meshLight;

    // Clear and draw value
    tft.fillRect(CENTER_X - 50, CENTER_Y, 100, 40, COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
    tft.setCursor(CENTER_X - 40, CENTER_Y + 10);
    tft.print(meshLight);

    // Label
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TICK, COLOR_BG);
    tft.setCursor(CENTER_X - 30, CENTER_Y + 50);
    if (meshLight == "--") {
      tft.print("No sensor");
    } else {
      tft.print("Lux");
    }
  }
}

// ============== LED SCREEN ==============

void drawLedScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 20, 20);
  tft.print("LED");

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TICK, COLOR_BG);
  tft.setCursor(CENTER_X - 45, 200);
  tft.print("L/R: Toggle LED");
}

void updateLedScreen() {
  static String lastLed = "";

  if (screenChanged) {
    screenChanged = false;
    drawLedScreen();
    lastLed = "";
  }

  if (meshLed != lastLed) {
    lastLed = meshLed;

    // Clear center area
    tft.fillRect(CENTER_X - 50, CENTER_Y - 40, 100, 80, COLOR_BG);

    // Draw LED icon with state color
    if (meshLed == "1") {
      drawLedIcon(CENTER_X, CENTER_Y - 10, COLOR_SET_TIME);
      tft.setTextSize(2);
      tft.setTextColor(COLOR_SET_TIME, COLOR_BG);
      tft.setCursor(CENTER_X - 15, CENTER_Y + 20);
      tft.print("ON");
    } else {
      drawLedIcon(CENTER_X, CENTER_Y - 10, COLOR_TICK);
      tft.setTextSize(2);
      tft.setTextColor(COLOR_TICK, COLOR_BG);
      tft.setCursor(CENTER_X - 20, CENTER_Y + 20);
      tft.print("OFF");
    }
  }
}

// ============== MOTION SCREEN ==============

void drawMotionScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(CENTER_X - 40, 20);
  tft.print("MOTION");
}

void updateMotionScreen() {
  static String lastMotion = "";
  static unsigned long lastUpdate = 0;

  if (screenChanged) {
    screenChanged = false;
    drawMotionScreen();
    lastMotion = "";
  }

  // Update every 500ms or when motion changes
  if (meshMotion != lastMotion || millis() - lastUpdate > 500) {
    lastMotion = meshMotion;
    lastUpdate = millis();

    // Clear center area
    tft.fillRect(CENTER_X - 60, CENTER_Y - 40, 120, 100, COLOR_BG);

    // Draw motion icon with state color
    if (meshMotion == "1") {
      drawMotionIcon(CENTER_X, CENTER_Y - 10, COLOR_SECOND);  // Red for motion
      tft.setTextSize(2);
      tft.setTextColor(COLOR_SECOND, COLOR_BG);
      tft.setCursor(CENTER_X - 40, CENTER_Y + 20);
      tft.print("DETECTED");
    } else {
      drawMotionIcon(CENTER_X, CENTER_Y - 10, COLOR_TICK);
      tft.setTextSize(2);
      tft.setTextColor(COLOR_TICK, COLOR_BG);
      tft.setCursor(CENTER_X - 35, CENTER_Y + 20);
      tft.print("No motion");
    }

    // Show time since last motion
    if (lastMotionTime > 0) {
      unsigned long elapsed = (millis() - lastMotionTime) / 1000;
      tft.setTextSize(1);
      tft.setTextColor(COLOR_TICK, COLOR_BG);
      tft.setCursor(CENTER_X - 45, CENTER_Y + 50);
      if (elapsed < 60) {
        tft.printf("Last: %lus ago", elapsed);
      } else {
        tft.printf("Last: %lum ago", elapsed / 60);
      }
    }
  }
}

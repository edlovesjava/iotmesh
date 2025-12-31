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
#include <Wire.h>
#include <Preferences.h>

// CST816T Touch Controller from SensorLib
#include "touch/TouchClassCST816.h"

// Board configuration (pins, colors, geometry, timing)
#include "BoardConfig.h"

// Extracted classes
#include "Battery.h"
#include "TimeSource.h"

// ============== GLOBALS ==============
MeshSwarm swarm;
TFT_eSPI tft = TFT_eSPI();
TouchClassCST816 touch;
Preferences preferences;
Battery battery;
TimeSource timeSource;

// Touch state
bool touchInitialized = false;
int16_t touchX = -1;
int16_t touchY = -1;
bool touchPressed = false;
unsigned long lastTouchTime = 0;
unsigned long screenTransitionTime = 0;  // When last screen change occurred

// Swipe gesture detection
int16_t touchStartX = -1;
int16_t touchStartY = -1;
bool touchActive = false;

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

// Previous hand positions for efficient redraw
float prevSecAngle = -999;
float prevMinAngle = -999;
float prevHourAngle = -999;

// Battery indicator state (for redraw optimization)
bool batteryIndicatorDirty = true;

// ============== SCREEN MODE ENUM (Phase 1.1) ==============
// From touch169_touch_navigation_spec.md Section 6
enum ScreenMode {
  SCREEN_CLOCK,               // Main clock face
  SCREEN_HUMIDITY,            // Humidity detail
  SCREEN_HUMIDITY_SETTINGS,   // Humidity zone settings
  SCREEN_TEMPERATURE,         // Temperature detail
  SCREEN_TEMP_SETTINGS,       // Temperature zone settings
  SCREEN_LIGHT,               // Light detail
  SCREEN_LIGHT_SETTINGS,      // Light zone settings
  SCREEN_MOTION_LED,          // Motion & LED control
  SCREEN_MOTION_LED_SETTINGS, // Motion/LED zone settings
  SCREEN_CALENDAR,            // Monthly calendar view
  SCREEN_DATE_SETTINGS,       // Set current date
  SCREEN_CLOCK_DETAILS,       // Clock details menu
  SCREEN_TIME_SETTINGS,       // Set time and timezone
  SCREEN_ALARM,               // Alarm settings
  SCREEN_STOPWATCH,           // Stopwatch
  SCREEN_NAV_MENU,            // Navigation menu overlay
  SCREEN_DEBUG                // Debug info
};

// Screen state
ScreenMode currentScreen = SCREEN_CLOCK;
ScreenMode previousScreen = SCREEN_CLOCK;  // For back navigation
bool screenChanged = true;
bool firstDraw = true;

// Power button state (long press to power off)
unsigned long pwrBtnPressTime = 0;
bool pwrBtnWasPressed = false;

// Boot button state (short press = back, long press = debug per spec)
unsigned long bootBtnPressTime = 0;
bool bootBtnWasPressed = false;
bool bootBtnShortPress = false;
bool bootBtnLongPress = false;

// Display sleep state
bool displayAsleep = false;
unsigned long lastActivityTime = 0;

// ============== FUNCTION DECLARATIONS ==============
void drawClockFace();
void drawHand(float angle, int length, uint16_t color, int width);
void eraseHand(float angle, int length, int width);
void updateClock();
void updateCorners();
void drawCornerLabels();
void checkPowerButton();
void checkBootButton();
void powerOff();
void drawBatteryIndicator();  // Uses Battery class
void drawDebugScreen();
void updateDebugScreen();
void displaySleep();
void displayWake();
void resetActivityTimer();
bool initTouch();
void handleTouch();
void testPreferences();

// Navigation functions (Phase 1.1)
void navigateTo(ScreenMode screen);
void navigateBack();
ScreenMode getParentScreen(ScreenMode screen);
const char* getScreenName(ScreenMode screen);
void renderCurrentScreen();
void processTouchZones();

// ============== SETUP ==============
void setup() {
  // CRITICAL: Latch power immediately to stay on when running from battery
  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, HIGH);

  // Setup power button input
  pinMode(PWR_BTN_PIN, INPUT);

  // Setup boot button (GPIO0) with internal pullup
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[TOUCH169] Starting...");

  // Initialize backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize I2C for touch controller (separate from display SPI)
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Serial.printf("[TOUCH169] I2C initialized on SDA=%d, SCL=%d\n", TOUCH_SDA, TOUCH_SCL);

  // Initialize display
  tft.init();
  tft.setRotation(0);  // Portrait mode
  tft.fillScreen(COLOR_BG);

  Serial.println("[TOUCH169] Display initialized");

  // POC-1: Initialize touch controller
  if (initTouch()) {
    Serial.println("[TOUCH169] Touch controller initialized (POC-1 PASS)");
  } else {
    Serial.println("[TOUCH169] Touch controller FAILED (POC-1 FAIL)");
  }

  // POC-2: Test Preferences library
  testPreferences();

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
      timeSource.setMeshTime(unixTime);
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

  // Initialize battery monitoring
  battery.begin();
  Serial.println("[TOUCH169] Battery monitoring initialized");

  // Clear startup message and draw clock
  delay(500);
  tft.fillScreen(COLOR_BG);
  drawClockFace();
  drawCornerLabels();

  // Initialize activity timer
  resetActivityTimer();

  Serial.println("[TOUCH169] Ready");
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
  checkPowerButton();
  checkBootButton();
  battery.update();  // Update battery monitoring
  handleTouch();  // POC-1: Handle touch input

  // Handle boot button - short press = back, long press = debug (per spec)
  if (bootBtnShortPress) {
    bootBtnShortPress = false;

    if (displayAsleep) {
      // Wake up display on button press
      displayWake();
    } else {
      // Short press = navigate back (per spec section 3)
      navigateBack();
    }
  }

  // Check for sleep timeout
  if (!displayAsleep && (millis() - lastActivityTime >= DISPLAY_SLEEP_TIMEOUT_MS)) {
    displaySleep();
  }

  // Only update display if awake
  if (!displayAsleep) {
    renderCurrentScreen();
  }
}

// ============== POWER MANAGEMENT ==============

void powerOff() {
  Serial.println("[TOUCH169] Powering off...");

  // Show power off message
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(CENTER_X - 60, CENTER_Y - 10);
  tft.print("Power Off");
  delay(500);

  // Turn off backlight
  digitalWrite(TFT_BL, LOW);

  // Release power latch - this will cut power on battery
  digitalWrite(PWR_EN_PIN, LOW);

  // If on USB power, we won't actually power off, so just wait
  delay(2000);

  // If still running (USB powered), restart instead
  Serial.println("[TOUCH169] Still powered (USB?), restarting...");
  ESP.restart();
}

void checkPowerButton() {
  bool btnPressed = digitalRead(PWR_BTN_PIN) == LOW;  // Active low
  unsigned long now = millis();

  if (btnPressed && !pwrBtnWasPressed) {
    // Button just pressed
    pwrBtnPressTime = now;
    pwrBtnWasPressed = true;
  } else if (btnPressed && pwrBtnWasPressed) {
    // Button held - check for long press to power off
    if (now - pwrBtnPressTime >= PWR_BTN_LONG_PRESS_MS) {
      powerOff();
    }
  } else if (!btnPressed && pwrBtnWasPressed) {
    // Button released
    pwrBtnWasPressed = false;
  }
}

void checkBootButton() {
  bool btnPressed = digitalRead(BOOT_BTN_PIN) == LOW;  // Active low (has pullup)
  unsigned long now = millis();

  if (btnPressed && !bootBtnWasPressed) {
    // Button just pressed
    bootBtnPressTime = now;
    bootBtnWasPressed = true;
    bootBtnLongPress = false;
  } else if (btnPressed && bootBtnWasPressed && !bootBtnLongPress) {
    // Button held - check for long press (debug screen per spec)
    if (now - bootBtnPressTime >= BOOT_BTN_LONG_PRESS_MS) {
      bootBtnLongPress = true;
      // Navigate to debug on long press
      if (!displayAsleep) {
        navigateTo(SCREEN_DEBUG);
        Serial.println("[TOUCH169] Long press -> Debug screen");
      }
    }
  } else if (!btnPressed && bootBtnWasPressed) {
    // Button released - check for short press (only if not long press)
    unsigned long pressDuration = now - bootBtnPressTime;
    if (!bootBtnLongPress && pressDuration >= BOOT_BTN_DEBOUNCE_MS) {
      bootBtnShortPress = true;
    }
    bootBtnWasPressed = false;
    bootBtnLongPress = false;
  }
}

// ============== BATTERY FUNCTIONS ==============

void drawBatteryIndicator() {
  // Only redraw when state changes
  if (!battery.stateChanged() && !batteryIndicatorDirty) return;
  batteryIndicatorDirty = false;

  ChargingState state = battery.getState();

  // Draw indicator in top-center area (below date)
  int indicatorX = CENTER_X;
  int indicatorY = 45;

  // Clear previous indicator area
  tft.fillRect(indicatorX - 20, indicatorY - 8, 40, 16, COLOR_BG);

  if (state == ChargingState::Unknown) return;

  // Draw battery outline
  tft.drawRect(indicatorX - 12, indicatorY - 5, 20, 10, COLOR_TEXT);
  tft.fillRect(indicatorX + 8, indicatorY - 2, 3, 4, COLOR_TEXT);

  // Fill based on state
  uint16_t fillColor;
  int percent = battery.getPercent();
  int fillWidth = (percent * 16) / 100;

  switch (state) {
    case ChargingState::Charging:
      fillColor = COLOR_HUMID;  // Green when charging
      // Draw plus symbol
      tft.setTextColor(COLOR_HUMID, COLOR_BG);
      tft.setTextSize(1);
      tft.setCursor(indicatorX - 18, indicatorY - 4);
      tft.print("+");
      break;
    case ChargingState::Full:
      fillColor = COLOR_HUMID;  // Green when full
      break;
    case ChargingState::Discharging:
      fillColor = (percent > 20) ? COLOR_TEXT : COLOR_SECOND;  // White or red when low
      break;
    default:
      fillColor = COLOR_TICK;
      break;
  }

  tft.fillRect(indicatorX - 10, indicatorY - 3, fillWidth, 6, fillColor);
}

// ============== DEBUG SCREEN ==============

void drawDebugScreen() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(60, 10);
  tft.print("DEBUG INFO");

  tft.drawLine(10, 35, 230, 35, COLOR_TICK);
}

void updateDebugScreen() {
  static unsigned long lastUpdate = 0;

  if (screenChanged) {
    screenChanged = false;
    drawDebugScreen();
    lastUpdate = 0;  // Force immediate update
  }

  // Update every 500ms
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();

  float voltage = battery.getVoltage();
  int percent = battery.getPercent();
  ChargingState state = battery.getState();

  tft.setTextSize(2);

  // Battery section
  tft.fillRect(10, 45, 220, 55, COLOR_BG);
  tft.setTextColor(COLOR_HUMID);
  tft.setCursor(10, 45);
  tft.print("Battery:");
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 65);
  tft.printf("%.2fV  %d%%", voltage, percent);

  // Show charging state
  tft.setCursor(120, 65);
  switch (state) {
    case ChargingState::Charging:
      tft.setTextColor(COLOR_HUMID);
      tft.print("[CHARGING]");
      break;
    case ChargingState::Full:
      tft.setTextColor(COLOR_HUMID);
      tft.print("[FULL]");
      break;
    case ChargingState::Discharging:
      tft.setTextColor(COLOR_TEXT);
      tft.print("[ON BAT]");
      break;
    default:
      tft.setTextColor(COLOR_TICK);
      tft.print("[...]");
      break;
  }

  // Draw battery bar
  int barWidth = (percent * 180) / 100;
  uint16_t barColor = (state == ChargingState::Charging || state == ChargingState::Full)
                      ? COLOR_HUMID : (percent > 20 ? COLOR_TEXT : COLOR_SECOND);
  tft.drawRect(10, 90, 184, 12, COLOR_TICK);
  tft.fillRect(12, 92, barWidth, 8, barColor);
  tft.fillRect(194, 94, 4, 4, COLOR_TICK);  // Battery nub

  // Mesh info
  tft.fillRect(10, 110, 220, 80, COLOR_BG);
  tft.setTextColor(COLOR_TEMP);
  tft.setCursor(10, 110);
  tft.print("Mesh:");
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 130);
  tft.printf("Node ID: %u", swarm.getNodeId());
  tft.setCursor(10, 145);
  tft.printf("Peers: %d", swarm.getPeerCount());
  tft.setCursor(10, 160);
  tft.printf("Role: %s", swarm.isCoordinator() ? "Coordinator" : "Member");
  tft.setCursor(10, 175);
  tft.printf("Uptime: %lus", millis() / 1000);

  // Sensor data section
  tft.setTextSize(2);
  tft.fillRect(10, 195, 220, 80, COLOR_BG);
  tft.setTextColor(COLOR_LIGHT);
  tft.setCursor(10, 195);
  tft.print("Sensors:");
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, 215);
  tft.printf("Temp: %s C", meshTemp.c_str());
  tft.setCursor(10, 230);
  tft.printf("Humidity: %s %%", meshHumid.c_str());
  tft.setCursor(10, 245);
  tft.printf("Light: %s", meshLight.c_str());
  tft.setCursor(10, 260);
  tft.printf("Motion: %s  LED: %s", meshMotion.c_str(), meshLed.c_str());
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

  // Try to get time from TimeSource
  if (!timeSource.getTime(&timeinfo)) {
    // Time not yet synced - show waiting message
    if (!timeSource.isValid()) {
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
  if (!timeSource.isValid()) {
    timeSource.markValid();
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

// ============== DISPLAY SLEEP FUNCTIONS ==============

void resetActivityTimer() {
  lastActivityTime = millis();
}

void displaySleep() {
  if (displayAsleep) return;

  displayAsleep = true;
  Serial.println("[TOUCH169] Display sleeping...");

  // Turn off backlight to save power
  digitalWrite(TFT_BL, LOW);
}

void displayWake() {
  if (!displayAsleep) return;

  displayAsleep = false;
  Serial.println("[TOUCH169] Display waking...");

  // Turn on backlight
  digitalWrite(TFT_BL, HIGH);

  // Force screen redraw
  screenChanged = true;
  batteryIndicatorDirty = true;  // Force battery indicator redraw

  // Reset activity timer
  resetActivityTimer();
}

// ============== TOUCH FUNCTIONS (POC-1) ==============

bool initTouch() {
  // CST816T touch controller at I2C address 0x15
  // Using SensorLib's TouchClassCST816 driver
  if (!touch.begin(Wire, 0x15, TOUCH_SDA, TOUCH_SCL)) {
    Serial.println("[TOUCH169] CST816T not found at 0x15");
    return false;
  }

  // Get model name for verification
  Serial.printf("[TOUCH169] Touch model: %s\n", touch.getModelName());

  touchInitialized = true;
  return true;
}

void handleTouch() {
  if (!touchInitialized) return;

  // Check for touch events
  // getPoint returns number of touch points, fills x_array and y_array
  uint8_t touched = touch.getPoint(&touchX, &touchY, 1);

  if (touched > 0) {
    unsigned long now = millis();

    // Wake display on touch (before debounce to feel responsive)
    if (displayAsleep) {
      displayWake();
      Serial.println("[TOUCH169] Touch wake");
      touchActive = false;  // Reset swipe tracking
      return;  // Don't process touch action when waking
    }

    // Track touch start for swipe detection
    if (!touchActive) {
      touchStartX = touchX;
      touchStartY = touchY;
      touchActive = true;
      Serial.printf("[TOUCH169] Touch start x=%d, y=%d\n", touchX, touchY);
    }

    // Debounce for tap processing (swipe uses start/end)
    if (now - lastTouchTime < TOUCH_DEBOUNCE_MS) return;
    lastTouchTime = now;

    // Ignore touches during cooldown after screen transition
    if (now - screenTransitionTime < TOUCH_COOLDOWN_MS) {
      Serial.printf("[TOUCH169] Touch ignored (cooldown) x=%d, y=%d\n", touchX, touchY);
      return;
    }

    // Reset activity timer on any touch
    resetActivityTimer();

    touchPressed = true;
  } else {
    // Touch released - check for swipe gesture
    if (touchActive) {
      int16_t deltaX = touchX - touchStartX;
      int16_t deltaY = touchY - touchStartY;

      // Swipe down detection (for nav menu on clock screen)
      if (deltaY > SWIPE_MIN_DISTANCE && abs(deltaX) < SWIPE_MAX_CROSS) {
        Serial.printf("[TOUCH169] Swipe DOWN detected (dy=%d)\n", deltaY);
        if (currentScreen == SCREEN_CLOCK) {
          navigateTo(SCREEN_NAV_MENU);
        }
        touchActive = false;
        touchPressed = false;
        return;
      }

      // Swipe up detection (close nav menu)
      if (deltaY < -SWIPE_MIN_DISTANCE && abs(deltaX) < SWIPE_MAX_CROSS) {
        Serial.printf("[TOUCH169] Swipe UP detected (dy=%d)\n", deltaY);
        if (currentScreen == SCREEN_NAV_MENU) {
          navigateBack();
        }
        touchActive = false;
        touchPressed = false;
        return;
      }

      // Not a swipe - process as tap at start position
      Serial.printf("[TOUCH169] Tap at x=%d, y=%d\n", touchStartX, touchStartY);

      // Use start position for tap detection (more accurate)
      touchX = touchStartX;
      touchY = touchStartY;
      processTouchZones();

      touchActive = false;
    }
    touchPressed = false;
  }
}

// ============== NAVIGATION FUNCTIONS (Phase 1.1) ==============

const char* getScreenName(ScreenMode screen) {
  switch (screen) {
    case SCREEN_CLOCK:             return "Clock";
    case SCREEN_HUMIDITY:          return "Humidity";
    case SCREEN_HUMIDITY_SETTINGS: return "Humidity Settings";
    case SCREEN_TEMPERATURE:       return "Temperature";
    case SCREEN_TEMP_SETTINGS:     return "Temp Settings";
    case SCREEN_LIGHT:             return "Light";
    case SCREEN_LIGHT_SETTINGS:    return "Light Settings";
    case SCREEN_MOTION_LED:        return "Motion/LED";
    case SCREEN_MOTION_LED_SETTINGS: return "Motion/LED Settings";
    case SCREEN_CALENDAR:          return "Calendar";
    case SCREEN_DATE_SETTINGS:     return "Date Settings";
    case SCREEN_CLOCK_DETAILS:     return "Clock Details";
    case SCREEN_TIME_SETTINGS:     return "Time Settings";
    case SCREEN_ALARM:             return "Alarm";
    case SCREEN_STOPWATCH:         return "Stopwatch";
    case SCREEN_NAV_MENU:          return "Navigation";
    case SCREEN_DEBUG:             return "Debug";
    default:                       return "Unknown";
  }
}

ScreenMode getParentScreen(ScreenMode screen) {
  // Define parent screen for each screen per navigation spec
  switch (screen) {
    // Sensor detail screens -> Clock
    case SCREEN_HUMIDITY:
    case SCREEN_TEMPERATURE:
    case SCREEN_LIGHT:
    case SCREEN_MOTION_LED:
    case SCREEN_CALENDAR:
    case SCREEN_CLOCK_DETAILS:
      return SCREEN_CLOCK;

    // Settings screens -> their parent detail screen
    case SCREEN_HUMIDITY_SETTINGS:
      return SCREEN_HUMIDITY;
    case SCREEN_TEMP_SETTINGS:
      return SCREEN_TEMPERATURE;
    case SCREEN_LIGHT_SETTINGS:
      return SCREEN_LIGHT;
    case SCREEN_MOTION_LED_SETTINGS:
      return SCREEN_MOTION_LED;
    case SCREEN_DATE_SETTINGS:
      return SCREEN_CALENDAR;
    case SCREEN_TIME_SETTINGS:
      return SCREEN_CLOCK_DETAILS;

    // Sub-screens of Clock Details
    case SCREEN_ALARM:
    case SCREEN_STOPWATCH:
      return SCREEN_CLOCK_DETAILS;

    // Nav menu -> previous screen
    case SCREEN_NAV_MENU:
      return previousScreen;

    // Debug -> Clock
    case SCREEN_DEBUG:
      return SCREEN_CLOCK;

    // Clock has no parent (root)
    case SCREEN_CLOCK:
    default:
      return SCREEN_CLOCK;
  }
}

void navigateTo(ScreenMode screen) {
  if (screen == currentScreen) return;

  // Save current screen for back navigation (except when going to nav menu)
  if (screen != SCREEN_NAV_MENU) {
    previousScreen = currentScreen;
  }

  Serial.printf("[NAV] %s -> %s\n", getScreenName(currentScreen), getScreenName(screen));

  currentScreen = screen;
  screenChanged = true;
  screenTransitionTime = millis();  // Start touch cooldown
  resetActivityTimer();
}

void navigateBack() {
  ScreenMode parent = getParentScreen(currentScreen);

  // If we're at the clock (root), do nothing
  if (currentScreen == SCREEN_CLOCK) {
    Serial.println("[NAV] Already at root (Clock)");
    return;
  }

  Serial.printf("[NAV] Back: %s -> %s\n", getScreenName(currentScreen), getScreenName(parent));

  currentScreen = parent;
  screenChanged = true;
  screenTransitionTime = millis();  // Start touch cooldown
  resetActivityTimer();
}

void renderCurrentScreen() {
  // Main screen rendering dispatcher
  // For now, only Clock and Debug are fully implemented
  // Other screens will be added in later phases

  switch (currentScreen) {
    case SCREEN_CLOCK:
      updateClock();
      updateCorners();
      drawBatteryIndicator();
      break;

    case SCREEN_DEBUG:
      updateDebugScreen();
      break;

    // Placeholder for future screens - show screen name
    default:
      if (screenChanged) {
        screenChanged = false;
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(COLOR_TEXT);
        tft.setTextSize(2);
        tft.setCursor(20, 20);
        tft.printf("< %s", getScreenName(currentScreen));
        tft.setTextSize(1);
        tft.setCursor(20, 60);
        tft.print("Screen not yet implemented");
        tft.setCursor(20, 80);
        tft.print("Press boot button to go back");
      }
      break;
  }
}

void processTouchZones() {
  // Touch zone detection based on spec coordinates
  // Clock screen touch zones (from touch169_touch_navigation_spec.md):
  // | Top-Left     | (0,0)     | 80x60   | Humidity Detail    |
  // | Top-Center   | (80,0)    | 80x40   | Calendar           |
  // | Top-Right    | (160,0)   | 80x60   | Temperature Detail |
  // | Bottom-Left  | (0,210)   | 80x50   | Light Detail       |
  // | Bottom-Right | (160,210) | 80x50   | Motion/LED Detail  |
  // | Center       | (60,80)   | 120x120 | Clock Details      |
  // Nav menu: Swipe down from clock screen (handled in handleTouch)

  if (currentScreen == SCREEN_CLOCK) {
    // Top-Left: Humidity (0,0) 80x60
    if (touchX >= 0 && touchX < 80 && touchY >= 0 && touchY < 60) {
      navigateTo(SCREEN_HUMIDITY);
      return;
    }
    // Top-Center: Calendar (80,0) 80x40
    if (touchX >= 80 && touchX < 160 && touchY >= 0 && touchY < 40) {
      navigateTo(SCREEN_CALENDAR);
      return;
    }
    // Top-Right: Temperature (160,0) 80x60
    if (touchX >= 160 && touchX < 240 && touchY >= 0 && touchY < 60) {
      navigateTo(SCREEN_TEMPERATURE);
      return;
    }
    // Bottom-Left: Light (0,210) 80x50
    if (touchX >= 0 && touchX < 80 && touchY >= 210 && touchY < 260) {
      navigateTo(SCREEN_LIGHT);
      return;
    }
    // Bottom-Right: Motion/LED (160,210) 80x50
    if (touchX >= 160 && touchX < 240 && touchY >= 210 && touchY < 260) {
      navigateTo(SCREEN_MOTION_LED);
      return;
    }
    // Center: Clock Details (60,80) 120x120
    if (touchX >= 60 && touchX < 180 && touchY >= 80 && touchY < 200) {
      navigateTo(SCREEN_CLOCK_DETAILS);
      return;
    }
  }
  // Navigation menu - back arrow or tap outside to close
  else if (currentScreen == SCREEN_NAV_MENU) {
    // Back Arrow - enlarged zone (0,0) 100x60 for easier tapping
    if (touchX >= 0 && touchX < 100 && touchY >= 0 && touchY < 60) {
      navigateBack();
      return;
    }
    // TODO: Add nav menu grid buttons here in future phase
  }
  // Detail screen header zones (all other detail screens)
  else if (currentScreen != SCREEN_DEBUG) {
    // Back Arrow - enlarged zone (0,0) 100x60 for easier tapping
    if (touchX >= 0 && touchX < 100 && touchY >= 0 && touchY < 60) {
      navigateBack();
      return;
    }
    // Gear Icon - enlarged zone (180,0) 60x60 for easier tapping
    if (touchX >= 180 && touchX < 240 && touchY >= 0 && touchY < 60) {
      switch (currentScreen) {
        case SCREEN_HUMIDITY:
          navigateTo(SCREEN_HUMIDITY_SETTINGS);
          break;
        case SCREEN_TEMPERATURE:
          navigateTo(SCREEN_TEMP_SETTINGS);
          break;
        case SCREEN_LIGHT:
          navigateTo(SCREEN_LIGHT_SETTINGS);
          break;
        case SCREEN_MOTION_LED:
          navigateTo(SCREEN_MOTION_LED_SETTINGS);
          break;
        case SCREEN_CALENDAR:
          navigateTo(SCREEN_DATE_SETTINGS);
          break;
        case SCREEN_CLOCK_DETAILS:
          navigateTo(SCREEN_TIME_SETTINGS);
          break;
        default:
          break;
      }
      return;
    }
  }
}

// ============== PREFERENCES FUNCTIONS (POC-2) ==============

void testPreferences() {
  Serial.println("[TOUCH169] Testing Preferences library (POC-2)...");

  // Open preferences namespace
  preferences.begin("touch169", false);  // false = read/write mode

  // Check if we have a stored test value
  int32_t bootCount = preferences.getInt("bootCount", 0);
  Serial.printf("[TOUCH169] Boot count before increment: %d\n", bootCount);

  // Increment and store
  bootCount++;
  preferences.putInt("bootCount", bootCount);

  // Read back to verify
  int32_t readBack = preferences.getInt("bootCount", -1);
  Serial.printf("[TOUCH169] Boot count after increment: %d\n", readBack);

  // Test timezone storage (simulating settings persistence)
  String testTimezone = preferences.getString("timezone", "");
  if (testTimezone.length() == 0) {
    // First run - store default timezone
    preferences.putString("timezone", "EST");
    Serial.println("[TOUCH169] Stored default timezone: EST");
  } else {
    Serial.printf("[TOUCH169] Read stored timezone: %s\n", testTimezone.c_str());
  }

  preferences.end();

  if (readBack == bootCount && readBack > 0) {
    Serial.println("[TOUCH169] Preferences write/read test (POC-2 PASS)");
  } else {
    Serial.println("[TOUCH169] Preferences write/read test (POC-2 FAIL)");
  }
}

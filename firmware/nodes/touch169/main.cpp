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
// Preferences included via SettingsManager.h

// Board configuration (pins, colors, geometry, timing)
#include "BoardConfig.h"

// Extracted classes
#include "Battery.h"
#include "TimeSource.h"
#include "Navigator.h"
#include "GestureDetector.h"
#include "SettingsManager.h"
#include "MeshSwarmAdapter.h"
#include "DisplayManager.h"
#include "PowerManager.h"
#include "TouchInput.h"
#include "InputManager.h"

// ============== GLOBALS ==============
MeshSwarm swarm;
TFT_eSPI tft = TFT_eSPI();
SettingsManager settings;
Battery battery;
TimeSource timeSource;
Navigator navigator;
GestureDetector gesture(SWIPE_MIN_DISTANCE, SWIPE_MAX_CROSS);
MeshSwarmAdapter meshState(swarm);
DisplayManager display(tft, navigator, TFT_BL);
PowerManager power;
TouchInput touchInput;
InputManager input(touchInput, gesture);

// Touch state (used by processTouchZones)
int16_t touchX = -1;
int16_t touchY = -1;

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

// Screen state (Navigator class manages currentScreen, previousScreen, screenChanged)
bool firstDraw = true;

// Power button state managed by PowerManager
// Boot button state managed by InputManager
// Display sleep state managed by DisplayManager

// ============== FUNCTION DECLARATIONS ==============
void drawClockFace();
void drawHand(float angle, int length, uint16_t color, int width);
void eraseHand(float angle, int length, int width);
void updateClock();
void updateCorners();
void drawCornerLabels();
void onPowerOff();            // Power off callback for PowerManager
void drawBatteryIndicator();  // Uses Battery class
void drawDebugScreen();
void updateDebugScreen();

// Input callbacks
void onTap(int16_t x, int16_t y);
void onSwipe(SwipeDirection dir);
void onTouch();
void onBootShortPress();
void onBootLongPress();

// Fallback rendering for screens not yet migrated to ScreenRenderer
void fallbackRender(Screen screen, TFT_eSPI& tft, Navigator& nav);
bool fallbackTouchHandler(Screen screen, int16_t x, int16_t y, Navigator& nav);

// Navigation functions (Phase 1.1) - uses Navigator class
void processTouchZones();

// ============== SETUP ==============
void setup() {
  // CRITICAL: Initialize power management FIRST to latch power on battery
  power.begin();

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

  // Initialize touch controller
  if (touchInput.begin(Wire)) {
    Serial.println("[TOUCH169] Touch controller initialized");
  } else {
    Serial.println("[TOUCH169] Touch controller FAILED");
  }

  // Initialize settings manager (loads from flash, increments boot count)
  settings.begin();
  Serial.printf("[TOUCH169] Settings initialized, boot count: %d\n", settings.getBootCount());

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

  // Initialize mesh state adapter (registers watchers for sensor data)
  meshState.setTimeSource(&timeSource);
  meshState.begin();
  Serial.println("[TOUCH169] MeshState adapter initialized");

  // Initialize battery monitoring
  battery.begin();
  Serial.println("[TOUCH169] Battery monitoring initialized");

  // Initialize display manager with fallback callbacks
  display.setFallbackRenderer(fallbackRender);
  display.setFallbackTouchHandler(fallbackTouchHandler);
  display.begin();
  Serial.println("[TOUCH169] DisplayManager initialized");

  // Set power off callback to show message before power down
  power.onPowerOff(onPowerOff);
  Serial.println("[TOUCH169] PowerManager callback set");

  // Initialize input manager with callbacks
  input.onTap(onTap);
  input.onSwipe(onSwipe);
  input.onTouch(onTouch);
  input.onBootShortPress(onBootShortPress);
  input.onBootLongPress(onBootLongPress);
  input.begin();
  Serial.println("[TOUCH169] InputManager initialized");

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
  power.update();    // Check power button (long press = power off)
  battery.update();  // Update battery monitoring
  input.update();    // Handle touch and boot button

  // Check for sleep timeout
  display.checkSleepTimeout();

  // Render current screen (DisplayManager checks if asleep)
  display.render();
}

// ============== POWER MANAGEMENT ==============

// Power off callback - shows message and turns off backlight
void onPowerOff() {
  // Show power off message
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(CENTER_X - 60, CENTER_Y - 10);
  tft.print("Power Off");
  delay(500);

  // Turn off backlight
  digitalWrite(TFT_BL, LOW);
}

// ============== BATTERY FUNCTIONS ==============

void drawBatteryIndicator() {
  // Only redraw when state changes
  if (!battery.stateChanged() && !batteryIndicatorDirty) return;
  batteryIndicatorDirty = false;

  ChargingState state = battery.getState();

  // Draw indicator above digital time display (bottom center)
  int indicatorX = CENTER_X;
  int indicatorY = SCREEN_HEIGHT - 42;  // Above digital time at SCREEN_HEIGHT - 28

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

  if (navigator.hasChanged()) {
    navigator.clearChanged();
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
  tft.printf("Temp: %s C", meshState.getTemperature().c_str());
  tft.setCursor(10, 230);
  tft.printf("Humidity: %s %%", meshState.getHumidity().c_str());
  tft.setCursor(10, 245);
  tft.printf("Light: %s", meshState.getLightLevel().c_str());
  tft.setCursor(10, 260);
  tft.printf("Motion: %s  LED: %s", meshState.getMotionRaw().c_str(), meshState.getLedRaw().c_str());
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
  if (navigator.hasChanged()) {
    navigator.clearChanged();
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
    // Reset corner prev values to force redraw with current sensor data
    prevTemp = "";
    prevHumid = "";
    prevLight = "";
    prevMotion = "";
    prevLed = "";
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
  // Two lines: day of week on top, month + day below
  if (min != lastMin || lastMin == -1) {
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    // Clear date area (between corner labels) - taller for two lines
    tft.fillRect(80, CORNER_MARGIN, 80, 36, COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_DATE, COLOR_BG);
    // Line 1: Day of week centered
    tft.setCursor(96, CORNER_MARGIN);
    tft.print(days[timeinfo.tm_wday]);
    // Line 2: Month + day centered
    char dateBuf[8];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d", months[timeinfo.tm_mon], timeinfo.tm_mday);
    int dateWidth = strlen(dateBuf) * 12;  // ~12px per char at size 2
    tft.setCursor(120 - dateWidth / 2, CORNER_MARGIN + 18);
    tft.print(dateBuf);
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
  // Get current values from mesh state adapter
  String meshHumid = meshState.getHumidity();
  String meshTemp = meshState.getTemperature();
  String meshLight = meshState.getLightLevel();
  String meshMotion = meshState.getMotionRaw();
  String meshLed = meshState.getLedRaw();

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

// ============== INPUT CALLBACKS ==============

void onTap(int16_t x, int16_t y) {
  touchX = x;
  touchY = y;
  processTouchZones();
}

void onSwipe(SwipeDirection dir) {
  switch (dir) {
    case SwipeDirection::Down:
      // Swipe down on clock screen opens nav menu
      if (navigator.current() == Screen::Clock) {
        navigator.navigateTo(Screen::NavMenu);
        display.resetActivityTimer();
      }
      break;

    case SwipeDirection::Up:
      // Swipe up on nav menu closes it
      if (navigator.current() == Screen::NavMenu) {
        navigator.navigateBack();
        display.resetActivityTimer();
      }
      break;

    default:
      // SwipeLeft, SwipeRight - not used yet
      break;
  }
}

void onTouch() {
  // Wake display on any touch
  if (display.isAsleep()) {
    display.wake();
    input.cancelTouch();  // Don't process gesture when waking
    Serial.println("[TOUCH169] Touch wake");
    return;
  }
  display.resetActivityTimer();
}

void onBootShortPress() {
  if (display.isAsleep()) {
    display.wake();
  } else {
    navigator.navigateBack();
    display.resetActivityTimer();
  }
}

void onBootLongPress() {
  if (!display.isAsleep()) {
    navigator.navigateTo(Screen::Debug);
    display.resetActivityTimer();
  }
}

// ============== NAVIGATION FUNCTIONS (Phase 1.1 - uses Navigator class) ==============

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

  Screen current = navigator.current();

  if (current == Screen::Clock) {
    // Top-Left: Humidity (0,0) 80x60
    if (touchX >= 0 && touchX < 80 && touchY >= 0 && touchY < 60) {
      navigator.navigateTo(Screen::Humidity);
      display.resetActivityTimer();
      return;
    }
    // Top-Center: Calendar (80,0) 80x40
    if (touchX >= 80 && touchX < 160 && touchY >= 0 && touchY < 40) {
      navigator.navigateTo(Screen::Calendar);
      display.resetActivityTimer();
      return;
    }
    // Top-Right: Temperature (160,0) 80x60
    if (touchX >= 160 && touchX < 240 && touchY >= 0 && touchY < 60) {
      navigator.navigateTo(Screen::Temperature);
      display.resetActivityTimer();
      return;
    }
    // Bottom-Left: Light (0,210) 80x50
    if (touchX >= 0 && touchX < 80 && touchY >= 210 && touchY < 260) {
      navigator.navigateTo(Screen::Light);
      display.resetActivityTimer();
      return;
    }
    // Bottom-Right: Motion/LED (160,210) 80x50
    if (touchX >= 160 && touchX < 240 && touchY >= 210 && touchY < 260) {
      navigator.navigateTo(Screen::MotionLed);
      display.resetActivityTimer();
      return;
    }
    // Center: Clock Details (60,80) 120x120
    if (touchX >= 60 && touchX < 180 && touchY >= 80 && touchY < 200) {
      navigator.navigateTo(Screen::ClockDetails);
      display.resetActivityTimer();
      return;
    }
  }
  // Navigation menu - back arrow or tap outside to close
  else if (current == Screen::NavMenu) {
    // Back Arrow - enlarged zone (0,0) 100x60 for easier tapping
    if (touchX >= 0 && touchX < 100 && touchY >= 0 && touchY < 60) {
      navigator.navigateBack();
      display.resetActivityTimer();
      return;
    }
    // TODO: Add nav menu grid buttons here in future phase
  }
  // Detail screen header zones (all other detail screens)
  else if (current != Screen::Debug) {
    // Back Arrow - enlarged zone (0,0) 100x60 for easier tapping
    if (touchX >= 0 && touchX < 100 && touchY >= 0 && touchY < 60) {
      navigator.navigateBack();
      display.resetActivityTimer();
      return;
    }
    // Gear Icon - enlarged zone (180,0) 60x60 for easier tapping
    if (touchX >= 180 && touchX < 240 && touchY >= 0 && touchY < 60) {
      switch (current) {
        case Screen::Humidity:
          navigator.navigateTo(Screen::HumiditySettings);
          break;
        case Screen::Temperature:
          navigator.navigateTo(Screen::TempSettings);
          break;
        case Screen::Light:
          navigator.navigateTo(Screen::LightSettings);
          break;
        case Screen::MotionLed:
          navigator.navigateTo(Screen::MotionLedSettings);
          break;
        case Screen::Calendar:
          navigator.navigateTo(Screen::DateSettings);
          break;
        case Screen::ClockDetails:
          navigator.navigateTo(Screen::TimeSettings);
          break;
        default:
          break;
      }
      display.resetActivityTimer();
      return;
    }
  }
}

// ============== FALLBACK FUNCTIONS FOR DISPLAYMANAGER ==============
// These handle screens not yet migrated to ScreenRenderer classes

void fallbackRender(Screen screen, TFT_eSPI& tft, Navigator& nav) {
  // Route to existing render functions
  switch (screen) {
    case Screen::Clock:
      if (nav.hasChanged()) {
        batteryIndicatorDirty = true;  // Force redraw when entering screen
      }
      updateClock();
      updateCorners();
      drawBatteryIndicator();
      break;

    case Screen::Debug:
      updateDebugScreen();
      break;

    // Placeholder for future screens - show screen name
    default:
      if (nav.hasChanged()) {
        nav.clearChanged();
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(COLOR_TEXT);
        tft.setTextSize(2);
        tft.setCursor(20, 20);
        tft.printf("< %s", Navigator::getScreenName(screen));
        tft.setTextSize(1);
        tft.setCursor(20, 60);
        tft.print("Screen not yet implemented");
        tft.setCursor(20, 80);
        tft.print("Press boot button to go back");
      }
      break;
  }
}

bool fallbackTouchHandler(Screen screen, int16_t x, int16_t y, Navigator& nav) {
  // Route to existing touch handler (processTouchZones uses global touchX/touchY)
  touchX = x;
  touchY = y;
  processTouchZones();
  return true;  // Always claim touch was handled
}

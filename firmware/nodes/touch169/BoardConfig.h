/**
 * @file BoardConfig.h
 * @brief Hardware configuration for Waveshare ESP32-S3-Touch-LCD-1.69
 *
 * Consolidates all pin definitions, display geometry, and hardware constants.
 * This file is the single source of truth for board-specific configuration.
 */

#ifndef BOARDCONFIG_H
#define BOARDCONFIG_H

#include <stdint.h>

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

// ============== DISPLAY PINS (configured in platformio.ini via TFT_eSPI) ==============
#ifndef TFT_BL
#define TFT_BL 15
#endif

// ============== POWER CONTROL PINS ==============
// Waveshare board has power latch circuit - must hold HIGH to stay powered on battery
// Old version: GPIO35/36, New version: GPIO41/40
#ifndef USE_NEW_PIN_CONFIG
#define USE_NEW_PIN_CONFIG 0  // Set to 1 for newer board revisions
#endif

#if USE_NEW_PIN_CONFIG
#define PWR_EN_PIN    41      // Power latch output (new boards)
#define PWR_BTN_PIN   40      // Power button input (new boards)
#else
#define PWR_EN_PIN    35      // Power latch output (old boards)
#define PWR_BTN_PIN   36      // Power button input (old boards)
#endif

// ============== BATTERY MONITORING ==============
#define BAT_ADC_PIN   1       // GPIO1 - battery voltage via divider
#define BAT_R1        200000.0  // 200k ohm
#define BAT_R2        100000.0  // 100k ohm
#define BAT_VREF      3.3       // ADC reference voltage

// ============== BOOT BUTTON ==============
#define BOOT_BTN_PIN  0       // GPIO0 - boot button

// ============== I2C PINS (Touch, IMU, RTC) ==============
#ifndef TOUCH_SDA
#define TOUCH_SDA 11
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL 10
#endif

// ============== DISPLAY GEOMETRY ==============
namespace BoardConfig {
    // Screen dimensions
    constexpr int16_t SCREEN_WIDTH  = 240;
    constexpr int16_t SCREEN_HEIGHT = 280;
    constexpr int16_t CENTER_X      = 120;
    constexpr int16_t CENTER_Y      = 140;  // Center of display

    // Clock face
    constexpr int16_t CLOCK_RADIUS    = 80;
    constexpr int16_t HOUR_HAND_LEN   = 40;
    constexpr int16_t MIN_HAND_LEN    = 55;
    constexpr int16_t SEC_HAND_LEN    = 65;

    // Corner layout
    constexpr int16_t CORNER_MARGIN   = 8;
    constexpr int16_t CORNER_TEXT_H   = 16;
}

// ============== DISPLAY COLORS (RGB565) ==============
namespace Colors {
    constexpr uint16_t BG       = 0x0000;  // Black
    constexpr uint16_t FACE     = 0x2104;  // Dark gray
    constexpr uint16_t HOUR     = 0xFFFF;  // White
    constexpr uint16_t MINUTE   = 0xFFFF;  // White
    constexpr uint16_t SECOND   = 0xF800;  // Red
    constexpr uint16_t TEXT     = 0xFFFF;  // White
    constexpr uint16_t TEMP     = 0xFD20;  // Orange
    constexpr uint16_t HUMID    = 0x07E0;  // Green
    constexpr uint16_t LIGHT    = 0xFFE0;  // Yellow
    constexpr uint16_t MOTION   = 0x07FF;  // Cyan
    constexpr uint16_t LED      = 0xF81F;  // Magenta
    constexpr uint16_t TICK     = 0x8410;  // Gray
    constexpr uint16_t DATE     = 0xAD55;  // Light gray
}

// ============== TIMING CONSTANTS ==============
namespace Timing {
    // Touch handling
    constexpr unsigned long TOUCH_DEBOUNCE_MS    = 100;  // Debounce touch events
    constexpr unsigned long TOUCH_COOLDOWN_MS    = 300;  // Ignore touches after screen transition

    // Swipe gesture thresholds
    constexpr int16_t SWIPE_MIN_DISTANCE = 50;   // Minimum pixels to count as swipe
    constexpr int16_t SWIPE_MAX_CROSS    = 40;   // Max perpendicular movement for swipe

    // Battery monitoring
    constexpr unsigned long VOLTAGE_READ_INTERVAL = 2000;  // Read voltage every 2 seconds
    constexpr float VOLTAGE_FULL_THRESHOLD    = 4.15f;  // Consider full above this
    constexpr float VOLTAGE_TREND_THRESHOLD   = 0.02f;  // Min voltage change to detect trend

    // Button handling
    constexpr unsigned long BOOT_BTN_DEBOUNCE_MS   = 50;
    constexpr unsigned long BOOT_BTN_LONG_PRESS_MS = 2000;  // 2 seconds for debug screen
    constexpr unsigned long PWR_BTN_LONG_PRESS_MS  = 2000;  // Hold 2 seconds to power off

    // Display sleep
    constexpr unsigned long DISPLAY_SLEEP_TIMEOUT_MS = 30000;  // 30 seconds to sleep
}

// ============== LEGACY MACROS (for gradual migration) ==============
// These will be removed once all code uses the namespace versions

#define SCREEN_WIDTH    BoardConfig::SCREEN_WIDTH
#define SCREEN_HEIGHT   BoardConfig::SCREEN_HEIGHT
#define CENTER_X        BoardConfig::CENTER_X
#define CENTER_Y        BoardConfig::CENTER_Y
#define CLOCK_RADIUS    BoardConfig::CLOCK_RADIUS
#define HOUR_HAND_LEN   BoardConfig::HOUR_HAND_LEN
#define MIN_HAND_LEN    BoardConfig::MIN_HAND_LEN
#define SEC_HAND_LEN    BoardConfig::SEC_HAND_LEN
#define CORNER_MARGIN   BoardConfig::CORNER_MARGIN
#define CORNER_TEXT_H   BoardConfig::CORNER_TEXT_H

#define COLOR_BG        Colors::BG
#define COLOR_FACE      Colors::FACE
#define COLOR_HOUR      Colors::HOUR
#define COLOR_MINUTE    Colors::MINUTE
#define COLOR_SECOND    Colors::SECOND
#define COLOR_TEXT      Colors::TEXT
#define COLOR_TEMP      Colors::TEMP
#define COLOR_HUMID     Colors::HUMID
#define COLOR_LIGHT     Colors::LIGHT
#define COLOR_MOTION    Colors::MOTION
#define COLOR_LED       Colors::LED
#define COLOR_TICK      Colors::TICK
#define COLOR_DATE      Colors::DATE

#define TOUCH_DEBOUNCE_MS       Timing::TOUCH_DEBOUNCE_MS
#define TOUCH_COOLDOWN_MS       Timing::TOUCH_COOLDOWN_MS
#define SWIPE_MIN_DISTANCE      Timing::SWIPE_MIN_DISTANCE
#define SWIPE_MAX_CROSS         Timing::SWIPE_MAX_CROSS
#define VOLTAGE_READ_INTERVAL   Timing::VOLTAGE_READ_INTERVAL
#define VOLTAGE_FULL_THRESHOLD  Timing::VOLTAGE_FULL_THRESHOLD
#define VOLTAGE_TREND_THRESHOLD Timing::VOLTAGE_TREND_THRESHOLD
#define BOOT_BTN_DEBOUNCE_MS    Timing::BOOT_BTN_DEBOUNCE_MS
#define BOOT_BTN_LONG_PRESS_MS  Timing::BOOT_BTN_LONG_PRESS_MS
#define PWR_BTN_LONG_PRESS_MS   Timing::PWR_BTN_LONG_PRESS_MS
#define DISPLAY_SLEEP_TIMEOUT_MS Timing::DISPLAY_SLEEP_TIMEOUT_MS

#endif // BOARDCONFIG_H

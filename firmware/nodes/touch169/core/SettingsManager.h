/**
 * SettingsManager - Persistent settings for touch169
 *
 * Centralizes all Preferences-based persistent storage.
 * Extracted from main.cpp as part of Phase R6 refactoring.
 *
 * Usage:
 *   SettingsManager settings;
 *   settings.begin();  // Load from flash
 *
 *   // Use convenience methods:
 *   int timeout = settings.getSleepTimeout();
 *   settings.setTempUnit('F');
 *
 *   // Or generic get/set:
 *   int value = settings.getInt("customKey", 0);
 *   settings.setString("customKey", "value");
 */

#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// Keys for persistent settings
namespace SettingKey {
  constexpr const char* BOOT_COUNT     = "bootCount";
  constexpr const char* BRIGHTNESS     = "brightness";
  constexpr const char* SLEEP_TIMEOUT  = "sleepTimeout";
  constexpr const char* TEMP_UNIT      = "tempUnit";      // 'C' or 'F'
  constexpr const char* CLOCK_FORMAT   = "clockFmt";      // 12 or 24
  constexpr const char* TIMEZONE       = "timezone";      // e.g., "EST", "PST"
  constexpr const char* TZ_OFFSET      = "tzOffset";      // Hours offset from UTC
  // Future: alarm settings, theme, etc.
}

// Default values
namespace SettingDefault {
  constexpr int BRIGHTNESS      = 255;     // Full brightness
  constexpr int SLEEP_TIMEOUT   = 30;      // 30 seconds
  constexpr char TEMP_UNIT      = 'C';     // Celsius
  constexpr int CLOCK_FORMAT    = 24;      // 24-hour format
  constexpr int TZ_OFFSET       = -5;      // EST (UTC-5)
}

/**
 * SettingsManager class - manages persistent settings via Preferences
 *
 * Responsibilities:
 * - Load/save settings from flash
 * - Provide typed accessors with defaults
 * - Track boot count for diagnostics
 * - Centralize all persistent storage
 */
class SettingsManager {
public:
  SettingsManager();

  /**
   * Initialize settings manager and load from flash
   * Must be called before using any settings
   */
  void begin();

  /**
   * Close preferences (optional, called automatically on ESP32)
   */
  void end();

  // ============== Generic Get/Set ==============

  /**
   * Get integer setting
   * @param key Setting key
   * @param defaultVal Default if not found
   */
  int getInt(const char* key, int defaultVal = 0);

  /**
   * Set integer setting
   * @param key Setting key
   * @param value Value to store
   */
  void setInt(const char* key, int value);

  /**
   * Get string setting
   * @param key Setting key
   * @param defaultVal Default if not found
   */
  String getString(const char* key, const String& defaultVal = "");

  /**
   * Set string setting
   * @param key Setting key
   * @param value Value to store
   */
  void setString(const char* key, const String& value);

  // ============== Convenience Methods ==============

  /**
   * Get display brightness (0-255)
   */
  int getBrightness();

  /**
   * Set display brightness (0-255)
   */
  void setBrightness(int brightness);

  /**
   * Get sleep timeout in seconds
   */
  int getSleepTimeout();

  /**
   * Set sleep timeout in seconds
   */
  void setSleepTimeout(int seconds);

  /**
   * Get temperature unit ('C' or 'F')
   */
  char getTempUnit();

  /**
   * Set temperature unit ('C' or 'F')
   */
  void setTempUnit(char unit);

  /**
   * Get clock format (12 or 24)
   */
  int getClockFormat();

  /**
   * Set clock format (12 or 24)
   */
  void setClockFormat(int format);

  /**
   * Get timezone string (e.g., "EST", "PST")
   */
  String getTimezone();

  /**
   * Set timezone string
   */
  void setTimezone(const String& tz);

  /**
   * Get timezone offset in hours from UTC
   */
  int getTimezoneOffset();

  /**
   * Set timezone offset in hours from UTC
   */
  void setTimezoneOffset(int hours);

  /**
   * Get boot count (incremented each time begin() is called)
   */
  int getBootCount();

private:
  Preferences _prefs;
  bool _initialized;
  int _bootCount;

  static constexpr const char* NAMESPACE = "touch169";
};

#endif // SETTINGS_MANAGER_H

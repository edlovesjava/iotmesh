/**
 * SettingsManager - Persistent settings for touch169
 *
 * Implementation of settings persistence via Preferences.
 */

#include "SettingsManager.h"

SettingsManager::SettingsManager()
  : _initialized(false)
  , _bootCount(0)
{
}

void SettingsManager::begin() {
  if (_initialized) return;

  _prefs.begin(NAMESPACE, false);  // false = read/write mode
  _initialized = true;

  // Increment and store boot count
  _bootCount = _prefs.getInt(SettingKey::BOOT_COUNT, 0) + 1;
  _prefs.putInt(SettingKey::BOOT_COUNT, _bootCount);

  Serial.printf("[SETTINGS] Initialized, boot count: %d\n", _bootCount);
}

void SettingsManager::end() {
  if (!_initialized) return;
  _prefs.end();
  _initialized = false;
}

// ============== Generic Get/Set ==============

int SettingsManager::getInt(const char* key, int defaultVal) {
  if (!_initialized) return defaultVal;
  return _prefs.getInt(key, defaultVal);
}

void SettingsManager::setInt(const char* key, int value) {
  if (!_initialized) return;
  _prefs.putInt(key, value);
}

String SettingsManager::getString(const char* key, const String& defaultVal) {
  if (!_initialized) return defaultVal;
  return _prefs.getString(key, defaultVal);
}

void SettingsManager::setString(const char* key, const String& value) {
  if (!_initialized) return;
  _prefs.putString(key, value);
}

// ============== Convenience Methods ==============

int SettingsManager::getBrightness() {
  return getInt(SettingKey::BRIGHTNESS, SettingDefault::BRIGHTNESS);
}

void SettingsManager::setBrightness(int brightness) {
  // Clamp to valid range
  if (brightness < 0) brightness = 0;
  if (brightness > 255) brightness = 255;
  setInt(SettingKey::BRIGHTNESS, brightness);
}

int SettingsManager::getSleepTimeout() {
  return getInt(SettingKey::SLEEP_TIMEOUT, SettingDefault::SLEEP_TIMEOUT);
}

void SettingsManager::setSleepTimeout(int seconds) {
  // Minimum 5 seconds, max 10 minutes
  if (seconds < 5) seconds = 5;
  if (seconds > 600) seconds = 600;
  setInt(SettingKey::SLEEP_TIMEOUT, seconds);
}

char SettingsManager::getTempUnit() {
  String unit = getString(SettingKey::TEMP_UNIT, String(SettingDefault::TEMP_UNIT));
  return (unit.length() > 0) ? unit.charAt(0) : SettingDefault::TEMP_UNIT;
}

void SettingsManager::setTempUnit(char unit) {
  // Only allow C or F
  if (unit != 'C' && unit != 'F') unit = 'C';
  setString(SettingKey::TEMP_UNIT, String(unit));
}

int SettingsManager::getClockFormat() {
  return getInt(SettingKey::CLOCK_FORMAT, SettingDefault::CLOCK_FORMAT);
}

void SettingsManager::setClockFormat(int format) {
  // Only allow 12 or 24
  if (format != 12 && format != 24) format = 24;
  setInt(SettingKey::CLOCK_FORMAT, format);
}

String SettingsManager::getTimezone() {
  return getString(SettingKey::TIMEZONE, "EST");
}

void SettingsManager::setTimezone(const String& tz) {
  setString(SettingKey::TIMEZONE, tz);
}

int SettingsManager::getTimezoneOffset() {
  return getInt(SettingKey::TZ_OFFSET, SettingDefault::TZ_OFFSET);
}

void SettingsManager::setTimezoneOffset(int hours) {
  // Valid range: UTC-12 to UTC+14
  if (hours < -12) hours = -12;
  if (hours > 14) hours = 14;
  setInt(SettingKey::TZ_OFFSET, hours);
}

int SettingsManager::getBootCount() {
  return _bootCount;
}

/**
 * Navigator - Screen navigation state machine for touch169
 *
 * Implementation of screen navigation logic.
 */

#include "Navigator.h"

Navigator::Navigator()
  : _current(Screen::Clock)
  , _previous(Screen::Clock)
  , _changed(true)  // Force initial draw
  , _transitionTime(0)
{
}

const char* Navigator::getScreenName(Screen screen) {
  switch (screen) {
    case Screen::Clock:           return "Clock";
    case Screen::Humidity:        return "Humidity";
    case Screen::HumiditySettings: return "Humidity Settings";
    case Screen::Temperature:     return "Temperature";
    case Screen::TempSettings:    return "Temp Settings";
    case Screen::Light:           return "Light";
    case Screen::LightSettings:   return "Light Settings";
    case Screen::MotionLed:       return "Motion/LED";
    case Screen::MotionLedSettings: return "Motion/LED Settings";
    case Screen::Calendar:        return "Calendar";
    case Screen::DateSettings:    return "Date Settings";
    case Screen::ClockDetails:    return "Clock Details";
    case Screen::TimeSettings:    return "Time Settings";
    case Screen::Alarm:           return "Alarm";
    case Screen::Stopwatch:       return "Stopwatch";
    case Screen::NavMenu:         return "Navigation";
    case Screen::Debug:           return "Debug";
    default:                      return "Unknown";
  }
}

Screen Navigator::getParent(Screen screen) const {
  // Define parent screen for each screen per navigation spec
  switch (screen) {
    // Sensor detail screens -> Clock
    case Screen::Humidity:
    case Screen::Temperature:
    case Screen::Light:
    case Screen::MotionLed:
    case Screen::Calendar:
    case Screen::ClockDetails:
      return Screen::Clock;

    // Settings screens -> their parent detail screen
    case Screen::HumiditySettings:
      return Screen::Humidity;
    case Screen::TempSettings:
      return Screen::Temperature;
    case Screen::LightSettings:
      return Screen::Light;
    case Screen::MotionLedSettings:
      return Screen::MotionLed;
    case Screen::DateSettings:
      return Screen::Calendar;
    case Screen::TimeSettings:
      return Screen::ClockDetails;

    // Sub-screens of Clock Details
    case Screen::Alarm:
    case Screen::Stopwatch:
      return Screen::ClockDetails;

    // Nav menu -> previous screen
    case Screen::NavMenu:
      return _previous;

    // Debug -> Clock
    case Screen::Debug:
      return Screen::Clock;

    // Clock has no parent (root)
    case Screen::Clock:
    default:
      return Screen::Clock;
  }
}

void Navigator::navigateTo(Screen screen) {
  if (screen == _current) return;

  // Save current screen for back navigation (except when going to nav menu)
  if (screen != Screen::NavMenu) {
    _previous = _current;
  }

  Serial.printf("[NAV] %s -> %s\n", getScreenName(_current), getScreenName(screen));

  _current = screen;
  _changed = true;
  _transitionTime = millis();
}

void Navigator::navigateBack() {
  Screen parent = getParent(_current);

  // If we're at the clock (root), do nothing
  if (_current == Screen::Clock) {
    Serial.println("[NAV] Already at root (Clock)");
    return;
  }

  Serial.printf("[NAV] Back: %s -> %s\n", getScreenName(_current), getScreenName(parent));

  _current = parent;
  _changed = true;
  _transitionTime = millis();
}

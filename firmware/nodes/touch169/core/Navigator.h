/**
 * Navigator - Screen navigation state machine for touch169
 *
 * Manages screen transitions, parent-child relationships, and back navigation.
 * Extracted from main.cpp as part of Phase R4 refactoring.
 *
 * Usage:
 *   Navigator nav;
 *   nav.navigateTo(Screen::Humidity);
 *   if (nav.hasChanged()) {
 *     // redraw screen
 *     nav.clearChanged();
 *   }
 *   nav.navigateBack();  // returns to parent screen
 */

#ifndef NAVIGATOR_H
#define NAVIGATOR_H

#include <Arduino.h>

// ============== SCREEN ENUM ==============
// From touch169_touch_navigation_spec.md Section 6
enum class Screen {
  Clock,               // Main clock face
  Humidity,            // Humidity detail
  HumiditySettings,    // Humidity zone settings
  Temperature,         // Temperature detail
  TempSettings,        // Temperature zone settings
  Light,               // Light detail
  LightSettings,       // Light zone settings
  MotionLed,           // Motion & LED control
  MotionLedSettings,   // Motion/LED zone settings
  Calendar,            // Monthly calendar view
  DateSettings,        // Set current date
  ClockDetails,        // Clock details menu
  TimeSettings,        // Set time and timezone
  Alarm,               // Alarm settings
  Stopwatch,           // Stopwatch
  NavMenu,             // Navigation menu overlay
  Debug                // Debug info
};

/**
 * Navigator class - manages screen navigation state
 *
 * Responsibilities:
 * - Track current and previous screen
 * - Define parent-child screen relationships
 * - Handle forward navigation (navigateTo)
 * - Handle back navigation (navigateBack)
 * - Track screen change flag for rendering
 */
class Navigator {
public:
  Navigator();

  // Get current screen
  Screen current() const { return _current; }

  // Get previous screen (for NavMenu return)
  Screen previous() const { return _previous; }

  // Navigate to a specific screen
  void navigateTo(Screen screen);

  // Navigate back to parent screen
  void navigateBack();

  // Get parent screen for a given screen
  Screen getParent(Screen screen) const;

  // Get human-readable screen name
  static const char* getScreenName(Screen screen);

  // Screen change tracking
  bool hasChanged() const { return _changed; }
  void clearChanged() { _changed = false; }
  void markChanged() { _changed = true; }

  // Screen transition timing (for touch cooldown)
  unsigned long getTransitionTime() const { return _transitionTime; }

private:
  Screen _current;
  Screen _previous;
  bool _changed;
  unsigned long _transitionTime;
};

#endif // NAVIGATOR_H

/**
 * InputManager - Unified input handling for touch and buttons
 *
 * Combines touch input, gesture detection, and boot button handling
 * into a single manager. Fires callbacks for different input events.
 *
 * Extracted from main.cpp as part of Phase R10 refactoring.
 *
 * Usage:
 *   InputManager input(touchInput, gesture);
 *   input.onTap([](int16_t x, int16_t y) { ... });
 *   input.onSwipe([](SwipeDirection dir) { ... });
 *   input.onBootShortPress([]() { ... });
 *   input.onBootLongPress([]() { ... });
 *   input.begin();
 *
 *   // In loop:
 *   input.update();
 */

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "../hardware/TouchInput.h"
#include "../hardware/GestureDetector.h"
#include "../BoardConfig.h"

// Swipe direction for callback (simpler than Gesture enum)
enum class SwipeDirection {
  None,
  Up,
  Down,
  Left,
  Right
};

// Callback types
typedef void (*TapCallback)(int16_t x, int16_t y);
typedef void (*SwipeCallback)(SwipeDirection dir);
typedef void (*ButtonCallback)();

class InputManager {
public:
  /**
   * Constructor
   * @param touch Reference to TouchInput instance
   * @param gesture Reference to GestureDetector instance
   */
  InputManager(TouchInput& touch, GestureDetector& gesture);

  /**
   * Initialize input handling
   * @param bootPin GPIO pin for boot button (default BOOT_BTN_PIN)
   */
  void begin(int bootPin = BOOT_BTN_PIN);

  /**
   * Update input state - call from main loop
   * Reads touch, detects gestures, checks boot button
   */
  void update();

  // === Callback setters ===

  /** Called on tap gesture with coordinates */
  void onTap(TapCallback cb) { _tapCallback = cb; }

  /** Called on swipe gesture with direction */
  void onSwipe(SwipeCallback cb) { _swipeCallback = cb; }

  /** Called on boot button short press */
  void onBootShortPress(ButtonCallback cb) { _bootShortCallback = cb; }

  /** Called on boot button long press (2 seconds) */
  void onBootLongPress(ButtonCallback cb) { _bootLongCallback = cb; }

  /** Called when any touch occurs (for wake/activity reset) */
  void onTouch(ButtonCallback cb) { _touchCallback = cb; }

  // === State queries ===

  /** Check if touch is currently active */
  bool isTouched() const { return _touchActive; }

  /** Get last touch X coordinate */
  int16_t getTouchX() const { return _touchX; }

  /** Get last touch Y coordinate */
  int16_t getTouchY() const { return _touchY; }

  /** Check if boot button short press occurred (clears flag) */
  bool consumeBootShortPress();

  /** Check if in touch cooldown period */
  bool isInCooldown() const;

  /** Start cooldown period (call after screen transitions) */
  void startCooldown();

  /** Cancel current touch sequence (call when waking from sleep) */
  void cancelTouch();

  // === Configuration ===

  /** Set touch debounce time in ms */
  void setDebounceMs(unsigned long ms) { _debounceMs = ms; }

  /** Set cooldown time after screen transitions */
  void setCooldownMs(unsigned long ms) { _cooldownMs = ms; }

  /** Set boot button long press threshold */
  void setLongPressMs(unsigned long ms) { _longPressMs = ms; }

private:
  void handleTouch();
  void handleBootButton();

  TouchInput& _touch;
  GestureDetector& _gesture;

  // Boot button state
  int _bootPin;
  unsigned long _bootPressTime;
  bool _bootWasPressed;
  bool _bootLongFired;
  bool _bootShortPending;

  // Touch state
  bool _touchActive;
  bool _wasTouched;
  int16_t _touchX;
  int16_t _touchY;
  unsigned long _lastTouchTime;
  unsigned long _cooldownStart;

  // Timing configuration
  unsigned long _debounceMs;
  unsigned long _cooldownMs;
  unsigned long _longPressMs;
  unsigned long _bootDebounceMs;

  // Callbacks
  TapCallback _tapCallback;
  SwipeCallback _swipeCallback;
  ButtonCallback _bootShortCallback;
  ButtonCallback _bootLongCallback;
  ButtonCallback _touchCallback;
};

#endif // INPUT_MANAGER_H

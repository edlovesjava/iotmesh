/**
 * PowerManager - Power button handling and power-off logic
 *
 * Manages the power latch circuit on Waveshare ESP32-S3-Touch-LCD-1.69.
 * The board has a power latch that must be held HIGH to stay powered on battery.
 * Long-pressing the power button triggers power off.
 *
 * Extracted from main.cpp as part of Phase R9 refactoring.
 *
 * Usage:
 *   PowerManager power(PWR_EN_PIN, PWR_BTN_PIN);
 *   power.begin();  // Call FIRST in setup() to latch power
 *
 *   // In loop:
 *   power.update();
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "../BoardConfig.h"

/**
 * Callback type for power off events
 * Called just before power is released, allowing cleanup/display message
 */
typedef void (*PowerOffCallback)();

/**
 * PowerManager class - handles power latch and power button
 *
 * Responsibilities:
 * - Latch power on startup (must call begin() first!)
 * - Monitor power button for long press
 * - Trigger power off sequence
 */
class PowerManager {
public:
  /**
   * Constructor
   * @param enablePin GPIO pin for power latch output
   * @param buttonPin GPIO pin for power button input
   * @param longPressMs Duration in ms for long press to trigger power off
   */
  PowerManager(int enablePin = PWR_EN_PIN,
               int buttonPin = PWR_BTN_PIN,
               unsigned long longPressMs = PWR_BTN_LONG_PRESS_MS);

  /**
   * Initialize power management
   * CRITICAL: Call this FIRST in setup() to latch power on battery operation
   */
  void begin();

  /**
   * Update power button state
   * Call from main loop
   */
  void update();

  /**
   * Set callback for power off event
   * Callback is invoked just before power is released
   */
  void onPowerOff(PowerOffCallback cb) { _powerOffCallback = cb; }

  /**
   * Trigger power off sequence
   * Can be called manually or triggered by long press
   */
  void powerOff();

  /**
   * Check if power button is currently pressed
   */
  bool isButtonPressed() const { return _buttonPressed; }

  /**
   * Get time button has been held (0 if not pressed)
   */
  unsigned long getButtonHeldTime() const;

  /**
   * Check if we're running on USB power
   * Returns true if powerOff() was called but device is still running
   */
  bool isUsbPowered() const { return _usbPowered; }

private:
  int _enablePin;
  int _buttonPin;
  unsigned long _longPressMs;

  bool _buttonPressed;
  unsigned long _buttonPressTime;
  bool _wasPressed;
  bool _usbPowered;

  PowerOffCallback _powerOffCallback;
};

#endif // POWER_MANAGER_H

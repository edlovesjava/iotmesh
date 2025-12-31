/**
 * TouchInput - Wrapper for CST816T touch controller
 *
 * Provides a clean interface to the touch hardware, abstracting
 * the SensorLib TouchClassCST816 driver.
 *
 * Extracted from main.cpp as part of Phase R10 refactoring.
 *
 * Usage:
 *   TouchInput touch;
 *   touch.begin(Wire, TOUCH_SDA, TOUCH_SCL);
 *
 *   // In loop:
 *   if (touch.read()) {
 *     int16_t x = touch.getX();
 *     int16_t y = touch.getY();
 *   }
 */

#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include <Arduino.h>
#include <Wire.h>
#include "touch/TouchClassCST816.h"
#include "BoardConfig.h"

class TouchInput {
public:
  TouchInput();

  /**
   * Initialize the touch controller
   * @param wire Wire instance for I2C
   * @param sda I2C data pin
   * @param scl I2C clock pin
   * @param addr I2C address (default 0x15 for CST816T)
   * @return true if initialization succeeded
   */
  bool begin(TwoWire& wire, int sda = TOUCH_SDA, int scl = TOUCH_SCL, uint8_t addr = 0x15);

  /**
   * Read touch state
   * @return true if screen is being touched
   */
  bool read();

  /**
   * Check if touch controller is initialized
   */
  bool isInitialized() const { return _initialized; }

  /**
   * Check if screen is currently being touched
   */
  bool isTouched() const { return _touched; }

  /**
   * Get X coordinate of last touch (valid after read() returns true)
   */
  int16_t getX() const { return _x; }

  /**
   * Get Y coordinate of last touch (valid after read() returns true)
   */
  int16_t getY() const { return _y; }

  /**
   * Get model name of touch controller
   */
  const char* getModelName();

private:
  TouchClassCST816 _touch;
  bool _initialized;
  bool _touched;
  int16_t _x;
  int16_t _y;
};

#endif // TOUCH_INPUT_H

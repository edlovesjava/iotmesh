/**
 * TouchInput - Wrapper for CST816T touch controller
 */

#include "TouchInput.h"

TouchInput::TouchInput()
  : _initialized(false)
  , _touched(false)
  , _x(-1)
  , _y(-1)
{
}

bool TouchInput::begin(TwoWire& wire, int sda, int scl, uint8_t addr) {
  if (!_touch.begin(wire, addr, sda, scl)) {
    Serial.println("[TOUCH] CST816T not found");
    return false;
  }

  Serial.printf("[TOUCH] Initialized: %s\n", _touch.getModelName());
  _initialized = true;
  return true;
}

bool TouchInput::read() {
  if (!_initialized) {
    _touched = false;
    return false;
  }

  uint8_t points = _touch.getPoint(&_x, &_y, 1);
  _touched = (points > 0);
  return _touched;
}

const char* TouchInput::getModelName() {
  if (!_initialized) return "Not initialized";
  return _touch.getModelName();
}

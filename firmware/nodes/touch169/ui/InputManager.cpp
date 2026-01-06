/**
 * InputManager - Unified input handling for touch and buttons
 */

#include "InputManager.h"

InputManager::InputManager(TouchInput& touch, GestureDetector& gesture)
  : _touch(touch)
  , _gesture(gesture)
  , _bootPin(BOOT_BTN_PIN)
  , _bootPressTime(0)
  , _bootWasPressed(false)
  , _bootLongFired(false)
  , _bootShortPending(false)
  , _touchActive(false)
  , _wasTouched(false)
  , _touchX(-1)
  , _touchY(-1)
  , _lastTouchTime(0)
  , _cooldownStart(0)
  , _debounceMs(TOUCH_DEBOUNCE_MS)
  , _cooldownMs(TOUCH_COOLDOWN_MS)
  , _longPressMs(BOOT_BTN_LONG_PRESS_MS)
  , _bootDebounceMs(BOOT_BTN_DEBOUNCE_MS)
  , _tapCallback(nullptr)
  , _swipeCallback(nullptr)
  , _bootShortCallback(nullptr)
  , _bootLongCallback(nullptr)
  , _touchCallback(nullptr)
{
}

void InputManager::begin(int bootPin) {
  _bootPin = bootPin;
  pinMode(_bootPin, INPUT_PULLUP);
  Serial.println("[INPUT] InputManager initialized");
}

void InputManager::update() {
  handleTouch();
  handleBootButton();
}

void InputManager::handleTouch() {
  bool touched = _touch.read();
  unsigned long now = millis();

  if (touched) {
    _touchX = _touch.getX();
    _touchY = _touch.getY();

    // Notify on any touch (for wake/activity)
    if (_touchCallback) {
      _touchCallback();
    }

    // Track touch start for gesture detection
    if (!_wasTouched) {
      _gesture.onTouchStart(_touchX, _touchY);
      Serial.printf("[INPUT] Touch start x=%d, y=%d\n", _touchX, _touchY);
    }

    // Debounce check
    if (now - _lastTouchTime < _debounceMs) return;
    _lastTouchTime = now;

    // Cooldown check (after screen transitions)
    if (isInCooldown()) {
      Serial.printf("[INPUT] Touch ignored (cooldown) x=%d, y=%d\n", _touchX, _touchY);
      return;
    }

    _touchActive = true;

  } else if (_wasTouched) {
    // Touch released - detect gesture
    _gesture.onTouchEnd(_touchX, _touchY);
    _touchActive = false;

    Gesture gestureType = _gesture.getGesture();

    switch (gestureType) {
      case Gesture::SwipeUp:
      case Gesture::SwipeDown:
      case Gesture::SwipeLeft:
      case Gesture::SwipeRight:
        if (_swipeCallback) {
          SwipeDirection dir;
          switch (gestureType) {
            case Gesture::SwipeUp:    dir = SwipeDirection::Up; break;
            case Gesture::SwipeDown:  dir = SwipeDirection::Down; break;
            case Gesture::SwipeLeft:  dir = SwipeDirection::Left; break;
            case Gesture::SwipeRight: dir = SwipeDirection::Right; break;
            default: dir = SwipeDirection::None; break;
          }
          Serial.printf("[INPUT] Swipe detected: %d\n", (int)dir);
          _swipeCallback(dir);
        }
        break;

      case Gesture::Tap:
        if (_tapCallback) {
          int16_t tapX = _gesture.getTapX();
          int16_t tapY = _gesture.getTapY();
          Serial.printf("[INPUT] Tap detected at x=%d, y=%d\n", tapX, tapY);
          _tapCallback(tapX, tapY);
        }
        break;

      default:
        break;
    }

    _gesture.reset();
  }

  _wasTouched = touched;
}

void InputManager::handleBootButton() {
  bool pressed = digitalRead(_bootPin) == LOW;  // Active low (has pullup)
  unsigned long now = millis();

  if (pressed && !_bootWasPressed) {
    // Button just pressed
    _bootPressTime = now;
    _bootWasPressed = true;
    _bootLongFired = false;

  } else if (pressed && _bootWasPressed && !_bootLongFired) {
    // Button held - check for long press
    if (now - _bootPressTime >= _longPressMs) {
      _bootLongFired = true;
      Serial.println("[INPUT] Boot button long press");
      if (_bootLongCallback) {
        _bootLongCallback();
      }
    }

  } else if (!pressed && _bootWasPressed) {
    // Button released
    unsigned long duration = now - _bootPressTime;
    if (!_bootLongFired && duration >= _bootDebounceMs) {
      _bootShortPending = true;
      Serial.println("[INPUT] Boot button short press");
      if (_bootShortCallback) {
        _bootShortCallback();
      }
    }
    _bootWasPressed = false;
    _bootLongFired = false;
  }
}

bool InputManager::consumeBootShortPress() {
  if (_bootShortPending) {
    _bootShortPending = false;
    return true;
  }
  return false;
}

bool InputManager::isInCooldown() const {
  if (_cooldownStart == 0) return false;
  return (millis() - _cooldownStart) < _cooldownMs;
}

void InputManager::startCooldown() {
  _cooldownStart = millis();
}

void InputManager::cancelTouch() {
  _gesture.reset();
  _wasTouched = false;
  _touchActive = false;
}

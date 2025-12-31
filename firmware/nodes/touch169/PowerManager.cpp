/**
 * PowerManager - Power button handling and power-off logic
 *
 * Implementation of power management for Waveshare ESP32-S3-Touch-LCD-1.69.
 */

#include "PowerManager.h"

PowerManager::PowerManager(int enablePin, int buttonPin, unsigned long longPressMs)
  : _enablePin(enablePin)
  , _buttonPin(buttonPin)
  , _longPressMs(longPressMs)
  , _buttonPressed(false)
  , _buttonPressTime(0)
  , _wasPressed(false)
  , _usbPowered(false)
  , _powerOffCallback(nullptr)
{
}

void PowerManager::begin() {
  // CRITICAL: Latch power immediately to stay on when running from battery
  pinMode(_enablePin, OUTPUT);
  digitalWrite(_enablePin, HIGH);

  // Setup power button input (active low)
  pinMode(_buttonPin, INPUT);

  Serial.println("[POWER] PowerManager initialized, power latched");
}

void PowerManager::update() {
  bool pressed = digitalRead(_buttonPin) == LOW;  // Active low
  unsigned long now = millis();

  if (pressed && !_wasPressed) {
    // Button just pressed
    _buttonPressTime = now;
    _wasPressed = true;
    _buttonPressed = true;
  } else if (pressed && _wasPressed) {
    // Button held - check for long press to power off
    if (now - _buttonPressTime >= _longPressMs) {
      powerOff();
    }
  } else if (!pressed && _wasPressed) {
    // Button released
    _wasPressed = false;
    _buttonPressed = false;
  }
}

void PowerManager::powerOff() {
  Serial.println("[POWER] Powering off...");

  // Call user callback for cleanup/display message
  if (_powerOffCallback != nullptr) {
    _powerOffCallback();
  }

  // Release power latch - this will cut power on battery
  digitalWrite(_enablePin, LOW);

  // If on USB power, we won't actually power off, so just wait
  delay(2000);

  // If still running (USB powered), mark it and restart
  _usbPowered = true;
  Serial.println("[POWER] Still powered (USB?), restarting...");
  ESP.restart();
}

unsigned long PowerManager::getButtonHeldTime() const {
  if (!_wasPressed) {
    return 0;
  }
  return millis() - _buttonPressTime;
}

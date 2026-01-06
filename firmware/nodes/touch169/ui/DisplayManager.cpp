/**
 * DisplayManager - Screen routing and display management
 *
 * Implementation of display management and screen routing.
 */

#include "DisplayManager.h"

DisplayManager::DisplayManager(TFT_eSPI& tft, Navigator& nav, int backlightPin)
  : _tft(tft)
  , _nav(nav)
  , _backlightPin(backlightPin)
  , _screenCount(0)
  , _asleep(false)
  , _lastActivityTime(0)
  , _sleepTimeoutMs(DISPLAY_SLEEP_TIMEOUT_MS)
  , _lastScreen(Screen::Clock)
  , _fallbackRenderer(nullptr)
  , _fallbackTouchHandler(nullptr)
{
  // Initialize screen registry
  for (int i = 0; i < MAX_SCREENS; i++) {
    _screens[i] = nullptr;
  }
}

void DisplayManager::begin() {
  _lastActivityTime = millis();
  _lastScreen = _nav.current();
  Serial.println("[DISPLAY] DisplayManager initialized");
}

bool DisplayManager::registerScreen(ScreenRenderer* screen) {
  if (screen == nullptr || _screenCount >= MAX_SCREENS) {
    return false;
  }

  _screens[_screenCount++] = screen;
  Serial.printf("[DISPLAY] Registered screen: %s\n",
                Navigator::getScreenName(screen->getScreen()));
  return true;
}

void DisplayManager::render() {
  if (_asleep) return;

  Screen current = _nav.current();

  // Handle screen transition
  if (current != _lastScreen) {
    handleScreenTransition(_lastScreen, current);
    _lastScreen = current;
  }

  // Find renderer for current screen
  ScreenRenderer* renderer = findRenderer(current);

  if (renderer != nullptr) {
    bool forceRedraw = _nav.hasChanged();
    if (forceRedraw) {
      _nav.clearChanged();
    }
    renderer->render(_tft, forceRedraw);
  } else if (_fallbackRenderer != nullptr) {
    // Use fallback for screens not yet migrated
    _fallbackRenderer(current, _tft, _nav);
  }
}

bool DisplayManager::handleTouch(int16_t x, int16_t y) {
  if (_asleep) {
    wake();
    return true;  // Touch consumed by wake
  }

  resetActivityTimer();

  Screen current = _nav.current();
  ScreenRenderer* renderer = findRenderer(current);

  if (renderer != nullptr) {
    return renderer->handleTouch(x, y, _nav);
  } else if (_fallbackTouchHandler != nullptr) {
    return _fallbackTouchHandler(current, x, y, _nav);
  }

  return false;
}

void DisplayManager::sleep() {
  if (_asleep) return;

  _asleep = true;
  Serial.println("[DISPLAY] Going to sleep...");

  // Turn off backlight
  digitalWrite(_backlightPin, LOW);
}

void DisplayManager::wake() {
  if (!_asleep) return;

  _asleep = false;
  Serial.println("[DISPLAY] Waking up...");

  // Turn on backlight
  digitalWrite(_backlightPin, HIGH);

  // Force screen redraw
  _nav.markChanged();

  // Reset activity timer
  resetActivityTimer();
}

void DisplayManager::resetActivityTimer() {
  _lastActivityTime = millis();
}

void DisplayManager::checkSleepTimeout() {
  if (!_asleep && (millis() - _lastActivityTime >= _sleepTimeoutMs)) {
    sleep();
  }
}

void DisplayManager::setSleepTimeout(unsigned long timeoutMs) {
  _sleepTimeoutMs = timeoutMs;
}

ScreenRenderer* DisplayManager::findRenderer(Screen screen) {
  for (int i = 0; i < _screenCount; i++) {
    if (_screens[i] != nullptr && _screens[i]->getScreen() == screen) {
      return _screens[i];
    }
  }
  return nullptr;
}

void DisplayManager::handleScreenTransition(Screen from, Screen to) {
  // Call onExit for previous screen
  ScreenRenderer* fromRenderer = findRenderer(from);
  if (fromRenderer != nullptr) {
    fromRenderer->onExit();
  }

  // Call onEnter for new screen
  ScreenRenderer* toRenderer = findRenderer(to);
  if (toRenderer != nullptr) {
    toRenderer->onEnter();
  }
}

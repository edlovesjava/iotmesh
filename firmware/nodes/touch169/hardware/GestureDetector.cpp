/**
 * GestureDetector - Touch gesture detection for touch169
 *
 * Implementation of gesture detection logic.
 */

#include "GestureDetector.h"

GestureDetector::GestureDetector(int16_t minSwipeDistance, int16_t maxCrossDistance)
  : _startX(0)
  , _startY(0)
  , _endX(0)
  , _endY(0)
  , _active(false)
  , _gesture(Gesture::None)
  , _minSwipeDistance(minSwipeDistance)
  , _maxCrossDistance(maxCrossDistance)
{
}

void GestureDetector::onTouchStart(int16_t x, int16_t y) {
  _startX = x;
  _startY = y;
  _endX = x;
  _endY = y;
  _active = true;
  _gesture = Gesture::None;
}

void GestureDetector::onTouchEnd(int16_t x, int16_t y) {
  if (!_active) return;

  _endX = x;
  _endY = y;
  _active = false;

  classifyGesture();
}

void GestureDetector::reset() {
  _startX = 0;
  _startY = 0;
  _endX = 0;
  _endY = 0;
  _active = false;
  _gesture = Gesture::None;
}

void GestureDetector::classifyGesture() {
  int16_t deltaX = _endX - _startX;
  int16_t deltaY = _endY - _startY;

  // Check for vertical swipe (up or down)
  if (abs(deltaY) >= _minSwipeDistance && abs(deltaX) < _maxCrossDistance) {
    if (deltaY > 0) {
      _gesture = Gesture::SwipeDown;
    } else {
      _gesture = Gesture::SwipeUp;
    }
    return;
  }

  // Check for horizontal swipe (left or right)
  if (abs(deltaX) >= _minSwipeDistance && abs(deltaY) < _maxCrossDistance) {
    if (deltaX > 0) {
      _gesture = Gesture::SwipeRight;
    } else {
      _gesture = Gesture::SwipeLeft;
    }
    return;
  }

  // Not a swipe - classify as tap
  _gesture = Gesture::Tap;
}

const char* GestureDetector::getGestureName(Gesture gesture) {
  switch (gesture) {
    case Gesture::None:       return "None";
    case Gesture::Tap:        return "Tap";
    case Gesture::SwipeUp:    return "SwipeUp";
    case Gesture::SwipeDown:  return "SwipeDown";
    case Gesture::SwipeLeft:  return "SwipeLeft";
    case Gesture::SwipeRight: return "SwipeRight";
    default:                  return "Unknown";
  }
}

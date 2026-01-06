/**
 * GestureDetector - Touch gesture detection for touch169
 *
 * Detects swipe gestures (up, down, left, right) and taps from touch input.
 * Extracted from main.cpp as part of Phase R5 refactoring.
 *
 * Usage:
 *   GestureDetector gesture;
 *   // or with custom thresholds:
 *   GestureDetector gesture(60, 30);  // minSwipe=60, maxCross=30
 *
 *   // On touch start:
 *   gesture.onTouchStart(x, y);
 *
 *   // On touch end:
 *   gesture.onTouchEnd(x, y);
 *   Gesture g = gesture.getGesture();
 *   if (g == Gesture::SwipeDown) { ... }
 *   if (g == Gesture::Tap) {
 *     int16_t tapX = gesture.getTapX();
 *     int16_t tapY = gesture.getTapY();
 *   }
 *
 *   // Reset for next gesture:
 *   gesture.reset();
 */

#ifndef GESTURE_DETECTOR_H
#define GESTURE_DETECTOR_H

#include <Arduino.h>

// Gesture types
enum class Gesture {
  None,        // No gesture detected yet
  Tap,         // Single tap (touch without significant movement)
  SwipeUp,     // Swipe upward
  SwipeDown,   // Swipe downward
  SwipeLeft,   // Swipe left
  SwipeRight   // Swipe right
};

/**
 * GestureDetector class - detects swipe and tap gestures
 *
 * Responsibilities:
 * - Track touch start and end positions
 * - Classify gesture based on movement distance and direction
 * - Provide tap coordinates when gesture is a tap
 */
class GestureDetector {
public:
  /**
   * Constructor with configurable thresholds
   * @param minSwipeDistance Minimum pixels to count as swipe (default 50)
   * @param maxCrossDistance Max perpendicular movement for swipe (default 40)
   */
  GestureDetector(int16_t minSwipeDistance = 50, int16_t maxCrossDistance = 40);

  /**
   * Call when touch begins
   * @param x Touch X coordinate
   * @param y Touch Y coordinate
   */
  void onTouchStart(int16_t x, int16_t y);

  /**
   * Call when touch ends - triggers gesture detection
   * @param x Final touch X coordinate
   * @param y Final touch Y coordinate
   */
  void onTouchEnd(int16_t x, int16_t y);

  /**
   * Reset state for next gesture
   */
  void reset();

  /**
   * Get detected gesture after onTouchEnd()
   * @return Gesture enum value
   */
  Gesture getGesture() const { return _gesture; }

  /**
   * Get tap X coordinate (valid when gesture is Tap)
   * Returns the touch start position for accuracy
   */
  int16_t getTapX() const { return _startX; }

  /**
   * Get tap Y coordinate (valid when gesture is Tap)
   * Returns the touch start position for accuracy
   */
  int16_t getTapY() const { return _startY; }

  /**
   * Check if a touch sequence is active
   */
  bool isActive() const { return _active; }

  /**
   * Get human-readable gesture name
   */
  static const char* getGestureName(Gesture gesture);

private:
  int16_t _startX;
  int16_t _startY;
  int16_t _endX;
  int16_t _endY;
  bool _active;
  Gesture _gesture;

  // Thresholds
  int16_t _minSwipeDistance;
  int16_t _maxCrossDistance;

  // Classify gesture based on start/end positions
  void classifyGesture();
};

#endif // GESTURE_DETECTOR_H

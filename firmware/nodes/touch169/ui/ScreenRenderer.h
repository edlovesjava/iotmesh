/**
 * ScreenRenderer - Abstract base class for screen rendering
 *
 * Defines the interface for all screen implementations.
 * Each screen is responsible for rendering and handling touch events.
 *
 * Extracted from main.cpp as part of Phase R8 refactoring.
 *
 * Usage:
 *   class ClockScreen : public ScreenRenderer {
 *   public:
 *     void render(TFT_eSPI& tft, bool forceRedraw) override;
 *     bool handleTouch(int16_t x, int16_t y, Navigator& nav) override;
 *     Screen getScreen() const override { return Screen::Clock; }
 *   };
 */

#ifndef SCREEN_RENDERER_H
#define SCREEN_RENDERER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../core/Navigator.h"

/**
 * ScreenRenderer abstract base class
 *
 * Responsibilities:
 * - Define interface for screen rendering
 * - Define interface for touch handling
 * - Provide screen identification
 */
class ScreenRenderer {
public:
  virtual ~ScreenRenderer() = default;

  /**
   * Render the screen
   * @param tft TFT display instance
   * @param forceRedraw If true, redraw everything regardless of change detection
   */
  virtual void render(TFT_eSPI& tft, bool forceRedraw) = 0;

  /**
   * Handle touch event on this screen
   * @param x Touch X coordinate
   * @param y Touch Y coordinate
   * @param nav Navigator for screen transitions
   * @return true if touch was handled, false otherwise
   */
  virtual bool handleTouch(int16_t x, int16_t y, Navigator& nav) = 0;

  /**
   * Get the screen type this renderer handles
   */
  virtual Screen getScreen() const = 0;

  /**
   * Called when screen becomes active (navigated to)
   * Override to perform initialization
   */
  virtual void onEnter() {}

  /**
   * Called when screen becomes inactive (navigated away)
   * Override to perform cleanup
   */
  virtual void onExit() {}

  /**
   * Check if screen needs a full redraw
   * Override if screen tracks its own dirty state
   */
  virtual bool needsRedraw() const { return false; }

  /**
   * Clear the redraw flag after rendering
   */
  virtual void clearRedraw() {}
};

#endif // SCREEN_RENDERER_H

/**
 * DisplayManager - Screen routing and display management
 *
 * Routes rendering to the appropriate ScreenRenderer based on current screen.
 * Manages display sleep/wake and screen transitions.
 *
 * Extracted from main.cpp as part of Phase R8 refactoring.
 *
 * Usage:
 *   DisplayManager display(tft, navigator);
 *   display.registerScreen(new ClockScreen(...));
 *   display.registerScreen(new DebugScreen(...));
 *   display.begin();
 *
 *   // In loop:
 *   display.render();
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Navigator.h"
#include "ScreenRenderer.h"
#include "BoardConfig.h"

/**
 * DisplayManager class - manages screen rendering and display state
 *
 * Responsibilities:
 * - Route rendering to appropriate ScreenRenderer
 * - Manage display sleep/wake
 * - Track activity for sleep timeout
 * - Handle screen transitions
 */
class DisplayManager {
public:
  /**
   * Constructor
   * @param tft TFT display instance
   * @param nav Navigator instance for screen state
   * @param backlightPin GPIO pin for backlight control
   */
  DisplayManager(TFT_eSPI& tft, Navigator& nav, int backlightPin = TFT_BL);

  /**
   * Initialize display manager
   */
  void begin();

  /**
   * Register a screen renderer
   * @param screen Pointer to ScreenRenderer (takes ownership)
   * @return true if registered, false if registry full
   */
  bool registerScreen(ScreenRenderer* screen);

  /**
   * Render current screen
   * Called from main loop
   */
  void render();

  /**
   * Handle touch event
   * Routes to current screen's handleTouch
   * @param x Touch X coordinate
   * @param y Touch Y coordinate
   * @return true if touch was handled
   */
  bool handleTouch(int16_t x, int16_t y);

  /**
   * Put display to sleep
   */
  void sleep();

  /**
   * Wake display from sleep
   */
  void wake();

  /**
   * Check if display is asleep
   */
  bool isAsleep() const { return _asleep; }

  /**
   * Reset activity timer (call on user interaction)
   */
  void resetActivityTimer();

  /**
   * Check and handle sleep timeout
   * Call from main loop
   */
  void checkSleepTimeout();

  /**
   * Set sleep timeout in milliseconds
   */
  void setSleepTimeout(unsigned long timeoutMs);

  /**
   * Get TFT instance for direct access when needed
   */
  TFT_eSPI& getTft() { return _tft; }

  /**
   * Set callback for rendering screens not in registry
   * Used for screens still implemented in main.cpp during migration
   */
  typedef void (*FallbackRenderCallback)(Screen screen, TFT_eSPI& tft, Navigator& nav);
  void setFallbackRenderer(FallbackRenderCallback cb) { _fallbackRenderer = cb; }

  /**
   * Set callback for handling touch on screens not in registry
   */
  typedef bool (*FallbackTouchCallback)(Screen screen, int16_t x, int16_t y, Navigator& nav);
  void setFallbackTouchHandler(FallbackTouchCallback cb) { _fallbackTouchHandler = cb; }

private:
  static const int MAX_SCREENS = 16;

  TFT_eSPI& _tft;
  Navigator& _nav;
  int _backlightPin;

  ScreenRenderer* _screens[MAX_SCREENS];
  int _screenCount;

  bool _asleep;
  unsigned long _lastActivityTime;
  unsigned long _sleepTimeoutMs;

  Screen _lastScreen;  // Track screen changes

  FallbackRenderCallback _fallbackRenderer;
  FallbackTouchCallback _fallbackTouchHandler;

  /**
   * Find renderer for a screen type
   * @return Pointer to renderer or nullptr if not found
   */
  ScreenRenderer* findRenderer(Screen screen);

  /**
   * Handle screen transition (call onExit/onEnter)
   */
  void handleScreenTransition(Screen from, Screen to);
};

#endif // DISPLAY_MANAGER_H

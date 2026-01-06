/**
 * @file DebugScreen.h
 * @brief Debug information screen for touch169 node
 *
 * Displays diagnostic information:
 * - Battery voltage, percentage, charging state
 * - Mesh network info (node ID, peers, role)
 * - IMU data (temperature, accelerometer)
 * - Mesh sensor values (temp, humidity, light, motion, LED)
 *
 * Extracted from main.cpp as part of screen refactoring.
 */

#ifndef DEBUG_SCREEN_H
#define DEBUG_SCREEN_H

#include "../ScreenRenderer.h"
#include "../../core/Battery.h"
#include "../../hardware/IMU.h"
#include "../../mesh/IMeshState.h"
#include <MeshSwarm.h>

/**
 * @brief Debug screen showing system diagnostics
 *
 * Dependencies are injected via constructor to allow for testing
 * and to decouple from global variables.
 */
class DebugScreen : public ScreenRenderer {
public:
  /**
   * @brief Construct DebugScreen with dependencies
   * @param battery Reference to Battery instance
   * @param swarm Reference to MeshSwarm instance
   * @param imu Reference to IMU instance
   * @param meshState Reference to IMeshState instance
   */
  DebugScreen(Battery& battery, MeshSwarm& swarm, IMU& imu, IMeshState& meshState);

  // ScreenRenderer interface
  void render(TFT_eSPI& tft, bool forceRedraw) override;
  bool handleTouch(int16_t x, int16_t y, Navigator& nav) override;
  Screen getScreen() const override { return Screen::Debug; }
  void onEnter() override;
  bool needsRedraw() const override { return _needsRedraw; }
  void clearRedraw() override { _needsRedraw = false; }

private:
  void drawHeader(TFT_eSPI& tft);
  void drawBatterySection(TFT_eSPI& tft);
  void drawMeshSection(TFT_eSPI& tft);
  void drawIMUSection(TFT_eSPI& tft);
  void drawSensorSection(TFT_eSPI& tft);

  Battery& _battery;
  MeshSwarm& _swarm;
  IMU& _imu;
  IMeshState& _meshState;

  bool _needsRedraw;
  unsigned long _lastUpdate;
  static constexpr unsigned long UPDATE_INTERVAL_MS = 500;
};

#endif // DEBUG_SCREEN_H

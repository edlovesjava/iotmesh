/**
 * @file DebugScreen.cpp
 * @brief Debug information screen implementation
 */

#include "DebugScreen.h"
#include "../../BoardConfig.h"

DebugScreen::DebugScreen(Battery& battery, MeshSwarm& swarm, IMU& imu, IMeshState& meshState)
  : _battery(battery)
  , _swarm(swarm)
  , _imu(imu)
  , _meshState(meshState)
  , _needsRedraw(true)
  , _lastUpdate(0)
{
}

void DebugScreen::onEnter() {
  _needsRedraw = true;
  _lastUpdate = 0;  // Force immediate update
}

void DebugScreen::render(TFT_eSPI& tft, bool forceRedraw) {
  if (forceRedraw || _needsRedraw) {
    drawHeader(tft);
    _needsRedraw = false;
    _lastUpdate = 0;  // Force data update after header redraw
  }

  // Throttle updates to every 500ms
  if (millis() - _lastUpdate < UPDATE_INTERVAL_MS) return;
  _lastUpdate = millis();

  drawBatterySection(tft);
  drawMeshSection(tft);
  drawIMUSection(tft);
  drawSensorSection(tft);
}

bool DebugScreen::handleTouch(int16_t x, int16_t y, Navigator& nav) {
  // Debug screen doesn't handle touch - boot button navigates back
  return false;
}

void DebugScreen::drawHeader(TFT_eSPI& tft) {
  tft.fillScreen(Colors::BG);
  tft.setTextColor(Colors::TEXT);
  tft.setTextSize(2);
  tft.setCursor(60, 10);
  tft.print("DEBUG INFO");

  tft.drawLine(10, 35, 230, 35, Colors::TICK);
}

void DebugScreen::drawBatterySection(TFT_eSPI& tft) {
  float voltage = _battery.getVoltage();
  int percent = _battery.getPercent();
  ChargingState state = _battery.getState();

  tft.setTextSize(2);

  // Battery header
  tft.fillRect(10, 45, 220, 55, Colors::BG);
  tft.setTextColor(Colors::HUMID);
  tft.setCursor(10, 45);
  tft.print("Battery:");

  // Voltage and percentage
  tft.setTextColor(Colors::TEXT);
  tft.setCursor(10, 65);
  tft.printf("%.2fV  %d%%", voltage, percent);

  // Charging state
  tft.setCursor(120, 65);
  switch (state) {
    case ChargingState::Charging:
      tft.setTextColor(Colors::HUMID);
      tft.print("[CHARGING]");
      break;
    case ChargingState::Full:
      tft.setTextColor(Colors::HUMID);
      tft.print("[FULL]");
      break;
    case ChargingState::Discharging:
      tft.setTextColor(Colors::TEXT);
      tft.print("[ON BAT]");
      break;
    default:
      tft.setTextColor(Colors::TICK);
      tft.print("[...]");
      break;
  }

  // Battery bar
  int barWidth = (percent * 180) / 100;
  uint16_t barColor = (state == ChargingState::Charging || state == ChargingState::Full)
                      ? Colors::HUMID : (percent > 20 ? Colors::TEXT : Colors::SECOND);
  tft.drawRect(10, 90, 184, 12, Colors::TICK);
  tft.fillRect(12, 92, 180, 8, Colors::BG);  // Clear old bar
  tft.fillRect(12, 92, barWidth, 8, barColor);
  tft.fillRect(194, 94, 4, 4, Colors::TICK);  // Battery nub
}

void DebugScreen::drawMeshSection(TFT_eSPI& tft) {
  tft.fillRect(10, 110, 220, 50, Colors::BG);
  tft.setTextColor(Colors::TEMP);
  tft.setTextSize(2);
  tft.setCursor(10, 110);
  tft.print("Mesh:");

  tft.setTextColor(Colors::TEXT);
  tft.setTextSize(1);
  tft.setCursor(10, 128);
  tft.printf("ID:%u Peers:%d %s", _swarm.getNodeId(), _swarm.getPeerCount(),
             _swarm.isCoordinator() ? "COORD" : "");
  tft.setCursor(10, 143);
  tft.printf("Uptime: %lus", millis() / 1000);
}

void DebugScreen::drawIMUSection(TFT_eSPI& tft) {
  tft.setTextSize(2);
  tft.fillRect(10, 160, 220, 45, Colors::BG);
  tft.setTextColor(Colors::MOTION);
  tft.setCursor(10, 160);
  tft.print("IMU:");

  tft.setTextSize(1);
  tft.setTextColor(Colors::TEXT);

  if (_imu.isAvailable()) {
    IMUVector accel = _imu.getAccel();
    tft.setCursor(10, 178);
    tft.printf("Temp: %.1f C", _imu.getTemperature());
    tft.setCursor(10, 193);
    tft.printf("Accel: %.2f %.2f %.2f", accel.x, accel.y, accel.z);
  } else {
    tft.setCursor(10, 178);
    tft.print("Not available");
  }
}

void DebugScreen::drawSensorSection(TFT_eSPI& tft) {
  tft.setTextSize(2);
  tft.fillRect(10, 210, 220, 70, Colors::BG);
  tft.setTextColor(Colors::LIGHT);
  tft.setCursor(10, 210);
  tft.print("Mesh Sensors:");

  tft.setTextSize(1);
  tft.setTextColor(Colors::TEXT);
  tft.setCursor(10, 228);
  tft.printf("Temp: %s C  Humid: %s%%",
             _meshState.getTemperature().c_str(),
             _meshState.getHumidity().c_str());
  tft.setCursor(10, 243);
  tft.printf("Light: %s", _meshState.getLightLevel().c_str());
  tft.setCursor(10, 258);
  tft.printf("Motion: %s  LED: %s",
             _meshState.getMotionRaw().c_str(),
             _meshState.getLedRaw().c_str());
}

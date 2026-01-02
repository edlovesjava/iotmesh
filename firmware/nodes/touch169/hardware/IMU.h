/**
 * @file IMU.h
 * @brief QMI8658 IMU wrapper for touch169 node
 *
 * Provides accelerometer, gyroscope, and temperature readings from the
 * onboard QMI8658 6-axis IMU on the Waveshare ESP32-S3-Touch-LCD-1.69.
 *
 * I2C Address: 0x6B
 * Uses shared I2C bus (SDA=GPIO11, SCL=GPIO10)
 */

#ifndef IMU_H
#define IMU_H

#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>

// QMI8658 I2C address on Waveshare board
#define IMU_I2C_ADDR 0x6B

/**
 * @brief 3-axis vector for accelerometer/gyroscope data
 */
struct IMUVector {
  float x;
  float y;
  float z;
};

/**
 * @brief QMI8658 IMU wrapper class
 *
 * Provides a simplified interface to the QMI8658 6-axis IMU.
 * Handles initialization, configuration, and data retrieval.
 */
class IMU {
public:
  /**
   * @brief Construct IMU instance
   */
  IMU();

  /**
   * @brief Initialize the IMU sensor
   * @param wire Reference to TwoWire instance (must be initialized)
   * @param sda I2C SDA pin (default from BoardConfig)
   * @param scl I2C SCL pin (default from BoardConfig)
   * @return true if initialization successful
   */
  bool begin(TwoWire& wire, int sda = -1, int scl = -1);

  /**
   * @brief Check if IMU is available and initialized
   * @return true if IMU is ready
   */
  bool isAvailable() const { return _available; }

  /**
   * @brief Update sensor readings (call periodically)
   *
   * Checks if new data is ready and updates cached values.
   * Call this in the main loop or before reading values.
   */
  void update();

  /**
   * @brief Get board/chip temperature
   * @return Temperature in Celsius
   */
  float getTemperature() const { return _temperature; }

  /**
   * @brief Get accelerometer readings
   * @return IMUVector with x, y, z acceleration in g
   */
  IMUVector getAccel() const { return _accel; }

  /**
   * @brief Get gyroscope readings
   * @return IMUVector with x, y, z angular velocity in dps
   */
  IMUVector getGyro() const { return _gyro; }

  /**
   * @brief Get raw chip ID for diagnostics
   * @return Chip ID byte (should be 0x05 for QMI8658)
   */
  uint8_t getChipID() const;

  /**
   * @brief Check if new data is available
   * @return true if sensor has new readings
   */
  bool hasNewData() const { return _hasNewData; }

private:
  SensorQMI8658 _qmi;
  bool _available;
  bool _hasNewData;

  // Cached sensor values
  float _temperature;
  IMUVector _accel;
  IMUVector _gyro;
};

#endif // IMU_H

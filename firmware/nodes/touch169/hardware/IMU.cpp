/**
 * @file IMU.cpp
 * @brief QMI8658 IMU implementation for touch169 node
 */

#include "IMU.h"
#include "../BoardConfig.h"

IMU::IMU()
  : _available(false)
  , _hasNewData(false)
  , _temperature(0.0f)
  , _accel{0.0f, 0.0f, 0.0f}
  , _gyro{0.0f, 0.0f, 0.0f}
{
}

bool IMU::begin(TwoWire& wire, int sda, int scl) {
  // Use BoardConfig defaults if not specified
  if (sda < 0) sda = TOUCH_SDA;
  if (scl < 0) scl = TOUCH_SCL;

  Serial.printf("[IMU] Initializing QMI8658 on I2C addr=0x%02X, SDA=%d, SCL=%d\n",
                IMU_I2C_ADDR, sda, scl);

  // Initialize the sensor
  // Note: Wire should already be initialized by main.cpp
  if (!_qmi.begin(wire, IMU_I2C_ADDR, sda, scl)) {
    Serial.println("[IMU] QMI8658 not found!");
    _available = false;
    return false;
  }

  Serial.printf("[IMU] QMI8658 found, chip ID: 0x%02X\n", _qmi.getChipID());

  // Configure accelerometer: 4G range, 100Hz output rate, low-pass filter
  _qmi.configAccelerometer(
    SensorQMI8658::ACC_RANGE_4G,
    SensorQMI8658::ACC_ODR_125Hz,
    SensorQMI8658::LPF_MODE_0
  );

  // Configure gyroscope: 256 dps range, ~112Hz output rate, low-pass filter
  _qmi.configGyroscope(
    SensorQMI8658::GYR_RANGE_256DPS,
    SensorQMI8658::GYR_ODR_112_1Hz,
    SensorQMI8658::LPF_MODE_0
  );

  // Enable both sensors
  _qmi.enableAccelerometer();
  _qmi.enableGyroscope();

  _available = true;
  Serial.println("[IMU] QMI8658 initialized successfully");

  // Do initial reading
  update();

  return true;
}

void IMU::update() {
  if (!_available) {
    _hasNewData = false;
    return;
  }

  // Check if new data is ready
  if (_qmi.getDataReady()) {
    // Read temperature
    _temperature = _qmi.getTemperature_C();

    // Read accelerometer
    _qmi.getAccelerometer(_accel.x, _accel.y, _accel.z);

    // Read gyroscope
    _qmi.getGyroscope(_gyro.x, _gyro.y, _gyro.z);

    _hasNewData = true;
  } else {
    _hasNewData = false;
  }
}

uint8_t IMU::getChipID() const {
  if (!_available) return 0;
  // Cast away const - getChipID doesn't modify state but isn't marked const in library
  return const_cast<SensorQMI8658&>(_qmi).getChipID();
}

/**
 * @file Battery.h
 * @brief Battery monitoring and charging state detection
 *
 * Handles voltage reading via ADC and voltage divider,
 * charging state detection through voltage trend analysis.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include "../BoardConfig.h"

// Charging states
enum class ChargingState {
    Unknown,
    Charging,
    Full,
    Discharging
};

/**
 * @class Battery
 * @brief Monitors battery voltage and detects charging state
 *
 * Uses a voltage divider on the ADC to read battery voltage.
 * Tracks voltage history to detect charging/discharging trends.
 */
class Battery {
public:
    /**
     * @brief Initialize battery monitoring
     */
    void begin();

    /**
     * @brief Update battery readings (call from loop)
     *
     * Samples voltage periodically and updates charging state.
     * Only updates at VOLTAGE_READ_INTERVAL.
     */
    void update();

    /**
     * @brief Get current battery voltage
     * @return Voltage in volts (e.g., 3.7, 4.2)
     */
    float getVoltage() const { return _lastVoltage; }

    /**
     * @brief Get battery percentage (0-100)
     * @return Percentage based on LiPo voltage curve
     */
    int getPercent() const;

    /**
     * @brief Get current charging state
     * @return ChargingState enum value
     */
    ChargingState getState() const { return _state; }

    /**
     * @brief Check if charging state changed since last check
     * @return true if state changed
     *
     * Clears the changed flag after reading.
     */
    bool stateChanged();

    /**
     * @brief Get charging state as string
     * @return Human-readable state name
     */
    const char* getStateString() const;

    /**
     * @brief Force a voltage reading (bypasses interval check)
     * @return Current voltage
     */
    float readVoltageNow();

private:
    static constexpr int HISTORY_SIZE = 5;

    float _voltageHistory[HISTORY_SIZE] = {0};
    int _historyIndex = 0;
    int _sampleCount = 0;
    unsigned long _lastReadTime = 0;
    float _lastVoltage = 0;
    ChargingState _state = ChargingState::Unknown;
    bool _stateChanged = false;

    float readRawVoltage();
    void updateState();
};

#endif // BATTERY_H

/**
 * @file Battery.cpp
 * @brief Battery monitoring implementation
 */

#include "Battery.h"

void Battery::begin() {
    // Initialize ADC pin (ESP32 does this automatically, but explicit is clearer)
    pinMode(BAT_ADC_PIN, INPUT);

    // Take initial reading
    _lastVoltage = readRawVoltage();
}

void Battery::update() {
    unsigned long now = millis();
    if (now - _lastReadTime < VOLTAGE_READ_INTERVAL) return;
    _lastReadTime = now;

    _lastVoltage = readRawVoltage();
    _voltageHistory[_historyIndex] = _lastVoltage;
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;

    if (_sampleCount < HISTORY_SIZE) {
        _sampleCount++;
        return;  // Need full history for trend analysis
    }

    updateState();
}

float Battery::readRawVoltage() {
    int adcValue = analogRead(BAT_ADC_PIN);
    float voltage = (float)adcValue * (BAT_VREF / 4095.0);
    float actualVoltage = voltage * ((BAT_R1 + BAT_R2) / BAT_R2);
    return actualVoltage;
}

float Battery::readVoltageNow() {
    _lastVoltage = readRawVoltage();
    return _lastVoltage;
}

int Battery::getPercent() const {
    // LiPo voltage range: 3.0V (empty) to 4.2V (full)
    // Linear approximation
    if (_lastVoltage >= 4.2f) return 100;
    if (_lastVoltage <= 3.0f) return 0;
    return (int)((_lastVoltage - 3.0f) / 1.2f * 100);
}

void Battery::updateState() {
    // Calculate trend from voltage history
    // Compare average of older samples vs newer samples
    float older = (_voltageHistory[(_historyIndex + 0) % HISTORY_SIZE] +
                   _voltageHistory[(_historyIndex + 1) % HISTORY_SIZE]) / 2.0f;
    float newer = (_voltageHistory[(_historyIndex + 3) % HISTORY_SIZE] +
                   _voltageHistory[(_historyIndex + 4) % HISTORY_SIZE]) / 2.0f;
    float trend = newer - older;

    // Determine charging state
    ChargingState newState;
    if (_lastVoltage >= VOLTAGE_FULL_THRESHOLD &&
        abs(trend) < VOLTAGE_TREND_THRESHOLD) {
        newState = ChargingState::Full;
    } else if (trend > VOLTAGE_TREND_THRESHOLD) {
        newState = ChargingState::Charging;
    } else if (trend < -VOLTAGE_TREND_THRESHOLD) {
        newState = ChargingState::Discharging;
    } else if (_lastVoltage >= VOLTAGE_FULL_THRESHOLD) {
        newState = ChargingState::Full;
    } else {
        // Stable voltage below full - likely on USB but not charging
        newState = (_lastVoltage > 4.0f) ? ChargingState::Full : ChargingState::Discharging;
    }

    if (newState != _state) {
        _state = newState;
        _stateChanged = true;
        Serial.printf("[Battery] State: %s (%.2fV, trend: %+.3fV)\n",
                      getStateString(), _lastVoltage, trend);
    }
}

bool Battery::stateChanged() {
    bool changed = _stateChanged;
    _stateChanged = false;
    return changed;
}

const char* Battery::getStateString() const {
    switch (_state) {
        case ChargingState::Charging:    return "Charging";
        case ChargingState::Full:        return "Full";
        case ChargingState::Discharging: return "Discharging";
        default:                         return "Unknown";
    }
}

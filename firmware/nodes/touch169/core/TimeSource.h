/**
 * @file TimeSource.h
 * @brief Time management with mesh sync support
 *
 * Handles time synchronization from mesh network and provides
 * consistent time access with timezone support.
 */

#ifndef TIMESOURCE_H
#define TIMESOURCE_H

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include "../BoardConfig.h"

/**
 * @class TimeSource
 * @brief Manages time from mesh network or system clock
 *
 * Supports time sync from mesh network with automatic timezone
 * adjustment. Falls back to system time if mesh time unavailable.
 */
class TimeSource {
public:
    /**
     * @brief Set time from mesh network
     * @param unixTime Unix timestamp (seconds since epoch)
     *
     * Also sets system time so getLocalTime() works.
     */
    void setMeshTime(unsigned long unixTime);

    /**
     * @brief Check if mesh time has been received
     * @return true if mesh time is available
     */
    bool hasMeshTime() const { return _hasMeshTime; }

    /**
     * @brief Check if any valid time is available
     * @return true if mesh or system time is valid
     */
    bool isValid() const { return _timeValid; }

    /**
     * @brief Mark time as valid (called on first successful read)
     */
    void markValid() { _timeValid = true; }

    /**
     * @brief Get current time as struct tm
     * @param timeinfo Output struct tm
     * @return true if time was retrieved successfully
     *
     * Tries mesh time first, then system time.
     */
    bool getTime(struct tm* timeinfo);

    /**
     * @brief Get timezone offset in seconds
     * @return Total offset (GMT + daylight)
     */
    long getTimezoneOffset() const { return GMT_OFFSET_SEC + DAYLIGHT_OFFSET; }

    /**
     * @brief Set timezone offset
     * @param gmtOffset GMT offset in seconds
     * @param daylightOffset Daylight saving offset in seconds
     */
    void setTimezone(long gmtOffset, long daylightOffset);

private:
    unsigned long _meshTimeBase = 0;    // Unix timestamp when sync received
    unsigned long _meshTimeMillis = 0;  // millis() when sync received
    bool _hasMeshTime = false;
    bool _timeValid = false;
    long _gmtOffset = GMT_OFFSET_SEC;
    long _daylightOffset = DAYLIGHT_OFFSET;

    bool getMeshTime(struct tm* timeinfo);
};

#endif // TIMESOURCE_H

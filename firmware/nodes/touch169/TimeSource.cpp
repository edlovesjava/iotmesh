/**
 * @file TimeSource.cpp
 * @brief Time management implementation
 */

#include "TimeSource.h"
#include <string.h>

void TimeSource::setMeshTime(unsigned long unixTime) {
    _meshTimeBase = unixTime;
    _meshTimeMillis = millis();
    _hasMeshTime = true;

    // Also set the system time so getLocalTime() works
    struct timeval tv;
    tv.tv_sec = unixTime;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

void TimeSource::setTimezone(long gmtOffset, long daylightOffset) {
    _gmtOffset = gmtOffset;
    _daylightOffset = daylightOffset;
}

bool TimeSource::getTime(struct tm* timeinfo) {
    // Try mesh time first
    if (getMeshTime(timeinfo)) {
        return true;
    }
    // Fall back to system time
    return getLocalTime(timeinfo);
}

bool TimeSource::getMeshTime(struct tm* timeinfo) {
    if (!_hasMeshTime) return false;

    // Calculate current time based on mesh time + elapsed millis
    unsigned long elapsed = (millis() - _meshTimeMillis) / 1000;
    time_t currentTime = _meshTimeBase + elapsed + _gmtOffset + _daylightOffset;

    // Convert to struct tm
    struct tm* t = localtime(&currentTime);
    if (t) {
        memcpy(timeinfo, t, sizeof(struct tm));
        return true;
    }
    return false;
}

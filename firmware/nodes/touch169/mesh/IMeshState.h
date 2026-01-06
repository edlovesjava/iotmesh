/**
 * IMeshState - Abstract interface for mesh data access
 *
 * Decouples UI components from MeshSwarm implementation.
 * Enables testing with MockMeshState without live mesh.
 *
 * Extracted from main.cpp as part of Phase R7 refactoring.
 */

#ifndef IMESH_STATE_H
#define IMESH_STATE_H

#include <Arduino.h>

// Callback type for state changes (function pointer to avoid heap allocation)
typedef void (*StateChangeCallback)(const String& value);

/**
 * IMeshState interface - abstract access to mesh sensor data
 *
 * Responsibilities:
 * - Provide typed accessors for sensor values
 * - Support state change notifications
 * - Allow actuator control (LED, etc.)
 * - Abstract away MeshSwarm implementation details
 */
class IMeshState {
public:
  virtual ~IMeshState() = default;

  // ============== Sensor Values ==============
  // Returns "--" if no data available

  /**
   * Get temperature value from mesh
   */
  virtual String getTemperature() const = 0;

  /**
   * Get humidity value from mesh
   */
  virtual String getHumidity() const = 0;

  /**
   * Get light level from mesh
   */
  virtual String getLightLevel() const = 0;

  /**
   * Get motion detection state
   */
  virtual bool getMotionDetected() const = 0;

  /**
   * Get raw motion value string (for debug display)
   */
  virtual String getMotionRaw() const = 0;

  /**
   * Get LED state
   */
  virtual bool getLedState() const = 0;

  /**
   * Get raw LED value string (for debug display)
   */
  virtual String getLedRaw() const = 0;

  /**
   * Check if any sensor data has been received
   */
  virtual bool hasSensorData() const = 0;

  // ============== Network Info ==============

  /**
   * Get number of nodes in mesh
   */
  virtual int getNodeCount() const = 0;

  // ============== State Change Notifications ==============

  /**
   * Register callback for state changes on a specific key
   * @param key State key to watch ("temp", "humid", "light", "motion", "led")
   * @param cb Callback function (receives new value)
   */
  virtual void onStateChange(const char* key, StateChangeCallback cb) = 0;

  // ============== Actuator Control ==============

  /**
   * Set LED state
   * @param on true = LED on, false = LED off
   */
  virtual void setLedState(bool on) = 0;

  /**
   * Set arbitrary state key
   * @param key State key
   * @param value Value to set
   */
  virtual void setState(const char* key, const String& value) = 0;
};

#endif // IMESH_STATE_H

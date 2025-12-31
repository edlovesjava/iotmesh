/**
 * MeshSwarmAdapter - Production implementation of IMeshState
 *
 * Wraps MeshSwarm to provide cached sensor values and state change callbacks.
 * Uses static allocation with fixed-size arrays for ESP32 optimization.
 *
 * Extracted from main.cpp as part of Phase R7 refactoring.
 *
 * Usage:
 *   MeshSwarm swarm;
 *   MeshSwarmAdapter meshState(swarm);
 *   meshState.begin();  // Register watchers after swarm.begin()
 *
 *   // Read sensor values
 *   String temp = meshState.getTemperature();
 *
 *   // Control actuators
 *   meshState.setLedState(true);
 */

#ifndef MESH_SWARM_ADAPTER_H
#define MESH_SWARM_ADAPTER_H

#include "IMeshState.h"
#include <MeshSwarm.h>

// Forward declaration for TimeSource integration
class TimeSource;

/**
 * MeshSwarmAdapter class - implements IMeshState using MeshSwarm
 *
 * Responsibilities:
 * - Register watchers on MeshSwarm for sensor keys
 * - Cache sensor values for efficient polling
 * - Support zone-specific fallback keys (temp_zone1, etc.)
 * - Notify registered callbacks on state changes
 * - Sync time to TimeSource when "time" key received
 */
class MeshSwarmAdapter : public IMeshState {
public:
  /**
   * Constructor
   * @param swarm Reference to MeshSwarm instance
   */
  MeshSwarmAdapter(MeshSwarm& swarm);

  /**
   * Initialize adapter and register mesh watchers
   * Call after swarm.begin() has been called
   */
  void begin();

  /**
   * Set TimeSource for time sync
   * @param ts Pointer to TimeSource instance (can be nullptr)
   */
  void setTimeSource(TimeSource* ts);

  // ============== IMeshState Implementation ==============

  String getTemperature() const override { return _temp; }
  String getHumidity() const override { return _humidity; }
  String getLightLevel() const override { return _light; }
  bool getMotionDetected() const override { return _motion == "1"; }
  bool getLedState() const override { return _led == "1"; }
  bool hasSensorData() const override { return _hasSensorData; }

  int getNodeCount() const override;

  void onStateChange(const char* key, StateChangeCallback cb) override;

  void setLedState(bool on) override;
  void setState(const char* key, const String& value) override;

  // ============== Raw Value Access (for debug screens) ==============

  /**
   * Get raw motion value string
   */
  const String& getMotionRaw() const { return _motion; }

  /**
   * Get raw LED value string
   */
  const String& getLedRaw() const { return _led; }

private:
  static const int MAX_STATE_CALLBACKS = 8;

  // Callback slot - associates a key with a callback function
  struct StateCallbackSlot {
    const char* key;           // State key to match
    StateChangeCallback cb;    // Callback function pointer
  };

  MeshSwarm& _swarm;
  TimeSource* _timeSource;

  // Cached sensor values
  String _temp;
  String _humidity;
  String _light;
  String _motion;
  String _led;
  bool _hasSensorData;

  // Static callback array (no heap allocation)
  StateCallbackSlot _callbacks[MAX_STATE_CALLBACKS];
  int _callbackCount;

  /**
   * Fire all callbacks registered for a specific key
   * @param key State key that changed
   * @param value New value
   */
  void notifyCallbacks(const char* key, const String& value);

  /**
   * Handle zone-specific fallback keys
   * @param key Full key (e.g., "temp_zone1")
   * @param value Value received
   */
  void handleZoneFallback(const String& key, const String& value);
};

#endif // MESH_SWARM_ADAPTER_H

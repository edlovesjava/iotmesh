/**
 * MeshSwarmAdapter - Production implementation of IMeshState
 *
 * Implementation of mesh state caching and callback notification.
 */

#include "MeshSwarmAdapter.h"
#include "../core/TimeSource.h"
#include <cstring>

// Static instance pointer for lambda callbacks
// (lambdas with captures can't be converted to function pointers,
// so we use a static pointer that the lambdas can access)
static MeshSwarmAdapter* s_instance = nullptr;

MeshSwarmAdapter::MeshSwarmAdapter(MeshSwarm& swarm)
  : _swarm(swarm)
  , _timeSource(nullptr)
  , _temp("--")
  , _humidity("--")
  , _light("--")
  , _motion("0")
  , _led("0")
  , _hasSensorData(false)
  , _callbackCount(0)
{
  // Initialize callback slots
  for (int i = 0; i < MAX_STATE_CALLBACKS; i++) {
    _callbacks[i].key = nullptr;
    _callbacks[i].cb = nullptr;
  }
}

void MeshSwarmAdapter::setTimeSource(TimeSource* ts) {
  _timeSource = ts;
}

void MeshSwarmAdapter::begin() {
  // Store instance pointer for static lambda access
  s_instance = this;

  // Watch temperature
  _swarm.watchState("temp", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance) {
      s_instance->_temp = value;
      s_instance->_hasSensorData = true;
      s_instance->notifyCallbacks("temp", value);
      Serial.printf("[MESHSTATE] Temperature: %s C\n", value.c_str());
    }
  });

  // Watch humidity
  _swarm.watchState("humid", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance) {
      s_instance->_humidity = value;
      s_instance->_hasSensorData = true;
      s_instance->notifyCallbacks("humid", value);
      Serial.printf("[MESHSTATE] Humidity: %s %%\n", value.c_str());
    }
  });

  // Watch light level
  _swarm.watchState("light", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance) {
      s_instance->_light = value;
      s_instance->_hasSensorData = true;
      s_instance->notifyCallbacks("light", value);
      Serial.printf("[MESHSTATE] Light: %s\n", value.c_str());
    }
  });

  // Watch motion
  _swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance) {
      s_instance->_motion = value;
      s_instance->notifyCallbacks("motion", value);
      Serial.printf("[MESHSTATE] Motion: %s\n", value.c_str());
    }
  });

  // Watch LED state
  _swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance) {
      s_instance->_led = value;
      s_instance->notifyCallbacks("led", value);
      Serial.printf("[MESHSTATE] LED: %s\n", value.c_str());
    }
  });

  // Watch time sync from gateway
  _swarm.watchState("time", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance && s_instance->_timeSource) {
      unsigned long unixTime = value.toInt();
      if (unixTime > 1700000000) {
        s_instance->_timeSource->setMeshTime(unixTime);
        Serial.printf("[MESHSTATE] Time synced: %lu\n", unixTime);
      }
    }
  });

  // Wildcard watcher for zone-specific fallback keys
  _swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    if (s_instance) {
      s_instance->handleZoneFallback(key, value);
    }
  });

  Serial.println("[MESHSTATE] Watchers registered");
}

int MeshSwarmAdapter::getNodeCount() const {
  return _swarm.getPeerCount();
}

void MeshSwarmAdapter::onStateChange(const char* key, StateChangeCallback cb) {
  if (_callbackCount < MAX_STATE_CALLBACKS && cb != nullptr) {
    _callbacks[_callbackCount].key = key;
    _callbacks[_callbackCount].cb = cb;
    _callbackCount++;
  }
}

void MeshSwarmAdapter::setLedState(bool on) {
  _swarm.setState("led", on ? "1" : "0");
}

void MeshSwarmAdapter::setState(const char* key, const String& value) {
  _swarm.setState(key, value);
}

void MeshSwarmAdapter::notifyCallbacks(const char* key, const String& value) {
  for (int i = 0; i < _callbackCount; i++) {
    if (_callbacks[i].key != nullptr && strcmp(_callbacks[i].key, key) == 0) {
      if (_callbacks[i].cb != nullptr) {
        _callbacks[i].cb(value);
      }
    }
  }
}

void MeshSwarmAdapter::handleZoneFallback(const String& key, const String& value) {
  // Temperature from zone (temp_zone1, temp_kitchen, etc.) - only if no base temp
  if (key.startsWith("temp_") && _temp == "--") {
    _temp = value;
    _hasSensorData = true;
    notifyCallbacks("temp", value);
    Serial.printf("[MESHSTATE] Temperature (zone fallback): %s = %s C\n", key.c_str(), value.c_str());
  }

  // Humidity from zone - only if no base humidity
  if (key.startsWith("humidity_") && _humidity == "--") {
    _humidity = value;
    _hasSensorData = true;
    notifyCallbacks("humid", value);
    Serial.printf("[MESHSTATE] Humidity (zone fallback): %s = %s %%\n", key.c_str(), value.c_str());
  }

  // Light from zone - only if no base light
  if (key.startsWith("light_") && _light == "--") {
    _light = value;
    _hasSensorData = true;
    notifyCallbacks("light", value);
    Serial.printf("[MESHSTATE] Light (zone fallback): %s = %s\n", key.c_str(), value.c_str());
  }

  // Motion from zone - always update (motion is event-based)
  if (key.startsWith("motion_")) {
    _motion = value;
    notifyCallbacks("motion", value);
    Serial.printf("[MESHSTATE] Motion (zone): %s = %s\n", key.c_str(), value.c_str());
  }
}

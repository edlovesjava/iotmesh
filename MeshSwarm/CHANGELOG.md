# Changelog

All notable changes to MeshSwarm will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2024-12-20

### Added
- OTA firmware updates via mesh network
  - `enableOTAReceive(role)` for nodes to receive updates
  - `enableOTADistribution(bool)` for gateway to distribute updates
  - `checkForOTAUpdates()` for gateway polling
- Telemetry support with server integration
  - `enableTelemetry(bool)` to enable/disable
  - `setTelemetryServer(url)` for gateway configuration
  - `setTelemetryInterval(ms)` for push frequency
  - `pushTelemetry()` for manual push
- Gateway mode for WiFi bridge
  - `setGatewayMode(bool)` to enable
  - `connectToWiFi(ssid, password)` for station mode
  - `isGateway()` to check mode
- Example sketches
  - BasicNode, ButtonNode, LedNode, SensorNode, WatcherNode, GatewayNode
- keywords.txt for Arduino IDE syntax highlighting

### Changed
- Updated library.properties with new dependencies

## [1.0.0] - 2024-12-15

### Added
- Initial release
- Self-organizing mesh network using painlessMesh
- Automatic coordinator election (lowest node ID wins)
- Distributed shared state with key-value store
- State watchers with callback support
- Conflict resolution (version + origin ID)
- OLED display support (SSD1306 128x64)
- Serial command interface
- Customization hooks (onLoop, onSerialCommand, onDisplayUpdate)
- Built-in commands: status, peers, state, set, get, sync, reboot

### Dependencies
- painlessMesh
- ArduinoJson
- Adafruit SSD1306
- Adafruit GFX Library

/*
 * MeshSwarm Library
 *
 * Self-organizing ESP32 mesh network with distributed shared state.
 *
 * Features:
 *   - Auto peer discovery and coordinator election
 *   - Distributed key-value state with conflict resolution
 *   - State watcher callbacks
 *   - OLED display support
 *   - Serial command interface
 */

#ifndef MESH_SWARM_H
#define MESH_SWARM_H

#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <map>
#include <vector>
#include <functional>

// ============== DEFAULT CONFIGURATION ==============
// Override these before including MeshSwarm.h if needed

#ifndef MESH_PREFIX
#define MESH_PREFIX     "swarm"
#endif

#ifndef MESH_PASSWORD
#define MESH_PASSWORD   "swarmnet123"
#endif

#ifndef MESH_PORT
#define MESH_PORT       5555
#endif

// OLED Configuration
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH    128
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT   64
#endif

#ifndef OLED_RESET
#define OLED_RESET      -1
#endif

#ifndef OLED_ADDR
#define OLED_ADDR       0x3C
#endif

#ifndef I2C_SDA
#define I2C_SDA         21
#endif

#ifndef I2C_SCL
#define I2C_SCL         22
#endif

// Timing
#ifndef HEARTBEAT_INTERVAL
#define HEARTBEAT_INTERVAL   5000
#endif

#ifndef STATE_SYNC_INTERVAL
#define STATE_SYNC_INTERVAL  10000
#endif

#ifndef DISPLAY_INTERVAL
#define DISPLAY_INTERVAL     500
#endif

// Telemetry
#ifndef TELEMETRY_INTERVAL
#define TELEMETRY_INTERVAL   30000
#endif

#ifndef STATE_TELEMETRY_MIN_INTERVAL
#define STATE_TELEMETRY_MIN_INTERVAL  2000  // Min ms between state-triggered pushes
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION     "1.0.0"
#endif

// ============== MESSAGE TYPES ==============
enum MsgType {
  MSG_HEARTBEAT  = 1,
  MSG_STATE_SET  = 2,
  MSG_STATE_SYNC = 3,
  MSG_STATE_REQ  = 4,
  MSG_COMMAND    = 5,
  MSG_TELEMETRY  = 6   // Node telemetry to gateway
};

// ============== DATA STRUCTURES ==============
struct StateEntry {
  String value;
  uint32_t version;
  uint32_t origin;
  unsigned long timestamp;
};

struct Peer {
  uint32_t id;
  String name;
  String role;
  unsigned long lastSeen;
  bool alive;
};

// State change callback type
typedef std::function<void(const String& key, const String& value, const String& oldValue)> StateCallback;

// Custom loop callback type (for node-specific logic)
typedef std::function<void()> LoopCallback;

// Custom serial command handler
typedef std::function<bool(const String& input)> SerialHandler;

// Custom display handler
typedef std::function<void(Adafruit_SSD1306& display, int startLine)> DisplayHandler;

// ============== MESHSWARM CLASS ==============
class MeshSwarm {
public:
  MeshSwarm();

  // Initialization
  void begin(const char* nodeName = nullptr);
  void begin(const char* prefix, const char* password, uint16_t port, const char* nodeName = nullptr);

  // Main loop - call from Arduino loop()
  void update();

  // State management
  bool setState(const String& key, const String& value);
  bool setStates(std::initializer_list<std::pair<String, String>> states);  // Batch update
  String getState(const String& key, const String& defaultVal = "");
  void watchState(const String& key, StateCallback callback);
  void broadcastFullState();
  void requestStateSync();

  // Node info
  uint32_t getNodeId() { return myId; }
  String getNodeName() { return myName; }
  String getRole() { return myRole; }
  bool isCoordinator() { return myRole == "COORD"; }
  int getPeerCount();

  // Peer access
  std::map<uint32_t, Peer>& getPeers() { return peers; }

  // Display access
  Adafruit_SSD1306& getDisplay() { return display; }

  // Mesh access (for advanced usage)
  painlessMesh& getMesh() { return mesh; }

  // Customization hooks
  void onLoop(LoopCallback callback);
  void onSerialCommand(SerialHandler handler);
  void onDisplayUpdate(DisplayHandler handler);

  // Set custom status line for display
  void setStatusLine(const String& status);

  // Heartbeat data customization
  void setHeartbeatData(const String& key, int value);

  // Telemetry to server
  void setTelemetryServer(const char* url, const char* apiKey = nullptr);
  void setTelemetryInterval(unsigned long ms);
  void enableTelemetry(bool enable);
  bool isTelemetryEnabled() { return telemetryEnabled; }
  void pushTelemetry();

  // WiFi station mode for telemetry
  void connectToWiFi(const char* ssid, const char* password);
  bool isWiFiConnected();

  // Gateway mode - receives telemetry from other nodes and pushes to server
  void setGatewayMode(bool enable);
  bool isGateway() { return gatewayMode; }

private:
  // Core objects
  painlessMesh mesh;
  Adafruit_SSD1306 display;

  // State
  std::map<String, StateEntry> sharedState;
  std::map<String, std::vector<StateCallback>> stateWatchers;
  std::map<uint32_t, Peer> peers;

  // Node identity
  uint32_t myId;
  String myName;
  String myRole;
  uint32_t coordinatorId;

  // Timing
  unsigned long lastHeartbeat;
  unsigned long lastStateSync;
  unsigned long lastDisplayUpdate;
  unsigned long lastTelemetryPush;
  unsigned long lastStateTelemetryPush;  // For debouncing state-triggered pushes
  unsigned long bootTime;

  // Telemetry config
  String telemetryUrl;
  String telemetryApiKey;
  unsigned long telemetryInterval;
  bool telemetryEnabled;
  bool gatewayMode;

  // Custom hooks
  std::vector<LoopCallback> loopCallbacks;
  std::vector<SerialHandler> serialHandlers;
  std::vector<DisplayHandler> displayHandlers;

  // Display state
  String lastStateChange;
  String customStatus;

  // Custom heartbeat data
  std::map<String, int> heartbeatExtras;

  // Internal methods
  void initMesh(const char* prefix, const char* password, uint16_t port);
  void initDisplay();

  void onReceive(uint32_t from, String &msg);
  void onNewConnection(uint32_t nodeId);
  void onDroppedConnection(uint32_t nodeId);
  void onChangedConnections();

  void electCoordinator();
  void sendHeartbeat();
  void pruneDeadPeers();
  void updateDisplay();
  void processSerial();

  void triggerWatchers(const String& key, const String& value, const String& oldValue);
  void broadcastState(const String& key);
  void handleStateSet(uint32_t from, JsonObject& data);
  void handleStateSync(uint32_t from, JsonObject& data);
  void handleTelemetry(uint32_t from, JsonObject& data);
  void sendTelemetryToGateway();
  void pushTelemetryForNode(uint32_t nodeId, JsonObject& data);

  String createMsg(MsgType type, JsonDocument& data);
  String nodeIdToName(uint32_t id);
};

#endif // MESH_SWARM_H

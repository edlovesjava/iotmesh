/*
 * ESP32 Mesh Swarm - Shared State Edition
 * 
 * Self-organizing mesh with distributed shared state.
 * Any node can set state, all nodes sync automatically.
 * Nodes can watch for state changes and react.
 * 
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - Optional: Button on GPIO0, LED on GPIO2
 * 
 * Libraries:
 *   - painlessMesh
 *   - ArduinoJson
 *   - Adafruit SSD1306
 *   - Adafruit GFX Library
 */

#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <map>
#include <vector>
#include <functional>

// ============== MESH CONFIGURATION ==============
#define MESH_PREFIX     "swarm"
#define MESH_PASSWORD   "swarmnet123"
#define MESH_PORT       5555

// ============== OLED CONFIGURATION ==============
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR       0x3C
#define I2C_SDA         21
#define I2C_SCL         22

// ============== HARDWARE (customize per node) ==============
// #define BUTTON_PIN      0    // Boot button on most ESP32 boards
// #define LED_PIN         2    // Onboard LED on most ESP32 boards

// Comment/uncomment to enable features per node:
#define ENABLE_BUTTON        // This node has a button
#define ENABLE_LED           // This node has an LED to control

// ============== TIMING ==============
#define HEARTBEAT_INTERVAL   5000
#define STATE_SYNC_INTERVAL  10000  // Full state sync broadcast
#define DISPLAY_INTERVAL     500
#define DEBOUNCE_MS          50

// ============== MESSAGE TYPES ==============
enum MsgType {
  MSG_HEARTBEAT  = 1,
  MSG_STATE_SET  = 2,   // Single state update
  MSG_STATE_SYNC = 3,   // Full state dump (on join or periodic)
  MSG_STATE_REQ  = 4,   // Request state sync
  MSG_COMMAND    = 5
};

// ============== SHARED STATE ==============
struct StateEntry {
  String value;
  uint32_t version;
  uint32_t origin;
  unsigned long timestamp;
};


std::map<String, StateEntry> sharedState;

// State change callback type
typedef std::function<void(const String& key, const String& value, const String& oldValue)> StateCallback;
std::map<String, std::vector<StateCallback>> stateWatchers;

// ============== PEER TRACKING ==============
struct Peer {
  uint32_t id;
  String name;
  String role;
  unsigned long lastSeen;
  bool alive;
};

std::map<uint32_t, Peer> peers;

// ============== GLOBALS ==============
painlessMesh mesh;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

uint32_t myId = 0;
String myName = "";
String myRole = "PEER";
uint32_t coordinatorId = 0;

unsigned long lastHeartbeat = 0;
unsigned long lastStateSync = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long bootTime = 0;

// Button state
#ifdef ENABLE_BUTTON
bool lastButtonState = HIGH;
unsigned long lastButtonChange = 0;
#endif

// Display info
String lastStateChange = "";

// ============== FORWARD DECLARATIONS ==============
void onReceive(uint32_t from, String &msg);
void onNewConnection(uint32_t nodeId);
void onDroppedConnection(uint32_t nodeId);
void onChangedConnections();

void electCoordinator();
void sendHeartbeat();
void pruneDeadPeers();
void updateDisplay();
void processSerial();

// State functions
bool setState(const String& key, const String& value);
String getState(const String& key, const String& defaultVal = "");
void watchState(const String& key, StateCallback callback);
void broadcastState(const String& key);
void broadcastFullState();
void requestStateSync();
void handleStateSet(uint32_t from, JsonObject& data);
void handleStateSync(uint32_t from, JsonObject& data);

String createMsg(MsgType type, JsonDocument& data);
String nodeIdToName(uint32_t id);

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════╗");
  Serial.println("║   ESP32 MESH SWARM - SHARED STATE ║");
  Serial.println("╚═══════════════════════════════════╝");
  Serial.println();

  // Hardware setup
  #ifdef ENABLE_BUTTON
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    Serial.println("[HW] Button enabled on GPIO" + String(BUTTON_PIN));
  #endif
  
  #ifdef ENABLE_LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("[HW] LED enabled on GPIO" + String(LED_PIN));
  #endif

  // OLED setup
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Init failed!");
  } else {
    Serial.println("[OLED] Initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Mesh Swarm");
    display.println("Shared State");
    display.display();
  }

  // Stagger startup to reduce scan collisions
  uint32_t chipId = ESP.getEfuseMac() & 0xFFFF;
  uint32_t startDelay = (chipId % 3) * 500;
  Serial.printf("[BOOT] Startup delay: %dms\n", startDelay);
  delay(startDelay);

  // Mesh setup
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  
  mesh.onReceive(&onReceive);
  mesh.onNewConnection(&onNewConnection);
  mesh.onDroppedConnection(&onDroppedConnection);
  mesh.onChangedConnections(&onChangedConnections);

  myId = mesh.getNodeId();
  myName = nodeIdToName(myId);
  bootTime = millis();
  
  Serial.printf("[MESH] Node ID: %u\n", myId);
  Serial.printf("[MESH] Name: %s\n", myName.c_str());
  Serial.println();

  // ============== REGISTER STATE WATCHERS ==============
  
  #ifdef ENABLE_LED
    // Watch for "led" state changes
    watchState("led", [](const String& key, const String& value, const String& oldValue) {
      bool ledOn = (value == "1" || value == "on" || value == "true");
      digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
      Serial.printf("[LED] State changed: %s -> %s (LED %s)\n", 
                    oldValue.c_str(), value.c_str(), ledOn ? "ON" : "OFF");
    });
  #endif

  // Example: watch temperature threshold
  watchState("temp", [](const String& key, const String& value, const String& oldValue) {
    Serial.printf("[WATCH] Temperature: %s -> %s\n", oldValue.c_str(), value.c_str());
  });

  Serial.println("Commands: status, peers, state, set <key> <val>, get <key>");
  Serial.println("─────────────────────────────────────────────────────────");
  Serial.println();
}

// ============== MAIN LOOP ==============
void loop() {
  mesh.update();
  
  unsigned long now = millis();
  
  // Heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    pruneDeadPeers();
    lastHeartbeat = now;
  }
  
  // Periodic full state sync (consistency check)
  if (now - lastStateSync >= STATE_SYNC_INTERVAL) {
    broadcastFullState();
    lastStateSync = now;
  }
  
  // Display update
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // Button handling
  #ifdef ENABLE_BUTTON
    handleButton();
  #endif
  
  // Serial commands
  if (Serial.available()) {
    processSerial();
  }
}

// ============== BUTTON HANDLING ==============
#ifdef ENABLE_BUTTON
void handleButton() {
  bool buttonState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();
  
  // Debounce
  if (buttonState != lastButtonState && (now - lastButtonChange > DEBOUNCE_MS)) {
    lastButtonChange = now;
    lastButtonState = buttonState;
    
    // Button pressed (LOW because INPUT_PULLUP)
    if (buttonState == LOW) {
      // Toggle LED state
      String currentLed = getState("led", "0");
      String newLed = (currentLed == "1") ? "0" : "1";
      setState("led", newLed);
      Serial.printf("[BUTTON] Pressed! LED state: %s -> %s\n", currentLed.c_str(), newLed.c_str());
    }
  }
}
#endif

// ============== SHARED STATE FUNCTIONS ==============

bool setState(const String& key, const String& value) {
  String oldValue = "";
  uint32_t newVersion = 1;
  
  // Check existing entry
  if (sharedState.count(key)) {
    oldValue = sharedState[key].value;
    newVersion = sharedState[key].version + 1;
    
    // No change, skip
    if (oldValue == value) {
      return false;
    }
  }
  
  // Update local state
  StateEntry entry;
  entry.value = value;
  entry.version = newVersion;
  entry.origin = myId;
  entry.timestamp = millis();
  sharedState[key] = entry;
  
  // Trigger local watchers
  triggerWatchers(key, value, oldValue);
  
  // Broadcast to mesh
  broadcastState(key);
  
  // Update display
  lastStateChange = key + "=" + value;
  
  return true;
}

String getState(const String& key, const String& defaultVal) {
  if (sharedState.count(key)) {
    return sharedState[key].value;
  }
  return defaultVal;
}

void watchState(const String& key, StateCallback callback) {
  stateWatchers[key].push_back(callback);
}

void triggerWatchers(const String& key, const String& value, const String& oldValue) {
  if (stateWatchers.count(key)) {
    for (auto& cb : stateWatchers[key]) {
      cb(key, value, oldValue);
    }
  }
  
  // Also trigger wildcard watchers (key "*")
  if (stateWatchers.count("*")) {
    for (auto& cb : stateWatchers["*"]) {
      cb(key, value, oldValue);
    }
  }
}

void broadcastState(const String& key) {
  if (!sharedState.count(key)) return;
  
  StateEntry& entry = sharedState[key];
  
  JsonDocument data;
  data["k"] = key;
  data["v"] = entry.value;
  data["ver"] = entry.version;
  data["org"] = entry.origin;
  
  String msg = createMsg(MSG_STATE_SET, data);
  mesh.sendBroadcast(msg);
}

void broadcastFullState() {
  if (sharedState.empty()) return;
  
  JsonDocument data;
  JsonArray arr = data["s"].to<JsonArray>();
  
  for (auto& kv : sharedState) {
    JsonObject entry = arr.add<JsonObject>();
    entry["k"] = kv.first;
    entry["v"] = kv.second.value;
    entry["ver"] = kv.second.version;
    entry["org"] = kv.second.origin;
  }
  
  String msg = createMsg(MSG_STATE_SYNC, data);
  mesh.sendBroadcast(msg);
}

void requestStateSync() {
  JsonDocument data;
  data["req"] = 1;
  String msg = createMsg(MSG_STATE_REQ, data);
  mesh.sendBroadcast(msg);
}

void handleStateSet(uint32_t from, JsonObject& data) {
  String key = data["k"] | "";
  String value = data["v"] | "";
  uint32_t version = data["ver"] | 0;
  uint32_t origin = data["org"] | from;
  
  if (key.length() == 0) return;
  
  bool shouldUpdate = false;
  String oldValue = "";
  
  if (!sharedState.count(key)) {
    // New key, accept it
    shouldUpdate = true;
  } else {
    StateEntry& existing = sharedState[key];
    oldValue = existing.value;
    
    if (version > existing.version) {
      // Higher version wins
      shouldUpdate = true;
    } else if (version == existing.version && origin < existing.origin) {
      // Same version, lower origin ID wins (deterministic)
      shouldUpdate = true;
    }
  }
  
  if (shouldUpdate && oldValue != value) {
    StateEntry entry;
    entry.value = value;
    entry.version = version;
    entry.origin = origin;
    entry.timestamp = millis();
    sharedState[key] = entry;
    
    // Trigger watchers
    triggerWatchers(key, value, oldValue);
    
    // Update display
    lastStateChange = key + "=" + value;
    
    Serial.printf("[STATE] %s = %s (v%u from %s)\n", 
                  key.c_str(), value.c_str(), version, nodeIdToName(origin).c_str());
  }
}

void handleStateSync(uint32_t from, JsonObject& data) {
  JsonArray arr = data["s"].as<JsonArray>();
  
  for (JsonObject entry : arr) {
    handleStateSet(from, entry);
  }
  
  Serial.printf("[SYNC] Received %d state entries from %s\n", 
                arr.size(), nodeIdToName(from).c_str());
}

// ============== MESH CALLBACKS ==============

void onReceive(uint32_t from, String &msg) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);
  
  if (err) {
    Serial.printf("[RX] JSON error from %u\n", from);
    return;
  }
  
  MsgType type = (MsgType)doc["t"].as<int>();
  String senderName = doc["n"] | "???";
  JsonObject data = doc["d"].as<JsonObject>();
  
  switch (type) {
    case MSG_HEARTBEAT: {
      Peer &p = peers[from];
      p.id = from;
      p.name = senderName;
      p.role = data["role"] | "PEER";
      p.lastSeen = millis();
      p.alive = true;
      electCoordinator();
      break;
    }
    
    case MSG_STATE_SET:
      handleStateSet(from, data);
      break;
      
    case MSG_STATE_SYNC:
      handleStateSync(from, data);
      break;
      
    case MSG_STATE_REQ:
      // Someone requested state, send ours
      broadcastFullState();
      break;
      
    case MSG_COMMAND:
      // Handle commands
      break;
  }
}

void onNewConnection(uint32_t nodeId) {
  Serial.printf("[MESH] + Connected: %s\n", nodeIdToName(nodeId).c_str());
  sendHeartbeat();
  
  // Share our state with new node
  broadcastFullState();
}

void onDroppedConnection(uint32_t nodeId) {
  Serial.printf("[MESH] - Dropped: %s\n", nodeIdToName(nodeId).c_str());
  if (peers.count(nodeId)) {
    peers[nodeId].alive = false;
  }
  electCoordinator();
}

void onChangedConnections() {
  Serial.printf("[MESH] Topology changed. Nodes: %d\n", mesh.getNodeList().size());
  electCoordinator();
}

// ============== COORDINATOR ELECTION ==============

void electCoordinator() {
  auto nodeList = mesh.getNodeList();
  uint32_t lowest = myId;
  
  for (auto &id : nodeList) {
    if (id < lowest) lowest = id;
  }
  
  String oldRole = myRole;
  coordinatorId = lowest;
  myRole = (lowest == myId) ? "COORD" : "PEER";
  
  if (oldRole != myRole) {
    Serial.printf("[ROLE] %s -> %s\n", oldRole.c_str(), myRole.c_str());
  }
}

// ============== HEARTBEAT ==============

void sendHeartbeat() {
  JsonDocument data;
  data["role"] = myRole;
  data["up"] = (millis() - bootTime) / 1000;
  data["heap"] = ESP.getFreeHeap();
  data["states"] = sharedState.size();
  
  String msg = createMsg(MSG_HEARTBEAT, data);
  mesh.sendBroadcast(msg);
}

void pruneDeadPeers() {
  unsigned long now = millis();
  for (auto it = peers.begin(); it != peers.end(); ) {
    if (now - it->second.lastSeen > 15000) {
      it = peers.erase(it);
    } else {
      ++it;
    }
  }
}

// ============== HELPERS ==============

String createMsg(MsgType type, JsonDocument& data) {
  JsonDocument doc;
  doc["t"] = type;
  doc["n"] = myName;
  doc["d"] = data;
  
  String out;
  serializeJson(doc, out);
  return out;
}

String nodeIdToName(uint32_t id) {
  String hex = String(id, HEX);
  hex.toUpperCase();
  if (hex.length() > 4) {
    hex = hex.substring(hex.length() - 4);
  }
  return "N" + hex;
}

// ============== DISPLAY ==============

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  // Line 1: Identity
  uint32_t uptime = (millis() - bootTime) / 1000;
  display.printf("%s [%s] %d:%02d\n", myName.c_str(), myRole.c_str(), uptime/60, uptime%60);
  
  // Line 2: Network
  int alivePeers = 0;
  for (auto &p : peers) if (p.second.alive) alivePeers++;
  display.printf("Peers:%d States:%d\n", alivePeers, sharedState.size());
  
  // Line 3: Separator
  display.println("------------------------");
  
  // Lines 4-6: State values (up to 4)
  int shown = 0;
  for (auto& kv : sharedState) {
    if (shown >= 4) break;
    String line = kv.first + "=" + kv.second.value;
    if (line.length() > 21) line = line.substring(0, 21);
    display.println(line);
    shown++;
  }
  
  // Fill empty lines
  while (shown < 4) {
    display.println();
    shown++;
  }
  
  // Line 7: Last change
  if (lastStateChange.length() > 0) {
    display.printf("Last:%s\n", lastStateChange.substring(0, 16).c_str());
  }
  
  display.display();
}

// ============== SERIAL COMMANDS ==============

void processSerial() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  
  if (input.length() == 0) return;
  
  if (input == "status") {
    Serial.println("\n--- NODE STATUS ---");
    Serial.printf("ID: %u (%s)\n", myId, myName.c_str());
    Serial.printf("Role: %s\n", myRole.c_str());
    Serial.printf("Peers: %d\n", peers.size());
    Serial.printf("States: %d\n", sharedState.size());
    Serial.printf("Heap: %u\n", ESP.getFreeHeap());
    Serial.println();
  }
  else if (input == "peers") {
    Serial.println("\n--- PEERS ---");
    for (auto& p : peers) {
      Serial.printf("  %s [%s] %s\n", p.second.name.c_str(), 
                    p.second.role.c_str(), p.second.alive ? "✓" : "✗");
    }
    Serial.println();
  }
  else if (input == "state") {
    Serial.println("\n--- SHARED STATE ---");
    for (auto& kv : sharedState) {
      Serial.printf("  %s = %s (v%u from %s)\n", 
                    kv.first.c_str(), 
                    kv.second.value.c_str(),
                    kv.second.version,
                    nodeIdToName(kv.second.origin).c_str());
    }
    Serial.println();
  }
  else if (input.startsWith("set ")) {
    // Parse "set key value"
    int firstSpace = input.indexOf(' ', 4);
    if (firstSpace > 0) {
      String key = input.substring(4, firstSpace);
      String value = input.substring(firstSpace + 1);
      setState(key, value);
      Serial.printf("[SET] %s = %s\n", key.c_str(), value.c_str());
    } else {
      Serial.println("Usage: set <key> <value>");
    }
  }
  else if (input.startsWith("get ")) {
    String key = input.substring(4);
    String value = getState(key, "(not set)");
    Serial.printf("[GET] %s = %s\n", key.c_str(), value.c_str());
  }
  else if (input == "sync") {
    broadcastFullState();
    Serial.println("[SYNC] Broadcast full state");
  }
  else if (input == "reboot") {
    ESP.restart();
  }
  else {
    Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, reboot");
  }
}

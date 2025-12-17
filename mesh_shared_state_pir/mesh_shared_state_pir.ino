/*
 * ESP32 Mesh Swarm - PIR Motion Sensor Node
 *
 * Polls a Lafvin Nano over I2C for PIR motion state and
 * publishes to the mesh network shared state.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - Lafvin Nano with PIR sensor (I2C slave at 0x42)
 *   - Level shifter between ESP32 (3.3V) and Nano (5V)
 *
 * I2C Bus:
 *   - OLED at 0x3C
 *   - PIR Nano at 0x42
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

// ============== PIR MODULE CONFIGURATION ==============
#define PIR_I2C_ADDR    0x42      // Lafvin Nano I2C address
#define PIR_POLL_MS     200       // Poll interval (ms)
#define MOTION_ZONE     "zone1"   // Zone identifier for this sensor

// PIR Nano Register Map
#define PIR_REG_STATUS        0x00
#define PIR_REG_MOTION_COUNT  0x01
#define PIR_REG_LAST_MOTION   0x02
#define PIR_REG_CONFIG        0x03
#define PIR_REG_HOLD_TIME     0x04
#define PIR_REG_VERSION       0x05

// Status bits
#define PIR_STATUS_MOTION     0x01
#define PIR_STATUS_READY      0x02

// ============== TIMING ==============
#define HEARTBEAT_INTERVAL   5000
#define STATE_SYNC_INTERVAL  10000
#define DISPLAY_INTERVAL     500

// ============== MESSAGE TYPES ==============
enum MsgType {
  MSG_HEARTBEAT  = 1,
  MSG_STATE_SET  = 2,
  MSG_STATE_SYNC = 3,
  MSG_STATE_REQ  = 4,
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
unsigned long lastPirPoll = 0;
unsigned long bootTime = 0;

String lastStateChange = "";

// PIR state
bool pirConnected = false;
bool pirReady = false;
bool lastMotionState = false;
uint8_t pirVersion = 0;
uint8_t motionCount = 0;
uint8_t lastMotionSec = 255;

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
void pollPirModule();

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

// ============== PIR I2C FUNCTIONS ==============

uint8_t pirReadRegister(uint8_t reg) {
  Wire.beginTransmission(PIR_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return 0xFF;  // Error
  }

  if (Wire.requestFrom(PIR_I2C_ADDR, (uint8_t)1) != 1) {
    return 0xFF;  // Error
  }

  return Wire.read();
}

bool pirWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PIR_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool pirCheckConnection() {
  Wire.beginTransmission(PIR_I2C_ADDR);
  return Wire.endTransmission() == 0;
}

void pollPirModule() {
  // Check connection
  if (!pirCheckConnection()) {
    if (pirConnected) {
      Serial.println("[PIR] Module disconnected!");
      pirConnected = false;
      pirReady = false;
    }
    return;
  }

  if (!pirConnected) {
    pirConnected = true;
    pirVersion = pirReadRegister(PIR_REG_VERSION);
    Serial.printf("[PIR] Module connected! Version: 0x%02X\n", pirVersion);
  }

  // Read status
  uint8_t status = pirReadRegister(PIR_REG_STATUS);
  if (status == 0xFF) return;  // Read error

  pirReady = (status & PIR_STATUS_READY) != 0;
  bool motionDetected = (status & PIR_STATUS_MOTION) != 0;

  // Read additional info
  lastMotionSec = pirReadRegister(PIR_REG_LAST_MOTION);

  // State changed?
  if (motionDetected != lastMotionState) {
    lastMotionState = motionDetected;

    // Read motion count (auto-clears on Nano)
    motionCount = pirReadRegister(PIR_REG_MOTION_COUNT);

    // Update mesh state
    String motionValue = motionDetected ? "1" : "0";
    setState("motion", motionValue);

    // Also publish zone-specific state
    String zoneKey = String("motion_") + MOTION_ZONE;
    setState(zoneKey, motionValue);

    Serial.printf("[PIR] Motion %s (count: %d)\n",
                  motionDetected ? "DETECTED" : "cleared", motionCount);
  }
}

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════╗");
  Serial.println("║   ESP32 MESH - PIR MOTION NODE    ║");
  Serial.println("╚═══════════════════════════════════╝");
  Serial.println();

  // I2C setup (shared by OLED and PIR module)
  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Init failed!");
  } else {
    Serial.println("[OLED] Initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Mesh PIR Node");
    display.println("Starting...");
    display.display();
  }

  // Check PIR module
  Serial.print("[PIR] Checking I2C address 0x");
  Serial.print(PIR_I2C_ADDR, HEX);
  Serial.print("... ");

  if (pirCheckConnection()) {
    pirConnected = true;
    pirVersion = pirReadRegister(PIR_REG_VERSION);
    Serial.printf("Found! Version: 0x%02X\n", pirVersion);

    // Wait for PIR to be ready
    Serial.println("[PIR] Waiting for sensor ready...");
    unsigned long waitStart = millis();
    while (millis() - waitStart < 35000) {  // Max 35 sec wait
      uint8_t status = pirReadRegister(PIR_REG_STATUS);
      if (status & PIR_STATUS_READY) {
        pirReady = true;
        Serial.println("[PIR] Sensor ready!");
        break;
      }
      delay(1000);
      Serial.print(".");
    }
    if (!pirReady) {
      Serial.println("\n[PIR] Warning: Sensor not ready yet");
    }
  } else {
    Serial.println("Not found!");
  }
  Serial.println();

  // Stagger startup
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
  Serial.printf("[PIR] Zone: %s\n", MOTION_ZONE);
  Serial.println();

  Serial.println("Commands: status, peers, state, pir, set <key> <val>, get <key>");
  Serial.println("─────────────────────────────────────────────────────────────────");
  Serial.println();
}

// ============== MAIN LOOP ==============
void loop() {
  mesh.update();

  unsigned long now = millis();

  // Poll PIR module
  if (now - lastPirPoll >= PIR_POLL_MS) {
    pollPirModule();
    lastPirPoll = now;
  }

  // Heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    pruneDeadPeers();
    lastHeartbeat = now;
  }

  // Periodic full state sync
  if (now - lastStateSync >= STATE_SYNC_INTERVAL) {
    broadcastFullState();
    lastStateSync = now;
  }

  // Display update
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = now;
  }

  // Serial commands
  if (Serial.available()) {
    processSerial();
  }
}

// ============== SHARED STATE FUNCTIONS ==============

bool setState(const String& key, const String& value) {
  String oldValue = "";
  uint32_t newVersion = 1;

  if (sharedState.count(key)) {
    oldValue = sharedState[key].value;
    newVersion = sharedState[key].version + 1;

    if (oldValue == value) {
      return false;
    }
  }

  StateEntry entry;
  entry.value = value;
  entry.version = newVersion;
  entry.origin = myId;
  entry.timestamp = millis();
  sharedState[key] = entry;

  triggerWatchers(key, value, oldValue);
  broadcastState(key);
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
    shouldUpdate = true;
  } else {
    StateEntry& existing = sharedState[key];
    oldValue = existing.value;

    if (version > existing.version) {
      shouldUpdate = true;
    } else if (version == existing.version && origin < existing.origin) {
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

    triggerWatchers(key, value, oldValue);
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
      broadcastFullState();
      break;

    case MSG_COMMAND:
      break;
  }
}

void onNewConnection(uint32_t nodeId) {
  Serial.printf("[MESH] + Connected: %s\n", nodeIdToName(nodeId).c_str());
  sendHeartbeat();
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
  data["pir"] = pirConnected ? 1 : 0;

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

  // Line 3: PIR Status
  display.print("PIR:");
  if (!pirConnected) {
    display.println("DISCONNECTED");
  } else if (!pirReady) {
    display.println("WARMING UP");
  } else {
    display.printf("%s last:%ds\n",
                   lastMotionState ? "MOTION!" : "idle",
                   lastMotionSec);
  }

  // Line 4: Separator
  display.println("------------------------");

  // Lines 5-7: State values (up to 3)
  int shown = 0;
  for (auto& kv : sharedState) {
    if (shown >= 3) break;
    String line = kv.first + "=" + kv.second.value;
    if (line.length() > 21) line = line.substring(0, 21);
    display.println(line);
    shown++;
  }

  while (shown < 3) {
    display.println();
    shown++;
  }

  // Line 8: Last change
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
                    p.second.role.c_str(), p.second.alive ? "OK" : "DEAD");
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
  else if (input == "pir") {
    Serial.println("\n--- PIR MODULE ---");
    Serial.printf("Connected: %s\n", pirConnected ? "YES" : "NO");
    Serial.printf("Ready: %s\n", pirReady ? "YES" : "NO");
    Serial.printf("Version: 0x%02X\n", pirVersion);
    Serial.printf("Motion: %s\n", lastMotionState ? "DETECTED" : "none");
    Serial.printf("Last motion: %d sec ago\n", lastMotionSec);
    Serial.printf("Zone: %s\n", MOTION_ZONE);
    Serial.println();
  }
  else if (input.startsWith("set ")) {
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
  else if (input == "scan") {
    Serial.println("\n--- I2C SCAN ---");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.printf("  Found device at 0x%02X", addr);
        if (addr == OLED_ADDR) Serial.print(" (OLED)");
        if (addr == PIR_I2C_ADDR) Serial.print(" (PIR Nano)");
        Serial.println();
        found++;
      }
    }
    Serial.printf("Found %d device(s)\n\n", found);
  }
  else if (input == "reboot") {
    ESP.restart();
  }
  else {
    Serial.println("Commands: status, peers, state, pir, scan, set <k> <v>, get <k>, sync, reboot");
  }
}

/*
 * ESP32 Mesh Swarm - Base Firmware
 * 
 * Self-organizing, self-healing mesh network.
 * Flash identical code to all nodes.
 * 
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 * 
 * Features:
 *   - Auto peer discovery
 *   - Coordinator election (lowest node ID)
 *   - Self-healing (auto reconnect, re-election)
 *   - Heartbeat health monitoring
 *   - OLED status display
 *   - Extensible message system
 * 
 * Libraries (install via Library Manager):
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

// ============== MESH CONFIGURATION ==============
#define MESH_PREFIX     "swarm"
#define MESH_PASSWORD   "swarmnet123"
#define MESH_PORT       5555

// ============== OLED CONFIGURATION ==============
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR       0x3C  // Try 0x3D if display doesn't work
#define I2C_SDA         21
#define I2C_SCL         22

// ============== TIMING ==============
#define HEARTBEAT_INTERVAL   5000   // Heartbeat broadcast (ms)
#define PEER_TIMEOUT         15000  // Mark peer stale after this (ms)
#define DISPLAY_INTERVAL     500    // OLED refresh (ms)

//=========== SENSOR ACTUATOR ======= 
#define ENABLE_BUTTON        // Keep this
// #define ENABLE_LED        // Comment out


// ============== MESSAGE TYPES ==============
enum MsgType {
  MSG_HEARTBEAT = 1,
  MSG_DATA      = 2,
  MSG_COMMAND   = 3,
  MSG_RESPONSE  = 4
};

// ============== DATA STRUCTURES ==============
struct Peer {
  uint32_t id;
  String name;
  String role;
  unsigned long lastSeen;
  uint32_t uptime;
  bool alive;
};

struct StateEntry {
  String value;       // The value (as string, can be JSON)
  uint32_t version;   // Increment on each change (conflict resolution)
  uint32_t origin;    // Node ID that set it
  unsigned long time; // Local timestamp when received
};

// ============== GLOBALS ==============
painlessMesh mesh;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

uint32_t myId = 0;
String myName = "";
String myRole = "PEER";
uint32_t coordinatorId = 0;

std::map<uint32_t, Peer> peers;

unsigned long lastHeartbeat = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long bootTime = 0;

String lastMessage = "";  // For display

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

String createMsg(MsgType type, JsonDocument &data);
String nodeIdToName(uint32_t id);

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════╗");
  Serial.println("║     ESP32 MESH SWARM v1.0         ║");
  Serial.println("╚═══════════════════════════════════╝");
  Serial.println();

  // Initialize I2C and OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Init failed! Check wiring.");
  } else {
    Serial.println("[OLED] Initialized");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Mesh Swarm");
    display.println("Starting...");
    display.display();
  }

  // Initialize mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  
  mesh.onReceive(&onReceive);
  mesh.onNewConnection(&onNewConnection);
  mesh.onDroppedConnection(&onDroppedConnection);
  mesh.onChangedConnections(&onChangedConnections);

  // Get our identity
  myId = mesh.getNodeId();
  myName = nodeIdToName(myId);
  bootTime = millis();
  
  Serial.printf("[MESH] Node ID: %u\n", myId);
  Serial.printf("[MESH] Name: %s\n", myName.c_str());
  Serial.printf("[MESH] Network: %s\n", MESH_PREFIX);
  Serial.println();
  Serial.println("Commands: status, peers, ping, reboot");
  Serial.println("─────────────────────────────────────");
  Serial.println();
}

// ============== MAIN LOOP ==============
void loop() {
  mesh.update();
  
  unsigned long now = millis();
  
  // Periodic heartbeat
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    pruneDeadPeers();
    lastHeartbeat = now;
  }
  
  // Update display
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // Serial commands
  if (Serial.available()) {
    processSerial();
  }
}

// ============== MESH CALLBACKS ==============

void onReceive(uint32_t from, String &msg) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, msg);
  
  if (err) {
    Serial.printf("[RX] JSON error from %u: %s\n", from, err.c_str());
    return;
  }
  
  MsgType type = (MsgType)doc["t"].as<int>();
  String senderName = doc["n"] | "???";
  JsonObject data = doc["d"].as<JsonObject>();
  
  switch (type) {
    case MSG_HEARTBEAT: {
      // Update peer info
      Peer &p = peers[from];
      p.id = from;
      p.name = senderName;
      p.role = data["role"] | "PEER";
      p.uptime = data["up"] | 0;
      p.lastSeen = millis();
      p.alive = true;
      
      // Re-elect if needed
      electCoordinator();
      break;
    }
    
    case MSG_DATA: {
      // Store for display, extensible for sensor data
      String dtype = data["type"] | "unknown";
      lastMessage = senderName + ":" + dtype;
      Serial.printf("[DATA] %s -> %s\n", senderName.c_str(), dtype.c_str());
      break;
    }
    
    case MSG_COMMAND: {
      String cmd = data["cmd"] | "";
      Serial.printf("[CMD] %s from %s\n", cmd.c_str(), senderName.c_str());
      // Handle commands here
      break;
    }
    
    case MSG_RESPONSE: {
      String res = data["res"] | "";
      Serial.printf("[RSP] %s from %s\n", res.c_str(), senderName.c_str());
      break;
    }
  }
}

void onNewConnection(uint32_t nodeId) {
  Serial.printf("[MESH] + Connected: %u (%s)\n", nodeId, nodeIdToName(nodeId).c_str());
  
  // Send immediate heartbeat to introduce ourselves
  sendHeartbeat();
}

void onDroppedConnection(uint32_t nodeId) {
  Serial.printf("[MESH] - Dropped: %u (%s)\n", nodeId, nodeIdToName(nodeId).c_str());
  
  // Mark as dead
  if (peers.count(nodeId)) {
    peers[nodeId].alive = false;
  }
  
  // Re-elect coordinator if needed
  electCoordinator();
}

void onChangedConnections() {
  Serial.printf("[MESH] Topology changed. Nodes: %d\n", mesh.getNodeList().size());
  electCoordinator();
}

// ============== COORDINATOR ELECTION ==============

void electCoordinator() {
  auto nodeList = mesh.getNodeList();
  
  // Start with ourselves as candidate
  uint32_t lowest = myId;
  
  // Check all connected nodes
  for (auto &id : nodeList) {
    if (id < lowest) {
      lowest = id;
    }
  }
  
  // Update role
  String oldRole = myRole;
  coordinatorId = lowest;
  myRole = (lowest == myId) ? "COORD" : "PEER";
  
  if (oldRole != myRole) {
    Serial.printf("[ROLE] %s -> %s\n", oldRole.c_str(), myRole.c_str());
    if (myRole == "COORD") {
      Serial.println("[ROLE] ★ I am now coordinator");
    }
  }
}

// ============== HEARTBEAT ==============

void sendHeartbeat() {
  JsonDocument data;
  data["role"] = myRole;
  data["up"] = (millis() - bootTime) / 1000;
  data["heap"] = ESP.getFreeHeap();
  data["peers"] = mesh.getNodeList().size();
  
  String msg = createMsg(MSG_HEARTBEAT, data);
  mesh.sendBroadcast(msg);
}

void pruneDeadPeers() {
  unsigned long now = millis();
  
  for (auto it = peers.begin(); it != peers.end(); ) {
    if (now - it->second.lastSeen > PEER_TIMEOUT) {
      Serial.printf("[PEER] Pruned stale: %s\n", it->second.name.c_str());
      it = peers.erase(it);
    } else {
      ++it;
    }
  }
}

// ============== MESSAGE HELPERS ==============

String createMsg(MsgType type, JsonDocument &data) {
  JsonDocument doc;
  doc["t"] = type;
  doc["n"] = myName;
  doc["d"] = data;
  
  String out;
  serializeJson(doc, out);
  return out;
}

String nodeIdToName(uint32_t id) {
  // Create short readable name from ID
  String hex = String(id, HEX);
  hex.toUpperCase();
  // Take last 4 chars
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
  
  // Line 1: Identity and role
  uint32_t uptime = (millis() - bootTime) / 1000;
  uint32_t mins = uptime / 60;
  uint32_t secs = uptime % 60;
  
  display.printf("%s [%s] %d:%02d\n", 
                 myName.c_str(), 
                 myRole.c_str(),
                 mins, secs);
  
  // Line 2: Separator
  display.println("------------------------");
  
  // Line 3: Peer count
  int alivePeers = 0;
  for (auto &p : peers) {
    if (p.second.alive) alivePeers++;
  }
  display.printf("Peers: %d\n", alivePeers);
  
  // Lines 4-5: Peer list (up to 4 peers shown)
  int shown = 0;
  String peerLine1 = "";
  String peerLine2 = "";
  
  for (auto &p : peers) {
    if (!p.second.alive) continue;
    
    String entry = p.second.name;
    if (p.second.id == coordinatorId) {
      entry += "*";  // Mark coordinator
    }
    entry += " ";
    
    if (shown < 2) {
      peerLine1 += entry;
    } else if (shown < 4) {
      peerLine2 += entry;
    }
    shown++;
  }
  
  if (peerLine1.length() > 0) display.println(peerLine1);
  else display.println("(searching...)");
  
  if (peerLine2.length() > 0) display.println(peerLine2);
  else display.println();
  
  // Line 6-7: Last message or status
  display.println("------------------------");
  if (lastMessage.length() > 0) {
    display.printf("Last: %s\n", lastMessage.substring(0, 20).c_str());
  } else {
    display.printf("Heap: %u\n", ESP.getFreeHeap());
  }
  
  // Coordinator indicator at bottom
  if (myRole == "COORD") {
    display.setCursor(100, 56);
    display.print("[*]");
  }
  
  display.display();
}

// ============== SERIAL INTERFACE ==============

void processSerial() {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();
  
  if (cmd == "status") {
    Serial.println();
    Serial.println("╔═══════════════════════════════════╗");
    Serial.println("║           NODE STATUS             ║");
    Serial.println("╠═══════════════════════════════════╣");
    Serial.printf("║  ID:    %u\n", myId);
    Serial.printf("║  Name:  %s\n", myName.c_str());
    Serial.printf("║  Role:  %s\n", myRole.c_str());
    Serial.printf("║  Coordinator: %s\n", nodeIdToName(coordinatorId).c_str());
    Serial.printf("║  Uptime: %lu sec\n", (millis() - bootTime) / 1000);
    Serial.printf("║  Heap:  %u bytes\n", ESP.getFreeHeap());
    Serial.printf("║  Peers: %d\n", peers.size());
    Serial.println("╚═══════════════════════════════════╝");
    Serial.println();
  }
  else if (cmd == "peers") {
    Serial.println("\n--- Peers ---");
    if (peers.empty()) {
      Serial.println("  (none)");
    } else {
      for (auto &p : peers) {
        Peer &peer = p.second;
        unsigned long age = (millis() - peer.lastSeen) / 1000;
        Serial.printf("  %s [%s] up=%us seen=%lus ago %s\n",
                      peer.name.c_str(),
                      peer.role.c_str(),
                      peer.uptime,
                      age,
                      peer.alive ? "✓" : "✗");
      }
    }
    Serial.println();
  }
  else if (cmd == "ping") {
    Serial.println("[PING] Broadcasting heartbeat");
    sendHeartbeat();
  }
  else if (cmd == "reboot") {
    Serial.println("[REBOOT] Restarting...");
    delay(500);
    ESP.restart();
  }
  else if (cmd == "help" || cmd == "?") {
    Serial.println("\nCommands:");
    Serial.println("  status  - Show node info");
    Serial.println("  peers   - List known peers");
    Serial.println("  ping    - Send heartbeat");
    Serial.println("  reboot  - Restart node");
    Serial.println();
  }
  else if (cmd.length() > 0) {
    Serial.printf("Unknown: %s (try 'help')\n", cmd.c_str());
  }
}

// ============== EXTENSION POINTS ==============

/*
 * To add sensor capability:
 * 
 * void sendSensorData(const char* type, float value) {
 *   JsonDocument data;
 *   data["type"] = type;
 *   data["value"] = value;
 *   String msg = createMsg(MSG_DATA, data);
 *   mesh.sendBroadcast(msg);
 * }
 * 
 * Call from loop: sendSensorData("temp", 22.5);
 */

/*
 * To add command handling:
 * 
 * In MSG_COMMAND case:
 *   if (cmd == "SET_LED") {
 *     bool state = data["value"] | false;
 *     digitalWrite(LED_PIN, state);
 *   }
 */

/*
 * To send targeted message:
 * 
 * void sendTo(uint32_t targetId, MsgType type, JsonDocument &data) {
 *   String msg = createMsg(type, data);
 *   mesh.sendSingle(targetId, msg);
 * }
 */

/*
 * MeshSwarm Library Implementation
 */

#include "MeshSwarm.h"

// ============== CONSTRUCTOR ==============
MeshSwarm::MeshSwarm()
  : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
    myId(0),
    myName(""),
    myRole("PEER"),
    coordinatorId(0),
    lastHeartbeat(0),
    lastStateSync(0),
    lastDisplayUpdate(0),
    lastTelemetryPush(0),
    bootTime(0),
    telemetryUrl(""),
    telemetryApiKey(""),
    telemetryInterval(TELEMETRY_INTERVAL),
    telemetryEnabled(false),
    gatewayMode(false),
    lastStateChange(""),
    customStatus("")
{
}

// ============== INITIALIZATION ==============
void MeshSwarm::begin(const char* nodeName) {
  begin(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, nodeName);
}

void MeshSwarm::begin(const char* prefix, const char* password, uint16_t port, const char* nodeName) {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("       ESP32 MESH SWARM NODE");
  Serial.println("========================================");
  Serial.println();

  // Initialize display
  initDisplay();

  // Stagger startup to reduce collisions
  uint32_t chipId = ESP.getEfuseMac() & 0xFFFF;
  uint32_t startDelay = (chipId % 3) * 500;
  Serial.printf("[BOOT] Startup delay: %dms\n", startDelay);
  delay(startDelay);

  // Initialize mesh
  initMesh(prefix, password, port);

  myId = mesh.getNodeId();
  myName = nodeName ? String(nodeName) : nodeIdToName(myId);
  bootTime = millis();

  Serial.printf("[MESH] Node ID: %u\n", myId);
  Serial.printf("[MESH] Name: %s\n", myName.c_str());
  Serial.println();
  Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, reboot");
  Serial.println("----------------------------------------");
  Serial.println();
}

void MeshSwarm::initDisplay() {
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
    display.println("Starting...");
    display.display();
  }
}

void MeshSwarm::initMesh(const char* prefix, const char* password, uint16_t port) {
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(prefix, password, port);

  // Use lambdas to capture 'this' for callbacks
  mesh.onReceive([this](uint32_t from, String &msg) {
    this->onReceive(from, msg);
  });

  mesh.onNewConnection([this](uint32_t nodeId) {
    this->onNewConnection(nodeId);
  });

  mesh.onDroppedConnection([this](uint32_t nodeId) {
    this->onDroppedConnection(nodeId);
  });

  mesh.onChangedConnections([this]() {
    this->onChangedConnections();
  });
}

// ============== MAIN LOOP ==============
void MeshSwarm::update() {
  mesh.update();

  unsigned long now = millis();

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

  // Telemetry push
  if (telemetryEnabled && (now - lastTelemetryPush >= telemetryInterval)) {
    if (gatewayMode) {
      // Gateway pushes its own telemetry directly
      pushTelemetry();
    } else {
      // Regular node sends telemetry via mesh to gateway
      sendTelemetryToGateway();
    }
    lastTelemetryPush = now;
  }

  // Serial commands
  if (Serial.available()) {
    processSerial();
  }

  // Custom loop callbacks
  for (auto& cb : loopCallbacks) {
    cb();
  }
}

// ============== STATE MANAGEMENT ==============
bool MeshSwarm::setState(const String& key, const String& value) {
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

  // Push telemetry on state change
  if (telemetryEnabled) {
    if (gatewayMode) {
      pushTelemetry();
    } else {
      sendTelemetryToGateway();
    }
    lastTelemetryPush = millis();
  }

  return true;
}

String MeshSwarm::getState(const String& key, const String& defaultVal) {
  if (sharedState.count(key)) {
    return sharedState[key].value;
  }
  return defaultVal;
}

void MeshSwarm::watchState(const String& key, StateCallback callback) {
  stateWatchers[key].push_back(callback);
}

void MeshSwarm::triggerWatchers(const String& key, const String& value, const String& oldValue) {
  if (stateWatchers.count(key)) {
    for (auto& cb : stateWatchers[key]) {
      cb(key, value, oldValue);
    }
  }

  // Wildcard watchers
  if (stateWatchers.count("*")) {
    for (auto& cb : stateWatchers["*"]) {
      cb(key, value, oldValue);
    }
  }
}

void MeshSwarm::broadcastState(const String& key) {
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

void MeshSwarm::broadcastFullState() {
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

void MeshSwarm::requestStateSync() {
  JsonDocument data;
  data["req"] = 1;
  String msg = createMsg(MSG_STATE_REQ, data);
  mesh.sendBroadcast(msg);
}

void MeshSwarm::handleStateSet(uint32_t from, JsonObject& data) {
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

void MeshSwarm::handleStateSync(uint32_t from, JsonObject& data) {
  JsonArray arr = data["s"].as<JsonArray>();

  for (JsonObject entry : arr) {
    handleStateSet(from, entry);
  }

  Serial.printf("[SYNC] Received %d state entries from %s\n",
                arr.size(), nodeIdToName(from).c_str());
}

// ============== MESH CALLBACKS ==============
void MeshSwarm::onReceive(uint32_t from, String &msg) {
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

    case MSG_TELEMETRY:
      // Only gateway handles telemetry messages
      if (gatewayMode) {
        handleTelemetry(from, data);
      }
      break;
  }
}

void MeshSwarm::onNewConnection(uint32_t nodeId) {
  Serial.printf("[MESH] + Connected: %s\n", nodeIdToName(nodeId).c_str());
  sendHeartbeat();
  broadcastFullState();
}

void MeshSwarm::onDroppedConnection(uint32_t nodeId) {
  Serial.printf("[MESH] - Dropped: %s\n", nodeIdToName(nodeId).c_str());
  if (peers.count(nodeId)) {
    peers[nodeId].alive = false;
  }
  electCoordinator();
}

void MeshSwarm::onChangedConnections() {
  Serial.printf("[MESH] Topology changed. Nodes: %d\n", mesh.getNodeList().size());
  electCoordinator();
}

// ============== COORDINATOR ELECTION ==============
void MeshSwarm::electCoordinator() {
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
void MeshSwarm::sendHeartbeat() {
  JsonDocument data;
  data["role"] = myRole;
  data["up"] = (millis() - bootTime) / 1000;
  data["heap"] = ESP.getFreeHeap();
  data["states"] = sharedState.size();

  // Add custom heartbeat data
  for (auto& kv : heartbeatExtras) {
    data[kv.first] = kv.second;
  }

  String msg = createMsg(MSG_HEARTBEAT, data);
  mesh.sendBroadcast(msg);
}

void MeshSwarm::pruneDeadPeers() {
  unsigned long now = millis();
  for (auto it = peers.begin(); it != peers.end(); ) {
    if (now - it->second.lastSeen > 15000) {
      it = peers.erase(it);
    } else {
      ++it;
    }
  }
}

int MeshSwarm::getPeerCount() {
  int count = 0;
  for (auto &p : peers) {
    if (p.second.alive) count++;
  }
  return count;
}

// ============== CUSTOMIZATION ==============
void MeshSwarm::onLoop(LoopCallback callback) {
  loopCallbacks.push_back(callback);
}

void MeshSwarm::onSerialCommand(SerialHandler handler) {
  serialHandlers.push_back(handler);
}

void MeshSwarm::onDisplayUpdate(DisplayHandler handler) {
  displayHandlers.push_back(handler);
}

void MeshSwarm::setStatusLine(const String& status) {
  customStatus = status;
}

void MeshSwarm::setHeartbeatData(const String& key, int value) {
  heartbeatExtras[key] = value;
}

// ============== HELPERS ==============
String MeshSwarm::createMsg(MsgType type, JsonDocument& data) {
  JsonDocument doc;
  doc["t"] = type;
  doc["n"] = myName;
  doc["d"] = data;

  String out;
  serializeJson(doc, out);
  return out;
}

String MeshSwarm::nodeIdToName(uint32_t id) {
  String hex = String(id, HEX);
  hex.toUpperCase();
  if (hex.length() > 4) {
    hex = hex.substring(hex.length() - 4);
  }
  return "N" + hex;
}

// ============== DISPLAY ==============
void MeshSwarm::updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Line 1: Identity
  uint32_t uptime = (millis() - bootTime) / 1000;
  display.printf("%s [%s] %d:%02d\n", myName.c_str(), myRole.c_str(), uptime/60, uptime%60);

  // Line 2: Network
  display.printf("Peers:%d States:%d\n", getPeerCount(), sharedState.size());

  // Line 3: Custom status or separator
  if (customStatus.length() > 0) {
    display.println(customStatus.substring(0, 21));
  } else {
    display.println("---------------------");
  }

  // Call custom display handlers (lines 4+)
  int startLine = 3;
  for (auto& handler : displayHandlers) {
    handler(display, startLine);
  }

  // If no custom handlers, show state values
  if (displayHandlers.empty()) {
    // Lines 4-7: State values (up to 4)
    int shown = 0;
    for (auto& kv : sharedState) {
      if (shown >= 4) break;
      String line = kv.first + "=" + kv.second.value;
      if (line.length() > 21) line = line.substring(0, 21);
      display.println(line);
      shown++;
    }

    while (shown < 4) {
      display.println();
      shown++;
    }

    // Line 8: Last change
    if (lastStateChange.length() > 0) {
      display.printf("Last:%s\n", lastStateChange.substring(0, 16).c_str());
    }
  }

  display.display();
}

// ============== SERIAL COMMANDS ==============
void MeshSwarm::processSerial() {
  String input = Serial.readStringUntil('\n');
  input.trim();

  if (input.length() == 0) return;

  // Try custom handlers first
  for (auto& handler : serialHandlers) {
    if (handler(input)) {
      return;  // Handler consumed the command
    }
  }

  // Built-in commands
  if (input == "status") {
    Serial.println("\n--- NODE STATUS ---");
    Serial.printf("ID: %u (%s)\n", myId, myName.c_str());
    Serial.printf("Role: %s\n", myRole.c_str());
    Serial.printf("Peers: %d\n", getPeerCount());
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
        Serial.printf("  Found device at 0x%02X\n", addr);
        found++;
      }
    }
    Serial.printf("Found %d device(s)\n\n", found);
  }
  else if (input == "reboot") {
    ESP.restart();
  }
  else if (input == "telem") {
    Serial.println("\n--- TELEMETRY STATUS ---");
    Serial.printf("Enabled: %s\n", telemetryEnabled ? "YES" : "NO");
    Serial.printf("Gateway: %s\n", gatewayMode ? "YES" : "NO");
    if (gatewayMode) {
      Serial.printf("URL: %s\n", telemetryUrl.length() > 0 ? telemetryUrl.c_str() : "(not set)");
      Serial.printf("WiFi: %s\n", isWiFiConnected() ? "Connected" : "Not connected");
      if (isWiFiConnected()) {
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
      }
    } else {
      Serial.println("Mode: Sending via mesh to gateway");
    }
    Serial.printf("Interval: %lu ms\n", telemetryInterval);
    Serial.println();
  }
  else if (input == "push") {
    if (telemetryEnabled) {
      pushTelemetry();
      Serial.println("[TELEM] Manual push triggered");
    } else {
      Serial.println("[TELEM] Telemetry not enabled");
    }
  }
  else {
    Serial.println("Commands: status, peers, state, set <k> <v>, get <k>, sync, scan, telem, push, reboot");
  }
}

// ============== TELEMETRY ==============
void MeshSwarm::setTelemetryServer(const char* url, const char* apiKey) {
  telemetryUrl = String(url);
  if (apiKey != nullptr) {
    telemetryApiKey = String(apiKey);
  }
  Serial.printf("[TELEM] Server: %s\n", telemetryUrl.c_str());
}

void MeshSwarm::setTelemetryInterval(unsigned long ms) {
  telemetryInterval = ms;
  Serial.printf("[TELEM] Interval: %lu ms\n", telemetryInterval);
}

void MeshSwarm::enableTelemetry(bool enable) {
  telemetryEnabled = enable;
  Serial.printf("[TELEM] %s\n", enable ? "Enabled" : "Disabled");
}

void MeshSwarm::connectToWiFi(const char* ssid, const char* password) {
  // painlessMesh supports station mode alongside mesh
  mesh.stationManual(ssid, password);
  Serial.printf("[WIFI] Connecting to %s...\n", ssid);
}

bool MeshSwarm::isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void MeshSwarm::pushTelemetry() {
  if (!telemetryEnabled || telemetryUrl.length() == 0) {
    return;
  }

  if (!isWiFiConnected()) {
    Serial.println("[TELEM] WiFi not connected, skipping push");
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/nodes/" + String(myId, HEX) + "/telemetry";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
  http.setTimeout(5000);  // 5 second timeout

  // Build JSON payload
  JsonDocument doc;
  doc["name"] = myName;
  doc["uptime"] = (millis() - bootTime) / 1000;
  doc["heap_free"] = ESP.getFreeHeap();
  doc["peer_count"] = getPeerCount();
  doc["role"] = myRole;
  doc["firmware"] = FIRMWARE_VERSION;

  // Include all shared state
  JsonObject state = doc["state"].to<JsonObject>();
  for (auto& entry : sharedState) {
    state[entry.first] = entry.second.value;
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  if (httpCode == 200 || httpCode == 201) {
    Serial.println("[TELEM] Push OK");
  } else {
    Serial.printf("[TELEM] Push failed: %d\n", httpCode);
  }
  http.end();
}

// ============== GATEWAY MODE ==============
void MeshSwarm::setGatewayMode(bool enable) {
  gatewayMode = enable;
  Serial.printf("[GATEWAY] %s\n", enable ? "Enabled" : "Disabled");
}

void MeshSwarm::sendTelemetryToGateway() {
  // Build telemetry data
  JsonDocument data;
  data["name"] = myName;
  data["uptime"] = (millis() - bootTime) / 1000;
  data["heap_free"] = ESP.getFreeHeap();
  data["peer_count"] = getPeerCount();
  data["role"] = myRole;
  data["firmware"] = FIRMWARE_VERSION;

  // Include all shared state
  JsonObject state = data["state"].to<JsonObject>();
  for (auto& entry : sharedState) {
    state[entry.first] = entry.second.value;
  }

  // Send via mesh broadcast (gateway will pick it up)
  String msg = createMsg(MSG_TELEMETRY, data);
  mesh.sendBroadcast(msg);

  Serial.println("[TELEM] Sent to gateway via mesh");
}

void MeshSwarm::handleTelemetry(uint32_t from, JsonObject& data) {
  // Gateway received telemetry from another node - push to server
  Serial.printf("[GATEWAY] Received telemetry from %s\n", nodeIdToName(from).c_str());

  // Debug: dump the telemetry payload
  String debugPayload;
  serializeJson(data, debugPayload);
  Serial.printf("[GATEWAY] Payload: %s\n", debugPayload.c_str());

  pushTelemetryForNode(from, data);
}

void MeshSwarm::pushTelemetryForNode(uint32_t nodeId, JsonObject& data) {
  if (!isWiFiConnected() || telemetryUrl.length() == 0) {
    Serial.println("[GATEWAY] Cannot push - WiFi not connected or no server URL");
    return;
  }

  HTTPClient http;
  String url = telemetryUrl + "/api/v1/nodes/" + String(nodeId, HEX) + "/telemetry";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  if (telemetryApiKey.length() > 0) {
    http.addHeader("X-API-Key", telemetryApiKey);
  }
  http.setTimeout(5000);

  String payload;
  serializeJson(data, payload);

  int httpCode = http.POST(payload);
  if (httpCode == 200 || httpCode == 201) {
    Serial.printf("[GATEWAY] Push OK for %s\n", nodeIdToName(nodeId).c_str());
  } else {
    Serial.printf("[GATEWAY] Push failed for %s: %d\n", nodeIdToName(nodeId).c_str(), httpCode);
  }
  http.end();
}

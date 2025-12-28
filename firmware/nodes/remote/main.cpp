/**
 * Remote Control Node - Cheap Yellow Display (ESP32-2432S028R)
 *
 * Touch-enabled dashboard for monitoring and controlling mesh network nodes.
 * Displays active nodes and allows navigation to view node state.
 *
 * Features:
 *   - 2.4" TFT display (240x320 ILI9341)
 *   - Touch interface for navigation
 *   - Shows all active mesh nodes
 *   - Click node to view detailed state
 *   - Displays sensor values and actuator status
 *
 * Hardware:
 *   - ESP32-2432S028R "Cheap Yellow Display"
 *   - 2.4" IPS TFT LCD (240x320)
 *   - Resistive touch screen
 *
 * Display Layout:
 *   - Header: Network status (peers, uptime)
 *   - Main: Grid of active node buttons
 *   - Detail: Selected node's state key/values
 *   - Footer: Back button when in detail view
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <TFT_eSPI.h>
#include <esp_ota_ops.h>

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Remote"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "remote"
#endif

// Display configuration for ESP32-2432S028R
// Note: Pin configuration is in platformio.ini build_flags
#define TFT_WIDTH 240
#define TFT_HEIGHT 320

// Touch calibration (may need adjustment per device)
#define TOUCH_THRESHOLD 600

// UI Layout
#define HEADER_HEIGHT 30
#define FOOTER_HEIGHT 40
#define NODE_BUTTON_HEIGHT 50
#define NODE_BUTTON_MARGIN 5
#define NODES_PER_PAGE 5

// Colors (RGB565)
#define COLOR_BG 0x0000        // Black
#define COLOR_HEADER 0x001F    // Blue
#define COLOR_TEXT 0xFFFF      // White
#define COLOR_NODE_BTN 0x07E0  // Green
#define COLOR_NODE_ACTIVE 0x07FF  // Cyan
#define COLOR_FOOTER 0xF800    // Red
#define COLOR_DETAIL_BG 0x18E3 // Dark gray

// ============== GLOBALS ==============
MeshSwarm swarm;
TFT_eSPI tft = TFT_eSPI();

// UI State
enum ViewMode {
  VIEW_NODE_LIST,
  VIEW_NODE_DETAIL
};

ViewMode currentView = VIEW_NODE_LIST;
uint32_t selectedNodeId = 0;
String selectedNodeName = "";

// State cache for detail view
struct StateItem {
  String key;
  String value;
  uint32_t origin;
};
std::vector<StateItem> stateCache;

// Touch handling
unsigned long lastTouchTime = 0;
#define TOUCH_DEBOUNCE 250

// Display refresh
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 1000

// Track state changes for refresh
bool stateChanged = false;

// ============== TOUCH HANDLING ==============

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool valid;
};

TouchPoint getTouch() {
  TouchPoint tp;
  tp.valid = false;
  
  uint16_t touchX = 0, touchY = 0;
  bool pressed = tft.getTouch(&touchX, &touchY, TOUCH_THRESHOLD);
  
  if (pressed) {
    tp.x = touchX;
    tp.y = touchY;
    tp.valid = true;
    
    // Constrain to screen bounds
    tp.x = constrain(tp.x, 0, TFT_WIDTH - 1);
    tp.y = constrain(tp.y, 0, TFT_HEIGHT - 1);
  }
  
  return tp;
}

// ============== DISPLAY RENDERING ==============

void drawHeader() {
  tft.fillRect(0, 0, TFT_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
  tft.setTextColor(COLOR_TEXT, COLOR_HEADER);
  tft.setTextSize(1);
  
  // Title
  tft.setCursor(5, 5);
  tft.print("MeshSwarm Remote");
  
  // Network info
  int peers = swarm.getPeerCount();
  unsigned long uptime = millis() / 1000;
  tft.setCursor(5, 17);
  tft.printf("Peers:%d Up:%02d:%02d", peers, uptime/60, uptime%60);
}

void drawNodeList() {
  // Clear main area
  tft.fillRect(0, HEADER_HEIGHT, TFT_WIDTH, TFT_HEIGHT - HEADER_HEIGHT, COLOR_BG);
  
  // Get list of peers - Peer is in global namespace, not MeshSwarm::
  std::map<uint32_t, Peer>& peers = swarm.getPeers();
  
  if (peers.empty()) {
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(20, 120);
    tft.print("No nodes found");
    tft.setTextSize(1);
    tft.setCursor(20, 145);
    tft.print("Waiting for mesh...");
    return;
  }
  
  // Draw node buttons
  int yPos = HEADER_HEIGHT + NODE_BUTTON_MARGIN;
  int nodeIndex = 0;
  
  for (auto& kv : peers) {
    if (!kv.second.alive) continue;
    if (nodeIndex >= NODES_PER_PAGE) break;
    
    uint32_t nodeId = kv.first;
    String nodeName = kv.second.name;
    String nodeRole = kv.second.role;
    
    // Draw button
    uint16_t btnColor = COLOR_NODE_BTN;
    tft.fillRoundRect(NODE_BUTTON_MARGIN, yPos, 
                      TFT_WIDTH - 2*NODE_BUTTON_MARGIN, 
                      NODE_BUTTON_HEIGHT - NODE_BUTTON_MARGIN,
                      5, btnColor);
    
    // Draw text
    tft.setTextColor(COLOR_BG, btnColor);
    tft.setTextSize(2);
    tft.setCursor(NODE_BUTTON_MARGIN + 10, yPos + 8);
    tft.print(nodeName);
    
    tft.setTextSize(1);
    tft.setCursor(NODE_BUTTON_MARGIN + 10, yPos + 30);
    tft.printf("Role: %s", nodeRole.c_str());
    
    yPos += NODE_BUTTON_HEIGHT;
    nodeIndex++;
  }
}

void updateStateCache() {
  stateCache.clear();
  
  // Use state watcher approach - we'll collect all state we see
  // For now, just note that state is tracked via watchers
  // In a future enhancement, we could expose getSharedState() in MeshSwarm
}

void drawNodeDetail() {
  // Clear main area
  tft.fillRect(0, HEADER_HEIGHT, TFT_WIDTH, TFT_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT, COLOR_DETAIL_BG);
  
  // Node title
  tft.setTextColor(COLOR_TEXT, COLOR_DETAIL_BG);
  tft.setTextSize(2);
  tft.setCursor(10, HEADER_HEIGHT + 10);
  tft.print(selectedNodeName);
  
  int yPos = HEADER_HEIGHT + 40;
  
  tft.setTextSize(1);
  tft.setCursor(10, yPos);
  
  if (stateCache.empty()) {
    tft.print("No state data");
    tft.setCursor(10, yPos + 20);
    tft.print("State updates will");
    tft.setCursor(10, yPos + 35);
    tft.print("appear here...");
  } else {
    int shown = 0;
    const int maxEntries = 8;
    
    for (auto& item : stateCache) {
      if (shown >= maxEntries) break;
      
      tft.setCursor(10, yPos);
      tft.setTextColor(COLOR_NODE_ACTIVE, COLOR_DETAIL_BG);
      tft.print(item.key);
      tft.print(": ");
      tft.setTextColor(COLOR_TEXT, COLOR_DETAIL_BG);
      tft.print(item.value);
      
      yPos += 20;
      shown++;
    }
  }
  
  // Draw footer (back button)
  tft.fillRect(0, TFT_HEIGHT - FOOTER_HEIGHT, TFT_WIDTH, FOOTER_HEIGHT, COLOR_FOOTER);
  tft.setTextColor(COLOR_TEXT, COLOR_FOOTER);
  tft.setTextSize(2);
  tft.setCursor(TFT_WIDTH/2 - 30, TFT_HEIGHT - FOOTER_HEIGHT + 12);
  tft.print("< BACK");
}

void updateDisplay() {
  drawHeader();
  
  switch (currentView) {
    case VIEW_NODE_LIST:
      drawNodeList();
      break;
    case VIEW_NODE_DETAIL:
      drawNodeDetail();
      break;
  }
}

// ============== TOUCH EVENT HANDLERS ==============

void handleTouchNodeList(int16_t x, int16_t y) {
  // Check if touch is in node button area
  if (y < HEADER_HEIGHT || y > TFT_HEIGHT) return;
  
  // Calculate which node was touched
  int nodeIndex = (y - HEADER_HEIGHT - NODE_BUTTON_MARGIN) / NODE_BUTTON_HEIGHT;
  
  if (nodeIndex < 0 || nodeIndex >= NODES_PER_PAGE) return;
  
  // Find the corresponding peer
  std::map<uint32_t, Peer>& peers = swarm.getPeers();
  int currentIndex = 0;
  
  for (auto& kv : peers) {
    if (!kv.second.alive) continue;
    
    if (currentIndex == nodeIndex) {
      // Switch to detail view
      selectedNodeId = kv.first;
      selectedNodeName = kv.second.name;
      currentView = VIEW_NODE_DETAIL;
      updateStateCache();
      updateDisplay();
      
      Serial.printf("[TOUCH] Selected node: %s (ID: %08X)\n", 
                    selectedNodeName.c_str(), selectedNodeId);
      break;
    }
    currentIndex++;
  }
}

void handleTouchNodeDetail(int16_t x, int16_t y) {
  // Check if back button was pressed
  if (y >= TFT_HEIGHT - FOOTER_HEIGHT && y <= TFT_HEIGHT) {
    currentView = VIEW_NODE_LIST;
    selectedNodeId = 0;
    selectedNodeName = "";
    updateDisplay();
    Serial.println("[TOUCH] Back to node list");
  }
}

void handleTouch() {
  unsigned long now = millis();
  if (now - lastTouchTime < TOUCH_DEBOUNCE) return;
  
  TouchPoint tp = getTouch();
  if (!tp.valid) return;
  
  lastTouchTime = now;
  
  Serial.printf("[TOUCH] x=%d, y=%d\n", tp.x, tp.y);
  
  switch (currentView) {
    case VIEW_NODE_LIST:
      handleTouchNodeList(tp.x, tp.y);
      break;
    case VIEW_NODE_DETAIL:
      handleTouchNodeDetail(tp.x, tp.y);
      break;
  }
}

// ============== STATE TRACKING ==============

void onStateChange(const String& key, const String& value, const String& oldValue) {
  Serial.printf("[STATE] %s: %s -> %s\n", key.c_str(), oldValue.c_str(), value.c_str());
  
  // Update state cache
  bool found = false;
  for (auto& item : stateCache) {
    if (item.key == key) {
      item.value = value;
      found = true;
      break;
    }
  }
  
  if (!found) {
    StateItem item;
    item.key = key;
    item.value = value;
    item.origin = 0; // We don't have access to origin through this callback
    stateCache.push_back(item);
  }
  
  stateChanged = true;
}

// ============== SETUP ==============

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n========================================");
  Serial.println("  MeshSwarm Remote Control Node");
  Serial.println("  Cheap Yellow Display (ESP32-2432S028R)");
  Serial.println("========================================\n");
  
  // Mark OTA partition as valid
  esp_ota_mark_app_valid_cancel_rollback();
  
  // Initialize TFT display
  tft.init();
  tft.setRotation(0);  // Portrait mode (240x320)
  tft.fillScreen(COLOR_BG);
  
  // Calibrate touch (these values may need adjustment)
  uint16_t calData[5] = {275, 3620, 264, 3532, 2};
  tft.setTouch(calData);
  
  Serial.println("[INIT] Display initialized");
  
  // Show splash screen
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.print("MeshSwarm");
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.print("Remote Control");
  tft.setTextSize(1);
  tft.setCursor(20, 180);
  tft.print("Initializing mesh...");
  
  // Initialize MeshSwarm (disable OLED display support)
  swarm.begin(NODE_NAME);
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive(NODE_TYPE);
  
  Serial.println("[INIT] MeshSwarm initialized");
  Serial.println("[MODE] Remote Control - Touch Dashboard");
  
  // Watch all state changes for updates
  swarm.watchState("*", onStateChange);
  
  delay(2000);
  
  // Initial display
  updateDisplay();
}

// ============== MAIN LOOP ==============

void loop() {
  swarm.update();
  
  // Handle touch input
  handleTouch();
  
  // Periodic display refresh
  unsigned long now = millis();
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL || stateChanged) {
    updateDisplay();
    lastDisplayUpdate = now;
    stateChanged = false;
  }
}
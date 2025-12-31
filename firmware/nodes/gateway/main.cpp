/**
 * Mesh Gateway Node
 *
 * A dedicated gateway that bridges the mesh network to the telemetry server.
 * This node connects to WiFi and receives telemetry from all other mesh nodes,
 * then pushes it to the server via HTTP.
 *
 * Other nodes in the mesh do NOT need WiFi credentials - they send telemetry
 * through the mesh to this gateway.
 *
 * Features:
 *   - Connects to WiFi for server access
 *   - Maintains mesh network with other nodes
 *   - Receives MSG_TELEMETRY from other nodes
 *   - Pushes telemetry to server for all nodes
 *   - Also pushes its own telemetry
 *   - OTA firmware distribution to mesh nodes
 *
 * Hardware:
 *   - ESP32 Dev Module
 *   - OLED display (optional, SSD1306 at 0x3C)
 *
 * Serial Commands:
 *   - status: Show node status
 *   - peers: List connected peers
 *   - state: Show shared state
 *   - telem: Show telemetry/gateway status
 *   - push: Manual telemetry push
 *   - reboot: Restart node
 */

#include <Arduino.h>
#include <MeshSwarm.h>
#include <esp_ota_ops.h>
#include <time.h>

// Include credentials (gitignored)
#if __has_include("credentials.h")
#include "credentials.h"
#else
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
#define TELEMETRY_URL "http://192.168.1.100:8000"
#define TELEMETRY_KEY ""
#endif

// ============== BUILD-TIME CONFIGURATION ==============
#ifndef NODE_NAME
#define NODE_NAME "Gateway"
#endif

#ifndef NODE_TYPE
#define NODE_TYPE "gateway"
#endif

// Gateway's own telemetry interval (milliseconds)
#ifndef TELEMETRY_PUSH_INTERVAL
#define TELEMETRY_PUSH_INTERVAL 30000  // 30 seconds
#endif

// Time sync interval (milliseconds)
#ifndef TIME_SYNC_INTERVAL
#define TIME_SYNC_INTERVAL 60000  // 1 minute
#endif

// NTP settings
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif

#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC -18000  // EST = UTC-5
#endif

#ifndef DAYLIGHT_OFFSET
#define DAYLIGHT_OFFSET 3600   // DST = +1 hour
#endif

// ============== DISPLAY SCREEN NAVIGATION ==============
#define BOOT_BUTTON_PIN  0
#define EXT_BUTTON_PIN   5    // External button for screen navigation
#define DEBOUNCE_MS      200  // Longer debounce for screen nav

// Screen modes - STATE is a single screen that pages through all state entries
enum GatewayScreen {
  SCREEN_OVERVIEW = 0,
  SCREEN_WIFI,
  SCREEN_NODES,
  SCREEN_STATE,
  SCREEN_COUNT  // Total number of screens
};

// State display config
#define STATE_ENTRIES_PER_PAGE 5  // Single column, 5 rows
volatile int statePage = 0;       // Current state page

// ============== GLOBALS ==============
MeshSwarm swarm;
unsigned long lastTimeSync = 0;
bool ntpConfigured = false;

// Screen navigation state
volatile GatewayScreen currentScreen = SCREEN_OVERVIEW;
volatile unsigned long lastButtonPress = 0;
volatile bool screenChanged = false;

// State cache for display (since sharedState is private)
std::map<String, String> stateCache;

// Screen names for status line (21 chars max for OLED width)
// Note: STATE screen name is generated dynamically with page number
const char* screenNames[] = {
  "--- OVERVIEW ---",
  "---- WIFI ----",
  "---- NODES ----",
  "---- STATE ----"  // Placeholder, actual name set dynamically
};

// Buffer for dynamic state screen name
char stateScreenName[22];

// ============== HELPER FUNCTIONS ==============
int getStateTotalPages() {
  int stateCount = stateCache.size();
  if (stateCount == 0) return 1;
  return (stateCount + STATE_ENTRIES_PER_PAGE - 1) / STATE_ENTRIES_PER_PAGE;
}

// ============== INTERRUPT HANDLER ==============
void IRAM_ATTR onButtonPress() {
  unsigned long now = millis();
  if (now - lastButtonPress > DEBOUNCE_MS) {
    lastButtonPress = now;

    // If on STATE screen, cycle through pages first
    if (currentScreen == SCREEN_STATE) {
      int totalPages = getStateTotalPages();
      statePage++;
      if (statePage >= totalPages) {
        // Done with state pages, go to next screen
        statePage = 0;
        currentScreen = static_cast<GatewayScreen>((currentScreen + 1) % SCREEN_COUNT);
      }
    } else {
      currentScreen = static_cast<GatewayScreen>((currentScreen + 1) % SCREEN_COUNT);
      if (currentScreen == SCREEN_STATE) {
        statePage = 0;  // Reset to first page when entering STATE screen
      }
    }
    screenChanged = true;
  }
}

// Update status line when screen changes
void updateStatusLine() {
  if (screenChanged) {
    screenChanged = false;

    if (currentScreen == SCREEN_STATE) {
      // Generate dynamic state screen name with page info
      int totalPages = getStateTotalPages();
      snprintf(stateScreenName, sizeof(stateScreenName), "-- STATE %d/%d --", statePage + 1, totalPages);
      swarm.setStatusLine(stateScreenName);
      Serial.printf("[GATEWAY] Screen: %s\n", stateScreenName);
    } else {
      swarm.setStatusLine(screenNames[currentScreen]);
      Serial.printf("[GATEWAY] Screen: %s\n", screenNames[currentScreen]);
    }
  }
}

// ============== RSSI TO QUALITY STRING ==============
const char* rssiToQuality(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  return "Poor";
}

// ============== SCREEN DRAWING FUNCTIONS ==============
// All screens get 6 lines (lines 3-8) after the 2-line header from MeshSwarm

void drawOverviewScreen(Adafruit_SSD1306& disp) {
  // Lines 4-8: Gateway status (line 3 is status line with screen name)
  disp.printf("WiFi:%s\n", swarm.isWiFiConnected() ? "Connected" : "Disconnected");
  disp.println("Server:OK  OTA:Ready");
  disp.printf("IP:%s\n", WiFi.localIP().toString().c_str());
  unsigned long uptime = millis() / 1000;
  disp.printf("Up:%lu:%02lu:%02lu\n", uptime / 3600, (uptime / 60) % 60, uptime % 60);
  disp.printf("Nodes:%d States:%d\n", swarm.getPeerCount() + 1, stateCache.size());
}

void drawWifiScreen(Adafruit_SSD1306& disp) {
  // Lines 4-8: WiFi details (line 3 is status line with screen name)
  if (swarm.isWiFiConnected()) {
    String ssid = WiFi.SSID();
    if (ssid.length() > 18) ssid = ssid.substring(0, 18);
    disp.printf("SSID:%s\n", ssid.c_str());
    disp.printf("IP:%s\n", WiFi.localIP().toString().c_str());
    int rssi = WiFi.RSSI();
    disp.printf("RSSI:%ddBm %s\n", rssi, rssiToQuality(rssi));
    disp.printf("Ch:%d GW:%s\n", WiFi.channel(), WiFi.gatewayIP().toString().c_str());
    disp.printf("MAC:%s\n", WiFi.macAddress().c_str());
  } else {
    disp.println("Not connected");
  }
}

void drawNodesScreen(Adafruit_SSD1306& disp) {
  // Lines 4-8: Node list (line 3 is status line with screen name)
  auto& peers = swarm.getPeers();

  // 2-column layout: 10 chars per column, 5 rows = 10 nodes max
  // Prefix: * = coordinator, - = not alive, ? = unknown, (space) = alive
  String leftCol[5];
  String rightCol[5];
  int idx = 0;

  // Add self first (gateway is always shown)
  String selfName = swarm.isCoordinator() ? "*Gateway" : " Gateway";
  if (idx < 5) leftCol[idx] = selfName.substring(0, 10);
  else if (idx < 10) rightCol[idx - 5] = selfName.substring(0, 10);
  idx++;

  // Add all known peers (not just alive ones)
  for (auto& kv : peers) {
    if (idx >= 10) break;
    Peer& peer = kv.second;

    // Determine prefix based on status
    char prefix;
    if (peer.role == "COORD") {
      prefix = '*';  // Coordinator
    } else if (peer.role.length() == 0) {
      prefix = '?';  // Unknown role (never received heartbeat)
    } else if (!peer.alive) {
      prefix = '-';  // Not alive (missed heartbeats)
    } else {
      prefix = ' ';  // Known and alive
    }

    String name = String(prefix) + peer.name.substring(0, 9);
    if (idx < 5) leftCol[idx] = name;
    else rightCol[idx - 5] = name;
    idx++;
  }

  // Draw 5 rows, 2 columns
  for (int row = 0; row < 5; row++) {
    // Pad left column to 10 chars
    String left = leftCol[row];
    while (left.length() < 10) left += " ";
    String right = rightCol[row];
    disp.printf("%s %s\n", left.c_str(), right.c_str());
  }
}

void drawStateScreen(Adafruit_SSD1306& disp) {
  // Lines 4-8: State entries (line 3 is status line with screen name)
  // Single column, 5 entries per page, value always visible

  if (stateCache.size() == 0) {
    disp.println("(no state)");
    return;
  }

  int startIdx = statePage * STATE_ENTRIES_PER_PAGE;
  int shown = 0;
  int entryNum = 0;

  for (auto& kv : stateCache) {
    if (entryNum >= startIdx && shown < STATE_ENTRIES_PER_PAGE) {
      // Format: key=value, ensuring value is visible
      // Total width: 21 chars, reserve at least 6 for value (=xxxxx)
      String key = kv.first;
      String val = kv.second;

      // Truncate value if too long (max 10 chars)
      if (val.length() > 10) val = val.substring(0, 10);

      // Calculate max key length: 21 - 1 (=) - val.length()
      int maxKeyLen = 20 - val.length();
      if (maxKeyLen < 1) maxKeyLen = 1;
      if (key.length() > maxKeyLen) key = key.substring(0, maxKeyLen);

      disp.printf("%s=%s\n", key.c_str(), val.c_str());
      shown++;
    }
    entryNum++;
  }

  // Fill remaining lines
  while (shown < STATE_ENTRIES_PER_PAGE) {
    disp.println();
    shown++;
  }
}

void setup() {
  Serial.begin(115200);

  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  // Initialize mesh swarm with a name
  swarm.begin(NODE_NAME);

  // Connect to WiFi (required for gateway)
  swarm.connectToWiFi(WIFI_SSID, WIFI_PASSWORD);

  // Configure as gateway
  swarm.setGatewayMode(true);
  swarm.setTelemetryServer(TELEMETRY_URL, TELEMETRY_KEY);
  swarm.setTelemetryInterval(TELEMETRY_PUSH_INTERVAL);
  swarm.enableTelemetry(true);

  // Enable OTA distribution (gateway polls server and distributes to mesh)
  swarm.enableOTADistribution(true);

  // Start HTTP server for Remote Command Protocol API
  swarm.startHTTPServer(80);

  // Configure buttons for screen navigation with interrupts
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(EXT_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOOT_BUTTON_PIN), onButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(EXT_BUTTON_PIN), onButtonPress, FALLING);
  Serial.printf("[GATEWAY] Boot button on GPIO%d (interrupt)\n", BOOT_BUTTON_PIN);
  Serial.printf("[GATEWAY] External button on GPIO%d (interrupt)\n", EXT_BUTTON_PIN);

  // Set initial status line
  swarm.setStatusLine(screenNames[currentScreen]);

  // Register status line update handler
  swarm.onLoop(updateStatusLine);

  // Watch all state changes to populate cache for display
  swarm.watchState("*", [](const String& key, const String& value, const String& oldValue) {
    stateCache[key] = value;
  });

  // Register custom display handler for multi-screen navigation
  swarm.onDisplayUpdate([](Adafruit_SSD1306& disp, int startLine) {
    switch (currentScreen) {
      case SCREEN_OVERVIEW:
        drawOverviewScreen(disp);
        break;
      case SCREEN_WIFI:
        drawWifiScreen(disp);
        break;
      case SCREEN_NODES:
        drawNodesScreen(disp);
        break;
      case SCREEN_STATE:
        drawStateScreen(disp);
        break;
    }
  });

  Serial.println();
  Serial.println("========================================");
  Serial.println("       MESH GATEWAY NODE");
  Serial.println("========================================");
  Serial.println();
  Serial.println("This node bridges mesh -> server");
  Serial.println("Other nodes send telemetry via mesh");
  Serial.println("Press button to cycle display screens");
  Serial.println();
  Serial.println("Waiting for WiFi connection...");
  Serial.println();
}

void loop() {
  swarm.update();

  // Check for OTA updates (polls every OTA_POLL_INTERVAL)
  swarm.checkForOTAUpdates();

  // Show WiFi connection status once and configure NTP
  static bool wifiReported = false;
  if (!wifiReported && swarm.isWiFiConnected()) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("[GATEWAY] WiFi Connected!");
    Serial.printf("[GATEWAY] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[GATEWAY] Server: %s\n", TELEMETRY_URL);
    Serial.println("[GATEWAY] Ready to receive telemetry from mesh");
    Serial.println("[GATEWAY] OTA distribution enabled");
    Serial.println("[GATEWAY] HTTP API enabled on port 80");
    Serial.println("[GATEWAY]   GET  /api/nodes  - List nodes");
    Serial.println("[GATEWAY]   GET  /api/state  - Get state");
    Serial.println("[GATEWAY]   POST /api/command - Send command");
    Serial.println("========================================");
    Serial.println();

    // Wake display (status line shows current screen name)
    swarm.getPowerManager().wake();

    wifiReported = true;

    // Configure NTP time sync - use UTC (zero offsets) so time() returns UTC
    // Nodes will apply their own timezone offsets
    configTime(0, 0, NTP_SERVER);
    ntpConfigured = true;
    Serial.printf("[GATEWAY] NTP configured: %s (publishing UTC)\n", NTP_SERVER);

    // Wait briefly for NTP sync, then publish time immediately
    delay(2000);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
      time_t utcNow;
      time(&utcNow);
      swarm.setState("time", String(utcNow));
      // Show local time in serial output for convenience
      time_t localNow = utcNow + GMT_OFFSET_SEC + DAYLIGHT_OFFSET;
      struct tm localTime;
      gmtime_r(&localNow, &localTime);  // Use gmtime_r since we manually applied offset
      Serial.printf("[GATEWAY] Initial time sync: UTC %lu (local %02d:%02d:%02d)\n",
                    utcNow, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
      lastTimeSync = millis();
    }
  }

  // Publish time to mesh periodically
  if (ntpConfigured && millis() - lastTimeSync > TIME_SYNC_INTERVAL) {
    lastTimeSync = millis();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      // Publish UTC timestamp - nodes apply their own timezone offsets
      time_t utcNow;
      time(&utcNow);
      swarm.setState("time", String(utcNow));
      // Show local time in serial output for convenience
      time_t localNow = utcNow + GMT_OFFSET_SEC + DAYLIGHT_OFFSET;
      struct tm localTime;
      gmtime_r(&localNow, &localTime);  // Use gmtime_r since we manually applied offset
      Serial.printf("[GATEWAY] Time sync: UTC %lu (local %02d:%02d:%02d)\n",
                    utcNow, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    }
  }
}

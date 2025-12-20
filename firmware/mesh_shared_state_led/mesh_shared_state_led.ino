/*
 * ESP32 Mesh Swarm - LED Output Node
 *
 * Watches shared LED state and controls LED accordingly.
 * Also indicates network connectivity with a second LED.
 *
 * Hardware:
 *   - ESP32 (original dual-core)
 *   - SSD1306 OLED 128x64 (I2C: SDA=21, SCL=22)
 *   - State LED on GPIO2 (lights when led=1)
 *   - Peer LED on GPIO4 (lights when at least one peer connected)
 */

#include <MeshSwarm.h>
#include <esp_ota_ops.h>

// ============== LED CONFIGURATION ==============
#define LED_STATE_PIN   2         // LED for shared state (led=1)
#define LED_PEER_PIN    4         // LED for peer connectivity
#define LED_MOTION_PIN  15        // LED for motion detect (motion=1)
// ============== GLOBALS ==============
MeshSwarm swarm;
bool lastPeerState = false;

// ============== SETUP ==============
void setup() {
  // Mark OTA partition as valid (enables automatic rollback on boot failure)
  esp_ota_mark_app_valid_cancel_rollback();

  swarm.begin("LED");
  swarm.enableTelemetry(true);
  swarm.enableOTAReceive("led");  // Enable OTA updates for this node type

  // LED setup
  pinMode(LED_STATE_PIN, OUTPUT);
  pinMode(LED_PEER_PIN, OUTPUT);
  pinMode(LED_MOTION_PIN, OUTPUT);
  digitalWrite(LED_STATE_PIN, LOW);
  digitalWrite(LED_PEER_PIN, LOW);
  digitalWrite(LED_MOTION_PIN, LOW);
  Serial.printf("[HW] State LED on GPIO%d\n", LED_STATE_PIN);
  Serial.printf("[HW] Peer LED on GPIO%d\n", LED_PEER_PIN);
  Serial.printf("[HW] Motion LED on GPIO%d\n", LED_MOTION_PIN);

  // Watch for LED state changes
  swarm.watchState("led", [](const String& key, const String& value, const String& oldValue) {
    bool ledOn = (value == "1" || value == "on" || value == "true");
    digitalWrite(LED_STATE_PIN, ledOn ? HIGH : LOW);
    Serial.printf("[LED] State changed: %s -> %s (LED %s)\n",
                  oldValue.c_str(), value.c_str(), ledOn ? "ON" : "OFF");
  });

  // Watch for motion state changes
  swarm.watchState("motion", [](const String& key, const String& value, const String& oldValue) {
    bool motionDetect = (value == "1" || value == "on" || value == "true");
    digitalWrite(LED_MOTION_PIN, motionDetect ? HIGH : LOW);
    Serial.printf("[MOTION] State changed: %s -> %s (LED %s)\n",
                  oldValue.c_str(), value.c_str(), motionDetect ? "ON" : "OFF");
  });

  // Check peer connectivity in loop
  swarm.onLoop([]() {
    bool hasPeers = swarm.getPeerCount() > 0;
    if (hasPeers != lastPeerState) {
      lastPeerState = hasPeers;
      digitalWrite(LED_PEER_PIN, hasPeers ? HIGH : LOW);
      Serial.printf("[PEER] %s\n", hasPeers ? "Connected" : "No peers");
    }
  });

  // Custom display
  swarm.onDisplayUpdate([](Adafruit_SSD1306& display, int startLine) {
    String ledState = swarm.getState("led", "0");
    bool ledOn = (ledState == "1");
    int peerCount = swarm.getPeerCount();
    String motionDetect = swarm.getState("motion", "0");
    bool motionSet = (motionDetect == "1");

    display.println("Mode: LED OUTPUT");
    display.println("---------------------");
    display.printf("led=%s\n", ledState.c_str());
    display.printf("State LED: %s\n", ledOn ? "ON" : "OFF");
    display.printf("Peer LED:  %s (%d)\n", peerCount > 0 ? "ON" : "OFF", peerCount);
    display.printf("Motion LED:  %s (%d)\n", motionSet > 0 ? "ON" : "OFF");
  });
}

// ============== MAIN LOOP ==============
void loop() {
  swarm.update();
}

# AIoT Swarm Intelligence Vision

## Overview

This document explores the evolution of IoT Mesh from simple sensor/actuator networks to **Artificial Intelligence of Things (AIoT)** with distributed edge intelligence and swarm-based reasoning. The goal is to create a mesh network where nodes collectively perceive, analyze, and reason about their environment.

## The Vision

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SWARM INTELLIGENCE LAYERS                            │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                     COLLECTIVE REASONING                                │ │
│  │                                                                         │ │
│  │   "Someone is at the door"  "House is empty"  "Anomaly detected"       │ │
│  │   "Baby is crying"          "Party happening"  "Intruder alert"        │ │
│  │                                                                         │ │
│  │   Emergent understanding from distributed perception                    │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    ▲                                         │
│                                    │ Fusion + Inference                      │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                     EDGE AI PROCESSING                                  │ │
│  │                                                                         │ │
│  │   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐              │ │
│  │   │ Voice    │  │ Vision   │  │ Audio    │  │ Motion   │              │ │
│  │   │ Analysis │  │ Analysis │  │ Analysis │  │ Patterns │              │ │
│  │   │          │  │          │  │          │  │          │              │ │
│  │   │ Wake word│  │ Person   │  │ Glass    │  │ Activity │              │ │
│  │   │ Commands │  │ detection│  │ breaking │  │ classify │              │ │
│  │   │ Speaker  │  │ Face rec │  │ Baby cry │  │ Presence │              │ │
│  │   │ ID       │  │ Gesture  │  │ Dog bark │  │ Tracking │              │ │
│  │   └──────────┘  └──────────┘  └──────────┘  └──────────┘              │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    ▲                                         │
│                                    │ Feature Extraction                      │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                     DISTRIBUTED SENSING                                 │ │
│  │                                                                         │ │
│  │   🎤 Microphones    📷 Cameras    🌡️ Environment    📡 Presence        │ │
│  │   - I2S MEMS       - ESP-CAM     - Temp/Humidity   - PIR               │ │
│  │   - Arrays         - OV2640      - Light           - mmWave            │ │
│  │   - Direction      - Low-res     - Air quality     - IR grid           │ │
│  │                                                                         │ │
│  │   Every room, every angle, every modality                              │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Core Principle: Offline-First, Minimal Delegation

The AIoT swarm is designed around a fundamental principle: **maximize local intelligence, minimize external dependency**.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    INTELLIGENCE HIERARCHY                                    │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  LEVEL 4: Cloud/Large Models (AVOID)                                │   │
│   │  - GPT-4, Claude, cloud vision APIs                                 │   │
│   │  - Only for: Training new models, rare edge cases                   │   │
│   │  - Goal: 0% of real-time decisions                                  │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              ▲ Rare escalation                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  LEVEL 3: Gateway Reasoning (MINIMIZE)                              │   │
│   │  - Runs on Gateway ESP32 or Raspberry Pi                            │   │
│   │  - Cross-room fusion, pattern learning, anomaly detection           │   │
│   │  - Goal: <10% of decisions                                          │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              ▲ Complex fusion                               │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  LEVEL 2: Room-Level Fusion (PREFERRED)                             │   │
│   │  - Coordinator node in each room                                    │   │
│   │  - Combines observations from room sensors                          │   │
│   │  - Goal: ~30% of decisions                                          │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                              ▲ Semantic events                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  LEVEL 1: Edge Node AI (MAXIMIZE)                                   │   │
│   │  - On-device TFLite Micro inference                                 │   │
│   │  - Wake word, person detection, sound classification                │   │
│   │  - Goal: >60% of decisions made here                                │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   DESIGN GOAL: Push intelligence DOWN the stack, not up                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Why Offline-First?

| Benefit | Description |
|---------|-------------|
| **Reliability** | Works during internet outage, router failure, cloud downtime |
| **Latency** | Local inference: 10-100ms. Cloud round-trip: 500-2000ms |
| **Privacy** | Audio/video never leaves the home network |
| **Cost** | No API fees, no subscriptions, no vendor lock-in |
| **Autonomy** | Self-sufficient swarm, no external dependencies |
| **Security** | Reduced attack surface, no cloud credentials to leak |

### Minimal Delegation Strategy

```cpp
// Decision tree for where to process
if (canDecideOnDevice()) {
    // LEVEL 1: Edge node handles it
    // Example: Wake word detected → respond immediately
    executeLocally();
}
else if (needsRoomContext()) {
    // LEVEL 2: Room coordinator fuses observations
    // Example: "Is this a real intruder or the cat?"
    publishToRoomCoordinator();
}
else if (needsHomeContext()) {
    // LEVEL 3: Gateway reasoning
    // Example: "Which room is the person moving to?"
    delegateToGateway();
}
else if (excedsLocalCapability()) {
    // LEVEL 4: Cloud (rare, with user consent)
    // Example: "What breed is this dog?" (novel query)
    // ONLY if user explicitly enables cloud features
    queueForCloudWithConsent();
}
```

### Edge Model Capabilities (ESP32-S3)

What can run locally without any delegation:

| Task | Model Size | Latency | Accuracy | Delegated? |
|------|-----------|---------|----------|------------|
| Wake word ("Hey Home") | 20 KB | 10ms | 95%+ | Never |
| Voice command (10 phrases) | 50 KB | 30ms | 92%+ | Never |
| Person detection | 300 KB | 200ms | 90%+ | Never |
| Face detection | 200 KB | 150ms | 88%+ | Never |
| Known face recognition | 100 KB | 100ms | 85%+ | Rarely* |
| Sound classification (10 types) | 100 KB | 50ms | 90%+ | Never |
| Anomaly detection | 30 KB | 10ms | 85%+ | Never |

*Only for enrolling new faces; recognition runs locally

### Graceful Degradation

When higher levels are unavailable, the swarm continues functioning:

```
┌─────────────────────────────────────────────────────────────────┐
│                    DEGRADATION MODES                             │
│                                                                  │
│  Full Connectivity:                                              │
│    All 4 levels available → Maximum intelligence                 │
│                                                                  │
│  Internet Down:                                                  │
│    Levels 1-3 available → 99% functionality preserved           │
│    - Local inference works                                       │
│    - Room fusion works                                           │
│    - Gateway reasoning works                                     │
│    - Only cloud features unavailable                             │
│                                                                  │
│  Gateway Down:                                                   │
│    Levels 1-2 available → 90% functionality preserved            │
│    - Edge AI works                                               │
│    - Room coordinators work                                      │
│    - Cross-room tracking limited                                 │
│    - No telemetry/logging                                        │
│                                                                  │
│  Single Node Isolated:                                           │
│    Level 1 only → 60% functionality preserved                    │
│    - Local inference works                                       │
│    - Direct actions work (turn on light)                         │
│    - No coordination                                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Anti-Patterns to Avoid

| Anti-Pattern | Why It's Bad | Better Approach |
|--------------|--------------|-----------------|
| Cloud speech-to-text | Latency, privacy, cost | On-device wake word + commands |
| Cloud vision API | Sends images externally | On-device person/face detection |
| LLM for every query | Expensive, slow, overkill | Rule-based + small classifiers |
| Real-time cloud streaming | Bandwidth, privacy | Local processing, semantic events only |
| Cloud-dependent actions | Fails when offline | Local decision with cloud enhancement |

### When Cloud IS Appropriate

Cloud/large models should only be used for:

1. **Training new models** - Use Edge Impulse to train, then deploy to edge
2. **Enrolling new faces** - One-time upload with explicit consent
3. **Novel queries** - "What is this plant?" (rare, user-initiated)
4. **Firmware updates** - Checked periodically, not real-time dependent
5. **Optional analytics** - Aggregated, anonymized, opt-in

```cpp
// Cloud usage must be:
// 1. Explicit (user-initiated or consented)
// 2. Non-blocking (swarm works without it)
// 3. Privacy-respecting (no raw audio/video)
// 4. Rare (not for routine decisions)

if (userExplicitlyRequestedCloud() && hasUserConsent()) {
    queueCloudRequest(semanticDataOnly);  // Never raw media
}
```

## Robust Offline Architecture

### Pre-Deployed Models

All AI models are flashed to nodes at build time or deployed via OTA - never downloaded at runtime:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MODEL DEPLOYMENT FLOW                         │
│                                                                  │
│   DEVELOPMENT TIME (with network)                               │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐   │
│   │ Edge Impulse │ ──► │ Export       │ ──► │ TFLite Model │   │
│   │ Training     │     │ Quantized    │     │ (.tflite)    │   │
│   └──────────────┘     └──────────────┘     └──────────────┘   │
│                                                    │             │
│                                                    ▼             │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │ Model embedded in firmware binary (PROGMEM/SPIFFS)       │ │
│   └──────────────────────────────────────────────────────────┘ │
│                              │                                   │
│                              ▼                                   │
│   DEPLOYMENT (via USB or OTA)                                   │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │ Firmware + Models flashed to ESP32-S3                     │ │
│   │ Node is now FULLY AUTONOMOUS                              │ │
│   └──────────────────────────────────────────────────────────┘ │
│                                                                  │
│   RUNTIME (no network needed)                                   │
│   ┌──────────────────────────────────────────────────────────┐ │
│   │ Model loaded from flash → runs inference locally          │ │
│   │ No download, no API call, no internet required            │ │
│   └──────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### State Persistence

Node state and learned patterns survive reboots and power cycles:

```cpp
// MeshSwarm persistent configuration (NVS)
struct NodeConfig {
    char nodeName[32];           // User-assigned name
    char roomId[16];             // Room assignment
    uint8_t faceEncodings[10][128]; // Known face embeddings
    float normalPatterns[24];    // Hourly activity baselines
    uint32_t modelVersion;       // Deployed model version
};

// Saved to NVS flash - survives reboot, OTA, power loss
swarm.saveConfig(config);

// On boot - no network needed to restore state
swarm.loadConfig(&config);
```

### Self-Healing Mesh

The painlessMesh network automatically recovers from disruptions:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MESH SELF-HEALING                             │
│                                                                  │
│  Normal Operation:            Node Failure:                      │
│  ┌───┐   ┌───┐   ┌───┐       ┌───┐   ┌───┐   ┌───┐            │
│  │ A ├───┤ B ├───┤ C │       │ A ├───┤ X │───┤ C │            │
│  └─┬─┘   └─┬─┘   └─┬─┘       └─┬─┘   └───┘   └─┬─┘            │
│    │       │       │           │               │                │
│  ┌─┴─┐   ┌─┴─┐   ┌─┴─┐       ┌─┴─┐   ┌───┐   ┌─┴─┐            │
│  │ D ├───┤ E ├───┤ F │  ──►  │ D ├───┤ E ├───┤ F │            │
│  └───┘   └───┘   └───┘       └───┘   └───┘   └───┘            │
│                                        ▲                        │
│                              Mesh reroutes around B             │
│                                                                  │
│  Automatic Recovery:                                            │
│  • Node detection: 3-5 seconds                                  │
│  • Route recalculation: automatic                               │
│  • State resync: on reconnection                                │
│  • No coordinator required for basic mesh                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Autonomous Operation Modes

Each node can operate independently when isolated:

| Isolation Level | Capability | Example Actions |
|-----------------|------------|-----------------|
| **Full mesh** | Complete swarm intelligence | Cross-room tracking, collective reasoning |
| **Room only** | Room-level coordination | Light follows motion in room |
| **Single node** | Local AI + direct action | Voice command → LED on same node |
| **Power loss** | Instant recovery on boot | Loads last config, rejoins mesh |

### No Runtime Dependencies

```cpp
// WRONG - cloud dependency
void onVoiceCommand(const char* audio) {
    String text = cloudSpeechToText(audio);  // ❌ Fails offline
    processCommand(text);
}

// RIGHT - fully local
void onVoiceCommand(const int16_t* audio, size_t len) {
    // Model runs on-device, embedded in firmware
    int command_id = tflite_classify(audio, len);  // ✅ Always works
    if (command_id == CMD_LIGHTS_ON) {
        swarm.setState("lights", "1");
    }
}
```

### Offline Capability Matrix

| Feature | Requires Internet? | Requires Mesh? | Single Node? |
|---------|-------------------|----------------|--------------|
| Wake word detection | No | No | ✅ Yes |
| Voice commands | No | No | ✅ Yes |
| Person detection | No | No | ✅ Yes |
| Room-level fusion | No | Local mesh | No |
| Cross-room tracking | No | Full mesh | No |
| OTA updates | Periodic check | No | N/A |
| Telemetry logging | Optional | Gateway | No |

## From IoT to AIoT

### Current State: Simple Sensing

```
PIR Node → "motion=1" → LED Node turns on
```

### Target State: Intelligent Perception

```
PIR + Camera + Audio + mmWave →
  Local AI: "Person detected, walking, no face match" →
    Swarm Fusion: "Unknown visitor at front door" →
      Action: Notify + Record + Announce
```

## Core Concepts

### 1. Edge AI Nodes

Instead of simple sensor reporting, nodes perform local inference:

| Node Type | Sensors | Local AI Capability |
|-----------|---------|---------------------|
| **Voice Node** | I2S microphone array | Wake word, command recognition, speaker ID |
| **Vision Node** | Camera (OV2640/OV5640) | Person detection, face recognition, gesture |
| **Audio Node** | I2S microphone | Sound classification (glass break, baby cry, bark) |
| **Presence Node** | mmWave radar + PIR | Human presence, activity classification, counting |
| **Environment Node** | Temp/Humidity/AQ/Light | Anomaly detection, pattern learning |

### 2. Swarm State Evolution

Current MeshSwarm state is simple key-value:
```cpp
swarm.setState("motion", "1");
```

AIoT swarm state includes **semantic observations**:
```cpp
// Raw sensor event
swarm.publishObservation({
  "type": "presence",
  "node": "living_room_cam",
  "timestamp": 1703750400,
  "confidence": 0.92,
  "data": {
    "persons": 2,
    "faces_recognized": ["alice"],
    "faces_unknown": 1,
    "activity": "sitting",
    "zone": "couch_area"
  }
});
```

### 3. Collective Reasoning

Multiple observations from different nodes are fused to derive higher-level understanding:

```
┌─────────────────────────────────────────────────────────────────┐
│                    REASONING EXAMPLE                             │
│                                                                  │
│  Observations:                                                   │
│    • living_room_cam: 2 persons detected, 1 unknown             │
│    • front_door_pir: motion 30 seconds ago                      │
│    • doorbell_audio: ding-dong 35 seconds ago                   │
│    • voice_node: unknown voice speaking                         │
│                                                                  │
│  Inference Chain:                                                │
│    1. Doorbell rang → someone arrived                           │
│    2. Front door motion → they entered                          │
│    3. Unknown face + voice → visitor (not family)               │
│    4. Alice present → she let them in                           │
│                                                                  │
│  Conclusion: "Alice has a visitor"                              │
│                                                                  │
│  Action: Log event, no alert (Alice is home)                    │
└─────────────────────────────────────────────────────────────────┘
```

## Hardware Considerations

### ESP32 Family for Edge AI

| Chip | AI Capability | Best For |
|------|---------------|----------|
| ESP32 (original) | Limited (no SIMD) | Simple sensors, gateway |
| **ESP32-S3** | Vector instructions, 512KB SRAM | Audio ML, small vision models |
| ESP32-C3 | RISC-V, limited | Low-power sensors |
| **ESP32-P4** (coming) | Dual-core 400MHz, AI accelerator | Vision, advanced audio |

### Recommended Node Hardware

| Use Case | Hardware | Notes |
|----------|----------|-------|
| **Voice/Audio AI** | ESP32-S3 + INMP441 (I2S mic) | Keyword spotting, sound classification |
| **Vision AI** | ESP32-S3 + OV2640 | Person detection, face detection |
| **Advanced Vision** | ESP32-P4 or Raspberry Pi | Real-time object detection |
| **Presence Radar** | ESP32 + LD2410/LD2450 | mmWave human presence, no camera needed |
| **Multi-Sensor Fusion** | ESP32-S3 | Combine audio + PIR + environment |

### Neural Network Accelerators

For advanced AI, consider co-processors:

| Accelerator | Performance | Power | Integration |
|-------------|-------------|-------|-------------|
| Coral Edge TPU | 4 TOPS | 2W | USB or PCIe |
| Intel NCS2 | 1 TOPS | 1.5W | USB |
| Kendryte K210 | 0.5 TOPS | 0.3W | SPI/UART |
| MAX78000 | 0.5 TOPS | <100mW | Integrated MCU |

## Software Architecture

### Edge AI Stack

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ Scene       │  │ Activity    │  │ Anomaly     │             │
│  │ Understanding│  │ Recognition │  │ Detection   │             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘             │
│         └────────────────┼────────────────┘                     │
│                          ▼                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              SWARM FUSION ENGINE                          │   │
│  │  - Temporal correlation across nodes                      │   │
│  │  - Spatial reasoning (room-level, zone-level)            │   │
│  │  - Confidence aggregation                                 │   │
│  │  - Conflict resolution                                    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                          ▲                                       │
│         ┌────────────────┼────────────────┐                     │
│         ▼                ▼                ▼                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ Voice AI    │  │ Vision AI   │  │ Sensor AI   │             │
│  │ Models      │  │ Models      │  │ Models      │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│                          ▲                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              INFERENCE RUNTIME                            │   │
│  │  TensorFlow Lite Micro / ESP-DL / Edge Impulse           │   │
│  └──────────────────────────────────────────────────────────┘   │
│                          ▲                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              MESHSWARM NETWORK                            │   │
│  │  Observations, State Sync, Commands                       │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### ML Frameworks for ESP32

| Framework | Strengths | Best For |
|-----------|-----------|----------|
| **TensorFlow Lite Micro** | Google support, wide model zoo | General purpose |
| **ESP-DL** | Optimized for ESP32-S3 | Vision on Espressif chips |
| **Edge Impulse** | End-to-end (training → deployment) | Rapid prototyping |
| **Eloquent TinyML** | Arduino-friendly | Simple classification |
| **microTVM** | Compiler-based optimization | Maximum performance |

### Model Types for Edge AI

| Task | Model Architecture | Size | ESP32-S3 Inference |
|------|-------------------|------|-------------------|
| Wake word | Keyword spotting CNN | 20-50 KB | 10-50ms |
| Sound classification | MobileNet audio | 100-300 KB | 50-200ms |
| Person detection | MobileNet SSD | 300-500 KB | 200-500ms |
| Face detection | BlazeFace | 200 KB | 100-200ms |
| Gesture recognition | Custom CNN | 50-100 KB | 20-50ms |
| Anomaly detection | Autoencoder | 10-50 KB | 5-20ms |

## Swarm Intelligence Patterns

### 1. Distributed Consensus

Multiple nodes observing the same event reach agreement:

```cpp
// Voice node hears "turn on the lights"
// Nearby presence node confirms person in room
// Confidence: HIGH (multi-modal confirmation)

swarm.publishConsensusRequest({
  "event": "voice_command",
  "command": "lights_on",
  "location": "living_room",
  "speaker_confidence": 0.85,
  "presence_confirmed": true
});

// Other nodes vote on validity
// Consensus reached → action taken
```

### 2. Spatial Reasoning

Nodes share location context for tracking:

```cpp
// Front door node: "person detected, entering"
// Hallway node: "person detected, moving east"
// Living room node: "person detected, sitting"

// Swarm deduces: single person moved from door → living room
// Not: three different people
```

### 3. Temporal Correlation

Events across time are linked:

```
Timeline:
  T+0s:  Doorbell rings
  T+5s:  Front door opens (contact sensor)
  T+8s:  Voice: "Hello"
  T+10s: PIR: motion in hallway
  T+15s: Camera: 2 persons in living room

Inference: Visitor arrived and was greeted
```

### 4. Anomaly Detection

Swarm learns "normal" and detects deviations:

```
Normal Pattern (learned):
  - 7am: Kitchen activity (coffee maker, movement)
  - 8am: Front door opens (leaving for work)
  - 6pm: Front door opens (returning)
  - 11pm: All quiet

Anomaly Detected:
  - 3am: Kitchen light on
  - 3am: Movement detected
  - No known faces visible

Action: Alert homeowner
```

## Example: Comprehensive Room Awareness

### Node Configuration

```
Living Room Deployment:
  ┌─────────────────────────────────────────────────────────┐
  │                                                          │
  │   📷 ESP32-S3 Vision Node (ceiling corner)              │
  │      - OV2640 camera                                    │
  │      - Person detection + face recognition              │
  │      - Activity classification                          │
  │                                                          │
  │   🎤 ESP32-S3 Voice Node (wall mount)                   │
  │      - 4-mic array (INMP441)                            │
  │      - Wake word: "Hey Home"                            │
  │      - Command recognition                              │
  │      - Sound event detection                            │
  │                                                          │
  │   📡 ESP32 + LD2450 Presence Node (ceiling)             │
  │      - mmWave 3-target tracking                         │
  │      - Position coordinates                             │
  │      - Movement speed/direction                         │
  │                                                          │
  │   🌡️ ESP32 Environment Node (wall)                      │
  │      - Temp/Humidity/CO2/Light                          │
  │      - Occupancy inference from CO2                     │
  │                                                          │
  │   💡 ESP32 LED Controller                               │
  │      - Scene execution                                  │
  │      - Responds to swarm reasoning                      │
  │                                                          │
  └─────────────────────────────────────────────────────────┘
```

### Data Flow

```cpp
// Vision node observation
{
  "source": "living_room_cam",
  "type": "vision",
  "timestamp": 1703750400,
  "detections": [
    {"class": "person", "confidence": 0.94, "bbox": [...], "face_id": "alice"},
    {"class": "person", "confidence": 0.89, "bbox": [...], "face_id": null}
  ],
  "activity": "watching_tv"
}

// Presence node observation
{
  "source": "living_room_radar",
  "type": "presence",
  "timestamp": 1703750400,
  "targets": [
    {"id": 1, "x": 2.1, "y": 1.5, "speed": 0.0, "zone": "couch"},
    {"id": 2, "x": 2.3, "y": 1.6, "speed": 0.0, "zone": "couch"}
  ]
}

// Fused understanding
{
  "source": "swarm_reasoner",
  "type": "scene",
  "timestamp": 1703750400,
  "room": "living_room",
  "occupancy": 2,
  "known_persons": ["alice"],
  "unknown_persons": 1,
  "activity": "watching_tv",
  "confidence": 0.91
}
```

## Privacy Considerations

Edge AI enables **privacy-preserving** intelligence:

| Approach | Description |
|----------|-------------|
| **On-device inference** | Raw audio/video never leaves the node |
| **Semantic events only** | Mesh shares "person detected", not images |
| **Local face encoding** | Face IDs are hashes, not photos |
| **No cloud dependency** | All reasoning happens locally |
| **User-controlled zones** | Define private areas (no recording) |

```
┌─────────────────────────────────────────────────────────────────┐
│                    PRIVACY-FIRST ARCHITECTURE                    │
│                                                                  │
│   Camera Node                    Mesh Network                    │
│   ┌──────────────────┐          ┌──────────────────┐           │
│   │                  │          │                  │           │
│   │  📷 Raw Video    │          │  ❌ No images    │           │
│   │       │          │          │                  │           │
│   │       ▼          │          │  ✅ Only events: │           │
│   │  ┌─────────────┐ │   ───►   │  "person in      │           │
│   │  │ On-device   │ │          │   living room"   │           │
│   │  │ ML Model    │ │          │                  │           │
│   │  └─────────────┘ │          │  ✅ Face ID:     │           │
│   │       │          │          │  "alice" (hash)  │           │
│   │       ▼          │          │                  │           │
│   │  Semantic Events │          │  ✅ Activity:    │           │
│   │  Only            │          │  "watching_tv"   │           │
│   └──────────────────┘          └──────────────────┘           │
│                                                                  │
│   Video is processed and discarded - only meaning is shared     │
└─────────────────────────────────────────────────────────────────┘
```

## Implementation Roadmap

### Phase 1: Single-Node AI (Foundation)

- [ ] ESP32-S3 Voice Node with wake word detection
- [ ] ESP32-S3 Vision Node with person detection
- [ ] mmWave presence node with zone tracking
- [ ] TensorFlow Lite Micro integration
- [ ] Model deployment workflow (Edge Impulse)

### Phase 2: Observation Protocol (MeshSwarm Extension)

- [ ] Define observation message format
- [ ] Add `publishObservation()` to MeshSwarm API
- [ ] Observation storage and forwarding
- [ ] Timestamp synchronization (NTP via Gateway)
- [ ] Observation TTL and cleanup

### Phase 3: Local Fusion (Single Room)

- [ ] Temporal correlation within room
- [ ] Multi-sensor confirmation (camera + radar)
- [ ] Activity state machine
- [ ] Confidence aggregation
- [ ] Scene inference

### Phase 4: Swarm Reasoning (Multi-Room)

- [ ] Cross-room tracking (person movement)
- [ ] Home-wide occupancy model
- [ ] Anomaly detection (learned patterns)
- [ ] Collective decision making
- [ ] Action coordination

### Phase 5: Learning and Adaptation

- [ ] On-device learning (personalization)
- [ ] Federated learning across nodes
- [ ] Pattern discovery (routines)
- [ ] User feedback integration
- [ ] Model updates without OTA

## Example Use Cases

### 1. Intelligent Presence

```
Scenario: Is anyone home?

Current IoT: PIR says motion = 0, assume empty

AIoT Swarm:
- No PIR motion for 30 minutes
- But mmWave detects stationary breathing in bedroom
- Camera confirms person in bed
- Environment shows elevated CO2

Conclusion: Someone is home, sleeping
Action: Don't arm security, maintain quiet mode
```

### 2. Baby Monitoring

```
Scenario: Baby is crying

Audio Node Detection:
- Sound classified as "baby_cry" (confidence: 0.93)
- Duration: 15 seconds and ongoing

Swarm Context:
- Nursery camera: baby in crib, moving
- Living room presence: 2 adults, watching TV
- Time: 2:30 AM

Action:
- Alert parent devices (not TV, they're watching)
- Turn on soft nursery light
- If no response in 2 minutes, escalate alert
```

### 3. Security Enhancement

```
Scenario: Unknown person at night

Detection Chain:
1. Outdoor camera: person approaching (11:30 PM)
2. No vehicle detected in driveway
3. Face not recognized
4. No phone geofence of family members nearby
5. Doorbell not pressed

Conclusion: Potential prowler

Action:
- Record video
- Turn on outdoor lights
- Alert homeowner with video clip
- Track movement across cameras
```

### 4. Elder Care

```
Scenario: Daily wellness check

Observations Over 24 Hours:
- Kitchen activity: Normal breakfast time
- Bathroom: Normal frequency
- Movement: Slower than usual (10% deviation)
- Voice: No speech detected (unusual)
- Sleep: Restless (more movement than average)

Inference: Possible illness or distress

Action:
- Send wellness check to caregiver
- Include activity summary
- No false alarm (gradual deviation, not emergency)
```

## Technology Stack Summary

| Layer | Technology | Purpose |
|-------|------------|---------|
| **Hardware** | ESP32-S3, ESP32-P4, Coral TPU | Edge compute |
| **Sensing** | Cameras, Mics, Radar, Env | Multi-modal input |
| **Inference** | TFLite Micro, ESP-DL, Edge Impulse | On-device ML |
| **Networking** | MeshSwarm (painlessMesh) | Node communication |
| **Fusion** | Custom reasoning engine | Cross-node intelligence |
| **Storage** | NVS, SD Card, Gateway server | Patterns and models |
| **Interface** | Voice, Display (CYD), App | User interaction |

## Open Questions

1. **Where does reasoning happen?**
   - Each node? Gateway? Dedicated reasoning node?

2. **How to handle conflicting observations?**
   - Camera sees 2 people, radar sees 3

3. **Learning without cloud?**
   - Federated learning on mesh?
   - Gateway-based training?

4. **Latency requirements?**
   - Real-time response: < 100ms
   - Scene understanding: < 1s

5. **Model updates?**
   - How to improve models over time?
   - User feedback loop?

## References

- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [ESP-DL](https://github.com/espressif/esp-dl)
- [Edge Impulse](https://www.edgeimpulse.com/)
- [ESP32-S3 AI Capabilities](https://www.espressif.com/en/news/ESP32-S3)
- [LD2410/LD2450 mmWave](https://www.hlktech.net/index.php?id=988)
- [Person Detection Model](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/person_detection)
- [Keyword Spotting](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech)

# Shared Component Reuse Proposal

## Executive Summary

Analysis of three display nodes (touch169, clock, remote) reveals significant code duplication and opportunities for shared components. This proposal identifies reusable abstractions that could be extracted to a common library, reducing maintenance burden and ensuring consistent behavior across nodes.

## Current State Analysis

### Node Comparison

| Feature | touch169 | clock | remote |
|---------|----------|-------|--------|
| **MCU** | ESP32-S3 | ESP32 | ESP32 |
| **Display** | ST7789 1.69" 240x280 | GC9A01 1.28" Round 240x240 | ILI9341 2.4" 240x320 |
| **Input** | Capacitive Touch + Boot Button | 3 Buttons | Resistive Touch |
| **Library** | TFT_eSPI | DIYables_TFT_Round | TFT_eSPI |
| **Lines of Code** | ~900 | ~1860 | ~425 |
| **Architecture** | Refactored (11 classes) | Monolithic | Monolithic |

### Duplicated Code Patterns

#### 1. Time Management (~150 lines duplicated)
All three nodes implement mesh time synchronization:
- `setMeshTime(unixTime)` - store mesh time with millis offset
- `getMeshTime(struct tm*)` - calculate current time from stored base
- Timezone offset handling (GMT_OFFSET_SEC, DAYLIGHT_OFFSET)
- NTP configuration (only clock node currently)

**touch169**: Uses `TimeSource` class (already extracted)
**clock**: Inline functions `setMeshTime()`, `getMeshTime()`
**remote**: No time display (but could benefit)

#### 2. Mesh State Watching (~100 lines duplicated)
Both clock and touch169 implement similar state watchers:
```cpp
swarm.watchState("temp", [](const String& key, const String& value, ...) {
  meshTemp = value;
});
swarm.watchState("humidity", ...);
swarm.watchState("motion", ...);
swarm.watchState("led", ...);
swarm.watchState("light", ...);
```

**touch169**: Uses `MeshSwarmAdapter` class implementing `IMeshState`
**clock**: Inline lambdas with global variables
**remote**: Uses wildcard watcher for all state

#### 3. Screen Navigation State Machine (~80 lines duplicated)
Both touch169 and clock implement screen navigation:
- Screen enum (SCREEN_CLOCK, SCREEN_SENSOR, etc.)
- Current screen tracking
- Screen changed flag for redraw optimization
- Navigation functions (`switchScreen()`, back navigation)

**touch169**: Uses `Navigator` class
**clock**: Inline with `ScreenMode` enum and `currentScreen` variable

#### 4. Gesture/Input Detection (~120 lines duplicated)
Touch169 and clock both implement gesture handling:
- Button debouncing and state tracking
- Long press detection
- Repeat-on-hold for value adjustment

**touch169**: Uses `GestureDetector` and `InputManager` classes
**clock**: Inline with interrupt handlers and complex button state machine

#### 5. Analog Clock Face Rendering (~100 lines duplicated)
Both touch169 and clock draw analog clocks with:
- Clock face with ticks
- Hour/minute/second hands with erase-redraw
- Angle calculations
- Hand thickness handling

**touch169**: Inline in `main.cpp` (drawClockFace, drawHand, eraseHand, updateClock)
**clock**: Nearly identical inline functions

#### 6. Sensor Corner/Gauge Display (~80 lines duplicated)
Both display temperature, humidity, motion, LED state:
- Color-coded values (cyan for temp, green for humidity)
- State change detection for efficient redraw
- Arc/gauge visualizations

---

## Proposed Shared Components

### Tier 1: High Value, Low Risk (Recommend First)

#### 1.1 `TimeSource` (Already Exists in touch169)
**Location**: `lib/MeshSwarmUI/TimeSource.h/.cpp`
**Reuse**: Move from touch169 to shared lib

```cpp
class TimeSource {
public:
  void setMeshTime(unsigned long unixTime);
  bool getTime(struct tm* timeinfo);
  bool isValid() const;
  void setTimezone(long gmtOffsetSec, int daylightOffset);
};
```

**Benefits**:
- Eliminates 150 lines from clock
- Consistent time handling across all display nodes
- Foundation for future RTC integration in clock node

#### 1.2 `IMeshState` + `MeshSwarmAdapter` (Already Exists in touch169)
**Location**: `lib/MeshSwarmUI/IMeshState.h`, `MeshSwarmAdapter.h/.cpp`
**Reuse**: Move from touch169 to shared lib

```cpp
class IMeshState {
public:
  virtual String getTemperature() const = 0;
  virtual String getHumidity() const = 0;
  virtual String getLightLevel() const = 0;
  virtual bool getMotionDetected() const = 0;
  virtual bool getLedState() const = 0;
  virtual void setLedState(bool on) = 0;
};
```

**Benefits**:
- Eliminates ~100 lines from clock
- Consistent sensor data access
- Enables mock data for testing/demos
- Foundation for remote node state display

#### 1.3 `Navigator` (Already Exists in touch169)
**Location**: `lib/MeshSwarmUI/Navigator.h/.cpp`
**Reuse**: Move from touch169 to shared lib

```cpp
enum class Screen { Clock, Sensor, Settings, Stopwatch, Alarm, Light, LED, Motion, Debug };

class Navigator {
public:
  Screen current() const;
  void navigateTo(Screen screen);
  void navigateBack();
  bool hasChanged();
  void clearChanged();
  static const char* getScreenName(Screen s);
};
```

**Benefits**:
- Eliminates ~80 lines from clock
- Consistent navigation behavior
- Easy to extend with new screens

### Tier 2: Medium Value, Medium Effort

#### 2.1 `ClockFace` Widget
**Location**: `lib/MeshSwarmUI/widgets/ClockFace.h/.cpp`
**New Component**: Extract from both nodes

```cpp
class ClockFace {
public:
  ClockFace(int16_t centerX, int16_t centerY, int16_t radius);

  void setTime(int hour, int minute, int second);
  void render(TFT_eSPI& tft, bool forceRedraw);

  // Configuration
  void setHandLengths(int16_t hour, int16_t minute, int16_t second);
  void setColors(uint16_t hour, uint16_t minute, uint16_t second, uint16_t tick);

private:
  void drawFace(TFT_eSPI& tft);
  void drawHand(TFT_eSPI& tft, float angle, int len, uint16_t color, int width);
  void eraseHand(TFT_eSPI& tft, float angle, int len, int width);

  // Cached state for incremental redraw
  int _lastHour, _lastMin, _lastSec;
  float _prevHourAngle, _prevMinAngle, _prevSecAngle;
};
```

**Benefits**:
- Eliminates ~100 lines from each node
- Consistent clock rendering
- Easy to configure for different display sizes

#### 2.2 `ButtonInput` Class
**Location**: `lib/MeshSwarmUI/ButtonInput.h/.cpp`
**New Component**: Extract from clock node

```cpp
class ButtonInput {
public:
  ButtonInput(int pin, bool activeLow = true, bool useInterrupt = true);

  void begin();
  void update();

  bool isPressed() const;
  bool wasPressed();      // Returns true once per press
  bool wasLongPressed();  // Returns true once per long press
  bool isHeld() const;    // True while held after long press

  // Configuration
  void setDebounceMs(unsigned long ms);
  void setLongPressMs(unsigned long ms);
  void setRepeatMs(unsigned long ms);

  // Callbacks
  void onPress(void (*cb)());
  void onLongPress(void (*cb)());
  void onRepeat(void (*cb)());

private:
  int _pin;
  bool _activeLow;
  // ... state tracking
};
```

**Benefits**:
- Eliminates ~120 lines from clock
- Interrupt-safe button handling
- Consistent debounce/long-press behavior
- Could be used in other nodes (button, gateway with OLED)

#### 2.3 `SensorCornerWidget`
**Location**: `lib/MeshSwarmUI/widgets/SensorCorner.h/.cpp`
**New Component**: Extract from touch169/clock

```cpp
class SensorCornerWidget {
public:
  enum Position { TopLeft, TopRight, BottomLeft, BottomRight };

  SensorCornerWidget(Position pos, const char* label, uint16_t color);

  void setValue(const String& value);
  void setUnit(const char* unit);
  void render(TFT_eSPI& tft, bool forceRedraw);

  // For touch detection
  bool containsPoint(int16_t x, int16_t y) const;

private:
  Position _pos;
  String _value, _lastValue;
  const char* _label;
  const char* _unit;
  uint16_t _color;
  int16_t _x, _y, _w, _h;
};
```

**Benefits**:
- Reusable across touch169 corners and clock sensor gauges
- Encapsulates dirty checking
- Touch zone handling built-in

### Tier 3: Future Consideration

#### 3.1 `ArcGauge` Widget
Extract arc drawing for sensor gauges (clock node).

#### 3.2 `MenuItem` / `MenuSystem`
Extract circular menu system from clock node for reuse.

#### 3.3 `DisplayAdapter` Abstraction
Wrap TFT_eSPI/DIYables_TFT_Round with common interface.

---

## Proposed Library Structure

```
lib/
└── MeshSwarmUI/
    ├── MeshSwarmUI.h           # Main include file
    ├── README.md               # Usage documentation
    │
    ├── core/
    │   ├── TimeSource.h/.cpp   # From touch169
    │   ├── Navigator.h/.cpp    # From touch169
    │   ├── IMeshState.h        # From touch169
    │   └── MeshSwarmAdapter.h/.cpp  # From touch169
    │
    ├── input/
    │   ├── GestureDetector.h/.cpp   # From touch169
    │   ├── ButtonInput.h/.cpp       # New, from clock patterns
    │   └── TouchInput.h/.cpp        # From touch169
    │
    └── widgets/
        ├── Widget.h             # Base class
        ├── ClockFace.h/.cpp     # Analog clock
        ├── SensorCorner.h/.cpp  # Sensor value display
        └── ArcGauge.h/.cpp      # Arc-style gauge
```

---

## Migration Strategy

### Phase 1: Create Library Structure (Low Risk)
1. Create `lib/MeshSwarmUI/` directory
2. Copy existing touch169 classes as-is
3. Update touch169 to use library path
4. Verify touch169 still builds

### Phase 2: Migrate clock Node (Medium Risk)
1. Add `MeshSwarmUI` dependency to clock
2. Replace inline time functions with `TimeSource`
3. Replace mesh state globals with `MeshSwarmAdapter`
4. Replace screen enum with `Navigator`
5. Test all clock functionality

### Phase 3: Migrate remote Node (Low Risk)
1. Add `MeshSwarmUI` dependency
2. Use `IMeshState` for state caching
3. Consider adding `TimeSource` for header uptime display

### Phase 4: Extract New Widgets (Medium Effort)
1. Create `ClockFace` widget from touch169/clock
2. Create `ButtonInput` from clock patterns
3. Update both nodes to use widgets
4. Test thoroughly

---

## Metrics & Success Criteria

| Metric | Current | Target |
|--------|---------|--------|
| clock main.cpp lines | 1860 | ~800 |
| Duplicated time code | 150 lines | 0 |
| Duplicated state code | 100 lines | 0 |
| Duplicated clock face code | 100 lines | 0 |
| Shared components | 0 | 8+ |

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| DIYables vs TFT_eSPI API differences | Medium | Medium | Abstract via Widget base class |
| Different display geometries | Low | Low | Parameterize in constructors |
| Button interrupt handling differences | Medium | Low | Optional interrupt mode in ButtonInput |
| Clock node uses round display | Low | Medium | ClockFace already parameterized by radius |

---

## Recommendation

**Start with Tier 1** (TimeSource, IMeshState, Navigator) as these:
1. Already exist and are tested in touch169
2. Provide immediate value to clock node refactoring
3. Low risk - pure extraction with no API changes
4. Establish the library structure for future additions

**Timeline suggestion**:
- Tier 1: 1 session to extract and migrate clock
- Tier 2: 2-3 sessions for widget extraction
- Tier 3: As needed for future features

---

## Appendix: Code Comparison

### Time Management Comparison

**touch169 (TimeSource class)**:
```cpp
class TimeSource {
  void setMeshTime(unsigned long unixTime);
  bool getTime(struct tm* timeinfo);
  bool isValid() const;
  void markValid();
};
```

**clock (inline)**:
```cpp
unsigned long meshTimeBase = 0;
unsigned long meshTimeMillis = 0;
bool hasMeshTime = false;

void setMeshTime(unsigned long unixTime) {
  meshTimeBase = unixTime;
  meshTimeMillis = millis();
  hasMeshTime = true;
  struct timeval tv = { unixTime, 0 };
  settimeofday(&tv, NULL);
}

bool getMeshTime(struct tm* timeinfo) {
  if (!hasMeshTime) return false;
  unsigned long elapsed = (millis() - meshTimeMillis) / 1000;
  time_t currentTime = meshTimeBase + elapsed + GMT_OFFSET_SEC + DAYLIGHT_OFFSET;
  struct tm* t = localtime(&currentTime);
  if (t) { memcpy(timeinfo, t, sizeof(struct tm)); return true; }
  return false;
}
```

### Clock Face Comparison

**touch169**:
```cpp
void drawClockFace() {
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS, COLOR_FACE);
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS - 1, COLOR_FACE);
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * PI / 180;
    int x1 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 10);
    int y1 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 10);
    int x2 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 3);
    int y2 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 3);
    // ... draw ticks
  }
  tft.fillCircle(CENTER_X, CENTER_Y, 6, COLOR_HOUR);
}
```

**clock (nearly identical)**:
```cpp
void drawClockFace() {
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS, COLOR_FACE);
  tft.drawCircle(CENTER_X, CENTER_Y, CLOCK_RADIUS - 1, COLOR_FACE);
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * PI / 180;
    int x1 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 8);
    int y1 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 8);
    int x2 = CENTER_X + cos(angle - PI/2) * (CLOCK_RADIUS - 2);
    int y2 = CENTER_Y + sin(angle - PI/2) * (CLOCK_RADIUS - 2);
    tft.drawLine(x1, y1, x2, y2, COLOR_TICK);
  }
  tft.fillCircle(CENTER_X, CENTER_Y, 5, COLOR_HOUR);
}
```

Only differences are tick lengths (10/3 vs 8/2) and center dot size (6 vs 5).

# Touch169 Code Refactoring Plan

## Current State

The `main.cpp` file (~1400 lines) is monolithic, mixing:
- Hardware pin definitions and initialization
- Power management (battery, charging, sleep)
- Display rendering (clock, corners, debug, placeholders)
- Touch input handling and gesture detection
- Navigation state machine
- Mesh state callbacks
- Serial command processing

This violates SRP (Single Responsibility Principle) and makes the code hard to test, maintain, and extend.

## Design Principles

### 1. Single Responsibility Principle (SRP)
Each class should have one reason to change:
- `Battery` changes when battery hardware/logic changes
- `Display` changes when rendering changes
- `TouchInput` changes when touch handling changes

### 2. Separation of Concerns
- **Hardware Abstraction Layer (HAL)**: Direct hardware interaction (GPIO, I2C, ADC)
- **Domain/Business Logic**: State management, navigation, timing
- **Presentation**: Screen rendering, UI layout

### 3. Dependency Injection
- Pass dependencies into classes rather than creating them internally
- Makes testing easier, allows swapping implementations

### 4. Interface Segregation
- Define abstract interfaces for hardware (allows mocking for tests)
- Screens don't need to know about I2C, just "get temperature"

---

## Proposed Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.cpp                                 │
│  - Setup and wire dependencies                                   │
│  - Main loop delegates to controllers                            │
└─────────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
┌───────────────┐   ┌─────────────────┐   ┌─────────────────┐
│ PowerManager  │   │ DisplayManager  │   │  InputManager   │
│               │   │                 │   │                 │
│ - Battery     │   │ - ScreenRouter  │   │ - TouchInput    │
│ - Sleep/Wake  │   │ - Screens[]     │   │ - ButtonInput   │
│ - Charging    │   │                 │   │ - GestureDetect │
└───────────────┘   └─────────────────┘   └─────────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Hardware Abstraction Layer                    │
│  - PinConfig      - I2CBus        - ADCReader                   │
│  - GPIOWrapper    - SPIBus        - RTCDriver                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Class Breakdown

### Hardware Abstraction Layer (HAL)

#### `BoardConfig` (Static/Constants)
```cpp
// Pin definitions, hardware constants
namespace BoardConfig {
  constexpr int TFT_MOSI = 7;
  constexpr int TFT_SCK = 6;
  // ... all pin definitions
  constexpr int BAT_ADC_PIN = 1;
  constexpr float BAT_VREF = 6.6;
}
```

#### `Battery`
```cpp
class Battery {
public:
  Battery(int adcPin, float vRef, float fullThreshold);

  float readVoltage();
  int getPercentage();
  ChargingState getChargingState();
  void update();  // Call periodically to track trends

private:
  int _adcPin;
  float _vRef;
  float _voltageHistory[3];
  ChargingState _state;
};
```

#### `TouchInput`
```cpp
class TouchInput {
public:
  TouchInput(int sdaPin, int sclPin, uint8_t addr);

  bool begin();
  bool read();  // Returns true if touched
  int16_t getX();
  int16_t getY();
  bool isPressed();

private:
  TouchClassCST816 _touch;
  int16_t _x, _y;
  bool _pressed;
};
```

#### `IMU` (QMI8658)
```cpp
class IMU {
public:
  IMU(int sdaPin, int sclPin, uint8_t addr);

  bool begin();
  float getTemperature();  // Board temperature
  // Future: getAccel(), getGyro()

private:
  SensorQMI8658 _imu;
};
```

---

### Domain/Business Logic

#### `PowerManager`
```cpp
class PowerManager {
public:
  PowerManager(Battery& battery, int pwrEnPin, int pwrBtnPin);

  void begin();
  void update();

  void sleep();
  void wake();
  bool isAsleep();

  void powerOff();
  bool isPowerButtonPressed();

  Battery& getBattery();

private:
  Battery& _battery;
  int _pwrEnPin, _pwrBtnPin;
  bool _asleep;
  unsigned long _lastActivity;
};
```

#### `Navigator`
```cpp
enum class Screen {
  Clock, Humidity, Temperature, Light, MotionLed,
  Calendar, ClockDetails, TimeSettings, DateSettings,
  Alarm, Stopwatch, NavMenu, Debug,
  HumiditySettings, TempSettings, LightSettings, MotionLedSettings
};

class Navigator {
public:
  Navigator();

  Screen current();
  Screen previous();

  void navigateTo(Screen screen);
  void navigateBack();
  Screen getParent(Screen screen);

  bool hasScreenChanged();
  void clearScreenChanged();

private:
  Screen _current;
  Screen _previous;
  bool _changed;
};
```

#### `GestureDetector`
```cpp
enum class Gesture { None, Tap, SwipeUp, SwipeDown, SwipeLeft, SwipeRight };

class GestureDetector {
public:
  GestureDetector(int minSwipeDist = 50, int maxCrossDist = 40);

  void onTouchStart(int16_t x, int16_t y);
  void onTouchEnd(int16_t x, int16_t y);
  void reset();

  Gesture getGesture();
  int16_t getTapX();
  int16_t getTapY();

private:
  int16_t _startX, _startY;
  int16_t _endX, _endY;
  bool _active;
  int _minSwipe, _maxCross;
};
```

---

### Presentation Layer

#### `ScreenRenderer` (Abstract Base)
```cpp
class ScreenRenderer {
public:
  virtual ~ScreenRenderer() = default;

  virtual void render(TFT_eSPI& tft, bool forceRedraw) = 0;
  virtual void handleTouch(int16_t x, int16_t y, Navigator& nav) = 0;
  virtual Screen getScreen() const = 0;
};
```

#### `ClockScreen`
```cpp
class ClockScreen : public ScreenRenderer {
public:
  ClockScreen(MeshState& mesh, Battery& battery);

  void render(TFT_eSPI& tft, bool forceRedraw) override;
  void handleTouch(int16_t x, int16_t y, Navigator& nav) override;
  Screen getScreen() const override { return Screen::Clock; }

private:
  MeshState& _mesh;
  Battery& _battery;

  void drawClockFace(TFT_eSPI& tft);
  void drawCorners(TFT_eSPI& tft);
  void drawChargingIndicator(TFT_eSPI& tft);

  // Touch zones
  bool isHumidityZone(int16_t x, int16_t y);
  bool isTemperatureZone(int16_t x, int16_t y);
  // ...
};
```

#### `DebugScreen`
```cpp
class DebugScreen : public ScreenRenderer {
public:
  DebugScreen(MeshSwarm& swarm, Battery& battery, IMU& imu);

  void render(TFT_eSPI& tft, bool forceRedraw) override;
  void handleTouch(int16_t x, int16_t y, Navigator& nav) override;
  Screen getScreen() const override { return Screen::Debug; }

private:
  MeshSwarm& _swarm;
  Battery& _battery;
  IMU& _imu;
};
```

#### `DisplayManager`
```cpp
class DisplayManager {
public:
  DisplayManager(TFT_eSPI& tft, Navigator& nav);

  void registerScreen(std::unique_ptr<ScreenRenderer> screen);
  void render();
  void handleTouch(int16_t x, int16_t y);

  void sleep();
  void wake();
  bool isAsleep();

private:
  TFT_eSPI& _tft;
  Navigator& _nav;
  std::vector<std::unique_ptr<ScreenRenderer>> _screens;
  bool _asleep;

  ScreenRenderer* findScreen(Screen screen);
};
```

---

## File Structure

```
firmware/nodes/touch169/
├── main.cpp                    # Setup, loop, wire dependencies
├── config/
│   └── BoardConfig.h           # Pin definitions, constants
├── hal/
│   ├── Battery.h/.cpp          # Battery voltage, charging detection
│   ├── TouchInput.h/.cpp       # CST816T wrapper
│   ├── IMU.h/.cpp              # QMI8658 wrapper
│   └── Backlight.h/.cpp        # Display backlight control
├── domain/
│   ├── Navigator.h/.cpp        # Screen state machine
│   ├── GestureDetector.h/.cpp  # Swipe/tap detection
│   └── PowerManager.h/.cpp     # Sleep, wake, power-off
├── screens/
│   ├── ScreenRenderer.h        # Abstract base class
│   ├── ClockScreen.h/.cpp      # Main clock face
│   ├── DebugScreen.h/.cpp      # Debug info
│   ├── DetailScreen.h/.cpp     # Base for sensor details
│   ├── SettingsScreen.h/.cpp   # Base for settings screens
│   └── NavMenuScreen.h/.cpp    # Navigation menu
└── managers/
    ├── DisplayManager.h/.cpp   # Screen routing, rendering
    └── InputManager.h/.cpp     # Touch + button handling
```

---

## Refactoring Strategy

### Phase R1: Extract BoardConfig
- Move all `#define` pins to `BoardConfig.h`
- No behavior change, just organization

### Phase R2: Extract Battery
- Create `Battery` class
- Move voltage reading, charging state, trend detection
- Update `main.cpp` to use `Battery` instance

### Phase R3: Extract Navigator
- Create `Navigator` class
- Move screen enum, `navigateTo()`, `navigateBack()`, `getParent()`
- Update `main.cpp` to use `Navigator` instance

### Phase R4: Extract GestureDetector
- Create `GestureDetector` class
- Move swipe detection logic out of `handleTouch()`

### Phase R5: Extract ScreenRenderer Base
- Create abstract `ScreenRenderer` class
- Create `ClockScreen` and `DebugScreen`
- Create `DisplayManager` to route to screens

### Phase R6: Extract PowerManager
- Create `PowerManager` class
- Move sleep/wake, power button, activity timer

### Phase R7: Extract TouchInput
- Create `TouchInput` wrapper class
- Encapsulate CST816T initialization and reading

### Phase R8: Add IMU
- Create `IMU` class for QMI8658
- Add temperature reading to `DebugScreen`

---

## Design Decisions

### 1. Memory Management: Static vs Dynamic Allocation

| Approach | Pros | Cons |
|----------|------|------|
| **Static (arrays, fixed size)** | Predictable memory, no fragmentation, faster | Less flexible, must size at compile time |
| **Dynamic (vector, unique_ptr)** | Flexible, cleaner code | Heap fragmentation risk, slight overhead |

**Decision:** Start with **static allocation** for screen registry (fixed array of screen pointers). ESP32-S3 has 320KB RAM but mesh + WiFi already uses significant heap. Can revisit if we need dynamic screen loading.

```cpp
// Static approach
ScreenRenderer* screens[16];  // Fixed max screens
int screenCount = 0;
```

### 2. File Organization: Header + Implementation

**Decision:** Use **separate .h and .cpp files** for all classes.
- Cleaner separation of interface from implementation
- Faster incremental builds (change .cpp, only recompile that unit)
- Easier to read and maintain
- Standard C++ practice

### 3. Event/Callback System

**Decision:** Add a **simple observer/callback pattern** for:
- Touch events (tap, swipe)
- Button events (short press, long press)
- Navigation events (screen changed)
- Power events (sleep, wake)

```cpp
// Callback types
using TouchCallback = std::function<void(int16_t x, int16_t y)>;
using GestureCallback = std::function<void(Gesture gesture)>;
using NavigationCallback = std::function<void(Screen from, Screen to)>;

// Registration
void onTap(TouchCallback cb);
void onSwipe(GestureCallback cb);
void onNavigate(NavigationCallback cb);
```

Benefits:
- Decouples input handling from screen logic
- Screens can register for events they care about
- Easier to add new event types later
- Enables testing with mock events

### 4. Namespace vs Class for Config

**Decision:** Use **namespace** for `BoardConfig` (pure constants, no state).

```cpp
namespace BoardConfig {
  constexpr int TFT_MOSI = 7;
  // ...
}
```

---

## Benefits of Refactoring

1. **Maintainability**: Each class has one job, easier to understand
2. **Testability**: Can test `Navigator` without hardware
3. **Reusability**: `Battery` class could be used in other nodes
4. **Extensibility**: Adding new screens = add new `ScreenRenderer` subclass
5. **Debugging**: Smaller classes = smaller scope to search for bugs
6. **Collaboration**: Multiple people can work on different classes

## Risks

1. **Over-engineering**: Don't abstract too early, keep pragmatic
2. **Performance**: Virtual function overhead (minimal on ESP32)
3. **Memory**: More classes = more SRAM for vtables (minimal)
4. **Compile time**: More files = longer incremental builds

---

## Future Refactoring Considerations

### Button Input for ScreenRenderer

Extend `ScreenRenderer` to handle button events in addition to touch. The touch169 has three buttons:

| Button | GPIO | Notes |
|--------|------|-------|
| PWR | GPIO3 | Power on/off, active high |
| BOOT | GPIO0 | Built-in, active low with pullup |
| RST | - | Hardware reset, not software accessible |

**Button Event Types:**
- Short press (< 500ms)
- Long press (> 2s)
- Double tap (two presses within 300ms)

**Proposed Interface Extension:**

```cpp
enum class ButtonId { BOOT, POWER };
enum class ButtonEvent { SHORT_PRESS, LONG_PRESS, DOUBLE_TAP };

class ScreenRenderer {
public:
  virtual void render(TFT_eSPI& tft, bool forceRedraw) = 0;
  virtual void handleTouch(int16_t x, int16_t y, Navigator& nav) = 0;
  virtual void handleButton(ButtonId btn, ButtonEvent event, Navigator& nav) {}  // Optional
  virtual Screen getScreen() const = 0;
};
```

**ButtonInput Class:**

```cpp
class ButtonInput {
public:
  ButtonInput(int pin, bool activeLow = true);

  void update();  // Call in loop

  bool isShortPress();
  bool isLongPress();
  bool isDoubleTap();

  void onShortPress(std::function<void()> cb);
  void onLongPress(std::function<void()> cb);
  void onDoubleTap(std::function<void()> cb);

private:
  int _pin;
  bool _activeLow;
  unsigned long _pressTime;
  unsigned long _lastReleaseTime;
  int _tapCount;
  // ... state machine for detection
};
```

**Integration with InputManager:**

```cpp
class InputManager {
public:
  InputManager(TouchInput& touch, ButtonInput& bootBtn, ButtonInput& pwrBtn);

  void update();

  void onTap(TouchCallback cb);
  void onSwipe(GestureCallback cb);
  void onButton(ButtonId btn, ButtonEvent event, ButtonCallback cb);

private:
  TouchInput& _touch;
  ButtonInput& _bootBtn;
  ButtonInput& _pwrBtn;
  GestureDetector _gesture;
};
```

This allows screens to optionally handle button events (e.g., stopwatch START/STOP on boot button) while keeping default behavior in the main navigation controller.

### Composable UI Widgets

The clock screen is complex with multiple independent elements (corners, clock face, charging indicator). Break these into reusable widget components that can be composed into screens.

**Widget Base Class:**

```cpp
class Widget {
public:
  Widget(int16_t x, int16_t y, int16_t w, int16_t h);
  virtual ~Widget() = default;

  virtual void render(TFT_eSPI& tft, bool forceRedraw) = 0;
  virtual bool needsRedraw() const { return _dirty; }
  void markDirty() { _dirty = true; }

  // Bounds for touch detection
  bool containsPoint(int16_t x, int16_t y) const;
  int16_t getX() const { return _x; }
  int16_t getY() const { return _y; }

protected:
  int16_t _x, _y, _w, _h;
  bool _dirty = true;
};
```

**Corner Sensor Widgets:**

```cpp
class SensorCornerWidget : public Widget {
public:
  SensorCornerWidget(int16_t x, int16_t y, const char* icon, uint16_t color);

  void setValue(const String& value);
  void setUnit(const char* unit);
  void render(TFT_eSPI& tft, bool forceRedraw) override;

private:
  String _value = "--";
  const char* _unit;
  const char* _icon;
  uint16_t _color;
  String _lastValue;
};

// Specialized corners
class HumidityCorner : public SensorCornerWidget {
public:
  HumidityCorner() : SensorCornerWidget(0, 0, "💧", COLOR_HUMIDITY) {}
};

class TemperatureCorner : public SensorCornerWidget {
public:
  TemperatureCorner() : SensorCornerWidget(160, 0, "🌡", COLOR_TEMP) {}
};

class LightCorner : public SensorCornerWidget {
public:
  LightCorner() : SensorCornerWidget(0, 210, "☀", COLOR_LIGHT) {}
};

class MotionLedCorner : public SensorCornerWidget {
public:
  MotionLedCorner() : SensorCornerWidget(160, 210, "👁", COLOR_MOTION) {}
  void setMotion(bool detected);
  void setLedState(bool on);
};
```

**Clock Face Widget:**

```cpp
class AnalogClockWidget : public Widget {
public:
  AnalogClockWidget(int16_t centerX, int16_t centerY, int16_t radius);

  void setTime(int hour, int minute, int second);
  void render(TFT_eSPI& tft, bool forceRedraw) override;

private:
  int _hour, _minute, _second;
  int _lastHour, _lastMinute, _lastSecond;
  int16_t _centerX, _centerY, _radius;

  void drawHand(TFT_eSPI& tft, float angle, int length, uint16_t color, int width);
  void eraseHand(TFT_eSPI& tft, float angle, int length);
};
```

**Other Reusable Widgets:**

```cpp
// Header bar with back arrow and optional gear icon
class HeaderWidget : public Widget {
public:
  HeaderWidget(const char* title, bool showGear = false);
  void render(TFT_eSPI& tft, bool forceRedraw) override;

private:
  const char* _title;
  bool _showGear;
};

// Battery/charging indicator
class BatteryWidget : public Widget {
public:
  BatteryWidget(int16_t x, int16_t y);
  void setVoltage(float voltage);
  void setChargingState(ChargingState state);
  void render(TFT_eSPI& tft, bool forceRedraw) override;

private:
  float _voltage;
  ChargingState _state;
};

// Date display for top-center
class DateWidget : public Widget {
public:
  DateWidget(int16_t x, int16_t y);
  void setDate(int month, int day);
  void render(TFT_eSPI& tft, bool forceRedraw) override;
};
```

**Composed ClockScreen:**

```cpp
class ClockScreen : public ScreenRenderer {
public:
  ClockScreen(MeshState& mesh, Battery& battery);

  void render(TFT_eSPI& tft, bool forceRedraw) override;
  void handleTouch(int16_t x, int16_t y, Navigator& nav) override;
  Screen getScreen() const override { return Screen::Clock; }

private:
  // Composed widgets
  AnalogClockWidget _clock;
  HumidityCorner _humidity;
  TemperatureCorner _temperature;
  LightCorner _light;
  MotionLedCorner _motionLed;
  DateWidget _date;
  BatteryWidget _battery;

  // Update widgets from mesh state
  void updateFromMesh();
};

void ClockScreen::render(TFT_eSPI& tft, bool forceRedraw) {
  updateFromMesh();

  // Each widget only redraws if dirty
  _clock.render(tft, forceRedraw);
  _humidity.render(tft, forceRedraw);
  _temperature.render(tft, forceRedraw);
  _light.render(tft, forceRedraw);
  _motionLed.render(tft, forceRedraw);
  _date.render(tft, forceRedraw);
  _battery.render(tft, forceRedraw);
}

void ClockScreen::handleTouch(int16_t x, int16_t y, Navigator& nav) {
  // Use widget bounds for touch detection
  if (_humidity.containsPoint(x, y)) {
    nav.navigateTo(Screen::Humidity);
  } else if (_temperature.containsPoint(x, y)) {
    nav.navigateTo(Screen::Temperature);
  } else if (_clock.containsPoint(x, y)) {
    nav.navigateTo(Screen::ClockDetails);
  }
  // ...
}
```

**Benefits of Composition:**
- **Reusability**: `SensorCornerWidget` used on clock screen and potentially detail screens
- **Encapsulation**: Each widget manages its own dirty state and rendering
- **Testability**: Widgets can be unit tested independently
- **Maintainability**: Change corner layout without touching clock logic
- **Flexibility**: Easy to add/remove widgets or change positions

**Widget Registry for Screens:**

```cpp
class ComposedScreen : public ScreenRenderer {
protected:
  static const int MAX_WIDGETS = 10;
  Widget* _widgets[MAX_WIDGETS];
  int _widgetCount = 0;

  void addWidget(Widget* w) {
    if (_widgetCount < MAX_WIDGETS) {
      _widgets[_widgetCount++] = w;
    }
  }

  void renderAll(TFT_eSPI& tft, bool forceRedraw) {
    for (int i = 0; i < _widgetCount; i++) {
      _widgets[i]->render(tft, forceRedraw);
    }
  }

  Widget* findWidgetAt(int16_t x, int16_t y) {
    for (int i = 0; i < _widgetCount; i++) {
      if (_widgets[i]->containsPoint(x, y)) {
        return _widgets[i];
      }
    }
    return nullptr;
  }
};
```

### Mesh State Abstraction

Abstract mesh capabilities similar to board hardware. This decouples UI from MeshSwarm implementation and enables testing without a live mesh.

**Current Problem:**
```cpp
// Scattered throughout main.cpp
swarm.watchState("temp", [](const String& key, const String& value, ...) {
  meshTemp = value;  // Global variable
});

// In updateCorners()
tft.print(meshTemp);  // Direct access to global
```

**MeshState Interface:**

```cpp
// Abstract interface for mesh data access
class IMeshState {
public:
  virtual ~IMeshState() = default;

  // Sensor values (returns "--" if no data)
  virtual String getTemperature() const = 0;
  virtual String getHumidity() const = 0;
  virtual String getLightLevel() const = 0;
  virtual bool getMotionDetected() const = 0;
  virtual bool getLedState() const = 0;

  // Node discovery
  virtual int getNodeCount() const = 0;
  virtual std::vector<String> getNodesOfType(const char* type) const = 0;

  // State change notifications
  virtual void onStateChange(const char* key, std::function<void(const String&)> cb) = 0;

  // Actuator control
  virtual void setLedState(bool on) = 0;
  virtual void setState(const char* key, const String& value) = 0;
};
```

**MeshSwarmAdapter (Production Implementation):**

```cpp
class MeshSwarmAdapter : public IMeshState {
public:
  MeshSwarmAdapter(MeshSwarm& swarm);

  void begin();  // Register watchers

  // IMeshState implementation
  String getTemperature() const override { return _temp; }
  String getHumidity() const override { return _humidity; }
  String getLightLevel() const override { return _light; }
  bool getMotionDetected() const override { return _motion == "1"; }
  bool getLedState() const override { return _led == "1"; }

  int getNodeCount() const override {
    return _swarm.getNodeCount();
  }

  std::vector<String> getNodesOfType(const char* type) const override {
    // Parse node list from mesh state
    std::vector<String> nodes;
    // ... implementation
    return nodes;
  }

  void onStateChange(const char* key, std::function<void(const String&)> cb) override {
    _callbacks[key] = cb;
  }

  void setLedState(bool on) override {
    _swarm.setState("led", on ? "1" : "0");
  }

  void setState(const char* key, const String& value) override {
    _swarm.setState(key, value);
  }

private:
  MeshSwarm& _swarm;
  String _temp = "--";
  String _humidity = "--";
  String _light = "--";
  String _motion = "0";
  String _led = "0";
  std::map<String, std::function<void(const String&)>> _callbacks;

  void setupWatchers();
};

void MeshSwarmAdapter::begin() {
  _swarm.watchState("temp", [this](const String& key, const String& value, const String& old) {
    _temp = value;
    if (_callbacks.count("temp")) _callbacks["temp"](value);
  });

  _swarm.watchState("humid", [this](const String& key, const String& value, const String& old) {
    _humidity = value;
    if (_callbacks.count("humid")) _callbacks["humid"](value);
  });

  // ... other watchers
}
```

**MockMeshState (For Testing/Simulation):**

```cpp
class MockMeshState : public IMeshState {
public:
  // Settable values for testing
  void setTemperature(const String& temp) {
    _temp = temp;
    if (_callbacks.count("temp")) _callbacks["temp"](temp);
  }

  void setHumidity(const String& humid) {
    _humidity = humid;
    if (_callbacks.count("humid")) _callbacks["humid"](humid);
  }

  void simulateMotion(bool detected) {
    _motion = detected;
    if (_callbacks.count("motion")) _callbacks["motion"](detected ? "1" : "0");
  }

  // IMeshState implementation returns mock values
  String getTemperature() const override { return _temp; }
  String getHumidity() const override { return _humidity; }
  // ...

private:
  String _temp = "72.5";
  String _humidity = "45";
  bool _motion = false;
  bool _led = false;
  std::map<String, std::function<void(const String&)>> _callbacks;
};
```

**Widget Integration with IMeshState:**

```cpp
class SensorCornerWidget : public Widget {
public:
  SensorCornerWidget(int16_t x, int16_t y, const char* icon, uint16_t color);

  // Bind to mesh state source
  void bindToMeshState(IMeshState& mesh, const char* stateKey);

  void update();  // Pull latest value from mesh
  void render(TFT_eSPI& tft, bool forceRedraw) override;

private:
  IMeshState* _mesh = nullptr;
  const char* _stateKey = nullptr;
  String _value = "--";
  String _lastValue;
};

void SensorCornerWidget::bindToMeshState(IMeshState& mesh, const char* stateKey) {
  _mesh = &mesh;
  _stateKey = stateKey;

  // Register for push updates
  mesh.onStateChange(stateKey, [this](const String& value) {
    if (_value != value) {
      _value = value;
      markDirty();
    }
  });
}

void SensorCornerWidget::update() {
  if (!_mesh) return;

  // Pull current value (for initial load or polling fallback)
  String newValue;
  if (strcmp(_stateKey, "temp") == 0) newValue = _mesh->getTemperature();
  else if (strcmp(_stateKey, "humid") == 0) newValue = _mesh->getHumidity();
  // ...

  if (_value != newValue) {
    _value = newValue;
    markDirty();
  }
}
```

**ClockScreen Using Abstraction:**

```cpp
class ClockScreen : public ScreenRenderer {
public:
  ClockScreen(IMeshState& mesh, Battery& battery);

  void render(TFT_eSPI& tft, bool forceRedraw) override;

private:
  IMeshState& _mesh;
  Battery& _battery;

  HumidityCorner _humidity;
  TemperatureCorner _temperature;
  LightCorner _light;
  MotionLedCorner _motionLed;
  // ...
};

ClockScreen::ClockScreen(IMeshState& mesh, Battery& battery)
  : _mesh(mesh), _battery(battery)
{
  // Bind widgets to mesh state
  _humidity.bindToMeshState(mesh, "humid");
  _temperature.bindToMeshState(mesh, "temp");
  _light.bindToMeshState(mesh, "light");
  _motionLed.bindToMeshState(mesh, "motion");
}
```

**Benefits of Mesh Abstraction:**

| Benefit | Description |
|---------|-------------|
| **Testability** | Use MockMeshState for unit tests without hardware |
| **Simulation** | Run UI with simulated sensor values for demos |
| **Decoupling** | Screens don't depend on MeshSwarm directly |
| **Push Updates** | Widgets auto-update when mesh state changes |
| **Extensibility** | Easy to add new state sources (WiFi API, BLE, etc.) |

**Refactoring Phase:**

Add as **Phase R9: Mesh State Abstraction** after IMU:
1. Create `IMeshState` interface
2. Create `MeshSwarmAdapter` implementation
3. Create `MockMeshState` for testing
4. Update widgets to use `bindToMeshState()`
5. Update screens to receive `IMeshState&` instead of accessing globals

---

## Next Steps

1. Review and discuss this plan
2. Decide on questions above
3. Start with Phase R1 (BoardConfig) as low-risk first step
4. Iterate through phases, testing after each


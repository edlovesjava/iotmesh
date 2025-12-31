# Touch169 Touch Navigation & Enhanced UI Feature Spec

## Overview

Enhance the Waveshare ESP32-S3-Touch-LCD-1.69 node with touch-based navigation, hierarchical screen system, and improved sleep/wake behavior. The touch controller (CST816T) will enable intuitive navigation through sensor detail screens and clock sub-screens.

## Current State

The touch169 node currently has:
- Analog clock with sensor data in corners (humidity, temp, light, motion/LED)
- Boot button (GPIO0) for screen toggle (clock ↔ debug)
- Power button for power off (2-second hold)
- 30-second display sleep timeout, wake on boot button
- Charging state detection and display

## Proposed Changes

### 1. Sleep/Wake Improvements

| Current | Proposed |
|---------|----------|
| 30-second timeout | 60-second timeout |
| Wake on boot button only | Wake on touch OR boot button |
| No activity reset on interaction | Reset timeout on any touch event |

### 2. Touch Navigation System

#### Hardware Integration

The CST816T touch controller is on I2C:
- **Address**: 0x15
- **SDA**: GPIO11
- **SCL**: GPIO10
- **Features**: Single-point capacitive touch, gesture detection

Add SensorLib library for touch support:
```ini
lib_deps =
    ${env.lib_deps}
    bodmer/TFT_eSPI @ ^2.5.43
    lewishe/SensorLib @ ^0.2.0
```

#### Touch Zones on Clock Screen

```
+---------------------------+
|  [HUM]      date    [TEMP]|  <- Tap corners for detail screens
|    42%              72°F  |
|                           |
|         ╭─────╮           |
|         │     │           |
|         │  🕐 │           |  <- Tap center for clock details
|         │     │           |
|         ╰─────╯           |
|                           |
|  [LUX]   12:34:56   [PIR] |  <- Tap corners for detail screens
|    850             [LED]  |
+---------------------------+
```

**Touch Zone Definitions:**

| Zone | Position | Size | Action |
|------|----------|------|--------|
| Top-Left | (0,0) | 80x60 | Go to Humidity Detail Screen |
| Top-Right | (160,0) | 80x60 | Go to Temperature Detail Screen |
| Bottom-Left | (0,220) | 80x60 | Go to Light Detail Screen |
| Bottom-Right | (160,220) | 80x60 | Go to Motion/LED Detail Screen |
| Center | (60,80) | 120x120 | Go to Clock Details Screen |

### 3. Boot Button Behavior

| Press Type | Current Behavior | New Behavior |
|------------|------------------|--------------|
| Short tap (< 500ms) | Toggle clock ↔ debug | Return to previous screen / parent screen |
| Long hold (> 2s) | N/A | Go to Debug Screen |

**Navigation Stack:**
- Boot button acts as "back" button
- From any detail screen → returns to clock
- From clock details sub-screens → returns to clock details
- From clock details → returns to clock
- Long hold from anywhere → jumps to debug screen

### 4. Screen Hierarchy

```
Clock (Home)
├── Humidity Detail
│   └── (Full screen humidity history/gauge)
├── Temperature Detail
│   └── (Full screen temperature history/gauge)
├── Light Detail
│   └── (Full screen light level display)
├── Motion/LED Detail
│   └── (Motion history, LED control toggle)
└── Clock Details
    ├── Time/Date View (expanded time, full date, day of week)
    ├── Alarm Settings (set alarm time, toggle on/off)
    └── Stopwatch (start/stop/reset)

Debug Screen (long-hold accessible from any screen)
```

### 5. Screen Specifications

#### 5.1 Humidity Detail Screen

```
+---------------------------+
|    ← HUMIDITY             |
|                           |
|         ╭───╮             |
|        ╱     ╲            |
|       │  42%  │           |  <- Large arc gauge
|        ╲     ╱            |
|         ╰───╯             |
|                           |
|    Source: DHT Node       |
|    Last update: 5s ago    |
+---------------------------+
```

- Large animated arc gauge (0-100%)
- Color gradient: Red (dry) → Green (optimal) → Blue (humid)
- Show source node name if available
- Timestamp of last update

#### 5.2 Temperature Detail Screen

```
+---------------------------+
|    ← TEMPERATURE          |
|                           |
|         ╭───╮             |
|        ╱     ╲            |
|       │ 72°F  │           |  <- Large arc gauge
|        ╲     ╱            |
|         ╰───╯             |
|        (22.2°C)           |
|                           |
|    Source: DHT Node       |
|    Last update: 5s ago    |
+---------------------------+
```

- Large arc gauge with temperature scale
- Show both Fahrenheit and Celsius
- Color gradient: Blue (cold) → Green (comfortable) → Red (hot)
- Temperature comfort zone indicator

#### 5.3 Light Detail Screen

```
+---------------------------+
|    ← LIGHT LEVEL          |
|                           |
|                           |
|         ☀️ 850            |  <- Large centered value
|          LUX              |
|                           |
|    ████████░░░░░░         |  <- Level bar
|    Dark        Bright     |
|                           |
|    Source: Light Node     |
+---------------------------+
```

- Large numeric display with sun icon
- Horizontal bar showing relative brightness
- Descriptive label: Dark / Dim / Normal / Bright / Very Bright

#### 5.4 Motion/LED Control Screen

```
+---------------------------+
|    ← MOTION & LED         |
|                           |
|    MOTION                 |
|    ┌─────────────────┐    |
|    │   ● DETECTED    │    |  <- Red when motion, gray when clear
|    │   Last: 5s ago  │    |
|    └─────────────────┘    |
|                           |
|    LED CONTROL            |
|    ┌─────────────────┐    |
|    │      ⬤ ON       │    |  <- Tap to toggle LED
|    │   [Tap to toggle]│   |
|    └─────────────────┘    |
|                           |
+---------------------------+
```

- Motion status with time since last detection
- LED control with tap-to-toggle functionality
- Visual feedback on LED state change

#### 5.5 Clock Details Screen

```
+---------------------------+
|    ← CLOCK                |
|                           |
|      12:34:56 PM          |  <- Large digital time
|                           |
|    Monday, December 30    |
|         2024              |
|                           |
|    ┌───────┐ ┌───────┐    |
|    │ ALARM │ │ TIMER │    |  <- Sub-screen navigation
|    └───────┘ └───────┘    |
|                           |
+---------------------------+
```

**Sub-screens accessible by tapping buttons:**

##### 5.5.1 Alarm Screen

```
+---------------------------+
|    ← ALARM                |
|                           |
|      ⏰  07:00            |  <- Tap time to edit
|                           |
|    ┌───────────────────┐  |
|    │      [  ON  ]     │  |  <- Toggle switch
|    └───────────────────┘  |
|                           |
|    Tap time to adjust     |
|    + / - buttons appear   |
+---------------------------+
```

- Tap time display to enter edit mode
- Show + / - buttons for hour and minute adjustment
- Toggle switch for enable/disable

##### 5.5.2 Stopwatch Screen

```
+---------------------------+
|    ← STOPWATCH            |
|                           |
|                           |
|      00:00.0              |  <- Large stopwatch display
|                           |
|    ┌─────────┐ ┌────────┐ |
|    │  START  │ │ RESET  │ |  <- Touch buttons
|    └─────────┘ └────────┘ |
|                           |
|                           |
+---------------------------+
```

- Large stopwatch display (MM:SS.d)
- START/STOP and RESET touch buttons
- Persists while in background (keeps running)

### 6. Implementation Approach

#### 6.1 Touch Controller Setup

```cpp
#include <TouchDrvCSTXXX.hpp>

// Touch pins
#define TOUCH_SDA   11
#define TOUCH_SCL   10
#define TOUCH_ADDR  0x15

TouchDrvCSTXXX touch;

void setupTouch() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    touch.begin(Wire, TOUCH_ADDR, TOUCH_SDA, TOUCH_SCL);
    touch.setMaxCoordinates(240, 280);
}
```

#### 6.2 Screen State Machine

```cpp
enum ScreenMode {
    SCREEN_CLOCK,           // Main clock face
    SCREEN_HUMIDITY,        // Humidity detail
    SCREEN_TEMPERATURE,     // Temperature detail
    SCREEN_LIGHT,           // Light detail
    SCREEN_MOTION_LED,      // Motion & LED control
    SCREEN_CLOCK_DETAILS,   // Clock details menu
    SCREEN_ALARM,           // Alarm settings
    SCREEN_STOPWATCH,       // Stopwatch
    SCREEN_DEBUG            // Debug info
};

ScreenMode currentScreen = SCREEN_CLOCK;
ScreenMode previousScreen = SCREEN_CLOCK;
```

#### 6.3 Touch Handler

```cpp
void handleTouch() {
    int16_t x, y;
    if (touch.getPoint(&x, &y)) {
        resetActivityTimer();  // Reset sleep timeout on any touch

        if (displayAsleep) {
            displayWake();
            return;  // Don't process touch that woke display
        }

        // Route touch to current screen handler
        switch (currentScreen) {
            case SCREEN_CLOCK:
                handleClockTouch(x, y);
                break;
            case SCREEN_CLOCK_DETAILS:
                handleClockDetailsTouch(x, y);
                break;
            // ... other screens
        }
    }
}

void handleClockTouch(int16_t x, int16_t y) {
    if (y < 60) {
        if (x < 80) navigateTo(SCREEN_HUMIDITY);
        else if (x > 160) navigateTo(SCREEN_TEMPERATURE);
    } else if (y > 220) {
        if (x < 80) navigateTo(SCREEN_LIGHT);
        else if (x > 160) navigateTo(SCREEN_MOTION_LED);
    } else if (x > 60 && x < 180 && y > 80 && y < 200) {
        navigateTo(SCREEN_CLOCK_DETAILS);
    }
}
```

#### 6.4 Boot Button Handler (Modified)

```cpp
#define BOOT_BTN_SHORT_PRESS_MS   50
#define BOOT_BTN_LONG_PRESS_MS    2000

void checkBootButton() {
    bool btnPressed = digitalRead(BOOT_BTN_PIN) == LOW;
    unsigned long now = millis();

    if (btnPressed && !bootBtnWasPressed) {
        bootBtnPressTime = now;
        bootBtnWasPressed = true;
    } else if (btnPressed && bootBtnWasPressed) {
        // Check for long press while held
        if (now - bootBtnPressTime >= BOOT_BTN_LONG_PRESS_MS) {
            if (!bootBtnLongPressFired) {
                bootBtnLongPressFired = true;
                navigateTo(SCREEN_DEBUG);  // Long press → debug
            }
        }
    } else if (!btnPressed && bootBtnWasPressed) {
        unsigned long pressDuration = now - bootBtnPressTime;
        if (pressDuration >= BOOT_BTN_SHORT_PRESS_MS &&
            pressDuration < BOOT_BTN_LONG_PRESS_MS) {
            navigateBack();  // Short press → back
        }
        bootBtnWasPressed = false;
        bootBtnLongPressFired = false;
    }
}

void navigateBack() {
    resetActivityTimer();

    if (displayAsleep) {
        displayWake();
        return;
    }

    switch (currentScreen) {
        case SCREEN_CLOCK:
            // Already at home, do nothing
            break;
        case SCREEN_ALARM:
        case SCREEN_STOPWATCH:
            navigateTo(SCREEN_CLOCK_DETAILS);
            break;
        default:
            navigateTo(SCREEN_CLOCK);
            break;
    }
}
```

### 7. Dependencies

#### New Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| SensorLib | ^0.2.0 | CST816T touch controller driver |

#### Modified platformio.ini

```ini
[env:touch169]
; ... existing config ...
lib_deps =
    ${env.lib_deps}
    bodmer/TFT_eSPI @ ^2.5.43
    lewishe/SensorLib @ ^0.2.0
```

### 8. File Changes

| File | Change Type | Description |
|------|-------------|-------------|
| `firmware/nodes/touch169/main.cpp` | Modify | Add touch handling, new screens, navigation |
| `firmware/platformio.ini` | Modify | Add SensorLib dependency |
| `firmware/nodes/touch169/info/pinout.txt` | Modify | Document touch controller details |

### 9. Testing Plan

1. **Touch Calibration**: Verify touch coordinates map correctly to screen zones
2. **Sleep/Wake**: Confirm 60s timeout and touch/button wake
3. **Navigation**: Test all touch zones navigate to correct screens
4. **Back Button**: Verify boot button returns to parent screens
5. **Long Press**: Confirm debug screen access from any screen
6. **Activity Reset**: Touch and button presses reset sleep timer
7. **LED Toggle**: Test LED control works from motion/LED screen
8. **Stopwatch**: Verify stopwatch keeps running when navigating away

### 10. Future Considerations

- Swipe gestures for screen navigation (left/right swipe)
- Touch-based time setting (spin dial interface)
- Brightness adjustment via swipe down gesture
- Quick action menu on clock long-press

## References

- [Clock node implementation](../firmware/nodes/clock/main.cpp) - Circular menu pattern
- [SensorLib CST816T example](https://github.com/lewishe/SensorLib/blob/main/examples/TouchDrv_CSTxxx_GetPoint/TouchDrv_CSTxxx_GetPoint.ino)
- [Waveshare ESP32-S3-LCD-1.69 Pinout](../firmware/nodes/touch169/info/pinout.txt)

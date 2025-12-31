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
|  [HUM]    [date]    [TEMP]|  <- Tap corners for detail, date for calendar
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
|          [ ≡ ]            |  <- Tap nav icon for menu
+---------------------------+
```

**Touch Zone Definitions:**

| Zone | Position | Size | Action |
|------|----------|------|--------|
| Top-Left | (0,0) | 80x60 | Go to Humidity Detail Screen |
| Top-Center | (80,0) | 80x40 | Go to Calendar Screen |
| Top-Right | (160,0) | 80x60 | Go to Temperature Detail Screen |
| Bottom-Left | (0,210) | 80x50 | Go to Light Detail Screen |
| Bottom-Right | (160,210) | 80x50 | Go to Motion/LED Detail Screen |
| Center | (60,80) | 120x120 | Go to Clock Details Screen |
| Bottom-Center | (80,260) | 80x20 | Open Navigation Menu |

**Detail Screen Header Zones** (applies to all detail screens):

| Zone | Position | Size | Action |
|------|----------|------|--------|
| Back Arrow | (0,0) | 60x40 | Navigate back to parent screen |
| Gear Icon | (200,0) | 40x40 | Open settings for this screen |

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
│   ├── (Full screen humidity gauge with zone selector)
│   └── Humidity Zone Settings [⚙] (set default DHT zone)
├── Temperature Detail
│   ├── (Full screen temperature gauge with zone selector)
│   └── Temperature Zone Settings [⚙] (set default DHT zone)
├── Light Detail
│   ├── (Full screen light level with zone selector)
│   └── Light Zone Settings [⚙] (set default light zone)
├── Motion/LED Detail
│   ├── (Motion status and LED control with zone selectors)
│   └── Motion/LED Zone Settings [⚙] (set default PIR and LED zones)
├── Calendar
│   ├── (Monthly calendar view with navigation)
│   └── Date Settings [⚙] (set current date)
├── Clock Details
│   ├── Time/Date View (expanded time, full date, day of week)
│   ├── Time Settings [⚙] (set time and timezone)
│   ├── Alarm Settings (set alarm time, toggle on/off)
│   └── Stopwatch (start/stop/reset)
└── Navigation Menu (accessible from any screen via bottom-center icon)
    └── Direct links to all screens

Debug Screen (long-hold accessible from any screen)
```

### 4.1 Navigation Menu

The Navigation Menu provides quick access to all screens from anywhere in the app. It is accessible by tapping the navigation icon (≡) displayed at the bottom center of every screen.

**Consistent Nav Icon:**
- Displayed on ALL screens (including detail screens) at bottom-center
- Position: `(80, 250)` with size `80x30`
- Icon: Three horizontal lines (hamburger menu) "≡"
- Tapping opens the Navigation Menu overlay

#### Navigation Menu Screen

```
+---------------------------+
|         NAVIGATE          |
|                           |
|   ┌─────────┐ ┌─────────┐ |
|   │  🏠     │ │  💧     │ |
|   │  Clock  │ │Humidity │ |
|   └─────────┘ └─────────┘ |
|   ┌─────────┐ ┌─────────┐ |
|   │  🌡️     │ │  ☀️     │ |
|   │  Temp   │ │  Light  │ |
|   └─────────┘ └─────────┘ |
|   ┌─────────┐ ┌─────────┐ |
|   │  🚶     │ │  📅     │ |
|   │Motion/LED│ │Calendar │ |
|   └─────────┘ └─────────┘ |
|   ┌─────────┐ ┌─────────┐ |
|   │  ⏰     │ │  🐛     │ |
|   │  Alarm  │ │  Debug  │ |
|   └─────────┘ └─────────┘ |
|          [ ✕ ]            |  <- Close menu / back
+---------------------------+
```

**Navigation Menu Features:**
- Grid layout of touchable buttons for all available screens
- Each button shows icon and label
- Tapping a button navigates directly to that screen
- Bottom close button (✕) or boot button returns to previous screen
- Semi-transparent overlay or full-screen replacement (implementation choice)

**Menu Button Definitions:**

| Button | Icon | Target Screen |
|--------|------|---------------|
| Clock | 🏠 | SCREEN_CLOCK (Home) |
| Humidity | 💧 | SCREEN_HUMIDITY |
| Temp | 🌡️ | SCREEN_TEMPERATURE |
| Light | ☀️ | SCREEN_LIGHT |
| Motion/LED | 🚶 | SCREEN_MOTION_LED |
| Calendar | 📅 | SCREEN_CALENDAR |
| Alarm | ⏰ | SCREEN_ALARM |
| Debug | 🐛 | SCREEN_DEBUG |

### 5. Screen Specifications

#### 5.1 Humidity Detail Screen

```
+---------------------------+
|    ← HUMIDITY        [⚙]  |  <- Gear for zone settings
|                           |
|         ╭───╮             |
|        ╱     ╲            |
|       │  42%  │           |  <- Large arc gauge
|        ╲     ╱            |
|         ╰───╯             |
|                           |
|    [<] Kitchen DHT [>]    |  <- Zone selector
|    Last update: 5s ago    |
|          [ ≡ ]            |
+---------------------------+
```

- Large animated arc gauge (0-100%)
- Color gradient: Red (dry) → Green (optimal) → Blue (humid)
- Zone selector with left/right arrows to switch between available DHT nodes
- Gear icon (⚙) opens Humidity Zone Settings
- Timestamp of last update

#### 5.2 Temperature Detail Screen

```
+---------------------------+
|    ← TEMPERATURE     [⚙]  |  <- Gear for zone settings
|                           |
|         ╭───╮             |
|        ╱     ╲            |
|       │ 72°F  │           |  <- Large arc gauge
|        ╲     ╱            |
|         ╰───╯             |
|        (22.2°C)           |
|                           |
|    [<] Kitchen DHT [>]    |  <- Zone selector
|    Last update: 5s ago    |
|          [ ≡ ]            |
+---------------------------+
```

- Large arc gauge with temperature scale
- Show both Fahrenheit and Celsius
- Color gradient: Blue (cold) → Green (comfortable) → Red (hot)
- Zone selector with left/right arrows to switch between available DHT nodes
- Gear icon (⚙) opens Temperature Zone Settings
- Temperature comfort zone indicator

#### 5.3 Light Detail Screen

```
+---------------------------+
|    ← LIGHT LEVEL     [⚙]  |  <- Gear for zone settings
|                           |
|                           |
|         ☀️ 850            |  <- Large centered value
|          LUX              |
|                           |
|    ████████░░░░░░         |  <- Level bar
|    Dark        Bright     |
|                           |
|    [<] Living Room [>]    |  <- Zone selector
|          [ ≡ ]            |
+---------------------------+
```

- Large numeric display with sun icon
- Horizontal bar showing relative brightness
- Descriptive label: Dark / Dim / Normal / Bright / Very Bright
- Zone selector with left/right arrows to switch between available Light nodes
- Gear icon (⚙) opens Light Zone Settings

#### 5.4 Motion/LED Control Screen

```
+---------------------------+
|    ← MOTION & LED    [⚙]  |  <- Gear for zone settings
|                           |
|    MOTION                 |
|    ┌─────────────────┐    |
|    │   ● DETECTED    │    |  <- Red when motion, gray when clear
|    │   Last: 5s ago  │    |
|    │  [<] Hallway [>]│    |  <- PIR zone selector
|    └─────────────────┘    |
|                           |
|    LED CONTROL            |
|    ┌─────────────────┐    |
|    │      ⬤ ON       │    |  <- Tap to toggle LED
|    │  [<] Kitchen [>]│    |  <- LED zone selector
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

- Motion status with time since last detection
- PIR zone selector to switch between available motion sensors
- LED zone selector to switch between available LED nodes
- LED control with tap-to-toggle functionality
- Visual feedback on LED state change
- Gear icon (⚙) opens Motion/LED Zone Settings

#### 5.5 Calendar Screen

```
+---------------------------+
|    ← CALENDAR        [⚙]  |  <- Gear icon for date settings
|      December 2025        |
|    [<]            [>]     |  <- Month navigation
|                           |
|  Su Mo Tu We Th Fr Sa     |
|   1  2  3  4  5  6  7     |
|   8  9 10 11 12 13 14     |
|  15 16 17 18 19 20 21     |
|  22 23 24 25 26 27 28     |
|  29 30[31] 1  2  3  4     |  <- Today highlighted
|                           |
|          [ ≡ ]            |
+---------------------------+
```

- Monthly calendar grid view
- Current day highlighted with box/circle
- Left/right arrows to navigate months
- Tap [<] or [>] to change month
- Days from adjacent months shown in dimmed color
- Gear icon (⚙) in header navigates to Date Settings screen
- Swipe left/right for month navigation (future enhancement)

#### 5.6 Clock Details Screen

```
+---------------------------+
|    ← CLOCK           [⚙]  |  <- Gear icon for time/timezone settings
|                           |
|      12:34:56 PM          |  <- Large digital time
|        UTC-5 (EST)        |  <- Current timezone
|                           |
|    Monday, December 30    |
|         2025              |
|                           |
|    ┌───────┐ ┌───────┐    |
|    │ ALARM │ │ TIMER │    |  <- Sub-screen navigation
|    └───────┘ └───────┘    |
|          [ ≡ ]            |
+---------------------------+
```

- Gear icon (⚙) in header navigates to Time Settings screen

**Sub-screens accessible by tapping buttons:**

##### 5.6.1 Alarm Screen

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
|          [ ≡ ]            |
+---------------------------+
```

- Tap time display to enter edit mode
- Show + / - buttons for hour and minute adjustment
- Toggle switch for enable/disable

##### 5.6.2 Stopwatch Screen

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
|          [ ≡ ]            |
+---------------------------+
```

- Large stopwatch display (MM:SS.d)
- START/STOP and RESET touch buttons
- Persists while in background (keeps running)

##### 5.6.3 Time Settings Screen

```
+---------------------------+
|    ← TIME SETTINGS        |
|                           |
|    SET TIME               |
|    ┌─────────────────┐    |
|    │   [+]     [+]   │    |
|    │   12  :  34     │    |  <- Hour : Minute
|    │   [-]     [-]   │    |
|    └─────────────────┘    |
|                           |
|    TIMEZONE               |
|    ┌─────────────────┐    |
|    │  [<] UTC-5 [>]  │    |  <- Scroll through timezones
|    │      (EST)      │    |
|    └─────────────────┘    |
|                           |
|    ┌─────────────────┐    |
|    │      SAVE       │    |  <- Apply changes
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

**Time Settings Features:**
- Hour adjustment with +/- buttons (12 or 24-hour format)
- Minute adjustment with +/- buttons
- Timezone selector with left/right navigation
- Common timezones: UTC-12 to UTC+14 with abbreviations
- SAVE button applies changes to RTC (PCF85063)
- Changes take effect immediately on save

**Timezone List (subset):**
| Offset | Abbreviation | Name |
|--------|--------------|------|
| UTC-8 | PST | Pacific Standard |
| UTC-7 | MST | Mountain Standard |
| UTC-6 | CST | Central Standard |
| UTC-5 | EST | Eastern Standard |
| UTC+0 | GMT | Greenwich Mean |
| UTC+1 | CET | Central European |
| UTC+8 | CST | China Standard |
| UTC+9 | JST | Japan Standard |

##### 5.6.4 Date Settings Screen

```
+---------------------------+
|    ← DATE SETTINGS        |
|                           |
|    SET DATE               |
|                           |
|    MONTH                  |
|    ┌─────────────────┐    |
|    │ [<] December [>]│    |
|    └─────────────────┘    |
|                           |
|    DAY          YEAR      |
|    ┌───────┐  ┌───────┐   |
|    │[+] 30 │  │[+]2024│   |
|    │  [-]  │  │  [-]  │   |
|    └───────┘  └───────┘   |
|                           |
|    ┌─────────────────┐    |
|    │      SAVE       │    |  <- Apply changes
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

**Date Settings Features:**
- Month selector with left/right navigation (January - December)
- Day adjustment with +/- buttons (1-31, adjusted for month)
- Year adjustment with +/- buttons
- Validates date (no Feb 30, etc.)
- SAVE button applies changes to RTC (PCF85063)
- Returns to Calendar screen after save

#### 5.7 Sensor Zone Settings Screens

Each sensor type has a Zone Settings screen accessible via the gear icon (⚙) on the detail screen. These allow setting the default zone displayed on the main clock screen corners.

##### 5.7.1 Humidity Zone Settings

```
+---------------------------+
|    ← HUMIDITY SETTINGS    |
|                           |
|    DEFAULT ZONE           |
|    (shown on clock)       |
|                           |
|    ┌─────────────────┐    |
|    │ ○ Kitchen DHT   │    |
|    │ ● Bedroom DHT   │    |  <- Radio button selection
|    │ ○ Garage DHT    │    |
|    │ ○ Outdoor DHT   │    |
|    └─────────────────┘    |
|                           |
|    ┌─────────────────┐    |
|    │      SAVE       │    |
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

##### 5.7.2 Temperature Zone Settings

```
+---------------------------+
|    ← TEMP SETTINGS        |
|                           |
|    DEFAULT ZONE           |
|    (shown on clock)       |
|                           |
|    ┌─────────────────┐    |
|    │ ○ Kitchen DHT   │    |
|    │ ● Bedroom DHT   │    |  <- Radio button selection
|    │ ○ Garage DHT    │    |
|    │ ○ Outdoor DHT   │    |
|    └─────────────────┘    |
|                           |
|    ┌─────────────────┐    |
|    │      SAVE       │    |
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

##### 5.7.3 Light Zone Settings

```
+---------------------------+
|    ← LIGHT SETTINGS       |
|                           |
|    DEFAULT ZONE           |
|    (shown on clock)       |
|                           |
|    ┌─────────────────┐    |
|    │ ○ Living Room   │    |
|    │ ● Kitchen       │    |  <- Radio button selection
|    │ ○ Bedroom       │    |
|    │ ○ Outdoor       │    |
|    └─────────────────┘    |
|                           |
|    ┌─────────────────┐    |
|    │      SAVE       │    |
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

##### 5.7.4 Motion/LED Zone Settings

```
+---------------------------+
|    ← MOTION/LED SETTINGS  |
|                           |
|    DEFAULT PIR ZONE       |
|    ┌─────────────────┐    |
|    │ ○ Hallway       │    |
|    │ ● Front Door    │    |  <- Radio button selection
|    │ ○ Garage        │    |
|    └─────────────────┘    |
|                           |
|    DEFAULT LED ZONE       |
|    ┌─────────────────┐    |
|    │ ● Kitchen       │    |
|    │ ○ Living Room   │    |  <- Radio button selection
|    └─────────────────┘    |
|                           |
|    ┌─────────────────┐    |
|    │      SAVE       │    |
|    └─────────────────┘    |
|          [ ≡ ]            |
+---------------------------+
```

**Zone Settings Common Features:**
- Lists all discovered nodes of that sensor type from mesh network
- Radio button selection for default zone
- Default zone determines which node's data appears on clock screen corners
- SAVE button persists selection to flash storage
- Returns to parent detail screen after save
- Nodes are discovered dynamically from mesh state

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
    SCREEN_CLOCK,               // Main clock face
    SCREEN_HUMIDITY,            // Humidity detail
    SCREEN_HUMIDITY_SETTINGS,   // Humidity zone settings
    SCREEN_TEMPERATURE,         // Temperature detail
    SCREEN_TEMP_SETTINGS,       // Temperature zone settings
    SCREEN_LIGHT,               // Light detail
    SCREEN_LIGHT_SETTINGS,      // Light zone settings
    SCREEN_MOTION_LED,          // Motion & LED control
    SCREEN_MOTION_LED_SETTINGS, // Motion/LED zone settings
    SCREEN_CALENDAR,            // Monthly calendar view
    SCREEN_DATE_SETTINGS,       // Set current date
    SCREEN_CLOCK_DETAILS,       // Clock details menu
    SCREEN_TIME_SETTINGS,       // Set time and timezone
    SCREEN_ALARM,               // Alarm settings
    SCREEN_STOPWATCH,           // Stopwatch
    SCREEN_NAV_MENU,            // Navigation menu overlay
    SCREEN_DEBUG                // Debug info
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
            case SCREEN_NAV_MENU:
                handleNavMenuTouch(x, y);
                break;
            case SCREEN_CLOCK_DETAILS:
                handleClockDetailsTouch(x, y);
                break;
            // ... other screens (all should call handleNavIconTouch first)
            default:
                // All detail screens check nav icon first
                if (!handleNavIconTouch(x, y)) {
                    // Handle screen-specific touch
                }
                break;
        }
    }
}

void handleClockTouch(int16_t x, int16_t y) {
    // Check nav icon first (bottom center, all screens)
    if (y > 250 && x > 80 && x < 160) {
        navigateTo(SCREEN_NAV_MENU);
        return;
    }

    if (y < 40) {
        // Top row: humidity (left), calendar (center), temperature (right)
        if (x < 80) navigateTo(SCREEN_HUMIDITY);
        else if (x > 160) navigateTo(SCREEN_TEMPERATURE);
        else navigateTo(SCREEN_CALENDAR);  // Top center = date/calendar
    } else if (y < 60) {
        // Below date area, still in corners
        if (x < 80) navigateTo(SCREEN_HUMIDITY);
        else if (x > 160) navigateTo(SCREEN_TEMPERATURE);
    } else if (y > 220 && y <= 250) {
        if (x < 80) navigateTo(SCREEN_LIGHT);
        else if (x > 160) navigateTo(SCREEN_MOTION_LED);
    } else if (x > 60 && x < 180 && y > 80 && y < 200) {
        navigateTo(SCREEN_CLOCK_DETAILS);
    }
}

// Common nav icon handler - call from all screen touch handlers
bool handleNavIconTouch(int16_t x, int16_t y) {
    if (y > 250 && x > 80 && x < 160) {
        navigateTo(SCREEN_NAV_MENU);
        return true;
    }
    return false;
}

void handleNavMenuTouch(int16_t x, int16_t y) {
    // Close button at bottom
    if (y > 250 && x > 80 && x < 160) {
        navigateBack();
        return;
    }

    // Grid buttons (2 columns x 4 rows)
    int col = (x < 120) ? 0 : 1;
    int row = (y - 40) / 50;  // Adjust based on actual layout

    ScreenMode targets[4][2] = {
        {SCREEN_CLOCK, SCREEN_HUMIDITY},
        {SCREEN_TEMPERATURE, SCREEN_LIGHT},
        {SCREEN_MOTION_LED, SCREEN_CALENDAR},
        {SCREEN_ALARM, SCREEN_DEBUG}
    };

    if (row >= 0 && row < 4) {
        navigateTo(targets[row][col]);
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
        case SCREEN_NAV_MENU:
            // Return to screen that was showing before menu opened
            navigateTo(previousScreen);
            break;
        case SCREEN_TIME_SETTINGS:
        case SCREEN_ALARM:
        case SCREEN_STOPWATCH:
            navigateTo(SCREEN_CLOCK_DETAILS);
            break;
        case SCREEN_DATE_SETTINGS:
            navigateTo(SCREEN_CALENDAR);
            break;
        // Zone settings return to their parent detail screens
        case SCREEN_HUMIDITY_SETTINGS:
            navigateTo(SCREEN_HUMIDITY);
            break;
        case SCREEN_TEMP_SETTINGS:
            navigateTo(SCREEN_TEMPERATURE);
            break;
        case SCREEN_LIGHT_SETTINGS:
            navigateTo(SCREEN_LIGHT);
            break;
        case SCREEN_MOTION_LED_SETTINGS:
            navigateTo(SCREEN_MOTION_LED);
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
9. **Nav Menu Icon**: Verify nav icon (≡) displays on all screens at bottom center
10. **Nav Menu Access**: Confirm tapping nav icon opens navigation menu from any screen
11. **Nav Menu Buttons**: Test all 8 menu buttons navigate to correct screens
12. **Nav Menu Close**: Verify close button (✕) and boot button return to previous screen
13. **Nav Menu State**: Confirm previousScreen is correctly preserved when opening/closing menu
14. **Calendar Access**: Verify tapping date text on clock opens calendar screen
15. **Calendar Navigation**: Test month navigation arrows work correctly
16. **Calendar Display**: Verify current day is highlighted and month/year displays correctly
17. **Time Settings Access**: Verify gear icon on Clock Details opens Time Settings
18. **Time Adjustment**: Test hour/minute +/- buttons increment/decrement correctly
19. **Timezone Selection**: Verify timezone left/right navigation cycles through options
20. **Time Save**: Confirm SAVE button writes to RTC and returns to Clock Details
21. **Date Settings Access**: Verify gear icon on Calendar opens Date Settings
22. **Date Adjustment**: Test month/day/year controls work correctly
23. **Date Validation**: Verify invalid dates (Feb 30, etc.) are prevented
24. **Date Save**: Confirm SAVE button writes to RTC and returns to Calendar
25. **Zone Selector**: Verify left/right arrows on sensor screens cycle through available nodes
26. **Zone Discovery**: Confirm nodes are discovered from mesh state dynamically
27. **Zone Settings Access**: Verify gear icon on each sensor screen opens zone settings
28. **Zone Settings List**: Confirm all discovered nodes appear in radio button list
29. **Default Zone Selection**: Verify radio button selection persists after SAVE
30. **Clock Corner Update**: Confirm changing default zone updates clock screen corner display
31. **Zone Persistence**: Verify zone selections survive device reboot (flash storage)
32. **Multiple Sensors**: Test zone switching with multiple DHT/PIR/Light nodes on mesh

### 10. Recommendations

#### 10.1 UX Improvements

- **No Nodes Found State**: Zone settings should show message when no nodes of that type discovered
- **Cancel Clarification**: Back button on settings screens = discard changes (no save)
- **Transition Animations**: Consider fade or slide transitions between screens
- **12/24 Hour Format Toggle**: Add option to Time Settings
- **Temperature Unit Toggle**: Add Fahrenheit/Celsius preference to Temperature Settings

#### 10.2 Gesture Enhancements

The CST816T supports native gesture detection - leverage for:
- Swipe left/right for prev/next screen navigation
- Swipe down for brightness/settings quick panel
- Long-press on clock for quick actions menu

#### 10.3 Additional Features

- **Widget Mode**: Auto-cycling quick glance widgets on clock face
- **Notification Badges**: Alert indicators on nav menu icons (e.g., alarm set)
- **Per-Screen Timeout**: Longer display timeout for stopwatch/active screens
- **Touch Feedback**: Audio click or visual ripple on touch
- **Accessibility**: Larger touch zones option, high contrast mode
- **Power Management**: Deep sleep when not charging, wake on touch interrupt
- **Multi-Language**: Localized day/month names

### 11. Future Considerations

- NTP time sync when gateway connected
- DST automatic adjustment
- Multi-zone comparison view (show all sensors of same type)
- Historical data graphs for sensors
- Alarm with mesh-triggered actions (turn on LED when alarm fires)
- Screen mirroring to other display nodes
- Add Clock Details or Stopwatch to nav menu for direct access

#### 11.1 WiFi Mode & Connection Info Screen

Enable the touch169 to operate in WiFi client mode as an alternative to mesh-only mode:

**WiFi Mode Features:**
- Switch between Mesh-only mode and WiFi+Mesh mode
- Connect to local WiFi network for internet access
- Connect to cloud service (or local server) to obtain mesh status remotely
- Receive push notifications for sensor alerts
- NTP time sync over WiFi
- OTA firmware updates directly (without gateway)

**Connection Info Screen:**
- Display current connection mode (Mesh / WiFi / Both)
- Show WiFi SSID and signal strength when connected
- Show mesh node count and coordinator status
- Display server connection status (connected/disconnected)
- IP address and MAC address info
- Button to switch modes or configure WiFi credentials

**WiFi Configuration:**
- WiFi credentials entry via touch keyboard or QR code scan
- Store multiple WiFi networks with priority
- Auto-fallback to mesh-only if WiFi unavailable
- Captive portal for initial setup

#### 11.2 Bluetooth Phone Pairing & Remote Monitoring

Enable the touch169 to pair with a smartphone for portable mesh monitoring away from home:

**Bluetooth Features:**
- BLE pairing with companion mobile app (iOS/Android)
- Phone acts as internet bridge when away from home WiFi
- Real-time mesh status pushed to watch via phone connection
- Two-way communication: view sensors and control actuators remotely

**Remote Monitoring Use Cases:**
- Take the touch169 as a portable "mesh remote" when traveling
- Monitor home sensors (temperature, motion, light) from anywhere
- Receive alerts when motion detected or thresholds exceeded
- Control LEDs and other actuators remotely through the device

**Pairing Screen:**
- Show Bluetooth pairing status and paired device name
- QR code for easy app download/pairing
- Signal strength indicator when connected
- Disconnect/forget device option

**Data Sync:**
- Phone fetches mesh state from cloud/home server
- Pushes updates to touch169 over BLE
- Caches last known state when disconnected
- Configurable sync interval to save battery

**Companion App Requirements:**
- Background service to maintain BLE connection
- Cloud API integration for mesh state retrieval
- Push notification forwarding to touch169
- Secure authentication for remote access

## References

- [Clock node implementation](../firmware/nodes/clock/main.cpp) - Circular menu pattern
- [SensorLib CST816T example](https://github.com/lewishe/SensorLib/blob/main/examples/TouchDrv_CSTxxx_GetPoint/TouchDrv_CSTxxx_GetPoint.ino)
- [Waveshare ESP32-S3-LCD-1.69 Pinout](../firmware/nodes/touch169/info/pinout.txt)

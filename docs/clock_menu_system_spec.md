# Clock Node Circular Menu System Specification

## Overview

Add a circular menu system to the clock node that appears around the screen edge when the Mode button is pressed. The menu provides quick access to different screens/apps using simple vector icons positioned around the display circumference.

## User Interaction

- **Open menu**: Press Mode button (menu appears, current screen highlighted)
- **Navigate**: Left/Right buttons rotate selection around the ring
- **Select different screen**: Press Mode on a different screen → switches to it, menu closes
- **Close without switching**: Press Mode when already on current screen → menu closes, stays on same screen
- **No auto-dismiss**: Menu stays open until Mode is pressed

## Menu Items (8 positions around the ring)

| Position | Angle | Icon | App/Screen |
|----------|-------|------|------------|
| Top | 270° | Clock icon | Analog clock (current) |
| Top-Right | 315° | Thermometer | Temperature/humidity sensors |
| Right | 0° | Gear/wrench | Time set mode |
| Bottom-Right | 45° | Stopwatch | Stopwatch (new) |
| Bottom | 90° | Bell | Alarm (new) |
| Bottom-Left | 135° | Lightbulb | Light sensor status |
| Left | 180° | LED icon | LED control status |
| Top-Left | 225° | Motion waves | PIR motion status |

## Visual Design

### Menu Ring
- **Radius**: 100px from center (outer edge of icons)
- **Icon size**: ~20x20px each
- **Background**: Semi-transparent dark overlay on current screen
- **Selection highlight**: Yellow arc segment behind selected icon + yellow icon color

### Vector Icon Designs (line-based, minimalist)

```
Clock:        Circle with two hands (hour/min)
Thermometer:  Vertical line with bulb at bottom
Gear:         Circle with 4 small lines radiating out
Stopwatch:    Circle with button on top, single hand
Bell:         Bell shape (arc + clapper)
Lightbulb:    Bulb outline with rays
LED:          Small circle with radiating lines
Motion:       3 curved lines (wifi-style waves)
```

## New Screens/Apps to Add

### 1. Stopwatch Screen
- Large digital display: MM:SS.ms
- Left button: Start/Stop
- Right button: Reset (when stopped) / Lap (when running)
- Mode button: Return to menu

### 2. Alarm Screen
- Shows current alarm time (or "No Alarm")
- Left/Right: Adjust alarm time (when in set mode)
- Mode button: Toggle alarm on/off, enter set mode
- Visual indicator when alarm is active

### 3. Mesh Device Status Screens
- **Light**: Shows current light sensor value from mesh (`light` state key)
- **LED**: Shows LED state, allows toggle via mesh (`led` state key)
- **Motion**: Shows motion state, last trigger time (`motion` state key)

## Implementation Plan

### File: `firmware/nodes/clock/main.cpp`

#### Phase 1: Menu System Core

1. **Add menu state variables**
   ```cpp
   bool menuActive = false;
   int menuSelection = 0;  // 0-7 for 8 menu items
   const int MENU_ITEM_COUNT = 8;
   ```

2. **Define menu item structure**
   ```cpp
   enum MenuItem {
     MENU_CLOCK = 0,
     MENU_SENSORS,
     MENU_SETTINGS,
     MENU_STOPWATCH,
     MENU_ALARM,
     MENU_LIGHT,
     MENU_LED,
     MENU_MOTION
   };
   ```

3. **Add icon drawing functions**
   ```cpp
   void drawMenuIcon(int index, int cx, int cy, uint16_t color);
   void drawClockIcon(int cx, int cy, uint16_t color);
   void drawThermometerIcon(int cx, int cy, uint16_t color);
   // ... etc for each icon
   ```

4. **Add menu rendering**
   ```cpp
   void drawMenu() {
     // Draw semi-transparent overlay
     // Draw 8 icons at calculated positions
     // Highlight selected item with yellow arc + color
   }
   ```

5. **Modify button handlers**
   - Mode button: Toggle `menuActive`, or select item if menu is open
   - Left/Right: Rotate `menuSelection` when menu active

#### Phase 2: Expand Screen Enum

```cpp
enum ScreenMode {
  SCREEN_CLOCK = 0,
  SCREEN_SENSOR,
  SCREEN_STOPWATCH,
  SCREEN_ALARM,
  SCREEN_LIGHT,
  SCREEN_LED,
  SCREEN_MOTION,
  SCREEN_COUNT
};
```

#### Phase 3: Add New Screens

1. **Stopwatch**
   - Add state: `stopwatchRunning`, `stopwatchStartTime`, `stopwatchElapsed`
   - Add functions: `drawStopwatchScreen()`, `updateStopwatchScreen()`

2. **Alarm**
   - Add state: `alarmEnabled`, `alarmHour`, `alarmMinute`
   - Add functions: `drawAlarmScreen()`, `updateAlarmScreen()`, `checkAlarm()`

3. **Mesh Status Screens**
   - Add state watchers for `light`, `led`, `motion`
   - Add functions: `drawLightScreen()`, `drawLedScreen()`, `drawMotionScreen()`

#### Phase 4: Menu Navigation Flow

```
Normal Display (e.g., on Clock screen)
     |
[Mode Press]
     v
Menu Active (overlay, Clock icon highlighted as current)
     |
[Left/Right] --> Rotate selection to other icons
     |
[Mode Press on same screen] --> Close menu, stay on Clock
     |
[Mode Press on different screen] --> Switch to that screen, menu closes
```

**Key behavior:**
- Menu selection starts on the icon matching the current screen
- If user presses Mode without changing selection → just close menu
- If user navigates to different icon and presses Mode → switch screen and close menu

## Critical Files

- `firmware/nodes/clock/main.cpp` - All implementation
- `firmware/nodes/clock/README.md` - Update documentation

## Icon Position Calculation

```cpp
void getMenuIconPosition(int index, int* x, int* y) {
  // 8 items, starting at top (270°), going clockwise
  float angle = (270 + index * 45) * PI / 180.0;
  int radius = 95;  // Distance from center to icon center
  *x = CENTER_X + cos(angle) * radius;
  *y = CENTER_Y + sin(angle) * radius;
}
```

## Estimated Scope

- Menu system core: ~150 lines
- Icon drawing functions: ~100 lines
- Stopwatch screen: ~80 lines
- Alarm screen: ~80 lines
- Mesh status screens: ~60 lines each (180 total)
- Button handler updates: ~30 lines

**Total: ~540 lines of new code**

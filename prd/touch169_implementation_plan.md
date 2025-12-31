# Touch169 Enhanced UI Implementation Plan

## Overview

This document outlines the phased implementation plan for the touch169 enhanced navigation and UI features as specified in [touch169_touch_navigation_spec.md](./touch169_touch_navigation_spec.md).

**Development Approach:**
- Task-by-task implementation with human quick review
- Phase completion requires full testing on device
- Milestone completion = product ready for delivery at that state

**Hardware:** Waveshare ESP32-S3-Touch-LCD-1.69
**Simulation:** See [mesh_simulator_proposal.md](./mesh_simulator_proposal.md) for testing without full mesh

---

## Implementation Status

| Phase | Status | Notes |
|-------|--------|-------|
| 1.0 POC Validation | ✅ COMPLETE | Touch + Preferences working |
| 1.1 Screen Architecture | ✅ COMPLETE | 17 screen enum, navigation, dispatcher |
| 1.2 Touch Zone Framework | ✅ COMPLETE | Zones + swipe gestures implemented |
| 1.3 Clock Details Screen | 🔲 NOT STARTED | |
| 1.4 Time Settings Screen | 🔲 NOT STARTED | |
| 1.5 Date Settings Screen | 🔲 NOT STARTED | |
| 1.6 Navigation Menu | 🔲 NOT STARTED | Swipe-down trigger done |
| 1.7 Boot Button Behavior | ✅ COMPLETE | Short=back, long=debug |
| 1.8 Internal Temp Monitoring | 🔲 NOT STARTED | |

**Last Updated:** 2025-12-30

---

## Milestone 1: Core Navigation Foundation

**Goal:** Establish touch input, screen architecture, and basic clock/settings navigation.
**Deliverable:** Working clock with time/date settings and navigation menu.

### Phase 1.0: Proof of Concept Validation ✅ COMPLETE

Validate core technical assumptions before building features.

| Task | Description | Validation | Status |
|------|-------------|------------|--------|
| **POC-1: Touch Input** | Integrate SensorLib CST816T driver, read touch coordinates | Serial output shows x,y on touch; wake from sleep on touch | ✅ |
| **POC-2: Preferences Storage** | Test ESP32 Preferences library for settings persistence | Write timezone, reboot, read back correctly | ✅ |

**Phase Validation:**
- [x] Touch coordinates display on serial monitor
- [x] Touch wakes display from sleep
- [x] Preferences survive power cycle
- [x] No I2C conflicts with display/RTC

---

### Phase 1.1: Screen Architecture ✅ COMPLETE

Refactor existing clock code to support multi-screen navigation.

| Task | Description | Validation | Status |
|------|-------------|------------|--------|
| 1.1.1 | Create `ScreenMode` enum and `currentScreen` state machine | Code compiles, clock still displays | ✅ |
| 1.1.2 | Implement `navigateTo()` and `navigateBack()` functions | Functions exist, no-op for now | ✅ |
| 1.1.3 | Create screen rendering dispatcher (`renderCurrentScreen()`) | Clock renders via dispatcher | ✅ |
| 1.1.4 | Add `previousScreen` tracking for nav menu back behavior | Variable tracked on navigation | ✅ |
| 1.1.5 | Implement activity timer reset on touch/button | Display sleep timeout resets | ✅ |

**Phase Validation:**
- [x] Clock displays correctly through new architecture
- [x] Boot button navigates (back/debug)
- [x] Sleep/wake still functional
- [x] No memory leaks after extended runtime

---

### Phase 1.2: Touch Zone Framework ✅ COMPLETE

Implement touch detection and zone-based navigation.

| Task | Description | Validation | Status |
|------|-------------|------------|--------|
| 1.2.1 | Create `TouchZone` struct with bounds and action | Inline zone checks in processTouchZones() | ✅ |
| 1.2.2 | Implement `handleTouch()` main dispatcher | Touch events routed to handlers | ✅ |
| 1.2.3 | Implement clock screen zone detection | All 6 corner/center zones work | ✅ |
| 1.2.4 | Add swipe gestures for nav menu | Swipe down=open, swipe up=close | ✅ |
| 1.2.5 | Add header zone detection (back arrow, gear) | Enlarged zones (100x60, 60x60) | ✅ |
| 1.2.6 | Add touch cooldown after screen transitions | 300ms cooldown prevents accidental taps | ✅ |

**Phase Validation:**
- [x] All clock screen zones trigger correct navigation
- [x] Touch coordinates map correctly to zones
- [x] No false triggers or dead zones
- [x] Touch debouncing works (no double-triggers)
- [x] Swipe gestures detected reliably

---

### Phase 1.3: Clock Details Screen

First detail screen implementation - establishes the pattern for all screens.

| Task | Description | Validation |
|------|-------------|------------|
| 1.3.1 | Create `renderClockDetails()` with digital time display | Screen renders with time |
| 1.3.2 | Add timezone display below time | Shows "UTC-5 (EST)" |
| 1.3.3 | Add day/date display | Shows "Monday, December 30, 2025" |
| 1.3.4 | Add ALARM and TIMER button graphics | Buttons render |
| 1.3.5 | Add header with back arrow and gear icon | Header renders |
| 1.3.6 | Add nav icon at bottom | Nav icon renders |
| 1.3.7 | Implement `handleClockDetailsTouch()` | Buttons/icons respond |
| 1.3.8 | Wire clock center tap to navigate to Clock Details | Tap center opens Clock Details |
| 1.3.9 | Wire back arrow and boot button to return to clock | Back navigation works |

**Phase Validation:**
- [ ] Tap clock center opens Clock Details
- [ ] Clock Details displays all elements correctly
- [ ] Back arrow returns to clock
- [ ] Boot button returns to clock
- [ ] Gear icon ready (no-op for now)
- [ ] ALARM/TIMER buttons ready (no-op for now)

---

### Phase 1.4: Time Settings Screen

First settings screen - establishes settings pattern with save/cancel.

| Task | Description | Validation |
|------|-------------|------------|
| 1.4.1 | Create `renderTimeSettings()` layout | Screen renders |
| 1.4.2 | Add hour/minute display with +/- buttons | Time controls render |
| 1.4.3 | Add timezone selector with left/right arrows | Timezone control renders |
| 1.4.4 | Add SAVE button | Button renders |
| 1.4.5 | Implement hour +/- touch handlers | Hour increments/decrements |
| 1.4.6 | Implement minute +/- touch handlers | Minute increments/decrements |
| 1.4.7 | Implement timezone left/right handlers | Timezone cycles through list |
| 1.4.8 | Implement SAVE - write to RTC and Preferences | Time persists after reboot |
| 1.4.9 | Wire gear icon on Clock Details to Time Settings | Gear opens Time Settings |
| 1.4.10 | Back button discards changes | Changes not saved on back |

**Phase Validation:**
- [ ] Gear icon opens Time Settings
- [ ] Hour/minute adjustable with +/- buttons
- [ ] Timezone selector cycles through options
- [ ] SAVE writes to RTC
- [ ] Settings persist after reboot
- [ ] Back discards unsaved changes
- [ ] Clock shows updated time after save

---

### Phase 1.5: Date Settings Screen

| Task | Description | Validation |
|------|-------------|------------|
| 1.5.1 | Create `renderDateSettings()` layout | Screen renders |
| 1.5.2 | Add month selector with left/right arrows | Month control renders |
| 1.5.3 | Add day +/- controls | Day control renders |
| 1.5.4 | Add year +/- controls | Year control renders |
| 1.5.5 | Implement month navigation | Month cycles Jan-Dec |
| 1.5.6 | Implement day +/- with validation | Day respects month limits |
| 1.5.7 | Implement year +/- | Year adjustable |
| 1.5.8 | Implement SAVE - write to RTC | Date persists |
| 1.5.9 | Add placeholder Calendar screen | Calendar screen exists |
| 1.5.10 | Wire gear icon on Calendar to Date Settings | Gear opens Date Settings |

**Phase Validation:**
- [ ] Date Settings accessible from Calendar gear
- [ ] All date fields adjustable
- [ ] Invalid dates prevented (Feb 30, etc.)
- [ ] SAVE writes to RTC
- [ ] Date persists after reboot

---

### Phase 1.6: Navigation Menu

| Task | Description | Validation |
|------|-------------|------------|
| 1.6.1 | Create `renderNavMenu()` with 2x4 grid | Menu renders |
| 1.6.2 | Add button graphics with icons and labels | All 8 buttons visible |
| 1.6.3 | Add close button (X) at bottom | Close button renders |
| 1.6.4 | Implement `handleNavMenuTouch()` grid detection | Buttons respond to touch |
| 1.6.5 | Wire each button to `navigateTo()` target | Navigation works |
| 1.6.6 | Wire close button to `navigateBack()` | Close returns to previous |
| 1.6.7 | Add nav icon to all existing screens | Icon visible everywhere |
| 1.6.8 | Wire all nav icons to open Nav Menu | Nav menu accessible from anywhere |

**Phase Validation:**
- [ ] Nav menu opens from any screen via nav icon
- [ ] All 8 destination buttons work
- [ ] Close button returns to previous screen
- [ ] Boot button also closes menu
- [ ] previousScreen correctly tracked

---

### Phase 1.7: Boot Button Behavior ✅ COMPLETE

| Task | Description | Validation | Status |
|------|-------------|------------|--------|
| 1.7.1 | Implement short press detection (< 500ms) | Short press detected | ✅ |
| 1.7.2 | Implement long press detection (> 2s) | Long press detected | ✅ |
| 1.7.3 | Short press calls `navigateBack()` | Back navigation works | ✅ |
| 1.7.4 | Long press navigates to Debug screen | Debug accessible | ✅ |
| 1.7.5 | Refactor existing debug screen to new architecture | Debug screen works | ✅ |

**Phase Validation:**
- [x] Short press = back from any screen
- [x] Long press = debug from any screen
- [x] Back from clock = no-op (already home)
- [x] Debug screen shows mesh/system info

---

### Phase 1.8: Internal Temperature Monitoring

Add QMI8658 IMU temperature sensor to monitor device heat.

| Task | Description | Validation |
|------|-------------|------------|
| 1.8.1 | Add SensorQMI8658 to includes | Compiles |
| 1.8.2 | Initialize QMI8658 on I2C bus (addr 0x6B) | Serial shows init success |
| 1.8.3 | Add `readInternalTemp()` function | Returns temperature in C |
| 1.8.4 | Add internal temp display to Debug screen | Shows "Board: XX.X C" |
| 1.8.5 | Add thermal warning threshold (e.g., > 50C) | Warning logged to serial |
| 1.8.6 | Optional: Add ESP32 internal temp for comparison | Shows "CPU: XX.X C" |

**Phase Validation:**
- [ ] QMI8658 initializes without I2C conflicts
- [ ] Internal temperature reads correctly on debug screen
- [ ] Temperature updates periodically
- [ ] Thermal warning appears if device overheats

---

### Milestone 1 Validation Checklist

- [ ] Clock displays with sensor corners
- [ ] Tap clock center opens Clock Details
- [ ] Clock Details shows time, timezone, date
- [ ] Gear opens Time Settings, back discards, save persists
- [ ] Calendar placeholder exists, gear opens Date Settings
- [ ] Nav menu accessible from all screens
- [ ] All nav buttons navigate correctly
- [ ] Boot short press = back, long press = debug
- [ ] Display sleep/wake works with touch
- [ ] Settings persist across reboots
- [ ] No crashes after 1 hour runtime

---

## Milestone 2: Sensor Detail Screens

**Goal:** Implement all sensor detail screens with zone switching.
**Deliverable:** Full sensor viewing with multi-zone support.

### Phase 2.1: Humidity Detail Screen

| Task | Description | Validation |
|------|-------------|------------|
| 2.1.1 | Create `renderHumidityDetail()` with arc gauge | Screen renders |
| 2.1.2 | Add zone selector with left/right arrows | Zone selector renders |
| 2.1.3 | Add "Last update" timestamp | Timestamp renders |
| 2.1.4 | Implement arc gauge drawing (0-100%) | Gauge animates |
| 2.1.5 | Add color gradient (red-green-blue) | Colors correct |
| 2.1.6 | Implement zone switching from mesh state | Zones cycle |
| 2.1.7 | Wire clock top-left tap to Humidity Detail | Navigation works |
| 2.1.8 | Add gear icon (placeholder for zone settings) | Gear visible |

**Phase Validation:**
- [ ] Humidity screen accessible from clock
- [ ] Gauge displays current humidity
- [ ] Zone selector shows available DHT nodes
- [ ] Back navigation works

---

### Phase 2.2: Temperature Detail Screen

| Task | Description | Validation |
|------|-------------|------------|
| 2.2.1 | Create `renderTemperatureDetail()` with arc gauge | Screen renders |
| 2.2.2 | Add Fahrenheit display with Celsius subtitle | Both units shown |
| 2.2.3 | Implement zone switching | Zones cycle |
| 2.2.4 | Wire clock top-right tap | Navigation works |

**Phase Validation:**
- [ ] Temperature screen accessible from clock
- [ ] Both F and C displayed
- [ ] Zone selector works
- [ ] Color gradient (blue-green-red) correct

---

### Phase 2.3: Light Detail Screen

| Task | Description | Validation |
|------|-------------|------------|
| 2.3.1 | Create `renderLightDetail()` with large lux value | Screen renders |
| 2.3.2 | Add horizontal level bar | Bar renders |
| 2.3.3 | Add descriptive label (Dark/Dim/Normal/Bright) | Label updates |
| 2.3.4 | Implement zone switching | Zones cycle |
| 2.3.5 | Wire clock bottom-left tap | Navigation works |

**Phase Validation:**
- [ ] Light screen accessible from clock
- [ ] Lux value and bar display correctly
- [ ] Zone selector works

---

### Phase 2.4: Motion/LED Control Screen

| Task | Description | Validation |
|------|-------------|------------|
| 2.4.1 | Create `renderMotionLedDetail()` layout | Screen renders |
| 2.4.2 | Add motion status indicator (red/gray dot) | Status shows |
| 2.4.3 | Add "Last detected" timestamp | Timestamp updates |
| 2.4.4 | Add PIR zone selector | Selector works |
| 2.4.5 | Add LED toggle button | Toggle renders |
| 2.4.6 | Add LED zone selector | Selector works |
| 2.4.7 | Implement LED toggle via mesh state | LED toggles |
| 2.4.8 | Wire clock bottom-right tap | Navigation works |

**Phase Validation:**
- [ ] Motion/LED screen accessible from clock
- [ ] Motion status updates from mesh
- [ ] LED toggle controls mesh LED node
- [ ] Both zone selectors work independently

---

### Phase 2.5: Calendar Screen

| Task | Description | Validation |
|------|-------------|------------|
| 2.5.1 | Create `renderCalendar()` with month/year header | Header renders |
| 2.5.2 | Add month navigation arrows | Arrows render |
| 2.5.3 | Implement calendar grid (7 columns, 6 rows) | Grid renders |
| 2.5.4 | Highlight current day | Today highlighted |
| 2.5.5 | Dim days from adjacent months | Dimming works |
| 2.5.6 | Implement month navigation | Months cycle |
| 2.5.7 | Wire clock top-center (date) tap | Navigation works |

**Phase Validation:**
- [ ] Calendar accessible from clock date tap
- [ ] Correct days for each month
- [ ] Today highlighted
- [ ] Month navigation works
- [ ] Gear opens Date Settings

---

### Milestone 2 Validation Checklist

- [ ] All 4 corner taps navigate to correct sensor screens
- [ ] Date tap navigates to calendar
- [ ] All sensor screens display data from mesh
- [ ] Zone selectors work on all applicable screens
- [ ] LED toggle controls actual LED node
- [ ] Calendar displays correctly for any month
- [ ] All back navigation works
- [ ] Nav menu works from all new screens

---

## Milestone 3: Alarm, Stopwatch & Zone Settings

**Goal:** Complete Clock Details sub-screens and zone configuration.
**Deliverable:** Full alarm/timer functionality and persistent zone preferences.

### Phase 3.1: Alarm Screen

| Task | Description | Validation |
|------|-------------|------------|
| 3.1.1 | Create `renderAlarm()` with time display | Screen renders |
| 3.1.2 | Add ON/OFF toggle switch | Toggle renders |
| 3.1.3 | Implement time tap to edit mode | Edit mode works |
| 3.1.4 | Add +/- buttons in edit mode | Buttons appear |
| 3.1.5 | Save alarm to Preferences | Alarm persists |
| 3.1.6 | Wire ALARM button on Clock Details | Navigation works |

**Phase Validation:**
- [ ] Alarm screen accessible from Clock Details
- [ ] Time editable with +/- buttons
- [ ] Toggle enables/disables alarm
- [ ] Settings persist after reboot

---

### Phase 3.2: Stopwatch Screen

| Task | Description | Validation |
|------|-------------|------------|
| 3.2.1 | Create `renderStopwatch()` with time display | Screen renders |
| 3.2.2 | Add START/STOP and RESET buttons | Buttons render |
| 3.2.3 | Implement stopwatch timer logic | Timer counts |
| 3.2.4 | Persist running state when navigating away | Timer continues in background |
| 3.2.5 | Wire TIMER button on Clock Details | Navigation works |

**Phase Validation:**
- [ ] Stopwatch accessible from Clock Details
- [ ] START begins counting
- [ ] STOP pauses
- [ ] RESET clears to 00:00.0
- [ ] Timer continues when viewing other screens

---

### Phase 3.3: Zone Settings Screens

| Task | Description | Validation |
|------|-------------|------------|
| 3.3.1 | Create generic `renderZoneSettings()` with radio list | Screen renders |
| 3.3.2 | Populate list from mesh node discovery | Nodes listed |
| 3.3.3 | Implement radio button selection | Selection works |
| 3.3.4 | Implement SAVE to Preferences | Selection persists |
| 3.3.5 | Create Humidity Zone Settings | Screen works |
| 3.3.6 | Create Temperature Zone Settings | Screen works |
| 3.3.7 | Create Light Zone Settings | Screen works |
| 3.3.8 | Create Motion/LED Zone Settings (dual list) | Screen works |
| 3.3.9 | Wire all gear icons to respective settings | Navigation works |
| 3.3.10 | Update clock corners to use default zones | Clock uses saved zones |

**Phase Validation:**
- [ ] All 4 zone settings screens accessible
- [ ] Available nodes listed from mesh
- [ ] Selection saved and persists
- [ ] Clock corners reflect default zone selections
- [ ] "No nodes found" message when applicable

---

### Milestone 3 Validation Checklist

- [ ] Alarm editable and persists
- [ ] Stopwatch runs in background
- [ ] All zone settings accessible and functional
- [ ] Default zones persist across reboots
- [ ] Clock corners show data from selected default zones

---

## Milestone 4: Polish & Optimization

**Goal:** Refine UX, optimize performance, handle edge cases.
**Deliverable:** Production-ready touch169 with all features polished.

### Phase 4.1: UX Polish

| Task | Description | Validation |
|------|-------------|------------|
| 4.1.1 | Add screen transition animations (optional) | Smooth transitions |
| 4.1.2 | Add touch feedback (visual ripple) | Feedback visible |
| 4.1.3 | Add "no data" states for missing sensors | Graceful handling |
| 4.1.4 | Add loading indicators where needed | No blank screens |
| 4.1.5 | Tune touch zone sizes for usability | No mis-taps |

### Phase 4.2: Performance & Stability

| Task | Description | Validation |
|------|-------------|------------|
| 4.2.1 | Profile memory usage | No leaks |
| 4.2.2 | Optimize screen rendering | Smooth updates |
| 4.2.3 | Test extended runtime (24+ hours) | Stable |
| 4.2.4 | Test rapid navigation | No crashes |
| 4.2.5 | Test with mesh disconnected | Graceful degradation |

### Milestone 4 Validation Checklist

- [ ] All screens render smoothly
- [ ] No memory leaks
- [ ] Graceful handling of missing data
- [ ] 24+ hour runtime stable
- [ ] Touch responsive and accurate

---

## Simulation & Testing

For development without full mesh hardware, see:
- [mesh_simulator_proposal.md](./mesh_simulator_proposal.md) - Mock mesh data for testing

**Testing Hardware:**
- Waveshare ESP32-S3-Touch-LCD-1.69 (required for phase validation)
- Optional: Additional mesh nodes (DHT, PIR, LED, Light) for full integration testing

**Simulation Options:**
- Wokwi ESP32 simulator (limited touch support)
- Mock mesh state injection via serial commands
- Simulated sensor data from PC over USB

---

## Summary

| Milestone | Phases | Key Deliverables |
|-----------|--------|------------------|
| M1 | 1.0 - 1.7 | Touch POC, screen architecture, clock details, time/date settings, nav menu |
| M2 | 2.1 - 2.5 | All sensor detail screens, zone selectors, calendar |
| M3 | 3.1 - 3.3 | Alarm, stopwatch, zone settings persistence |
| M4 | 4.1 - 4.2 | Polish, optimization, stability |

**Estimated Scope:** ~65 tasks across 4 milestones

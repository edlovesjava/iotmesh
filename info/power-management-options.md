# ESP32 Power Management Options (Research Notes)

## Overview
ESP32-based devices offer several layers of power management. The biggest gains come from sleep modes and radio duty-cycling, complemented by firmware optimizations and hardware power gating.

## 1) Sleep Modes (Primary Lever)
ESP32 variants typically support multiple sleep states. Choosing the right one depends on latency tolerance, required wake sources, and state retention needs.

- **Modem-sleep**
  - CPU stays active while Wi-Fi/BLE modem sleeps when idle.
  - Good for moderate savings without losing responsiveness.

- **Light-sleep**
  - CPU pauses; RAM is retained; RTC can wake the system.
  - Faster wake than deep sleep; still allows meaningful savings.

- **Deep-sleep**
  - Most of the chip powers down; RTC domain remains.
  - Best savings; limited wake sources (RTC timer, GPIO, touch/ULP depending on chip).

- **Hibernate** (varies by ESP32 variant)
  - Further shuts down RTC domain, lowest power draw.
  - Fewest wake options; longest wake latency.

## 2) Radio Power-Saving (Wi-Fi/BLE)
Reducing radio duty cycle can drastically lower power draw, especially for always-connected devices.

- **Wi-Fi power save**
  - Use DTIM-based listening intervals to reduce active time.

- **BLE advertising interval tuning**
  - Longer intervals reduce power for beacon/advertising roles.

## 3) Duty Cycling & Event-Driven Wake
Design firmware so the device wakes only when necessary.

- **RTC timer wake** for periodic sensor reads.
- **GPIO/touch wake** for event-driven devices (buttons, PIR sensors).
- **ULP coprocessor** for simple sensing while the main CPU sleeps.

## 4) CPU Frequency Scaling & Peripheral Gating
Lowering CPU frequency and disabling unused peripherals reduces consumption while active.

- **Dynamic frequency scaling** to reduce CPU clock during idle work.
- **Peripheral clock gating** (disable I2C/SPI/UART when not in use).

## 5) Hardware Power Gating (Sensors/Peripherals)
External components can dominate power draw; control their power rails.

- **MOSFET/Load switch** to fully cut power to sensors.
- **Use low-power sensors** with dedicated interrupt pins for wake.

## 6) Firmware & Protocol-Level Optimizations
Smarter communication patterns reduce radio-on time.

- **Batch telemetry** rather than streaming continuously.
- **Minimize Wi-Fi scan time** (static IP, preferred AP channels).
- **Use RTC memory** to skip heavy initialization after wake.

## Notes for MeshSwarm Context
MeshSwarm is currently positioned as best-suited for **mains-powered** devices (displays, gateways, control panels). Introducing low-power/sleepy nodes may require protocol changes such as:

- **Store-and-forward** or buffering messages for sleeping nodes.
- **State reconciliation** upon wake (full state sync or delta backlog).
- **Command timeouts** and retry strategies for nodes that are offline by design.

These implications should be considered when drafting PRD requirements for “sleepy end device” support.

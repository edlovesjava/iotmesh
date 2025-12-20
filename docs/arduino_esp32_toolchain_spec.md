# 📘 Arduino/ESP32 Agent Toolchain Specification (v1.0)

## 1. Purpose

Define a modular toolchain that allows an LLM‑based agent to:
- Inspect Arduino/ESP32 projects
- Build and flash firmware
- Analyze compiler/runtime errors
- Perform static analysis
- Interact with hardware via serial and I2C
- Edit project files deterministically

The goal is to minimize LLM reasoning by delegating deterministic tasks to tools.

## 2. System Architecture Overview

The system consists of four layers:

| Layer                  | Description                                   |
|------------------------|-----------------------------------------------|
| 1. Agent Brain         | LLM planner that selects tools and interprets results |
| 2. Orchestrator        | Stable API surface exposing tool functions    |
| 3. Tools               | CLI utilities, analyzers, hardware interfaces |
| 4. Workspace + Hardware| Project files and ESP32 device                |

The agent interacts only with the orchestrator.

## 3. Orchestrator API Specification

All functions must be:
- Deterministic
- Idempotent
- Return structured JSON
- Never return raw terminal noise unless explicitly requested

Below is the full API.

### `list_project_files(project_path)`

**Description:** Return a recursive listing of all files in the project.  
**Returns:**
```json
{ "ok": true, "files": ["project.ino", "src/main.cpp", "lib/MyLib/MyLib.cpp"] }
```

### `read_file(path)`

**Description:** Return the full contents of a file.  
**Returns:**
```json
{ "ok": true, "content": "void setup() { ... }" }
```

### `write_file(path, diff)`

**Description:** Apply a unified diff patch to a file.  
**Returns:**
```json
{ "ok": true, "applied": true }
```

### `build_arduino(project_path, fqbn)`

**Description:** Compile the project using Arduino CLI.  
**Returns:**
```json
{ "ok": false, "errors": ["'Wire' was not declared in this scope"], "warnings": ["unused variable 'x'"] }
```

### `flash_arduino(project_path, fqbn, port)`

**Description:** Upload firmware to the ESP32.  
**Returns:**
```json
{ "ok": true, "port": "/dev/ttyUSB0" }
```

### `monitor_serial(port, baud, timeout)`

**Description:** Capture serial output for a fixed duration.  
**Returns:**
```json
{ "ok": true, "log": "Guru Meditation Error: Core 1 panic'ed..." }
```

### `scan_i2c(port)`

**Description:** Run an I2C bus scan.  
**Returns:**
```json
{ "ok": true, "devices": ["0x3C", "0x76"] }
```

### `run_static_analysis(project_path)`

**Description:** Run `clang-tidy`, `cppcheck`, and Arduino Lint.  
**Returns:**
```json
{
  "ok": true,
  "issues": [
    {
      "file": "src/main.cpp",
      "line": 42,
      "severity": "error",
      "message": "Possible null pointer dereference"
    }
  ]
}
```

### `get_pinout_info(chip, pin)`

**Description:** Return pin capabilities and constraints.  
**Returns:**
```json
{
  "ok": true,
  "pin": 1,
  "capabilities": ["input-only", "RTC", "touch"],
  "warnings": ["Bootstrapping pin: avoid pulling low at boot"]
}
```

## 4. Agent Behavior Specification

The agent must follow a deterministic loop:

### 4.1 Initialization

1. Call `list_project_files()`
2. Call `read_file()` on all `.ino`, `.cpp`, `.h` files
3. Build an internal map of:
   - Entry points
   - Libraries used
   - Hardware interfaces (I2C, SPI, UART)

### 4.2 Build Loop

1. Call `build_arduino()`
2. If `ok == true`, proceed to flash
3. If `ok == false`:
   - Parse errors
   - Call `run_static_analysis()`
   - Identify minimal edits
   - Call `write_file(diff)`
   - Repeat build

### 4.3 Flash & Test Loop

1. Call `flash_arduino()`
2. Call `monitor_serial()`
3. If runtime errors detected:
   - Parse logs
   - If hardware-related → call `get_pinout_info()`
   - If I2C-related → call `scan_i2c()`
   - Apply fixes via `write_file(diff)`
   - Rebuild and flash

### 4.4 Stopping Conditions

The agent must stop when:
- Build succeeds
- Flash succeeds
- Serial output shows stable operation for N seconds
- No new errors appear

## 5. Tool Requirements

### 5.1 Arduino CLI

- Must support ESP32 boards via `esp32:esp32` core
- Must be preconfigured with board manager URLs
- Must expose FQBN list

### 5.2 Static Analysis Tools

- `clang-tidy` with Arduino‑compatible config
- `cppcheck` with MISRA‑friendly rules
- Arduino Lint for library structure

### 5.3 Serial Tools

- pyserial or Arduino CLI monitor
- Must support auto‑reconnect

### 5.4 Pinout Database

Local JSON files for:
- ESP32
- ESP32‑S2
- ESP32‑S3
- ESP32‑C3

## 6. Non‑Functional Requirements

### Performance

- Each tool call must complete in < 5 seconds (except build/flash)
- Serial monitor must stream at ≥ 115200 baud

### Reliability

- All tools must return structured JSON
- No raw terminal escape codes

### Safety

- Agent must never write outside project directory
- Agent must not modify bootloader partitions

## 7. Example End‑to‑End Flow

1. User: “Fix this ESP32‑S3 sketch and flash it.”
2. Agent: `list_project_files()`
3. Agent: `read_file()`
4. Agent: `build_arduino()` → fails
5. Agent: `run_static_analysis()`
6. Agent: `write_file(diff)`
7. Agent: `build_arduino()` → success
8. Agent: `flash_arduino()`
9. Agent: `monitor_serial()` → detects I2C error
10. Agent: `scan_i2c()`
11. Agent: `write_file(diff)`
12. Agent: rebuild → flash → monitor
13. Success

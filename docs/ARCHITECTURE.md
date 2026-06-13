# Architecture

---

## File Structure

```
ha-stiebel-control/
├── esphome/
│   ├── heatingpump.yaml                    # User entry point: board + model selection
│   ├── secrets.yaml                        # Credentials (gitignored)
│   ├── secrets.yaml.example                # Template for new users
│   ├── ci_secrets.yaml                     # Placeholder credentials for CI
│   ├── ci_heatingpump_s3.yaml              # CI compile target (ESP32-S3)
│   ├── ci_heatingpump_s2.yaml              # CI compile target (ESP32-S2)
│   └── ha-stiebel-control/
│       ├── common.yaml                     # Shared: WiFi, MQTT, API, OTA, sensors, SG Ready
│       ├── board_esp32s3.yaml              # ESP32-S3 hardware config (16MB flash, OPI PSRAM)
│       ├── board_esp32s2.yaml              # ESP32-S2 / generic ESP32 hardware config
│       ├── can_esp32.yaml                  # Built-in TWAI CAN (256 RX / 64 TX queue)
│       ├── can_mcp2515.yaml                # MCP2515 SPI CAN (legacy)
│       ├── wpl13e.yaml                     # Model: WPL 13 E — includes signal table
│       ├── wpf10.yaml                      # Model: WPF 10 / WPF 10M — community
│       ├── signal_requests_base.h          # Universal signals macro (all models)
│       ├── signal_requests_model.h         # Dispatcher (selects model at compile time)
│       ├── signal_requests_wpl13e.h        # WPL13E signal polling table
│       ├── signal_requests_wpf10.h         # WPF10 signal polling table
│       ├── ha-stiebel-control.h            # Core C++: CAN, MQTT discovery, calculated sensors
│       ├── sg_ready_controller.h           # SG Ready state machine (pure C++, testable)
│       ├── config.h                        # Timing constants, limits
│       └── elster/
│           ├── ElsterTable.h               # 3800+ signal definitions with HA metadata
│           ├── KElsterTable.cpp/h          # Table lookup, value conversion
│           └── NUtils.cpp/h, NTypes.h      # Utility functions and types
├── packages/
│   ├── ha_stiebel_control.yaml             # HA helpers (input_datetime for clock sync)
│   └── sg_ready_automation_example.yaml    # Example automation: E3DC → SG Ready
├── tests/
│   ├── test_sg_ready.cpp                   # Catch2 tests for SgReadyController
│   ├── test_can_logic.cpp                  # Tests for CAN/signal functions
│   ├── test_kelster.cpp                    # Tests for Elster table functions
│   ├── test_nutils.cpp                     # Tests for NUtils
│   ├── signal_requests_stub.cpp            # Stub for native test builds
│   ├── esphome_stubs.h                     # ESPHome API stubs for host compilation
│   ├── test_mqtt_smoke.py                  # MQTT regression test (post-OTA)
│   └── models/
│       ├── _common_smoke.json              # Required MQTT topics: all models
│       ├── wpl13e_smoke.json               # Required MQTT topics: WPL13E
│       └── wpf10_smoke.json                # Required MQTT topics: WPF10 (community stub)
├── docs/                                   # Extended documentation
├── manuals/                                # PDF manuals for reference
├── Makefile                                # Developer task runner
├── CLAUDE.md                               # Claude Code agent context
└── .github/
    ├── copilot-instructions.md             # Single source of truth for AI agents
    └── workflows/
        ├── ci.yml                          # CI: compile + unit test + YAML lint
        └── (release job in ci.yml)         # Release: compile → attach binaries → GitHub Release
```

---

## Data Flow

```
Heat Pump CAN Bus (Elster Protocol, 20kbps)
         │
         ▼
ESP32-S3 (built-in TWAI controller, board-specific CAN TX/RX pins)
 or ESP32-S2 + MCP2515 (SPI, board-specific pins)
         │
         ▼
ESPHome firmware (ha-stiebel-control.h)
  ┌──────────────────────────────────────┐
  │ processSignalRequests()              │  ← periodic CAN read requests
  │   └─ readSignal(member, "SIGNAL")    │
  │                                      │
  │ processAndUpdate(can_id, frame)      │  ← incoming CAN frames
  │   └─ processCanMessage()             │  ← decode Elster value
  │   └─ updateSensor(cm, ei, value)     │  ← publish MQTT + track for calcs
  │                                      │
  │ processCalculatedSensors()           │  ← scheduled derived values
  │   └─ publishDeltaTContinuous()       │
  │   └─ updateCOPCalculations()         │
  │   └─ publishDate(), publishTime()    │
  └──────────────────────────────────────┘
         │ MQTT
         ▼
MQTT Broker
  heatingpump/{MEMBER}/{SIGNAL}/state    ← raw sensor values
  heatingpump/calculated/{name}/state    ← derived values
  homeassistant/{type}/heatingpump/+/config ← auto-discovery
         │
         ▼
Home Assistant
  sensor.*, number.*, select.*           ← auto-created entities
```

---

## CAN Bus Architecture

The ESP32 acts as `PC` (CAN ID `0x680`) on the heat pump's internal CAN bus. It sends
read requests to specific CAN members and listens for their responses.

CAN member IDs:

| Name | CAN ID | Role |
|------|--------|------|
| `KESSEL` | 0x180 | Boiler / storage tank |
| `MANAGER` | 0x480 | Main controller |
| `HEIZMODUL` | 0x500 | Heat pump compressor module |
| `PC` | 0x680 | Our ESP32 (transmit address) |

The `CanMember` struct:
```cpp
typedef struct {
    const char *Name;    // e.g. "MANAGER"
    uint32_t CanId;      // e.g. 0x480
} CanMember;
```

Read frames: 7 bytes, indexed by Elster signal index. Single-byte index (0x00–0xFF) or
two-byte extended index (0xFA prefix + 2 bytes).

---

## Multi-Model Architecture

Signal polling is split into two layers to avoid duplication across models:

**`signal_requests_base.h`** — `SIGNAL_REQUESTS_BASE` macro with universal signals present
on all Elster heat pumps (date/time, EVU lock, operating mode, energy counters).

**`signal_requests_<model>.h`** — model-specific file that starts with `SIGNAL_REQUESTS_BASE`
and appends model-specific signals. Defines `signalRequests[]` and `SIGNAL_REQUEST_COUNT_VALUE`.

**`signal_requests_model.h`** — dispatcher that `#include`s the right model file based on
the `HA_MODEL_*` preprocessor flag (currently selects wpl13e or wpf10).

**`<model>.yaml`** — thin ESPHome package that brings the model `.h` into the build via
`esphome: includes:`.

The `HA_DEVICE_MODEL` string define (injected via `platformio_options: build_flags`) controls
the HA device identifier: `stiebel_eltron_<model>`.

---

## SG Ready Controller

The SG Ready state machine lives in `sg_ready_controller.h` as a pure C++ class
(`SgReadyController`) with no ESPHome dependencies. It interacts with hardware via the
`ISgReadyIO` interface, which is implemented by `EspHomeSgReadyIO` in production and
by `FakeIO` in the native unit test suite.

This separation allows the state machine logic to be tested with Catch2 on the host
without any ESP32 or ESPHome toolchain.

---

## MQTT Discovery Pattern

When a new CAN signal is received for the first time, `updateSensor()` calls
`publishMqttDiscovery()`, which publishes a retained JSON config message to:
```
homeassistant/{component}/heatingpump/{unique_id}/config
```

HA picks this up automatically. Signal state is then published to:
```
heatingpump/{CAN_MEMBER}/{SIGNAL_NAME}/state
```

Writable entities (numbers, selects) have a matching command topic:
```
heatingpump/{CAN_MEMBER}/{SIGNAL_NAME}/set
```

Discovery is re-published every 15 minutes for reliability, and can be triggered manually:
```bash
mosquitto_pub -h BROKER -u USER -P PASS -t "heatingpump/republish_discoveries" -m ""
```

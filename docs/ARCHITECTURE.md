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
│   ├── ci_heatingpump_esp32.yaml           # CI compile target (classic ESP32 + MCP2515)
│   ├── ci_heatingpump_s2.yaml              # CI config check of the deprecated board_esp32s2.yaml alias
│   └── ha-stiebel-control/
│       ├── common.yaml                     # Shared: MQTT, API, OTA, sensors, SG Ready; includes the network package
│       ├── network_wifi.yaml               # Default network: WiFi, captive portal, WiFi sensors
│       ├── network_ethernet_w5500.yaml     # Alternative network: W5500 SPI Ethernet
│       ├── board_esp32s3.yaml              # ESP32-S3 hardware config (ESP-IDF, 16MB flash, OPI PSRAM)
│       ├── board_esp32.yaml                # Classic ESP32 (esp32dev, ESP-IDF, 4MB) hardware config
│       ├── board_esp32s2.yaml              # Deprecated alias of board_esp32.yaml (never was an S2)
│       ├── no_captive_portal.yaml          # Optional: removes captive portal + fallback hotspot
│       ├── can_esp32.yaml                  # Built-in TWAI CAN (256 RX / 64 TX queue)
│       ├── can_mcp2515.yaml                # MCP2515 SPI CAN (legacy)
│       ├── <model>.yaml                    # Model package (wpl9, wpl10ac, wpl13e, wpl17, wpl18e, wpf10, wpc7, ttl33, wpla7):
│       │                                   #   only selects its signal_requests_<model>.h
│       ├── signal_requests_base.h          # Universal signals macro (all models) + shared row macros
│       ├── signal_requests_<model>.h       # Request table of one model (e.g. signal_requests_wpl13e.h)
│       ├── model_profile.h                 # Model capabilities (kModel), the only place that looks at the model
│       ├── can_members.h                   # Bus members (CanMembers[], CanMemberType) and SignalRequest, no ESPHome deps
│       ├── ha-stiebel-control.h            # CAN entry points: processAndUpdate(), updateSensor()
│       ├── signal_route.h                  # Signal key and model route (pure C++)
│       ├── mqtt_publish.h                  # MQTT publish, publish-on-change state cache
│       ├── can_io.h                        # Frame decode, read/write requests, read-back
│       ├── paced_writes.h                  # Paced write queue (SG Ready, time programs)
│       ├── discovery.h                     # HA device blocks and discovery payloads
│       ├── bus_scan.h                      # Bus scan of one member
│       ├── request_manager.h               # Polling of the model's request table
│       ├── calculated.h                    # calculatedSensors[] and derived values
│       ├── energy.h                        # COP and combined energy values
│       ├── writables.h                     # Writable numbers/selects, climate discovery
│       ├── sg_ready_io.h                   # SG Ready glue (ESPHome adapter, commands)
│       ├── time_programs.h                 # WPMW II time programs in HA
│       ├── set_commands.h                  # MQTT set dispatcher handleSetCommand()
│       ├── fault_list.h                    # Manager fault list
│       ├── republish.h                     # Discovery/state republish after a connect
│       ├── sg_ready_controller.h           # SG Ready state machine (pure C++, testable)
│       ├── dhw_program.h                   # WPMW II time programs: parsing/validation (pure C++, testable)
│       ├── config.h                        # Timing constants, limits
│       └── elster/
│           ├── ElsterTable.h               # signal table (name, index, type, flags) + ElsterMetaTable (HA metadata)
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
│   ├── check_modules.py                    # Firmware headers: no include cycle, main header size
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
ESP32-S3 (built-in TWAI controller)
  — Waveshare ESP32-S3-RS485-CAN: GPIO15 TX, GPIO16 RX
  — other boards: check schematic
 or classic ESP32 + MCP2515 (SPI)
  — generic DevKit: CLK GPIO18, MOSI GPIO23, MISO GPIO19, CS GPIO5
  — other boards: check schematic
         │
         ▼
ESPHome firmware
  ┌────────────────────────────────────────────────────────┐
  │ processSignalRequests()          request_manager.h     │  ← periodic CAN read requests
  │   └─ readSignal(member, "SIGNAL")  can_io.h            │
  │                                                        │
  │ processAndUpdate(can_id, frame)  ha-stiebel-control.h  │  ← incoming CAN frames
  │   └─ processCanMessage()           can_io.h            │  ← decode Elster value
  │   └─ updateSensor(cm, ei, value)   ha-stiebel-control.h│  ← publish MQTT + track for calcs
  │                                                        │
  │ processCalculatedSensors()       calculated.h          │  ← scheduled derived values
  │   └─ publishDeltaTContinuous()                         │
  │   └─ publishDate(), publishTime()                      │
  │ updateCOPCalculations()          energy.h              │  ← on each energy counter value
  └────────────────────────────────────────────────────────┘
         │ MQTT
         ▼
MQTT Broker
  heatingpump/{MEMBER}/{SIGNAL}/state    ← raw sensor values (root = device_name, HA_TOPIC_ROOT)
  heatingpump/calculated/{name}/state    ← derived values
  homeassistant/{type}/heatingpump/+/config ← auto-discovery
         │
         ▼
Home Assistant
  sensor.*, number.*, select.*           ← auto-created entities
```

---

## Firmware Modules

The firmware C++ code is split by responsibility (#109). `ha-stiebel-control.h` holds only the
CAN entry points (`processAndUpdate()` for every received frame, `updateSensor()` for every
decoded value) and includes the modules. Each module owns its global state and includes the
modules it uses; the includes form no cycle. Everything still compiles into one translation unit
(the sketch ESPHome generates, or `tests/test_can_logic.cpp`), so the split does not change the firmware.

The table is in include order: a module includes only modules above it. `config.h`,
`model_profile.h`, `can_members.h`, the language files and the Elster library are used by many
modules and not listed.

| Module | Responsibility | Includes (modules) |
|---|---|---|
| `signal_route.h` | Signal key (`signalKey()`) and model route (`resolveSignalRoute()`), pure C++ | — |
| `mqtt_publish.h` | `mqttPublish()` with failure counter, publish-on-change state cache | signal_route |
| `can_io.h` | Frame decode, read/write requests, read-back after a write, clock/date writes | signal_route, mqtt_publish |
| `paced_writes.h` | Paced write queue for SG Ready and the time programs | can_io |
| `discovery.h` | HA device blocks, default entity IDs, discovery of raw signals and calculated sensors | signal_route, mqtt_publish |
| `bus_scan.h` | Bus scan of one CAN member | can_io, mqtt_publish |
| `request_manager.h` | Polling of the model's request table, back-off | signal_route, can_io, bus_scan |
| `calculated.h` | `calculatedSensors[]` and the derived values, their scheduler | signal_route, mqtt_publish, discovery, request_manager |
| `energy.h` | COP and the combined energy values | calculated |
| `writables.h` | Writable numbers and selects, their discovery and the climate entities; device limits of the numbers (0xF8/0xF9, #79), hooked into `writeSignal()` and the request manager | signal_route, mqtt_publish, discovery, can_io, request_manager |
| `sg_ready_io.h` | ESPHome adapter of `SgReadyController` (`sg_ready_controller.h`), SG Ready commands | signal_route, mqtt_publish, paced_writes, writables |
| `time_programs.h` | WPMW II time programs in Home Assistant (parsing: `dhw_program.h`) | signal_route, mqtt_publish, discovery, paced_writes |
| `set_commands.h` | Dispatcher of every `<root>/<A>/<B>/set` command | can_io, writables, sg_ready_io, time_programs |
| `fault_list.h` | Fault list of the manager, remedy texts | can_io, bus_scan, mqtt_publish, discovery |
| `republish.h` | Discovery and state republish after an MQTT connect or HA birth message | mqtt_publish, discovery, writables, calculated, energy, time_programs |
| `ha-stiebel-control.h` | CAN entry points `processAndUpdate()`, `updateSensor()` | all of the above |

`common.yaml` lists every module in `esphome: includes:` so ESPHome copies it.
`tests/check_modules.py` (CI) checks that the includes form no cycle, that
`ha-stiebel-control.h` stays below 800 lines and that every included module is listed.

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

**`<model>.yaml`** — thin ESPHome package that brings the model `.h` into the build via
`esphome: includes:`. There is no dispatcher: the model YAML picks the table.

**`model_profile.h`** — the build flag `-DHA_DEVICE_MODEL=<model>` (from the `device_model`
substitution) is evaluated here only. It sets the preprocessor switches for the signal table and
the language files (`HA_CONTROLLER_WPMW2`, `HA_CONTROLLER_WPM3`, `HA_ELSTER_WPL_FAMILY`) and the
constexpr `kModel` with the capability flags (`wpmw2WritesVerified`, `canBRoutes`,
`perMemberEnergy`, …). Code reads `kModel.<flag>` and never compares model names.

The `HA_DEVICE_MODEL_STR` string also forms the HA device identifier: `stiebel_eltron_<model>`.

---

## SG Ready Controller

The SG Ready state machine lives in `sg_ready_controller.h` as a pure C++ class
(`SgReadyController`) with no ESPHome dependencies. It interacts with hardware via the
`ISgReadyIO` interface, which is implemented by `EspHomeSgReadyIO` in production and
by `FakeIO` in the native unit test suite.

This separation allows the state machine logic to be tested with Catch2 on the host
without any ESP32 or ESPHome toolchain.

`EspHomeSgReadyIO` does not write directly: it puts each write into a paced queue
(`queuePacedWrite()` / `pacedWriteStep()` in `paced_writes.h`). The queue sends one write
at a time, at least 1.5 s apart, checks the settled read-back and sends a write once more if
the heat pump still reports another value. Writes sent back to back were partly lost on a
WPMW II (#85). The time-program commands (DHW, heating circuit 1) use the same queue.

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

`heatingpump` is the default topic root. It is the `device_name` substitution, passed to the C++
code as `-DHA_DEVICE_NAME` and used there as `HA_TOPIC_ROOT` (`config.h`); YAML uses
`${device_name}`.

All discovery configs are retained. They are sent again (a few per 250 ms, so the MQTT
queue never sees a burst) on every broker connect and when Home Assistant publishes its
birth message (`homeassistant/status` = `online`). There is no periodic republish.
A writable number is also published once more when its device limits (switch bytes `0xF8`/`0xF9`)
have been read, with the same `unique_id` and the device range.
A republish can also be triggered manually:
```bash
mosquitto_pub -h BROKER -u USER -P PASS -t "heatingpump/republish_discoveries" -m ""
```

## COP and booster energy

COP = (heat yield + 2WE yield) / electrical consumption, per DHW, heating and total.
`WAERMEERTRAG_2WE_*` is the second heat source, i.e. the electric booster (NHZ/DHC);
kr0ner/OneESP32ToRuleThemAll names the same WPM3 registers `EL_ENERGIEAUFNAHME_*_NHZ`.
For a resistive booster heat ≈ electricity, so adding it to the heat side is correct as
long as the booster's electricity is also part of `EL_AUFNAHMELEISTUNG_*` — which has not
been verified on hardware. The MWh registers only carry whole MWh; where the model provides
`*_SUM_KWH`, the remainder is added (`copMWh()`), and the COP waits until all four
remainders are known (`copKwhIncomplete()`).

## Polling back-off and entity creation

A signal answering `-255` (0x8000, "not available") creates no HA entity until it has
delivered a real value once. From the third consecutive `-255` its poll interval doubles
per answer, capped at 6 h (`nextBackoffMs()`); a valid value resets it. Entities that
already exist keep receiving `-255` so their history stays intact.

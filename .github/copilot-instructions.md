# ha-stiebel-control AI Assistant Instructions

## Project Overview
ESPHome/Home Assistant integration for monitoring and controlling Stiebel Eltron heat pumps via CAN bus interface. Supports two hardware configurations: ESP32-S3 with built-in TWAI CAN (recommended) and ESP32-S2 with MCP2515 SPI CAN transceiver (legacy). Based on the Elster protocol for communication with heat pump components.

## Architecture

### Core Components
- **ESPHome Firmware** (`esphome/`): ESP32 code interfacing with Stiebel Eltron heat pump via CAN bus
  - `heatingpump.yaml`: Main entry point with board selection (comment/uncomment blocks for ESP32-S3 or ESP32-S2)
  - **Modular Board Configuration** (`ha-stiebel-control/`):
    - `board_esp32s3.yaml`: ESP32-S3 board config (16MB flash, OPI PSRAM @ 80MHz)
    - `board_esp32s2.yaml`: ESP32-S2/generic ESP32 board config
    - `can_esp32.yaml`: Built-in TWAI CAN controller (256 RX queue, 64 TX queue, optimized for high traffic)
    - `can_mcp2515.yaml`: MCP2515 SPI-based CAN controller (2 RX buffers, legacy)
    - `common.yaml`: Board-agnostic core (WiFi, MQTT, API, OTA, buttons, text sensors)
    - `wpl13e.yaml`: Device-specific sensors for WPL13E heat pump model (template for other models)
  - `ha-stiebel-control.h`: Main C++ logic for CAN communication and Elster protocol handling
  
- **Elster Protocol Library** (`esphome/ha-stiebel-control/elster/`): C/C++ implementation of Stiebel Eltron's proprietary protocol
  - `ElsterTable.h`: 3800+ line table mapping signal names to CAN indices and data types
  - `KElsterTable.cpp/.h`: Helper functions for table lookups and value conversions
  - `NUtils.cpp/.h`, `NTypes.h`: Utility functions and type definitions

- **Home Assistant Integration** (`packages/`): Home Assistant configuration packages
  - `ha_stiebel_control.yaml`: Input helpers for datetime control (heatingpump_time, heatingpump_date)
  
- **Documentation** (`manuals/`): PDF manuals for Stiebel Eltron heat pumps
  - Consult these for understanding signal meanings, system behavior, and operational modes

### CAN Bus Architecture
- **CAN Member IDs**: Fixed identifiers for heat pump components (defined in `ha-stiebel-control.h`):
  - `KESSEL` (0x180): Boiler/storage tank
  - `MANAGER` (0x480): Main controller
  - `HEIZMODUL` (0x500): Heating module
  - Additional: ATEZ, control modules, mixer modules, etc.
- **Communication Pattern**: ESP32 acts as `PC` (0x680) on the CAN bus, reads/writes signals using Elster indices
- **Universal Frame Handler**: Single `on_frame` handler with `can_id_mask: 0` processes all CAN messages via `processAndUpdate()`

## Critical Patterns

### ESPHome Sensor Definition Pattern
All sensors follow this template structure:
```yaml
- platform: template
  name: "SIGNAL_NAME"
  id: SIGNAL_NAME
  unit_of_measurement: "°C"
  device_class: "temperature"
  state_class: "measurement"
  accuracy_decimals: 1
  update_interval: 1min
  lambda: |-
    readSignal(&CanMembers[cm_manager], "SIGNAL_NAME");
    return {};
```
**Key points:**
- Sensors return `{}` - actual value publishing handled by `updateSensor()` in C++ code
- Signal names must match entries in `ElsterTable.h` exactly
- Use appropriate `CanMember` enum: `cm_kessel`, `cm_manager`, `cm_heizmodul`
- Home Assistant entities auto-prefixed with "heatingpump_"

### Raw vs. Calculated Sensors

**Raw Sensors** (Direct CAN Bus Readings):
- Defined in `wpl13e.yaml` as ESPHome template sensors
- Read directly from CAN bus using `readSignal(&CanMembers[...], "SIGNAL_NAME")`
- Examples: TEMP_AUSSEN, SPEICHERSOLLTEMP, PROGRAMMSCHALTER
- Published automatically via MQTT discovery to Home Assistant
- **Adding new raw sensor:**
  1. Find signal name in `ElsterTable.h` (search by German name)
  2. Add template sensor to `wpl13e.yaml` following the pattern above
  3. Use correct `CanMember` enum based on signal source
  4. Set appropriate `update_interval` (1min for frequent, 10min for rare)

**Calculated Sensors** (Derived Values):
- Computed in C++ code within `ha-stiebel-control.h`
- Published to MQTT topic: `heatingpump/calculated/{sensor_name}`
- Examples: 
  - `date`: Computed from JAHR, MONAT, TAG signals
  - `time`: Computed from STUNDE, MINUTE, SEKUNDE signals
  - `betriebsart`: "Sommer" or "Winter" derived from SOMMERBETRIEB
  - `delta_t_continuous`: Flow - Return temperature
  - `compressor_active`: Binary sensor from VERDICHTER > 2
  - `cop_ww`, `cop_heizung`, `cop_gesamt`: COP calculations
- **Adding new calculated sensor:**
  1. Add calculation logic in `ha-stiebel-control.h` (typically in interval callbacks)
  2. Publish to MQTT: `id(mqtt_client).publish("heatingpump/calculated/your_sensor", value)`
  3. Optionally create HA template sensor in `packages/ha_stiebel_control.yaml` to subscribe to the topic
  4. Use appropriate state class, device class, and units

### Write Operations
Use `writeSignal()` followed by `id(SENSOR).update()`:
```cpp
writeSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER", value);
readSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER");
```

### Configuration Variables Pattern
**Multi-Board Architecture** - Project uses package-based modular configuration:
```yaml
# User selects ONE configuration block in heatingpump.yaml

# ESP32-S3 Configuration (Recommended)
substitutions:
  can_tx_pin: GPIO15      # Built-in TWAI TX
  can_rx_pin: GPIO16      # Built-in TWAI RX
  can_id_pc: "0x680"
packages:
  board: !include ha-stiebel-control/board_esp32s3.yaml
  can: !include ha-stiebel-control/can_esp32.yaml
  base: !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml

# OR

# ESP32-S2 Configuration (Legacy)
substitutions:
  can_clk_pin: GPIO18     # MCP2515 SPI CLK
  can_mosi_pin: GPIO23    # MCP2515 SPI MOSI
  can_miso_pin: GPIO19    # MCP2515 SPI MISO
  can_cs_pin: GPIO5       # MCP2515 SPI CS
  can_id_pc: "0x680"
packages:
  board: !include ha-stiebel-control/board_esp32s2.yaml
  can: !include ha-stiebel-control/can_mcp2515.yaml
  base: !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml
```

**Critical CAN Bus ID Convention:**
- Both `can_esp32.yaml` and `can_mcp2515.yaml` use `id: my_mcp2515` for the canbus component
- This allows C++ code in `ha-stiebel-control.h` to work with both platforms using `id(my_mcp2515).send_data()`
- **Never** change the canbus component ID - it's intentionally standardized across both platforms

### Device Variants
To support additional heat pump models:
1. Create new device-specific YAML file (e.g., `wpl25a.yaml`) based on `wpl13e.yaml` structure
2. **Consult manuals** (`manuals/` folder) to understand signal meaning and system behavior
2. Locate signal name in `esphome/elster/ElsterTable.h` (search by German name)
3. Add template sensor to appropriate section in `wpl13e.yaml` or device-specific file
4. Use correct `CanMember` enum based on which component provides the signal:
   - `cm_kessel` for boiler/tank signals
   - `cm_manager` for control system signals
   - `cm_heizmodul` for heat pump module signals
5. Set appropriate `update_interval` (1min for frequent, 10min for rare)

## File Modification Guidelines

### Which File to Modify
**Follow these rules strictly to maintain architecture:**

- **heatingpump.yaml**: ONLY modify to select board configuration (comment/uncomment blocks). Never add sensors, components, or logic here.
- **board_esp32s3.yaml / board_esp32s2.yaml**: ONLY board-specific hardware config (flash, PSRAM, framework). Never add WiFi, MQTT, sensors, or CAN config.
- **can_esp32.yaml / can_mcp2515.yaml**: ONLY CAN platform config. Never add sensors or non-CAN components. Maintain standardized `id: my_mcp2515`.
- **common.yaml**: MUST remain board-agnostic. Only WiFi, MQTT, API, OTA, buttons, text sensors, intervals. Never add board-specific pins or CAN config.
- **wpl13e.yaml**: Raw sensor definitions only. Add new heat pump sensors here following template pattern.
- **ha-stiebel-control.h**: C++ logic for CAN communication, calculated sensors, write operations. Modify for complex calculations.
- **ElsterTable.h**: Signal definitions. Modify ONLY to add payloadOn/payloadOff for binary sensors or correct signal parameters.
- **packages/ha_stiebel_control.yaml**: Home Assistant input helpers and template sensors. Add HA-side calculations here.

### Package System Rules
**Critical for multi-board support:**
- Never merge package files - keep board/can/common/sensors separate
- Board config files must not reference CAN pins or SPI
- CAN config files must not reference board hardware (flash, PSRAM)
- Common.yaml must work with ANY board configuration
- All board-specific values use substitutions (${can_tx_pin}, ${can_clk_pin})

# Development Workflow

### Adding New Sensors
1. Locate signal name in `esphome/elster/ElsterTable.h` (search by German name)
2. Add template sensor to `wpl13e.yaml` following the pattern
3. Use correct `CanMember` enum based on signal source
4. Set appropriate `update_interval` (1min for frequent, 10min for rare)

### Datetime Control Pattern
**Home Assistant does NOT support datetime.mqtt platform**
- Use `input_datetime` helpers in `packages/ha_stiebel_control.yaml`
- Sync to ESPHome via `homeassistant` platform text sensors
- Button press triggers lambda to parse datetime string and write to CAN
- Example:
  ```yaml
  button:
    - platform: template
      name: "Update Time"
      on_press:
        then:
          - lambda: |-
              std::string time_str = id(heatingpump_time).state;
              // Parse HH:MM:SS and write STUNDE, MINUTE, SEKUNDE signals
  ```
- **Never** attempt to use MQTT datetime entities - they don't exist

### Binary Sensor Pattern
**For binary sensors to work correctly:**
1. Signal must have `payloadOn` and `payloadOff` in ElsterTable.h (10th and 11th parameters)
2. Example: `"EVU_SPERRE_AKTIV", ..., "on", "off", ...`
3. Without these, sensor shows "Unbekannt" (Unknown) in Home Assistant
4. Inversion logic (if needed) handled in C++ code, not ElsterTable.h

### Testing and Verification Workflow
**After any code changes:**
1. Run `esphome compile heatingpump.yaml` to check for errors
2. Verify flash usage stays under 50% (allows OTA updates)
3. Check RAM usage stays under 50%
4. Test BOTH board configurations if modifying shared files:
   - Uncomment ESP32-S3 block, compile
   - Uncomment ESP32-S2 block, compile
5. Monitor logs after upload for CAN errors or buffer overflows

### MQTT Discovery Pattern
**How entities appear in Home Assistant:**
- ESPHome automatically publishes MQTT discovery messages
- Entity IDs: `sensor.heatingpump_signal_name` (auto-prefixed)
- Discovery topic: `homeassistant/sensor/heatingpump/SIGNAL_NAME/config`
- Raw sensors: Published via MQTT discovery (automatic)
- Calculated sensors: Published to `heatingpump/calculated/{name}` topics
- Numbers/selects: Defined in ESPHome YAML, auto-discovered
- **Never** manually create MQTT discovery JSON - ESPHome handles this

## Common Pitfalls to Avoid

### Architecture Violations
- ❌ Adding sensors to heatingpump.yaml instead of wpl13e.yaml
- ❌ Adding WiFi/MQTT config to board-specific files
- ❌ Adding board config (flash, PSRAM) to common.yaml
- ❌ Changing canbus component ID from `my_mcp2515`
- ❌ Merging package files "for simplicity"
- ❌ Using different CAN buffer sizes without understanding impact

### Code Patterns
- ❌ Returning sensor values from lambda: `return 25.5;`
  ✅ Return empty: `return {};` and let C++ publish
- ❌ Using `GetElsterIndex()` in code
  ✅ Use string names: `readSignal(&CanMembers[cm_manager], "TEMP_AUSSEN")`
- ❌ Forgetting `id(SENSOR).update()` after write operations
  ✅ Always call update: `writeSignal(...); id(sensor).update();`
- ❌ Adding datetime.mqtt entities
  ✅ Use input_datetime + homeassistant text_sensor + button

### Signal Configuration
- ❌ Adding binary sensor without payloadOn/payloadOff
  ✅ Add `"on", "off"` to ElsterTable.h signal definition
- ❌ Using wrong CanMember enum (e.g., cm_kessel for manager signal)
  ✅ Consult manuals and ElsterTable.h comments for correct source
- ❌ Guessing signal names
  ✅ Search ElsterTable.h for exact German name

### Home Assistant Integration
- ❌ Creating duplicate entities in packages/ha_stiebel_control.yaml
  ✅ Use MQTT discovery for raw sensors, only add HA template sensors for calculations
- ❌ Hardcoding values in templates without checking for `unknown`/`unavailable`
  ✅ Always check: `{% if states('sensor.x') not in ['unknown', 'unavailable'] %}`

### Testing Mistakes
- ❌ Only testing one board configuration when modifying shared files
  ✅ Test both ESP32-S3 and ESP32-S2 configs
- ❌ Uploading firmware without checking flash usage
  ✅ Verify flash <50% to allow OTA updates
- ❌ Ignoring compilation warnings
  ✅ Fix all warnings - they often indicate real issues

## Development Best Practices

### Before Making Changes
1. **Understand the signal**: Consult `manuals/` folder PDFs for signal meaning
2. **Find the signal**: Search ElsterTable.h for exact name (German)
3. **Identify the source**: Determine which CanMember provides the signal
4. **Check existing patterns**: Look at similar sensors in wpl13e.yaml

### When Adding Features
1. **Start with raw sensors**: Add to wpl13e.yaml first, verify CAN communication
2. **Add calculations**: If derivation needed, add to ha-stiebel-control.h
3. **Test incrementally**: Upload and verify each sensor before adding more
4. **Monitor logs**: Check for CAN errors, buffer overflows, MQTT issues

### When Debugging
1. **Check logs first**: `esphome logs heatingpump.yaml`
2. **Verify signal exists**: Search ElsterTable.h for exact name
3. **Check CanMember**: Ensure correct enum used (cm_kessel/cm_manager/cm_heizmodul)
4. **Test write operations**: Use `readSignal()` immediately after `writeSignal()` to verify
5. **MQTT monitoring**: Use MQTT Explorer to see actual messages

# Development Workflow

### Adding New Sensors (Continued)
1. Locate signal name in `esphome/elster/ElsterTable.h` (search by German name)
2. Add template sensor to `wpl13e.yaml` following the pattern in packages/ha_stiebel_control.yaml` compute derived values
- Use Jinja2 templates for calculations (e.g., Delta T = flow - return temp)
- Input helpers sync to ESPHome via `homeassistant` platform text sensors
- Entity naming: prefix "heatingpump_" auto-added by ESPHome
- Package must be included in HA config: `homeassistant: packages: !include_dir_named packages`

### Modifying CAN Configuration
**DO NOT** modify CAN configuration directly in board-specific files. The architecture uses:
- `can_esp32.yaml`: ESP32-S3 built-in TWAI with optimized buffers (rx_queue_len: 256, tx_queue_len: 64)
- `can_mcp2515.yaml`: MCP2515 SPI controller with hardware-limited buffers (2 RX, 3 TX)
- Both use 20kbps bit rate, extended_id: false
- Both use `id: my_mcp2515` (standardized for C++ compatibility)
- **Never** modify `on_frame` handler - uses universal `processAndUpdate()` pattern

**ESP32-S3 Buffer Optimization:**
- RX queue: 256 entries (default 8) - prevents buffer overflows from CAN burst traffic
- TX queue: 64 entries (default 16) - handles write operation bursts
- These settings are critical for stable operation with heat pump CAN traffic

### Home Assistant Integration
- Template sensors in `ha_stiebel_control.yaml` compute derived values
- Use Jinja2 templates for calculations (e.g., Delta T = flow - return temp)
- Input helpers sync to ESPHome via `homeassistant` platform text sensors
- Entity naming: prefix "heatingpump_" auto-added by ESPHome

## Important Constraints

### ESPHome Specifications
**ESP32-S3 Configuration (Recommended):**
- Board: esp32-s3-devkitc-1
- Flash: 16MB
- PSRAM: OPI mode @ 80MHz
- Framework: Arduino
- CAN: Built-in TWAI controller (GPIO15 TX, GPIO16 RX)
- Buffer sizes: 256 RX queue, 64 TX queue

**ESP32-S2 Configuration (Legacy):**
- Board: esp32dev
- Framework: Arduino
- CAN transceiver: MCP2515 via SPI
- SPI pins: CLK (GPIO18), MOSI (GPIO23), MISO (GPIO19), CS (GPIO5)
- Buffer sizes: 2 RX, 3 TX (hardware limited)

**Common Requirements:**
- Must include all 7 Elster library files in includes
- CAN bus ID convention: `id: my_mcp2515` for both platforms
- Bit rate: 20kbps, extended_id: false

### Home Assistant Specifications  
- Use `state_class: measurement` for numeric sensors
- Device classes: `temperature`, `energy`, `duration`, `running`
- Jinja2 templates: check for `unknown`/`unavailable`/`none` states
- Icon format: `mdi:icon-name`

### Elster Protocol Rules
- Read operations: 7-byte CAN frames with generated read IDs
- Write operations: require exact value format per signal type
- Index resolution: single byte (0x00-0xFF) or double byte (0xFA prefix for 0x0000-0xFFFF)
- Value types: `et_dec_val` (0.1), `et_cent_val` (0.01), `et_bool`, etc.

## Key Files Reference
- Main entry point: [esphome/heatingpump.yaml](esphome/heatingpump.yaml)
- Board configs: [esphome/ha-stiebel-control/board_esp32s3.yaml](esphome/ha-stiebel-control/board_esp32s3.yaml), [board_esp32s2.yaml](esphome/ha-stiebel-control/board_esp32s2.yaml)
- CAN configs: [esphome/ha-stiebel-control/can_esp32.yaml](esphome/ha-stiebel-control/can_esp32.yaml), [can_mcp2515.yaml](esphome/ha-stiebel-control/can_mcp2515.yaml)
- CAN member definitions: [esphome/ha-stiebel-control/ha-stiebel-control.h](esphome/ha-stiebel-control/ha-stiebel-control.h#L30-L51)
- Signal table: [esphome/ha-stiebel-control/elster/ElsterTable.h](esphome/ha-stiebel-control/elster/ElsterTable.h)
- Base config: [esphome/ha-stiebel-control/common.yaml](esphome/ha-stiebel-control/common.yaml)
- Device sensors: [esphome/ha-stiebel-control/wpl13e.yaml](esphome/ha-stiebel-control/wpl13e.yaml)
- HA helpers: [packages/ha_stiebel_control.yaml](packages/ha_stiebel_control.yaml)
- Manuals: [manuals/](manuals/) - PDF reference documentation

## Common Gotchas
- **Don't** use `GetElsterIndex()` - use string names directly: `readSignal(&CanMembers[cm_manager], "SIGNAL_NAME")`
- **Don't** return sensor values in lambda - return `{}` and let C++ callback handle publishing
- **Always** check signal exists in ElsterTable.h before adding sensors
- **Archive folder**: Contains old code versions - reference only, not part of active codebase
- **Substitutions**: Currently not used but prepared via `${}` syntax for future device variants

---

## Multi-Model Architecture (v2.0+)

### How model files work

The signal request table is split into two layers:

1. **`signal_requests_base.h`** — `SIGNAL_REQUESTS_BASE` macro with universal Elster signals
   (date/time, EVU lock, operating mode, energy counters). All models include this.

2. **`signal_requests_<model>.h`** — model-specific file that starts with `SIGNAL_REQUESTS_BASE`
   and appends model-specific signals. Defines `signalRequests[]` and `SIGNAL_REQUEST_COUNT_VALUE`.

3. **`<model>.yaml`** — thin ESPHome package that declares its model `.h` in `esphome: includes:`.
   Generic sensors (COP, DHW, operating mode) live in `common.yaml`, not here.

4. **`ha-stiebel-control.h`** — forward-declares `signalRequests[]` and `SIGNAL_REQUEST_COUNT_VALUE`
   so functions can reference them before the model include is processed.

### Adding a new heat pump model

1. Create `esphome/ha-stiebel-control/signal_requests_yourmodel.h`:
```cpp
#ifndef SIGNAL_REQUESTS_YOURMODEL_H
#define SIGNAL_REQUESTS_YOURMODEL_H

#include "config.h"
#include "signal_requests_base.h"

const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // universal signals

    // Your model-specific signals here
    {"YOUR_SIGNAL", FREQ_30S, cm_manager},
};

const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif
```

2. Create `esphome/ha-stiebel-control/yourmodel.yaml`:
```yaml
esphome:
  includes:
    - ha-stiebel-control/signal_requests_yourmodel.h
```

3. Use it in `heatingpump.yaml`:
```yaml
substitutions:
  device_model: "yourmodel"   # used in HA device identifier
packages:
  sensors: !include ha-stiebel-control/yourmodel.yaml
```

4. Verify: `make config` (YAML parses) then `make compile` (C++ compiles)

### Device identifier

`device_model` substitution controls the HA device identifier: `stiebel_eltron_<device_model>`.
Set it in `heatingpump.yaml`. The `HA_DEVICE_MODEL` C++ string define is injected via
`platformio_options: build_flags:` in `common.yaml`.

### i18n — Display language

Entity display names are compile-time translated via `LNAME_*` macros. The `language`
substitution in `heatingpump.yaml` selects the language (`DE` default, `EN` available).
The build flag `-DHA_LANGUAGE_${language}` is injected in `common.yaml`.

**File layout** (`esphome/ha-stiebel-control/`):
- `lang_base.h` — all `LNAME_*` macros with German strings (single source of truth)
- `lang_en.h` — includes `lang_base.h`, then `#undef`/`#define` for English translations
- `language_select.h` — dispatch: `#ifdef HA_LANGUAGE_EN` → `lang_en.h`, else `lang_base.h`

**Rules:**
- Only `language_select.h` goes in the `esphome: includes:` list — `lang_base.h` and
  `lang_en.h` are copied transitively and must NOT be auto-included by ESPHome
- Signal protocol names (`TEMP_AUSSEN`, option values like `"Tagbetrieb"` used in CAN writes) stay German regardless of language
- Only signals with `hasMetadata=true` in `ElsterTable.h` have `LNAME_*` macros; unnamed signals fall back to the raw signal name

**Adding a new language:**
1. Create `lang_XX.h`: `#include "lang_base.h"`, then override desired macros
2. Add `#elif defined(HA_LANGUAGE_XX) / #include "lang_XX.h"` in `language_select.h`

### Version bumping

The project version lives in `common.yaml` under `esphome: project: version:`.
Bump it there before tagging a release. Tag format: `vX.Y.Z` — CI creates a GitHub Release
automatically.

### Releasing

1. Update version in `common.yaml` (`esphome: project: version: "X.Y.Z"`)
2. Update `CHANGELOG.md` with the new version section
3. Commit: `git commit -m "release: vX.Y.Z"`
4. Tag: `git tag vX.Y.Z && git push origin main --tags`
5. CI runs compile + release pipeline and creates the GitHub Release

### After OTA flash — mandatory

After every `make upload` to the production device, always run:
```bash
make smoke-test
```
This verifies all required signals (common + model-specific) appear within 120s.
Exit 0 = pass. Exit 1 = regression — investigate before declaring success.

Model smoke files: `tests/models/_common_smoke.json` (all models) +
`tests/models/<device_model>_smoke.json` (model-specific).

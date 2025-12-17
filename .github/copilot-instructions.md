# ha-stiebel-control AI Assistant Instructions

## Project Overview
ESPHome/Home Assistant integration for monitoring and controlling Stiebel Eltron heat pumps via CAN bus interface using ESP32 with MCP2515 CAN transceiver. Based on the Elster protocol for communication with heat pump components.

## Architecture

### Core Components
- **ESPHome Firmware** (`esphome/`): ESP32 code interfacing with Stiebel Eltron heat pump via CAN bus
  - `common.yaml`: Base configuration with CAN bus setup, SPI config, WiFi/network, buttons, and core text sensors
  - `wpl13e.yaml`: Device-specific sensors, selects, and text sensors for WPL13E heat pump model (template for other models)
  - `ha-stiebel-control.h`: Main C++ logic for CAN communication and Elster protocol handling
  
- **Elster Protocol Library** (`esphome/elster/`): C/C++ implementation of Stiebel Eltron's proprietary protocol
  - `ElsterTable.h`: 3800+ line table mapping signal names to CAN indices and data types
  - `KElsterTable.cpp/.h`: Helper functions for table lookups and value conversions
  - `NUtils.cpp/.h`, `NTypes.h`: Utility functions and type definitions

- **Home Assistant Integration** (`packages/`): Home Assistant configuration packages
  - `ha_stiebel_control.yaml`: Template sensors for computed values (delta T, compressor status) and input helpers for setting temperatures
  
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

### Write Operations
Use `writeSignal()` followed by `id(SENSOR).update()`:
```cpp
writeSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER", value);
readSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER");
```

### Configuration Variables Pattern
ESPHome configs use substitutions from Arduino framework (defined in main device YAML):
- `${can_clk_pin}`, `${can_mosi_pin}`, `${can_miso_pin}`, `${can_cs_pin}` - SPI pins for MCP2515
- `${can_id_pc}` - ESP32's CAN ID (typically 0x680)

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
6# Development Workflow

### Adding New Sensors
1. Locate signal name in `esphome/elster/ElsterTable.h` (search by German name)
2. Add template sensor packages/ha_stiebel_control.yaml` compute derived values
- Use Jinja2 templates for calculations (e.g., Delta T = flow - return temp)
- Input helpers sync to ESPHome via `homeassistant` platform text sensors
- Entity naming: prefix "heatingpump_" auto-added by ESPHome
- Package must be included in HA config: `homeassistant: packages: !include_dir_named packages`t_cent_val` (x.xx), `et_bool`, etc.

### Modifying CAN Configuration
- SPI pins configured in `common.yaml` under `spi:` section
- CAN bus settings: 20kbps bit rate, extended_id: false, platform: mcp2515
- **Never** modify `on_frame` handler - uses universal `processAndUpdate()` pattern

### Home Assistant Integration
- Template sensors in `ha_stiebel_control.yaml` compute derived values
- Use Jinja2 templates for calculations (e.g., Delta T = flow - return temp)
- Input helpers sync to ESPHome via `homeassistant` platform text sensors
- Entity naming: prefix "heatingpump_" auto-added by ESPHome

## Important Constraints

### ESPHome Specifications
- Platform: ESP32 (esp32dev board)
- Framework: Arduino
- CAN transceiver: MCP2515 via SPI
- Must include all 7 Elster library files in `includes:` section

### Home Assistant Specifications  
- Use `state_class: measurement` for numeric sensors
- Device classes: `temperature`, `energy`, `duration`, `running`
- Jinja2 templates: check for `unknown`/`unavailable`/`none` states
- Icon format: `mdi:icon-name`
packages/ha_stiebel_control.yaml](packages/ha_stiebel_control.yaml)
- Manuals: [manuals/](manuals/) - PDF reference documentation
### Elster Protocol Rules
- Read operations: 7-byte CAN frames with generated read IDs
- Write operations: require exact value format per signal type
- Index resolution: single byte (0x00-0xFF) or double byte (0xFA prefix for 0x0000-0xFFFF)
- Vallways** consult PDF manuals in `manuals/` folder for signal meanings and system behavior
- **Archive folder**: Contains old code versions - reference only, not part of active codebase
- **Substitutions**: Use `${}` syntax for device-specific GPIO pins and CAN IDs (defined in main device YAML)
## Key Files Reference
- CAN member definitions: [esphome/ha-stiebel-control.h](esphome/ha-stiebel-control.h#L30-L51)
- Signal table: [esphome/elster/ElsterTable.h](esphome/elster/ElsterTable.h)
- Base config: [esphome/common.yaml](esphome/common.yaml)
- Device sensors: [esphome/wpl13e.yaml](esphome/wpl13e.yaml)
- HA templates: [esphome/ha_stiebel_control.yaml](esphome/ha_stiebel_control.yaml)

## Common Gotchas
- **Don't** use `GetElsterIndex()` - use string names directly: `readSignal(&CanMembers[cm_manager], "SIGNAL_NAME")`
- **Don't** return sensor values in lambda - return `{}` and let C++ callback handle publishing
- **Always** check signal exists in ElsterTable.h before adding sensors
- **Archive folder**: Contains old code versions - reference only, not part of active codebase
- **Substitutions**: Currently not used but prepared via `${}` syntax for future device variants

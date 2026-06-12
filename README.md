# ha-stiebel-control

ESPHome/Home Assistant integration for monitoring and controlling Stiebel Eltron heat pumps via CAN bus interface. Features automatic MQTT discovery for 3800+ signals, writable controls, and intelligent sensor management.

Based on the Elster protocol implementation by [Jürg Müller](http://juerg5524.ch/list_data.php) and community work by [roberreiters](https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053).

## Features

- **Automatic MQTT Discovery**: All 3800+ Elster protocol signals automatically discovered in Home Assistant
- **Frequency-Based Polling**: Intelligent signal request scheduling (1min/10min/30min intervals)
- **Writable Controls**: Direct CAN bus write support for temperature setpoints and operating modes
- **SG Ready Support**: Smart Grid Ready integration for PV surplus utilization ([Documentation](SG_READY.md))
- **Calculated Sensors**: Automatic computation of COP, Delta-T, compressor status, and betriebsart
- **Minimal Configuration**: Only 22 essential sensors defined, rest auto-discovered
- **Button-Based Datetime Control**: Set heat pump time/date via Home Assistant helpers
- **Multi-Device Architecture**: Separate CAN member devices (Manager, Kessel, Heizmodul)

## Architecture

### Core Components

```
esphome/
├── heatingpump.yaml              # Main device config (board selection, substitutions, includes)
└── ha-stiebel-control/
    ├── board_esp32s3.yaml        # ESP32-S3 board config (16MB flash, OPI PSRAM)
    ├── board_esp32s2.yaml        # ESP32-S2/generic ESP32 board config
    ├── can_esp32.yaml            # Built-in TWAI CAN config (256 RX/64 TX buffers)
    ├── can_mcp2515.yaml          # MCP2515 SPI CAN config (legacy)
    ├── common.yaml               # Board-agnostic: WiFi, MQTT, API, OTA, buttons
    ├── wpl13e.yaml               # Device-specific sensors (template for other models)
    ├── ha-stiebel-control.h      # Main C++ logic: MQTT discovery, CAN write, calculators
    ├── signal_requests_wpl13e.h  # Polling schedule for signals
    ├── config.h                  # Blacklist configuration
    └── elster/                   # Elster protocol library
        ├── ElsterTable.h         # 3800+ signal definitions with metadata
        ├── KElsterTable.cpp/h    # Table lookup functions
        └── NUtils.cpp/h/NTypes.h # Utility functions

packages/
└── ha_stiebel_control.yaml       # Home Assistant helpers (input_datetime)
```

### Data Flow

```
Heat Pump CAN Bus (Elster Protocol)
         ↓
ESP32-S3 (Built-in TWAI) or ESP32-S2 + MCP2515
         ↓
ESPHome (ha-stiebel-control.h)
    - Reads CAN frames
    - Parses Elster signals
    - Publishes MQTT discovery
    - Publishes state updates
         ↓
MQTT Broker
         ↓
Home Assistant
    - Auto-creates entities
    - Updates states
    - Provides UI controls
```

### CAN Bus Architecture

- **ESP32 acts as**: `PC` (0x680) on CAN bus
- **CAN Members**:
  - `KESSEL` (0x180): Boiler/storage tank
  - `MANAGER` (0x480): Main controller
  - `HEIZMODUL` (0x500): Heat pump module
- **Communication**: Universal frame handler processes all CAN IDs with mask 0
- **Protocol**: Elster index-based read/write operations

## Hardware Requirements

### Supported Boards

This project supports two hardware configurations:

#### **Option 1: ESP32-S3 with Built-in CAN (Recommended)**

**Advantages:**
- ✅ No buffer overflows (256 RX queue vs 2 on MCP2515)
- ✅ Faster processing (no SPI overhead)
- ✅ Better timing accuracy (hardware CAN controller)
- ✅ Single board solution

**Hardware:**
1. **Waveshare ESP32-S3-RS485-CAN Board** (or similar ESP32-S3 with CAN transceiver)
   - **Specifications**: 16MB Flash, OPI PSRAM
   - **Configuration**: Automatically configured for optimal performance
2. **Connections**:
   - CAN-H (TXD2/GPIO15) → Heat pump CAN-H
   - CAN-L (RXD2/GPIO16) → Heat pump CAN-L
   - GND → Heat pump GND
   - Power: USB or 5V/3.3V input

#### **Option 2: ESP32-S2 with MCP2515 (Legacy)**

**Note:** May experience buffer overflows on high-traffic CAN buses.

**Hardware:**
1. **ESP32 Development Board** (ESP32-S2, ESP32 DevKit V1, etc.)
2. **MCP2515 CAN Transceiver Module**
3. **TJA1050/SN65HVD230 CAN Bus Transceiver** (often integrated on MCP2515 board)
4. **Connections**:
   - SPI: CLK (GPIO18), MOSI (GPIO23), MISO (GPIO19), CS (GPIO5)
   - CAN: H and L to heat pump CAN bus
   - Power: 5V or 3.3V depending on module

### Heat Pump Connection

- Connect to the service port on your Stiebel Eltron heat pump
- Typical CAN pinout: Pin 3 (CAN-H), Pin 5 (CAN-L), Pin 7 (GND)
- **Check your heat pump manual** for exact pinout

## Installation

### Path A — Git clone (recommended for contributors and power users)

**1. Clone the repository**

```bash
git clone https://github.com/bullitt186/ha-stiebel-control.git
cd ha-stiebel-control
```

**2. Configure your secrets**

Copy the example secrets file and fill in your credentials:

```bash
cp esphome/secrets.yaml.example esphome/secrets.yaml
# Edit esphome/secrets.yaml with your WiFi, MQTT, and OTA credentials
```

**3. Configure your board and heat pump model**

Edit `esphome/heatingpump.yaml` and set your substitutions:

```yaml
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  device_model: "wpl13e"   # your heat pump model — see Supported Models below
  can_tx_pin: GPIO15        # adjust for your board
  can_rx_pin: GPIO16
  can_id_pc: "0x680"

packages:
  board:   !include ha-stiebel-control/board_esp32s3.yaml   # or board_esp32s2.yaml
  can:     !include ha-stiebel-control/can_esp32.yaml        # or can_mcp2515.yaml
  base:    !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml           # your model yaml
```

For ESP32-S2 / MCP2515, uncomment the alternative block at the bottom of `heatingpump.yaml`.

**4. Compile and flash**

```bash
make compile          # verify both board variants compile
make upload           # compile + OTA flash (device must be on the network)
```

Or use the ESPHome dashboard in Home Assistant.

---

### Path B — Remote packages (no git clone needed)

Create a minimal `heatingpump.yaml` in your ESPHome config folder referencing this repo
at a pinned release tag:

```yaml
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  device_model: "wpl13e"
  can_tx_pin: GPIO15
  can_rx_pin: GPIO16
  can_id_pc: "0x680"

packages:
  board:   github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/board_esp32s3.yaml@v2.0.0
  can:     github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/can_esp32.yaml@v2.0.0
  base:    github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/common.yaml@v2.0.0
  sensors: github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/wpl13e.yaml@v2.0.0
```

ESPHome downloads and caches the files automatically. To upgrade, change `@v2.0.0` to the new tag.

Create a local `secrets.yaml` in your ESPHome folder with your credentials (see `esphome/secrets.yaml.example`).

---

### Home Assistant Integration

Copy `packages/ha_stiebel_control.yaml` to your HA `/config/packages/` folder and enable packages:

```yaml
# /config/configuration.yaml
homeassistant:
  packages: !include_dir_named packages
```

Restart Home Assistant. Entities appear automatically via MQTT discovery.

---

## Updating

### Git clone users

```bash
git pull
make check    # verify both board variants compile with the new code
make upload   # OTA flash to your device
```

Your `esphome/secrets.yaml` and any local customisations to `heatingpump.yaml` are
**not tracked by git** and will not be overwritten.

### Remote package users

Change the `@vX.Y.Z` tag in your local `heatingpump.yaml` to the new release version and recompile.

---

## Supported Models

| Model file | Heat pump | Status |
|------------|-----------|--------|
| `wpl13e.yaml` | WPL 13 E (air-source, 3 circuits) | ✅ Verified |
| `wpf10.yaml` | WPF 10 / WPF 10M (ground-source) | ⚠️ Community testing — signals may need adjustment |

To add a new model, see [Contributing a new heat pump model](#contributing-a-new-heat-pump-model) below.

---

### Old installation method

Copy the `esphome/` folder contents to your ESPHome config directory:

```
/config/esphome/
├── heatingpump.yaml
└── ha-stiebel-control/
    ├── board_esp32s3.yaml
    ├── board_esp32s2.yaml
    ├── can_esp32.yaml
    ├── can_mcp2515.yaml
    ├── common.yaml
    ├── wpl13e.yaml
    ├── ha-stiebel-control.h
    ├── signal_requests_wpl13e.h
    ├── config.h
    └── elster/
        ├── ElsterTable.h
        ├── KElsterTable.cpp
        ├── KElsterTable.h
        ├── NTypes.h
        ├── NUtils.cpp
        └── NUtils.h
```

### 2. Choose Your Board Type

Edit `heatingpump.yaml` and select your hardware configuration:

**For ESP32-S3 with built-in CAN:**
```yaml
# ESP32-S3 section (uncommented)
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  can_tx_pin: GPIO15  # TXD2 on Waveshare board
  can_rx_pin: GPIO16  # RXD2 on Waveshare board
  can_id_pc: "0x680"

packages:
  board: !include ha-stiebel-control/board_esp32s3.yaml
  can: !include ha-stiebel-control/can_esp32.yaml
  base: !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml
```

**For ESP32-S2 with MCP2515:**
```yaml
# ESP32-S2 section (uncomment this, comment out ESP32-S3)
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  can_clk_pin: GPIO18
  can_mosi_pin: GPIO23
  can_miso_pin: GPIO19
  can_cs_pin: GPIO5
  can_id_pc: "0x680"

packages:
  board: !include ha-stiebel-control/board_esp32s2.yaml
  can: !include ha-stiebel-control/can_mcp2515.yaml
  base: !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml
```

### 3. Configure WiFi and MQTT

Create a `secrets.yaml` file in your ESPHome directory and enter your credentials
```yaml
wifi_ssid: "YOUR_WIFI_SSID"
wifi_password: "YOUR_WIFI_PASSWORD"
mqtt_broker: "YOUR_MQTT_BROKER_IP"
mqtt_username: "YOUR_MQTT_USER"
mqtt_password: "YOUR_MQTT_PASSWORD"
```

### 4. Compile and Upload

```bash
esphome run esphome/heatingpump.yaml
```

Or use the ESPHome dashboard to compile and upload.

### 5. Home Assistant Integration

#### Add Package Configuration

Copy `packages/ha_stiebel_control.yaml` to your Home Assistant `/config/packages/` folder.

Enable packages in `/config/configuration.yaml`:
```yaml
homeassistant:
  packages: !include_dir_named packages
```

Restart Home Assistant.

#### Configure MQTT Integration

Ensure the MQTT integration is set up in Home Assistant:
1. Settings → Devices & Services → Add Integration → MQTT
2. Enter your MQTT broker details
3. MQTT entities will auto-discover

### 6. Verify Operation

1. **Check ESPHome Logs**:
   ```bash
   esphome logs esphome/heatingpump.yaml
   ```
   Look for:
   - `[MQTT_CONN] Connected to MQTT broker`
   - `[processCanMessage] MANAGER (0x480): ...`
   - `[MQTT] Discovery published for ...`

2. **Check Home Assistant**:
   - Go to Settings → Devices & Services → MQTT
   - Look for "Manager", "Kessel", "Heizmodul" devices
   - Entities should appear automatically

3. **Check MQTT Topics** (optional):
   ```bash
   mosquitto_sub -h YOUR_BROKER -u USER -P PASS -t "homeassistant/+/heatingpump/#" -v
   ```

## Usage

### Monitoring

All sensors are automatically discovered and categorized by CAN member:

- **Manager Device**: Control signals, operating mode, status
- **Kessel Device**: Storage tank temperatures, target temps
- **Heizmodul Device**: Heat pump module temperatures, compressor status

Sensors update based on polling frequency (1min, 10min, or 30min intervals).

### Writable Controls

Automatically discovered MQTT entities:

1. **Speicher Soll Temperatur** (Storage Target Temp 1): `number.speicher_soll_temperatur`
2. **Speicher Soll Temperatur 2** (Storage Target Temp 2): `number.speicher_soll_temperatur_2`
3. **Programmschalter** (Operating Mode): `select.programmschalter`
   - Options: Notbetrieb, Bereitschaft, Automatik, Tagbetrieb, Absenkbetrieb, Warmwasser

To change values:
- Use Home Assistant UI
- Call service via automation
- Publish to MQTT command topic

### SG Ready (Smart Grid Ready) PV Integration

**NEW**: Intelligent PV surplus utilization using the SG Ready standard.

Control your heat pump based on solar energy availability with 4 operating states:
- **State 1**: Grid lock - standby mode
- **State 2**: Normal operation  
- **State 3**: PV surplus available - boost DHW heating
- **State 4**: High PV surplus - maximum DHW heating

**Documentation**: See [SG_READY.md](SG_READY.md) for complete guide (quick start, configuration, automations, troubleshooting)

**Features**:
- ✅ ESPHome-native control (fast, reliable)
- ✅ Dropdown select in Home Assistant (`select.manager_sg_ready_zustand`)
- ✅ Compatible with any PV system (E3DC, SolarEdge, Fronius, etc.)
- ✅ Manual override capability for testing
- ✅ Optional temperature boost automations
- ✅ Replaces hardware EVU lock (e.g., Shelly relays)

### Setting Heat Pump Date/Time

1. Go to Home Assistant → Settings → Devices & Services → Helpers
2. Find `input_datetime.heatingpump_time` and `input_datetime.heatingpump_date`
3. Set desired time/date
4. Go to your heat pump device
5. Press "Update Uhrzeit" or "Update Datum" button

### Calculated Sensors

Auto-published to `heatingpump/calculated/` topics:

- **Date**: `YYYY-MM-DD` from JAHR, MONAT, TAG signals
- **Time**: `HH:MM:SS` from STUNDE, MINUTE, SEKUNDE signals
- **Betriebsart**: "Sommer" or "Winter" derived from SOMMERBETRIEB
- **Delta T Continuous**: Flow - Return temperature (always)
- **Delta T Running**: Flow - Return temperature (only when compressor active)
- **Compressor Active**: Binary sensor (on when VERDICHTER > 2)
- **COP WW**: Coefficient of Performance for hot water
- **COP Heizung**: Coefficient of Performance for heating
- **COP Gesamt**: Overall coefficient of performance

## Customization

### Supporting Different Heat Pump Models

To add support for your specific model:

1. Copy `esphome/ha-stiebel-control/wpl13e.yaml` to `esphome/ha-stiebel-control/your_model.yaml`
2. Edit `esphome/heatingpump.yaml`:
   ```yaml
   packages:
     base: !include ha-stiebel-control/common.yaml
     sensors: !include ha-stiebel-control/your_model.yaml
   ```
3. Consult PDF manuals in `manuals/` folder for signal meanings
4. Find signal names in `esphome/ha-stiebel-control/elster/ElsterTable.h`
5. Add template sensors following the pattern:
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

### Adjusting Polling Frequencies

Edit `esphome/ha-stiebel-control/signal_requests_wpl13e.h`:

```cpp
static const SignalRequest signalRequests[] = {
    {"YOUR_SIGNAL", FREQ_1MIN, cm_manager},   // Poll every 1 minute
    {"OTHER_SIGNAL", FREQ_10MIN, cm_kessel},  // Poll every 10 minutes
    {"RARE_SIGNAL", FREQ_30MIN, cm_heizmodul} // Poll every 30 minutes
};
```

### Blacklisting Unwanted Signals

Edit `esphome/ha-stiebel-control/config.h`:

```cpp
static const char* BLACKLISTED_SIGNALS[] = {
    "UNWANTED_SIGNAL",
    "ANOTHER_SIGNAL"
};
```

Or set `isBlacklisted` flag in `ElsterTable.h` entry.

### CAN Member IDs

If your heat pump uses different CAN IDs, edit `esphome/ha-stiebel-control/ha-stiebel-control.h`:

```cpp
static const CanMember CanMembers[] = {
    {"PC",        0x680, {0x00, 0x00}, {0x00, 0x00}, {0xE2, 0x00}},
    {"KESSEL",    0x180, {0x31, 0x00}, {0x30, 0x00}, {0x00, 0x00}},
    {"MANAGER",   0x480, {0x91, 0x00}, {0x90, 0x00}, {0x00, 0x00}},
    {"HEIZMODUL", 0x500, {0xA1, 0x00}, {0xA0, 0x00}, {0x00, 0x00}}
};
```

## Troubleshooting

### No CAN Messages Received

**For MCP2515 (ESP32-S2):**
- Check SPI wiring (CLK, MOSI, MISO, CS pins)
- Verify MCP2515 power (3.3V or 5V depending on module)

**For ESP32-S3 Built-in CAN:**
- Verify GPIO15 (TX) and GPIO16 (RX) connections
- Check CAN transceiver power

**Common to both:**
- Verify CAN-H and CAN-L connections to heat pump
- Check ESPHome logs for `[canbus] Setup CAN...`
- Verify heat pump CAN bus is active (should be always on)
- Check CAN bus termination (120Ω resistors at both ends)

### Entities Not Appearing in Home Assistant

- Verify MQTT broker connection
- Check MQTT discovery topics: `homeassistant/+/heatingpump/#`
- Ensure HA MQTT integration discovery is enabled
- Wait 15 minutes for auto-republish or manually trigger:
  ```bash
  mosquitto_pub -h BROKER -u USER -P PASS -t "heatingpump/republish_discoveries" -m ""
  ```

### Wrong Values or "Unavailable"

- Some signals may not be supported by your heat pump model
- Check signal polling frequency (may need to wait for next poll)
- Verify signal name exists in `ElsterTable.h`
- Check logs for parse errors or invalid values

### Binary Sensor Shows "Unknown"

- Verify `payloadOn` and `payloadOff` are set in ElsterTable.h:
  ```cpp
  { "SIGNAL_NAME", 0x0074, et_bool, "Name", "binary_sensor", "lock", "", "", "mdi:icon", "on", "off", false, true }
  ```

### Datetime Controls Not Working

- Verify `input_datetime` helpers exist in HA
- Check text_sensor entity IDs in `common.yaml` match helper names
- Press buttons after setting helpers (changes don't auto-sync)

## Advanced Configuration

### MQTT Topics Structure

- **Discovery**: `homeassistant/{component}/heatingpump/{unique_id}/config`
- **State**: `heatingpump/{CAN_MEMBER}/{SIGNAL_NAME}/state`
- **Command**: `heatingpump/command/{signal_name}/set`
- **Availability**: `heatingpump/status`

### Understanding Signal Types

From `ElsterTable.h`:
- `et_dec_val`: Decimal value (0.1 precision)
- `et_cent_val`: Centesimal value (0.01 precision)
- `et_mil_val`: Millisimal value (0.001 precision)
- `et_bool`: Boolean (true/false → on/off)
- `et_little_bool`: Inverted boolean
- `et_byte`: Single byte value
- `et_double_val`: Two-byte value
- `et_triple_val`: Three-byte value

### Manual MQTT Control

Write temperature setpoint:
```bash
mosquitto_pub -h BROKER -u USER -P PASS -t "heatingpump/command/speichersolltemp/set" -m "55"
```

Change operating mode:
```bash
mosquitto_pub -h BROKER -u USER -P PASS -t "heatingpump/command/programmschalter/set" -m "Automatik"
```

## Project Structure Details

- **heatingpump.yaml**: Main entry point with board selection (comment/uncomment blocks)
- **board_esp32s3.yaml**: ESP32-S3 configuration (16MB flash, OPI PSRAM, 80MHz)
- **board_esp32s2.yaml**: ESP32-S2/generic ESP32 configuration
- **can_esp32.yaml**: Built-in TWAI CAN controller (256 RX queue, 64 TX queue)
- **can_mcp2515.yaml**: MCP2515 SPI-based CAN controller
- **common.yaml**: Board-agnostic core (WiFi, MQTT, API, OTA, buttons, text sensors)
- **wpl13e.yaml**: Device-specific template sensors (22 essential sensors)
- **ha-stiebel-control.h**: Main C++ implementation (MQTT discovery, calculated sensors, CAN write)
- **signal_requests_wpl13e.h**: Polling schedule (defines which signals to read and how often)
- **config.h**: Blacklist configuration
- **ElsterTable.h**: 3800+ signal definitions with Home Assistant metadata
- **KElsterTable.cpp/h**: Helper functions for table lookups and value conversions

## Contributing

Pull requests are welcome! For major changes, please open an issue first.

### Contributing a new heat pump model

Adding support for a new heat pump model requires two files:

**1. Signal request table** — `esphome/ha-stiebel-control/signal_requests_yourmodel.h`

```cpp
#ifndef SIGNAL_REQUESTS_YOURMODEL_H
#define SIGNAL_REQUESTS_YOURMODEL_H

#include "config.h"
#include "signal_requests_base.h"   // universal signals (date, EVU, COP counters)

const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // always include this first

    // Add your model-specific signals here.
    // Format: {"SIGNAL_NAME", FREQUENCY, CAN_MEMBER}
    // Find signal names in esphome/ha-stiebel-control/elster/ElsterTable.h
    // Use cm_other if you're unsure which CAN member responds.
    {"AUSSENTEMP",           FREQ_30S,   cm_other},   // cm_other queries all members
    {"SPEICHERISTTEMP",      FREQ_30S,   cm_kessel},
    {"RUECKLAUFISTTEMP",     FREQ_30S,   cm_manager},
    // ...
};

const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif
```

**2. Model YAML package** — `esphome/ha-stiebel-control/yourmodel.yaml`

```yaml
esphome:
  includes:
    - ha-stiebel-control/signal_requests_yourmodel.h
# Add sensors here only if they are truly unique to your model.
# Generic sensors (COP, DHW, operating mode) are already in common.yaml.
```

**3. Use in `heatingpump.yaml`**

```yaml
substitutions:
  device_model: "yourmodel"
packages:
  sensors: !include ha-stiebel-control/yourmodel.yaml
```

**4. Test and submit**

```bash
make config    # YAML must parse cleanly
make compile   # must compile without errors
```

Submit a PR with:
- Your `signal_requests_yourmodel.h` and `yourmodel.yaml`
- ESPHome logs showing signals responding from your heat pump
- A note marking any signals as "unverified" if you couldn't confirm them

**Tips for finding signals:**
- Search `ElsterTable.h` by German name (e.g. "AUSSEN" for outside temperature)
- Use `cm_other` first — it queries all three CAN members (KESSEL, MANAGER, HEIZMODUL);
  the responding one will appear automatically in HA
- Signals that don't respond are silently ignored — no harm done

### Other contributions

- Dashboard examples
- Energy monitoring integrations
- Documentation improvements
- Signal metadata enhancements (friendly names, units, device classes in `ElsterTable.h`)

## Credits

- **Jürg Müller**: Original Elster protocol implementation and signal table
- **roberreiters**: Home Assistant community ESPHome CAN bus setup
- **Bastian Stahmer**: Original ha-stiebel-control implementation
- **ESPHome & Home Assistant Communities**: Continuous support and inspiration

## License

[GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html)

## Disclaimer

This project is not affiliated with or endorsed by Stiebel Eltron. Use at your own risk. Modifying your heat pump settings via CAN bus may void your warranty. Always consult your heat pump manual and follow local regulations.

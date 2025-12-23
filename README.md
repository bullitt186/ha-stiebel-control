# ha-stiebel-control

ESPHome/Home Assistant integration for monitoring and controlling Stiebel Eltron heat pumps via CAN bus interface. Features automatic MQTT discovery for 3800+ signals, writable controls, and intelligent sensor management.

Based on the Elster protocol implementation by [Jürg Müller](http://juerg5524.ch/list_data.php) and community work by [roberreiters](https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053).

## Features

- **Automatic MQTT Discovery**: All 3800+ Elster protocol signals automatically discovered in Home Assistant
- **Frequency-Based Polling**: Intelligent signal request scheduling (1min/10min/30min intervals)
- **Writable Controls**: Direct CAN bus write support for temperature setpoints and operating modes
- **Calculated Sensors**: Automatic computation of COP, Delta-T, compressor status, and betriebsart
- **Minimal Configuration**: Only 22 essential sensors defined, rest auto-discovered
- **Button-Based Datetime Control**: Set heat pump time/date via Home Assistant helpers
- **Multi-Device Architecture**: Separate CAN member devices (Manager, Kessel, Heizmodul)

## Architecture

### Core Components

```
esphome/
├── heatingpump.yaml              # Main device config (substitutions, includes)
├── ha-stiebel-control/
│   ├── common.yaml               # Base: CAN bus, MQTT, buttons, text sensors
│   ├── wpl13e.yaml               # Device-specific sensors (template for other models)
│   ├── ha-stiebel-control.h      # Main C++ logic: MQTT discovery, CAN write, calculators
│   ├── signal_requests_wpl13e.h  # Polling schedule for signals
│   ├── config.h                  # Blacklist configuration
│   └── elster/                   # Elster protocol library
│       ├── ElsterTable.h         # 3800+ signal definitions with metadata
│       ├── KElsterTable.cpp/h    # Table lookup functions
│       └── NUtils.cpp/h/NTypes.h # Utility functions

packages/
└── ha_stiebel_control.yaml       # Home Assistant helpers (input_datetime)
```

### Data Flow

```
Heat Pump CAN Bus (Elster Protocol)
         ↓
ESP32 + MCP2515 Transceiver
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

### ESP32 with CAN Interface

1. **ESP32 Development Board** (e.g., ESP32 DevKit V1)
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

### 1. ESPHome Configuration

#### Option A: New Installation

```bash
# In your ESPHome config directory
git clone https://github.com/bullitt186/ha-stiebel-control.git
cd ha-stiebel-control/esphome
```

Edit `heatingpump.yaml` and configure:
```yaml
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  # Adjust GPIO pins to match your hardware
  can_clk_pin: GPIO18
  can_mosi_pin: GPIO23
  can_miso_pin: GPIO19
  can_cs_pin: GPIO5
  can_id_pc: "0x680"
```

Edit `ha-stiebel-control/common.yaml` WiFi credentials:
```yaml
wifi:
  ssid: "YOUR_SSID"
  password: "YOUR_PASSWORD"
```

Configure MQTT broker:
```yaml
mqtt:
  broker: "YOUR_MQTT_BROKER_IP"
  username: "YOUR_MQTT_USERNAME"
  password: "YOUR_MQTT_PASSWORD"
```

#### Option B: Copy Files to Existing ESPHome Installation

Copy the `esphome/` folder contents to your ESPHome config directory:
```
/config/esphome/
├── heatingpump.yaml
└── ha-stiebel-control/
    └── ...
```

### 2. Compile and Upload

```bash
esphome run esphome/heatingpump.yaml
```

Or use the ESPHome dashboard to compile and upload.

### 3. Home Assistant Integration

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

### 4. Verify Operation

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

- Check SPI wiring (CLK, MOSI, MISO, CS pins)
- Verify CAN-H and CAN-L connections to heat pump
- Check ESPHome logs for `[canbus] Setup CAN...`
- Verify heat pump CAN bus is active (should be always on)

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

- **common.yaml**: Core ESPHome config, WiFi, MQTT, API, CAN bus, buttons, text sensors
- **wpl13e.yaml**: Device-specific template sensors (22 essential sensors)
- **ha-stiebel-control.h**: Main C++ implementation (MQTT discovery, calculated sensors, CAN write)
- **signal_requests_wpl13e.h**: Polling schedule (defines which signals to read and how often)
- **config.h**: Blacklist configuration
- **ElsterTable.h**: 3800+ signal definitions with Home Assistant metadata
- **KElsterTable.cpp/h**: Helper functions for table lookups and value conversions

## Contributing

Pull requests are welcome! For major changes, please open an issue first to discuss what you would like to change.

Areas for contribution:
- Support for additional heat pump models
- Dashboard examples
- Energy monitoring integrations
- Documentation improvements
- Signal metadata enhancements

## Credits

- **Jürg Müller**: Original Elster protocol implementation and signal table
- **roberreiters**: Home Assistant community ESPHome CAN bus setup
- **Bastian Stahmer**: Original ha-stiebel-control implementation
- **ESPHome & Home Assistant Communities**: Continuous support and inspiration

## License

[GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html)

## Disclaimer

This project is not affiliated with or endorsed by Stiebel Eltron. Use at your own risk. Modifying your heat pump settings via CAN bus may void your warranty. Always consult your heat pump manual and follow local regulations.

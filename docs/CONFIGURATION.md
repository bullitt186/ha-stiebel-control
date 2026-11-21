# Configuration Reference

---

## `secrets.yaml` Keys

All keys are required unless marked optional.

| Key | Description | Example |
|-----|-------------|---------|
| `wifi_ssid` | Wi-Fi network name | `"MyNetwork"` |
| `wifi_password` | Wi-Fi password | `"MyPassword"` |
| `mqtt_broker` | MQTT broker IP or hostname | `192.168.1.10` |
| `mqtt_username` | MQTT username | `"mqtt_user"` |
| `mqtt_password` | MQTT password | `"secret"` |
| `api_encryption_key` | 32-byte base64 key for ESPHome native API | `"abc...=="` |
| `ota_password` | OTA update password | `"change_me"` |

Generate `api_encryption_key`:
```bash
python3 -c "import base64, os; print(base64.b64encode(os.urandom(32)).decode())"
```

The key must match what HA's ESPHome integration has stored for this device. If you change it, you need to re-adopt the device in HA (Settings → Devices & Services → ESPHome → delete entry, then re-add).

---

## `heatingpump.yaml` Substitutions

| Substitution | Description | Default |
|--------------|-------------|---------|
| `device_name` | ESPHome node name and hostname; root of every MQTT topic (`<device_name>/…`), node ID of the discovery topics and prefix of the default entity IDs. Lower case `a-z`, `0-9`, `-`, at most 24 characters. Topics and entity IDs in this documentation use the default | `heatingpump` |
| `friendly_name` | Name of the ESPHome device in HA (native API: restart, reset reason, …). The MQTT device of the heat pump keeps its own name | `"Stiebel Eltron Wärmepumpe"` |
| `device_model` | Heat pump model — selects signal table and HA device identifier | `"wpl13e"` |
| `language` | Display language for HA entity names: `DE` (German, default) or `EN` (English) | `"DE"` |
| `can_tx_pin` | CAN TX GPIO (ESP32-S3 built-in TWAI) | board-specific — see note below |
| `can_rx_pin` | CAN RX GPIO (ESP32-S3 built-in TWAI) | board-specific — see note below |
| `can_id_pc` | CAN bus ID for the ESP32 (PC role) | `"0x680"` |
| `can_clk_pin` | SPI CLK (MCP2515 only) | board-specific — see note below |
| `can_mosi_pin` | SPI MOSI (MCP2515 only) | board-specific — see note below |
| `can_miso_pin` | SPI MISO (MCP2515 only) | board-specific — see note below |
| `can_cs_pin` | SPI CS (MCP2515 only) | board-specific — see note below |
| `can_bit_rate` | CAN bus speed: `20kbps` (CAN-A: WPL13E/WPF10/WPC7) or `50kbps` (CAN-B: WPL17/WPL23/WPL10AC/WPLA7) | `20kbps` |
| `disable_sg_ready` | `"true"` hides all SG Ready entities (models without SG Ready input) | `"false"` |
| `read_only` | `"true"`: never write to the heat pump — no setpoint/mode/climate entities, clock buttons hidden; reading and bus scan keep working | `"false"` |
| `network_package` | Network package from `ha-stiebel-control/`: `network_wifi.yaml` (WiFi + captive portal) or `network_ethernet_w5500.yaml` (W5500 Ethernet) | `"network_wifi.yaml"` |
| `eth_clk_pin` … `eth_rst_pin` | W5500 SPI pins (`eth_clk_pin`, `eth_mosi_pin`, `eth_miso_pin`, `eth_cs_pin`, `eth_int_pin`, `eth_rst_pin`), only with `network_ethernet_w5500.yaml` | Waveshare ESP32-S3-POE-ETH: GPIO13/11/12/14/10/9 |

> **Pin assignments are board-specific.** Always verify against your board's schematic
> before wiring. Known working values for common boards:
>
> **Waveshare ESP32-S3-RS485-CAN** (recommended board):
> `can_tx_pin: GPIO15`, `can_rx_pin: GPIO16`
>
> **Generic ESP32 DevKit + MCP2515 module:**
> `can_clk_pin: GPIO18`, `can_mosi_pin: GPIO23`, `can_miso_pin: GPIO19`, `can_cs_pin: GPIO5`
>
> These are the values shipped in `heatingpump.yaml`. Other boards may use different pins.

---

## Board files and framework

| `board:` package | Chip / board | Flash | Framework |
|------------------|--------------|-------|-----------|
| `board_esp32s3.yaml` | ESP32-S3 (`esp32-s3-devkitc-1`, e.g. Waveshare ESP32-S3-RS485-CAN), octal PSRAM @ 80 MHz | 16 MB | ESP-IDF |
| `board_esp32.yaml` | classic ESP32 (`esp32dev`, e.g. ESP32-WROOM DevKit) + MCP2515 | 4 MB | ESP-IDF |
| `board_esp32s2.yaml` | **deprecated alias** of `board_esp32.yaml` — it never configured an ESP32-S2 | 4 MB | ESP-IDF |

Since 2.5.0 both boards build with **ESP-IDF** instead of Arduino (ESPHome's recommended
framework; smaller firmware, nothing in this project needs Arduino).

### Updating an existing device from an Arduino build (≤ 2.4.x)

The first update after 2.5.0 switches the framework. A normal OTA update works:

- **Partition table:** OTA only replaces the app; the partition table on the device stays.
  ESPHome generates the same app slots for both frameworks (16 MB: `app0`/`app1` 0x7C0000
  each; 4 MB: 0x1C0000 each). Only the data partitions behind them differ (Arduino:
  `eeprom`, `spiffs`, `nvs` 0x60000; ESP-IDF: `nvs` 0x70000). The new firmware finds the
  existing `nvs` partition by name, so the old layout keeps working.
- **Stored values survive:** ESPHome keeps preferences (SG Ready boost values, energy base,
  summer-mode damping, restored states) in the NVS namespace `esphome` with the same keys on
  both frameworks.
- **Do not** run `esphome upload --partition-table` to "clean up" the old layout: it moves the
  NVS partition, and ESPHome documents that the data stored there is lost.
- **Bootloader:** OTA does not replace it. With a bootloader that supports rollback (built with
  `enable_ota_rollback`, ESPHome's default), a new firmware that crashes before it is marked
  valid is rolled back to the previous slot automatically. `safe_mode` (enabled in `common.yaml`)
  boots into an OTA-only mode after repeated failed boots.
- Clean the build files once after switching (ESPHome dashboard: *Clean Build Files*, CLI:
  `esphome clean heatingpump.yaml`) so no Arduino objects are reused.

**If the device does not come back** after the update (no API/MQTT after a few minutes, no
safe-mode OTA), flash it over USB: `esphome run heatingpump.yaml --device /dev/ttyACM0`
(or ESPHome Web / `esptool.py write_flash`). A USB flash writes bootloader, partition table and
app in the ESP-IDF layout; values stored in NVS can be lost because the NVS partition moves,
so note your SG Ready boost values first. Stop every program that holds the serial port before
flashing.

---

## Optional packages

### Network: WiFi or W5500 Ethernet

`common.yaml` includes the network package named by the substitution `network_package`.
The default `network_wifi.yaml` holds WiFi, the captive portal and the WiFi sensors, so
existing configurations need no change. For a board with a W5500 Ethernet module (e.g.
Waveshare ESP32-S3-POE-ETH) select the Ethernet package in the top-level substitutions:

```yaml
substitutions:
  network_package: network_ethernet_w5500.yaml
  # eth_cs_pin: GPIO14        # only if your wiring differs from the defaults
```

DHCP is used by default. WiFi, the fallback hotspot and the WiFi sensors are then not built;
`no_captive_portal.yaml` is not needed.

### `no_captive_portal.yaml` — no captive portal and no fallback hotspot

`common.yaml` keeps `captive_portal:` and the fallback hotspot (`wifi: ap:`) enabled by default,
so existing configurations behave as before. To drop both (saves flash, mainly interesting on
the 4 MB classic ESP32), list the package **after** `base`:

```yaml
packages:
  board: !include ha-stiebel-control/board_esp32.yaml
  can: !include ha-stiebel-control/can_mcp2515.yaml
  base: !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml
  no_captive_portal: !include ha-stiebel-control/no_captive_portal.yaml
```

Without it, Wi-Fi is configured only through `secrets.yaml`: if the credentials are wrong or
the network changes, the ESP stays offline and needs a USB flash. `api:` stays (the date/time
sync uses Home Assistant text sensors).

---

## Entity IDs

Every discovery config carries a language-independent `default_entity_id`:
`<domain>.<device_name>_<unique ID without "stiebel_">` (a `-` in the device name becomes `_`), e.g. `sensor.heatingpump_kessel_aussentemp`,
`number.heatingpump_manager_raumsolltemp_i`, `sensor.heatingpump_calculated_cop_gesamt`. DE and EN
builds therefore create the same IDs, and renaming a display name does not change them. The IDs in
this document and in `dashboards/` are these IDs (CI checks the dashboards with
`tests/check_dashboard_ids.py`).

The dashboards assume the default `device_name: heatingpump`; with another name, replace the
`heatingpump_` prefix in the dashboard YAML.

Two heat pumps on one broker need two device names: their topics and discovery configs are then
separate. The unique IDs (`stiebel_<member>_<signal>`) and the HA device identifiers are still the
same for both, so Home Assistant only shows the entities of the first one.

Installs from before 2.8 keep the IDs HA gave their entities then (derived from the display
name). To use the shipped dashboards there, rename the entity IDs under Settings → Entities, or
adjust the IDs in the dashboard.

## Writable Controls

These entities appear automatically in HA via MQTT discovery:

### Temperature Setpoints (number entities)

| Entity ID | Description | Range | Step |
|-----------|-------------|-------|------|
| `number.heatingpump_manager_einstell_speichersolltemp` | DHW target temperature | 20–60°C | 1°C |
| `number.heatingpump_manager_einstell_speichersolltemp2` | DHW eco target temperature | 20–60°C | 1°C |
| `number.heatingpump_manager_raumsolltemp_i` | Room setpoint circuit 1 | 10–30°C | 0.5°C |
| `number.heatingpump_manager_raumsolltemp_ii` | Room setpoint circuit 2 | 10–30°C | 0.5°C |
| `number.heatingpump_manager_raumsolltemp_iii` | Room setpoint circuit 3 | 10–30°C | 0.5°C |
| `number.heatingpump_manager_raumsolltemp_nacht` | Room setpoint night/reduced | 10–30°C | 0.5°C |
| `number.heatingpump_manager_sg_ready_boost_state3` | SG Ready DHW boost for state 3 | 0–10°C | 0.5°C |
| `number.heatingpump_manager_sg_ready_boost_state4` | SG Ready DHW boost for state 4 | 0–15°C | 0.5°C |
| `number.heatingpump_manager_sg_ready_room_boost_state3` | SG Ready room setpoint boost (heating circuit 1) for state 3, default 1 K, 0 = off | 0–5 K | 0.5 K |
| `number.heatingpump_manager_sg_ready_room_boost_state4` | SG Ready room setpoint boost (heating circuit 1) for state 4, default 2 K, 0 = off | 0–5 K | 0.5 K |

**Range from the device:** the ranges above are defaults and bound the result. About a minute after the request
manager starts, the firmware asks the heat pump for the limits of each writable number (Elster
switch bytes `0xF8` = minimum, `0xF9` = maximum), at least 2 s apart. The effective range is the
overlap of the device range and the default range: lower bound = max(device, default), upper bound
= min(device, default). The defaults are deliberate limits (e.g. standstill time >= 20 min), so the
device can only narrow them (e.g. heating curve 0.0 -> 0.2), never widen them. An empty overlap uses
the default range. The raw device range is written to the log. Once both answers are in, the number
(and the matching climate card) is published again with the effective range. A side the device does
not provide (`0x8000`) keeps its default on that side; `0x8000` on both sides, a range with minimum
≥ maximum, or an empty overlap keeps the default range for good. No answer after one retry: the
default range applies for now, and the request is repeated after 1 hour until an answer comes. On
success, discovery is sent again with the same unique_id. The limits are checked per row (name and
member): heating circuit 2 (`MISCHERMODUL_3`) has its own limits, separate from heating circuit 1.
Once the device range is known, every numeric write of the number is clamped to the effective range,
also from automations or plain MQTT; without a known device range writes are not changed. The limits
are read from the member the number's state comes from (model routing applies). The SG Ready boost
numbers are not on the bus and keep their range.

The room boost raises `RAUMSOLLTEMP_I` and with it the heating curve, which can add compressor
starts. With 0 the room setpoint is not written at all, neither on entering nor on leaving the
SG Ready state. All four boost values are stored on the ESP and survive a reboot.

### WPMW II Parameters (WPL13E and TTL 33, number entities)

Display parameters of the WPMW II manager, written to the MANAGER over CAN. The register
indices were found by bus scans and checked by changing each value on the display of a WPL13E
and of a TTL 33; other models do not get these entities. Ranges follow the WPMW II
commissioning list. The values change the heat pump's control behaviour: change one at a time
and watch starts, run times and room temperatures.

The parameter numbers below are those of the WPL13E manager. Other software versions number
them differently (a TTL 33 with software 6518 shows Sommerbetrieb as 8, Bivalenzpunkt as 25/31,
Reglerdynamik as 40, Stillstandzeit as 41), so go by the title on the display.

| Entity ID | WPMW II parameter | Register | Range | Step |
|-----------|-------------------|----------|-------|------|
| `number.heatingpump_manager_schaltwerkdynamikzeit_set` | 42 Reglerdynamik (switching distance between compressor stages and the 2nd heat generator) | 0xfdb0 | 1–30 | 1 |
| `number.heatingpump_manager_bivalentparalleltemperatur_hzg_set` | 27 bivalence point heating (2nd heat generator in parallel below) | 0x01ac | −20–30°C | 0.5°C |
| `number.heatingpump_manager_bivalentparalleltemperatur_ww_set` | 33 bivalence point hot water | 0x01ad | −20–30°C | 0.5°C |
| `number.heatingpump_manager_stillstandzeit_set` | 43 minimum compressor off time (the manual forbids less than 20 min in normal operation) | 0xfdb1 | 20–120 min | 1 min |
| `number.heatingpump_manager_sw_aussentemp_set` | 10 summer mode, AUSSENTEMP threshold | 0x01bf | 10–30°C | 0.5°C |

Parameters 10 and 43 were confirmed by changing them on the display and reading the bus. On
the WPMW II, `SOMMERBETRIEB_TEMP` (0xfdb5) and `SPERRZEIT` (0x0026) did not follow the display.

#### Measuring points per bus member (WPMW II)

The same Elster signal is a different sensor on the manager side (KESSEL, WPM terminals) and
in the heat pump module (HEIZMODUL, IWS). On WPMW II models (`wpl13e`, `wpl18e`, `ttl33`) these
entities get their own names (#46). Existing installs keep their entity IDs; only the shown
name changes.

| Signal | Member | Measuring point | Name (DE) |
|---|---|---|---|
| `RUECKLAUFISTTEMP` 0x0016 | KESSEL | control sensor heating circuit 1, terminal X2/3 (buffer) | Heizkreis 1 Regelfühler |
| `RUECKLAUFISTTEMP` 0x0016 | HEIZMODUL | heat pump return inside the unit | Wärmepumpe Rücklauf IWS |
| `WPVORLAUFIST` 0x01d6 | KESSEL | heat pump flow sensor, terminal X2/2 | Wärmepumpe Vorlauf Fühler WPM |
| `WPVORLAUFIST` 0x01d6 | HEIZMODUL | heat pump flow inside the unit | Wärmepumpe Vorlauf IWS |
| `VORLAUFISTTEMP` 0x000f | KESSEL | mixer flow heating circuit 2, terminal X2/7 | Heizkreis 2 Mischer Vorlauf |
| `KESSELSOLLTEMP` 0x0002 | KESSEL | return setpoint heating circuit 1 | Heizkreis 1 Rücklauf Soll |
| `AUSSENTEMP` 0x000c | HEIZMODUL | air intake at the unit (not the control outdoor sensor) | Luftansaugtemperatur |

#### More WPMW II settings (display title → register)

Found on a TTL 33 by changing one value on the display and comparing bus scans (#68). The
integration has no entities for most of them yet.

| Display | Member | Index | Signal |
|---|---|---|---|
| WW-Hysterese | MANAGER | 0x0022 | `HYSTERESEZEIT` |
| Rücklauf-Max | MANAGER | 0x0028 | `MAX_TEMP_KESSEL` |
| Mischer-Max (HK2) | MISCHERMODUL_3 | 0x0028 | `MAX_TEMP_KESSEL` |
| Dynamik-Misch (HK2, 60–240 = 6–24 K) | MISCHERMODUL_3 | 0x002b | `TN` |
| Einsatzgrenze Heizung / WW | MANAGER | 0x01ae / 0x01af | `BIVALENZALTERNATIVTEMPERATUR_HZG` / `_WW` |
| Frostschutz | MANAGER | 0x0a00 | `AUSSEN_FROSTTEMP` |
| Dauerlauf-Pumpe | MANAGER | 0xfdaf | `DAUERLAUF_PUFFERLADEPUMPE` |
| Pumpenzyklen | MANAGER | 0xfdb2 | `PUMPENZYKLEN` |
| Gebäudebauart | MANAGER | 0xfdb3 | `GEBAEUDEART` |
| Sommerbetrieb EIN/AUS | MANAGER | 0xfdb4 | `SOMMERBETRIEB` |
| WW-Automatik | MANAGER | 0xfdb9 | `AUTOMATIK_WARMWASSER` |

A second heating circuit (HK2) is handled by MISCHERMODUL_3 (0x602): its room targets
(`RAUMSOLLTEMP_I` / `_NACHT`), heating curve and heating program answer and accept writes
there. The integration's writable room target and heating curve go to the MANAGER, which is
heating circuit 1. Abtauzeit-Min was not found on the bus.

### Compressor Runtime Counters (WPL13E, sensor entities)

| Entity ID | Description | Unit |
|-----------|-------------|------|
| `sensor.heatingpump_heizmodul_lz_verd_1_heizbetrieb` | Compressor 1 heating runtime | h |
| `sensor.heatingpump_heizmodul_lz_verd_2_heizbetrieb` | Compressor 2 heating runtime | h |
| `sensor.heatingpump_heizmodul_lz_verd_1_2_heizbetrieb` | Compressor 1+2 heating runtime | h |
| `sensor.heatingpump_heizmodul_lz_verd_1_kuehlbetrieb` | Compressor 1 cooling runtime | h |
| `sensor.heatingpump_heizmodul_lz_verd_2_kuehlbetrieb` | Compressor 2 cooling runtime | h |
| `sensor.heatingpump_heizmodul_lz_verd_1_2_kuehlbetrieb` | Compressor 1+2 cooling runtime | h |

### CAN Bus Diagnostics (ESP32-S3 only, sensor entities)

Available automatically on ESP32-S3 / TWAI builds. Not present on MCP2515 builds.

| Entity ID | Description |
|-----------|-------------|
| `sensor.heatingpump_calculated_can_tec` | TX Error Counter (TEC) — rises on failed transmissions |
| `sensor.heatingpump_calculated_can_rec` | RX Error Counter (REC) — rises on receive errors |
| `sensor.heatingpump_calculated_can_bus_errors` | Total bus error count (cumulative) |
| `sensor.heatingpump_calculated_can_state` | Bus state: Läuft / Gestoppt / Bus-Off / Erholung |
| `sensor.heatingpump_calculated_can_rx_missed` | Frames lost because the RX queue was full (`rx_missed_count`, cumulative) |
| `sensor.heatingpump_calculated_can_rx_overrun` | Frames lost by a hardware RX FIFO overrun (`rx_overrun_count`, cumulative) |
| `sensor.heatingpump_calculated_can_rx_queue` | Frames waiting in the RX queue at the moment of the sample (`msgs_to_rx`) |

TEC/REC above 96 indicates warning level; above 127 the controller enters bus-off.
Rising "RX verloren" or a queue that does not drain to 0 means the main loop does not keep up
with the bus traffic. The three RX sensors are disabled by default (enable them in HA).

### MQTT Diagnostics (all builds, diagnostic category)

| Entity ID | Description |
|-----------|-------------|
| `sensor.heatingpump_calculated_mqtt_publish_failures` | Publishes the MQTT client refused since boot (not connected, or the `idf_send_async` queue was full and the message was dropped). Disabled by default. |

### ESP Diagnostics (native API, diagnostic category)

| Name | Description |
|------|-------------|
| Reset Grund | Reason of the last restart (e.g. brownout, task watchdog, software reset) |
| Betriebszeit | Uptime |
| Freier Heap | Free heap in bytes (every 60 s) |
| Größter freier Block | Largest free heap block in bytes; a falling value with stable free heap means fragmentation |
| Loop-Zeit | Longest main loop iteration in the last 60 s (ms) |

### Logging

The MQTT log topic `heatingpump/debug` only carries warnings and errors. The API/serial log
(`esphome logs heatingpump.yaml`) shows INFO. Per-frame CAN logs (`processCanMessage()`,
`readSignal()`) are DEBUG: set `logger: level: DEBUG` in your top-level YAML to see them.

### Mode Selects (select entities)

| Entity ID | Options |
|-----------|---------|
| `select.heatingpump_manager_programmschalter` | Notbetrieb, Bereitschaft, Automatik, Tagbetrieb, Absenkbetrieb, Warmwasser |
| `select.heatingpump_manager_sg_ready_state` | 1 - EVU Sperre, 2 - Normal, 3 - Empfohlen, 4 - Zwang |
| `select.heatingpump_manager_sg_ready_normal_mode` | Tagbetrieb (default), Automatik: mode written in SG Ready state 2, see SG_READY.md |

### DHW Time Program (WPMW II, text entities)

On WPL13E, WPL18E and TTL 33 the DHW program of the manager (`W_WASSERPROG_1_<day>` with
`_SCHALT_2` and `_SCHALT_3`, three periods per day) is read every 10 minutes and shown as one entity
per day, e.g. `text.heatingpump_ww_programm_mo`:

- Format `HH:MM-HH:MM`, up to three periods separated by commas: `06:30-09:00,11:00-17:00`.
  `-` = no period on that day.
- Writable on WPL13E and TTL 33 (tested on the display). Times must be on a 15-minute step,
  start before end, periods must not overlap. Invalid input writes nothing and the entity shows
  the program of the heat pump again. Only changed periods are written.
- WPL18E shows the program read-only (sensor), `read_only: "true"` too.
- The program applies only in `Automatik`: in `Tagbetrieb` the manager charges to the DHW setpoint
  all day. Outside the program it holds the DHW setpoint 2.
- The heating program of heating circuit 1 (`HEIZPROG_1_<day>`, manager) is shown the same way,
  e.g. `text.heatingpump_heiz_programm_mo` (WPL13E, WPL18E). `00:00-24:00`
  keeps the day setpoint around the clock in `Automatik`.
- The heating program of heating circuit 2 (mixer circuit) follows the same format, e.g.
  `text.heatingpump_heiz_programm_hk2_mo`. The WPMW II keeps it as `HEIZPROG_1_<day>`
  on the virtual member `MISCHERMODUL_3` (0x602), not as `HEIZPROG_2` on the manager. Without a period
  the circuit runs on its night setpoint in `Automatik`.

### Heating circuit 2 (WPMW II mixer circuit, number entities)

The WPMW II keeps the settings of heating circuit 2 on the virtual bus member `MISCHERMODUL_3`
(0x602): display "Raumtemp. 2" Tag/Absenk and "Heizkurve 2". `RAUMSOLLTEMP_II` on the manager is
**not** circuit 2 (a change there did not move the mixer flow setpoint). Writable on WPL13E and TTL 33.

| Entity | Signal | Range |
|---|---|---|
| Raumsoll HK2 Tag Einstellung | `RAUMSOLLTEMP_I` @ 0x602 | 10–30 °C, 0.5 K |
| Raumsoll HK2 Nacht Einstellung | `RAUMSOLLTEMP_NACHT` @ 0x602 | 10–30 °C, 0.5 K |
| Heizkurve HK2 Einstellung | `HEIZKURVE` @ 0x602 | 0.1–3.0 |

---

## Read-Only Calculated Sensors

Published to `heatingpump/calculated/` MQTT topics and auto-discovered in HA:

| Sensor | Topic | Description |
|--------|-------|-------------|
| Date | `heatingpump/calculated/date/state` | `YYYY-MM-DD` from heat pump clock |
| Time | `heatingpump/calculated/time/state` | `HH:MM:SS` from heat pump clock |
| Delta T (continuous) | `heatingpump/calculated/delta_t_continuous/state` | Heat pump flow − return (IWS sensors B1/B2); meaningful only while the pump circulates |
| Delta T (running) | `heatingpump/calculated/delta_t_running/state` | Same, only when compressor active |
| Control deviation | `heatingpump/calculated/regelabweichung/state` | KESSEL return sensor (control sensor HK1) − return setpoint |
| Compressor active | `heatingpump/calculated/compressor_active/state` | on/off |
| Defrost running | `heatingpump/calculated/abtauung_laeuft/state` | on/off; on while the heat pump module's last defrost duration (`ZEITDAUER_LETZTE_ABTAUUNG`) reads 0, i.e. a defrost is running |
| Compressor starts today | `heatingpump/calculated/verdichter_starts_heute/state` | Starts since the heat pump's midnight; an off phase under 2 min does not count as a new start |
| Compressor runtime today | `heatingpump/calculated/verdichter_laufzeit_heute/state` | Minutes the compressor ran today (30 s resolution) |
| Runtime per start today | `heatingpump/calculated/verdichter_laufzeit_mittel/state` | Runtime today ÷ starts today, in minutes |

The compressor counters use the heat pump's date for the day change and are held on the ESP
only: after an ESP reboot they start again from 0 (MQTT reconnects do not affect them).
| COP hot water | `heatingpump/calculated/cop_ww/state` | Coefficient of performance (DHW) |
| COP heating | `heatingpump/calculated/cop_heiz/state` | Coefficient of performance (heating) |
| COP total | `heatingpump/calculated/cop_gesamt/state` | Overall coefficient of performance |

---

## MQTT Topic Structure

All raw CAN signal values:
```
heatingpump/{CAN_MEMBER}/{SIGNAL_NAME}/state     ← read
heatingpump/{CAN_MEMBER}/{SIGNAL_NAME}/set       ← write (writable signals only)
```

Examples:
```
heatingpump/MANAGER/PROGRAMMSCHALTER/state       → "Tagbetrieb"
heatingpump/MANAGER/EINSTELL_SPEICHERSOLLTEMP/set ← "55"
heatingpump/KESSEL/SPEICHERISTTEMP/state         → "51.8"
```

CAN members: `MANAGER` (0x480), `KESSEL` (0x180), `HEIZMODUL` (0x500)

Availability:
```
heatingpump/status    → "online" or "offline"
```

### Publish on change, heartbeat

All `…/state` topics of raw signals and calculated sensors are retained and published **when the
value changes**. An unchanged value is sent again at most every 10 minutes (heartbeat), so the
topic shows the device is alive. The interval is `STATE_HEARTBEAT_MS` in
`esphome/ha-stiebel-control/config.h`:

```cpp
#define STATE_HEARTBEAT_MS (10UL * 60UL * 1000UL)   // milliseconds
```

- Measured temperatures that jitter can be held back with an optional deadband. Set for example
  `#define STATE_DEADBAND_TENTHS 3` (0.3 K, default 0 = off) in `config.h`. It applies to
  `AUSSENTEMP`, `SPEICHERISTTEMP`, `RUECKLAUFISTTEMP`, `VORLAUFISTTEMP` and `WPVORLAUFIST`: a smaller
  change than this against the last published value waits for the heartbeat, which sends the newest
  value.
- A signal is still **polled** at its request frequency; only the MQTT message is skipped.
  Home Assistant keeps the last value, its history shows a new point only on a change (or the
  heartbeat). `last_updated` therefore no longer moves with every poll.
- After every MQTT (re)connect all known values are published once (a few per 250 ms, after the
  discovery configs). A broker restart without persistence is covered that way.
- SG Ready writes and DHW program writes go through a queue: one write at a time, checked against
  the read-back and sent once more if needed (#85).
- After a write (setpoint, mode, SG Ready, clock) the read-back of that signal is published even
  if the heat pump kept the old value, so HA always gets an answer to a command.
- Not affected (always sent): SG Ready state and boost values, bus scan results, the fault list
  sensor and the fault remedy attribute, discovery configs.

---

## Polling Frequencies

Edit `esphome/ha-stiebel-control/signal_requests_wpl13e.h` (or your model's file):

```cpp
const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE           // universal signals, fixed frequencies

    {"AUSSENTEMP",   FREQ_30S,  cm_kessel},    // every 30 seconds
    {"HEIZKURVE",    FREQ_10MIN, cm_manager},   // every 10 minutes
};
```

Available frequency constants:

| Constant | Value |
|----------|-------|
| `FREQ_10S` | 10 seconds |
| `FREQ_30S` | 30 seconds |
| `FREQ_1MIN` | 60 seconds |
| `FREQ_5MIN` | 5 minutes |
| `FREQ_10MIN` | 10 minutes |
| `FREQ_30MIN` | 30 minutes |
| `FREQ_60MIN` | 60 minutes |

---

## Blacklisting Signals

To permanently suppress a signal (prevent it from appearing in HA), set `isBlacklisted` (4th field)
to `true` in its `ElsterTable[]` row in `ElsterTable.h`:

```cpp
//  Name              Index   Type        isBlacklisted  hasMetadata
{ "NOISY_SIGNAL"    , 0x0123, et_default, true         , false },
```

Blacklisted signals are skipped before any processing — they never generate MQTT messages
or HA entities.

---

## Thermostat (climate) entities

Two MQTT climate entities complement the number/select entities (which stay unchanged):

| Entity | Target temperature | Current temperature | Presets |
|--------|--------------------|---------------------|---------|
| `climate.…_heizung` | `RAUMSOLLTEMP_I` (day room setpoint) | — | operating modes of `PROGRAMMSCHALTER` |
| `climate.…_warmwasser` | `EINSTELL_SPEICHERSOLLTEMP` | `SPEICHERISTTEMP` | — |

They use the same `/set` topics as the number entities, so the CAN member mapping per model applies.

## Fault text

`FEHLERMELDUNG` is decoded via `ErrorList` in `ElsterTable.h` ("Keine Stoerung" when 0). On the
WPMW II (WPL13E, WPL18E, TTL 33) the fault number is in the high byte (`0x1D00` = HD-SENSOR-MAX);
the low byte is not part of it. The `abhilfe` attribute holds a short remedy
(`heatingpump/<MEMBER>/FEHLERMELDUNG/attributes`). WPL13E requests the heat pump module (IWS);
the WPMW II manager answers 69 (`0x0045`, no fault) on this index, so it is not requested.

## Bus scan

Asks one CAN member for every index of the signal table (about 3,600, ~9 min at 150 ms per
request) or for a hex range, and publishes every index that answers with a real value.
Normal polling pauses during the scan.

| Topic | Payload |
|-------|---------|
| `heatingpump/scan/start` | `HEIZMODUL` (table) or `MANAGER 0xfd00 0xfdff` (range) |
| `heatingpump/scan/stop` | anything |
| `heatingpump/scan/status` | `{"state":"running|done|stopped","member":…,"done":…,"total":…,"hits":…,"seconds":…}` |
| `heatingpump/scan/<MEMBER>/0x<idx>` | `{"name":…,"raw":…,"value":…}` (retained) |

The diagnostic buttons "Bus-Scan Manager/Heizmodul/Kessel" (disabled by default) start a table
scan. Turn the results into a request block and a Markdown table:

```bash
mosquitto_sub -h BROKER -u USER -P PASS -t 'heatingpump/scan/#' -v -W 5 > scan.txt
python3 helperscripts/scan_report.py scan.txt
```

## Polling back-off

A signal answering `-255` (not available on this model) creates no HA entity until it has
delivered a real value. From the third consecutive `-255` its interval doubles per answer,
up to 6 h; a real value resets it. See `docs/ARCHITECTURE.md`.

## Dashboards

`dashboards/heatingpump_standard.yaml` is a complete dashboard (overview, energy, diagnostics)
built from standard cards; `dashboards/heatingpump_schema.yaml` is a picture-elements card
with a system schema — copy `dashboards/www/heatingpump/` to `/config/www/heatingpump/` first.
Both use the entity IDs of a fresh install (see [Entity IDs](#entity-ids)); older installs keep
their original IDs and need to rename them in HA or adjust the dashboard.

### Heating curve and week program cards

`dashboards/www/stiebel-cards.js` adds two cards without further dependencies. Copy it to
`/config/www/stiebel-cards.js` and add the resource `/local/stiebel-cards.js` (type JavaScript
module, Settings → Dashboards → ⋮ → Resources); raise a `?v=2` suffix after an update.

- `custom:stiebel-heating-curve-card` draws the heating curve over the outside temperature, like
  the diagram in the WPMW II manual, with the night curve, grey comparison curves and the current
  operating point. − and + show a preview of another slope; *Übernehmen* writes it.
  The model `T = R + S · 20 · ((20 − Ta) / 20)^0.63` reproduces the manual's diagram and the
  display points (+20/0/−20 °C) of a WPL13E; the ring marks the setpoint the controller reports.
- `custom:stiebel-week-program-card` shows a time program (seven text entities, Monday first) as
  week bars; the pencil opens the text fields, *Mo auf Di–Fr* copies Monday.

Both cards draw only with Home Assistant theme variables (text, divider, primary and card colors,
font and spacing tokens), so themes and dark mode style them like the built-in cards. `color`
takes the values of the tile card: a theme color name (`orange`, `blue`, `primary`, …), a CSS
variable or a hex value; default is the heating color of the theme. Texts follow the user's
language (German or English), numbers the user's number format.

```yaml
- type: custom:stiebel-heating-curve-card
  title: Heizkurve HK1
  y_label: Rücklaufsoll
  slope: number.heatingpump_manager_heizkurve_set
  room: climate.heatingpump_climate_heizung          # or a number entity
  room_night: number.heatingpump_manager_raumsolltemp_nacht
  outdoor: sensor.heatingpump_kessel_aussentemp
  setpoint: sensor.heatingpump_kessel_kesselsolltemp  # optional marker
- type: custom:stiebel-week-program-card
  title: Heizprogramm HK1
  color: orange                                       # optional, theme color name or hex
  entities:
    - text.heatingpump_heiz_programm_mo
    # … one per day, Monday to Sunday
```

## Summer mode (WPL13E)

The WPMW II switches heating off when its *summer automatic* is on (`SOMMERBETRIEB`, parameter
10) and the damped outside temperature reaches the threshold (`SW_AUSSENTEMP`, 0x01bf,
display "AUSSENTEMP"); it switches back 1 K below. The damping averages the controller's outside sensor
over 24/48/72 h depending on `GEBAEUDEART` (1/2/3). The firmware reproduces this as
`binary_sensor.…_sommerbetrieb_aktiv` and publishes the damped value as
`sensor.…_aussentemperatur_gedampft`. The damped value is an exponential moving average kept
in NVS; on the very first boot it starts from the current temperature and needs about a day to
settle. Compare it with the display ("Sommerbetrieb" with the flap closed).

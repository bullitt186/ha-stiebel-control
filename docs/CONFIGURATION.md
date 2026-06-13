# Configuration Reference

---

## `secrets.yaml` Keys

All keys are required unless marked optional.

| Key | Description | Example |
|-----|-------------|---------|
| `wifi_ssid` | Wi-Fi network name | `"MyNetwork"` |
| `wifi_password` | Wi-Fi password | `"MyPassword"` |
| `mqtt_broker` | MQTT broker IP or hostname | `192.168.1.10` |
| `mqtt_username` | MQTT username | `"iotuser"` |
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
| `device_name` | ESPHome device hostname | `heatingpump` |
| `friendly_name` | Display name in HA | `"Stiebel Eltron Wärmepumpe"` |
| `device_model` | Heat pump model — selects signal table and HA device identifier | `"wpl13e"` |
| `can_tx_pin` | CAN TX GPIO (ESP32-S3 built-in TWAI) | board-specific — see note below |
| `can_rx_pin` | CAN RX GPIO (ESP32-S3 built-in TWAI) | board-specific — see note below |
| `can_id_pc` | CAN bus ID for the ESP32 (PC role) | `"0x680"` |
| `can_clk_pin` | SPI CLK (MCP2515 only) | board-specific — see note below |
| `can_mosi_pin` | SPI MOSI (MCP2515 only) | board-specific — see note below |
| `can_miso_pin` | SPI MISO (MCP2515 only) | board-specific — see note below |
| `can_cs_pin` | SPI CS (MCP2515 only) | board-specific — see note below |

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

## Writable Controls

These entities appear automatically in HA via MQTT discovery:

### Temperature Setpoints (number entities)

| Entity ID | Description | Range | Step |
|-----------|-------------|-------|------|
| `number.manager_speicher_soll_temperatur_einstellung` | DHW target temperature | 20–60°C | 1°C |
| `number.manager_speicher_soll_temperatur_2_einstellung` | DHW eco target temperature | 20–60°C | 1°C |
| `number.manager_raum_soll_temperatur_i_einstellung` | Room setpoint circuit 1 | 10–30°C | 0.5°C |
| `number.manager_sg_ready_boost_zustand_3` | SG Ready DHW boost for state 3 | 0–10°C | 0.5°C |
| `number.manager_sg_ready_boost_zustand_4` | SG Ready DHW boost for state 4 | 0–15°C | 0.5°C |

### Mode Selects (select entities)

| Entity ID | Options |
|-----------|---------|
| `select.manager_programmschalter` | Notbetrieb, Bereitschaft, Automatik, Tagbetrieb, Absenkbetrieb, Warmwasser |
| `select.manager_sg_ready_zustand` | 1 - EVU Sperre, 2 - Normal, 3 - Empfohlen, 4 - Zwang |

---

## Read-Only Calculated Sensors

Published to `heatingpump/calculated/` MQTT topics and auto-discovered in HA:

| Sensor | Topic | Description |
|--------|-------|-------------|
| Date | `heatingpump/calculated/date/state` | `YYYY-MM-DD` from heat pump clock |
| Time | `heatingpump/calculated/time/state` | `HH:MM:SS` from heat pump clock |
| Betriebsart | `heatingpump/calculated/betriebsart/state` | "Sommerbetrieb" or "Normalbetrieb" |
| Delta T (continuous) | `heatingpump/calculated/delta_t_continuous/state` | Flow − return temp, always |
| Delta T (running) | `heatingpump/calculated/delta_t_running/state` | Flow − return temp, only when compressor active |
| Compressor active | `heatingpump/calculated/compressor_active/state` | on/off |
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

To permanently suppress a signal (prevent it from appearing in HA), set `isBlacklisted: true`
in its `ElsterTable.h` entry:

```cpp
{ "NOISY_SIGNAL", 0x0123, et_default, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  true,   // ← isBlacklisted
  false }
```

Blacklisted signals are skipped before any processing — they never generate MQTT messages
or HA entities.

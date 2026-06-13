# Installation Guide

Two installation paths are supported. Choose the one that fits your workflow.

---

## Path A — Git Clone (Recommended)

Best for: contributors, users who want full control, or those who customise signal tables.

### 1. Clone the repository

```bash
git clone https://github.com/bullitt186/ha-stiebel-control.git
cd ha-stiebel-control
```

### 2. Create `secrets.yaml`

Copy the example and fill in your values:

```bash
cp esphome/secrets.yaml.example esphome/secrets.yaml
```

Edit `esphome/secrets.yaml`:

```yaml
wifi_ssid: "YourNetwork"
wifi_password: "YourPassword"

mqtt_broker: 192.168.1.10        # IP of your MQTT broker
mqtt_username: "your_mqtt_user"
mqtt_password: "your_mqtt_password"

# Generate with: python3 -c "import base64,os; print(base64.b64encode(os.urandom(32)).decode())"
# Must match the key stored in HA's ESPHome integration for this device.
api_encryption_key: "your-32-byte-base64-key=="

ota_password: "change_me"
```

> **Important:** `secrets.yaml` is gitignored. Never commit it.

### 3. Configure `heatingpump.yaml`

Open `esphome/heatingpump.yaml`. The ESP32-S3 block is active by default. Adjust to match your hardware:

```yaml
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  device_model: "wpl13e"          # your heat pump model — see Supported Models in README
  can_tx_pin: GPIOxx               # ⚠️ BOARD-SPECIFIC — check your board's schematic
  can_rx_pin: GPIOxx               # ⚠️ BOARD-SPECIFIC — check your board's schematic
  can_id_pc: "0x680"

packages:
  board:   !include ha-stiebel-control/board_esp32s3.yaml
  can:     !include ha-stiebel-control/can_esp32.yaml
  base:    !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml
```

> **Important — pin numbers are board-specific.** The file already contains correct
> values for the Waveshare ESP32-S3-RS485-CAN board. If you use a different board,
> look up the CAN transceiver connections in your board's schematic and change
> `can_tx_pin` / `can_rx_pin` accordingly before flashing.

For ESP32-S2 + MCP2515, comment out the S3 block and uncomment the S2 block at the bottom of the file. The SPI pin defaults are also board-specific — verify before use.

### 4. Compile and flash

First flash (USB cable required):
```bash
cd esphome
esphome run heatingpump.yaml
```

Subsequent updates over the air:
```bash
make upload   # compiles and OTA-flashes to device at configured IP
```

### 5. Verify

Watch the device come online:
```bash
make logs
```

Look for:
```
[MQTT_CONN] Connected to MQTT broker
[MQTT_CONN] Discovery publication complete
```

Then run the regression test:
```bash
make smoke-test
```

---

## Path B — Remote Packages (ESPHome Addon, No Git)

Best for: users who only want to use the integration and prefer managing everything inside the ESPHome addon.

Create a file called `heatingpump.yaml` in your ESPHome config folder (usually `/config/esphome/`) with this content:

```yaml
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  device_model: "wpl13e"
  can_tx_pin: GPIOxx   # ⚠️ replace with your board's CAN TX pin
  can_rx_pin: GPIOxx   # ⚠️ replace with your board's CAN RX pin
  can_id_pc: "0x680"

packages:
  board:   github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/board_esp32s3.yaml@v2.0.0
  can:     github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/can_esp32.yaml@v2.0.0
  base:    github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/common.yaml@v2.0.0
  sensors: github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/wpl13e.yaml@v2.0.0
```

ESPHome will download and cache the package files automatically on first compile.

Create a `secrets.yaml` in the same folder (see section 2 above for required keys).

To upgrade to a new release, change `@v2.0.0` to the new tag and recompile.

---

## Home Assistant Integration

### MQTT

Ensure the MQTT integration is configured in HA:
1. Settings → Devices & Services → Add Integration → MQTT
2. Enter your broker IP, username, and password
3. Enable MQTT discovery

Entities from the heat pump will appear automatically — no manual configuration needed.

### HA Helper Package (optional)

The `packages/ha_stiebel_control.yaml` file adds `input_datetime` helpers used to set the
heat pump's date and time from HA.

Copy it to your HA config:
```bash
cp packages/ha_stiebel_control.yaml /config/packages/
```

Enable packages in `/config/configuration.yaml`:
```yaml
homeassistant:
  packages: !include_dir_named packages
```

Restart Home Assistant.

---

## Verifying Operation

After the device is online, check:

1. **ESPHome logs** — `make logs` or the ESPHome addon log view
2. **HA MQTT devices** — Settings → Devices & Services → MQTT → should show "Manager", "Kessel", "Heizmodul" devices
3. **Regression test** — `make smoke-test` (git clone users)

See [CONFIGURATION.md](CONFIGURATION.md) for entity IDs, MQTT topics, and customisation options.

# Installation Guide

Two installation paths are supported. Choose the one that fits your workflow.

**Requirements:** ESPHome 2026.9.0 or newer (`esphome version`; the config declares
`min_version: 2026.9.0` and older versions refuse it). CI builds with ESPHome 2026.10.1.

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
ota_password: "your-existing-ota-password"  # Keep through the stepping-stone release
api_encryption_key: "your-32-byte-base64-key=="  # Also used by encrypted OTA
```

> **Important:** `secrets.yaml` is gitignored. Never commit it.

### 3. Configure `heatingpump.yaml`

Open `esphome/heatingpump.yaml`. The ESP32-S3 block is active by default. Adjust to match your hardware:

```yaml
substitutions:
  device_name: heatingpump
  friendly_name: "Stiebel Eltron Wärmepumpe"
  device_model: "wpl13e"          # your heat pump model — see Supported Models in README
  language: "DE"                  # display language for HA entity names: DE (default) or EN
  can_tx_pin: YOUR_CAN_TX          # board-specific — see pin note below
  can_rx_pin: YOUR_CAN_RX          # board-specific — see pin note below
  can_id_pc: "0x680"

packages:
  board:   !include ha-stiebel-control/board_esp32s3.yaml
  can:     !include ha-stiebel-control/can_esp32.yaml
  base:    !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/wpl13e.yaml
```

> **Language:** German is the default. Change `language: "DE"` to `language: "EN"` if you prefer English entity names in Home Assistant. The language is baked into the firmware at compile time — recompile and re-flash after changing it.

> **Pin numbers are board-specific — always verify against your board's schematic.**
> The `heatingpump.yaml` shipped in this repo already has correct values for the
> **Waveshare ESP32-S3-RS485-CAN** board: `can_tx_pin: GPIO15`, `can_rx_pin: GPIO16`.
> For any other board, update these values before flashing.

For a classic ESP32 (`esp32dev`) + MCP2515, comment out the S3 block and uncomment the ESP32 block at the bottom
of the file (`board_esp32.yaml`; the old name `board_esp32s2.yaml` still works). The SPI pins (`can_clk_pin`, `can_mosi_pin`, `can_miso_pin`, `can_cs_pin`) are
also board-specific. The repo ships with values typical for a generic ESP32 DevKit with a
standard MCP2515 breakout module (CLK: GPIO18, MOSI: GPIO23, MISO: GPIO19, CS: GPIO5) —
verify these against your specific board before flashing.

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
  language: "DE"            # DE (default) or EN
  can_tx_pin: YOUR_CAN_TX   # board-specific: Waveshare ESP32-S3-RS485-CAN → GPIO15
  can_rx_pin: YOUR_CAN_RX   # board-specific: Waveshare ESP32-S3-RS485-CAN → GPIO16
  can_id_pc: "0x680"

packages:
  board:   github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/board_esp32s3.yaml@v2.6.0
  can:     github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/can_esp32.yaml@v2.6.0
  base:    github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/common.yaml@v2.6.0
  sensors: github://bullitt186/ha-stiebel-control/esphome/ha-stiebel-control/wpl13e.yaml@v2.6.0
```

ESPHome will download and cache the package files automatically on first compile.

Create a `secrets.yaml` in the same folder (see section 2 above for required keys).

To upgrade to a new release, change `@v2.6.0` to the new tag and recompile.

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

## OTA Encryption Migration

OTA migration is delivered in two releases. This first stage keeps password
authentication and requires ESPHome 2026.9.0 or newer. A later release will
require encrypted OTA. Keep the existing API encryption key and OTA password
for installed devices; do not generate replacements during this migration.

### Stage 1: install a password-compatible stepping-stone

Choose the first published release whose CHANGELOG lists **OTA migration,
stage 1**. The release tag is assigned when that release is published; this
guide does not depend on a contributor's fork or a review branch.

For git-clone installations, fetch upstream release tags (`git fetch --tags
origin`), check out that stage-1 release and build/install it with ESPHome
**2026.9.x–2027.2.x**. For remote packages, pin every package reference to that
same release tag before building. Both paths keep this configuration:

```yaml
api:
  encryption:
    key: !secret api_encryption_key

ota:
  platform: esphome
  password: !secret ota_password
```

After installation, confirm the device log says
`Encryption: offered, plaintext accepted`. A device already showing that
message has completed stage 1. The password authenticates uploads in this
stage; it does not itself encrypt firmware in transit.

### Stage 2: require encryption in the later release

Only after stage 1 has been installed, update to the later release whose
CHANGELOG explicitly requires encrypted OTA. It will replace `password:` with
`encryption: {}` while keeping the same `api_encryption_key`. Install it and
confirm `Encryption: required`; only then remove `ota_password` from the local
secrets file. Until that release, the shared package ships stage 1.

ESPHome **2027.3.0 removes the plaintext upload fallback**. Complete stage 1
before upgrading the build tool to that version. Do not skip directly from
older device firmware to an encryption-required package. Removing the password
too early can fail with `ESP requests password, but no password given!`;
requiring encryption too early refuses a device that cannot yet offer it.

An installation already requiring encrypted OTA need not return to stage 1.
For recovery alternatives, consult the official migration guide instead of
leaving a plaintext exception permanently enabled.

References: [ESPHome OTA encryption and migration](https://esphome.io/components/ota/esphome/#encryption),
[native API encryption key](https://esphome.io/components/api/#configuration-variables).

## Verifying Operation

After the device is online, check:

1. **ESPHome logs** — `make logs` or the ESPHome addon log view
2. **HA MQTT devices** — Settings → Devices & Services → MQTT → should show "Manager", "Kessel", "Heizmodul" devices
3. **Regression test** — `make smoke-test` (git clone users)

See [CONFIGURATION.md](CONFIGURATION.md) for entity IDs, MQTT topics, and customisation options.

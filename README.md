# ha-stiebel-control

ESPHome firmware for monitoring and controlling Stiebel Eltron heat pumps via CAN bus.
Connects an ESP32 to the heat pump's internal Elster CAN bus and automatically publishes
3800+ signals to Home Assistant via MQTT discovery — no manual entity configuration needed.

Based on the Elster protocol by [Jürg Müller](http://juerg5524.ch/list_data.php) and
community work by [roberreiters](https://community.home-assistant.io/t/configured-my-esphome-with-mcp2515-can-bus-for-stiebel-eltron-heating-pump/366053).

---

## Table of Contents

- [Quick Start](#quick-start)
- [Supported Hardware](#supported-hardware)
- [Supported Models](#supported-models)
- [Installation](#installation)
- [Updating](#updating)
- [Features](#features)
- [SG Ready — PV Integration](#sg-ready--pv-integration)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Credits & License](#credits--license)

---

## Quick Start

```bash
git clone https://github.com/bullitt186/ha-stiebel-control.git
cd ha-stiebel-control
cp esphome/secrets.yaml.example esphome/secrets.yaml
# fill in your WiFi, MQTT, and API credentials in secrets.yaml
esphome run esphome/heatingpump.yaml
```

HA entities appear automatically via MQTT discovery within 60 seconds of the device coming online.

For full step-by-step instructions (including remote package installation without git) see [docs/INSTALLATION.md](docs/INSTALLATION.md).

---

## Supported Hardware

### Boards

| Option | Board | CAN interface | Recommended |
|--------|-------|---------------|-------------|
| ESP32-S3 | Waveshare ESP32-S3-RS485-CAN (or similar) | Built-in TWAI (GPIO15 TX, GPIO16 RX) | ✅ Yes |
| ESP32-S2 / ESP32 | Any ESP32 dev board | MCP2515 via SPI | Legacy |

The ESP32-S3 built-in CAN controller has a 256-entry receive queue vs. 2 on MCP2515 —
this prevents buffer overflows on the heat pump's busy CAN bus.

### Heat Pump Connection

Connect to the service port on your heat pump:

| Pin | Signal |
|-----|--------|
| 3 | CAN-H |
| 5 | CAN-L |
| 7 | GND |

Exact pinout varies — consult your heat pump manual (`manuals/` folder for supported models).

---

## Supported Models

| Model file | Heat pump | Status |
|------------|-----------|--------|
| `wpl13e.yaml` | WPL 13 E (air-source, 3 heating circuits) | ✅ Verified |
| `wpf10.yaml` | WPF 10 / WPF 10M (ground-source) | ⚠️ Community — signals may need adjustment |

To add support for your heat pump, see [Contributing](#contributing) below.

---

## Installation

Two paths are supported:

**Path A — Git clone** (recommended for contributors and advanced users):
full control, `make` targets, local signal customisation.

**Path B — Remote packages** (ESPHome addon users):
no git required, minimal local config file, automatic package download from GitHub.

Both paths are documented step-by-step in [docs/INSTALLATION.md](docs/INSTALLATION.md),
including secrets configuration, board selection, and Home Assistant setup.

---

## Updating

### Git clone

```bash
git pull
make check    # compile both board variants
make upload   # OTA flash to device
make smoke-test  # verify all required signals still appear
```

Your `esphome/secrets.yaml` and local changes to `heatingpump.yaml` are gitignored —
they will not be overwritten.

### Remote packages

Change the `@vX.Y.Z` tag in your local `heatingpump.yaml` to the new release and recompile.

---

## Features

- **Automatic MQTT discovery** — 3800+ Elster signals appear in HA without manual configuration
- **Multi-model architecture** — swap heat pump models by changing one line in `heatingpump.yaml`
- **SG Ready** — 4-state PV surplus control with automatic temperature boost ([docs](SG_READY.md))
- **Writable controls** — temperature setpoints and operating mode via HA entities or MQTT
- **Calculated sensors** — COP (hot water / heating / total), Delta-T, compressor status
- **Date/time sync** — set heat pump clock from HA via buttons
- **Frequency-based polling** — configurable per-signal intervals (10s–60min)
- **Native test suite** — 138 Catch2 unit tests covering core logic, no hardware needed
- **MQTT regression tests** — `make smoke-test` verifies required signals after each OTA flash

For entity IDs, MQTT topics, and configuration options see [docs/CONFIGURATION.md](docs/CONFIGURATION.md).
For architecture and internals see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## SG Ready — PV Integration

Automatically adjusts heat pump operation based on PV surplus:

| State | Trigger | Action |
|-------|---------|--------|
| 1 — EVU Sperre | Grid peak / no PV | Standby (Bereitschaft) |
| 2 — Normal | Standard | Comfort mode (Tagbetrieb) |
| 3 — Empfohlen | 1–2 kW surplus | Tagbetrieb + DHW boost + room +1°C |
| 4 — Zwang | >2 kW surplus | Tagbetrieb + larger DHW boost + room +2°C |

Works with any PV inverter that exposes an SG Ready state (E3DC, SolarEdge, Fronius, etc.).
No extra hardware required — replaces hardware EVU lock relays.

See [SG_READY.md](SG_READY.md) for full documentation, dashboard examples, and automation templates.

---

## Troubleshooting

### No CAN messages received

- **ESP32-S3**: verify GPIO15 (TX) and GPIO16 (RX) are connected to the CAN transceiver
- **MCP2515**: check SPI wiring (CLK GPIO18, MOSI GPIO23, MISO GPIO19, CS GPIO5)
- Check CAN-H/CAN-L polarity — swap if no messages appear
- Verify CAN bus termination (120Ω at each end of the bus)
- Check `make logs` for `[canbus] Setup CAN...`

### Entities not appearing in Home Assistant

- Confirm MQTT broker is connected: `heatingpump/status` should be `online`
- Enable MQTT discovery in HA: Settings → Devices & Services → MQTT → configure
- Wait up to 30 seconds after boot for discovery messages
- Trigger manual republish: publish an empty message to `heatingpump/republish_discoveries`

### Native API entities unavailable (WiFi signal, SG Ready sensors)

These entities use the ESPHome native API (not MQTT). If they show `unavailable`:
1. Verify `api_encryption_key` in `secrets.yaml` matches what HA has stored for this device
2. Reload the ESPHome config entry: Settings → Devices & Services → ESPHome → reload

### Wrong values or "Unknown"

- Some signals are not supported by all heat pump models — check your model's signal table
- Binary sensors showing "Unknown": add `payloadOn`/`payloadOff` to the signal's entry in `ElsterTable.h`
- Check `heatingpump/debug` MQTT topic for parse errors

---

## Contributing

### Adding a new heat pump model

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for the complete step-by-step guide.

In brief:
1. Create `signal_requests_yourmodel.h` using `SIGNAL_REQUESTS_BASE` + your model's signals
2. Create `yourmodel.yaml` (3 lines — just the `esphome: includes:` block)
3. Add a dispatcher entry in `signal_requests_model.h`
4. Create `tests/models/yourmodel_smoke.json` with verified required topics
5. Submit a PR with logs showing signals responding on your hardware

### Agentic development (Claude Code / Copilot)

This repo has a fully configured agentic harness. See [docs/AGENTIC_DEVELOPMENT.md](docs/AGENTIC_DEVELOPMENT.md) for setup and workflows.

### Other contributions

- Signal metadata (friendly names, units, icons) in `ElsterTable.h`
- Dashboard examples
- Documentation improvements

---

## Credits & License

- **Jürg Müller** — original Elster protocol implementation and signal table
- **roberreiters** — ESPHome CAN bus setup for Stiebel Eltron
- **Bastian Stahmer** — ha-stiebel-control implementation
- **Community contributors** — model support, testing, documentation

[GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html)

> This project is not affiliated with or endorsed by Stiebel Eltron GmbH & Co. KG.
> Modifying heat pump settings via CAN bus may void your warranty. Always consult your
> heat pump manual and follow local regulations. Use at your own risk.

# Changelog

All notable changes are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions: [SemVer](https://semver.org/).

---

## [2.0.0] — 2026-06-13

### Architecture

- **Multi-model architecture**: Signal request tables split into a universal base
  (`signal_requests_base.h`) and model-specific files. Adding a new heat pump model no
  longer requires editing shared code — only a new `.h` + `.yaml` file pair.
- **`device_model` substitution**: Controls the HA device identifier
  (`stiebel_eltron_<model>`). Existing WPL13E users are unaffected.
- **Generic sensors moved to `common.yaml`**: COP, energy accumulators, DHW temperature
  sensors, and operating mode sensors are now shared across all models.
  Model YAML files are now ~10 lines.

### SG Ready Refactor

- **`SgReadyController`**: Pure C++ class extracted from `ha-stiebel-control.h` with no
  ESPHome dependencies. All side effects (CAN writes, MQTT publishes, NVS persistence)
  injected via `ISgReadyIO` interface — fully unit-testable on the host.
- **Automatic temperature boost**: States 3 and 4 now apply DHW and room temperature
  boosts directly in firmware (no extra HA automations needed). Boost values configurable
  via `number.manager_sg_ready_boost_zustand_3` / `_4`.
- **NVS persistence**: Boost baseline temperatures survive device reboots.
- **Fixed `write→delay→read` ordering** in CAN signal writes.

### Testing

- **Native Catch2 test suite**: 138 test cases, 236 assertions covering `SgReadyController`,
  CAN protocol helpers (`generate_read_id`, `lookupCanMember`), Elster table functions
  (`SetValueType`, `TranslateString`, `GetElsterIndex`), and NUtils.
- Tests compile and run on the host with `make test` — no hardware or ESPHome toolchain needed.
- **MQTT smoke test harness** (`tests/test_mqtt_smoke.py`): post-OTA regression check.
  Subscribes for 120 seconds and verifies all required signals appear. Model-specific
  required topics in `tests/models/<model>_smoke.json`. Run with `make smoke-test`.

### New Models

- **WPF10 / WPF10M** (`wpf10.yaml`): community-contributed ground-source heat pump model
  based on PR #13 and issue #20. Signals marked unverified pending hardware validation.

### Release Infrastructure

- CI release job on `v*` tags: compiles both firmware variants, generates `manifest.json`,
  creates GitHub Release with firmware artifacts.
- `CHANGELOG.md`, `secrets.yaml.example`.

### Agentic Dev Setup

- `CLAUDE.md` importing `.github/copilot-instructions.md` — single source of truth for
  Claude Code and GitHub Copilot.
- `.claude/settings.json` — pre-approved `esphome`/`make`/`git` permissions.
- `Makefile` — `compile`, `compile-s2`, `check`, `test`, `smoke-test`, `upload`, `capture-baseline`.
- `.github/workflows/ci.yml` — compile + test + YAML lint on every push/PR.
- Issue templates (bug report, feature request).

### Documentation

- Full documentation split across `docs/`: INSTALLATION, CONFIGURATION, ARCHITECTURE,
  CONTRIBUTING, AGENTIC_DEVELOPMENT.
- README rewritten as a lean entry point with TOC.
- `SG_READY.md` translated to English and updated for current firmware behaviour.

### Bug Fixes

- **API encryption key**: Restored `encryption: key: !secret api_encryption_key` in
  `api:` block to maintain HA native API connection across OTA updates.

---

## [1.x] — 2023-08–2025-12

Major milestones in the 1.x series (see git log for full history):

### ESP32-S3 + Built-in CAN (late 2025)

- Added ESP32-S3 support with built-in TWAI CAN controller (256 RX queue vs 2 on MCP2515).
- Modular package architecture: `board_esp32s3.yaml`, `can_esp32.yaml`, `common.yaml`,
  `wpl13e.yaml` as separate composable packages.
- CAN buffer optimisation (256 RX / 64 TX entries).

### MQTT Auto-Discovery & Custom Discovery (2024–2025)

- Replaced ESPHome native MQTT discovery with custom C++ discovery publishing full HA
  metadata (device class, unit, state class, icon) per signal.
- Writable controls: MQTT number/select entities for temperature setpoints and operating mode.
- Calculated sensors: COP, Delta-T, compressor status, date/time, betriebsart.
- Frequency-based signal polling with round-robin scheduling and jitter.
- Signal blacklisting via `isBlacklisted` flag in `ElsterTable.h`.
- Per-signal HA metadata in `ElsterTable.h` (friendly names, units, device classes, icons).

### SG Ready v1 (late 2025)

- Initial SG Ready implementation with 4-state control.
- Boost amount configurable via HA number entities.
- NVS persistence for boost baseline temperatures.

### Elster Protocol Library (2023)

- Initial port of Jürg Müller's Elster protocol implementation to ESPHome.
- `ElsterTable.h` with 3800+ signal definitions.
- MCP2515 SPI CAN transceiver support.
- Initial WPL13E signal table.

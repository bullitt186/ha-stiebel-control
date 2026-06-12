# Changelog

All notable changes to ha-stiebel-control are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [2.0.0] — 2026-06-13

### Architecture (breaking for contributors, transparent for users)

- **Multi-model architecture**: Signal request tables are now split into a universal
  base (`signal_requests_base.h`) and thin model-specific files. Adding a new heat pump
  model no longer requires editing shared code — only a new `.h` + `.yaml` file.
- **Device identifier is now configurable**: `device_model` substitution controls the
  HA device ID (`stiebel_eltron_<model>`). Existing WPL13E users are unaffected.
- **Generic sensors moved to `common.yaml`**: COP, energy, DHW, and operating mode
  sensors are now shared across all models. Model files are thin.

### Added

- `signal_requests_base.h` — universal Elster protocol signals (date/time, EVU lock,
  operating mode, energy counters for COP)
- `signal_requests_model.h` — dispatcher header for model selection
- `signal_requests_wpf10.h` + `wpf10.yaml` — community WPF10/WPF10M ground-source model
  (based on PR #13 and issue #20; signals marked unverified need community validation)
- `device_model` substitution in `heatingpump.yaml`
- `project:` versioning in `common.yaml` — version visible in HA ESPHome dashboard
- CI release pipeline — GitHub Release with compiled firmware artifacts on `v*` tags
- `CHANGELOG.md` — this file
- `secrets.yaml.example` — safe template for new users

### Changed

- `wpl13e.yaml` reduced to a single `esphome: includes:` declaration
- `common.yaml` now contains all universal sensors (COP, DHW, operating mode)

### Fixed

- HA device identifier now uses `device_model` substitution instead of hardcoded
  `wpl13e` string — WPF10 users will correctly see their model name in HA

---

## [1.x] — prior work

See git log for prior history. The 1.x series covers the initial Elster/CAN
implementation, ESP32-S3 support, SG Ready integration, and agentic dev tooling.

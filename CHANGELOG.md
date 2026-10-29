# Changelog

All notable changes are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions: [SemVer](https://semver.org/).

---

## [2.5.0] — unreleased

Ideas and register facts from related projects (kr0ner/OneESP32ToRuleThemAll,
aly-fly/Stiebel-Eltron_heat_pump_monitor, VasekCZ81/stiebel-can-service), verified on a WPL13E.

### Added

- **Bus scan** — ask one CAN member for every signal-table index or a hex range; answers are
  published to `heatingpump/scan/…`, HA buttons "Bus-Scan Manager/Heizmodul/Kessel",
  `helperscripts/scan_report.py` turns the result into a request block for new models.
- **Thermostat cards** — MQTT climate entities for heating (room setpoint, operating mode as
  presets, compressor as action) and hot water. Number/select entities are unchanged.
- **Fault text** — `FEHLERMELDUNG` decoded to plain text ("Keine Stoerung", "Hochdruck", …)
  with a remedy attribute; fault list extended by non-conflicting WPM2 codes.
- **Summer mode (WPL13E)** — threshold `SOMMERBETRIEB_TEMP` (0xfdb5, found by bus scan) and
  `sommerbetrieb_aktiv`, computed like the WPMW II does it (damped outside temperature over
  24/48/72 h, 1 K hysteresis), plus the damped temperature itself. The damping starts from the
  current temperature on first boot and is kept in NVS afterwards.
- **`read_only` mode** — never writes to the heat pump; CI compiles this variant.
- **Dashboards** — standard Lovelace dashboard and a system schema with own SVGs (`dashboards/`).
- **Register names** — 32 `TEST_OBJEKT_*` rows named, 115 indices added for WPM3-generation
  models only (`helperscripts/import_signal_names.py`; facts only).

### Changed

- Signals answering `-255` (not available) are polled with growing intervals (up to 6 h) and
  create no HA entity until they deliver a real value.

### Fixed

- **Read requests of other bus nodes were decoded as values** (Elster telegram type was not
  checked). This produced constant fake `0` readings for the requesting node, e.g. "MANAGER
  VERDICHTER 0.0" on WPL13E, and hid real values such as the manager's return temperature.
- Short CAN frames dereferenced an unset member.
- Bool fields carrying other bits ("?") are no longer published to binary sensors (HA rejected
  them with a warning per poll). WPL13E `ABTAUUNGAKTIV` reads 0x0200 while the compressor runs.
- WPL13E refrigerant pressures (`ANZEIGE_HOCHDRUCK/NIEDERDRUCK`) are hundredths of bar.
- `make test` did not rebuild objects after header changes (stale `ElsterTable.h` in tests).

---

## [Unreleased]

### Added (WPL13E, verified on real hardware against the WPMW II manual)

- **Writable heating curve** (`HEIZKURVE` on MANAGER) — previously WPL17 only.
- **kWh-resolution energy**: `*_SUM_KWH` and daily `*_TAG_KWH` registers. COP now includes the
  kWh remainder (was whole MWh only) and the combined total/daily energy sensors appear.
- Diagnostics: hot-gas temperature, compressor DHW runtime, electric booster (DHC) runtimes.

### Fixed

- WPL13E: `compressor_active` was driven alternately by the IWS compressor value and a constant
  0.0 copy from the MANAGER seen on the bus. MANAGER copies of IWS values (and compressor-1 /
  cooling runtimes the WPMW II polls itself) are now ignored on WPL13E.
- COP waits until all kWh remainder registers are known (no jumps right after boot).
- Entities' MQTT discovery is resent on every broker (re)connect. Discoveries sent during boot,
  before the connection was up, were lost until the 15-minute refresh (e.g. renamed entities
  or writable numbers appeared late).

### Changed

- WPL13E: the heat pump module's `AUSSENTEMP` is labelled "Luftansaugtemperatur" (air intake at
  the unit, per the manual); the controlling outdoor sensor stays "Außen Temperatur".

### Removed (WPL13E — no valid value over 30 days to 11 months of HA history)

- `SAMMLERISTTEMP` (-40 °C, no sensor), `HEIZKURVE` on KESSEL (always 0), `WW_HYSTERSE`,
  `ANTILEGIONELLEN_ZEITPUNKT` (-255), compressor-1 / cooling / MANAGER runtime counters.

---

## [2.4.2] — 2026-10-27

### Fixed

- **Compile error "Bit rate ${CAN_BIT_RATE} is not supported"** when `heatingpump.yaml` has no
  `can_bit_rate` substitution (regression since 2.3.0). The CAN packages now default to
  `20kbps`; CAN-B models (WPL17/WPL23/WPL10AC) still set `can_bit_rate: 50kbps`. (#39)

### Removed

- **Calculated `betriebsart` sensor.** It mapped `SOMMERBETRIEB` (0xfdb4) to "Sommerbetrieb",
  but per the WPMW II manual that signal is the *summer-mode automatic* parameter (on/off),
  not whether summer mode is currently active — so it showed "Sommerbetrieb" in winter.
  The real operating mode remains available via `PROGRAMMSCHALTER` and `BETRIEBSART_WP`.
  The orphaned entity can be removed with `helperscripts/clean_mqtt_discovery.py`.

### Changed

- `SOMMERBETRIEB` is now labelled "Sommerbetrieb-Automatik" / "Summer Mode Automatic"
  (entity ID unchanged).

---

## [2.4.1] — 2026-10-27

### Fixed

- **No more dead energy/power/status entities on models without the WPL17 registers**
  (e.g. WPL13E): the combined energy totals, daily energy, electrical power and derived
  operating-status sensors (added in 2.3.0) are now announced to HA only once they have a
  value. Previously all 12 were created and stayed "unknown". Leftover entities can be
  removed with `helperscripts/clean_mqtt_discovery.py`.
- Removed a hardcoded personal OTA address (`use_address`) from `common.yaml`;
  `make upload` now defaults to `heatingpump.local` (override with `OTA_ADDR=`).

---

## [2.4.0] — 2026-10-27

### Added

- **WPC7 / Stiebel WPC-07cool / Tecalor TTF07c model** — community-tested on real hardware
  by @Mountain-wg and @EnderBow. Opt in with `device_model: "wpc7"` (CAN-A, 20 kbps).
  Signals that returned -255 on real units are commented out. (#27)
- **`disable_sg_ready` substitution** (default `"false"`): `"true"` hides all SG Ready
  entities and skips their MQTT discovery — for models without SG Ready input (e.g. WPC7).
  Already-retained discovery topics can be removed with `helperscripts/clean_mqtt_discovery.py`.

### Fixed

- **`et_little_bool` now accepts `0x0001` as `on`** (like `et_bool` since 2.3.0), so
  `SOMMERBETRIEB` no longer shows "unknown" on units sending `0x0001`.
- **`ANTILEGIONELLEN`** is now a bool (on/off binary sensor) instead of a raw number —
  WPC7 reported "unknown". ⚠️ Entity changes from `sensor` to `binary_sensor`; the old
  sensor entity can be removed with `helperscripts/clean_mqtt_discovery.py`.
- **`WP_PUMPENSTATUS`** (WPC7) published as on/off instead of 0/1.

---

## [2.3.0] — 2026-10-27

### Added

- **WPL17 (ICS classic) heat pump model** — hardware-verified by @Enrica-r over several weeks
  incl. heating season. Connects via CAN-B (X1.2) at **50 kbps**. Opt in with
  `device_model: "wpl17"`. Supersedes the unverified desk-research port (#28). (#36)
- **`can_bit_rate` substitution** (default `20kbps`, existing models unaffected).
- **New CAN members**: `HEIZMODUL_WPL` (0x514), `KESSEL_CANB` (0x201); `MISCHERMODUL_2` (0x601)
  re-added for the WPL17 heating program.
- **Derived sensors**: `calculated/betriebsstatus` (Aus/Heizen/Warmwasser/Kuehlen) and
  `calculated/el_leistung_w` (electrical power from daily Wh counters).
- **Diagnostic scan firmware** (`tools/heatingpump_scan.yaml`) and
  `helperscripts/clean_mqtt_discovery.py` to prune orphaned retained discovery topics.
- Smoke test manifest `tests/models/wpl17_smoke.json`.

### Fixed

- **Writable-number MQTT discovery** no longer emits an empty `device_class` — HA rejected
  the whole discovery payload, silently hiding e.g. the heating-curve input.
- **Combined energy totals** (`energie_*_total_kwh`, `waerme_*_total_kwh`) get a deglitch
  filter against transient ±1000 kWh spikes at the nightly register rollover.
- Manual PDF renamed so the repo clones on Windows (#35, #38).
- CI: `ota_password` added to `ci_secrets.yaml`.

### Note

- `et_bool` values now also accept `0x0100` as `true` globally (same value in both byte
  orders; `false` is always `0x0000`). No behaviour change expected for existing models.

---

## [2.2.0] — 2026-10-11

### Added

- **WPL10AC (WPM3) heat pump model** — hardware-verified by @fwetex on real hardware.
  Requires the WPM3 CAN B / display connection at 50 kbps. Opt in with `device_model: "wpl10ac"`.
  Includes DHW/boiler temperature, room actual temperature + humidity, setpoints, and
  WPM3-specific operating mode names (Komfortbetrieb, Eco-Modus, …). Closes #34.
- **New Elster registers**: `RAUMISTTEMP` (0x4ec7, room actual temperature),
  `FEUCHTE` (0x4ec8, humidity).
- **Smoke test manifest** for wpl10ac (`tests/models/wpl10ac_smoke.json`), verified on hardware.

### Fixed

- **SG Ready normal-mode value** is now model-aware: `"Komfortbetrieb"` for WPL10AC,
  `"Tagbetrieb"` for all other models — previously hardcoded to `"Tagbetrieb"`.
- **`MISCHERMODUL_2` renamed to `RAUMFERNFUEHLER_2`** — corrects the CAN ID from 0x601
  to 0x401 (this member was unused in all existing signal tables so no behaviour change).
- **`SOMMERBETRIEB` type** changed from `et_bool` to `et_little_bool` (matches the actual
  CAN encoding; C++ code already treated it as such).
- **`SEKUNDE` optional in `publishTime()`** — falls back to 0 if not available (WPM3 does not
  provide it).

### Improved

- Log message buffer enlarged from 120 to 160 chars to prevent truncation of long CAN traces.
- paho-mqtt 1.x/2.x compatibility shim in `tests/test_mqtt_smoke.py`.

---

## [2.1.2] — 2026-10-11

### Fixed

- **OTA password was hardcoded** — `common.yaml` contained a literal OTA password string instead of
  `!secret ota_password`. Users who clone fresh now get the correct `!secret` reference; existing
  deployments are unaffected (ESPHome keeps the flashed password). Closes #32.

---

## [2.1.1] — 2026-07-25

### Fixed

- **Compile error with ESPHome 2026.7** — `-DHA_DEVICE_MODEL='"wpl13e"'` relied on PlatformIO's
  shell-style quoting. The ESP-IDF/ninja build used since 2026.7 passes the flag through verbatim,
  so the quotes ended up in the source (`error: missing terminating ' character`). The flag is now
  quote-free and the string literal is built with `HA_DEVICE_MODEL_STR` in `config.h`. Closes #31.
- **`-Wformat` warnings** — `CanId` (`uint32_t`) printed with `%x`/`%u` warned under the
  ESP-IDF toolchain, where `uint32_t` is `long unsigned int`. Now cast explicitly.
- Self-heal from TWAI bus-off instead of requiring a manual reset.
- `PROGRAMMSCHALTER` option values are no longer translated (they are protocol values, not display
  names), and garbage CAN writes are rejected.
- Calculated/COP sensors and control sub-devices use the dynamic device id instead of a hard-coded
  model.
- `wpf10.yaml` was missing its `esphome: includes:` block.

## [2.1.0] — 2026-06-14

### Added

- **Multilingual entity names** — entity display names compile in German (default) or English.
  Set `language: "EN"` in `heatingpump.yaml` substitutions. Zero flash overhead for the unused
  language. Further languages can be added via `lang_XX.h` + one `#elif` in `language_select.h`.
  See `docs/CONTRIBUTING.md` for a step-by-step guide.
- **Writable room temperature setpoints** — `RAUMSOLLTEMP_II`, `RAUMSOLLTEMP_III`, and
  `RAUMSOLLTEMP_NACHT` now appear as writable `number` entities in HA (10–30 °C, 0.5 °C step),
  matching the existing `RAUMSOLLTEMP_I`. Closes #1.
- **CAN bus diagnostic sensors** (ESP32-S3 only) — four new sensors (`CAN TX Fehlerzähler`,
  `CAN RX Fehlerzähler`, `CAN Bus Fehler`, `CAN Bus Zustand`) published every 30 s using
  `twai_get_status_info()`. Appear under the diagnostic category, disabled by default in HA.
  Useful for debugging termination and ground issues without external tools. Closes #20.
- **Compressor runtime sensors** — `LZ_VERD_*` signals (`Laufzeit Verdichter`) now appear in HA
  with `device_class: duration`, `unit: h`, `state_class: total_increasing`. Polled every 10 min
  from both HEIZMODUL and MANAGER. Closes #3.

### Fixed

- `et_mil_val` signals incorrectly published as `device_class: temperature` / `unit: °C` — now
  correctly `device_class: duration` / `unit: h`. Resolves HA warning about missing unit.
- `LZ_VERD_*` signals had `isBlacklisted = true` in `ElsterTable.h`, silently dropping all CAN
  responses before MQTT publish.
- All-caps entity names for `ANTILEGIONELLEN`, `ANTILEGIONELLEN_ZEITPUNKT`, `BRENNER`,
  `GEBAEUDEART`, `HEIZKURVE`, `WW_ECO`, `WW_HYSTERSE` — now show proper display names.

### Improved

- **Test coverage** — 72 new unit test cases (392 assertions, up from 236). New coverage:
  MQTT discovery payload structure, `updateSensor` caching and global state dispatch, all
  calculated sensor publish functions, writable entity discovery, i18n macro resolution.
- **Documentation** — `language` substitution in both installation paths, localization
  contribution guide in `CONTRIBUTING.md`, updated entity reference tables in `CONFIGURATION.md`.

### Upgrade Notes

No breaking changes. Existing `heatingpump.yaml` files without a `language` substitution
compile with the German default — behaviour is unchanged. The new `LZ_VERD_*` sensors will
appear as new entities after flashing. CAN diagnostic sensors are disabled by default in HA.

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

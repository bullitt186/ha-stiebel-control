# Changelog

All notable changes are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions: [SemVer](https://semver.org/).

---

## [Unreleased]

### Upgrade notes

- WPL13E/WPL18E/TTL 33: some existing sensors get new display names; entity IDs stay. `STILLSTANDZEIT`
  now has the unit min, so HA asks once to update the unit of its statistics (Developer tools →
  Statistics).

### Fixed

- Raw energy counters decoded as plain integers (`*_TAG_WH`, `*_SUM_KWH`, `WAERMEERTRAG_RUECKGE_*`,
  solar yields; 24 signals with metadata) had no `state_class` in their discovery although their
  metadata says `total_increasing`/`measurement`, so Home Assistant kept no long-term statistics
  for them. They now carry it, and HA starts recording statistics for them after the update.
- A `total_increasing` signal (energy counters, runtime hours) that answers "not available"
  (`0x8000`) now keeps its last state instead of publishing `-255`: HA reads a drop as a new
  meter cycle and would add the next real value to the statistics sum again.
- WPMW II: until `PUMPENSTATUS` is known, the operating state compared the energy counter sums
  since boot instead of the last 5 minutes, because the 5-minute window never restarted there.
- Diagnostic scan firmware (`tools/heatingpump_scan.yaml`): it no longer keeps its own copy of the
  firmware header list, which had to be updated with every new module; the headers come from
  `common.yaml` like in every other config. CI now compiles the scan config in the layout
  `tools/README.md` describes (#122).
- Climate entities (heating, HK2, hot water) showed the mode "off" in Home Assistant (thermostat
  card "Aus"), because they announce no mode state. The mode now follows the availability of the
  ESP: `heat` while it is online, `off` while it is offline.
- WPMW II: `LZ_VERD_2_HEIZBETRIEB` used a signed type and would have turned negative above 32767 h;
  it is decoded as hours like the other runtime counters.
- SG Ready with `language: "EN"`: the controller published German state labels while the select used
  the English ones, on the same topic. All state labels now come from the language files (one
  `sgReadyStateLabel()`), and the command handler accepts either language (#93).
- A raw bus scan (`heatingpump/scan/start` with an index range) crashed the ESP after a few
  thousand indices: `GetElsterIndex()` cached every index that is not in the table in a hash map,
  which grew until a rehash failed (`exception/panic`, seen on a WPL13E at 0x601 0x0000-0x0fff).
  Index lookups now use a table sorted once at startup (binary search, fixed size) and cache nothing.
- WPL13E/WPL18E/TTL 33 (WPMW II): `FEHLERMELDUNG` takes the fault number from the high byte, so a
  high-pressure stop shows `HD-SENSOR-MAX` instead of `ERR 7424` (#52). The fault list is
  unchanged.

### Added

- New model `wpla7` (WPL-A 07, WPM3, 1 heating circuit, CAN 50 kbps): `wpla7.yaml`, the signal
  request table `signal_requests_wpla7.h`, and the CI compile target `ci_heatingpump_wpla7.yaml`.
  Verified on a WPL-A 07 230 Premium. Model profile flag `dhwActualOnKesselCanb` for the DHW actual
  temperature on `KESSEL_CANB`.
- WPL13E/WPL18E/TTL 33 (WPMW II): names, units and scaling for IWS values and parameters checked on a
  WPL 18 E display: evaporator outlet, compressor inlet and vapour injection temperature, the pressure
  sensor currents, condenser outlet (`FROSTTEMP`), fan output, controller and mixer dynamics,
  compressor off time and its countdown (`STILLSTANDZEIT_0`, in s), the current room setpoint, and
  names for eight further registers (#52). Most of them are not polled by default. Read-only; no new
  writes.
- WPMW II (WPL13E, WPL18E, TTL 33): room actual temperature of the remote control FE 7 as sensor
  "Raum-Ist Fernbedienung", and the circuit it is assigned to (parameter 22 "AUSWAHL FE",
  `FERNBEDIENUNGSZUORDNUNG`). The manager does not answer `RAUMISTTEMP`; its virtual members mirror
  the one FE sensor (a WPL13E's 0x601, 0x602, 0x301 and 0x302 all followed a heated FE). The room
  temperature is the current temperature of the climate of the assigned circuit only.
- WPMW II: effective room setpoint of the FE circuit as sensor "Raumsoll wirksam Fernbedienung"
  (`VERSTELLTE_RAUMSOLLTEMP` on 0x602): day or night setpoint plus the FE knob (±5 K). It shows
  the FE mode switch too (moon = night setpoint without correction).
- WPL13E/TTL 33: climate entity "Heizung HK2" for heating circuit 2 (day room setpoint on 0x602,
  current temperature from 0x602), so both circuits can use a thermostat card. The operating mode
  stays on the heating climate.
- WPMW II heating circuit 2 (mixer circuit): day and night room setpoint and heating curve as
  number entities. The manager keeps them on the virtual member `MISCHERMODUL_3` (0x602), found by a
  change of "Raumtemp. 2" on the display (20.0 -> 20.5 moved only `RAUMSOLLTEMP_I` @ 0x602);
  `RAUMSOLLTEMP_II` on the manager is not circuit 2. Writable on WPL13E and TTL 33.
- WPL13E/WPL18E/TTL 33: heating program of heating circuit 2 as one text entity per day, read from
  and written to `HEIZPROG_1_<day>` on `MISCHERMODUL_3` (0x602). In `Automatik` an empty program keeps
  the circuit on its night setpoint. TTL 33 now also polls the second and third period of both
  heating programs, so the circuit 1 and 2 program texts appear there.
- Writable numbers take their range from the heat pump (#79). The firmware reads each number's
  limits once with the Elster switch bytes `0xF8` (minimum) and `0xF9` (maximum), starting about a
  minute after the request manager and at least 2 s apart, and then publishes the number and
  climate discovery again with that range (same entity). The effective range is the overlap of the
  device range and the default range from `writableNumbers[]`: lower bound = max(device, default),
  upper bound = min(device, default). The defaults are deliberate limits (e.g. standstill time
  >= 20 min), so the device range can only narrow them (e.g. heating curve 0.0 -> 0.2), never widen
  them. An empty overlap uses the default range. The raw device range is written to the log.
  `0x8000` on one side keeps that side at the default; `0x8000` on both sides, a range with
  minimum >= maximum, or an empty overlap keeps the default range for good. No answer after one
  retry: the default range applies for now, and the request is repeated after 1 hour until an
  answer comes; on success, discovery is sent again with the same unique_id. Clamping applies per
  row (name and member): heating circuit 2 (`MISCHERMODUL_3`) has its own limits, separate from
  heating circuit 1. Once the device range is known, numeric writes of these numbers are clamped to
  the effective range, also those from automations or plain MQTT; without a known device range
  writes are not changed (as before). Limit answers never reach the sensor path, and read and
  write requests for the indices 0xF7–0xFA always use the long form, because in the short form
  that byte is the switch byte.

### Changed

- Firmware code split into modules (#109): `ha-stiebel-control.h` keeps only the CAN entry points;
  MQTT publishing, CAN I/O, discovery, calculated sensors, energy/COP, writables, SG Ready glue,
  time programs, set commands, bus scan, fault list and republish each have their own header (see
  `docs/ARCHITECTURE.md`). No change in behavior. Custom configs that list the firmware headers
  in `esphome: includes:` instead of using `common.yaml` must add the new headers.
- `dashboards/heatingpump_standard.yaml` follows one design language: every color has one meaning on
  every view (heating orange, hot water blue, electricity grey, flow red, return indigo, setpoints light
  grey, outdoor blue-grey, room pink/purple, compressor deep purple, COP green), the same names for the
  same values, tiles with trend lines instead of entity lists, and graphs for pressures and
  temperatures. New view "Heizkreise": both heating circuits side by side (thermostat, night setpoint,
  heating curve card, week program card) and the hot water program; it needs
  `dashboards/www/stiebel-cards.js`, the other views use built-in cards only.
- The `device_name` and `friendly_name` substitutions take effect (#102). `device_name` is the
  ESPHome node name and the root of every MQTT topic, discovery node ID and default entity ID
  (`<device_name>/MANAGER/…`, `sensor.<device_name>_…`); the firmware has no hard-coded
  `heatingpump/` topic left. `friendly_name` names the ESPHome device in HA. **Upgrade:** with the
  default `device_name: heatingpump` nothing changes on the bus or in MQTT. If your
  `heatingpump.yaml` sets another `device_name`, the hostname, the topics and the discovery configs
  move to that name: set it back to `heatingpump` to keep everything as it is. The ESPHome device in
  HA shows your `friendly_name` instead of "Heating Pump CAN Interface". The smoke test takes the
  root from `device_name` (`make smoke-test`, `--root`).
- Discovery configs carry a language-independent `default_entity_id`
  (`<domain>.heatingpump_<member>_<signal>`), so DE and EN builds create the same entity IDs on a
  fresh install. The shipped dashboards, `docs/CONFIGURATION.md`, `SG_READY.md` and the SG Ready
  example use these IDs; CI checks the dashboards (`tests/check_dashboard_ids.py`). **Existing
  installs keep their entity IDs**; rename them in HA or adjust the dashboard IDs to use the shipped
  dashboards (#96).
- Discovery: the device block is defined once (`HA_MAIN_DEVICE_JSON`, `memberDeviceJson()`), the
  COP sensors are rows of `calculatedSensors[]` (same topics and unique IDs as before), rows are
  addressed by the `CalcIdx` enum and flagged `onDemand` instead of a hard-coded index threshold.
  `can_members.h` holds the bus members and `SignalRequest`, so the test stub uses the real type (#95).
- MQTT set commands: one subscription on `heatingpump/+/+/set` and one C++ dispatcher
  (`handleSetCommand()`) replace about 25 YAML handlers that addressed table rows by position. Rows
  are found by signal name and member, values are checked against the row's min/max (#92).
- `common.yaml` no longer declares unused internal template sensors or polls `PROGRAMMSCHALTER`
  and `SOMMERBETRIEB` a second time; `WW_ECO` moved into the request tables (#98). Dead code
  (`processCanMessageDebug()`, `tests/main.cpp`) removed, `ai_refine_friendly_names.py` archived (#99).
- Agent instructions (`.github/copilot-instructions.md`, imported by `CLAUDE.md`), `docs/ARCHITECTURE.md`
  and `README.md` describe the current code: request tables per model, `model_profile.h`, `id: my_can`,
  no dispatcher. CI checks that every repo path named there exists (`tests/check_doc_paths.py`, #100).
- Model capabilities live in one place, `model_profile.h` (`kModel` flags such as
  `wpmw2WritesVerified`, `canBRoutes`, `perMemberEnergy`); no code compares model names any more
  and the build flag `-DHA_MODEL_<model>` is gone (`HA_DEVICE_MODEL` is the only input). The unit
  tests of each model now compile its real signal table and operating mode names (#94).
- Energy counter request rows are defined once (`SIGNAL_REQUESTS_ENERGY_IWS`), and the wpl18e smoke
  manifest refers to wpl13e (`same_as`) instead of copying it (#101).
- Firmware builds without compiler warnings (#120), and one runtime DJB2 hash, `elsterNameHash()`,
  replaces the duplicate `hash_runtime()` (#121).
- Development: mutation testing of the C++ logic with Mull (`make mutate` for changed lines,
  `make mutate-all`, weekly non-blocking CI workflow), a Claude Code Stop hook that runs the fast
  local checks, and design-first test rules in `CLAUDE.md`.

## [2.7.0] — 2026-11-15

Contributions from SolderAndSyntax and bastard-GIT: WPMW II clock/date writes, W5500 Ethernet,
0x9000 handling, request and addressing fixes. SG Ready is more reliable and can leave the time
programs active, which are now writable from Home Assistant.

### Upgrade notes

- WPL13E/WPL18E: `ABTAUUNGAKTIV` is no longer polled. Its HA entity
  (`binary_sensor.heizmodul_abtauung_aktiv`) stays as unavailable and can be deleted; use
  "Abtauung läuft" instead.
- TTL 33: the raw sensors `W_WASSERPROG_1_<day>` are replaced by one text entity per day; the old
  ones stay as unavailable entities and can be deleted.
- SG Ready writes now go out one at a time and are checked (about 6 s per write), so a change of
  state takes up to 20 s on the bus instead of happening at once.
- A boost now starts from the setpoints read from the heat pump. If your DHW or room setpoint was
  left boosted by an older version, set it back once before the next SG Ready cycle.
- `common.yaml` no longer contains WiFi; a local copy that patches the `wifi:` block must patch
  `network_wifi.yaml` instead.

### Added

- WPMW II: DHW time program as one text entity per day (`W_WASSERPROG_1_<day>` with up to three
  periods, e.g. `06:30-09:00,11:00-17:00`), read on WPL13E, WPL18E and TTL 33, writable on WPL13E
  and TTL 33. Input is checked (15-minute steps, start before end, no overlap) before anything is
  written. On TTL 33 the former raw sensors `W_WASSERPROG_1_<day>` are replaced by these entities (#82).
  The heating program of heating circuit 1 (`HEIZPROG_1_<day>`, manager) is shown the same way on
  WPL13E and WPL18E.
- SG Ready: select for the operating mode in state 2, `Tagbetrieb` (default, as before) or
  `Automatik`, so the time programs and the DHW setpoint 2 apply in normal operation (#85).
- Calculated binary sensor "Abtauung läuft" (`abtauung_laeuft`): on while the heat pump module's
  last defrost duration reads 0, which it does from the start of a defrost until it ends. On a WPL13E
  `ABTAUUNGAKTIV` stayed off through three defrosts; the `wpl13e` table now polls the duration every
  minute instead of `ABTAUUNGAKTIV` (#48).
- WPMW II models: the "Update Uhrzeit" and "Update Datum" buttons write `UHRZEIT` as one value and `DATUM`
  followed by `JAHR`, the way the WPMW II manager accepts them (#68). The single `STUNDE`/`MINUTE`/`SEKUNDE`
  writes are ignored there, and `DATUM` alone resets the year to 2000. Other models are unchanged.
- Network package selectable by the substitution `network_package`: `network_wifi.yaml`
  (default, WiFi, captive portal and WiFi sensors moved out of `common.yaml` unchanged) or the
  new `network_ethernet_w5500.yaml` (W5500 SPI Ethernet, DHCP, pin defaults for the Waveshare
  ESP32-S3-POE-ETH). Existing configurations need no change.
- SG Ready room setpoint boost is configurable per state (`number.…_sg_ready_raum_boost_zustand_3/4`,
  0–5 K, defaults 1 K and 2 K as before). With 0 the room setpoint of heating circuit 1 is left
  alone, so SG Ready no longer shifts the heating curve (#50).
- WPMW II models: sensors that share a signal name but measure different points get their own
  names per bus member, e.g. KESSEL `RUECKLAUFISTTEMP` is "Heizkreis 1 Regelfühler", HEIZMODUL
  `RUECKLAUFISTTEMP` is "Wärmepumpe Rücklauf IWS" (#46). Entity IDs of existing installs stay.
- Calculated sensors for short-cycling: compressor starts today, runtime today and runtime per start
  (#49). Off phases under 2 min do not count as a new start; the day follows the heat pump's date.

### Fixed

- SG Ready: the boost baseline is the DHW and room setpoint read from the heat pump, not the
  constants 48 °C / 20 °C, so leaving state 3/4 restores the user's setpoints (#85).
- SG Ready: writes sent back to back were partly lost on a WPL13E (DHW boost not applied, room
  setpoint not restored). SG Ready writes now go out one at a time with a gap, each checked against
  the read-back and sent once more if it did not take; every write is logged (#85).
- Numeric signals answering `0x9000` (function switched off, e.g. `BIVALENZALTERNATIVTEMPERATUR_HZG`
  on KESSEL) were published as a number: `et_dec_val` showed -2867.2 °C and went into the long-term
  statistics. `0x9000` is now not published for `et_dec_val`, `et_cent_val` and `et_mil_val`: the
  entity keeps its last state, and it does not count for the poll back-off, because the device does
  answer (#78). `et_hours` and `et_little_endian` are unchanged, because there `0x9000` is a valid
  value.
- The request manager started every call at the top of the request table, because
  `signalProcessingStartIndex` was never updated. With more due requests than one per second
  (e.g. wpl13e/wpl18e about 68, wpl17 about 63 per minute) the end of the table could starve;
  it now continues after the last checked entry.
- Read and write requests to bus members with an id whose lower 7 bits exceed 7 went to the
  wrong member: the frame header kept only 3 bits, so a request to 0x514 (HEIZMODUL_WPL) went to
  0x504. Byte 1 (data[1]) now carries the lower 7 bits of the receiver id.
- Right after an MQTT connect ESPHome queues its subscriptions and native states; its async publish
  queue holds 30 messages, so the SG Ready states and part of the republished discovery configs and
  states were dropped (seen as `mqtt_publish_failures` after a boot). Own republishing now starts
  3 s after the connect, and the SG Ready states are retried until they are queued.
- `mqtt_publish_failures` counts only publishes refused while connected (dropped messages);
  attempts while disconnected are sent again after the connect and no longer inflate it.
- SG Ready boost values were lost on a reboot (the docs said they were stored). All four boost
  values are now kept in NVS.
- The read-back right after a write often returned the old value on the WPMW II, so HA showed the
  previous setting until the next poll. Every write is now read back a second time about 4 s later
  (reported by SolderAndSyntax, #68).

### Changed

- `ttl33`: the heating curve and the WPMW II parameters (Reglerdynamik, Stillstandzeit, summer mode
  threshold, bivalence points heating/hot water) are writable number entities, as on `wpl13e`. Each
  value was written over CAN on a TTL 33 and checked on the WPMW II display (by SolderAndSyntax).
- `ttl33` table polls `SCHALTWERKDYNAMIKZEIT` (display "Reglerdynamik"), so its writable entity no
  longer stays unknown.

## [2.6.0] — 2026-11-06

Contributions from reniko, SolderAndSyntax and hawkeye80: three new models, WPMW II fixes and
parameters checked against the display of a WPL13E.

### Upgrade notes

- The sensor "Sommerbetrieb Grenztemperatur" now reads `SW_AUSSENTEMP` (0x01bf). The old sensor
  for 0xfdb5 and the `SPERRZEIT` sensor are no longer published; on an existing install they stay
  in HA as unavailable entities and can be deleted.
- `EVU_SPERRE_AKTIV` has no device class any more (see Fixed). A local override of the device
  class can be removed.
- `make smoke-test` and the helper scripts no longer default to a broker: set `MQTT_BROKER` and
  `MQTT_USER` (and `MQTT_PASS`).

### Added

- New model `wpl9` (WPL 09 ACS classic with WPM3/FET) with both DHW settings routed through
  CAN-B, by hawkeye80 (#43). Live-checked by its author on the previous firmware; see
  `docs/WPL9.md` for what is and is not validated.
- New models with a WPMW II manager: `ttl33` (Tecalor TTL 33, by SolderAndSyntax, #62) and `wpl18e`
  (WPL 18 E, reported by reniko, #52). Register corrections are shared through `HA_CONTROLLER_WPMW2`.
- WPL13E: five WPMW II parameters are writable number entities: Reglerdynamik (parameter 42,
  0xfdb0), compressor minimum off time (43, 0xfdb1, at least 20 min), the summer mode threshold
  (10, 0x01bf) and the bivalence points for heating and hot water (27/33, 0x01ac/0x01ad).
  Parameters 10, 42 and 43 were confirmed by a change on the display; 38 WW-Hysterese (0x0022) is
  read. Other models do not get these entities; a `/set` for them is ignored with a warning.
  Indices from a bus scan, ranges from the WPMW II commissioning list.
- Optional deadband for measured temperatures, `STATE_DEADBAND_TENTHS` in `config.h`, off by default
  (proposed by SolderAndSyntax, #64).

### Fixed

- `EVU_SPERRE_AKTIV` used HA's `lock` device class, which shows `on` as "unlocked", so an active
  utility lock read "Entsperrt". The sensor has no device class now; `on` still means lock active.
- Compressor runtime registers `0x07fc`–`0x0801` are plain hours (`et_hours`); values such as 6129 h
  showed as 6.129 h (by hawkeye80, #43). The WPL13E keeps its own row for `0x07fd`.
- Summer mode used `SOMMERBETRIEB_TEMP` (0xfdb5) as threshold, which reads 10 °C while the display
  shows 20 °C. The threshold is `SW_AUSSENTEMP` (0x01bf), as SolderAndSyntax found on a TTL 33 (#62),
  and the sensor "Sommerbetrieb Grenztemperatur" now shows that register.
- Decode buffer overflow: values such as `not used time domain` (21 bytes) overran the 16-byte
  buffer in `processCanMessage()` and could reboot the ESP, for example during a bus scan
  (reported by reniko, #52).
- `ABTAUUNGAKTIV` stayed `unknown` in Home Assistant because its binary sensor had no
  `on`/`off` payloads (reported by reniko, #52).
- WPMW II: the energy totals only moved once a night. They are now read from `HEIZMODUL` (0x500)
  and follow the day counters, which the `wpl13e` table polls now (by reniko, #52). A total no
  longer steps back by up to 1 kWh at the day change, when the whole-kWh `SUM` replaces the day
  value. After a reboot the totals wait for the day counters instead of the stored base alone.
- WPMW II: `betriebsstatus` tells hot water from heating by the charging pump (`PUMPENSTATUS` bit
  `0x4000`) instead of counter deltas, which step only hourly there. The `el_leistung_*` sensors
  stay off on these models for the same reason (by reniko, #52).
- WPMW II: `compressor_active` stayed `on` when a utility lock cut power to the heat pump module
  during a run. It now goes `off` after 60 s without a frame from `HEIZMODUL` (by reniko, #52).
- `language: EN` did not compile: `lang_en.h` was not copied to the build (#61). It is now listed
  in `includes` and only applies with `HA_LANGUAGE_EN`. CI compiles an English build.
- Remote packages (`github://...`) failed with "Could not find file" (#41): include paths are now
  relative to the package files. A leftover local `ha-stiebel-control` folder no longer shadows
  newer package files. CI checks the layout without a local folder.

### Changed

- Helper scripts, the smoke test and the docs carry no site-specific MQTT broker or user.
- Docs describe the split signal table of 2.5.0 (#57): new section "Signal table" in
  `docs/CONTRIBUTING.md` (adding signals with and without HA metadata, binary sensor payloads,
  blacklisting); README, CONFIGURATION, ARCHITECTURE and the agent instructions no longer refer
  to the old 13-field rows.
- The test that compared every `ElsterTable` row with frozen digests of the pre-2.5.0 table was
  removed: it guarded the one-off conversion and failed for every new or changed signal. The
  consistency checks (main/meta table one to one, no hash collisions) stay.

---

## [2.5.0] — 2026-10-31

Ideas and register facts from related projects (kr0ner/OneESP32ToRuleThemAll,
aly-fly/Stiebel-Eltron_heat_pump_monitor, VasekCZ81/stiebel-can-service), verified on a WPL13E.

### Breaking changes

- **ESPHome 2026.7.0 or newer is required** (#44, thanks to hawkeye80). `common.yaml` now
  declares `esphome: min_version: 2026.7.0`; older ESPHome versions refuse the config. Update
  ESPHome (dashboard add-on or `pip install -U esphome`) before installing 2.5.0. CI and the release
  build use ESPHome 2026.10.1.

- **Firmware framework switched from Arduino to ESP-IDF** (#58) for both boards
  (`board_esp32s3.yaml`, `board_esp32.yaml`). ESP-IDF is ESPHome's recommended framework and
  nothing in this project needs Arduino. Flash: ESP32-S3 17.5 % -> 16.9 %, 4 MB ESP32 79.1 % -> 76.5 % of the
  app partition (ESPHome 2026.5.3). PSRAM (octal, 80 MHz) and 16 MB flash are unchanged.
  **Migration:** the first update after 2.5.0 switches the framework.
  - A normal OTA update works: OTA replaces only the app, both frameworks use the same app slot
    sizes, and the firmware keeps using the existing partition table and its `nvs` partition, so
    stored values (SG Ready boost values, energy base, summer-mode damping) are kept.
  - Clean the build files once (dashboard *Clean Build Files* / `esphome clean heatingpump.yaml`).
  - Do **not** run `esphome upload --partition-table` afterwards: it moves the NVS partition and
    the stored values are lost.
  - If the device does not come back (no API/MQTT, no safe-mode OTA), flash it over USB
    (`esphome run heatingpump.yaml --device <serial port>`, ESPHome Web or esptool). Note the
    SG Ready boost values before, a USB flash writes the ESP-IDF partition layout and can lose them.
  - Own lambdas or packages that use Arduino APIs (`String`, `Serial`, `WiFi`, Arduino
    libraries) need to be ported; the project code itself needed one change (TWAI diagnostics
    are compiled only when the TWAI driver is present, i.e. not on MCP2515 builds).
  - Details: `docs/CONFIGURATION.md` → "Board files and framework".

- **Delta T (`delta_t_continuous`, `delta_t_running`) now uses the heat pump's own sensors.**
  Before, flow and return were taken from whichever bus member published last (IWS, WPM sensors and
  the manager copy were mixed), so the value jumped between two measuring points. Measured on a
  WPL 13 E: IWS flow minus IWS return is 2-3 K while the compressor runs, but the WPM return sensor
  X2/3 (control sensor of heating circuit 1, sits in the buffer) reads about 0.6 K higher than the IWS
  return and holds its value after the compressor stops, which gave wrong and noisy spreads.
  - New source: `WPVORLAUFIST` and `RUECKLAUFISTTEMP` of the **heat pump module (HEIZMODUL, IWS B1/B2)**.
  - Fallback for models that deliver no IWS values: the KESSEL pair (`WPVORLAUFIST` X2/2 and
    `RUECKLAUFISTTEMP` X2/3), never a mix of both sources.
  - Entities, names and topics are unchanged, **but the values differ** (typically 0.5-1 K lower
    while running, and no more jumps). Long-term statistics and automations or alerts that use these
    sensors see a step at the update; thresholds may need adjusting.
  - `delta_t_continuous` is only meaningful while the pump circulates (the IWS sensors cool down to
    the pipe temperature in standby); use `delta_t_running` for evaluations.
  - WPL13E now polls `WPVORLAUFIST` and `RUECKLAUFISTTEMP` from the heat pump module explicitly.

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
- **Fault history** — the manager's stored fault list (display parameter 48, 20 entries,
  `FEHLERFELD_0..139`) is read every 30 min into one sensor "Fehlerliste": state is the newest
  fault, attributes hold time, remedy, count and all entries (dashboard renders it as a table).
- **Control deviation (all models with KESSEL return sensor)** — `Regelabweichung` = KESSEL return
  sensor (control sensor of heating circuit 1) minus the return setpoint. On a WPL 13 E the
  compressor starts at about -0.7 K and stops at about +2.6 K (five runs, second resolution).
- **ESP diagnosis** — `Reset Grund` (debug reset reason: brownout, watchdog, software), `Betriebszeit`
  (uptime) and `WLAN BSSID` as diagnostic entities, to find out why the ESP restarts during DHW charging.
- **WPL13E status registers** — `KESSELSTATUS`, `PUMPENSTATUS` and `HEIZKREIS_STATUS` (raw bit fields;
  the first two flip with every compressor run).
- **WPL13E signals from full bus scans** — bivalence points and heat-pump limits per heating/DHW,
  max. flow/return temperature, frost protection, booster total runtime (`LAUFZEIT_2WE`),
  defrost count and duration of the last defrost, superheat actual/target, flow setpoint.
- **More diagnostics** (#55, diagnostic category, disabled by default): CAN `RX verloren (Queue
  voll)` (`rx_missed_count`), `RX Überlauf (FIFO)` (`rx_overrun_count`) and `RX Warteschlange`
  (`msgs_to_rx`, frames waiting in the RX queue) on TWAI builds; `MQTT Sendefehler` (publishes the
  MQTT client refused, e.g. full `idf_send_async` queue) on all builds. WiFi disconnect reason
  was not added: ESPHome's `wifi: on_disconnect` trigger has no reason/BSSID parameter
  (2026.5 and 2026.10); the current BSSID is already available as `WLAN BSSID`.
- **Register names** — 32 `TEST_OBJEKT_*` rows named, 115 indices added for WPM3-generation
  models only (`helperscripts/import_signal_names.py`; facts only).
- **Optional package `no_captive_portal.yaml`** (#58) — removes `captive_portal:` and the
  fallback hotspot (`wifi: ap:`); list it after `base`. Saves about 88 KB of flash on the 4 MB
  ESP32 (76.5 % -> 71.6 % of the app partition). Default unchanged: both stay enabled, existing
  configs keep working. Without them, Wi-Fi is configured only via `secrets.yaml` (wrong
  credentials need a USB flash).

- **WPL13E: writable heating curve** (`HEIZKURVE` on MANAGER) — previously WPL17 only.
- **WPL13E: kWh-resolution energy** — `*_SUM_KWH` and daily `*_TAG_KWH` registers. COP now includes
  the kWh remainder (was whole MWh only) and the combined total/daily energy sensors appear.
- **WPL13E diagnostics** — hot-gas temperature, compressor DHW runtime, electric booster (DHC)
  runtimes.

### Changed

- **Build flags moved to `esphome: build_flags`** (#44, thanks to hawkeye80). The five `-DHA_*`
  defines (`HA_DEVICE_MODEL`, `HA_MODEL_*`, `HA_LANGUAGE_*`, `HA_DISABLE_SG_READY_*`,
  `HA_READ_ONLY_*`) were set via `platformio_options: build_flags`, which ESPHome deprecates and
  stops supporting in 2026.12.0 (warning on every build with the ESP-IDF backend). Values and
  substitutions are unchanged. CI/release ESPHome pin 2026.5.3 -> 2026.10.1.
- WPL13E: the heat pump module's `AUSSENTEMP` is labelled "Luftansaugtemperatur" (air intake at
  the unit, per the manual); the controlling outdoor sensor stays "Außen Temperatur".
- **WiFi post-connect roaming disabled** (#54): `wifi: post_connect_roaming: false`. ESPHome
  scans all channels every 5 min while RSSI is below -49 dBm; on a stationary heat pump
  controller this only stalled the MQTT connection (write timeouts, disconnects, before
  `idf_send_async` task watchdog resets).
- **`board_esp32.yaml` replaces `board_esp32s2.yaml`** (#58). The file always configured a
  classic ESP32 (`esp32dev`), never an ESP32-S2. `board_esp32s2.yaml` stays as a deprecated
  alias that includes the new file, so existing `!include`s keep working. New CI/Make targets
  `ci_heatingpump_esp32.yaml` / `make compile-esp32`; `ci_heatingpump_s2.yaml` and
  `make compile-s2` remain and now test the alias. Comments and docs no longer call it "S2".
- Signals answering `-255` (not available) are polled with growing intervals (up to 6 h) and
  create no HA entity until they deliver a real value.
- **MQTT publishing no longer blocks the main loop** (#54): `mqtt: idf_send_async: true` hands
  every publish to the esp-mqtt task through a queue. Before, a WiFi/TCP stall could block the
  loop for up to the 10 s network timeout and trigger a task watchdog reset. When the queue is
  full, messages are dropped and counted in a `Dropped N messages` warning.
- **`heatingpump/debug` only carries warnings and errors** (`log_topic` level `WARN`). Per-frame
  and per-request logs (`processCanMessage()`, `readSignal()`, "Discovery published", periodic
  date/time, COP and energy logs) are DEBUG now. The API/serial log (`esphome logs`) still shows
  INFO. About 290 fewer MQTT messages per minute on a WPL13E.
- **MQTT discovery is no longer re-sent every 15 minutes** (#53, #54). All configs are retained;
  they are sent again on broker connect, when Home Assistant comes online (birth message
  `homeassistant/status` = `online`) and on the manual trigger, paced at a few per 250 ms.
- "SG Ready Aktueller Zustand" refreshes every 60 s instead of 5 s and is published immediately
  when the state changes.
- New diagnostic sensors "Freier Heap", "Größter freier Block" and "Loop-Zeit"
  (`sensor: platform: debug`, every 60 s) to classify future restarts.
- **States are published only on change, with a 10 min heartbeat** (#55). Raw signal and
  calculated sensor states went out on every CAN answer although 78 % (raw) and 94 %
  (calculated) of them repeated the previous value. Now an unchanged value is sent again at most
  every `STATE_HEARTBEAT_MS` (`config.h`, default 10 min). After every MQTT (re)connect all known
  values are published once more (a few per 250 ms, after the discovery configs), so a broker
  restart without persistence does not leave constant values empty until the next heartbeat.
  Always sent as before: SG Ready state/boost values, bus scan results, fault list and fault
  remedy attributes, discovery configs. After a write (number/select/SG Ready/clock) the next
  read-back of that signal is published even if the value did not change.
- **No heap allocations per CAN frame and per request** (#55): signal lookups by name use a
  hash cache instead of `std::string` keys, the request table is resolved to `ElsterIndex`
  pointers once at startup (was up to two name searches per entry and second), member/signal
  keys for discovery, back-off and scheduling are integers, and the CAN frame is no longer copied
  per frame.
- **SG Ready no longer blocks the loop** (#55): the read-back after each write was preceded by
  `delay(100)` (100-400 ms per state change). It is now queued and sent ~100 ms later from the
  250 ms interval, one read per step.
- **ElsterTable rows slimmed down** (#57): rows hold only name, index, type and two flag bits
  (8 instead of ~44 bytes); the HA metadata of the 120 rows that use it moved to a side table
  (`ElsterMetaTable`, `GetElsterMeta()`), unused metadata strings of the other rows were dropped.
  Converted by `tools/elster_table_split.py`, verified row by row for every model and language.
  Flash: ESP32-S3 20.0 % -> 17.5 %, 4 MB ESP32 build 90.0 % -> 79.0 % of the app partition
  (-202 KB).

### Fixed

- WPL13E: `compressor_active` was driven alternately by the IWS compressor value and a constant
  0.0 copy from the MANAGER seen on the bus. MANAGER copies of IWS values (and compressor-1 /
  cooling runtimes the WPMW II polls itself) are now ignored on WPL13E.
- COP waits until all kWh remainder registers are known (no jumps right after boot).
- Discovery configs sent during boot, before the MQTT connection was up, were lost until the
  15-minute refresh (e.g. renamed entities or writable numbers appeared late); they are now sent
  on every broker (re)connect.
- **Read requests of other bus nodes were decoded as values** (Elster telegram type was not
  checked). This produced constant fake `0` readings for the requesting node, e.g. "MANAGER
  VERDICHTER 0.0" on WPL13E, and hid real values such as the manager's return temperature.
- Short CAN frames dereferenced an unset member.
- Bool fields carrying other bits ("?") are no longer published to binary sensors (HA rejected
  them with a warning per poll). WPL13E `ABTAUUNGAKTIV` reads 0x0200 while the compressor runs.
- WPL13E refrigerant pressures (`ANZEIGE_HOCHDRUCK/NIEDERDRUCK`) are hundredths of bar.
- WPL13E `LZ_VERD_2_HEIZBETRIEB` is plain hours ("Laufzeit Verdichter Heizen"); heating +
  DHW now add up to the total compressor runtime.
- Remedy text for `HD-SENSOR-MAX` named a sensor fault; it is the high-pressure limit
  (heating flow too low or room temperature / heating curve too high).
- WPL13E no longer polls `LAUFZEIT_WP1`: the manager fills it only on demand (read 0 otherwise).
- WPL13E `VERDICHTER` (0x07a8) is a status word (0x0008 idle, 0x0221/0x0228 running), not a power in W;
  `VERDAMPFERTEMP` (0x0014) is not a temperature (-30 idle, +58..+76 within minutes of a start);
  `HEIZKURVE` is a slope without unit. All three are published without unit and device class.
- `make test` did not rebuild objects after header changes (stale `ElsterTable.h` in tests).
- **Periodic discovery republish dropped nearly all signals** (#53). The blacklist filter took the
  lowercase uid fragment after the last `_` as signal name, found no table row and treated it as
  blacklisted: every 15 min 66-69 of ~70 signals were removed from the discovered set and
  rediscovered over the next minute (a wave of retained payloads; the first run blocked the loop
  for ~0.8 s). The filter is gone (blacklisted signals never get discovered) and a republish now
  actually re-sends the raw signal configs too.
- **`ElsterTable` was compiled into the firmware twice** (#56): the `static` table in
  `ElsterTable.h` gave main.cpp and `KElsterTable.cpp` one copy each (~159 KB). It is now defined
  once in `KElsterTable.cpp`; other files see `extern` declarations and use `ElsterTableCount`
  instead of `High(ElsterTable)`. The 4 MB ESP32 build drops from 98.7 % to 90.0 % of the app
  partition (-159 KB).

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

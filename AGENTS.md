# AGENTS.md

<!-- repo-kit:begin v1 (managed, do not edit; https://github.com/bullitt186/repo-kit) -->
## How to work in this repository (all coding agents)

This block is shared by all repositories of this maintainer and kept in sync by
[repo-kit](https://github.com/bullitt186/repo-kit). Repository-specific
instructions follow below it and win where they are more specific.

A pull request that follows this block passes review without rework. The
maintainer reviews against the same rules: the **Definition of Done** in
`CONTRIBUTING.md`.

### Before you change anything

1. Read this file to the end, `CONTRIBUTING.md`, and the architecture decision
   records in `docs/adr/` (start with `docs/adr/README.md`). An *Accepted* ADR is
   binding: do not work against it. If your change needs a different decision,
   write a new ADR that supersedes it, in the same pull request.
2. Find the issue for the work. If there is none and the change is more than a
   small fix, open one first and describe the problem, not only the solution.
3. Keep one topic per pull request. Do not reformat, rename or refactor code
   that your change does not need.

### Design first

Write the design down before the code: the behaviour that changes, the modules
and interfaces involved, and how it is tested. Put it in the issue or the pull
request description; for a small change the commit message is enough.

Write an ADR (`docs/adr/`, see its README) when the change:

- adds, removes or replaces a runtime dependency or external service,
- changes a public interface: API, CLI, configuration keys, file or data formats,
  entity or topic names, anything users or other systems rely on,
- changes the architecture: module boundaries, data flow, persistence, threading,
- sets a security, privacy or compatibility rule,
- reverses or replaces an earlier ADR.

Bug fixes and refactorings inside existing decisions need no ADR.

### Tests

- A bug fix starts with a regression test that fails for the reason the bug
  describes. Commit it with the fix.
- New behaviour comes with tests. Expected values come from the specification,
  documentation, a protocol table or a recorded real payload, never from running
  the code under test and copying its output.
- Assert observable results (output, state, sent request, raised error), not only
  that something was called.
- Changing an expected value in an existing test needs a reason in the commit
  message that names the source of the new value.
- Do not weaken lint rules, thresholds or checks to make a change pass; fix the
  code. A justified exception gets a comment and a note in the commit message.

### Documentation

Documentation changes in the same pull request as the behaviour it describes:
README, `docs/`, configuration examples, `CHANGELOG.md` (section `[Unreleased]`),
and the ADRs. Outdated documentation counts as a bug. Relative links in Markdown
must point to files that exist.

### Before you open the pull request

- Run the repository's full check (see below) and make it pass. Never commit or
  push on red, never use `--no-verify`.
- Rebase on or merge the current default branch and run the check again.
- Fill in the pull request template completely; tick only what you did.
- No secrets, tokens, personal data, local paths or host names in code, tests,
  logs, commits or pull request text.

### Continuous integration

CI runs the repository's checks and `PR hygiene` from repo-kit. PR hygiene fails
when code changes come without a test change or without a `CHANGELOG.md` entry,
when an ADR is malformed or missing from the index, or when a changed Markdown
file has a broken relative link. Only the maintainer can waive a rule, with the
labels `no-test-change` or `skip-changelog`. Do not ask for a waiver when a test
or changelog entry is possible.

### Respect

Do not change CI configuration, branch protection, release workflows, licence,
or this block unless the issue asks for it. Maintainer decisions recorded in
issues and ADRs are not up for silent change.
<!-- repo-kit:end -->

## This repository

### Project Overview
ESPHome firmware plus Home Assistant integration for Stiebel Eltron / Tecalor heat pumps on the
CAN bus (Elster protocol). The ESP32 acts as bus member `PC` (0x680), polls signals, publishes them
via MQTT discovery and writes settings. Two hardware variants, both on ESP-IDF: ESP32-S3 with the
built-in TWAI controller (recommended) and a classic ESP32 (`esp32dev`) with an MCP2515.

### Repository layout (`esphome/`)
- `heatingpump.yaml` — user entry point: substitutions (`device_model`, pins, `language`, …) and the
  package list (board, can, network, base, model). Never add sensors or logic here.
- `ha-stiebel-control/` — the packages and the C++ code:
  - `board_esp32s3.yaml`, `board_esp32.yaml` (`board_esp32s2.yaml` is a deprecated alias of the latter)
  - `can_esp32.yaml` (TWAI, 256 RX / 64 TX queue), `can_mcp2515.yaml` (SPI). Both use **`id: my_can`**;
    the C++ code sends with `id(my_can).send_data(...)`. Do not rename it.
  - `network_wifi.yaml` (default) / `network_ethernet_w5500.yaml`, selected by `network_package`
  - `common.yaml` — board-agnostic core: MQTT, API, OTA, build flags, `includes:`, MQTT command
    handlers (`on_message`), intervals, template sensors/buttons
  - `<model>.yaml` — model package (`wpl9`, `wpl10ac`, `wpl13e`, `wpl17`, `wpl18e`, `wpf10`, `wpc7`,
    `ttl33`, `wpla7`). It only adds the model's request table to `esphome: includes:`; it holds no sensors.
  - `signal_requests_base.h` — `SIGNAL_REQUESTS_BASE` (universal signals) and shared row macros
    such as `SIGNAL_REQUESTS_ENERGY_IWS`
  - `signal_requests_<model>.h` — what the model polls: `{"SIGNAL", FREQ_xx, cm_member}` rows
  - `model_profile.h` — model capabilities (`kModel`), see below
  - `can_members.h` — bus members (`CanMembers[]`, `CanMemberType`) and `SignalRequest`
  - `ha-stiebel-control.h` — only the CAN entry points: `processAndUpdate()` (every received
    frame) and `updateSensor()` (publish a decoded value, hand it to the module that uses it).
    It includes the firmware modules below (#109); see "Firmware modules".
  - `sg_ready_controller.h` — SG Ready state machine (pure C++, IO injected via `ISgReadyIO`)
  - `dhw_program.h` — WPMW II time program parsing/validation (pure C++)
  - `config.h` — timing constants, `HA_DEVICE_MODEL_STR`, `HA_TOPIC_ROOT`
  - `lang_base.h` (German `LNAME_*`), `lang_en.h` (English overrides), `language_select.h`
  - `elster/ElsterTable.h` — `ElsterTable[]` (name, index, type, blacklist, hasMetadata) and
    `ElsterMetaTable[]` (HA metadata); `elster/KElsterTable.cpp/.h`, `elster/NUtils.*`, `elster/NTypes.h`
- `packages/ha_stiebel_control.yaml` — HA helpers (clock/date sync). Docs: `docs/`, `SG_READY.md`.
- `manuals/` — Stiebel PDFs; consult them for signal meaning.

### Firmware modules (`esphome/ha-stiebel-control/`, #109)
One header per responsibility. Each module owns its global state and includes the modules it
uses; the includes form no cycle. The table is in include order: a module includes only modules
above it. The shared configuration is only the build flags, `config.h` (`HA_TOPIC_ROOT`) and
`model_profile.h` (`kModel`).

| Module | Owns |
|---|---|
| `signal_route.h` | `SignalRoute`/`resolveSignalRoute()`, `isWpmw2Model()`, `elsterNameHash()`, `signalKey()` (pure C++) |
| `mqtt_publish.h` | `mqttPublish()`, failure counter, publish-on-change state cache, `publishMqttState()` |
| `can_io.h` | frame decode (`processCanMessage()`), `readSignal()`/`writeSignal()`, read-back queue, `disableCanRequests`, frame observers `canRxObserver`/`canTxObserver`, clock/date writes |
| `can_debug.h` | raw CAN frame stream over MQTT (`canDebugCommand()`, `canDebugStep()`), opt-in via `can_debug: "true"` (stream off by default) |
| `paced_writes.h` | `queuePacedWrite()`/`pacedWriteStep()`: one write at a time, checked by read-back |
| `discovery.h` | device blocks (`HA_MAIN_DEVICE_JSON`, `memberDeviceJson()`), `defaultEntityIdJson()`, discovery of raw signals and calculated sensors |
| `bus_scan.h` | bus scan (`startScan()`, `scanStep()`, `handleScanFrame()`) |
| `request_manager.h` | polling of `signalRequests[]` (`processSignalRequests()`), back-off |
| `calculated.h` | `calculatedSensors[]`/`CalcIdx`, date/time, Delta T, compressor, summer mode, diagnostics, `processCalculatedSensors()` |
| `energy.h` | COP and the combined energy values |
| `writables.h` | `writableNumbers[]`/`writableSelects[]`, `writableNumberApplies()`, number/select/climate discovery, device limits (`numberLimitsStep()`, `handleLimitAnswer()`, `clampToNumberLimits()`) |
| `sg_ready_io.h` | ESPHome adapter of `SgReadyController` (NVS, paced writes, MQTT), SG Ready commands |
| `time_programs.h` | WPMW II time programs in HA (one text per day) |
| `set_commands.h` | `handleSetCommand()`: every `<root>/<A>/<B>/set` topic |
| `fault_list.h` | manager fault list, `faultRemedy()` |
| `republish.h` | discovery and state republish after an MQTT connect or HA birth message |

**Adding code:** put it in the module that owns the data; `ha-stiebel-control.h` stays below
800 lines. A module may include only modules higher up in this table, never one that includes
it. A new header goes into `common.yaml` `includes:` (ESPHome copies only listed files) and into
this table and `docs/ARCHITECTURE.md`. `python3 tests/check_modules.py` checks cycles, the size
of `ha-stiebel-control.h` and the `includes:` list (CI).

### How a signal reaches Home Assistant
1. A row in `signal_requests_<model>.h` makes the request manager poll it
   (`processSignalRequests()` in `request_manager.h`, round-robin, one request per second).
2. The answer is decoded in `processCanMessage()` (`can_io.h`) with the type from `ElsterTable.h`
   and handled by `updateSensor()`: MQTT discovery on first value, then publish on change
   (`<root>/<MEMBER>/<SIGNAL>/state`, root `HA_TOPIC_ROOT` from `device_name`, default
   `heatingpump`; never write the root as a literal). Names, units and device classes come from
   `ElsterMetaTable` (`LNAME_*`), otherwise from type defaults.
3. Calculated values live in `calculatedSensors[]` in `calculated.h`; address rows by the
   `CalcIdx` enum (`CALC_IDX_*`, same order as the table, checked by a static_assert), never by
   number. `onDemand` rows are announced with their first value. Discovery payloads use
   `HA_MAIN_DEVICE_JSON` / `memberDeviceJson()` for the device block.
4. Writable settings are `writableNumbers[]` / `writableSelects[]` (`writables.h`). Every
   `<root>/<A>/<B>/set` command goes through `handleSetCommand()` (`set_commands.h`), which finds
   the row by signal name and member and checks `writableNumberApplies()` and min/max; adding a
   writable entity only touches its row.
   Writes go through `writeSignal()` (read-back is scheduled automatically); SG Ready and time
   program writes go through the paced queue (`queuePacedWrite()`), one at a time with read-back.

**Adding a sensor:** find the exact signal name in `elster/ElsterTable.h`, add a request row to the
model table(s) with the right member (`cm_manager`, `cm_kessel`, `cm_heizmodul`, …) and frequency.
For HA metadata (unit, device class, binary payloads) set `hasMetadata = true` and add an
`ElsterMetaTable` row plus an `LNAME_*` macro. Binary sensors need `payloadOn`/`payloadOff`.

### Models (`model_profile.h`)
The build flag `-DHA_DEVICE_MODEL=<model>` (from `device_model`) is evaluated **only** in
`model_profile.h`. It defines the preprocessor switches for the signal table and language files
(`HA_CONTROLLER_WPMW2`, `HA_CONTROLLER_WPM3`, `HA_ELSTER_WPL_FAMILY`) and `kModel`, a constexpr
`ModelProfile` with flags such as `wpmw2`, `wpmw2WritesVerified`, `canBRoutes`, `canBDhwRoutes`,
`perMemberEnergy`. Code reads `kModel.<flag>`; never compare `HA_DEVICE_MODEL_STR` with a model name
and never add `#if defined(HA_MODEL_...)` elsewhere.

**Adding a model:** create `signal_requests_<model>.h` (start with `SIGNAL_REQUESTS_BASE`), a thin
`<model>.yaml` that includes it, `tests/models/<model>_smoke.json` (or `{"same_as": "<model>"}`),
add the model to the CI test matrix, and give it a profile in `model_profile.h` if it differs from
`kProfileGeneric`. See `docs/CONTRIBUTING.md`.

WPMW II specifics: heating circuit 2 lives on the virtual member `MISCHERMODUL_3` (0x602),
including its room setpoints, heating curve and program (`HEIZPROG_1_*`).

### File rules
- `heatingpump.yaml`: board/model selection only.
- Board files: hardware only (flash, PSRAM, framework). CAN files: CAN only, keep `id: my_can`.
- `common.yaml`: board-agnostic; no pins, no model names (use `kModel`).
- Model YAML: only the `includes:` entry of its request table.
- Signal protocol names and option values written to the bus stay German in every language
  (`"Tagbetrieb"`, `LNAME_OPT_*`); only display names are translated.

### i18n
`language` substitution → `-DHA_LANGUAGE_${language}` → `language_select.h` picks `lang_en.h` or
`lang_base.h`. `lang_base.h` is the single source of German strings; `lang_en.h` overrides with
`#undef`/`#define`. `common.yaml` lists all three headers in `includes:` so ESPHome copies them.

### Testing
- `make test` — native Catch2 tests; `make test MODEL_TEST_FLAGS=-DHA_DEVICE_MODEL=wpl13e` builds the
  tests with that model's tables and profile (CI runs every model).
- `make check` — compile both board variants; keep flash and RAM below 50 %.
- After every OTA flash: `make smoke-test` must pass (`tests/models/_common_smoke.json` +
  `<device_model>_smoke.json`).
- `python3 tests/check_doc_paths.py` — every repo path named in these docs must exist (CI).
- `python3 tests/check_modules.py` — no include cycle between the firmware headers,
  `ha-stiebel-control.h` below 800 lines, every included module listed in `common.yaml` (CI).

### Pitfalls
- Template sensor lambdas that only trigger reads return `{}`; the C++ side publishes values.
- Do not change `on_frame` in the CAN files; every frame goes through `processAndUpdate()`.
- Check `unknown`/`unavailable` in HA templates.
- The `archive/` folder is reference only.

### Releasing
1. Bump `esphome: project: version:` in `common.yaml`.
2. Move `[Unreleased]` in `CHANGELOG.md` to the new version (with upgrade notes if needed).
3. Commit `release: vX.Y.Z`, tag `vX.Y.Z`; CI builds and creates the GitHub release.

### Development commands

Run these `make` targets from the repo root:

| Target | What it does |
|--------|-------------|
| `make compile` | Compile the ESP32-S3 variant (default, recommended hardware) |
| `make compile-esp32` | Compile the classic ESP32 (esp32dev, 4 MB)/MCP2515 variant |
| `make compile-s2` | Deprecated alias config for the same board (`board_esp32s2.yaml` → `board_esp32.yaml`) |
| `make check` | Compile both variants — full quality gate |
| `make logs` | Stream live logs from connected device (ESP32-S3 config) |
| `make upload` | Compile and OTA-flash to production device (heatingpump.local) |
| `make config` | Dump merged YAML (useful for debugging package includes) |
| `make smoke-test` | MQTT regression test: verify all required signals appear within 120s |
| `make capture-baseline` | Capture new MQTT baseline for current model (300s, overwrites model file) |
| `make test` | Build and run the C++ unit tests (Catch2, no hardware) |
| `make mutate` | Mutation testing (Mull) of the C++ lines changed against `origin/main` |
| `make mutate-all` | Mutation testing of all C++ logic (slow; the weekly CI workflow runs it) |

**Before uploading**: always run `make check` first. Verify flash usage stays <50% in the compile output.

### Design-first changes and tests

For every change to the C++ logic (`esphome/ha-stiebel-control/*.h`, `elster/`) or to `tests/`:

1. Write down the design before writing code: the behavior that changes, the signals, CAN
   members and functions involved. Put it in the issue or pull request description; for a small
   change, the commit message is enough.
2. Derive the test cases from the specification: the Elster signal definitions in `ElsterTable.h`,
   the protocol rules (frame layout, index encoding, value types), the manuals in `manuals/`, the
   issue or a recorded CAN frame. Cover the boundaries the specification names.
3. Implement the change together with those tests. A one-test-at-a-time red/green cycle is not
   required.
4. Run `make test`, and `make check` before a firmware change is merged or flashed.

A bug fix starts with a regression test that reproduces the bug, wherever the logic is
unit-testable. Run it and confirm it fails for the reason the bug describes, not because of a
build error. Then fix the bug and commit the test with the fix.

Write tests that can fail:

- Take expected values from the specification (protocol tables, manuals, a recorded frame),
  never from running the implementation and copying its output. A test whose expected value came
  from the code under test only proves that the code does what it does.
- Assert on observable results: the decoded value, the CAN frame sent, the published state.
- Changing an expected value in an existing test requires a reason in the commit message: name the
  specification, manual page or recorded frame that makes the new value correct.
- Compiler warnings and lint rules (`-Wall -Wextra`, yamllint, pre-commit) are relaxed only as a
  justified exception, with the reason in the commit message. Otherwise refactor.

Line coverage shows which code ran, not whether a test notices a wrong result. Check test strength
with mutation testing: `make mutate` mutates the C++ lines changed against `origin/main`
(`MUTATE_BASE=<ref>` for another base) and lists surviving mutants in `.mull/mutation.txt`. A
survivor in changed code points to a missing or weak assertion. Mull mutates the C++ code under
`esphome/ha-stiebel-control/`, never `tests/` (scope in `mull.yml`). YAML is not mutated.
`.github/workflows/mutation.yml` runs `make mutate-all` weekly and reports the score; it does not
block pull requests.

Mull 0.38.0 needs the clang of the LLVM version its plugin was built for. On Ubuntu 24.04 install
the `Mull-0.38.0-ubuntu-amd64-24.04.deb` and `Mull-18-0.38.0-LLVM-18.1.3-ubuntu-amd64-24.04.deb`
release packages; the defaults (`clang++-18`, `/usr/lib/mull-ir-frontend-18`, `mull-runner`) then
match. On macOS unpack `Mull-0.38.0-macOS-*.zip` and the `Mull-<N>-...-macOS-*.zip` whose LLVM
major version `<N>` matches Homebrew `llvm`, then set `MULL_CXX=$(brew --prefix llvm)/bin/clang++`,
`MULL_IR_FRONTEND=<unpacked>/lib/mull-ir-frontend-<N>` and `MULL_RUNNER=<unpacked>/bin/mull-runner`.

### After OTA flash — mandatory checklist

After every `make upload`, run these steps in order:

1. Watch `heatingpump/status` go `offline` → `online` (within ~60s):
   ```bash
   mosquitto_sub -h <mqtt-broker> -u <mqtt-user> \
     --pw "$(grep mqtt_password esphome/secrets.yaml | sed 's/mqtt_password: *//')" \
     -t "heatingpump/status" -v
   ```
2. Run the MQTT smoke test — **must pass before considering the flash successful**:
   ```bash
   make smoke-test
   ```
3. Check HA: `sensor.heatingpump_sg_ready_aktueller_zustand` still shows a value.

If `make smoke-test` fails, check `heatingpump/debug` for errors and investigate before declaring success.

### Secrets

`esphome/secrets.yaml` contains real WiFi/MQTT credentials — it is gitignored and must **never** be committed. For CI, a credential-free stub lives at `esphome/ci_secrets.yaml`.

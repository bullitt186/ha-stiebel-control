# AGENTS.md

<!-- repo-kit:begin v2 (managed, do not edit; https://github.com/bullitt186/repo-kit) -->
## How to work in this repository (all coding agents)

This block is shared by all repositories of this maintainer and kept in sync by
[repo-kit](https://github.com/bullitt186/repo-kit). Repository-specific
instructions follow below it. They add detail; where they contradict this block,
this block wins, unless an ADR in `docs/adr/` records the exception.

A pull request that follows this block passes review without rework. The
maintainer reviews against the same rules: the **Definition of Done** in
`CONTRIBUTING.md`.

### Build the right thing

1. Read this file to the end, `CONTRIBUTING.md`, `docs/SCOPE.md` and the
   architecture decision records in `docs/adr/` (start with `docs/adr/README.md`).
   An *Accepted* ADR and the scope are binding. If your change needs a different
   decision, write a new ADR that supersedes the old one, in the same pull request.
2. Work from an issue. A feature or behaviour change needs an issue the maintainer
   has labelled `accepted`; if there is none, open one, describe the problem (not
   only the solution) and wait. Only a **small fix** may skip the issue: it repairs
   documented behaviour, changes fewer than about 50 lines of code, and adds no
   dependency, interface, setting or file format.
3. Build what the issue asks for and nothing else. Ideas you have on the way become
   a new issue, not part of this pull request. No reformatting, renaming or
   refactoring that the change does not need.
4. Prefer the smallest complete change: reuse what the repository already has
   (helpers, patterns, dependencies) before adding anything; no new dependency for
   a few lines. Fix the root cause, not the symptom.
5. Never guess protocol values, API fields or hardware behaviour. Take them from a
   source and name it; mark anything unconfirmed as an assumption.

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
- reverses or replaces an earlier ADR, or changes `docs/SCOPE.md`.

Bug fixes and refactorings inside existing decisions need no ADR.

### Tests

Design the test cases from the specification, then implement the change together
with them. A one-test-at-a-time red/green cycle is not required.

- A bug fix starts with a regression test that reproduces the bug. Run it and
  confirm it fails **for the reason the bug describes**, not because of a build,
  import or signature error. Then fix the bug and commit the test with the fix.
- Expected values come from the specification: documentation, a protocol table,
  a manual, the issue, or a recorded real payload. Never from running the code
  under test and copying its output; such a test only proves the code does what
  it does.
- Assert observable results (output, state, sent request, raised error), not only
  that something was called.
- Tests are fast, deterministic and offline. A flaky test is a bug to fix, not to
  re-run.
- Changing an expected value in an existing test needs a reason in the commit
  message that names the source of the new value.
- Coverage shows what ran, not whether a test notices a wrong result. Run mutation
  testing on the code you changed (see Commands below); kill a surviving mutant
  with a test, or say in the pull request why it is equivalent.
- Do not weaken lint rules, thresholds or checks to make a change pass; fix the
  code. A justified exception gets a comment next to it and a note in the commit
  message. Thresholds are only ever raised.

### Documentation

Documentation changes in the same pull request as the behaviour it describes:
README, `docs/`, configuration examples, the ADRs and `CHANGELOG.md`. Outdated
documentation counts as a bug. Relative links in Markdown must point to files
that exist.

Changelog entries go under `[Unreleased]`, in the Keep a Changelog sections
(Added, Changed, Fixed, Removed, Security). Write them for the user of the
project: what changes for them and what they have to do, not which functions
moved. A breaking change gets an entry under "Upgrade notes" with the steps.
Name outside contributors and link their issue or pull request. Releases, tags
and version numbers are the maintainer's
([release standard](https://github.com/bullitt186/repo-kit/blob/main/docs/RELEASING.md)).

### Commits and branches

- One branch per topic from the current default branch; never commit to the
  default branch. Agents working in parallel use their own git worktree; never
  run checkout, switch, reset, stash or clean in a checkout someone else uses.
- Commit each completed step with a Conventional Commits message (`feat:`,
  `fix:`, `docs:`, `test:`, `refactor:`, `chore:`, `ci:`); the body says why.
  Code, comments, commit messages and pull requests are in English.
- Reference the issue: `Closes #N` on GitHub; `Refs #N` on Forgejo, which closes
  an issue as soon as a commit with `Closes` is pushed.
- Never commit or push on red; never use `--no-verify` or skip hooks; treat every
  hook finding as work to do.

### Before you open the pull request

- Run the repository's **fast check** before every commit and the **full check**
  when your change can affect what it covers (see Commands below). Both must pass.
- Merge or rebase onto the current default branch and run the checks again.
- Fill in the pull request template completely; tick only what you did and say
  why anything stays unticked.
- After pushing, watch CI yourself. A red check on your pull request is your work,
  also when it looks unrelated.
- No secrets, tokens, personal data, local paths, host names or IP addresses in
  code, tests, fixtures, logs, commits or pull request text.

### Continuous integration

CI follows the repo-kit [CI standard](https://github.com/bullitt186/repo-kit/blob/main/docs/CI_STANDARD.md):
tests, lint and format, type check, build, secret scan, dependency review, static
analysis and `PR hygiene`. PR hygiene fails when code changes come without a test
change or a `CHANGELOG.md` entry, when `docs/SCOPE.md` changes without an ADR,
when an ADR is malformed or missing from the index, when a changed Markdown file
has a broken relative link, when code from an outside contributor links no issue,
or when a pull request changes more than about 800 lines of code and tests. Only
the maintainer can waive a rule (labels `no-test-change`, `skip-changelog`,
`large-change`). Do not ask for a waiver when a test, entry or split is possible.

### Respect

Do not change CI configuration, branch protection, release workflows, licence,
`docs/SCOPE.md` or this block unless the issue asks for it. Maintainer decisions
recorded in issues and ADRs are not up for silent change.
<!-- repo-kit:end -->

## This repository

### Purpose

Scope and non-goals: [docs/SCOPE.md](docs/SCOPE.md).

ESPHome firmware plus Home Assistant integration for Stiebel Eltron / Tecalor heat pumps on the
CAN bus (Elster protocol). The ESP32 acts as bus member `PC` (0x680), polls signals, publishes them
via MQTT discovery and writes settings. Two hardware variants, both on ESP-IDF: ESP32-S3 with the
built-in TWAI controller (recommended) and a classic ESP32 (`esp32dev`) with an MCP2515.

### Layout

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

#### Firmware modules (`esphome/ha-stiebel-control/`, #109)

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

#### How a signal reaches Home Assistant

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

#### Models (`model_profile.h`)

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

#### i18n

`language` substitution → `-DHA_LANGUAGE_${language}` → `language_select.h` picks `lang_en.h` or
`lang_base.h`. `lang_base.h` is the single source of German strings; `lang_en.h` overrides with
`#undef`/`#define`. `common.yaml` lists all three headers in `includes:` so ESPHome copies them.

### Commands

| Task | Command |
|---|---|
| Install / set up | a C++17 compiler (`g++`) and `make` for the tests; Python 3 with `pip install -r requirements-ci.txt` (ESPHome with its paho-mqtt, yamllint and clang-format in the versions CI uses) for `make check` and the checks below; `shellcheck` from the system package manager |
| Fast check (before every commit, also the stop hook) | `make test` (Catch2, default model; the stop hook `scripts/agent-stop-check.sh` adds the MQTT manifest tests, `git diff --check`, yamllint, `python3 tests/check_format.py` (shellcheck and clang-format) and `esphome config` of changed firmware) |
| Full check | `make check` (unit tests and compile of both board variants; keep flash and RAM below 50 %), plus `python3 tests/check_doc_paths.py` and `python3 tests/check_modules.py`; CI also runs `make test MODEL_TEST_FLAGS=-DHA_DEVICE_MODEL=<model>` for every model |
| Mutation testing of changed code | `make mutate` (Mull, the C++ lines changed against `origin/main`; set-up below) |

More `make` targets, run from the repository root:

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

- `make test MODEL_TEST_FLAGS=-DHA_DEVICE_MODEL=wpl13e` builds the tests with that model's tables
  and profile (CI runs every model).
- `python3 tests/check_doc_paths.py`: every repository path named in these docs must exist (CI).
- `python3 tests/check_modules.py`: no include cycle between the firmware headers,
  `ha-stiebel-control.h` below 800 lines, every included module listed in `common.yaml` (CI).

#### Mutation testing set-up

`make mutate` mutates the C++ lines changed against `origin/main` (`MUTATE_BASE=<ref>` for
another base) and lists surviving mutants in `.mull/mutation.txt`. Mull mutates the C++ code under
`esphome/ha-stiebel-control/`, never `tests/` (scope in `mull.yml`). YAML is not mutated.
`.github/workflows/mutation.yml` runs `make mutate-all` weekly and reports the score; it does not
block pull requests.

Mull 0.38.0 needs the clang of the LLVM version its plugin was built for. On Ubuntu 24.04 install
the `Mull-0.38.0-ubuntu-amd64-24.04.deb` and `Mull-18-0.38.0-LLVM-18.1.3-ubuntu-amd64-24.04.deb`
release packages; the defaults (`clang++-18`, `/usr/lib/mull-ir-frontend-18`, `mull-runner`) then
match. On macOS unpack `Mull-0.38.0-macOS-*.zip` and the `Mull-<N>-...-macOS-*.zip` whose LLVM
major version `<N>` matches Homebrew `llvm`, then set `MULL_CXX=$(brew --prefix llvm)/bin/clang++`,
`MULL_IR_FRONTEND=<unpacked>/lib/mull-ir-frontend-<N>` and `MULL_RUNNER=<unpacked>/bin/mull-runner`.

### Rules specific to this repository

#### File rules

- `heatingpump.yaml`: board/model selection only.
- Board files: hardware only (flash, PSRAM, framework). CAN files: CAN only, keep `id: my_can`.
- `common.yaml`: board-agnostic; no pins, no model names (use `kModel`).
- Model YAML: only the `includes:` entry of its request table.
- Signal protocol names and option values written to the bus stay German in every language
  (`"Tagbetrieb"`, `LNAME_OPT_*`); only display names are translated.

#### Specifications for tests

The specification for expected values is: the Elster signal definitions in `ElsterTable.h`, the
protocol rules (frame layout, index encoding, value types), the manuals in `manuals/`, the issue
or a recorded CAN frame. Cover the boundaries the specification names. Regression tests apply
wherever the logic is unit-testable; YAML and hardware behaviour are checked by `make check` and
`make smoke-test`. Compiler warnings and lint rules (`-Wall -Wextra`, yamllint, pre-commit) are
relaxed only as a justified exception, with the reason in the commit message.

#### CI standard exceptions

- Type check: C++ has no separate type checker; the compiler (`-Wall -Wextra` in the tests, the
  ESPHome compile in CI) is the type check.
- Format: clang-format (`.clang-format`) checks only the C++ lines a change touches
  (`python3 tests/check_format.py`, in CI against the base branch). The existing code keeps its
  hand-aligned signal tables and macros and is not reformatted in bulk; vendored code
  (`tests/catch2/`, `esphome/ha-stiebel-control/elster/`) is not checked. Format new lines with
  `git clang-format origin/main`.

#### Pitfalls

- Template sensor lambdas that only trigger reads return `{}`; the C++ side publishes values.
- Do not change `on_frame` in the CAN files; every frame goes through `processAndUpdate()`.
- Check `unknown`/`unavailable` in HA templates.
- The `archive/` folder is reference only.

#### After OTA flash — mandatory checklist

Before `make upload`, run `make check` and verify that flash usage stays below 50 % in the
compile output. After every `make upload`, run these steps in order:

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

#### Secrets

`esphome/secrets.yaml` contains real WiFi/MQTT credentials — it is gitignored and must **never** be committed. For CI, a credential-free stub lives at `esphome/ci_secrets.yaml`.

#### Releasing

Releases follow the repo-kit [release standard](https://github.com/bullitt186/repo-kit/blob/main/docs/RELEASING.md);
the version and the tag are the maintainer's. Repository specifics:

1. The release pull request bumps `esphome: project: version:` in `common.yaml` and moves
   `[Unreleased]` in `CHANGELOG.md` to the new version (with upgrade notes if needed). Write that
   section in the release-notes structure of the standard (summary, thank you, what's new, fixes,
   under the hood, upgrade notes): the release job publishes it verbatim as the release body.
2. After the maintainer's acceptance, the merge commit is tagged `vX.Y.Z`. CI compiles both
   variants and creates the GitHub release from the `CHANGELOG.md` section. No firmware binaries
   are attached: they would carry placeholder credentials, so users build with their own
   `secrets.yaml`.

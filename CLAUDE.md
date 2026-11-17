@.github/copilot-instructions.md

---

## Claude Code — Development Commands

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

## Design-first changes and tests

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

A Claude Code Stop hook (`scripts/agent-stop-check.sh`, registered in `.claude/settings.json`)
runs the fast local checks when the working tree has changes: `make test`, the MQTT manifest
tests, `git diff --check`, yamllint and, if firmware files changed, `esphome config` of the CI
configs with the CI stub secrets. It never compiles firmware or talks to the device, the broker or
Home Assistant. If a check fails, fix the finding before stopping.

## After OTA flash — mandatory checklist

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

## Secrets

`esphome/secrets.yaml` contains real WiFi/MQTT credentials — it is gitignored and must **never** be committed. For CI, a credential-free stub lives at `esphome/ci_secrets.yaml`.

## MCP Tools Available

- `mcp__mqtt__*` — publish/receive MQTT messages to verify sensor values after a firmware change
- `mcp__home-assistant__*` — check entity states in HA after deploy

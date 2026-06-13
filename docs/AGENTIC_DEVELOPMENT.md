# Agentic Development

This project is set up for agentic development with Claude Code and GitHub Copilot.
Both agents share a single source of truth and a fully automated quality pipeline.

---

## How the Harness Works

### Single source of truth

`.github/copilot-instructions.md` is the authoritative instructions file for all AI agents.

`CLAUDE.md` (at repo root) imports it with a single line:
```
@.github/copilot-instructions.md
```

**Edit `.github/copilot-instructions.md` — never `CLAUDE.md` directly.** One edit keeps
both Copilot and Claude Code in sync.

### Pre-approved permissions

`.claude/settings.json` pre-approves the routine operations agents need:
- `esphome compile/logs/run/config`
- `make *`
- `git log/diff/status/add/commit`

This means Claude Code can run `make check && make upload && make smoke-test` without
prompting for permission on each step.

### Make targets — the agent's vocabulary

All development tasks have named `make` targets. Agents are expected to use these rather
than raw commands:

| Target | What it does |
|--------|-------------|
| `make compile` | Compile ESP32-S3 variant |
| `make compile-s2` | Compile ESP32-S2/MCP2515 variant |
| `make check` | Compile both variants — full quality gate |
| `make test` | Run native Catch2 unit tests (no hardware) |
| `make logs` | Stream live device logs |
| `make upload` | Compile + OTA flash to production device |
| `make config` | Dump merged YAML (useful for debugging) |
| `make smoke-test` | MQTT regression test: 120s observation window |
| `make capture-baseline` | Capture new MQTT baseline for current model (300s) |

---

## Setting Up Claude Code

1. Install the Claude Code CLI
2. Clone the repo: `git clone https://github.com/bullitt186/ha-stiebel-control.git`
3. `.claude/settings.json` is picked up automatically — no extra configuration
4. Create your secrets file: `cp esphome/secrets.yaml.example esphome/secrets.yaml`
5. Start a Claude Code session in the repo root — `CLAUDE.md` is auto-injected

---

## Agentic Workflows

### Add a new sensor

Tell Claude: *"Add signal HEIZKURVE from cm_manager, polling every 10 minutes"*

Claude will:
1. Search `ElsterTable.h` to confirm the signal exists
2. Add it to `signal_requests_wpl13e.h`
3. Run `make config` to verify the YAML parses
4. Run `make compile` to verify the C++ compiles

### Flash and verify

Tell Claude: *"Compile and flash, then verify it works"*

Claude will run:
```bash
make check && make upload && make smoke-test
```

The smoke test asserts all required MQTT topics appear within 120 seconds. Exit 0 = pass.

### Update the MQTT baseline after intentional changes

Tell Claude: *"Capture a new baseline after the signal table change"*

Claude will run `make capture-baseline` (300-second capture), which overwrites
`tests/models/wpl13e_smoke.json` with the freshly observed topics.

### Investigate missing HA entities

Tell Claude: *"Check why sensor.X is unavailable in HA"*

Claude will:
1. Check `mcp__home-assistant__ha_get_state` for the entity
2. Check `mcp__mqtt__receive_message` for the underlying MQTT topic
3. Check `heatingpump/debug` for firmware-level errors
4. Diagnose and propose a fix

---

## Rules for Agents

These rules are also in `.github/copilot-instructions.md`, but are repeated here for
discoverability:

1. **Always run `make check` before committing** any change that touches firmware files
2. **Always run `make smoke-test` after `make upload`** — do not declare a flash successful until it passes
3. **Never commit `esphome/secrets.yaml`** — it is gitignored and contains real credentials
4. **`api_encryption_key` must be in `secrets.yaml`** and match what HA's ESPHome integration stores — if it changes, the HA integration entry must be re-adopted
5. **Edit `.github/copilot-instructions.md`**, not `CLAUDE.md`, for persistent agent guidance
6. **Bump `version:` in `common.yaml`** before tagging a release

---

## Post-OTA Checklist

After every `make upload`:

1. Watch `heatingpump/status` transition `offline` → `online` (within ~60s):
   ```bash
   mosquitto_sub -h 192.168.30.10 -u iotuser \
     --pw "$(grep mqtt_password esphome/secrets.yaml | sed 's/.*: //')" \
     -t "heatingpump/status" -v
   ```
2. Run the smoke test — must pass:
   ```bash
   make smoke-test
   ```
3. Check HA: `sensor.heatingpump_sg_ready_aktueller_zustand` still shows a value

If the smoke test fails, check `heatingpump/debug` for errors before declaring success.

---

## CI Pipeline

Every push/PR to `main` runs:

| Job | What it checks |
|-----|---------------|
| `unit-tests` | Native Catch2 suite (138 tests, no hardware) |
| `compile-s3` | Full ESP32-S3 firmware compile |
| `compile-s2` | Full ESP32-S2/MCP2515 firmware compile |
| `validate-yaml` | YAML lint on all package files |

On `v*` tags, a `release` job additionally:
- Compiles both firmware variants
- Generates `manifest.json` with firmware URLs and MD5s
- Creates a GitHub Release with attached `.bin` files

The unit tests and YAML validation run in seconds. The firmware compile jobs take ~5 minutes each and require the full ESP-IDF toolchain to be downloaded on first run.

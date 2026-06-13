@.github/copilot-instructions.md

---

## Claude Code — Development Commands

Run these `make` targets from the repo root:

| Target | What it does |
|--------|-------------|
| `make compile` | Compile the ESP32-S3 variant (default, recommended hardware) |
| `make compile-s2` | Compile the ESP32-S2/MCP2515 variant |
| `make check` | Compile both variants — full quality gate |
| `make logs` | Stream live logs from connected device (ESP32-S3 config) |
| `make upload` | Compile and OTA-flash to production device at 192.168.30.107 |
| `make config` | Dump merged YAML (useful for debugging package includes) |
| `make smoke-test` | MQTT regression test: verify all required signals appear within 120s |
| `make capture-baseline` | Capture new MQTT baseline for current model (300s, overwrites model file) |

**Before uploading**: always run `make check` first. Verify flash usage stays <50% in the compile output.

## After OTA flash — mandatory checklist

After every `make upload`, run these steps in order:

1. Watch `heatingpump/status` go `offline` → `online` (within ~60s):
   ```bash
   mosquitto_sub -h 192.168.30.10 -u iotuser -P $(grep mqtt_password esphome/secrets.yaml | awk '{print $2}') \
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

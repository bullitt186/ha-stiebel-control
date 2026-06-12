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
| `make upload` | Compile and OTA-flash to connected device |
| `make config` | Dump merged YAML (useful for debugging package includes) |

**Before uploading**: always run `make check` first. Verify flash usage stays <50% in the compile output.

## Secrets

`esphome/secrets.yaml` contains real WiFi/MQTT credentials — it is gitignored and must **never** be committed. For CI, a credential-free stub lives at `esphome/ci_secrets.yaml`.

## MCP Tools Available

- `mcp__mqtt__*` — publish/receive MQTT messages to verify sensor values after a firmware change
- `mcp__home-assistant__*` — check entity states in HA after deploy

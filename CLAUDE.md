@AGENTS.md

## Claude Code only

A Claude Code Stop hook (`scripts/agent-stop-check.sh`, registered in `.claude/settings.json`)
runs the fast local checks when the working tree has changes: `make test`, the MQTT manifest
tests, `git diff --check`, yamllint and, if firmware files changed, `esphome config` of the CI
configs with the CI stub secrets. It never compiles firmware or talks to the device, the broker or
Home Assistant. If a check fails, fix the finding before stopping.

### MCP tools

- `mcp__mqtt__*` — publish/receive MQTT messages to verify sensor values after a firmware change
- `mcp__home-assistant__*` — check entity states in HA after deploy

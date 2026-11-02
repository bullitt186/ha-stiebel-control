# Diagnostic tooling (development / testing only)

These files are **not** part of the production firmware and are **not a supported
configuration**. They are used to reverse-engineer unknown CAN registers or to
test writes on a real heat pump.

| File | Purpose |
|------|---------|
| `heatingpump_scan.yaml` | ESPHome config for the diagnostic scan firmware |
| `scan.yaml` | thin model package that selects the scan request table |
| `signal_requests_scan.h` | request table (sweeps a signal set across CAN members) |
| `elster_table_split.py` | one-off converter of `ElsterTable.h` to the slim row + `ElsterMetaTable` layout (#57); `--verify` compares old and new tables for every model and language (`elster_table_dump.cpp`, `elster_row_text.h`). Kept for reference; the conversion is done, new signals are added by hand (docs/CONTRIBUTING.md "Signal table") |

## Usage

1. Copy `heatingpump_scan.yaml`, `scan.yaml` and `signal_requests_scan.h` next to
   the existing `ha-stiebel-control/` folder in your ESPHome config directory.
2. Compile and install (OTA) via the ESPHome Device Builder.
3. Watch the MQTT topics / logs to see which member answers which signal.

> **Warning:** the scan firmware actively queries the bus and can write e.g.
> `PROGRAMMSCHALTER` on a **live** heat pump. Use only with explicit approval and
> at your own risk.

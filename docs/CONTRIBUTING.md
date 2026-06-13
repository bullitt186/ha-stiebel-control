# Contributing

---

## Adding a New Heat Pump Model

Models are defined by two files. Everything else is shared.

### Step 1 — Create the signal request table

Create `esphome/ha-stiebel-control/signal_requests_yourmodel.h`:

```cpp
#ifndef SIGNAL_REQUESTS_YOURMODEL_H
#define SIGNAL_REQUESTS_YOURMODEL_H

#include "config.h"
#include "signal_requests_base.h"   // universal signals: date/time, EVU, COP counters

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // always include this first — do not duplicate these signals

    // Add your model-specific signals below.
    // Format: {"SIGNAL_NAME", FREQUENCY, CAN_MEMBER}
    //
    // Finding signal names:
    //   grep "AUSSENTEMP" esphome/ha-stiebel-control/elster/ElsterTable.h
    //
    // Which CAN member to use:
    //   Use cm_other if unsure — it queries KESSEL, MANAGER, and HEIZMODUL.
    //   Whichever member responds will appear automatically in HA.
    //   If you know the exact member from logs or manuals, use it directly.

    {"AUSSENTEMP",           FREQ_30S,   cm_other},    // outside temp (varies by model)
    {"SPEICHERISTTEMP",      FREQ_30S,   cm_kessel},   // DHW actual temp
    {"RUECKLAUFISTTEMP",     FREQ_30S,   cm_manager},  // return flow temp
    // add more signals...
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE =
    sizeof(signalRequests) / sizeof(SignalRequest);

#endif
```

### Step 2 — Create the model YAML package

Create `esphome/ha-stiebel-control/yourmodel.yaml`:

```yaml
#
#  YourModel Heat Pump — Model Package
#
esphome:
  includes:
    - ha-stiebel-control/signal_requests_yourmodel.h
# Add sensors here only if they are truly unique to your model.
# Generic sensors (COP, DHW temperatures, operating mode) are already in common.yaml.
```

### Step 3 — Register the model in the dispatcher

Edit `esphome/ha-stiebel-control/signal_requests_model.h` and add an `#elif` branch:

```cpp
#elif defined(HA_MODEL_YOURMODEL)
#  include "signal_requests_yourmodel.h"
```

### Step 4 — Use your model in `heatingpump.yaml`

```yaml
substitutions:
  device_model: "yourmodel"
  device_model_flag: "YOURMODEL"   # must match the HA_MODEL_* flag in the dispatcher

packages:
  board:   !include ha-stiebel-control/board_esp32s3.yaml
  can:     !include ha-stiebel-control/can_esp32.yaml
  base:    !include ha-stiebel-control/common.yaml
  sensors: !include ha-stiebel-control/yourmodel.yaml
```

### Step 5 — Create a smoke test manifest

Create `tests/models/yourmodel_smoke.json`:

```json
{
  "description": "Required MQTT topics for YourModel. Verified on hardware YYYY-MM-DD.",
  "required_topics": [
    "heatingpump/KESSEL/SPEICHERISTTEMP/state",
    "heatingpump/MANAGER/RUECKLAUFISTTEMP/state"
  ]
}
```

List only signals you have verified on real hardware. Mark unverified signals in a comment.

### Step 6 — Test

```bash
make config    # YAML parses cleanly
make compile   # firmware compiles
make smoke-test DEVICE_MODEL=yourmodel  # after flashing — MQTT regression
```

### Step 7 — Submit a PR

Include:
- `signal_requests_yourmodel.h` and `yourmodel.yaml`
- The `signal_requests_model.h` dispatcher update
- `tests/models/yourmodel_smoke.json`
- ESPHome log snippet showing signals responding from your heat pump
- Heat pump model/variant info in the PR description

---

## Tips for Finding Signals

- Search `ElsterTable.h` by German keyword: `grep -i "aussen" esphome/ha-stiebel-control/elster/ElsterTable.h`
- Consult the PDF manuals in `manuals/` for signal meanings and expected sources
- Use `cm_other` for signals where you're unsure which CAN member responds — the responding member appears in logs: `[processCanMessage] KESSEL (0x180): SPEICHERISTTEMP: 51.8`
- Signals that don't respond on your pump are silently ignored — no harm done

---

## Other Contributions

- **Signal metadata** — add `friendlyName`, `unit`, `deviceClass`, `icon` to entries in `ElsterTable.h`
- **Dashboard examples** — add to `packages/` or link from the docs
- **SG Ready automations** — see `packages/sg_ready_automation_example.yaml` as a template
- **Documentation fixes** — edit files in `docs/` or `README.md`

For significant changes, open an issue first to discuss the approach.

# SG Ready — PV Surplus Integration

ESPHome-native Smart Grid Ready (SG Ready) control for Stiebel Eltron heat pumps.
Automatically adjusts heat pump operation based on PV surplus availability.

---

## What is SG Ready?

SG Ready is a standard for smart heat pump control with 4 operating states:

| State | Name | Typical PV situation | Heat pump action |
|-------|------|---------------------|-----------------|
| **1** | EVU Sperre | Grid demand / no PV | **Bereitschaft** — standby only |
| **2** | Normal | Standard operation | **Tagbetrieb** — comfort mode |
| **3** | Empfohlen | 1–2 kW surplus available | **Tagbetrieb** + DHW boost + room boost (+1°C) |
| **4** | Zwang | >2 kW surplus available | **Tagbetrieb** + larger DHW boost + room boost (+2°C) |

> **Important:** States 3 and 4 use `Tagbetrieb` (continuous comfort mode, ignores schedule)
> rather than `Warmwasser` — this keeps space heating active alongside DHW heating.

---

## Quick Start (5 minutes)

### 1. Flash the firmware

The SG Ready feature is built into the standard firmware. Just flash as normal:

```bash
make upload
make smoke-test
```

### 2. Verify new entities in HA

After flashing, these entities appear automatically:

- **Select**: `select.manager_sg_ready_zustand` — control SG Ready state
- **Number**: `number.manager_sg_ready_boost_zustand_3` — DHW boost for state 3 (default 5°C)
- **Number**: `number.manager_sg_ready_boost_zustand_4` — DHW boost for state 4 (default 8°C)
- **Sensor**: `sensor.heatingpump_sg_ready_aktueller_zustand` — read-only current state
- **Sensor**: `sensor.heatingpump_sg_ready_boost_state_3` / `_4` — read-only boost values

### 3. Create a HA automation

Map your PV system's SG Ready state to the heat pump. Example for E3DC:

```yaml
automation:
  - alias: "Heat pump: SG Ready sync"
    trigger:
      - platform: state
        entity_id: sensor.s10x_sg_ready_numeric
      - platform: homeassistant
        event: start
    condition:
      - condition: template
        value_template: >
          {{ states('sensor.s10x_sg_ready_numeric') | int(0) in [1,2,3,4] }}
    action:
      - service: select.select_option
        target:
          entity_id: select.manager_sg_ready_zustand
        data:
          option: >
            {% set s = states('sensor.s10x_sg_ready_numeric') | int %}
            {% if s == 1 %}1 - EVU Sperre
            {% elif s == 2 %}2 - Normal
            {% elif s == 3 %}3 - Empfohlen
            {% elif s == 4 %}4 - Zwang
            {% else %}2 - Normal{% endif %}
```

A ready-to-use example is in [`packages/sg_ready_automation_example.yaml`](packages/sg_ready_automation_example.yaml).

---

## How the Temperature Boost Works

The boost is applied **automatically by the firmware** — no extra HA automation needed.

When state 3 is activated:
1. Current DHW setpoint baseline is recorded (default: 48°C if not previously set)
2. `PROGRAMMSCHALTER` → `Tagbetrieb`
3. DHW setpoint → baseline + `boost_state3` (e.g. 48 + 5 = 53°C, capped at 60°C)
4. Room setpoint → baseline + 1°C (capped at 25°C)

When returning to state 2 (Normal):
1. `PROGRAMMSCHALTER` → `Tagbetrieb`
2. DHW setpoint → restored to baseline
3. Room setpoint → restored to baseline

State 1 (EVU Sperre):
1. Temperatures restored to baseline
2. `PROGRAMMSCHALTER` → `Bereitschaft`

Boost amounts are configurable via `number.manager_sg_ready_boost_zustand_3` and `_4`.
Values persist across firmware restarts (stored in ESP32 flash via NVS).

### Recommended boost values

| Season | State 3 | State 4 | Notes |
|--------|---------|---------|-------|
| Winter | 3–5°C | 6–8°C | DHW more important; heating runs anyway |
| Summer | 5–8°C | 8–12°C | DHW only mode; more boost headroom |

Maximum DHW temperature is typically 55–60°C per manufacturer — check your manual.

---

## System Architecture

```
┌─────────────────────────┐
│   PV System (e.g. E3DC) │
│   sensor.sg_ready_state │  (1–4)
└──────────────┬──────────┘
               │ HA Automation
               ▼
┌─────────────────────────┐
│  Home Assistant         │
│  select.manager_sg_...  │
└──────────────┬──────────┘
               │ MQTT heatingpump/MANAGER/SG_READY_STATE/set
               ▼
┌─────────────────────────┐
│  ESPHome ESP32          │
│  SgReadyController      │  ← applies boost, persists state
└──────────────┬──────────┘
               │ CAN Bus
               ▼
┌─────────────────────────┐
│  Stiebel Eltron         │
│  PROGRAMMSCHALTER       │
│  EINSTELL_SPEICHER...   │
└─────────────────────────┘
```

---

## Dashboard

```yaml
type: entities
title: SG Ready Status
entities:
  - entity: select.manager_sg_ready_zustand
    name: SG Ready State
  - entity: sensor.heatingpump_sg_ready_aktueller_zustand
    name: Current State (read-only)
  - entity: select.manager_programmschalter
    name: Operating Mode
  - entity: number.manager_sg_ready_boost_zustand_3
    name: Boost State 3 (°C)
  - entity: number.manager_sg_ready_boost_zustand_4
    name: Boost State 4 (°C)
  - entity: number.manager_speicher_soll_temperatur_einstellung
    name: DHW Setpoint
```

---

## Optimisation Tips

### Restrict to daytime only

```yaml
condition:
  - condition: time
    after: "07:00:00"
    before: "20:00:00"
```

### Avoid boosting when already hot

```yaml
condition:
  - condition: numeric_state
    entity_id: sensor.kessel_speicher_ist_temperatur
    below: 50
```

### State 4 timeout (prevent excessive runtime)

```yaml
- alias: "Heat pump: SG Ready state 4 timeout"
  trigger:
    - platform: state
      entity_id: select.manager_sg_ready_zustand
      to: "4 - Zwang"
      for: "01:00:00"
  action:
    - service: select.select_option
      target:
        entity_id: select.manager_sg_ready_zustand
      data:
        option: "2 - Normal"
```

---

## Hardware Simplification

If you previously used a Shelly relay or other hardware for EVU lock:

State 1 (`EVU Sperre`) now sends `Bereitschaft` directly via CAN — **the hardware relay
is no longer needed**. Remove it for a simpler, faster, more reliable setup.

---

## Troubleshooting

### Select doesn't appear in HA

1. Check MQTT: `homeassistant/select/heatingpump/stiebel_manager_sg_ready_state/config` should be retained
2. Check firmware logs for: `Discovery published for writable select: SG Ready Zustand`
3. Reload MQTT: Settings → Integrations → MQTT → Reload

### Heat pump doesn't change mode

1. Check logs: `[SG_READY] Applying SG Ready state 3`
2. Verify `select.manager_programmschalter` changes in HA
3. Check for CAN write errors in `heatingpump/debug`

### Boost not applied

The boost is applied automatically. If temperatures don't change:
1. Check the boost number entity shows a non-zero value
2. Wait 30–60 seconds for the heat pump's anti-cycling protection
3. Check logs: `[SG_READY] Applied DHW boost: 48.0 + 5.0 = 53.0°C`

---

## References

- [SG Ready standard (BWP)](https://www.waermepumpe.de/normen-technik/sg-ready/)
- Automation example: [`packages/sg_ready_automation_example.yaml`](packages/sg_ready_automation_example.yaml)
- Implementation: `esphome/ha-stiebel-control/sg_ready_controller.h`

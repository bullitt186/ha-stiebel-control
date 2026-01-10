# SG Ready Integration für ha-stiebel-control

ESPHome-native Smart Grid Ready (SG Ready) Steuerung für Stiebel Eltron Wärmepumpen mit PV-Überschussnutzung.

## 🚀 Schnellstart (5 Minuten)

### 1. Firmware flashen

```bash
cd esphome/ha-stiebel-control
esphome run wpl13e.yaml
```

### 2. Neue Entities in Home Assistant prüfen

Nach dem Flash erscheinen automatisch:
- **Select**: `select.manager_sg_ready_zustand`
- **Numbers**: `number.manager_sg_ready_boost_state_3` und `number.manager_sg_ready_boost_state_4`

### 3. Automation erstellen

```yaml
automation:
  - alias: SG Ready Sync
    trigger:
      - platform: state
        entity_id: sensor.s10x_sg_ready_numeric
    action:
      - service: select.select_option
        target:
          entity_id: select.manager_sg_ready_zustand
        data:
          option: >
            {% set s = states('sensor.s10x_sg_ready_numeric')|int(2) %}
            {{ ['1 - EVU Sperre','2 - Normal','3 - Empfohlen','4 - Zwang'][s-1] if s in [1,2,3,4] else '2 - Normal' }}
```

---

## 📖 Was ist SG Ready?

SG Ready ist ein Standard für intelligente Wärmepumpen-Steuerung im Smart Grid mit 4 Betriebszuständen:

| Zustand | Name | PV-Situation | Wärmepumpen-Reaktion |
|---------|------|--------------|---------------------|
| **1** | EVU Sperre | Netzbezug / Keine PV | **Bereitschaft** - Nur Notbetrieb |
| **2** | Normal | Normalbetrieb | **Automatik** - Programmierter Zeitplan |
| **3** | Empfohlen | 1-2 kW Überschuss | **Tagbetrieb** - Komfortmodus + Temp-Boost |
| **4** | Zwang | >2 kW Überschuss | **Tagbetrieb** - Komfortmodus + Max-Boost |

### Betriebsarten-Erklärung

- **Bereitschaft**: Standby, nur bei Bedarf
- **Automatik**: Folgt programmiertem Zeitplan (Tag-/Nachtabsenkung)
- **Tagbetrieb**: Dauerhaft Komfortmodus ohne Zeitplan (höherer Energieverbrauch)

**Wichtig**: Zustand 3 & 4 verwenden `Tagbetrieb` statt `Warmwasser`, damit die Raumheizung aktiv bleibt!

---

## ⚙️ Implementierung

### ESPHome Komponenten

**Select Dropdown**: `select.manager_sg_ready_zustand`
- Optionen: `1 - EVU Sperre`, `2 - Normal`, `3 - Empfohlen`, `4 - Zwang`
- MQTT Topics:
  - Command: `heatingpump/MANAGER/SG_READY_STATE/set`
  - State: `heatingpump/MANAGER/SG_READY_STATE/state`

**Temperature Boost Numbers**:
- `number.manager_sg_ready_boost_state_3` (0-10°C, Standard: 3°C)
- `number.manager_sg_ready_boost_state_4` (0-15°C, Standard: 5°C)

### Automatische Steuerung

Bei Zustandsänderung:
1. ESPHome empfängt MQTT-Befehl
2. `applySgReadyState()` wird aufgerufen
3. PROGRAMMSCHALTER wird via CAN gesetzt:
   - Zustand 1 → Bereitschaft
   - Zustand 2 → Automatik
   - Zustand 3 → Tagbetrieb
   - Zustand 4 → Tagbetrieb

### Temperatur-Boost (optional)

Die Boost-Werte werden in ESPHome gespeichert, aber **nicht automatisch angewendet**. Sie können in Home Assistant Automationen verwendet werden:

```yaml
# DHW Boost bei State 3
- alias: SG Ready State 3 DHW Boost
  trigger:
    - platform: state
      entity_id: select.manager_sg_ready_zustand
      to: "3 - Empfohlen"
  variables:
    current_temp: "{{ states('sensor.manager_einstell_speichersolltemp') | float(50) }}"
    boost: "{{ states('number.manager_sg_ready_boost_state_3') | float(3) }}"
  action:
    - service: mqtt.publish
      data:
        topic: heatingpump/MANAGER/EINSTELL_SPEICHERSOLLTEMP/set
        payload: "{{ current_temp + boost }}"

# Restore bei Verlassen von State 3
- alias: SG Ready State 3 Restore
  trigger:
    - platform: state
      entity_id: select.manager_sg_ready_zustand
      from: "3 - Empfohlen"
      to: ["1 - EVU Sperre", "2 - Normal"]
  variables:
    current_temp: "{{ states('sensor.manager_einstell_speichersolltemp') | float(50) }}"
    boost: "{{ states('number.manager_sg_ready_boost_state_3') | float(3) }}"
  action:
    - service: mqtt.publish
      data:
        topic: heatingpump/MANAGER/EINSTELL_SPEICHERSOLLTEMP/set
        payload: "{{ current_temp - boost }}"
```

Analog für State 4 mit `number.manager_sg_ready_boost_state_4`.

---

## 🏗️ System-Architektur

```
┌─────────────────────────┐
│   E3DC PV System        │
│   sensor.s10x_sg_ready  │ (1-4)
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Home Assistant         │
│  Automation             │
└───────────┬─────────────┘
            │ MQTT
            ▼
┌─────────────────────────┐
│  ESPHome ESP32          │
│  select.sg_ready_...    │
│  applySgReadyState()    │
└───────────┬─────────────┘
            │ CAN Bus
            ▼
┌─────────────────────────┐
│  Stiebel Eltron WP      │
│  PROGRAMMSCHALTER       │
└─────────────────────────┘
```

**Datenfluss**:
1. E3DC erkennt PV-Überschuss → `sensor.s10x_sg_ready_numeric` = 3
2. HA Automation triggert → `select.manager_sg_ready_zustand` = "3 - Empfohlen"
3. ESPHome empfängt via MQTT → `applySgReadyState(3)`
4. CAN Command → `PROGRAMMSCHALTER = "Tagbetrieb"`
5. Wärmepumpe wechselt in Komfortmodus

---

## 📊 Dashboard

```yaml
type: entities
title: SG Ready Status
entities:
  - entity: sensor.s10x_sg_ready_numeric
    name: E3DC Zustand
  - entity: select.manager_sg_ready_zustand
    name: WP SG Ready
  - entity: sensor.manager_programmschalter
    name: Betriebsart
  - entity: number.manager_sg_ready_boost_state_3
    name: Boost State 3
  - entity: number.manager_sg_ready_boost_state_4
    name: Boost State 4
  - entity: sensor.manager_einstell_speichersolltemp
    name: WW Soll
  - entity: sensor.kessel_speicheristtemp
    name: WW Ist
```

```yaml
type: history-graph
title: SG Ready Verlauf
entities:
  - sensor.s10x_sg_ready_numeric
  - sensor.manager_programmschalter
hours_to_show: 24
```

---

## 🔧 Konfiguration

### Boost-Werte anpassen

**Via UI**: Einfach die Number-Slider verstellen

**Via Automation**:
```yaml
- service: number.set_value
  target:
    entity_id: number.manager_sg_ready_boost_state_3
  data:
    value: 4  # 4°C Boost in State 3
```

### Empfohlene Boost-Werte

| Jahreszeit | State 3 | State 4 | Begründung |
|------------|---------|---------|------------|
| Winter | 2-3°C | 4-5°C | WW wichtiger, Heizung läuft sowieso |
| Sommer | 4-5°C | 7-10°C | Nur WW, mehr Boost möglich |

**Maximale WW-Temperatur beachten**: Meist 55-60°C laut Hersteller

### Manueller Override

Für Tests oder manuelle Steuerung:

```yaml
input_select:
  sg_ready_override:
    name: SG Ready Manuell
    options:
      - Auto (aus E3DC)
      - "1 - EVU Sperre"
      - "2 - Normal"
      - "3 - Empfohlen"
      - "4 - Zwang"
    initial: Auto (aus E3DC)

automation:
  - alias: SG Ready Manual Override
    trigger:
      - platform: state
        entity_id: input_select.sg_ready_override
    condition:
      - "{{ trigger.to_state.state != 'Auto (aus E3DC)' }}"
    action:
      - service: select.select_option
        target:
          entity_id: select.manager_sg_ready_zustand
        data:
          option: "{{ trigger.to_state.state }}"
```

---

## 🐛 Troubleshooting

### Select erscheint nicht in HA

1. **MQTT Explorer prüfen**: Topic `homeassistant/select/heatingpump/stiebel_manager_sg_ready_state/config` vorhanden?
2. **ESPHome Logs**: `Discovery published for writable select: SG Ready Zustand`
3. **HA MQTT neu laden**: Settings → Integrations → MQTT → Reload

### Wärmepumpe reagiert nicht

1. **Logs prüfen**:
   ```
   [SG_READY] Applying SG Ready state 3
   [SG_READY] State 3: Recommended - Tagbetrieb
   ```
2. **Sensor prüfen**: `sensor.manager_programmschalter` ändert sich?
3. **CAN Errors**: ESPHome Logs auf Write-Errors prüfen

### E3DC Sensor fehlt

- E3DC Integration in HA prüfen
- Alternative: `input_select` für manuelle Tests

### Falsche Betriebsart

Prüfe ob `sensor.s10x_sg_ready_numeric` wirklich Werte 1-4 liefert:
```yaml
{% set s = states('sensor.s10x_sg_ready_numeric') %}
{{ s }} - Typ: {{ s | int }}
```

---

## 💡 Optimierung

### Zeitliche Beschränkungen

Nur tagsüber State 3/4 zulassen:

```yaml
condition:
  - condition: time
    after: "07:00:00"
    before: "20:00:00"
```

### Temperatur-Grenzen

Nur boosten wenn WW nicht schon heiß genug:

```yaml
condition:
  - condition: numeric_state
    entity_id: sensor.kessel_speicheristtemp
    below: 50  # Nur unter 50°C boosten
```

### State 4 Timeout

Verhindere zu lange Laufzeiten:

```yaml
- alias: SG Ready State 4 Timeout
  trigger:
    - platform: state
      entity_id: select.manager_sg_ready_zustand
      to: "4 - Zwang"
      for: "01:00:00"  # Nach 1h
  action:
    - service: select.select_option
      data:
        option: "2 - Normal"
```

### Statistiken

```yaml
sensor:
  - platform: history_stats
    name: SG Ready State 3 Heute
    entity_id: select.manager_sg_ready_zustand
    state: "3 - Empfohlen"
    type: time
    start: "{{ now().replace(hour=0, minute=0, second=0) }}"
    end: "{{ now() }}"
```

---

## 🗑️ Hardware Vereinfachung

**Shelly EVU-Sperre kann entfernt werden!**

Die bisherige Hardware-Lösung (Shelly Relais am EVU-Eingang) wird nicht mehr benötigt:
- ✅ State 1 schaltet direkt via CAN in Bereitschaft
- ✅ Schneller (keine Netzwerk-Verzögerung)
- ✅ Zuverlässiger
- ✅ Ein Gerät weniger

---

## 📚 Hintergrund & Referenzen

### SG Ready Standard
- [BWP SG Ready Info](https://www.waermepumpe.de/normen-technik/sg-ready/)
- [E3DC Dokumentation](https://wohnen-mit-energie.de/data/documents/SG-Ready-Dokumentation_V1.70_2022-02-08-wme.pdf)

### Betriebsarten Details

**Automatik vs. Tagbetrieb**:
- **Automatik**: Zeitgesteuert, Tag-/Nachtabsenkung, energieeffizient
- **Tagbetrieb**: Dauerhaft Komfort-Temperatur, kein Zeitplan, höherer Verbrauch

**Warum nicht "Warmwasser"?**:
- Warmwasser-Modus deaktiviert die Raumheizung komplett
- Nur für reinen WW-Betrieb (z.B. Sommer)
- Für SG Ready ungeeignet, da Heizung ausfällt

### Implementierung

- **Code**: `esphome/ha-stiebel-control/ha-stiebel-control.h`
- **Config**: `esphome/ha-stiebel-control/common.yaml`
- **Beispiele**: `packages/sg_ready_automation_example.yaml`

---

## ✅ Erfolgsindikatoren

Die Integration funktioniert, wenn:

1. ✅ E3DC zeigt PV-Überschuss → SG Ready State wechselt zu 3/4
2. ✅ Wärmepumpe wechselt zu Tagbetrieb
3. ✅ WW-Temperatur steigt (bei Boost-Automation)
4. ✅ PV-Überschuss wird genutzt statt eingespeist
5. ✅ Logs zeigen saubere State-Transitions

**Typischer Ablauf**:
- E3DC Surplus-Detection: sofort
- HA Automation: 1-2 Sekunden
- ESPHome MQTT: <1 Sekunde
- CAN Befehl: 2-5 Sekunden
- Verdichter-Start: 30-60 Sekunden (Anti-Takt-Sperre)

---

**Viel Erfolg bei der Optimierung deiner PV-Eigenverbrauchsquote!** ☀️🔥💧

# WPL 09 ACS classic / WPM3 / FET

The owner built and installed this profile with ESPHome 2026.10.1. Live MQTT
observations and Home Assistant checks on 2026-10-29 confirmed fresh valid feedback
for 44 of 45 periodic requests and three passive sources. ABTAUUNGAKTIV remains
unresolved (fresh `?`, HA `unknown`); optional HEIZMODUL outside temperature had
no new message in this window.
[WPL9_VALIDATION.csv](WPL9_VALIDATION.csv) lists the requests, sources, observed
values and fresh-message counts from 19:54-20:05 UTC (20:54-21:05 CET). Read-path
validation does not independently establish register semantics, physical sensor
locations, measurement accuracy or write acceptance.

## Configuration

Keep the normal board, CAN and common packages. Select:

```yaml
substitutions:
  device_model: "wpl9"
  can_bit_rate: 50kbps
packages:
  sensors: !include ha-stiebel-control/wpl9.yaml
```

Use the WPM3 CAN-B/display connection with the normal PC sender (0x680).
FET room temperature/humidity (0x4ec7/0x4ec8, member 0x401) are received passively.
No extra active FET requests or register renaming is added.

## Read feedback

| Group | Source | Qualification |
| --- | --- | --- |
| Outside, DHW actual, heat-pump flow and heating pressure | KESSEL (0x180) | Fresh valid feedback; exact physical measuring points depend on the installation |
| Kessel/auxiliary targets and effective DHW target | KESSEL (0x180) | Generic target labels are not an exact WPM display mapping |
| Return, room targets, Kessel target and effective DHW target | MANAGER (0x480) | Fresh feedback; KESSEL return produced invalid -255 |
| Editable DHW settings 1/2 | KESSEL_CANB (0x201) | Fresh 50/45 degree C feedback; write acceptance has not been tested |
| Compressor-1 heating/cooling runtime | HEIZMODUL (0x500) | Whole-hour values 6129/2803 reached MQTT and HA; display-label mapping remains to be compared |
| Electrical/heat energy counters | HEIZMODUL (0x500) | Fresh valid counter responses, including zero; meter accuracy is not independently calibrated |
| Raw compressor signal | HEIZMODUL (0x500) | Fresh numeric feedback; physical unit and all operating states remain unverified |
| Defrost active | HEIZMODUL (0x500) | Unresolved: ten fresh `?` responses and HA `unknown`; this is not an inactive-state confirmation |
| Date/time, operating selector, EVU lock and summer mode | MANAGER (0x480) | Read feedback; command effects and every state transition are not claimed |
| FET room values and buffer bottom | 0x401 / KESSEL (0x180) | Fresh passive feedback; buffer bottom is not an extra polling-table entry |

Receive counts, rather than value changes or retained presence alone, were used
to verify freshness. HEIZMODUL outside temperature remains an optional passive
source. A quiet window is not proof that it is invalid or interchangeable with
the separate KESSEL outside-temperature signal.

KESSELSOLLTEMP is a generic CAN name. Its 0.1 degree changes are compatible with
a weather-dependent calculated target; they do not establish an actual-temperature
measurement. The exact WPM target association remains a separate display check.

## DHW routing and migration

| Register | Write | Immediate readback | Discovery state / polling |
| --- | --- | --- | --- |
| EINSTELL_SPEICHERSOLLTEMP (0x0013) | 0x201 | 0x201 | 0x201, ten minutes |
| EINSTELL_SPEICHERSOLLTEMP2 (0x0a06) | 0x201 | 0x201 | 0x201, ten minutes |

Both use normal generated headers (write 40,01; read 41,01). Fresh responses reach
the exact CAN-B state topics advertised by discovery. MANAGER, KESSEL_CANB and
WPL9-only KESSEL command aliases share the route resolver. Other model routes are
preserved. SG Ready uses this DHW route while preserving room-target handling
and the adapter's existing readback delay.

No setpoints were changed during validation. Write acceptance, persistence,
SG Ready boost/restore and cooling control remain untested on hardware. The
separate SPEICHERSOLLTEMP register is not assumed interchangeable with either
editable EINSTELL register; KESSEL and MANAGER values are not aliased.

Model selection changes the main device identifier from stiebel_eltron_wpl10ac
to stiebel_eltron_wpl9. Old retained MANAGER/KESSEL discoveries can coexist with
new CAN-B controls. Inspect consumers and migrate deliberately; this firmware
does not delete discoveries or rewrite dashboards. Backing up and disabling
invalid legacy entities preserves their IDs and history.

Both writable discoveries initially appeared only after a requested
heatingpump/republish_discoveries refresh. Startup discovery completeness remains
open; old retained entries alone do not establish successful startup discovery.

## VD runtime units

The six heating/cooling runtime registers 0x07fc through 0x0801 now use et_hours,
an unsigned whole-hour type. The old et_mil_val conversion divided raw 6129/2803
by 1000 while discovery advertised hours. They now publish 6129/2803 with the
same IDs, duration, h and total_increasing metadata. 0x8000 remains unavailable
(-255); other fractional signals retain their scaling. This shared decoder fix
applies only to these six entries. Blacklisted WW runtime registers are untouched.

CAN-to-MQTT host tests cover metadata, zero, invalid markers and unsigned values
above 32767. The owner installed the correction and MQTT/HA showed matching
whole-hour values. Existing history/statistics are not rescaled automatically;
review the conversion discontinuity separately before using historical sums.

## Unvalidated candidates retained for investigation

HEIZMODUL/VERDAMPFERTEMP and MANAGER/WW_ECO repeatedly returned fresh -255,
with no valid value in the checked recorder period starting 2026-10-27. Their
calls remain commented as Not validated on WPL9 so contributors can investigate
other members, registers or decoding. The evaporator entry is in the model table.
WW_ECO comes from a template lambda in common.yaml; only WPL9 selects its disabled
branch. Other models retain their original read. Definitions and passive decoding
remain available; there is no global blacklist or claim that the physical
function is absent.

After installing this change, 75 seconds of observation saw no new -255 from
these sources. Their backed-up retained states were cleared; a fresh subscription
and 70 more seconds found neither topic while healthy traffic continued. Future
passive CAN traffic can still recreate a topic. HA/broker cleanup is a separate
installation operation, not an automatic firmware migration.

KUEHLMODE (0x0287) has no verified boolean mapping for this installation. OTHER is
a fallback member dropped by the current decoder path. No cooling-active entity
or cooling request is added. delta_t_running intentionally pauses when the
compressor is off. Stored SG Ready settings are not proof of successful control.

ABTAUUNGAKTIV (0x0061, HEIZMODUL) stays visible as an unresolved polling candidate.
The shared boolean decoder accepts 0x0000, 0x0001 and 0x0100; another raw value
produces `?`. Capture and compare raw feedback with an actual WPM defrost state
before changing its source or mapping. Do not convert unknown responses to off.

## Validation and remaining checks

- Filtered host suites pass for WPL9 and WPL17. The local compiler aborts in the
  pre-existing invalid-enum test (ElsterType)255; it was excluded locally and
  remains enabled in GCC CI. The unfiltered local suite is not claimed to pass.
- The actual WW_ECO lambda sends zero CAN reads under the WPL9 macro and the
  original read for another model. ESPHome 2026.10.1 source generation passes for
  WPL9 S3 and the shared S2 entry point. Source generation is not a full firmware
  build; the firmware build must pass CI.
- Model smoke manifests support excluded_common_topics. WPL9 excludes its disabled
  MANAGER/WW_ECO source; other models retain their common requirements. This smoke
  test checks presence; use eleven minutes to verify fresh ten-minute feedback.
- Compare remaining labels with the WPM display, then test write acceptance,
  persistence, legacy aliases and SG Ready before claiming full control support.

Build-flag modernization and encrypted OTA are separate maintenance changes.
They are not part of this PR. Keep installed keys stable and private settings
outside the model package.

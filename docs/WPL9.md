# WPL 09 ACS classic / WPM3 / FET

This profile is based on upstream v2.4.2 (`1c6e34f`). The owner built and installed
it on a WPL 09 ACS classic with WPM3/FET. Live MQTT observation on 2026-10-28
confirmed both CAN-B DHW setting feedback topics, heating pressure and FET values.
The owner subsequently confirmed return-temperature updates from MANAGER.
Write acceptance, persistence and SG Ready boost/restore remain unverified.

## Configuration

Keep the normal board, CAN and common packages. Select:

```yaml
substitutions:
  device_model: "wpl9"
  can_bit_rate: 50kbps
packages:
  sensors: !include ha-stiebel-control/wpl9.yaml
```

Use the WPM3 CAN-B/display connection and the normal PC sender (0x680).
The supplied log shows passive FET room temperature/humidity at member 0x401,
indices 0x4ec7/0x4ec8. No extra active FET requests or register renaming is added.

## Explicit routing, not header overrides

| Register | Write destination | Immediate readback | Discovery state / periodic polling |
| --- | --- | --- | --- |
| EINSTELL_SPEICHERSOLLTEMP (0x0013) | 0x201 | 0x201 | 0x201 |
| EINSTELL_SPEICHERSOLLTEMP2 (0x0a06) | 0x201 | 0x201 | 0x201 |

Both registers use 0x201 for immediate readback, periodic polling and discovery
state, with normal generated headers (write 40,01; read 41,01). Polling is every
ten minutes. During live observation, new MQTT messages arrived for 0x0013
(50.0 degrees C) and 0x0a06 (45.0 degrees C); these were not merely retained data.
This confirms the feedback topics, but without a CAN capture does not distinguish
solicited responses from other bus traffic. Successful writes are not inferred
from these observations.

The configured SPEICHERSOLLTEMP polling source is MANAGER (0x480), matching the
local file. This effective target is not assumed to be interchangeable with either
editable EINSTELL register. DRUCK_HEIZKREIS is polled at KESSEL (0x180).

RUECKLAUFISTTEMP (0x0016) polls MANAGER (0x480) every 30 seconds. KESSEL returned
fresh -255 messages; after switching to MANAGER and flashing, the owner confirmed
that the existing return-temperature entity updates again.

Cooling is not verified by this profile. KUEHLMODE (0x0287) has no documented
boolean mapping in ElsterTable. A retained OTHER/KUEHLMODE value of 0 is not
accepted as verification: OTHER is a fallback for unmapped senders and the current
processAndUpdate path discards its messages. No cooling-active entity or new
cooling request is added.

Writable discovery now uses KESSEL_CANB for WPL9: unique IDs, command topics and
state topics move together. Old retained KESSEL and MANAGER discovery may remain
visible as duplicate controls; this draft does not delete those broker entries.
MANAGER, KESSEL_CANB and WPL9-only KESSEL command aliases share the same resolver.
Other models keep their existing routes. SG Ready uses the WPL9 route for DHW;
room targets and its 100 ms readback delay are preserved. Test SG Ready explicitly
before relying on boost/restore, since the local adapter previously used MANAGER.

## Hardware evidence and remaining limitations

Live MQTT observation on 2026-10-28, 23:27:36–23:31:44 Europe/Berlin:

| Signal | Evidence |
| --- | --- |
| KESSEL_CANB/EINSTELL_SPEICHERSOLLTEMP | New 50.0 degree C message at 23:31:44 |
| KESSEL_CANB/EINSTELL_SPEICHERSOLLTEMP2 | New 45.0 degree C message at 23:30:08 |
| Both writable DHW discoveries | Published with WPL9 device association and CAN-B command/state topics |
| KESSEL/DRUCK_HEIZKREIS | New messages, last value 1.61 bar |
| FET 0x401 | New messages, last values 20.2 degrees C and 57.5 percent |
| CAN diagnostics | Eight new updates per topic; last TEC 0, REC 0 and error counter 0 |

The writable discoveries were absent when observation started and arrived after
one explicitly requested discovery refresh. Startup discovery completeness still
needs investigation. Existing KESSEL/HEIZMODUL return/evaporator invalid values
must not be interpreted as temperatures. VERDAMPFERTEMP at HEIZMODUL and WW_ECO
at MANAGER remain invalid in the observation; not every inherited polling entry
has been validated. No setpoints were changed during the MQTT checks.

## Before proposing a merge

- Run the added WPL9 ESP32-S3 CI build. The owner has built and installed a local
  ESPHome 2026.10.x version; this does not replace the upstream CI checks.
  CI currently pins 2026.5.3, as upstream does.
- Test each editable register independently: record command, outgoing frame,
  valid read response, and the actual WPM setting; confirm persistence and restore
  the starting setting. A sent frame or -255 alone proves no successful write.
- Confirm the separate SPEICHERSOLLTEMP source; its identity with the editable
  registers is not assumed.
- Exercise discovered commands, legacy aliases and SG Ready boost/restore.
- Check that state updates reach the exact state_topic advertised by discovery.
- The model smoke baseline lists observed feedback topics and the owner-confirmed
  MANAGER return topic. Topic presence alone, especially retained data, does not
  establish fresh feedback or successful control.
- Review HA migration: device_model changes the main device identifier from
  stiebel_eltron_wpl10ac to stiebel_eltron_wpl9. Do not automatically delete retained
  discovery or assume existing automations/devices migrate without review.

## Scope and updates

The proposal adds a WPL9 profile, central routing, host tests, a smoke manifest and
a CI build. It does not change WPL10AC's polling, global writable-number owners,
OTA credentials, build flag syntax, payload_reset or time formatting.

The local firmware's build_flags change may still be required with ESPHome
2026.10.x; evaluate it separately from model support. Keep private settings in the
top-level local configuration. Once model support is merged and released, select
the official release and wpl9 profile instead of replaying local-all.patch.

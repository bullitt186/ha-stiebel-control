# WPL9 WPM counter correction and migration

## Source and accounting boundary

The WPL 09 ACS classic / WPM3 display counter families are read from
HEIZMODUL_WPL (0x514), replacing the mismatching HEIZMODUL (0x500) inputs.
The electrical/runtime family was compared with the WPM display on 2026-11-15;
later WPM photographs and live reads supported the thermal/NHZ family. These
observations validate this installation's data path, not sensor calibration.

| Family | Member / registers | Calculation or meaning |
|---|---|---|
| VD electrical heating / DHW | 0x514, 0x091a–0x0921 | SUM_MWH × 1000 + SUM_KWH, plus the current TAG contribution |
| VD thermal heating / DHW | 0x514, 0x092a–0x0931 | Same SUM/TAG sliding-total convention |
| NHZ thermal heating / DHW | 0x514, 0x0924/25 and 0x0928/29 | SUM_MWH × 1000 + SUM_KWH; no invented daily split |
| Heating / DHW / defrost runtime | 0x514, 0x4efb / 0x4efd / 0x4f06 | Whole hours |
| Cooling runtime | 0x514, 0x07ff | Whole hours; changes member from the old entity |
| Hardware lifetime starts | 0x514, 0x4ef0 / 0x4ef1 | Thousands × 1000 + remainder; fresh paired reads |

WPL9 cumulative COP is `(VD heat + NHZ heat) / (VD electricity + NHZ heat)`.
The electric resistance heater's electricity is approximated by its thermal
yield. Heating, DHW and the combined value use sums of energy, not an average
of the two ratios. Current-boot validated SUM and TAG inputs are required;
missing NHZ inputs do not mean zero. Incomplete inputs or a zero denominator
publish numeric unknown. These are cumulative energy ratios, not instantaneous
COP or a calibrated whole-system seasonal performance factor. Cooling energy
attribution and external auxiliary consumption remain unresolved.

Manufacturer references supporting the accounting distinction:

- [WPM commissioning manual, p. 11](https://www.stiebel-eltron.com/static/ste/docportal/manual/DM0000122888-fjw.pdf#page=11)
  lists electrical VD energy separately from NHZ heat.
- [ISG Modbus manual, block 4](https://www.stiebel-eltron.de/toolbox/content/docs/anleitungen/installation/ISG_Modbus/321798-44755-9770_ISG%20Modbus_de_en_fr_it_nl_cs_sk_pl_hu.pdf)
  separates NHZ heat (3507–3510) and VD electricity (3512/3513, 3515/3516).

## Split-counter rules

Pair high and low replies no more than 15 seconds apart and request the low
word after each high-word reply. SUM pairs require two consistent complete
reads after boot. A first torn 4/998 pair followed by 4/001 therefore cannot
establish a false extra MWh and freeze totals/COP. Confirmation reads are bounded.

The carry gate holds the recorded 1.996 → 2.996 → 2.030 kWh peak in either read
order, but permits legitimate 1.2–3 kWh thermal yield per ten-minute cycle.
A same-high decrease is never confirmed without a daily reset permission.
An observed date transition remains eligible across outages longer than the
old 30-minute window, including when TAG_KWH stays at zero. Reset candidates
need another nondecreasing fresh pair; a further decrease restarts confirmation.
SUM pairs never accept a daily reset. Invalid readings retain poll back-off.

A WPM reset before midnight can remain held until the day changes: (12,300) →
(0,999) → (1,900) → (2,800) are held, then (3,700) on the next observed date is
accepted. This delays publication rather than accepting a false decrease.
Tests cover this exact sequence, boot carry, delayed midnight and high-load yield.

## Continuity across an ESP restart

The 2026-11-21 hardware follow-up exposed a held DHW thermal total dropping from
8720.240 to 8720.000 kWh after reboot: the sanitizer's floor existed only in RAM.
A separate versioned NVS record now stores all four accepted combined totals,
the SUM bases, last TAG values and folded day contributions. Retaining the
folding state also prevents double-counting TAG after a reboot between a SUM
step and the later TAG reset. The existing energy-base record is unchanged.

Commit changed records before publishing a total, including a synchronous
preferences flush. Repeated identical data does not write flash. Save/flush
failure delays publication until a retry succeeds; malformed records are
rejected. Fresh current-boot counter inputs are still required. Existing
confirmed counter-replacement handling is retained. The first installation
initializes this new record from fresh inputs and cannot recover a fraction
already lost by older firmware or rewrite Home Assistant history.

Regression tests simulate RAM loss using keyed, committed preference bytes,
both save and flush failure, corrupt data, independent totals and a restart
between the SUM step and TAG reset. After the owner installed the corrected
2.8.0-based private build, a permitted ESP restart on 2026-11-21 was followed
for twelve minutes. All four totals arrived freshly and stayed unchanged or
increased: electricity heating 3116.264 → 3116.300 kWh, thermal heating
19614.363 → 19614.566 kWh, DHW electricity 2362.000 kWh, DHW heat 8720.000 kWh.
COP returned to 5.61 / 3.03 / 4.41 after current-boot inputs arrived, matching
independent arithmetic. No CAN/MQTT failure counters increased. The private
build also contains unrelated changes; this is evidence for the shared counter
code, not a claim that the isolated public PR binary was flashed.

## Entity and consumer migration

Eight corrected total/day sensors use unique IDs beginning
`stiebel_calculated_wpl9_` and `<device_name>/calculated/wpl9_` state topics.
They cover electrical and thermal heating/DHW, each total and today. Do not
alias them to old 0x500 histories. New NHZ totals and the paired hardware-start
entity are separate on-demand entities. COP keeps its IDs but changes its
definition; record the upgrade time when interpreting existing COP statistics.

1. Check the actual entity IDs in HA: existing registrations retain their IDs,
   and language-independent default IDs affect new registrations only.
2. Re-point Energy dashboard statistics and every `utility_meter` source to
   the new corrected counters. Check dashboards, templates, automations and
   external consumers before removing an old entity.
3. Remove only the exact obsolete retained discovery topics by sending an empty
   retained payload to each `homeassistant/<component>/<device_name>/<old-id>/config`
   topic. Then remove the now-orphaned unused entities in HA. Do not clear a
   wildcard namespace or delete the new WPL9 configs. Check for old retained
   state topics separately; this firmware does not delete broker data.

Raw EL_*/WAERMEERTRAG_* change member from 0x500 to 0x514.
LZ_VERD_1_HEIZBETRIEB is no longer requested; LZ_VERD_1_KUEHLBETRIEB moves to 0x514.
The firmware does not modify HA consumers, statistics or a user's manual starts
counter automatically. Compressor-active logic, estimated daily compressor
statistics, Delta-T and exploratory diagnostic registers are outside this PR.

## Operating modes and remaining checks

CAN values are unchanged. Announced WPL9 labels are Programmbetrieb,
Komfortbetrieb and ECO-Betrieb. Old MQTT command aliases Automatik, Tagbetrieb
and Absenkbetrieb are accepted for one migration release; HA select options and
state conditions must use the announced labels. Mode vocabulary is a separate
model capability, leaving WPL10AC's Programmierbetrieb spelling unchanged.
The owner confirmed all six display spellings on 2026-11-21: Notbetrieb,
Bereitschaftsbetrieb, Programmbetrieb, Komfortbetrieb, ECO-Betrieb and
Warmwasserbetrieb. The ECO spelling was clarified explicitly. Display
capitalization is normalized consistently with the other firmware labels.
Legacy Bereitschaft and Warmwasser commands also remain accepted as aliases.

A physical midnight rollover on the rebased firmware, active NHZ/defrost and a
high-load physical carry have not been newly observed. Native regressions do
not replace these checks. Keep the PR in draft until the required hardware
validation is complete; no control cycle is forced for validation.

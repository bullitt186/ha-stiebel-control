/*
 * Signal Request Table — WPL23 Heat Pump
 *
 * UNVERIFIED ON REAL HARDWARE. Ported from community project
 * kr0ner/OneESP32ToRuleThemAll, re-implemented against this project's
 * ElsterTable.h/signal_requests conventions — no code or YAML was copied
 * verbatim. This is a curated subset of the source project's WPL23 signals
 * (same shared WPL base as WPL17, plus a handful of WPL23-only signals).
 * Deliberately excludes raw relay outputs, stepper-motor phases, and X1
 * terminal config registers — internal wiring/calibration details, not
 * monitoring data.
 *
 * Self-contained on purpose rather than sharing a macro with
 * signal_requests_wpl17.h: WPL17 and WPL23 disagree on the Elster index for
 * six signal names (VERDICHTER_STARTS[_K], LAUFZEIT_VD_HEIZEN/WW/ABTAUEN),
 * and even disagree on which CAN member exposes LAUFZEIT_NHZ1/2/1_2 — a
 * shared macro would either need per-model parameters or silently encode
 * one model's choices as "shared", which is worse than light duplication.
 *
 * IMPORTANT: WPL23 runs its CAN bus at 50kbps, not the project default of
 * 20kbps. Set `can_bit_rate: 50kbps` in your heatingpump.yaml substitutions
 * when using this model — see can_esp32.yaml/can_mcp2515.yaml.
 *
 * CAN targets per the source project: Kessel (0x180), Manager (0x480),
 * Heizmodul at 0x514 (cm_heizmodul_wpl — NOT the default HEIZMODUL/0x500),
 * FET (0x402, room control unit), and HK1 (0x601, reusing our existing
 * MISCHERMODUL_2/cm_mischermodul_2 — same physical address, different name
 * in the source project).
 *
 * Several signals below reuse existing ElsterTable rows whose Name doesn't
 * match the source project's name for the same Index — these are very
 * likely the same underlying Elster signal under an older/different label
 * from the original 2014 table. Low-confidence matches are flagged inline.
 */

#ifndef SIGNAL_REQUESTS_WPL23_H
#define SIGNAL_REQUESTS_WPL23_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // WPL23: signals queried from KESSEL (0x180)
    // ========================================================================
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel},
    {"MAX_HEIZUNG_TEMP",           FREQ_10MIN, cm_kessel},  // WPL: MAXVORLAUFTEMP
    {"MAX_TEMP_KESSEL",            FREQ_10MIN, cm_kessel},  // WPL: MAXRUECKLAUFTEMP
    {"PUFFERTEMP_UNTEN1",          FREQ_1MIN,  cm_kessel},  // WPL: PUFFERISTTEMPERATUR
    {"PUFFERSOLL",                 FREQ_1MIN,  cm_kessel},  // WPL: PUFFERSOLLTEMPERATUR
    {"BIVALENTPARALLELTEMPERATUR_HZG", FREQ_10MIN, cm_kessel},  // WPL: BIVALENZTEMPERATUR_HZG
    {"BIVALENTPARALLELTEMPERATUR_WW",  FREQ_10MIN, cm_kessel},  // WPL: BIVALENZTEMPERATUR_WW
    {"BIVALENZALTERNATIVTEMPERATUR_HZG", FREQ_10MIN, cm_kessel},  // WPL: EINSATZGRENZE_HZG — low confidence
    {"BIVALENZALTERNATIVTEMPERATUR_WW",  FREQ_10MIN, cm_kessel},  // WPL: EINSATZGRENZE_WW — low confidence
    {"MAX_WW_TEMP",                FREQ_10MIN, cm_kessel},  // WPL: MAXIMALE_VORLAUFTEMP_WW
    {"WPVORLAUFIST",               FREQ_30S,   cm_kessel},  // WPL: VORLAUFTEMP (already used by WPF10 too)
    {"VORLAUFISTTEMP_WP",          FREQ_30S,   cm_kessel},
    {"RUECKLAUFISTTEMP_WP",        FREQ_30S,   cm_kessel},
    {"KUEHLEN_SOLLTEMP",           FREQ_1MIN,  cm_kessel},
    {"KUEHLEN_ISTTEMP",            FREQ_1MIN,  cm_kessel},
    {"AUSSEN_FROSTTEMP",           FREQ_10MIN, cm_kessel},  // WPL: ANLAGEFROST
    {"FROSTSCHUTZTEMPERATUR",      FREQ_10MIN, cm_kessel},
    {"VORLAUFISTTEMP_NHZ",         FREQ_30S,   cm_kessel},
    {"AUSLEGUNGSTEMPERATUR",       FREQ_10MIN, cm_kessel},
    {"WARMWASSERHYSTERESE",        FREQ_10MIN, cm_kessel},
    {"WW_SOLLTEMPERATUR",          FREQ_1MIN,  cm_kessel},
    {"HEIZUNGSDRUCK",              FREQ_30S,   cm_kessel},
    {"VOLUMENSTROM_WPL",           FREQ_30S,   cm_kessel},
    {"EINGESCHALTETE_STUFEN",      FREQ_10MIN, cm_kessel},  // source targets Manager — see Manager section below
    {"SILENT_LEISTUNG",            FREQ_10MIN, cm_kessel},
    {"SILENT_LUEFTER",             FREQ_10MIN, cm_kessel},
    {"WAERMEPUMPE_AUS",            FREQ_10MIN, cm_kessel},
    {"WARMWASSERBETRIEB",          FREQ_10MIN, cm_kessel},
    {"ANTILEGIONELLENBEHANDLUNG",  FREQ_10MIN, cm_kessel},
    {"WW_ANFORDERUNG",             FREQ_1MIN,  cm_kessel},
    {"VERDICHTER_STARTS",          FREQ_10MIN, cm_kessel},    // same index as WPC7 (0x071d) — see ElsterTable.h
    {"VERDICHTER_STARTS_K",        FREQ_10MIN, cm_kessel},    // same index as WPC7 (0x071c) — see ElsterTable.h
    {"LAUFZEIT_NHZ1",              FREQ_10MIN, cm_kessel},    // WPL23 targets Kessel; WPL17 targets Heizmodul for this name
    {"LAUFZEIT_NHZ2",              FREQ_10MIN, cm_kessel},
    {"LAUFZEIT_NHZ1_2",            FREQ_10MIN, cm_kessel},

    // ========================================================================
    // WPL23: signals queried from MANAGER (0x480)
    // ========================================================================
    {"HEIZEN_EFFIZIENZ_TAG",       FREQ_10MIN, cm_manager},
    {"HEIZEN_EFFIZIENZ_JAHR",      FREQ_10MIN, cm_manager},
    {"EINGESCHALTETE_STUFEN",      FREQ_10MIN, cm_manager},

    // ========================================================================
    // WPL23: signals queried from HEIZMODUL_WPL (0x514)
    // ========================================================================
    {"VERDAMPFERISTTEMP_KOMPENSIERT", FREQ_1MIN,  cm_heizmodul_wpl},  // WPL: VERDAMPFERTEMP — low confidence
    {"ANZEIGE_HOCHDRUCK",           FREQ_30S,   cm_heizmodul_wpl},  // WPL: DRUCK_HOCHDRUCK
    {"ANZEIGE_NIEDERDRUCK",         FREQ_30S,   cm_heizmodul_wpl},  // WPL: DRUCK_NIEDERDRUCK
    {"RUECKLAUFTEMP_QUELLE",        FREQ_1MIN,  cm_heizmodul_wpl},
    {"VORLAUFTEMP_QUELLE",          FREQ_1MIN,  cm_heizmodul_wpl},
    {"QUELLENDRUCK_WPL",            FREQ_1MIN,  cm_heizmodul_wpl},
    {"LEISTUNG_QUELLENPUMPE",       FREQ_1MIN,  cm_heizmodul_wpl},
    {"REKUPERATORISTTEMP",          FREQ_1MIN,  cm_heizmodul_wpl},  // WPL23: REKUPERATORTEMPERATUR
    {"LEISTUNGSREDUZIERUNG_KUEHLEN", FREQ_1MIN, cm_heizmodul_wpl},  // WPL23: ZWISCHENEINSPRITZUNGSTEMP — low confidence
    {"LAUFZEIT_VD_HEIZEN_WPL23",    FREQ_10MIN, cm_heizmodul_wpl},
    {"LAUFZEIT_VD_WW_WPL23",        FREQ_10MIN, cm_heizmodul_wpl},
    {"ABTAUZEIT_VERD1",             FREQ_10MIN, cm_heizmodul_wpl},  // WPL23: LAUFZEIT_VD_ABTAUEN
    {"ZEITDAUER_LETZTE_ABTAUUNG",   FREQ_10MIN, cm_heizmodul_wpl},  // WPL23: ZEIT_ABTAUEN — likely match
    {"STARTS_ABTAUUNG",             FREQ_10MIN, cm_heizmodul_wpl},  // WPL23: STARTS_ABTAUEN — likely match

    // ========================================================================
    // WPL23: signals queried from FET (0x402, room control unit)
    // ========================================================================
    {"RAUMISTTEMP_WPL",            FREQ_1MIN,  cm_fet},
    {"RAUMFEUCHTE_WPL",            FREQ_10MIN, cm_fet},
    {"RAUMSOLLTEMP",               FREQ_1MIN,  cm_fet},

    // ========================================================================
    // WPL23: signals queried from HK1 (0x601 — reuses MISCHERMODUL_2)
    // ========================================================================
    {"ISTTEMPERATUR",              FREQ_1MIN,  cm_mischermodul_2},
    {"SOLLTEMPERATUR",              FREQ_1MIN,  cm_mischermodul_2},
    {"KOMFORTTEMPERATUR",          FREQ_10MIN, cm_mischermodul_2},
    {"ECOTEMPERATUR",              FREQ_10MIN, cm_mischermodul_2},
    {"STEIGUNG_HEIZKURVE",         FREQ_10MIN, cm_mischermodul_2},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL23_H

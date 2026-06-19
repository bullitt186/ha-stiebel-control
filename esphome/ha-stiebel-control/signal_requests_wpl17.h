/*
 * Signal Request Table — WPL17 Heat Pump
 *
 * UNVERIFIED ON REAL HARDWARE. Ported from community project
 * kr0ner/OneESP32ToRuleThemAll, re-implemented against this project's
 * ElsterTable.h/signal_requests conventions — no code or YAML was copied
 * verbatim. This is a curated subset of the source project's WPL17 signals:
 * temperatures, pressures/flow, fan/compressor speed, PID/EXV control,
 * runtime counters, and operating state. Deliberately excludes raw relay
 * outputs (RELAIS_X_2_*), stepper-motor phases, and X1 terminal config
 * registers — internal wiring/calibration details, not monitoring data.
 *
 * IMPORTANT: WPL17 runs its CAN bus at 50kbps, not the project default of
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

#ifndef SIGNAL_REQUESTS_WPL17_H
#define SIGNAL_REQUESTS_WPL17_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // WPL17: signals queried from KESSEL (0x180)
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
    {"SOMMERBETRIEB_WPL17",        FREQ_1MIN,  cm_kessel},
    {"WARMWASSERBETRIEB",          FREQ_10MIN, cm_kessel},
    {"ANTILEGIONELLENBEHANDLUNG",  FREQ_10MIN, cm_kessel},
    {"WW_ANFORDERUNG",             FREQ_1MIN,  cm_kessel},
    {"LAUFZEIT_VD_HEIZEN_WPL17",   FREQ_10MIN, cm_kessel},
    {"LAUFZEIT_VD_WW_WPL17",       FREQ_10MIN, cm_kessel},
    {"LAUFZEIT_VD_KUEHLEN",        FREQ_10MIN, cm_kessel},
    {"LAUFZEIT_VD_ABTAUEN_WPL17",  FREQ_10MIN, cm_kessel},

    // ========================================================================
    // WPL17: signals queried from MANAGER (0x480)
    // ========================================================================
    {"HEIZEN_EFFIZIENZ_TAG",       FREQ_10MIN, cm_manager},
    {"HEIZEN_EFFIZIENZ_JAHR",      FREQ_10MIN, cm_manager},
    {"EINGESCHALTETE_STUFEN",      FREQ_10MIN, cm_manager},

    // ========================================================================
    // WPL17: signals queried from HEIZMODUL_WPL (0x514)
    // ========================================================================
    {"VERDAMPFERISTTEMP_KOMPENSIERT", FREQ_1MIN,  cm_heizmodul_wpl},  // WPL: VERDAMPFERTEMP — low confidence
    {"WE_KONFIGURATION",           FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: WP_WASSERVOLUMENSTROM — low confidence
    {"LUEFTERDREHZAHL",            FREQ_1MIN,  cm_heizmodul_wpl},  // WPL17: LUEFTERLEISTUNG_REL — low confidence
    {"SOLLDREHZAHL_LUEFTER",       FREQ_1MIN,  cm_heizmodul_wpl},
    {"ISTDREHZAHL_LUEFTER",        FREQ_1MIN,  cm_heizmodul_wpl},
    {"LAUFZEIT_DHC1",              FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: LAUFZEIT_NHZ1 — likely match
    {"LAUFZEIT_DHC2",              FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: LAUFZEIT_NHZ2 — likely match
    {"LZ_DHC12",                   FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: LAUFZEIT_NHZ1_2 — likely match
    {"ZEITDAUER_LETZTE_ABTAUUNG",  FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: ZEIT_ABTAUEN — likely match
    {"STARTS_ABTAUUNG",            FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: STARTS_ABTAUEN — likely match
    {"P_ANTEIL_EXV",                FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: P_FAKTOR
    {"I_ANTEIL_EXV",                FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: I_FAKTOR
    {"D_ANTEIL_EXV",                FREQ_10MIN, cm_heizmodul_wpl},  // WPL17: D_FAKTOR
    {"VORSTEUER_OEFFNUNGSGRAD",     FREQ_1MIN,  cm_heizmodul_wpl},  // WPL17: VORSTEUER_OEFFNUNGSGRAD_EXV
    {"EXV_OEFFNUNGSGRAD",           FREQ_1MIN,  cm_heizmodul_wpl},  // WPL17: OEFFNUNGSGRAD_EXV
    {"OEFFNUNGSGRAD_BYPASSVENT",    FREQ_1MIN,  cm_heizmodul_wpl},
    {"MAXIMALE_PUMPENLEISTUNG",     FREQ_10MIN, cm_heizmodul_wpl},
    {"STANDBY_PUMPENLEISTUNG",      FREQ_10MIN, cm_heizmodul_wpl},
    {"LEISTUNG_WARMWASSERPUMPE_WPL", FREQ_1MIN, cm_heizmodul_wpl},
    {"SOLLWERT_UEBERHITZUNG",       FREQ_1MIN,  cm_heizmodul_wpl},  // WPL17: SOLL_UEBERHITZUNG
    {"ISTWERT_UEBERHITZUNG_VERDAMPFER", FREQ_1MIN, cm_heizmodul_wpl},  // WPL17: IST_UEBERHITZUNG_V
    {"INTEGRAL_REGELABWEICHUNG",    FREQ_1MIN,  cm_heizmodul_wpl},  // WPL17: REGELABWEICHUNG — low confidence
    {"SPANNUNG_INVERTER",           FREQ_1MIN,  cm_heizmodul_wpl},
    {"STROM_INVERTER",              FREQ_1MIN,  cm_heizmodul_wpl},
    {"STROM_MOTOR",                 FREQ_1MIN,  cm_heizmodul_wpl},
    {"VERDICHTER_EINTRITTSTEMP",    FREQ_1MIN,  cm_heizmodul_wpl},
    {"ISTDREHZAHL_VERDICHTER",      FREQ_30S,   cm_heizmodul_wpl},
    {"SOLLDREHZAHL_VERDICHTER",     FREQ_30S,   cm_heizmodul_wpl},
    {"ANZEIGE_HOCHDRUCK",           FREQ_30S,   cm_heizmodul_wpl},  // WPL: DRUCK_HOCHDRUCK
    {"ANZEIGE_NIEDERDRUCK",         FREQ_30S,   cm_heizmodul_wpl},  // WPL: DRUCK_NIEDERDRUCK
    {"RUECKLAUFTEMP_QUELLE",        FREQ_1MIN,  cm_heizmodul_wpl},
    {"VORLAUFTEMP_QUELLE",          FREQ_1MIN,  cm_heizmodul_wpl},
    {"QUELLENDRUCK_WPL",            FREQ_1MIN,  cm_heizmodul_wpl},
    {"LEISTUNG_QUELLENPUMPE",       FREQ_1MIN,  cm_heizmodul_wpl},
    {"VERDICHTER_STARTS_WPL17",     FREQ_10MIN, cm_heizmodul_wpl},
    {"VERDICHTER_STARTS_K_WPL17",   FREQ_10MIN, cm_heizmodul_wpl},
    {"LAUFZEIT_NHZ1",               FREQ_10MIN, cm_heizmodul_wpl},  // bare property also exists; see LAUFZEIT_DHC1 above
    {"LAUFZEIT_NHZ2",               FREQ_10MIN, cm_heizmodul_wpl},
    {"LAUFZEIT_NHZ1_2",             FREQ_10MIN, cm_heizmodul_wpl},
    {"ZEIT_ABTAUEN",                FREQ_10MIN, cm_heizmodul_wpl},
    {"STARTS_ABTAUEN",              FREQ_10MIN, cm_heizmodul_wpl},

    // ========================================================================
    // WPL17: signals queried from FET (0x402, room control unit)
    // ========================================================================
    {"RAUMISTTEMP_WPL",            FREQ_1MIN,  cm_fet},
    {"RAUMFEUCHTE_WPL",            FREQ_10MIN, cm_fet},
    {"RAUMSOLLTEMP",               FREQ_1MIN,  cm_fet},

    // ========================================================================
    // WPL17: signals queried from HK1 (0x601 — reuses MISCHERMODUL_2)
    // ========================================================================
    {"ISTTEMPERATUR",              FREQ_1MIN,  cm_mischermodul_2},
    {"SOLLTEMPERATUR",             FREQ_1MIN,  cm_mischermodul_2},
    {"KOMFORTTEMPERATUR",          FREQ_10MIN, cm_mischermodul_2},
    {"ECOTEMPERATUR",              FREQ_10MIN, cm_mischermodul_2},
    {"STEIGUNG_HEIZKURVE",         FREQ_10MIN, cm_mischermodul_2},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL17_H

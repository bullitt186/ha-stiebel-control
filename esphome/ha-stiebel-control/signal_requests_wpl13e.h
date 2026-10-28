/*
 * Signal Request Table — WPL13E Heat Pump
 *
 * Contains the universal base signals plus WPL13E-specific signals.
 * To add a new model, copy this file, rename it, and replace the
 * model-specific section with your pump's signals.
 *
 * See signal_requests_base.h for the base macro definition.
 */

#ifndef SIGNAL_REQUESTS_WPL13E_H
#define SIGNAL_REQUESTS_WPL13E_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // WPL13E-SPECIFIC: temperatures and heating circuits
    // ========================================================================
    {"KESSELSOLLTEMP",             FREQ_30S,   cm_manager},
    {"KESSELSOLLTEMP",             FREQ_30S,   cm_kessel},
    {"SPEICHERSOLLTEMP",           FREQ_30S,   cm_manager},
    {"SPEICHERSOLLTEMP",           FREQ_30S,   cm_kessel},
    {"RAUMSOLLTEMP_I",             FREQ_30S,   cm_manager},
    {"RAUMSOLLTEMP_II",            FREQ_30S,   cm_manager},
    {"RAUMSOLLTEMP_III",           FREQ_30S,   cm_manager},
    {"RAUMSOLLTEMP_NACHT",         FREQ_30S,   cm_manager},
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel},
    {"AUSSENTEMP",                 FREQ_30S,   cm_heizmodul},
    // SAMMLERISTTEMP dropped: -40.0 (no sensor) for 11 months on a real WPL13E
    {"SPEICHERISTTEMP",            FREQ_30S,   cm_kessel},
    {"VORLAUFISTTEMP",             FREQ_30S,   cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_30S,   cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_30S,   cm_manager},
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_manager},
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_kessel},
    {"WPVORLAUFIST",               FREQ_30S,   cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_30S,   cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_30S,   cm_manager},
    {"ABTAUUNGAKTIV",              FREQ_1MIN,  cm_heizmodul},
    {"BETRIEBSART_WP",             FREQ_10MIN, cm_manager},

    // ========================================================================
    // SETTINGS (WPMW II parameters)
    // Dropped after 30 days of -255 on a real WPL13E: WW_HYSTERSE (manager+kessel;
    // WW_HYSTERESE 0x0022 does not answer either), ANTILEGIONELLEN_ZEITPUNKT.
    // HEIZKURVE on KESSEL is always 0.00 (6 months) - the real value is on MANAGER.
    // ========================================================================
    {"HEIZKURVE",                  FREQ_10MIN, cm_manager},
    {"ANTILEGIONELLEN",            FREQ_10MIN, cm_manager},
    // Fault code of the heat pump module (IWS), decoded via ErrorList. The MANAGER
    // answers a constant 69 on this index (no known fault code) - not requested.
    {"FEHLERMELDUNG",              FREQ_1MIN,  cm_heizmodul},
    {"GEBAEUDEART",                FREQ_10MIN, cm_manager},
    {"SOMMERBETRIEB_TEMP",         FREQ_30MIN, cm_manager},  // summer-mode threshold

    // ========================================================================
    // ENERGY: SUM_MWH (base) is whole MWh only; the kWh remainder and the
    // daily values ("WM TAG" in the WPMW II manual) refine COP and feed the
    // combined total/daily sensors.
    // ========================================================================
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_KWH",   FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_SUM_KWH",        FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_SUM_KWH",          FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_KWH",   FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_TAG_KWH",        FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_TAG_KWH",          FREQ_10MIN, cm_heizmodul},

    // ========================================================================
    // DIAGNOSTICS ("INFO IWS" in the WPMW II manual)
    // ANZEIGE_HOCHDRUCK/NIEDERDRUCK answer (~83 at standstill) but the scaling
    // is unverified until seen with the compressor running - not exposed yet.
    // ========================================================================
    {"HEISSGAS_TEMP",              FREQ_1MIN,  cm_heizmodul},
    {"ANZEIGE_HOCHDRUCK",          FREQ_1MIN,  cm_heizmodul},
    {"ANZEIGE_NIEDERDRUCK",        FREQ_1MIN,  cm_heizmodul},
    {"PROBE_WP_STATUS_4EDA",       FREQ_30S,   cm_heizmodul},  // TEMP: status word candidate, remove before release
    {"LAUFZEIT_DHC1",              FREQ_60MIN, cm_heizmodul},
    {"LAUFZEIT_DHC2",              FREQ_60MIN, cm_heizmodul},
    {"LZ_DHC12",                   FREQ_60MIN, cm_heizmodul},

    // ========================================================================
    // COMPRESSOR RUNTIME (single compressor = "VERD_2" on the IWS)
    // Dropped: VERD_1 / VERD_1_2 / KUEHL (no 2nd compressor, no cooling on the
    // "E" variant; -255 or 0 for 30 days) and all MANAGER copies (0 / -255).
    // ========================================================================
    {"LZ_VERD_2_HEIZBETRIEB",      FREQ_10MIN, cm_heizmodul},
    {"LZ_VERD_2_WW_BETRIEB",       FREQ_10MIN, cm_heizmodul},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL13E_H

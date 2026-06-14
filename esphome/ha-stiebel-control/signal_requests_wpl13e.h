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
    {"SAMMLERISTTEMP",             FREQ_30S,   cm_kessel},
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
    // PROBE SIGNALS — may or may not respond on WPL13E
    // If the pump responds, these appear automatically in HA via MQTT discovery
    // ========================================================================
    {"HEIZKURVE",                  FREQ_10MIN, cm_manager},
    {"HEIZKURVE",                  FREQ_10MIN, cm_kessel},
    {"WW_HYSTERSE",                FREQ_10MIN, cm_manager}, // typo in ElsterTable: "HYSTERSE"
    {"WW_HYSTERSE",                FREQ_10MIN, cm_kessel},
    {"ANTILEGIONELLEN",            FREQ_10MIN, cm_manager},
    {"ANTILEGIONELLEN_ZEITPUNKT",  FREQ_10MIN, cm_manager},
    {"GEBAEUDEART",                FREQ_10MIN, cm_manager},

    // ========================================================================
    // COMPRESSOR RUNTIME COUNTERS (Laufzeit Verdichter)
    // ========================================================================
    {"LZ_VERD_1_HEIZBETRIEB",    FREQ_10MIN, cm_heizmodul},
    {"LZ_VERD_2_HEIZBETRIEB",    FREQ_10MIN, cm_heizmodul},
    {"LZ_VERD_1_2_HEIZBETRIEB",  FREQ_10MIN, cm_heizmodul},
    {"LZ_VERD_1_KUEHLBETRIEB",   FREQ_10MIN, cm_heizmodul},
    {"LZ_VERD_2_KUEHLBETRIEB",   FREQ_10MIN, cm_heizmodul},
    {"LZ_VERD_1_2_KUEHLBETRIEB", FREQ_10MIN, cm_heizmodul},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL13E_H

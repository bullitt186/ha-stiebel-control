/*
 * Signal Request Table — WPF10 / WPF10M Heat Pump (Ground-Source)
 *
 * Based on community testing from PR #13 (vikin91) and issue #20 (korre73).
 * Signals marked (unverified) are from community reports and may need adjustment.
 *
 * To use this model:
 *   device_model: "wpf10"
 *   device_model_flag: "WPF10"
 *
 * Please open a PR with updated signals if you have a WPF10 and can verify them.
 */

#ifndef SIGNAL_REQUESTS_WPF10_H
#define SIGNAL_REQUESTS_WPF10_H

#include "config.h"
#include "signal_requests_base.h"

const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // WPF10-SPECIFIC: ground-source temperatures
    // ========================================================================
    {"QUELLENEINTRITTSTEMPERATUR",  FREQ_30S,  cm_heizmodul}, // brine source inlet temp
    {"QUELLENAUSTRITTSTEMP",        FREQ_30S,  cm_heizmodul}, // brine source outlet temp
    {"KESSELSOLLTEMP",              FREQ_30S,  cm_manager},
    {"SPEICHERSOLLTEMP",            FREQ_30S,  cm_manager},
    {"SPEICHERISTTEMP",             FREQ_30S,  cm_kessel},
    {"AUSSENTEMP",                  FREQ_30S,  cm_other},    // cm_other: varies by install
    {"RUECKLAUFISTTEMP",            FREQ_30S,  cm_manager},
    {"RUECKLAUFISTTEMP",            FREQ_30S,  cm_kessel},
    {"WPVORLAUFIST",                FREQ_30S,  cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP",   FREQ_30S,  cm_manager},
    {"EINSTELL_SPEICHERSOLLTEMP",   FREQ_30S,  cm_kessel},
    {"ABTAUUNGAKTIV",               FREQ_1MIN, cm_heizmodul},
    {"BETRIEBSART_WP",              FREQ_10MIN, cm_manager},

    // Probe signals (may not respond on all WPF10 configurations)
    {"RAUMSOLLTEMP_I",              FREQ_30S,  cm_manager},  // (unverified)
    {"HEIZKURVE",                   FREQ_10MIN, cm_manager},
    {"ANTILEGIONELLEN",             FREQ_10MIN, cm_manager},
};

const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPF10_H

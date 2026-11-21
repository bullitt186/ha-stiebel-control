/*
 * Signal Request Table — WPL 09 ACS classic / WPM3 / FET
 *
 * Based on the existing WPL10AC polling set and local WPL9 adjustments.
 * CAN B / WPM3 display connection, 50 kbps, normal PC sender 0x680.
 * FET temperature/humidity (0x4ec7/0x4ec8, member 0x401) are received passively.
 * Live audit on 2026-10-29: 44/45 periodic reads valid; ABTAUUNGAKTIV unresolved.
 * See docs/WPL9.md for per-signal evidence and remaining limitations.
 *
 * See signal_requests_base.h for the base macro definition.
 */

#ifndef SIGNAL_REQUESTS_WPL9_H
#define SIGNAL_REQUESTS_WPL9_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // BOILER / KESSEL (0x180)
    // ========================================================================
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel},
    {"HILFSKESSELSOLL",            FREQ_30S,   cm_kessel},
    {"KESSELSOLLTEMP",             FREQ_30S,   cm_kessel},
    {"SPEICHERISTTEMP",            FREQ_30S,   cm_kessel},   // DHW actual temperature
    {"DRUCK_HEIZKREIS",            FREQ_30S,   cm_kessel},
    {"WPVORLAUFIST",               FREQ_30S,   cm_kessel},   // Keep existing KESSEL flow-temperature feedback updating.
    {"SPEICHERSOLLTEMP",           FREQ_10MIN, cm_kessel},   // Preserve the separate legacy KESSEL feedback; do not alias MANAGER.

    // DHW settings: verified CAN-B feedback, matching writable discovery.
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_10MIN, cm_kessel_canb},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_10MIN, cm_kessel_canb},

    // ========================================================================
    // HEATING MODULE (0x500)
    // ========================================================================
    // Not validated on WPL9: HEIZMODUL returned fresh -255 in repeated audits.
    // Keep this candidate for future source/register investigation.
    // {"VERDAMPFERTEMP",          FREQ_30S,   cm_heizmodul},
    // Unresolved on WPL9: fresh '?' responses, HA unknown; not proof of inactive.
    {"ABTAUUNGAKTIV",              FREQ_1MIN,  cm_heizmodul},

    // MANAGER/WW_ECO is also not validated on WPL9 (fresh -255).
    // Its template-triggered read is disabled for this model in common.yaml.

    // ========================================================================
    // ROOM SETPOINTS — single member (MANAGER) to avoid duplicate/-255 entities
    // ========================================================================
    {"RAUMSOLLTEMP_I",             FREQ_30S,   cm_manager},
    {"RAUMSOLLTEMP_NACHT",         FREQ_30S,   cm_manager},
    {"SPEICHERSOLLTEMP",           FREQ_10MIN, cm_manager},
    {"KESSELSOLLTEMP",             FREQ_30S,   cm_manager},  // Existing MANAGER entity stopped changing after the profile update.
    // Restore the previously used MANAGER return-temperature source.
    // KESSEL returned fresh -255; valid MANAGER feedback was confirmed live.
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_manager},

    // Matched to WPM display photos and targeted CAN reads, 2026-11-15.
    {"LAUFZEIT_VD_HEIZEN",           FREQ_10MIN, cm_heizmodul_wpl},
    {"LAUFZEIT_VD_WW",               FREQ_10MIN, cm_heizmodul_wpl},
    {"LZ_VERD_1_KUEHLBETRIEB",        FREQ_10MIN, cm_heizmodul_wpl},
    {"LAUFZEIT_VD_ABTAUEN",           FREQ_10MIN, cm_heizmodul_wpl},
    // Poll thousands; its reply requests the remainder immediately (fresh pair).
    {"VERDICHTER_STARTS_K_4EF0",      FREQ_1MIN, cm_heizmodul_wpl},

    // ========================================================================
    // ENERGY: WPM electricity and heat counters on 0x514 (see migration notes)
    // ========================================================================
    SIGNAL_REQUESTS_ENERGY_IWS   // daily and total energy counters (signal_requests_base.h)
    // NHZ totals also need their kWh remainder (the photographed totals are <1 MWh).
    {"WAERMEERTRAG_2WE_HEIZ_SUM_KWH", FREQ_10MIN, cm_heizmodul_wpl},
    {"WAERMEERTRAG_2WE_WW_SUM_KWH",   FREQ_10MIN, cm_heizmodul_wpl},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL9_H

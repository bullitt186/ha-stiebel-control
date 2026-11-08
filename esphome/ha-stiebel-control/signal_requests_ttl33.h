/*
 * Signal Request Table — Tecalor TTL 33 with WPMW II manager
 *
 * Contains the universal base signals plus TTL 33-specific signals. Verified read-only on a
 * real unit (CAN bus of the WPMW II, 20 kbps) with ha-stiebel-control 2.5.0: every signal
 * below answered, values compared against the WPMW II display where the display shows them.
 *
 * CAN members on this unit (full bus scans of all members):
 *   KESSEL         0x180  heat pump: temperatures, setpoints, status words
 *   MANAGER        0x480  WPMW II: settings, time programs, runtimes, heating circuit 1
 *   HEIZMODUL      0x500  answers almost only with placeholders (0x00FF/0xFF00): no IWS values,
 *                         so no energy counters, COP, pressures or compressor runtimes
 *   MISCHERMODUL_2 0x601  mirrors the manager (heating circuit 1) - not requested
 *   MISCHERMODUL_3 0x602  heating circuit 2 (mixer circuit): room setpoints, heating curve and
 *                         time program match the display; most other indices of this member
 *                         return shifted or meaningless values
 *   BEDIENMODUL_3  0x302  same answers as MISCHERMODUL_3 (only MISCHER_ZU used)
 *
 * Notes:
 * - RAUMSOLLTEMP_II/III on the manager are comfort levels, not heating circuits (both 20.0 °C
 *   here, while circuit 2 is set to 26.0 °C); circuit 2 lives on 0x602.
 * - VORLAUFISTTEMP/VORLAUFSOLLTEMP on KESSEL are the mixer flow of circuit 2 (same values as on
 *   0x601/0x602 when read at the same moment; matches the display "Mischer-Ist").
 * - FEHLERMELDUNG on MANAGER and KESSEL is a constant 69 here as well (see the WPL13E table);
 *   only the heat pump module is requested.
 * - Summer mode: display parameter 8 (16.0 °C here) is SW_AUSSENTEMP (0x01bf).
 *   SOMMERBETRIEB_TEMP (0xfdb5) reads 10 °C on this unit; its meaning is unclear.
 * - Left out: unconnected sensors (-40.0: SAMMLERISTTEMP, WPVORLAUFIST on KESSEL),
 *   SPEICHER_STATUS and BETRIEBSART_WP (blacklisted), MISCHERMODUL_1 (0x600, no answer).
 */

#ifndef SIGNAL_REQUESTS_TTL33_H
#define SIGNAL_REQUESTS_TTL33_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // TTL33-SPECIFIC: temperatures and setpoints
    // ========================================================================
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel},
    {"SPEICHERISTTEMP",            FREQ_30S,   cm_kessel},
    {"SPEICHERSOLLTEMP",           FREQ_30S,   cm_kessel},   // currently valid DHW target
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_kessel},   // circuit 1 return (buffer bottom)
    {"VORLAUFISTTEMP",             FREQ_30S,   cm_kessel},   // circuit 2 mixer flow
    {"VORLAUFSOLLTEMP",            FREQ_1MIN,  cm_kessel},   // circuit 2 mixer flow target
    {"KESSELSOLLTEMP",             FREQ_30S,   cm_kessel},   // buffer target
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_10MIN, cm_manager},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_10MIN, cm_manager},

    // ========================================================================
    // Heating circuit 1 (manager)
    // ========================================================================
    {"RAUMSOLLTEMP_I",             FREQ_10MIN, cm_manager},
    {"RAUMSOLLTEMP_NACHT",         FREQ_10MIN, cm_manager},
    {"HEIZKURVE",                  FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MO",              FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DI",              FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MI",              FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DO",              FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_FR",              FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SA",              FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SO",              FREQ_10MIN, cm_manager},

    // ========================================================================
    // Heating circuit 2, mixer circuit (MISCHERMODUL_3, 0x602)
    // ========================================================================
    {"RAUMSOLLTEMP_I",             FREQ_10MIN, cm_mischermodul_3},
    {"RAUMSOLLTEMP_NACHT",         FREQ_10MIN, cm_mischermodul_3},
    {"HEIZKURVE",                  FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MO",              FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DI",              FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MI",              FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DO",              FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_FR",              FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SA",              FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SO",              FREQ_10MIN, cm_mischermodul_3},

    // ========================================================================
    // DHW time program (manager)
    // ========================================================================
    {"W_WASSERPROG_1_MO",          FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DI",          FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_MI",          FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DO",          FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_FR",          FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SA",          FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SO",          FREQ_10MIN, cm_manager},

    // ========================================================================
    // SETTINGS (WPMW II parameters)
    // ========================================================================
    {"GEBAEUDEART",                FREQ_10MIN, cm_manager},
    {"ANTILEGIONELLEN",            FREQ_10MIN, cm_manager},
    {"SW_AUSSENTEMP",              FREQ_10MIN, cm_manager},  // summer mode, display parameter 8
    {"SOMMERBETRIEB_TEMP",         FREQ_10MIN, cm_manager},  // meaning unclear on this unit
    {"SPERRZEIT",                  FREQ_10MIN, cm_manager},
    {"SCHALTWERKDYNAMIKZEIT",      FREQ_10MIN, cm_manager},  // display "Reglerdynamik" (writable number)
    {"STILLSTANDZEIT",             FREQ_10MIN, cm_manager},
    {"HYSTERESEZEIT",              FREQ_10MIN, cm_manager},  // display "WW-Hysterese" (changed on the display)
    {"MAX_TEMP_KESSEL",            FREQ_10MIN, cm_manager},  // display "Ruecklauf-Max" (changed on the display)
    {"BIVALENTPARALLELTEMPERATUR_HZG",   FREQ_10MIN, cm_manager},
    {"BIVALENTPARALLELTEMPERATUR_WW",    FREQ_10MIN, cm_manager},
    {"BIVALENZALTERNATIVTEMPERATUR_HZG", FREQ_10MIN, cm_manager},
    {"BIVALENZALTERNATIVTEMPERATUR_WW",  FREQ_10MIN, cm_manager},
    {"AUSSEN_FROSTTEMP",           FREQ_10MIN, cm_manager},

    // ========================================================================
    // RUNTIMES (manager)
    // ========================================================================
    {"LAUFZEIT_WP1",               FREQ_10MIN, cm_manager},
    {"LAUFZEIT_WP2",               FREQ_10MIN, cm_manager},
    {"LAUFZEIT_2WE",               FREQ_10MIN, cm_manager},  // electric booster

    // ========================================================================
    // STATUS (bit fields; meaning partly unknown)
    // ========================================================================
    {"KESSELSTATUS",               FREQ_1MIN,  cm_kessel},
    {"PUMPENSTATUS",               FREQ_1MIN,  cm_kessel},
    {"HZK_PUMPE",                  FREQ_1MIN,  cm_kessel},
    {"WP_PUMPENSTATUS",            FREQ_1MIN,  cm_kessel},
    {"ZWEITER_WE_STATUS",          FREQ_1MIN,  cm_manager},
    {"HEIZKREIS_STATUS",           FREQ_1MIN,  cm_manager},         // circuit 1
    {"HEIZKREIS_STATUS",           FREQ_1MIN,  cm_mischermodul_3},  // circuit 2
    {"MISCHER_AUF",                FREQ_1MIN,  cm_mischermodul_3},
    {"MISCHER_ZU",                 FREQ_1MIN,  cm_bedienmodul_3},
    {"FEHLERMELDUNG",              FREQ_1MIN,  cm_heizmodul},       // fault code, decoded via ErrorList
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_TTL33_H

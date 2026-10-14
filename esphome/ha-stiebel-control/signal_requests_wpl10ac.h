/*
 * Signal Request Table — WPL10 AC Heat Pump
 *
 * IMPORTANT — bus and bitrate:
 *   On the WPL 10 AC + WPM3 the full data set (DHW/boiler temperature, setpoints)
 *   is only available on the WPM3's CAN B / display connection at 50 kBit/s. On
 *   CAN A (20 kBit/s) the Boiler does not answer read requests. Set can_bit_rate
 *   to 50kbps and tap the CAN B / display connector.
 *
 * NOTE — sender:
 *   The Boiler (0x180) originally only answered DHW/setpoint reads sent as
 *   FES_COMFORT (0x100), but impersonating the WPM3 display destabilised the bus
 *   (bus-off after prolonged operation). The 0x100 sender override was removed and
 *   all reads use the normal ComfortSoft sender (0x680), which the Boiler answers
 *   on the CAN B / display connection.
 *
 * Member layout (verified via capture on CAN B / display):
 *   - KESSEL (0x180):  outside temp, DHW actual/setpoint, room setpoints
 *   - MANAGER (0x480): clock, operating mode, EVU lock
 *   - HEIZMODUL (0x500): compressor/defrost state, energy counters
 *   - RAUMFERNFUEHLER_2 (0x401): room actual temperature (0x4ec7), humidity (0x4ec8)
 *
 * See signal_requests_base.h for the base macro definition.
 */

#ifndef SIGNAL_REQUESTS_WPL10AC_H
#define SIGNAL_REQUESTS_WPL10AC_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // BOILER / KESSEL (0x180)
    // ========================================================================
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel},
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_kessel},
    {"HILFSKESSELSOLL",            FREQ_30S,   cm_kessel},
    {"KESSELSOLLTEMP",             FREQ_30S,   cm_kessel},
    {"SPEICHERISTTEMP",            FREQ_30S,   cm_kessel},   // DHW actual temperature
    {"SPEICHERSOLLTEMP",           FREQ_10MIN, cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_10MIN, cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_10MIN, cm_kessel},

    // ========================================================================
    // HEATING MODULE (0x500) — compressor/defrost state, verified
    // ========================================================================
    {"VERDAMPFERTEMP",             FREQ_30S,   cm_heizmodul},
    {"ABTAUUNGAKTIV",              FREQ_1MIN,  cm_heizmodul},

    // ========================================================================
    // ROOM SETPOINTS — single member (MANAGER) to avoid duplicate/-255 entities
    // ========================================================================
    {"RAUMSOLLTEMP_I",             FREQ_30S,   cm_manager},
    {"RAUMSOLLTEMP_NACHT",         FREQ_30S,   cm_manager},

    // ========================================================================
    // ENERGY COUNTERS (HEIZMODUL 0x500) — verified
    // ========================================================================
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH",  FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_KWH",   FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_WH",    FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_KWH",   FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_SUM_KWH",        FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_TAG_WH",         FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_TAG_KWH",        FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_SUM_KWH",          FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_TAG_WH",           FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_TAG_KWH",          FREQ_10MIN, cm_heizmodul},

    // ========================================================================
    // MAINTENANCE / MONITORING — pressure, flow, runtimes
    // (member/frequency best-guess; prune what the WPM3 does not answer)
    // ========================================================================
    {"WASSERDRUCK",                      FREQ_1MIN,  cm_kessel},    // heating circuit water pressure
    {"DURCHFLUSS_CH",                    FREQ_1MIN,  cm_kessel},    // heating flow
    {"DURCHFLUSS_WW",                    FREQ_1MIN,  cm_kessel},    // DHW flow
    {"LAUFZEIT_VERDICHTER_HEIZEN",       FREQ_10MIN, cm_heizmodul}, // compressor hours heating
    {"LAUFZEIT_VERDICHTER_KUEHLEN",      FREQ_10MIN, cm_heizmodul}, // compressor hours cooling
    {"LAUFZEIT_VERDICHTER_WW",           FREQ_10MIN, cm_heizmodul}, // compressor hours DHW
    {"LAUFZEIT_NHZ_HEIZEN",              FREQ_10MIN, cm_heizmodul}, // backup heater hours heating
    {"LAUFZEIT_NHZ_WW",                  FREQ_10MIN, cm_heizmodul}, // backup heater hours DHW
    {"ANZEIGE_HOCHDRUCK",                FREQ_1MIN,  cm_heizmodul}, // refrigerant high pressure
    {"ANZEIGE_NIEDERDRUCK",              FREQ_1MIN,  cm_heizmodul}, // refrigerant low pressure

    // ---- Power candidates (test whether the WPM3 answers with a live value) ----
    {"KESSELLEISTUNG",                   FREQ_1MIN,  cm_kessel},    // boiler/heating power
    {"NHZ_LEISTUNG1",                    FREQ_1MIN,  cm_heizmodul}, // backup heater power
    {"LEISTUNG_PUMPE_HZK",               FREQ_1MIN,  cm_kessel},    // heating pump power
    {"WAERMEMENGE",                      FREQ_1MIN,  cm_manager},   // heat meter reading
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL10AC_H
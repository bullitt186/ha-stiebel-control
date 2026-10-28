/*
 * Signal Request Table — WPL 09 ACS classic / WPM3 / FET (draft)
 *
 * Based on the existing WPL10AC polling set and local WPL9 adjustments.
 * CAN B / WPM3 display connection, 50 kbps, normal PC sender 0x680.
 * FET temperature/humidity (0x4ec7/0x4ec8, member 0x401) are received passively.
 * This is a candidate polling set, NOT a claim that every register is readable.
 * Test 0x0013/0x0a06 reads at 0x201; valid live replies still need evidence.
 * See docs/WPL9.md for the draft route table and validation checklist.
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

    // DHW settings: test CAN-B feedback, consistent with writable discovery.
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_10MIN, cm_kessel_canb},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_10MIN, cm_kessel_canb},

    // ========================================================================
    // HEATING MODULE (0x500) — inherited candidate polling set
    // ========================================================================
    {"VERDAMPFERTEMP",             FREQ_30S,   cm_heizmodul},
    {"ABTAUUNGAKTIV",              FREQ_1MIN,  cm_heizmodul},

    // ========================================================================
    // ROOM SETPOINTS — single member (MANAGER) to avoid duplicate/-255 entities
    // ========================================================================
    {"RAUMSOLLTEMP_I",             FREQ_30S,   cm_manager},
    {"RAUMSOLLTEMP_NACHT",         FREQ_30S,   cm_manager},
    {"SPEICHERSOLLTEMP",           FREQ_10MIN, cm_manager},
    // Restore the previously used MANAGER return-temperature source.
    // KESSEL returned fresh -255; MANAGER freshness needs checking after flashing.
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_manager},

    // ========================================================================
    // ENERGY COUNTERS (HEIZMODUL 0x500) — inherited candidate polling set
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
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL9_H

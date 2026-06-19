/*
 * Signal Request Table — WPC7 / Stiebel WPC-07cool / Tecalor TTF07c
 *
 * UNVERIFIED ON REAL HARDWARE. Ported from community project
 * kr0ner/OneESP32ToRuleThemAll (CAN register map for their TTF07c model),
 * re-implemented against this project's ElsterTable.h/signal_requests
 * conventions — no code or YAML was copied verbatim. See issue #2 and #21
 * for community thread. Please test against a real WPC7/TTF07c unit and
 * report back (or submit corrections) before relying on this for control.
 *
 * CAN targets confirmed from the source project: every TTF07c signal is
 * queried from either Kessel (0x180) or HK1 (0x301, our BEDIENMODUL_2) —
 * no new CanMembers entries were needed for this model.
 *
 * Known gaps (see docs/CONTRIBUTING.md "mark unverified signals"):
 * - VERDICHTER (compressor power, base signal) is queried against
 *   cm_heizmodul (0x500) in SIGNAL_REQUESTS_BASE, which doesn't exist as a
 *   physical node on WPC7. The source project instead derives a boolean
 *   "compressor running" from bit 3 of BETRIEBS_STATUS_WPC7 — a different
 *   signal entirely. No direct compressor-power equivalent is known for
 *   WPC7 yet; BETRIEBS_STATUS_WPC7 is requested below as a raw bitmask for
 *   future per-bit decoding.
 * - The 6 base energy-counter signals (EL_AUFNAHMELEISTUNG_*, WAERMEERTRAG_*)
 *   are also targeted at cm_heizmodul in the base set and likely won't
 *   respond on WPC7 either; COP sensors will probably stay unpublished
 *   until this is verified/fixed by someone with real hardware.
 * - A few signals below reuse existing ElsterTable.h rows whose Type
 *   differs from what the source project documents for TTF07c (index
 *   matches, type doesn't) — flagged inline. The existing Type was kept
 *   as-is since other models may depend on it; values may be mis-scaled.
 */

#ifndef SIGNAL_REQUESTS_WPC7_H
#define SIGNAL_REQUESTS_WPC7_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters

    // ========================================================================
    // WPC7-SPECIFIC: signals queried from KESSEL (0x180)
    // ========================================================================
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel},
    {"SPEICHERSOLLTEMP",           FREQ_30S,   cm_kessel},
    {"SPEICHERISTTEMP",            FREQ_30S,   cm_kessel},
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_kessel},
    {"QUELLE_IST",                 FREQ_30S,   cm_kessel},
    {"HEISSGAS_TEMP",              FREQ_1MIN,  cm_kessel},
    {"WW_HYSTERESE",               FREQ_10MIN, cm_kessel},
    {"VOLUMENSTROM_WPC7",          FREQ_30S,   cm_kessel},
    {"DRUCK_HEIZKREIS",            FREQ_30S,   cm_kessel},
    {"QUELLENDRUCK",               FREQ_30S,   cm_kessel},
    {"KUEHLEN_EINGESCHALTET",      FREQ_1MIN,  cm_kessel},
    {"LEISTUNG_HEIZKREISPUMPE",    FREQ_1MIN,  cm_kessel},
    {"LEISTUNG_WARMWASSERPUMPE",   FREQ_1MIN,  cm_kessel},
    {"LEISTUNG_SOLEPUMPE",         FREQ_1MIN,  cm_kessel},
    {"EINPHASIGER_BETRIEB",        FREQ_10MIN, cm_kessel},
    {"VERDICHTER_STARTS",          FREQ_10MIN, cm_kessel},
    {"VERDICHTER_STARTS_K",        FREQ_10MIN, cm_kessel},
    {"VERDICHTER_STILLSTAND",      FREQ_1MIN,  cm_kessel},
    {"REGLERDYNAMIK",              FREQ_10MIN, cm_kessel},
    {"BETRIEBS_STATUS_WPC7",       FREQ_30S,   cm_kessel},  // bitmask register, see header comment
    {"PROGRAMMSCHALTER",           FREQ_10MIN, cm_kessel},  // source project targets Kessel; base already queries cm_manager too

    // Index matches an existing ElsterTable row but the source project
    // documents a different Type — reusing the existing row as-is (see
    // header comment). Values may be mis-scaled until verified.
    {"MAX_WASSERDRUCK",            FREQ_10MIN, cm_kessel},  // TTF07c: HDSENSORMAX, et_dec_val expected (row has et_default)
    {"MASCHINENDRUCK",             FREQ_30S,   cm_kessel},  // TTF07c: DRUCK_HOCHDRUCK, et_cent_val expected (row has et_default)
    {"MATERIALNUMMER_HIGH",        FREQ_30S,   cm_kessel},  // TTF07c: HKISTTEMP, et_dec_val expected (row has et_default) — name mismatch too, low confidence
    {"MESSSTROM_NIEDERDRUCK",      FREQ_30S,   cm_kessel},  // TTF07c: DRUCK_NIEDERDRUCK, et_cent_val expected (row has et_default)

    // Index + Type match an existing row, name differs — same underlying
    // Elster signal, just curated under a different name already.
    {"PUFFERTEMP_UNTEN1",          FREQ_1MIN,  cm_kessel},  // TTF07c: PUFFERISTTEMP
    {"PUFFERSOLL",                 FREQ_1MIN,  cm_kessel},  // TTF07c: PUFFERSOLLTEMP
    {"MAX_HEIZUNG_TEMP",           FREQ_10MIN, cm_kessel},  // TTF07c: MAXVORLAUFTEMP — not queried by source YAML, untested guess
    {"HILFSKESSELSOLL",            FREQ_1MIN,  cm_kessel},  // TTF07c: HKSOLLTEMP
    {"QUELLENSOLLTEMPERATUR",      FREQ_1MIN,  cm_kessel},  // TTF07c: QUELLENTEMP_MIN
    {"AUSSEN_FROSTTEMP",           FREQ_10MIN, cm_kessel},  // TTF07c: ANLAGENFROST — not queried by source YAML, untested guess
    {"WP_STATUS",                  FREQ_10MIN, cm_kessel},  // TTF07c: QUELLENMEDIUM, et_byte expected (row has et_little_endian) — not queried by source YAML, untested guess

    // Writable controls already wired up generically in common.yaml —
    // querying them here makes the existing number entities work on WPC7.
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_30S,   cm_kessel},  // TTF07c: WW_KOMF_TEMP — not queried by source YAML, untested guess
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_30S,   cm_manager},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_30S,   cm_kessel},  // TTF07c: WW_ECO_TEMP — not queried by source YAML, untested guess
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_30S,   cm_manager},

    // Same Elster index as HEIZKURVE (0x010e) — TTF07c calls it STEIGUNG_HK1
    {"HEIZKURVE",                  FREQ_10MIN, cm_bedienmodul_2},

    // ========================================================================
    // WPC7-SPECIFIC: signals queried from HK1 / BEDIENMODUL_2 (0x301)
    // ========================================================================
    {"VORLAUFISTTEMP_WPC7",        FREQ_30S,   cm_bedienmodul_2},
    {"RAUMISTTEMP",                FREQ_1MIN,  cm_bedienmodul_2},
    {"VERSTELLTE_RAUMSOLLTEMP",    FREQ_1MIN,  cm_bedienmodul_2},
    {"VORLAUFSOLLTEMP",            FREQ_1MIN,  cm_bedienmodul_2},
    {"RAUMFEUCHTE",                FREQ_10MIN, cm_bedienmodul_2},
    {"RAUMEINFLUSS",               FREQ_10MIN, cm_bedienmodul_2},  // TTF07c expects et_little_endian (row has et_default) — unverified
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPC7_H

/*
 * Signal Request Table — WPL17 Heat Pump
 *
 * VERIFIED ON REAL HARDWARE (CAN-B, X1.2, 50 kbps, 2026-08-30/31).
 *
 * WPL17 has TWO CAN buses:
 *  - CAN-A (20 kbps): heating-level data (Vorlauf/Ruecklauf/Aussentemp/Verdichter),
 *    Kessel=0x180, Manager=0x480, Heizmodul=0x500.
 *  - CAN-B (X1.2, 50 kbps): the COMFORT level — Speicher-Ist-Temp, Raum-Soll-Temp,
 *    Heizkurve, Energien, Betriebsmodus. THIS is where the ISG-Web is connected
 *    and where the values the display shows are readable.
 *
 * To use the WPL17 with this project, connect to CAN-B and set
 * `can_bit_rate: 50kbps` in heatingpump.yaml substitutions.
 *
 * CAN-B member addresses (verified on a real WPL17 ICS classic):
 *   KESSEL_CANB   0x201 — Speicher-Ist/Soll-Temp, Raum-Soll-Temp, Heizkurve, Pumpen, Status
 *   MANAGER_CANB  0x480 — Programm-Schalter, Sommerbetrieb, Speicher-Soll, Raum-Soll,
 *                         Heizgrenzen, Ferien, Takt, WP-Status
 *   HEIZMODUL_CANB 0x500 — Energien (Strom/Waerme MWh+kWh), Verdichter, Vorlauf/Ruecklauf,
 *                         Drehzahlen, Laufzeiten, Kuehl-Zustand
 *
 * ANPASSEN FUER DEIN GERAET (How to customise):
 *  -------------------------------------------------
 *  - Jede Zeile hier ist EIN Wert, den die WPL17 abfragt und als HA-Entity publiziert.
 *  - Moechtest du einen Wert NICHT, kommentiere die Zeile aus (// davor).
 *  - Werte, die -255 liefern, sind am Geraet nicht vorhanden/aktiv — auskommentieren.
 *  - Kuehl-Register liefern -255, bis die Kuehlfunktion am Display freigeschaltet ist
 *    (Menue > Code 1000). Sie sind hier default AUSKOMMENTIERT.
 *  - Frequenz (FREQ_30S / FREQ_1MIN / FREQ_10MIN): wie oft abgefragt wird.
 *    Schnell = aktueller, aber mehr CAN-Buslast. Langsam = schonender.
 *
 * VERIFIZIERT (liefern echte Werte auf dem Testgeraet):
 *  - SPEICHERISTTEMP, EINSTELL_SPEICHERSOLLTEMP, RAUMSOLLTEMP_I (= Komforttemp),
 *    RAUMSOLLTEMP_NACHT (= Eco temp), HEIZKURVE (= Steigung, z.B. 0.40), AUSSENTEMP (vom Kessel),
 *    WPVORLAUFIST, RUECKLAUFISTTEMP, VERDICHTER, SOMMERBETRIEB, PROGRAMMSCHALTER,
 *    SG-Ready, Energiezähler (EL und WAERMEERTRAG), kombinierte kWh-Sensoren.
 *  - RAUMSOLLTEMP_II/_III liefern -255: dieses Geraet hat nur 1 Heizkreis.
 *
 * NICHT VERIFIZIERT / OPTIONAL (auskommentiert — aktivieren und testen, ob das Geraet
 *  den Wert liefert; falls -255, wieder auskommentieren):
 *  - Pumpen/Status (HZK_PUMPE, SPL_PUMPE, ZIRK_PUMPE, PUMPENSTATUS, HEIZKREIS_STATUS,
 *    SPEICHER_STATUS), WASSERDRUCK, VORLAUFSOLLTEMP, ERTRAG_* (Wärmeleistung),
 *    WW_DURCHLADEN, WW_ECO, Heizgrenzen, Ferien-Daten, WP_STATUS, WP_PUMPENSTATUS,
 *    ISTDREHZAHL/SOLLDREHZAHL_VERDICHTER, LAUFZEIT_VERDICHTER_*.
 *
 * Writables (HEIZKURVE, EINSTELL_SPEICHERSOLLTEMP, RAUMSOLLTEMP_I/NACHT,
 * PROGRAMMSCHALTER) are defined in ha-stiebel-control.h writableNumbers[] /
 * writableSelects[] and are addressed via the MQTT set-topics; the read-requests
 * here keep their state_topics populated.
 */

#ifndef SIGNAL_REQUESTS_WPL17_H
#define SIGNAL_REQUESTS_WPL17_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // Datum/Uhrzeit, EVU-Sperre, Betriebsart, Energiezähler

    // ========================================================================
    // WPL17 (CAN-B): KESSEL_CANB (0x201) — Speicher/Raum/Heizkurve/Aussentemp
    // ========================================================================
    {"AUSSENTEMP",                 FREQ_30S,   cm_kessel_canb},
    {"SPEICHERISTTEMP",            FREQ_30S,   cm_kessel_canb},
    {"SPEICHERSOLLTEMP",           FREQ_1MIN,  cm_kessel_canb},
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_1MIN,  cm_kessel_canb},
    {"RAUMSOLLTEMP_I",             FREQ_1MIN,  cm_kessel_canb},   // Komforttemperatur
    {"RAUMSOLLTEMP_NACHT",         FREQ_1MIN,  cm_kessel_canb},   // Eco-Temperatur
    {"HEIZKURVE",                  FREQ_10MIN, cm_kessel_canb},   // Steigung (et_cent_val, 0.40)

    // --- OPTIONAL: Werte, die das Geraet evtl. liefert (testen, -255 -> auskommentieren) ---
    // AUSSENTEMP + VORLAUFSOLLTEMP liegen auf dem Kessel (0x180). Die Pumpen/Status-
    // Register werden ebenfalls vom Kessel (0x180) getestet (KESSEL_CANB 0x201 lieferte -255).
    // VERIFIZIERT auf Kessel (0x180): VORLAUFSOLLTEMP, HZK_PUMPE, PUMPENSTATUS,
    // HEIZKREIS_STATUS, ERTRAG_AKT, ERTRAG_TAG_*, WW_ECO (alle liefern echte Werte,
    // bei WP aus = 0 bzw. WW_ECO=on).
    {"VORLAUFSOLLTEMP",           FREQ_10MIN, cm_kessel},       // berechnete Vorlauf-Solltemperatur
    {"HZK_PUMPE",                 FREQ_30S,   cm_kessel},       // Heizkreis-Pumpe an/aus (0/1)
    {"PUMPENSTATUS",              FREQ_30S,   cm_kessel},       // Bitfeld: welche Pumpe läuft
    {"HEIZKREIS_STATUS",          FREQ_30S,   cm_kessel},       // Bitfeld Heizkreis-Status
    {"WW_ECO",                    FREQ_1MIN,  cm_kessel},       // WW-Eco (reduzierte WW-Temp)
    {"ERTRAG_AKT",                FREQ_30S,   cm_kessel},       // aktuelle Wärmeleistung (W)
    {"ERTRAG_TAG_W",              FREQ_10MIN, cm_kessel},       // Wärmeertrag heute (Wh-Anteil)
    {"ERTRAG_TAG_KW",             FREQ_10MIN, cm_kessel},       // Wärmeertrag heute (kWh-Anteil)

    // -255 auf Kessel (0x180) — liegen vermutlich auf einem anderen Teilnehmer:
    // {"SPL_PUMPE",               FREQ_30S,   cm_kessel},       // Speicherlade-/Sole-Pumpe  (-255)
    // {"ZIRK_PUMPE",              FREQ_30S,   cm_kessel},       // Zirkulationspumpe          (-255)
    // {"WW_DURCHLADEN",           FREQ_1MIN,  cm_kessel},       // WW-Schnellladung           (-255)
    // {"WASSERDRUCK",             FREQ_10MIN, cm_kessel},       // Heizungswasserdruck        (-255)

    // --- EINSTELLREGISTER gegen KESSEL (0x180) getestet — liefern auf der Komfortebene -255.
    // Analog AUSSENTEMP/VORLAUFSOLLTEMP koennten diese Heizungs-Einstellwerte auf dem
    // Kessel liegen. Auskommentieren wenn -255.
    {"HEIZGRENZE_TAG",            FREQ_10MIN, cm_kessel},       // Heizgrenze Tag
    {"HEIZGRENZE_NACHT",          FREQ_10MIN, cm_kessel},       // Heizgrenze Nacht
    {"TAKTSPERRE",                FREQ_10MIN, cm_kessel},       // Min.-Ausschaltzeit Verdichter
    {"WW_HYSTERSE",               FREQ_10MIN, cm_kessel},       // WW-Hysterese
    {"RUECKLAUF_HYSTERESE",       FREQ_10MIN, cm_kessel},       // Rücklauf-Hysterese
    {"WW_BETRIEB",                FREQ_1MIN,  cm_kessel},       // WW-Betriebsmodus
    {"AKTIVES_HEIZPROGRAMM",      FREQ_1MIN,  cm_kessel},       // welches Heizprogramm
    {"ECO_BETRIEB",               FREQ_1MIN,  cm_kessel},       // Eco-Betrieb
    {"ECO_EIN_AUS",               FREQ_1MIN,  cm_kessel},       // Eco-Schalter
    {"PARTY_EIN_AUS",             FREQ_1MIN,  cm_kessel},       // Party-Schalter
    {"FERIENBETRIEB",             FREQ_1MIN,  cm_kessel},       // Ferienmodus
    {"FERIEN_ABSENKTEMP",         FREQ_10MIN, cm_kessel},       // Ferien-Absenktemp
    {"BIVALENTPARALLELTEMPERATUR_HZG", FREQ_10MIN, cm_kessel},  // Bivalenzpunkt Heizung
    {"BIVALENTPARALLELTEMPERATUR_WW",  FREQ_10MIN, cm_kessel},  // Bivalenzpunkt WW
    {"WP_PUMPENSTATUS",           FREQ_30S,   cm_kessel},       // Bitfeld Pumpenstatus

    // ========================================================================
    // WPL17 (CAN-B): MANAGER_CANB (0x480) — Betriebsmodus + Speicher/Raum/Heizkurve
    // ========================================================================
    {"PROGRAMMSCHALTER",           FREQ_1MIN,  cm_manager},  // Automatik/Tagbetrieb/etc.
    {"SOMMERBETRIEB",              FREQ_1MIN,  cm_manager},  // Heizung aus, nur WW (on/off)
    {"EINSTELL_SPEICHERSOLLTEMP",  FREQ_1MIN,  cm_manager},
    {"RAUMSOLLTEMP_I",             FREQ_1MIN,  cm_manager},
    {"RAUMSOLLTEMP_NACHT",         FREQ_1MIN,  cm_manager},
    // HEIZKURVE nur vom KESSEL_CANB abfragen (0x201) - dort liegt der schreibbare
    // Wert. Der Manager liefert einen redundanten/veralteten Wert (0.4).

    // --- OPTIONAL: Betriebs-/Statuswerte (testen, -255 -> auskommentieren) ---
    // VERIFIZIERT auf Manager (0x480): WP_STATUS, ANTILEGIONELLEN. Die anderen
    // Einstellregister liefern auf der Komfortebene -255 (getestet) — sie werden
    // gegen den Kessel (0x180) getestet (Abschnitt KESSEL weiter unten).
    {"WP_STATUS",                 FREQ_30S,   cm_manager},  // Bitfeld Betriebsstatus (Verdichter/Heiz/WW/Kühl) ✓
    {"ANTILEGIONELLEN",           FREQ_1MIN,  cm_manager},  // Legionellenschutz aktiv ✓ (0)

    // -255 auf Manager_CANB (Komfortebene):
    // {"WP_PUMPENSTATUS",           FREQ_30S,   cm_manager},  // Bitfeld Pumpenstatus  (-255)
    // {"AKTIVES_HEIZPROGRAMM",      FREQ_1MIN,  cm_manager},  // welches Heizprogramm   (-255)
    // {"ECO_BETRIEB",               FREQ_1MIN,  cm_manager},  // Eco-Betrieb            (-255)
    // {"ECO_EIN_AUS",               FREQ_1MIN,  cm_manager},  // Eco-Schalter           (-255)
    // {"PARTY_EIN_AUS",             FREQ_1MIN,  cm_manager},  // Party-Schalter         (-255)
    // {"HEIZGRENZE_TAG",            FREQ_10MIN, cm_manager},  // Heizgrenze Tag         (-255)
    // {"HEIZGRENZE_NACHT",          FREQ_10MIN, cm_manager},  // Heizgrenze Nacht       (-255)
    // {"TAKTSPERRE",                FREQ_10MIN, cm_manager},  // Min.-Ausschaltzeit     (-255)
    // {"WW_BETRIEB",                FREQ_1MIN,  cm_manager},  // WW-Betriebsmodus       (-255)
    // {"WW_HYSTERSE",               FREQ_10MIN, cm_manager},  // WW-Hysterese           (-255)
    // {"RUECKLAUF_HYSTERESE",       FREQ_10MIN, cm_manager},  // Rücklauf-Hysterese     (-255)
    // {"SOLL_DIFFERENZ_RUECKLAUF",  FREQ_10MIN, cm_manager},  // Soll-Diff. Rücklauf    (-255)
    // {"MAX_DIFFERENZ_RUECKLAUF",   FREQ_10MIN, cm_manager},  // Max-Diff. Rücklauf     (-255)
    // {"FERIENBETRIEB",             FREQ_1MIN,  cm_manager},  // Ferienmodus            (-255)
    // {"FERIEN_ABSENKTEMP",         FREQ_10MIN, cm_manager},  // Ferien-Absenktemp      (-255)
    // {"BIVALENTPARALLELTEMPERATUR_HZG", FREQ_10MIN, cm_manager},  // Bivalenzpunkt Heizung  (-255)
    // {"BIVALENTPARALLELTEMPERATUR_WW",  FREQ_10MIN, cm_manager},  // Bivalenzpunkt WW       (-255)

    // ========================================================================
    // WPL17 (CAN-B): HEIZMODUL_CANB (0x500) — Energien + Vorlauf/Ruecklauf/Verdichter
    // ========================================================================
    {"WPVORLAUFIST",               FREQ_30S,   cm_heizmodul},
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_heizmodul},
    {"VERDICHTER",                 FREQ_30S,   cm_heizmodul},

    // Energie-Register (MWh = et_double_val, kWh = separate Register).
    // Die kombinierten "Gesamt kWh" berechneten Sensoren (MWh*1000+kWh) werden
    // in ha-stiebel-control.h aus diesen Werten abgeleitet.
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_MWH",   FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_KWH",   FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_SUM_MWH",        FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_SUM_KWH",        FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_SUM_MWH",          FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_SUM_KWH",          FREQ_10MIN, cm_heizmodul},

    // --- AKTUELLE ELEKTRISCHE LEISTUNG (W): aus feinen Tageszählern (Wh) abgeleitet ---
    // Diese Wh-Zähler werden alle 30s gelesen; aus der Differenz pro Zeit berechnet
    // ha-stiebel-control.h den berechneten Sensor "Aktuelle elektrische Leistung" (W).
    // HINWEIS: Die TAG_WH-Zaehler liegen auf dem WPL17-Heizmodul (0x514), das sie
    // passiv sendet (sensor.heizmodul_wpl_*). Von 0x500 lieferten sie 0.
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH", FREQ_30S, cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_WH",   FREQ_30S, cm_heizmodul_wpl},

    // --- OPTIONAL: Verdichter-Drehzahl / Laufzeiten (testen, -255 -> auskommentieren) ---
    // {"ISTDREHZAHL_VERDICHTER_1",   FREQ_30S,   cm_heizmodul},  // Ist-Drehzahl Verdichter (U/min)
    // {"SOLLDREHZAHL_VERDICHTER_1",  FREQ_30S,   cm_heizmodul},  // Soll-Drehzahl Verdichter (U/min)
    // {"LAUFZEIT_VERDICHTER_HEIZEN", FREQ_10MIN, cm_heizmodul},  // Laufzeit Verdichter Heizbetrieb
    // {"LAUFZEIT_VERDICHTER_WW",     FREQ_10MIN, cm_heizmodul},  // Laufzeit Verdichter WW
    // {"LAUFZEIT_VERDICHTER_KUEHLEN",FREQ_10MIN, cm_heizmodul},  // Laufzeit Verdichter Kühlbetrieb
    // {"LAUFZEIT_NHZ_HEIZEN",        FREQ_10MIN, cm_heizmodul},  // Laufzeit Elektrostab (NHZ) Heizen
    // {"LAUFZEIT_NHZ_WW",            FREQ_10MIN, cm_heizmodul},  // Laufzeit Elektrostab (NHZ) WW

    // --- PUMPEN-TEST gegen HEIZMODUL (0x500) — SPL/ZIRK lieferten -255 (wie auf Kessel).
    // Diese Register sind auf den getesteten Teilnehmern (0x201/0x180/0x500) nicht vorhanden.
    // Moeglicherweise stecken sie im PUMPENSTATUS-Bitfeld des Kessels (0x180).
    // {"SPL_PUMPE",                 FREQ_30S,   cm_heizmodul},      // Speicherlade-/Sole-Pumpe  (-255)
    // {"ZIRK_PUMPE",                FREQ_30S,   cm_heizmodul},      // Zirkulationspumpe          (-255)
    // {"WASSERDRUCK",               FREQ_10MIN, cm_heizmodul},      // Heizungswasserdruck        (-255)
    // {"WW_DURCHLADEN",             FREQ_1MIN,  cm_heizmodul},      // WW-Schnellladung           (-255)

    // ========================================================================
    // WPL17 (CAN-B): LEGIONELLEN-SCHUTZ (Anti-Legionellen Zeitpunkt)
    // ========================================================================
    // {"ANTILEGIONELLEN_ZEITPUNKT",  FREQ_1MIN,  cm_manager},  // -255 auf diesem Geraet (nicht belegt)

    // ========================================================================
    // WPL17 (CAN-B): Kuehl-Register — vorhanden, aber default deaktiviert.
    // Liefern -255 bis die Kuehlfunktion am Display (Code 1000) freigeschaltet ist.
    // Default AUSKOMMENTIERT — nach Freigabe aktivieren.
    // ========================================================================
    // {"KUEHLMODE",                 FREQ_10MIN, cm_kessel_canb},
    // {"KUEHLMODE",                 FREQ_10MIN, cm_manager},
    // {"KUEHL_RAUMSOLL_TAG",        FREQ_10MIN, cm_kessel_canb},
    // {"KUEHL_RAUMSOLL_TAG",        FREQ_10MIN, cm_manager},
    // {"KUEHL_RAUMSOLL_NACHT",      FREQ_10MIN, cm_kessel_canb},
    // {"KUEHL_VORLAUFSOLLTEMP",     FREQ_10MIN, cm_kessel_canb},
    // {"KUEHL_FREIGABE_TEMPERATUR", FREQ_10MIN, cm_kessel_canb},
    // {"KUEHLZUSTAND",              FREQ_10MIN, cm_heizmodul},  // Kühl-Zustand
    // {"KUEHLFUNKTION",             FREQ_10MIN, cm_heizmodul},  // Kühlfunktion an/aus
    // {"MAX_RL_KUEHLEN",            FREQ_10MIN, cm_heizmodul},  // max. Rücklauftemp Kühlen (Kondensat)
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL17_H
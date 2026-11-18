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
    {"WW_ECO",                     FREQ_1MIN,  cm_manager},  // DHW eco mode (was a YAML poller, #98)

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
    {"BETRIEBSART_WP",             FREQ_10MIN, cm_manager},

    // ========================================================================
    // SETTINGS (WPMW II parameters)
    // Dropped after 30 days of -255 on a real WPL13E: WW_HYSTERSE (manager+kessel;
    // WW_HYSTERESE 0x0022 does not answer either), ANTILEGIONELLEN_ZEITPUNKT.
    // HEIZKURVE on KESSEL is always 0.00 (6 months) - the real value is on MANAGER.
    // ========================================================================
    {"HEIZKURVE",                  FREQ_10MIN, cm_manager},
    {"ANTILEGIONELLEN",            FREQ_10MIN, cm_manager},
    // Fault code of the heat pump module (IWS), decoded via ErrorList (fault number in
    // the high byte). The MANAGER answers 69 = 0x0045 on this index, which is "no
    // fault" with 0x45 in the low byte - not requested.
    {"FEHLERMELDUNG",              FREQ_1MIN,  cm_heizmodul},
    {"GEBAEUDEART",                FREQ_10MIN, cm_manager},
    // Display parameters compared with the bus. 42 Reglerdynamik (0xfdb0, named
    // SCHALTWERKDYNAMIKZEIT), 43 Stillstandzeit (0xfdb1) and the summer threshold of
    // parameter 10 (0x01bf) moved with a change on the display; 38 WW-Hysterese (0x0022)
    // matches the display. 0xfdb5 SOMMERBETRIEB_TEMP (10) and 0x0026 SPERRZEIT (20) did
    // not move and are not requested.
    {"SW_AUSSENTEMP",              FREQ_30MIN, cm_manager},
    {"SCHALTWERKDYNAMIKZEIT",      FREQ_60MIN, cm_manager},
    {"STILLSTANDZEIT",             FREQ_60MIN, cm_manager},
    {"HYSTERESEZEIT",              FREQ_60MIN, cm_manager},
    // Settings and totals found by a bus scan of the manager (WPMW II)
    {"BIVALENTPARALLELTEMPERATUR_HZG",   FREQ_60MIN, cm_manager},
    {"BIVALENTPARALLELTEMPERATUR_WW",    FREQ_60MIN, cm_manager},
    {"BIVALENZALTERNATIVTEMPERATUR_HZG", FREQ_60MIN, cm_manager},
    {"BIVALENZALTERNATIVTEMPERATUR_WW",  FREQ_60MIN, cm_manager},
    {"MAX_HEIZUNG_TEMP",                 FREQ_60MIN, cm_manager},
    {"MAX_TEMP_KESSEL",                  FREQ_60MIN, cm_manager},
    {"AUSSEN_FROSTTEMP",                 FREQ_60MIN, cm_manager},
    // LAUFZEIT_WP1 (0x01c4) is not requested: the WPMW II fills it only on demand
    // (5576 h once, 0 otherwise); use LZ_VERD_2_HEIZBETRIEB + LZ_VERD_2_WW_BETRIEB.
    {"LAUFZEIT_2WE",                     FREQ_30MIN, cm_manager},  // electric booster total

    // ========================================================================
    // ENERGY: SUM_MWH (base) is whole MWh only; the kWh remainder and the
    // daily values ("WM TAG" in the WPMW II manual) refine COP and feed the
    // combined total/daily sensors.
    // ========================================================================
    SIGNAL_REQUESTS_ENERGY_IWS   // daily and total energy counters (signal_requests_base.h)

    // ========================================================================
    // DIAGNOSTICS ("INFO IWS" in the WPMW II manual)
    // Refrigerant pressures in bar (et_cent_val for WPL13E, see ElsterTable.h)
    // ========================================================================
    // Heat pump flow/return (IWS B1/B2): basis of Delta T (the WPM's X2/2 and X2/3 measure elsewhere)
    {"WPVORLAUFIST",               FREQ_30S,   cm_heizmodul},
    {"RUECKLAUFISTTEMP",           FREQ_30S,   cm_heizmodul},
    {"HEISSGAS_TEMP",              FREQ_1MIN,  cm_heizmodul},
    {"SOLLWERT_UEBERHITZUNG",            FREQ_10MIN, cm_heizmodul},
    {"ISTWERT_UEBERHITZUNG_VERDAMPFER",  FREQ_1MIN,  cm_heizmodul},
    {"STARTS_ABTAUUNG",                  FREQ_30MIN, cm_heizmodul},
    // 0 while a defrost runs (calculated abtauung_laeuft); ABTAUUNGAKTIV stays off on the
    // WPL13E during a defrost and is no longer requested.
    {"ZEITDAUER_LETZTE_ABTAUUNG",        FREQ_1MIN,  cm_heizmodul},
    {"VORLAUFSOLLTEMP",                  FREQ_1MIN,  cm_kessel},
    {"ANZEIGE_HOCHDRUCK",          FREQ_1MIN,  cm_heizmodul},
    {"ANZEIGE_NIEDERDRUCK",        FREQ_1MIN,  cm_heizmodul},
    // Bit fields that change with the compressor (measured on a WPL 13 E, 1/min scan):
    // KESSELSTATUS 0x0000 -> 0x0100 and PUMPENSTATUS bit 0 flip at start/stop of every
    // heating run. HEIZKREIS_STATUS changes with the time programs (about 23:00).
    {"KESSELSTATUS",               FREQ_30S,   cm_kessel},
    {"PUMPENSTATUS",               FREQ_30S,   cm_kessel},
    {"HEIZKREIS_STATUS",           FREQ_30S,   cm_kessel},
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

    // ========================================================================
    // DHW TIME PROGRAM (manager): three periods per day, shown as one text per
    // day and writable from Home Assistant (#82)
    // ========================================================================
    {"W_WASSERPROG_1_MO",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_MO_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_MO_SCHALT_3",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DI",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DI_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DI_SCHALT_3",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_MI",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_MI_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_MI_SCHALT_3",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DO",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DO_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_DO_SCHALT_3",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_FR",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_FR_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_FR_SCHALT_3",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SA",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SA_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SA_SCHALT_3",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SO",           FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SO_SCHALT_2",  FREQ_10MIN, cm_manager},
    {"W_WASSERPROG_1_SO_SCHALT_3",  FREQ_10MIN, cm_manager},

    // Heating program of heating circuit 1 (manager), same layout
    {"HEIZPROG_1_MO",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MO_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MO_SCHALT_3",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DI",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DI_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DI_SCHALT_3",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MI",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MI_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_MI_SCHALT_3",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DO",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DO_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_DO_SCHALT_3",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_FR",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_FR_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_FR_SCHALT_3",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SA",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SA_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SA_SCHALT_3",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SO",               FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SO_SCHALT_2",      FREQ_10MIN, cm_manager},
    {"HEIZPROG_1_SO_SCHALT_3",      FREQ_10MIN, cm_manager},

    // Heating circuit 2 (mixer circuit): settings and program on the manager's virtual
    // member MISCHERMODUL_3 (0x602), display "Raumtemp. 2" / "Heizkurve 2"
    // Remote control FE 7: room actual temperature (the manager itself does not answer it, its
    // virtual members mirror the one FE sensor) and the circuit it is assigned to (parameter 22)
    {"RAUMISTTEMP",                 FREQ_1MIN,  cm_mischermodul_3},
    // Effective room setpoint of the FE circuit: day/night setpoint plus the FE knob (+-5 K);
    // the FE mode switch shows here too (moon = night setpoint without correction)
    {"VERSTELLTE_RAUMSOLLTEMP",     FREQ_1MIN,  cm_mischermodul_3},
    {"FERNBEDIENUNGSZUORDNUNG",     FREQ_10MIN, cm_manager},
    {"RAUMSOLLTEMP_I",              FREQ_10MIN, cm_mischermodul_3},
    {"RAUMSOLLTEMP_NACHT",          FREQ_10MIN, cm_mischermodul_3},
    {"HEIZKURVE",                   FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MO",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MO_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MO_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DI",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DI_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DI_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MI",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MI_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_MI_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DO",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DO_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_DO_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_FR",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_FR_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_FR_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SA",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SA_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SA_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SO",               FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SO_SCHALT_2",      FREQ_10MIN, cm_mischermodul_3},
    {"HEIZPROG_1_SO_SCHALT_3",      FREQ_10MIN, cm_mischermodul_3},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPL13E_H

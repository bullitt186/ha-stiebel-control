/*
 * Signal Request Table - WPL-A Heat Pump (Air-Source) with HSBC 300 cool (integrated WPM3)
 *
 * Verified on real hardware: WPL-A 07 HK 230 Premium + integral cylinder HSBC 300 cool
 * (integrated manager WPM3), built-in CAN at 50 kbit/s, sender 0x680.
 *
 * Member layout on the WPL-A 07:
 *   KESSEL         0x180 - outside temp, buffer, flow, operating status 0x0176, DHW params 0x4fxx
 *   KESSEL_CANB    0x201 - DHW actual/setpoint temps; the display writes the DHW setpoints here
 *   MANAGER        0x480 - clock, operating mode, status word 0x4eef, pump and tuning params
 *   HEIZMODUL      0x500 - compressor state (base table)
 *   HEIZMODUL_WPL  0x514 - compressor, inverter and refrigerant circuit
 *   MISCHERMODUL_2 0x601 - heating circuit 1 (min/max/setpoint/comfort/eco/curve)
 *   FES            0x100 - the display. It answers reads from 0x680 with 0x8000 (not
 *                          supported), so nothing is polled there.
 *
 * Notes:
 *   - MINTEMP (0x4ea7) on MISCHERMODUL_2: a large negative raw value is normal, the
 *     controller encodes "off" this way.
 *   - SAMMEL_RELAISSTATUS (0x0a20) is blacklisted in ElsterTable.h, so it is not polled.
 */

#ifndef SIGNAL_REQUESTS_WPLA7_H
#define SIGNAL_REQUESTS_WPLA7_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE

    // ========================================================================
    // KESSEL (0x180)
    // ========================================================================

    {"AUSSENTEMP",                            FREQ_30S,   cm_kessel},   // outside temp
    {"PUFFERTEMP_UNTEN1",                     FREQ_30S,   cm_kessel},   // buffer actual temp (HK1)
    {"PUFFERSOLL",                            FREQ_30S,   cm_kessel},   // buffer setpoint temp (HK1)
    {"BIVALENTPARALLELTEMPERATUR_HZG",        FREQ_10MIN, cm_kessel},   // bivalence temp heating
    {"WPVORLAUFIST",                          FREQ_30S,   cm_kessel},   // heat pump flow temp
    {"BETRIEBS_STATUS",                       FREQ_30S,   cm_kessel},   // 0x0176 bit field, bits 0/6/12/14 checked on the display
    {"PUMPENSTATUS",                          FREQ_1MIN,  cm_kessel},   // 0x01d2 bit field
    {"WP_PUMPENSTATUS",                       FREQ_1MIN,  cm_kessel},   // 0xfdad on/off
    {"SOLLSPREIZUNG",                         FREQ_10MIN, cm_kessel},   // 0x067e K
    {"AUSLEGUNGSTEMPERATUR",                  FREQ_10MIN, cm_kessel},   // 0x0665 °C
    {"WAERMEBEDARF",                          FREQ_10MIN, cm_kessel},   // 0x0666 kW
    {"REGLERDYNAMIK_HEIZEN",                  FREQ_10MIN, cm_kessel},   // 0x4e8a
    {"WARMWASSERBETRIEB",                     FREQ_10MIN, cm_kessel},   // 0x4f24
    {"WARMWASSERHYSTERESE",                   FREQ_10MIN, cm_kessel},   // 0x4f25 K
    {"ANTILEGIONELLENBEHANDLUNG",             FREQ_10MIN, cm_kessel},   // 0x4f28 on/off
    {"WW_SOLLTEMPERATUR",                     FREQ_10MIN, cm_kessel},   // 0x4ef9 anti-legionella setpoint °C
    {"STILLSTANDSZEIT",                       FREQ_10MIN, cm_kessel},   // 0x4f2e min
    {"PROGRAMMSCHALTER",                      FREQ_10MIN, cm_kessel},   // 0x0112 operating mode switch

    // ========================================================================
    // KESSEL_CANB (0x201) - DHW
    // ========================================================================

    {"SPEICHERSOLLTEMP",                      FREQ_30S,   cm_kessel_canb},   // 0x0003 DHW setpoint
    {"SPEICHERISTTEMP",                       FREQ_30S,   cm_kessel_canb},   // 0x000e DHW actual temp
    {"EINSTELL_SPEICHERSOLLTEMP",             FREQ_30S,   cm_kessel_canb},   // 0x0013 DHW comfort setpoint (writable)
    {"EINSTELL_SPEICHERSOLLTEMP2",            FREQ_30S,   cm_kessel_canb},   // 0x0a06 DHW eco setpoint (writable)
    {"PROGRAMMSCHALTER",                      FREQ_10MIN, cm_kessel_canb},   // 0x0112 operating mode switch

    // ========================================================================
    // MANAGER (0x480)
    // ========================================================================

    {"BETRIEBS_STATUS_4EEF",                  FREQ_30S,   cm_manager},   // WPL-A status word (bit field)
    {"WAERMEPUMPEN_STATUS_4EDA",              FREQ_1MIN,  cm_manager},   // meaning open
    {"EINGESCHALTETE_STUFEN",                 FREQ_10MIN, cm_manager},   // 0x4efa
    {"ZEITINTERVALL_4F4D",                    FREQ_1MIN,  cm_manager},   // raw +-165 seen
    {"MINIMALE_PUMPLEISTUNG",                 FREQ_10MIN, cm_manager},   // 0x4f98 %
    {"MAXIMALE_PUMPLEISTUNG",                 FREQ_10MIN, cm_manager},   // 0x4f99 %
    {"SPREIZUNG",                             FREQ_10MIN, cm_manager},   // 0x4f91 K
    {"PUMPENLEISTUNG_STANDBY_4F8A",           FREQ_10MIN, cm_manager},   // %
    {"PUMPENLEISTUNG_HEIZEN",                 FREQ_10MIN, cm_manager},   // 0x4f8c %
    {"PUMPENLEISTUNG_KUEHLEN",                FREQ_10MIN, cm_manager},   // 0x4f92 %
    {"EVU_SPERRE",                            FREQ_10MIN, cm_manager},   // 0xabe1
    {"EVU_SPERRE_CONFIG",                     FREQ_10MIN, cm_manager},   // 0x4f84

    // ========================================================================
    // HEIZMODUL (0x500) - IWS registers
    // ========================================================================

    {"REKUPERATORISTTEMP",                    FREQ_1MIN,  cm_heizmodul},   // 0x07a3
    {"MESSSTROM_HOCHDRUCK",                   FREQ_1MIN,  cm_heizmodul},   // 0x07a4 mA
    {"MESSSTROM_NIEDERDRUCK",                 FREQ_1MIN,  cm_heizmodul},   // 0x07a5 mA
    {"VERDAMPFERISTTEMP_KOMPENSIERT",         FREQ_1MIN,  cm_heizmodul},   // 0x07a9
    {"VORLAUFISTTEMP_WP_IWS",                 FREQ_1MIN,  cm_heizmodul},   // 0x0822
    {"LUEFTERDREHZAHL",                       FREQ_1MIN,  cm_heizmodul},   // 0x0833 %
    {"LEISTUNGSREDUZIERUNG_KUEHLEN",          FREQ_1MIN,  cm_heizmodul},   // 0x05d7

    // ========================================================================
    // HEIZMODUL_WPL (0x514) - compressor, inverter, refrigerant circuit
    // ========================================================================

    {"RUECKLAUFISTTEMP",                      FREQ_30S,   cm_heizmodul_wpl},   // return flow temp
    {"HEISSGAS_TEMP",                         FREQ_30S,   cm_heizmodul_wpl},   // hot gas temp
    {"VERDICHTER_EINTRITTSTEMP",              FREQ_30S,   cm_heizmodul_wpl},   // 0x06d9 compressor inlet temp
    {"ANZEIGE_NIEDERDRUCK",                   FREQ_30S,   cm_heizmodul_wpl},   // low pressure
    {"STARTS_ABTAUUNG",                       FREQ_10MIN, cm_heizmodul_wpl},   // defrost starts
    {"ZEITDAUER_LETZTE_ABTAUUNG",             FREQ_10MIN, cm_heizmodul_wpl},   // last defrost duration
    {"LAUFZEIT_DHC1",                         FREQ_10MIN, cm_heizmodul_wpl},   // aux heater 1 runtime
    {"LAUFZEIT_DHC2",                         FREQ_10MIN, cm_heizmodul_wpl},   // aux heater 2 runtime
    {"LZ_DHC12",                              FREQ_10MIN, cm_heizmodul_wpl},   // aux heater 1+2 runtime
    {"VERDICHTER_STARTS_K_4EF0",              FREQ_10MIN, cm_heizmodul_wpl},   // compressor starts (thousands)
    {"VERDICHTER_STARTS_4EF1",                FREQ_10MIN, cm_heizmodul_wpl},   // compressor starts (units)
    {"LAUFZEIT_VD_HEIZEN",                    FREQ_10MIN, cm_heizmodul_wpl},   // 0x4efb compressor heating runtime
    {"LAUFZEIT_VD_WW",                        FREQ_10MIN, cm_heizmodul_wpl},   // 0x4efd compressor DHW runtime
    {"LAUFZEIT_VD_ABTAUEN",                   FREQ_10MIN, cm_heizmodul_wpl},   // 0x4f06 compressor defrost runtime
    {"RESTSTILLSTANDSZEIT",                   FREQ_1MIN,  cm_heizmodul_wpl},   // 0x4f4e min
    // 0xabe1 on HEIZMODUL_WPL is not an EVU lock signal, therefore not polled here
    {"UMGEBUNGSTEMPERATUR_INVERTER",          FREQ_1MIN,  cm_heizmodul_wpl},   // inverter ambient temp
    {"TEMPERATUR_INV_VERDICHTER",             FREQ_1MIN,  cm_heizmodul_wpl},   // 0xc1e7 inverter/compressor temp
    {"SPANNUNG_INVERTER",                     FREQ_1MIN,  cm_heizmodul_wpl},   // 0x06b1 V
    {"STROM_INVERTER",                        FREQ_1MIN,  cm_heizmodul_wpl},   // 0x06b2 A
    {"STROM_MOTOR",                           FREQ_1MIN,  cm_heizmodul_wpl},   // 0x06b4 motor current (A), answers on 0x514
    {"OEFFNUNGSGRAD_BYPASSVENT",              FREQ_1MIN,  cm_heizmodul_wpl},   // 0xc2c3 bypass valve opening
    {"ADAPTION_UEBERHITZUNG",                 FREQ_1MIN,  cm_heizmodul_wpl},   // superheat adaption
    {"UNTERKUEHLUNG_EXVENTIL_EIN",            FREQ_1MIN,  cm_heizmodul_wpl},   // 0x4f9f subcooling EXV on
    {"BETRIEBSART_KAELTEKREISREGLER",         FREQ_1MIN,  cm_heizmodul_wpl},   // 0x4fa0 raw 0/3, meaning open
    {"SOLL_UEBERHITZUNG_SAUGGAS_VERDICHTER",  FREQ_1MIN,  cm_heizmodul_wpl},   // 0x4fa1
    {"IST_UEBERHITZUNG_SAUGGAS_VERDICHTER",   FREQ_1MIN,  cm_heizmodul_wpl},   // 0x4fa2
    {"VERDICHTERDREHZAHLGRENZE",              FREQ_1MIN,  cm_heizmodul_wpl},   // 0x4fac
    {"EXV_OEFFNUNGSGRAD",                     FREQ_1MIN,  cm_heizmodul_wpl},   // 0x07ca %
    {"SOLLWERT_UEBERHITZUNG",                 FREQ_1MIN,  cm_heizmodul_wpl},   // 0x07f3 K
    {"ISTWERT_UEBERHITZUNG_VERDAMPFER",       FREQ_1MIN,  cm_heizmodul_wpl},   // 0x07f4 K
    {"INTEGRAL_REGELABWEICHUNG",              FREQ_1MIN,  cm_heizmodul_wpl},   // 0x033d
    {"VORSTEUER_OEFFNUNGSGRAD",               FREQ_1MIN,  cm_heizmodul_wpl},   // 0x07f6 %
    {"P_ANTEIL_EXV",                          FREQ_1MIN,  cm_heizmodul_wpl},   // 0x07f7
    {"I_ANTEIL_EXV",                          FREQ_1MIN,  cm_heizmodul_wpl},   // 0x07f8
    {"UNTERK_COND",                           FREQ_1MIN,  cm_heizmodul_wpl},   // 0x0aff condenser subcooling

    // ========================================================================
    // MISCHERMODUL_2 (0x601) - heating circuit 1
    // ========================================================================

    {"MINTEMP",                               FREQ_10MIN, cm_mischermodul_2},   // 0x4ea7 HK1 min flow temp
    {"MAXTEMP",                               FREQ_10MIN, cm_mischermodul_2},   // 0x4ea8
    {"SOLLTEMPERATUR",                        FREQ_10MIN, cm_mischermodul_2},   // 0x4eb0
    {"ISTTEMPERATUR",                         FREQ_10MIN, cm_mischermodul_2},   // 0x4eb4
    {"KOMFORTTEMPERATUR",                     FREQ_10MIN, cm_mischermodul_2},   // 0x4eb8
    {"ECOTEMPERATUR",                         FREQ_10MIN, cm_mischermodul_2},   // 0x4eb9
    {"STEIGUNG_HEIZKURVE",                    FREQ_10MIN, cm_mischermodul_2},   // 0x4f2b

    // ========================================================================
    // BEDIENMODUL_2 - operating mode probe
    // ========================================================================

    {"PROGRAMMSCHALTER",                      FREQ_10MIN, cm_bedienmodul_2},   // 0x0112 operating mode switch

    // ========================================================================
    // ROOM SETPOINTS - MANAGER, single heating circuit
    // ========================================================================

    {"RAUMSOLLTEMP_I",                        FREQ_1MIN,  cm_manager},
    {"RAUMSOLLTEMP_NACHT",                    FREQ_1MIN,  cm_manager},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPLA7_H

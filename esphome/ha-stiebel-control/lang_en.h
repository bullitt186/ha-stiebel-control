/*
 * lang_en.h — English display name overrides
 *
 * Includes lang_base.h (German defaults) first, then redefines only the
 * macros that have English translations. Any untranslated macro keeps its
 * German default — this is intentional and safe.
 *
 * To add a translation: #undef LNAME_FOO, then #define LNAME_FOO "English"
 */

#pragma once
#include "lang_base.h"

// ============================================================================
// ElsterTable signal friendly names
// ============================================================================
#undef  LNAME_BRENNER
#define LNAME_BRENNER                          "Boiler"
#undef  LNAME_KESSELSOLLTEMP
#define LNAME_KESSELSOLLTEMP                   "Boiler Target Temperature"
#undef  LNAME_SPEICHERSOLLTEMP
#define LNAME_SPEICHERSOLLTEMP                 "Storage Target Temperature"
#undef  LNAME_RAUMSOLLTEMP_I
#define LNAME_RAUMSOLLTEMP_I                   "Room Target Temperature I"
#undef  LNAME_RAUMSOLLTEMP_II
#define LNAME_RAUMSOLLTEMP_II                  "Room Target Temperature II"
#undef  LNAME_RAUMSOLLTEMP_III
#define LNAME_RAUMSOLLTEMP_III                 "Room Target Temperature III"
#undef  LNAME_RAUMSOLLTEMP_NACHT
#define LNAME_RAUMSOLLTEMP_NACHT               "Room Target Temperature Night"
#undef  LNAME_AUSSENTEMP
#define LNAME_AUSSENTEMP                       "Outside Temperature"
#undef  LNAME_SAMMLERISTTEMP
#define LNAME_SAMMLERISTTEMP                   "Collector Actual Temperature"
#undef  LNAME_SPEICHERISTTEMP
#define LNAME_SPEICHERISTTEMP                  "Storage Actual Temperature"
#undef  LNAME_VORLAUFISTTEMP
#define LNAME_VORLAUFISTTEMP                   "Flow Actual Temperature"
#undef  LNAME_EINSTELL_SPEICHERSOLLTEMP
#define LNAME_EINSTELL_SPEICHERSOLLTEMP        "DHW Target Temperature Setting"
#undef  LNAME_EINSTELL_SPEICHERSOLLTEMP2
#define LNAME_EINSTELL_SPEICHERSOLLTEMP2       "DHW Target Temperature 2 Setting"
#undef  LNAME_VERDAMPFERTEMP
#define LNAME_VERDAMPFERTEMP                   "Evaporator Temperature"
#undef  LNAME_RUECKLAUFISTTEMP
#define LNAME_RUECKLAUFISTTEMP                 "Return Actual Temperature"
#undef  LNAME_ABTAUUNGAKTIV
#define LNAME_ABTAUUNGAKTIV                    "Defrost Active"
#undef  LNAME_EVU_SPERRE_AKTIV
#define LNAME_EVU_SPERRE_AKTIV                 "Grid Lock Active"
#undef  LNAME_SOLAR_AKT_LEISTUNG_W
#define LNAME_SOLAR_AKT_LEISTUNG_W             "Solar Current Power W"
#undef  LNAME_SOLAR_TAGESERTRAG_WH
#define LNAME_SOLAR_TAGESERTRAG_WH             "Solar Daily Yield Wh"
#undef  LNAME_SOLAR_TAGESERTRAG_KWH
#define LNAME_SOLAR_TAGESERTRAG_KWH            "Solar Daily Yield kWh"
#undef  LNAME_SOLAR_GESAMTERTRAG_WH
#define LNAME_SOLAR_GESAMTERTRAG_WH            "Solar Total Yield Wh"
#undef  LNAME_SOLAR_GESAMTERTRAG_KWH
#define LNAME_SOLAR_GESAMTERTRAG_KWH           "Solar Total Yield kWh"
#undef  LNAME_SOLAR_GESAMTERTRAG_MWH
#define LNAME_SOLAR_GESAMTERTRAG_MWH           "Solar Total Yield MWh"
#undef  LNAME_SOLAR_WOCHENERTRAG_WH
#define LNAME_SOLAR_WOCHENERTRAG_WH            "Solar Weekly Yield Wh"
#undef  LNAME_SOLAR_WOCHENERTRAG_KWH
#define LNAME_SOLAR_WOCHENERTRAG_KWH           "Solar Weekly Yield kWh"
#undef  LNAME_SOLAR_WOCHENERTRAG_MWH
#define LNAME_SOLAR_WOCHENERTRAG_MWH           "Solar Weekly Yield MWh"
#undef  LNAME_SOLAR_JAHRESERTRAG_WH
#define LNAME_SOLAR_JAHRESERTRAG_WH            "Solar Annual Yield Wh"
#undef  LNAME_SOLAR_JAHRESERTRAG_KWH
#define LNAME_SOLAR_JAHRESERTRAG_KWH           "Solar Annual Yield kWh"
#undef  LNAME_SOLAR_JAHRESERTRAG_MWH
#define LNAME_SOLAR_JAHRESERTRAG_MWH           "Solar Annual Yield MWh"
#undef  LNAME_ANTILEGIONELLEN
#define LNAME_ANTILEGIONELLEN                  "Anti-Legionella"
#undef  LNAME_ANTILEGIONELLEN_ZEITPUNKT
#define LNAME_ANTILEGIONELLEN_ZEITPUNKT        "Anti-Legionella Time"
#undef  LNAME_HEIZKURVE
#define LNAME_HEIZKURVE                        "Heating Curve"
#undef  LNAME_PROGRAMMSCHALTER
#define LNAME_PROGRAMMSCHALTER                 "Operating Mode"
#undef  LNAME_TAG
#define LNAME_TAG                              "Day"
#undef  LNAME_MONAT
#define LNAME_MONAT                            "Month"
#undef  LNAME_JAHR
#define LNAME_JAHR                             "Year"
#undef  LNAME_STUNDE
#define LNAME_STUNDE                           "Hour"
#undef  LNAME_MINUTE
#define LNAME_MINUTE                           "Minute"
#undef  LNAME_SEKUNDE
#define LNAME_SEKUNDE                          "Second"
#undef  LNAME_WW_HYSTERSE
#define LNAME_WW_HYSTERSE                      "DHW Hysteresis"
#undef  LNAME_WPVORLAUFIST
#define LNAME_WPVORLAUFIST                     "Heat Pump Flow Actual Temperature"
#undef  LNAME_HILFSKESSELSOLL
#define LNAME_HILFSKESSELSOLL                  "Auxiliary Boiler Target Temperature"
#undef  LNAME_WW_ECO
#define LNAME_WW_ECO                           "DHW ECO"
#undef  LNAME_WAERMEERTRAG_RUECKGE_TAG_WH
#define LNAME_WAERMEERTRAG_RUECKGE_TAG_WH      "Heat Recovery Daily Yield Wh"
#undef  LNAME_WAERMEERTRAG_RUECKGE_TAG_KWH
#define LNAME_WAERMEERTRAG_RUECKGE_TAG_KWH     "Heat Recovery Daily Yield kWh"
#undef  LNAME_WAERMEERTRAG_RUECKGE_SUMME_KWH
#define LNAME_WAERMEERTRAG_RUECKGE_SUMME_KWH   "Heat Recovery Total Yield kWh"
#undef  LNAME_WAERMEERTRAG_RUECKGE_SUMME_MWH
#define LNAME_WAERMEERTRAG_RUECKGE_SUMME_MWH   "Heat Recovery Total Yield MWh"
#undef  LNAME_VERDICHTER
#define LNAME_VERDICHTER                       "Compressor"
#undef  LNAME_LZ_VERD_1_HEIZBETRIEB
#define LNAME_LZ_VERD_1_HEIZBETRIEB            "Compressor 1 Heating Runtime"
#undef  LNAME_LZ_VERD_2_HEIZBETRIEB
#define LNAME_LZ_VERD_2_HEIZBETRIEB            "Compressor 2 Heating Runtime"
#undef  LNAME_LZ_VERD_1_2_HEIZBETRIEB
#define LNAME_LZ_VERD_1_2_HEIZBETRIEB          "Compressor 1+2 Heating Runtime"
#undef  LNAME_LZ_VERD_1_KUEHLBETRIEB
#define LNAME_LZ_VERD_1_KUEHLBETRIEB           "Compressor 1 Cooling Runtime"
#undef  LNAME_LZ_VERD_2_KUEHLBETRIEB
#define LNAME_LZ_VERD_2_KUEHLBETRIEB           "Compressor 2 Cooling Runtime"
#undef  LNAME_LZ_VERD_1_2_KUEHLBETRIEB
#define LNAME_LZ_VERD_1_2_KUEHLBETRIEB         "Compressor 1+2 Cooling Runtime"
#undef  LNAME_EL_AUFNAHMELEISTUNG_WW_TAG_WH
#define LNAME_EL_AUFNAHMELEISTUNG_WW_TAG_WH    "Electrical Consumption DHW Daily Wh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_WW_TAG_KWH
#define LNAME_EL_AUFNAHMELEISTUNG_WW_TAG_KWH   "Electrical Consumption DHW Daily kWh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_WW_SUM_KWH
#define LNAME_EL_AUFNAHMELEISTUNG_WW_SUM_KWH   "Electrical Consumption DHW Total kWh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_WW_SUM_MWH
#define LNAME_EL_AUFNAHMELEISTUNG_WW_SUM_MWH   "Electrical Consumption DHW Total MWh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH
#define LNAME_EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH  "Electrical Consumption Heating Daily Wh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH
#define LNAME_EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH "Electrical Consumption Heating Daily kWh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH
#define LNAME_EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH "Electrical Consumption Heating Total kWh"
#undef  LNAME_EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH
#define LNAME_EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH "Electrical Consumption Heating Total MWh"
#undef  LNAME_WAERMEERTRAG_2WE_WW_TAG_WH
#define LNAME_WAERMEERTRAG_2WE_WW_TAG_WH       "Thermal Yield 2nd Source DHW Daily Wh"
#undef  LNAME_WAERMEERTRAG_2WE_WW_TAG_KWH
#define LNAME_WAERMEERTRAG_2WE_WW_TAG_KWH      "Thermal Yield 2nd Source DHW Daily kWh"
#undef  LNAME_WAERMEERTRAG_2WE_WW_SUM_KWH
#define LNAME_WAERMEERTRAG_2WE_WW_SUM_KWH      "Thermal Yield 2nd Source DHW Total kWh"
#undef  LNAME_WAERMEERTRAG_2WE_WW_SUM_MWH
#define LNAME_WAERMEERTRAG_2WE_WW_SUM_MWH      "Thermal Yield 2nd Source DHW Total MWh"
#undef  LNAME_WAERMEERTRAG_2WE_HEIZ_TAG_WH
#define LNAME_WAERMEERTRAG_2WE_HEIZ_TAG_WH     "Thermal Yield 2nd Source Heating Daily Wh"
#undef  LNAME_WAERMEERTRAG_2WE_HEIZ_TAG_KWH
#define LNAME_WAERMEERTRAG_2WE_HEIZ_TAG_KWH    "Thermal Yield 2nd Source Heating Daily kWh"
#undef  LNAME_WAERMEERTRAG_2WE_HEIZ_SUM_KWH
#define LNAME_WAERMEERTRAG_2WE_HEIZ_SUM_KWH    "Thermal Yield 2nd Source Heating Total kWh"
#undef  LNAME_WAERMEERTRAG_2WE_HEIZ_SUM_MWH
#define LNAME_WAERMEERTRAG_2WE_HEIZ_SUM_MWH    "Thermal Yield 2nd Source Heating Total MWh"
#undef  LNAME_WAERMEERTRAG_WW_TAG_WH
#define LNAME_WAERMEERTRAG_WW_TAG_WH           "Thermal Yield DHW Daily Wh"
#undef  LNAME_WAERMEERTRAG_WW_TAG_KWH
#define LNAME_WAERMEERTRAG_WW_TAG_KWH          "Thermal Yield DHW Daily kWh"
#undef  LNAME_WAERMEERTRAG_WW_SUM_KWH
#define LNAME_WAERMEERTRAG_WW_SUM_KWH          "Thermal Yield DHW Total kWh"
#undef  LNAME_WAERMEERTRAG_WW_SUM_MWH
#define LNAME_WAERMEERTRAG_WW_SUM_MWH          "Thermal Yield DHW Total MWh"
#undef  LNAME_WAERMEERTRAG_HEIZ_TAG_WH
#define LNAME_WAERMEERTRAG_HEIZ_TAG_WH         "Thermal Yield Heating Daily Wh"
#undef  LNAME_WAERMEERTRAG_HEIZ_TAG_KWH
#define LNAME_WAERMEERTRAG_HEIZ_TAG_KWH        "Thermal Yield Heating Daily kWh"
#undef  LNAME_WAERMEERTRAG_HEIZ_SUM_KWH
#define LNAME_WAERMEERTRAG_HEIZ_SUM_KWH        "Thermal Yield Heating Total kWh"
#undef  LNAME_WAERMEERTRAG_HEIZ_SUM_MWH
#define LNAME_WAERMEERTRAG_HEIZ_SUM_MWH        "Thermal Yield Heating Total MWh"
#undef  LNAME_GEBAEUDEART
#define LNAME_GEBAEUDEART                      "Building Type"
#undef  LNAME_WP_STATUS
#define LNAME_WP_STATUS                        "Heat Pump Status"
#undef  LNAME_SOMMERBETRIEB
#define LNAME_SOMMERBETRIEB                    "Summer Mode"
#undef  LNAME_BETRIEBSART_WP
#define LNAME_BETRIEBSART_WP                   "Heat Pump Operating Mode"

// ============================================================================
// Calculated sensor names
// ============================================================================
#undef  LNAME_CALC_DATE
#define LNAME_CALC_DATE                        "Date"
#undef  LNAME_CALC_TIME
#define LNAME_CALC_TIME                        "Time"
#undef  LNAME_CALC_BETRIEBSART
#define LNAME_CALC_BETRIEBSART                 "Operating Mode"
#undef  LNAME_CALC_DELTA_T_CONTINUOUS
#define LNAME_CALC_DELTA_T_CONTINUOUS          "Delta T HP (continuous)"
#undef  LNAME_CALC_DELTA_T_RUNNING
#define LNAME_CALC_DELTA_T_RUNNING             "Delta T HP (compressor on only)"
#undef  LNAME_CALC_COMPRESSOR_ACTIVE
#define LNAME_CALC_COMPRESSOR_ACTIVE           "HP Compressor Active"
#undef  LNAME_CALC_CAN_TEC
#define LNAME_CALC_CAN_TEC                     "CAN TX Error Counter"
#undef  LNAME_CALC_CAN_REC
#define LNAME_CALC_CAN_REC                     "CAN RX Error Counter"
#undef  LNAME_CALC_CAN_BUS_ERRORS
#define LNAME_CALC_CAN_BUS_ERRORS              "CAN Bus Errors"
#undef  LNAME_CALC_CAN_STATE
#define LNAME_CALC_CAN_STATE                   "CAN Bus State"

// ============================================================================
// Writable number friendly names
// ============================================================================
#undef  LNAME_NUM_SPEICHERSOLLTEMP
#define LNAME_NUM_SPEICHERSOLLTEMP             "DHW Target Temperature Setting"
#undef  LNAME_NUM_SPEICHERSOLLTEMP2
#define LNAME_NUM_SPEICHERSOLLTEMP2            "DHW Target Temperature 2 Setting"
#undef  LNAME_NUM_SG_READY_BOOST3
#define LNAME_NUM_SG_READY_BOOST3              "SG Ready Boost State 3"
#undef  LNAME_NUM_SG_READY_BOOST4
#define LNAME_NUM_SG_READY_BOOST4              "SG Ready Boost State 4"
#undef  LNAME_NUM_RAUMSOLLTEMP_I
#define LNAME_NUM_RAUMSOLLTEMP_I               "Room Target Temperature I Setting"
#undef  LNAME_NUM_RAUMSOLLTEMP_II
#define LNAME_NUM_RAUMSOLLTEMP_II              "Room Target Temperature II Setting"
#undef  LNAME_NUM_RAUMSOLLTEMP_III
#define LNAME_NUM_RAUMSOLLTEMP_III             "Room Target Temperature III Setting"
#undef  LNAME_NUM_RAUMSOLLTEMP_NACHT
#define LNAME_NUM_RAUMSOLLTEMP_NACHT           "Room Target Temperature Night Setting"

// ============================================================================
// Writable select friendly names
// ============================================================================
#undef  LNAME_SEL_PROGRAMMSCHALTER
#define LNAME_SEL_PROGRAMMSCHALTER             "Operating Mode"
#undef  LNAME_SEL_SG_READY
#define LNAME_SEL_SG_READY                     "SG Ready State"

// ============================================================================
// Select option strings
// ============================================================================
#undef  LNAME_OPT_NOTBETRIEB
#define LNAME_OPT_NOTBETRIEB                   "Emergency"
#undef  LNAME_OPT_BEREITSCHAFT
#define LNAME_OPT_BEREITSCHAFT                 "Standby"
#undef  LNAME_OPT_AUTOMATIK
#define LNAME_OPT_AUTOMATIK                    "Automatic"
#undef  LNAME_OPT_TAGBETRIEB
#define LNAME_OPT_TAGBETRIEB                   "Day Mode"
#undef  LNAME_OPT_ABSENKBETRIEB
#define LNAME_OPT_ABSENKBETRIEB                "Night Mode"
#undef  LNAME_OPT_WARMWASSER
#define LNAME_OPT_WARMWASSER                   "Hot Water"
#undef  LNAME_OPT_SG_EVU_SPERRE
#define LNAME_OPT_SG_EVU_SPERRE               "1 - Grid Lock"
#undef  LNAME_OPT_SG_NORMAL
#define LNAME_OPT_SG_NORMAL                    "2 - Normal"
#undef  LNAME_OPT_SG_EMPFOHLEN
#define LNAME_OPT_SG_EMPFOHLEN                 "3 - Recommended"
#undef  LNAME_OPT_SG_ZWANG
#define LNAME_OPT_SG_ZWANG                     "4 - Forced"

// ============================================================================
// COP sensor names
// ============================================================================
#undef  LNAME_COP_WW
#define LNAME_COP_WW                           "COP Hot Water"
#undef  LNAME_COP_HEIZ
#define LNAME_COP_HEIZ                         "COP Heating"
#undef  LNAME_COP_GESAMT
#define LNAME_COP_GESAMT                       "COP Total"

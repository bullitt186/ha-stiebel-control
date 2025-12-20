/*
 * Signal Request Table for WPL13E Heat Pump
 * 
 * This file contains the device-specific signal request configuration.
 * To support a different heat pump model, create a new file like this
 * with the appropriate signals for that model.
 * 
 * Usage: Include this file in your device-specific YAML (e.g., wpl13e.yaml)
 */

#ifndef SIGNAL_REQUESTS_WPL13E_H
#define SIGNAL_REQUESTS_WPL13E_H

#include "config.h"
#include "ha-stiebel-control.h"


// Configurable signal request table for WPL13E model
// Edit this table to customize which signals are requested and how often
static const SignalRequest signalRequests[] = {
    // ========================================================================
    // TIME AND DATE SIGNALS
    // ========================================================================
    {"JAHR", FREQ_1MIN, cm_manager},
    {"MONAT", FREQ_1MIN, cm_manager},
    {"TAG", FREQ_1MIN, cm_manager},
    {"STUNDE", FREQ_1MIN, cm_manager},
    {"MINUTE", FREQ_1MIN, cm_manager},
    {"SEKUNDE", FREQ_1MIN, cm_manager},
    
    // ========================================================================
    // STATUS AND CONTROL SIGNALS
    // ========================================================================
    {"WP_STATUS", FREQ_1MIN, cm_manager},
    {"EVU_SPERRE_AKTIV", FREQ_1MIN, cm_manager},
    {"ABTAUUNGAKTIV", FREQ_1MIN, cm_heizmodul},
    {"BETRIEBSART_WP", FREQ_10MIN, cm_manager},
    {"PROGRAMMSCHALTER", FREQ_10MIN, cm_manager},
    {"SOMMERBETRIEB", FREQ_1MIN, cm_manager},
    
    // ========================================================================
    // TEMPERATURE SIGNALS (30 SECOND INTERVAL)
    // cm_other = request from all available CAN members
    // ========================================================================
    {"KESSELSOLLTEMP", FREQ_30S, cm_manager},
    {"KESSELSOLLTEMP", FREQ_30S, cm_kessel},
    {"SPEICHERSOLLTEMP", FREQ_30S, cm_manager},
    {"SPEICHERSOLLTEMP", FREQ_30S, cm_kessel},
    // {"VORLAUFSOLLTEMP", FREQ_30S, cm_other},
    {"RAUMSOLLTEMP_I", FREQ_30S, cm_manager},
    {"RAUMSOLLTEMP_II", FREQ_30S, cm_manager},
    {"RAUMSOLLTEMP_III", FREQ_30S, cm_manager},
    {"RAUMSOLLTEMP_NACHT", FREQ_30S, cm_manager},
    {"AUSSENTEMP", FREQ_30S, cm_kessel},
    {"AUSSENTEMP", FREQ_30S, cm_heizmodul},
    {"SAMMLERISTTEMP", FREQ_30S, cm_kessel},
    {"SPEICHERISTTEMP", FREQ_30S, cm_kessel},
    {"VORLAUFISTTEMP", FREQ_30S, cm_kessel},
    //{"RAUMISTTEMP", FREQ_30S, cm_other},
    //{"VERSTELLTE_RAUMSOLLTEMP", FREQ_30S, cm_other},
    {"EINSTELL_SPEICHERSOLLTEMP", FREQ_30S, cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP", FREQ_30S, cm_manager},
    // {"VERDAMPFERTEMP", FREQ_30S, cm_other},
    // {"SAMMLERSOLLTEMP", FREQ_30S, cm_other},
    {"RUECKLAUFISTTEMP", FREQ_30S, cm_manager},
    {"RUECKLAUFISTTEMP", FREQ_30S, cm_kessel},
   //  {"SPEICHER_UNTEN_TEMP", FREQ_30S, cm_other},
   //  {"SOLARZONENTEMP", FREQ_30S, cm_other},
   //  {"SPEICHER_OBEN_TEMP", FREQ_30S, cm_other},
   //  {"KOLLEKTORTEMP", FREQ_30S, cm_other},
   //  {"FESTSTOFFKESSELTEMP", FREQ_30S, cm_other},
   //  {"MIN_TEMP_KESSEL", FREQ_30S, cm_other},
   //  {"ANFAHRTEMP", FREQ_30S, cm_other},
   //  {"MAX_TEMP_KESSEL", FREQ_30S, cm_other},
   //  {"MAX_TEMP_HZK", FREQ_30S, cm_other},
   //  {"KOLLEKTORTEMP_2", FREQ_30S, cm_other},
   //  {"MULTIFUNKTION_ISTTEMP", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_OBEN1", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_MITTE1", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_UNTEN1", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_OBEN2", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_MITTE2", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_UNTEN2", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_OBEN3", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_MITTE3", FREQ_30S, cm_other},
   //  {"PUFFERTEMP_UNTEN3", FREQ_30S, cm_other},
   //  {"AUSSENTEMPVERZOEGERUNG", FREQ_30S, cm_other},
   //  {"AUSWAHL_STANDARDTEMP", FREQ_30S, cm_other},
   //  {"MIN_TEMP_HZK", FREQ_30S, cm_other},
   //  {"FERIEN_ABSENKTEMP", FREQ_30S, cm_other},
   //  {"WW_MAXTEMP", FREQ_30S, cm_other},
   //  {"KESSELSOLLTEMP_2WE", FREQ_30S, cm_other},
   //  {"ABWESENHEITSTEMP", FREQ_30S, cm_other},
   //  {"EINSTELL_SPEICHERSOLLTEMP3", FREQ_30S, cm_other},
   //  {"ABGASTEMP", FREQ_30S, cm_other},
   //  {"WW_SCHNELL_START_TEMPERATUR", FREQ_30S, cm_other},
   //  {"MAX_WW_TEMP", FREQ_30S, cm_other},
   //  {"BIVALENTPARALLELTEMPERATUR_HZG", FREQ_30S, cm_other},
   //  {"BIVALENTPARALLELTEMPERATUR_WW", FREQ_30S, cm_other},
   //  {"BIVALENZALTERNATIVTEMPERATUR_HZG", FREQ_30S, cm_other},
   //  {"BIVALENZALTERNATIVTEMPERATUR_WW", FREQ_30S, cm_other},
   //  {"QUELLENSOLLTEMPERATUR", FREQ_30S, cm_other},
   //  {"AUSSENTEMPERATUR_WARMWASSER", FREQ_30S, cm_other},
   //  {"SOLARTEMP_MAX", FREQ_30S, cm_other},
   //  {"ESTRICH_SOCKELTEMPERATUR", FREQ_30S, cm_other},
   //  {"ESTRICH_MAX_TEMPERATUR", FREQ_30S, cm_other},
   //  {"SW_AUSSENTEMP", FREQ_30S, cm_other},
   //  {"MAX_HEIZUNG_TEMP", FREQ_30S, cm_other},
   // {"TAUPUNKT_TEMP", FREQ_30S, cm_other},
   //  {"HEISSGAS_TEMP", FREQ_30S, cm_other},
    {"WPVORLAUFIST", FREQ_30S, cm_kessel},
    {"VERDICHTER", FREQ_30S, cm_heizmodul},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_30S, cm_kessel},
    {"EINSTELL_SPEICHERSOLLTEMP2", FREQ_30S, cm_manager},

    
    // ========================================================================
    // ENERGY COUNTERS (10 MINUTE INTERVAL)
    // Required for COP (Coefficient of Performance) calculations
    // ========================================================================
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_2WE_WW_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_2WE_WW_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_2WE_HEIZ_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_2WE_HEIZ_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_WW_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_TAG_KWH", FREQ_10MIN, cm_heizmodul},
    {"WAERMEERTRAG_HEIZ_SUM_MWH", FREQ_10MIN, cm_heizmodul},
    
    // ========================================================================
    // RUNTIME COUNTERS (COMMENTED OUT - ENABLE IF NEEDED)
    // ========================================================================
    // {"LZ_VERD_1_2_HEIZBETRIEB", FREQ_10MIN, cm_manager},
    // {"LZ_VERD_1_2_WW_BETRIEB", FREQ_10MIN, cm_manager},
    // {"LZ_VERD_1_HEIZBETRIEB", FREQ_10MIN, cm_manager},
    // {"LZ_VERD_1_WW_BETRIEB", FREQ_10MIN, cm_manager},
    
    // ========================================================================
    // DEVICE INFORMATION (60 MINUTE INTERVAL)
    // ========================================================================
    {"SOFTWARE_NUMMER", FREQ_60MIN, cm_other},
    {"SOFTWARE_VERSION", FREQ_60MIN, cm_other},
    {"GERAETE_ID", FREQ_60MIN, cm_other}
};

// Number of signals in the request table
#define SIGNAL_REQUEST_COUNT (sizeof(signalRequests) / sizeof(SignalRequest))

#endif // SIGNAL_REQUESTS_WPL13E_H

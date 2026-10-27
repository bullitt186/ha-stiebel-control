/*
 *
 *  Copyright (C) 2023 Bastian Stahmer (bastian@stahmer.net)
 *  This program is part of the ESPHome / Home Assistant Program "ha-stiebel-control"
 *  and is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(ha_stiebel_control_H)
#define ha_stiebel_control_H

// ============================================================================
// INCLUDES
// ============================================================================
#include "ElsterTable.h"
#include "KElsterTable.h"
#include "config.h"
#include "language_select.h"
#include "sg_ready_controller.h"
#include <driver/twai.h>
#include <sstream>
#include <iomanip>
#include <set>
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
// ============================================================================
// CAN BUS MEMBER DEFINITIONS
// ============================================================================

typedef struct
{
    const char *Name;
    uint32_t CanId;
} CanMember;

static const CanMember CanMembers[] =
    {
        //  Name  CanId
        {"KESSEL", 0x180},
        {"ATEZ", 0x280},
        {"BEDIENMODUL_1", 0x300},
        {"BEDIENMODUL_2", 0x301},
        {"BEDIENMODUL_3", 0x302},
        {"BEDIENMODUL_4", 0x303},
        {"RAUMFERNFUEHLER", 0x400},
        {"RAUMFERNFUEHLER_2", 0x401},
        {"MANAGER", 0x480},
        {"HEIZMODUL", 0x500},
        {"BUSKOPPLER", 0x580},
        {"MISCHERMODUL_1", 0x600},
        {"MISCHERMODUL_3", 0x602},
        {"MISCHERMODUL_4", 0x603},
        {"PC", 0x680},
        {"FREMDGERAET", 0x700},
        {"DCF_MODUL", 0x780},
        // WPL17 (CAN-B, X1.2, 50 kbps) model-specific members. The WPL17 has two
        // buses (CAN-A 20 kbps / CAN-B 50 kbps); these addresses live on CAN-B.
        {"HEIZMODUL_WPL", 0x514},  // WPL17 Heizmodul (fine daily energy counters)
        {"KESSEL_CANB", 0x201},    // WPL17 DHW / room / heating-curve registers
        {"MISCHERMODUL_2", 0x601}, // WPL17 exposes the heating program here
        {"OTHER", 0x000}};

typedef enum
{
    // Die Reihenfolge muss mit CanMembers übereinstimmen!
    cm_kessel = 0,
    cm_atez,
    cm_bedienmodul_1,
    cm_bedienmodul_2,
    cm_bedienmodul_3,
    cm_bedienmodul_4,
    cm_raumfernfuehler,
    cm_raumfernfuehler_2,
    cm_manager,
    cm_heizmodul,
    cm_buskoppler,
    cm_mischermodul_1,
    cm_mischermodul_3,
    cm_mischermodul_4,
    cm_pc,
    cm_fremdgeraet,
    cm_dcf_modul,
    cm_heizmodul_wpl,
    cm_kessel_canb,
    cm_mischermodul_2,
    cm_other
} CanMemberType;

// ============================================================================
// MQTT AUTO-DISCOVERY CONFIGURATION
// ============================================================================

// ============================================================================
// CALCULATED SENSOR DISCOVERY CONFIGURATION
// ============================================================================

struct CalculatedSensorConfig {
    const char* uniqueId;          // Unique ID for Home Assistant
    const char* name;              // Friendly name
    const char* stateTopic;        // MQTT state topic
    const char* component;         // "sensor" or "binary_sensor"
    const char* deviceClass;       // Device class (empty string if none)
    const char* unit;              // Unit of measurement (empty string if none)
    const char* stateClass;        // State class (empty string if none)
    const char* icon;              // MDI icon
    const char* payloadOn;         // For binary sensors (empty string if not applicable)
    const char* payloadOff;        // For binary sensors (empty string if not applicable)
    const char* entityCategory;    // "diagnostic" | "config" | "" (empty = normal)
    bool enabledByDefault;         // false = disabled in HA until user enables
};

static const CalculatedSensorConfig calculatedSensors[] = {
    // Date sensor
    {"stiebel_calculated_date", LNAME_CALC_DATE, "heatingpump/calculated/date/state",
     "sensor", "", "", "", "mdi:calendar", "", "", "", true},

    // Time sensor
    {"stiebel_calculated_time", LNAME_CALC_TIME, "heatingpump/calculated/time/state",
     "sensor", "", "", "", "mdi:clock", "", "", "", true},

    // Betriebsart sensor
    {"stiebel_calculated_betriebsart", LNAME_CALC_BETRIEBSART, "heatingpump/calculated/betriebsart/state",
     "sensor", "", "", "", "mdi:cog", "", "", "", true},

    // Delta T continuous
    {"stiebel_calculated_delta_t_continuous", LNAME_CALC_DELTA_T_CONTINUOUS, "heatingpump/calculated/delta_t_continuous/state",
     "sensor", "temperature", "K", "measurement", "mdi:thermometer", "", "", "", true},

    // Delta T running (only when compressor active)
    {"stiebel_calculated_delta_t_running", LNAME_CALC_DELTA_T_RUNNING, "heatingpump/calculated/delta_t_running/state",
     "sensor", "temperature", "K", "measurement", "mdi:thermometer-chevron-up", "", "", "", true},

    // Compressor active binary sensor
    {"stiebel_calculated_compressor_active", LNAME_CALC_COMPRESSOR_ACTIVE, "heatingpump/calculated/compressor_active/state",
     "binary_sensor", "running", "", "", "mdi:engine", "on", "off", "", true},

    // CAN bus diagnostic sensors (TWAI / ESP32-S3; silently inactive on MCP2515 builds)
    {"stiebel_calculated_can_tec",        LNAME_CALC_CAN_TEC,        "heatingpump/calculated/can_tec/state",
     "sensor", "", "", "measurement", "mdi:alert-network", "", "", "diagnostic", false},
    {"stiebel_calculated_can_rec",        LNAME_CALC_CAN_REC,        "heatingpump/calculated/can_rec/state",
     "sensor", "", "", "measurement", "mdi:alert-network-outline", "", "", "diagnostic", false},
    {"stiebel_calculated_can_bus_errors", LNAME_CALC_CAN_BUS_ERRORS, "heatingpump/calculated/can_bus_errors/state",
     "sensor", "", "", "total_increasing", "mdi:network-off", "", "", "diagnostic", false},
    {"stiebel_calculated_can_state",      LNAME_CALC_CAN_STATE,      "heatingpump/calculated/can_state/state",
     "sensor", "", "", "", "mdi:can", "", "", "diagnostic", false},

    // Kombinierte Energie-Gesamtwerte (MWh*1000 + kWh), zusaetzlich zu den Einzelwerten
    {"stiebel_calculated_energie_heiz_total_kwh", LNAME_CALC_ENERGIE_HEIZ_TOTAL_KWH, "heatingpump/calculated/energie_heiz_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true},
    {"stiebel_calculated_energie_ww_total_kwh", LNAME_CALC_ENERGIE_WW_TOTAL_KWH, "heatingpump/calculated/energie_ww_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true},
    {"stiebel_calculated_waerme_heiz_total_kwh", LNAME_CALC_WAERME_HEIZ_TOTAL_KWH, "heatingpump/calculated/waerme_heiz_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true},
    {"stiebel_calculated_waerme_ww_total_kwh", LNAME_CALC_WAERME_WW_TOTAL_KWH, "heatingpump/calculated/waerme_ww_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true},

    // Aktuelle elektrische Leistung (W) — aus den feinen Tageszaehlern (EL_*_TAG_WH)
    // abgeleitet (Differenz der Wh-Zaehler pro Zeitintervall). Kein Stromzaehler noetig;
    // die WP zaehlt ihren Stromverbrauch selbst.
    {"stiebel_calculated_el_leistung_w", LNAME_CALC_EL_LEISTUNG_W, "heatingpump/calculated/el_leistung_w/state",
     "sensor", "power", "W", "measurement", "mdi:lightning-bolt", "", "", "", true},

    // Abgeleiteter Betriebsstatus: "Aus" | "Heizen" | "Warmwasser" | "Kuehlen".
    // Ersetzt das (auf der WPL17 unbrauchbare) WP_STATUS-Bitfeld, siehe Ableitung in
    // publishCombinedEnergySensors().
    {"stiebel_calculated_betriebsstatus", LNAME_CALC_BETRIEBSSTATUS, "heatingpump/calculated/betriebsstatus/state",
     "sensor", "", "", "", "mdi:heat-pump", "", "", "", true},

    // Kombinierte TAGES-Energiewerte (kWh-Anteil + Wh/1000 -> Wh-Aufloesung).
    // Die Tageszaehler aktualisieren laufend und fallen um Mitternacht auf 0
    // zurueck; der Geraete-Gesamtzahler springt nur einmal pro Nacht. In HA ist
    // state_class total_increasing korrekt (Reset wird als Zaehlerneustart
    // behandelt) und als Basis fuer utility_meter (Tag/Monat/Jahr) geeignet.
    {"stiebel_calculated_energie_heiz_tag_kwh", LNAME_CALC_ENERGIE_HEIZ_TAG_KWH, "heatingpump/calculated/energie_heiz_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true},
    {"stiebel_calculated_energie_ww_tag_kwh", LNAME_CALC_ENERGIE_WW_TAG_KWH, "heatingpump/calculated/energie_ww_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true},
    {"stiebel_calculated_waerme_heiz_tag_kwh", LNAME_CALC_WAERME_HEIZ_TAG_KWH, "heatingpump/calculated/waerme_heiz_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true},
    {"stiebel_calculated_waerme_ww_tag_kwh", LNAME_CALC_WAERME_WW_TAG_KWH, "heatingpump/calculated/waerme_ww_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true},

    // Getrennte elektrische Leistung (W) aus den jeweiligen TAG_WH-Zaehlern.
    // Damit ist ein evtl. Riemann-Integral oder eine Leistungsanzeige pro
    // Betriebsart (Heizen / Warmwasser) in HA moeglich.
    {"stiebel_calculated_el_leistung_heiz_w", LNAME_CALC_EL_LEISTUNG_HEIZ_W, "heatingpump/calculated/el_leistung_heiz_w/state",
     "sensor", "power", "W", "measurement", "mdi:lightning-bolt", "", "", "", true},
    {"stiebel_calculated_el_leistung_ww_w", LNAME_CALC_EL_LEISTUNG_WW_W, "heatingpump/calculated/el_leistung_ww_w/state",
     "sensor", "power", "W", "measurement", "mdi:lightning-bolt", "", "", "", true},
};

static const size_t CALCULATED_SENSOR_COUNT = sizeof(calculatedSensors) / sizeof(CalculatedSensorConfig);

// ============================================================================
// WRITABLE NUMBER CONFIGURATION (for MQTT Number entities)
// ============================================================================

struct WritableNumberConfig {
    const char* signalName;        // CAN signal name like "EINSTELL_SPEICHERSOLLTEMP"
    const char* friendlyName;      // Display name in Home Assistant
    CanMemberType member;          // Which CAN member to write to
    float min;                     // Minimum value
    float max;                     // Maximum value
    float step;                    // Step size
    const char* unit;              // Unit of measurement
    const char* icon;              // MDI icon
    const char* deviceClass;       // HA device class
};

static const WritableNumberConfig writableNumbers[] = {
    // ============================================================
    // SCHREIBBARE WERTE (MQTT Number Entities)
    // ============================================================
    // Hinweis fuer User: Moechtest du ein Feld nicht schreibbar haben
    // (z.B. weil es auf dem Dashboard nicht erscheinen soll oder am
    // Geraet nicht vorhanden ist), kommentiere die Zeile aus (// davor).
    //
    // ACHTUNG: Diese Werte werden DIREKT auf die Waermepumpe geschrieben.
    // Nur aktivieren, was das Geraet wirklich kann. Die "heiklen"
    // Felder (die du nicht im Dashboard zeigen willst) kannst du
    // einfach hier lassen und im Dashboard ausblenden.
    // ============================================================

    // Primary storage target temperature (WW-Solltemperatur)
    {"EINSTELL_SPEICHERSOLLTEMP", LNAME_NUM_SPEICHERSOLLTEMP,
     cm_manager, 20.0, 60.0, 1.0, "°C", "mdi:thermometer-high", "temperature"},

    // Secondary storage target temperature (comfort/eco modes)
    {"EINSTELL_SPEICHERSOLLTEMP2", LNAME_NUM_SPEICHERSOLLTEMP2,
     cm_manager, 20.0, 60.0, 1.0, "°C", "mdi:thermometer-low", "temperature"},

    // SG Ready boost temperatures (not real CAN signals, handled internally)
    {"SG_READY_BOOST_STATE3", LNAME_NUM_SG_READY_BOOST3,
     cm_manager, 0.0, 10.0, 0.5, "°C", "mdi:thermometer-plus", "temperature"},

    {"SG_READY_BOOST_STATE4", LNAME_NUM_SG_READY_BOOST4,
     cm_manager, 0.0, 15.0, 0.5, "°C", "mdi:thermometer-chevron-up", "temperature"},

    // Room temperature setpoints for heating circuits 1–3 and night/reduced mode.
    // ACHTUNG: WPL17 hat nur 1 Heizkreis -> RAUMSOLLTEMP_II/_III liefern -255.
    // Sie sind fuer wpl17 automatisch uebersprungen (siehe publishAllWritableNumberDiscoveries).
    {"RAUMSOLLTEMP_I", LNAME_NUM_RAUMSOLLTEMP_I,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_II", LNAME_NUM_RAUMSOLLTEMP_II,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_III", LNAME_NUM_RAUMSOLLTEMP_III,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_NACHT", LNAME_NUM_RAUMSOLLTEMP_NACHT,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer-outline", "temperature"},

    // Heizkurve (Steigung), et_cent_val -> 0.40 wird als roh 40 geschrieben.
    // Liegt auf KESSEL_CANB (0x201) — dort ist der schreibbare Wert.
    {"HEIZKURVE", LNAME_NUM_HEIZKURVE,
     cm_kessel_canb, 0.0, 2.0, 0.05, "", "mdi:chart-line", ""}
};

static const size_t WRITABLE_NUMBER_COUNT = sizeof(writableNumbers) / sizeof(WritableNumberConfig);

// ============================================================================
// WRITABLE SELECT CONFIGURATION (for MQTT Select entities)
// ============================================================================

struct WritableSelectConfig {
    const char* signalName;        // CAN signal name like "PROGRAMMSCHALTER"
    const char* friendlyName;      // Display name in Home Assistant
    CanMemberType member;          // Which CAN member to write to
    const char** options;          // Array of option strings
    size_t optionCount;            // Number of options
    const char* icon;              // MDI icon
};

// Operating mode options for PROGRAMMSCHALTER
static const char* programmschalterOptions[] = {
    LNAME_OPT_NOTBETRIEB,
    LNAME_OPT_BEREITSCHAFT,
    LNAME_OPT_AUTOMATIK,
    LNAME_OPT_TAGBETRIEB,
    LNAME_OPT_ABSENKBETRIEB,
    LNAME_OPT_WARMWASSER,
};

// SG Ready state options (Smart Grid Ready)
static const char* sgReadyOptions[] = {
    LNAME_OPT_SG_EVU_SPERRE,
    LNAME_OPT_SG_NORMAL,
    LNAME_OPT_SG_EMPFOHLEN,
    LNAME_OPT_SG_ZWANG,
};

static const WritableSelectConfig writableSelects[] = {
    // ============================================================
    // SCHREIBBARE AUSWAHLFELDER (MQTT Select Entities)
    // ============================================================
    // Auskommentieren = Feld erscheint nicht als schreibbares Element.
    // ============================================================

    // Heat pump operating mode (Automatik/Tagbetrieb/Absenkbetrieb/etc.)
    {"PROGRAMMSCHALTER", LNAME_SEL_PROGRAMMSCHALTER,
     cm_manager, programmschalterOptions, 6, "mdi:dip-switch"},

    // SG Ready state (Smart Grid Ready)
    {"SG_READY_STATE", LNAME_SEL_SG_READY,
     cm_manager, sgReadyOptions, 4, "mdi:solar-power"}
};

static const size_t WRITABLE_SELECT_COUNT = sizeof(writableSelects) / sizeof(WritableSelectConfig);

// ============================================================================
// WRITABLE DATETIME CONFIGURATION (for MQTT Datetime entities)
// ============================================================================
// RUNTIME STATE TRACKING
// ============================================================================

// SG Ready: legacy globals kept for YAML sensor lambdas that read currentSgReadyState
// The controller is the authoritative source; these are updated via sgReadyStateInt().
static int currentSgReadyState = 2;

// Track which signals have been discovered
static std::set<std::string> discoveredSignals;

// Track which calculated sensors have been discovered
static std::set<std::string> discoveredCalculatedSensors;

// Track which writable numbers have been discovered
static std::set<std::string> discoveredWritableNumbers;

// Track which writable selects have been discovered
static std::set<std::string> discoveredWritableSelects;

// Track next scheduled request time per unique signal key (MEMBER_SIGNAL)
static std::unordered_map<std::string, unsigned long> nextRequestTime;

// Track starting position for round-robin signal processing (prevents starvation)
static int signalProcessingStartIndex = 0;

// Track sensor values for calculated sensors
static float lastWpVorlaufIst = NAN;
static float lastRuecklaufIstTemp = NAN;
static float lastVerdichterValue = NAN;

// Track date/time component values
static int lastJahr = -1;
static int lastMonat = -1;
static int lastTag = -1;
static int lastStunde = -1;
static int lastMinute = -1;
static int lastSekunde = -1;

// Scheduled update times for calculated sensors (milliseconds)
static unsigned long nextDeltaTUpdate = 0;
static unsigned long nextCompressorUpdate = 0;
static unsigned long nextDateTimeUpdate = 0;
static unsigned long nextBetriebsartUpdate = 0;
static unsigned long nextCanDiagUpdate = 0;

// UID cache to avoid repeated string operations on every signal update
static std::unordered_map<std::string, std::string> uidCache;

// ============================================================================
// SIGNAL REQUEST CONFIGURATION
// ============================================================================

// Signal request configuration structure
typedef struct {
    const char* signalName;
    unsigned long frequency;     // Request frequency in seconds
    CanMemberType member;        // Use cm_other for "all members"
} SignalRequest;

// Forward declarations for the model-specific signal request table.
// The actual definition comes from signal_requests_*.h, included by the
// model's YAML package (e.g. wpl13e.yaml via esphome: includes:).
extern const SignalRequest signalRequests[];
extern const size_t SIGNAL_REQUEST_COUNT_VALUE;
#define SIGNAL_REQUEST_COUNT SIGNAL_REQUEST_COUNT_VALUE

// Runtime state for signal request manager
static bool requestManagerStarted = false;
static unsigned long requestManagerStartTime = 0;

// PASSIVE LISTEN MODE: wenn true, sendet processSignalRequests() NICHTS.
// Wird von der YAML-Konfiguration (heatingpump_scan.yaml) per on_boot gesetzt,
// damit die Scan-Firmware rein passiv auf dem Bus lauscht.
static bool disableCanRequests = false;

// ============================================================================
// CAN BUS HELPER FUNCTIONS
// ============================================================================

// Only compile function implementations when building with ESPHome framework
// Skip when compiling ha-dummy.cpp or other standalone contexts
#if !defined(HA_DUMMY_BUILD)

/**
 * Simple struct for CAN ID bytes (avoids heap allocation)
 */
struct CanIdBytes {
    uint8_t first;
    uint8_t second;
};

/**
 * Generate CAN read ID from member CAN ID
 */
CanIdBytes generate_read_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>((address & 0xF0) + 1), static_cast<uint8_t>(can_id & 7)};
}

CanIdBytes generate_write_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>(address & 0xF0), static_cast<uint8_t>(can_id & 7)};
}

const CanMember &lookupCanMember(uint32_t canId)
{
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i) {
        if (CanMembers[i].CanId == canId) {
            return CanMembers[i];
        }
    }
    // Return the last element if member is not found
    return CanMembers[sizeof(CanMembers) / sizeof(CanMembers[0]) - 1];
}

// Check if signal is permanently blacklisted (lookup in ElsterTable)
inline bool isPermanentlyBlacklisted(const char* signalName) {
    const ElsterIndex *ei = GetElsterIndex(signalName);
    return ei->isBlacklisted;
}

// Helper: Get type-based defaults for Home Assistant metadata
inline void getTypeDefaults(ElsterType type, const char*& component, const char*& deviceClass, const char*& unit, const char*& stateClass, const char*& icon) {
    switch(type) {
        case et_dec_val:
        case et_cent_val:
            component = "sensor"; deviceClass = "temperature"; unit = "°C"; stateClass = "measurement"; icon = "mdi:thermometer";
            break;
        case et_mil_val:
            component = "sensor"; deviceClass = "duration"; unit = "h"; stateClass = "total_increasing"; icon = "mdi:timer";
            break;
        case et_bool:
        case et_little_bool:
            component = "binary_sensor"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:electric-switch";
            break;
        case et_byte:
        case et_little_endian:
            component = "sensor"; deviceClass = ""; unit = ""; stateClass = "measurement"; icon = "mdi:counter";
            break;
        case et_err_nr:
            component = "sensor"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:alert-circle";
            break;
        case et_dev_id:
        case et_dev_nr:
            component = "text"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:identifier";
            break;
        case et_betriebsart:
            component = "sensor"; deviceClass = "enum"; unit = ""; stateClass = ""; icon = "mdi:hvac";
            break;
        case et_zeit:
            component = "sensor"; deviceClass = "timestamp"; unit = ""; stateClass = ""; icon = "mdi:clock";
            break;
        case et_datum:
            component = "sensor"; deviceClass = "date"; unit = ""; stateClass = ""; icon = "mdi:calendar";
            break;
        default:
            component = "sensor"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:gauge";
            break;
    }
}

const ElsterIndex *processCanMessage(const std::vector<uint8_t> &msg, uint32_t can_id, std::string &signalValue, const CanMember **outCanMember)
{
    // Return if the message is too small
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const CanMember &cm = lookupCanMember(can_id);
    *outCanMember = &cm;  // Return CanMember to caller

    const ElsterIndex *ei;
    uint8_t byte1;
    uint8_t byte2;
    char charValue[16];

    if (msg[2] == 0xfa)
    {
        byte1 = msg[5];
        byte2 = msg[6];
        ei = GetElsterIndex(msg[4] + (msg[3] << 8));
    }
    else
    {
        byte1 = msg[3];
        byte2 = msg[4];
        ei = GetElsterIndex(msg[2]);
    }

    switch (ei->Type)
    {
    case et_double_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    case et_triple_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    default:
        SetValueType(charValue, ei->Type, static_cast<int>(byte2 + (byte1 << 8)));
        break;
    }

    uint16_t rawIdx = (msg[2] == 0xfa) ? (uint16_t)(msg[4] + (msg[3] << 8)) : (uint16_t)msg[2];
    ESP_LOGI("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(idx=0x%04x, %s)", cm.Name, (unsigned)can_id, ei->Name, charValue, (unsigned)rawIdx, ElsterTypeStr[ei->Type]);

    signalValue = charValue;
    return ei;
}

// Passiver Dekodierer: identisch zu processCanMessage, aber OHNE INFO-Log.
// Gibt den ElsterIndex + formatierten Wert zurueck, damit der LISTEN-Sniffer
// gezielt nur Temperatur-Signale (et_dec_val/et_cent_val) loggen kann.
// Hinweis: 0x8000 (kein Sensor) wird als Wert -255 formatiert (SetValueType).
const ElsterIndex *processCanMessageDebug(const std::vector<uint8_t> &msg, uint32_t can_id, std::string &signalValue, const CanMember **outCanMember)
{
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const CanMember &cm = lookupCanMember(can_id);
    *outCanMember = &cm;

    const ElsterIndex *ei;
    uint8_t byte1;
    uint8_t byte2;
    char charValue[16];

    if (msg[2] == 0xfa)
    {
        byte1 = msg[5];
        byte2 = msg[6];
        ei = GetElsterIndex(msg[4] + (msg[3] << 8));
    }
    else
    {
        byte1 = msg[3];
        byte2 = msg[4];
        ei = GetElsterIndex(msg[2]);
    }

    switch (ei->Type)
    {
    case et_double_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    case et_triple_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    default:
        SetValueType(charValue, ei->Type, static_cast<int>(byte2 + (byte1 << 8)));
        break;
    }

    signalValue = charValue;
    return ei;
}

void readSignal(const CanMember *cm, const ElsterIndex *ei)
{
    constexpr bool use_extended_id = false; // No use of extended ID
    const uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    const uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    CanIdBytes readId = generate_read_id(cm->CanId);

    if (IndexByte1 == 0x00)
    {
        data = {readId.first,
                readId.second,
                IndexByte2,
                0x00,
                0x00,
                0x00,
                0x00};
    }
    else
    {
        data = {readId.first,
                readId.second,
                0xFA,
                IndexByte1,
                IndexByte2,
                0x00,
                0x00};
    }

    char logmsg[160];
    snprintf(logmsg, sizeof(logmsg), "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, cm->Name, (unsigned)cm->CanId, readId.first, readId.second, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("readSignal()", "%s", logmsg);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void readSignal(const CanMember *cm, const char *elsterName)
{
    readSignal(cm, GetElsterIndex(elsterName));
    return;
}

void writeSignal(const CanMember *cm, const ElsterIndex *ei, const char *&str)
{
    bool use_extended_id = false;
    int writeValue = TranslateString(str, ei->Type);
    if (writeValue == -1) {
        ESP_LOGW("writeSignal()", "TranslateString failed for \"%s\" (input: \"%s\") — refusing to write", ei->Name, str);
        return;
    }
    uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    CanIdBytes writeId = generate_write_id(cm->CanId);

    if (IndexByte1 == 0x00)
    {
        data = {writeId.first,
                writeId.second,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF),
                0x00,
                0x00};
    }
    else
    {
        data = {writeId.first,
                writeId.second,
                0xfa,
                IndexByte1,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF)};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             ei->Name, ei->Index, writeValue, cm->Name, (unsigned)cm->CanId, writeId.first, writeId.second,
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("writeSignal()", "%s", logmsg);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void writeSignal(const CanMember *cm, const char *elsterName, const char *str)
{
    const char* strCopy = str;  // Create a non-const pointer we can pass by reference
    writeSignal(cm, GetElsterIndex(elsterName), strCopy);
    return;
}

// Unified function to publish MQTT discovery for calculated sensors
void publishCalculatedSensorDiscovery(const CalculatedSensorConfig& config, bool forceRepublish = false) {
    // Check if already published (unless force republish)
    if (!forceRepublish && discoveredCalculatedSensors.find(config.uniqueId) != discoveredCalculatedSensors.end()) {
        return;
    }
    
    // Mark as discovered
    discoveredCalculatedSensors.insert(config.uniqueId);
    
    // Build discovery topic
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/%s/heatingpump/%s/config", config.component, config.uniqueId);
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.name << "\","
            << "\"unique_id\":\"" << config.uniqueId << "\","
            << "\"state_topic\":\"" << config.stateTopic << "\"";
    
    // Add device class if specified
    if (config.deviceClass[0] != '\0') {
        payload << ",\"device_class\":\"" << config.deviceClass << "\"";
    }
    
    // Add unit if specified
    if (config.unit[0] != '\0') {
        payload << ",\"unit_of_measurement\":\"" << config.unit << "\"";
    }
    
    // Add state class if specified
    if (config.stateClass[0] != '\0') {
        payload << ",\"state_class\":\"" << config.stateClass << "\"";
    }
    
    // Add icon if specified
    if (config.icon[0] != '\0') {
        payload << ",\"icon\":\"" << config.icon << "\"";
    }
    
    // For binary sensors, add payload on/off
    if (config.payloadOn[0] != '\0' && config.payloadOff[0] != '\0') {
        payload << ",\"payload_on\":\"" << config.payloadOn << "\","
                << "\"payload_off\":\"" << config.payloadOff << "\"";
    }

    // Add entity category if specified
    if (config.entityCategory[0] != '\0') {
        payload << ",\"entity_category\":\"" << config.entityCategory << "\"";
    }

    // Add enabled_by_default if false (true is HA default, no need to emit)
    if (!config.enabledByDefault) {
        payload << ",\"enabled_by_default\":false";
    }

    // Add device info
    payload << ",\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";

    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    ESP_LOGI("MQTT", "Discovery published for calculated sensor: %s", config.name);
}

// Publish all calculated sensor discoveries (used during startup and republish)
// Energy/power/status sensors (calculatedSensors[10..]) need model-specific registers
// (SUM_KWH, TAG_*); they announce themselves on their first value in
// publishCombinedEnergySensors(), so models without them get no dead HA entities.
static const size_t FIRST_ON_DEMAND_CALCULATED_SENSOR = 10;

void publishAllCalculatedSensorDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        ESP_LOGI("MQTT", "Republishing all calculated sensor discoveries");
    }

    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; i++) {
        if (i >= FIRST_ON_DEMAND_CALCULATED_SENSOR &&
            discoveredCalculatedSensors.find(calculatedSensors[i].uniqueId) == discoveredCalculatedSensors.end()) {
            continue;   // not seen a value yet
        }
        publishCalculatedSensorDiscovery(calculatedSensors[i], forceRepublish);
    }
}

// ============================================================================
// MQTT WRITABLE NUMBER (for temperature setpoints, etc.)
// ============================================================================

// Liefert den effektiven CAN-Member fuer einen writable number Eintrag.
// Auf WPL17 (CAN-B) liegen EINSTELL_SPEICHERSOLLTEMP / _2 und HEIZKURVE auf dem
// Kessel (0x201) statt auf dem Manager (0x480). Andere Modelle (CAN-A) behalten
// den konfigurierten Member. Damit erscheint das HA-Eingabefeld am richtigen
// Geraet und der Command-/State-Topic passt zum Owner des Registers.
static const CanMember* getWritableNumberMember(const WritableNumberConfig& config) {
    if (strcmp(HA_DEVICE_MODEL_STR, "wpl17") == 0) {
        if (strcmp(config.signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0 ||
            strcmp(config.signalName, "EINSTELL_SPEICHERSOLLTEMP2") == 0 ||
            strcmp(config.signalName, "HEIZKURVE") == 0) {
            return &CanMembers[cm_kessel_canb];
        }
    }
    return &CanMembers[config.member];
}

// Publish MQTT discovery for a writable number entity
void publishWritableNumberDiscovery(const WritableNumberConfig& config, bool forceRepublish = false) {
#ifdef HA_DISABLE_SG_READY_true
    if (strncmp(config.signalName, "SG_READY_", 9) == 0) return;  // SG Ready disabled via substitution
#endif
    // Get CanMember for building topics (modellabhaengig fuer WPL17)
    const CanMember* cm = getWritableNumberMember(config);
    
    // Build unique ID. HEIZKURVE gets a "_set" suffix so it never collides with the
    // read-only HEIZKURVE signal sensor of the same name (sensor.kessel_canb_heizkurve).
    char uniqueId[128];
    if (strcmp(config.signalName, "HEIZKURVE") == 0) {
        snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s_set", cm->Name, config.signalName);
    } else {
        snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm->Name, config.signalName);
    }
    std::string uid(uniqueId);
    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
    
    // Check if already published (unless force republish)
    if (!forceRepublish && discoveredWritableNumbers.find(uid) != discoveredWritableNumbers.end()) {
        return;
    }
    
    // Mark as discovered
    discoveredWritableNumbers.insert(uid);
    
    // Build discovery topic: homeassistant/number/heatingpump/<unique_id>/config
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/number/heatingpump/%s/config", uid.c_str());
    
    // Build command topic: heatingpump/<MEMBER>/<SIGNAL>/set
    char commandTopic[128];
    snprintf(commandTopic, sizeof(commandTopic), "heatingpump/%s/%s/set", cm->Name, config.signalName);
    
    // Build state topic: heatingpump/<MEMBER>/<SIGNAL>/state
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm->Name, config.signalName);
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << "\"command_topic\":\"" << commandTopic << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"min\":" << config.min << ","
            << "\"max\":" << config.max << ","
            << "\"step\":" << config.step << ","
            << "\"mode\":\"box\",";
    // Nur nicht-leere optionale Felder senden: HA verwirft sonst die ganze
    // Discovery (z. B. 'device_class':'' ist fuer 'number' ungueltig).
    if (config.unit[0] != '\0') {
        payload << "\"unit_of_measurement\":\"" << config.unit << "\",";
    }
    if (config.deviceClass[0] != '\0') {
        payload << "\"device_class\":\"" << config.deviceClass << "\",";
    }
    payload << "\"icon\":\"" << config.icon << "\",";
    
    // Check if this is an SG Ready control - assign to main device
    bool isSgReady = (strncmp(config.signalName, "SG_READY_", 9) == 0);
    
    if (isSgReady) {
        // SG Ready controls go to main device
        payload << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    } else {
        // Regular controls go to their respective CAN member device
        char canMemberDeviceId[64];
        snprintf(canMemberDeviceId, sizeof(canMemberDeviceId), "stiebel_%s", cm->Name);

        const char* canMemberFriendlyName = cm->Name;
        if (strcmp(cm->Name, "KESSEL") == 0) canMemberFriendlyName = "Kessel";
        else if (strcmp(cm->Name, "MANAGER") == 0) canMemberFriendlyName = "Manager";
        else if (strcmp(cm->Name, "HEIZMODUL") == 0) canMemberFriendlyName = "Heizmodul";

        payload << "\"device\":{\"identifiers\":[\"" << canMemberDeviceId << "\"],"
                << "\"name\":\"" << canMemberFriendlyName << "\","
                << "\"via_device\":\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    }

    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    ESP_LOGI("MQTT", "Discovery published for writable number: %s", config.friendlyName);
}

// Publish all writable number discoveries
void publishAllWritableNumberDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredWritableNumbers.clear();
        ESP_LOGI("MQTT", "Republishing all writable number discoveries");
    }
    
    ESP_LOGI("MQTT_DISC", "Publishing %d writable number discoveries", WRITABLE_NUMBER_COUNT);
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        // WPL17 has only ONE heating circuit (RAUMSOLLTEMP_I). Skip II/III so the
        // number entities are not created on single-circuit models (-255 only).
        const char* sig = writableNumbers[i].signalName;
        bool isWpl17 = (strcmp(HA_DEVICE_MODEL_STR, "wpl17") == 0);
        // WPL17 has only ONE heating circuit (RAUMSOLLTEMP_I). Skip II/III so the
        // number entities are not created on single-circuit models (-255 only).
        if ((strcmp(sig, "RAUMSOLLTEMP_II") == 0 || strcmp(sig, "RAUMSOLLTEMP_III") == 0) && isWpl17) {
            ESP_LOGI("MQTT_DISC", "Skipping %s on %s (single heating circuit)", sig, HA_DEVICE_MODEL_STR);
            continue;
        }
        // HEIZKURVE only exists on KESSEL_CANB (WPL17); skip on other models.
        if (strcmp(sig, "HEIZKURVE") == 0 && !isWpl17) {
            ESP_LOGI("MQTT_DISC", "Skipping %s on %s (WPL17 only)", sig, HA_DEVICE_MODEL_STR);
            continue;
        }
        ESP_LOGI("MQTT_DISC", "Publishing writable number %d: %s", i, sig);
        publishWritableNumberDiscovery(writableNumbers[i], forceRepublish);
    }
    ESP_LOGI("MQTT_DISC", "Completed publishing writable number discoveries");
}

// ============================================================================
// WRITABLE SELECT DISCOVERY (MQTT Select entities)
// ============================================================================

void publishWritableSelectDiscovery(const WritableSelectConfig& config, bool forceRepublish = false) {
#ifdef HA_DISABLE_SG_READY_true
    if (strncmp(config.signalName, "SG_READY_", 9) == 0) return;  // SG Ready disabled via substitution
#endif
    // Get CAN member
    const CanMember* cm = &CanMembers[config.member];
    
    // Build unique ID (lowercase with underscores)
    char uniqueId[128];
    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm->Name, config.signalName);
    std::string uniqueIdStr(uniqueId);
    std::transform(uniqueIdStr.begin(), uniqueIdStr.end(), uniqueIdStr.begin(), ::tolower);
    
    // Check if already published (unless forcing)
    if (!forceRepublish && discoveredWritableSelects.count(uniqueIdStr) > 0) {
        return;
    }
    
    // Build discovery topic
    std::ostringstream discoveryTopicStream;
    discoveryTopicStream << "homeassistant/select/heatingpump/" << uniqueIdStr << "/config";
    std::string discoveryTopic = discoveryTopicStream.str();
    
    // Build command and state topics
    std::ostringstream commandTopicStream;
    commandTopicStream << "heatingpump/" << cm->Name << "/" << config.signalName << "/set";
    std::string commandTopic = commandTopicStream.str();
    
    std::ostringstream stateTopicStream;
    stateTopicStream << "heatingpump/" << cm->Name << "/" << config.signalName << "/state";
    std::string stateTopic = stateTopicStream.str();
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uniqueIdStr << "\","
            << "\"command_topic\":\"" << commandTopic << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"options\":[";
    
    // Add options array
    for (size_t i = 0; i < config.optionCount; i++) {
        if (i > 0) payload << ",";
        payload << "\"" << config.options[i] << "\"";
    }
    payload << "],";
    
    // Add icon if specified
    if (config.icon && strlen(config.icon) > 0) {
        payload << "\"icon\":\"" << config.icon << "\",";
    }
    
    // Check if this is an SG Ready control - assign to main device
    bool isSgReady = (strncmp(config.signalName, "SG_READY_", 9) == 0);
    
    if (isSgReady) {
        // SG Ready controls go to main device
        payload << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    } else {
        // Regular controls go to their respective CAN member device
        char canMemberDeviceId[64];
        snprintf(canMemberDeviceId, sizeof(canMemberDeviceId), "stiebel_%s", cm->Name);

        const char* canMemberFriendlyName = cm->Name;
        if (strcmp(cm->Name, "KESSEL") == 0) canMemberFriendlyName = "Kessel";
        else if (strcmp(cm->Name, "MANAGER") == 0) canMemberFriendlyName = "Manager";
        else if (strcmp(cm->Name, "HEIZMODUL") == 0) canMemberFriendlyName = "Heizmodul";

        payload << "\"device\":{\"identifiers\":[\"" << canMemberDeviceId << "\"],"
                << "\"name\":\"" << canMemberFriendlyName << "\","
                << "\"via_device\":\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    }

    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    // Mark as discovered
    discoveredWritableSelects.insert(uniqueIdStr);
    
    ESP_LOGI("MQTT", "Discovery published for writable select: %s", config.friendlyName);
}

// Publish all writable select discoveries
void publishAllWritableSelectDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredWritableSelects.clear();
        ESP_LOGI("MQTT", "Republishing all writable select discoveries");
    }
    
    ESP_LOGI("MQTT_DISC", "Publishing %d writable select discoveries", WRITABLE_SELECT_COUNT);
    for (size_t i = 0; i < WRITABLE_SELECT_COUNT; i++) {
        ESP_LOGI("MQTT_DISC", "Publishing writable select %d: %s", i, writableSelects[i].signalName);
        publishWritableSelectDiscovery(writableSelects[i], forceRepublish);
    }
    ESP_LOGI("MQTT_DISC", "Completed publishing writable select discoveries");
}

// ============================================================================
// Note: Datetime controls are now implemented using Home Assistant input_datetime
// helpers synced via text_sensor platform and triggered by buttons.
// MQTT datetime discovery is not supported by Home Assistant.
// ============================================================================

void publishDate()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[0]);
    
    // Validate that all values have been received
    if (lastJahr < 0 || lastMonat < 0 || lastTag < 0) {
        ESP_LOGW("CALC", "Cannot publish date: sensors not initialized (Jahr=%d, Monat=%d, Tag=%d)", 
                 lastJahr, lastMonat, lastTag);
        return;
    }
    
    int ijahr = lastJahr;
    int imonat = lastMonat;
    int itag = lastTag;
    
    // Log raw values for debugging
    // ESP_LOGD("CALC", "Date values: Jahr=%d, Monat=%d, Tag=%d", ijahr, imonat, itag);
    
    // Validate ranges
    if (ijahr < 0 || ijahr > 99 || imonat < 1 || imonat > 12 || itag < 1 || itag > 31) {
        ESP_LOGW("CALC", "Date values out of range: Jahr=%d, Monat=%d, Tag=%d", ijahr, imonat, itag);
        return;
    }
    
    // Format date components
    std::string jahr = (ijahr < 10) ? "0" + std::to_string(ijahr) : std::to_string(ijahr);
    std::string monat = (imonat < 10) ? "0" + std::to_string(imonat) : std::to_string(imonat);
    std::string tag = (itag < 10) ? "0" + std::to_string(itag) : std::to_string(itag);
    
    std::string datum = "20" + jahr + "-" + monat + "-" + tag;
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/date/state";
    id(mqtt_client).publish(stateTopic, datum.c_str(), datum.length(), 0, true);
    ESP_LOGI("CALC", "Published date: %s (Jahr=%d, Monat=%d, Tag=%d)", datum.c_str(), ijahr, imonat, itag);
}

void publishTime()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[1]);
    
    // Validate that the essential values have been received.
    // Some models (e.g. WPL 10 AC / WPM3) do not provide SEKUNDE, so treat it
    // as optional and fall back to 0 rather than refusing to publish.
    if (lastStunde < 0 || lastMinute < 0) {
        ESP_LOGW("CALC", "Cannot publish time: sensors not initialized (Stunde=%d, Minute=%d, Sekunde=%d)", 
                 lastStunde, lastMinute, lastSekunde);
        return;
    }
    
    int istunde = lastStunde;
    int iminute = lastMinute;
    int isekunde = (lastSekunde < 0) ? 0 : lastSekunde;
    
    // Log raw values for debugging
    // ESP_LOGD("CALC", "Time values: Stunde=%d, Minute=%d, Sekunde=%d", istunde, iminute, isekunde);
    
    // Validate ranges
    if (istunde < 0 || istunde > 23 || iminute < 0 || iminute > 59 || isekunde < 0 || isekunde > 59) {
        ESP_LOGW("CALC", "Time values out of range: Stunde=%d, Minute=%d, Sekunde=%d", istunde, iminute, isekunde);
        return;
    }
    
    // Format time directly without helper function
    char zeit[9];
    snprintf(zeit, sizeof(zeit), "%02d:%02d:%02d", istunde, iminute, isekunde);
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/time/state";
    id(mqtt_client).publish(stateTopic, zeit, strlen(zeit), 0, true);
    ESP_LOGI("CALC", "Published time: %s (Stunde=%d, Minute=%d, Sekunde=%d)", zeit, istunde, iminute, isekunde);
}

// Datetime states are now handled by Home Assistant input_datetime helpers
// Read by ESPHome text_sensors and written via button triggers

void publishBetriebsart(const std::string& sommerBetriebValue)
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[2]);
    
    // Determine Betriebsart based on SOMMERBETRIEB value
    // SOMMERBETRIEB is et_little_bool type, so value is "on" or "off"
    std::string betriebsart;
    std::string icon;
    
    if (sommerBetriebValue == "on") {
        betriebsart = "Sommerbetrieb";
        icon = "mdi:white-balance-sunny";
    } else {
        betriebsart = "Normalbetrieb";
        icon = "mdi:circle-outline";
    }
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/betriebsart/state";
    id(mqtt_client).publish(stateTopic, betriebsart.c_str(), betriebsart.length(), 0, true);
    // ESP_LOGD("CALC", "Published Betriebsart: %s (SOMMERBETRIEB=%s)", betriebsart.c_str(), sommerBetriebValue.c_str());
}

void publishDeltaTContinuous()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[3]);
    
    // Check if both temperature values are valid
    if (std::isnan(lastWpVorlaufIst) || std::isnan(lastRuecklaufIstTemp)) {
        return; // Silently skip if values not ready
    }
    
    // Validate temperature range (sanity check)
    if (lastWpVorlaufIst < -50 || lastRuecklaufIstTemp < -50) {
        return; // Silently skip invalid ranges
    }
    
    float deltaT = lastWpVorlaufIst - lastRuecklaufIstTemp;
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = "heatingpump/calculated/delta_t_continuous/state";
    id(mqtt_client).publish(stateTopic, value, strlen(value), 0, true);
}

void publishDeltaTRunning()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[4]);
    
    // Check if compressor is running (value > 2 or not NaN and not 0)
    bool compressorRunning = (!std::isnan(lastVerdichterValue) && lastVerdichterValue > 2.0);
    
    if (!compressorRunning) {
        return; // Silently skip if compressor not running
    }
    
    // Check if both temperature values are valid
    if (std::isnan(lastWpVorlaufIst) || std::isnan(lastRuecklaufIstTemp)) {
        return; // Silently skip if values not ready
    }
    
    // Validate temperature range (sanity check)
    if (lastWpVorlaufIst < -50 || lastRuecklaufIstTemp < -50) {
        return; // Silently skip invalid ranges
    }
    
    float deltaT = lastWpVorlaufIst - lastRuecklaufIstTemp;
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = "heatingpump/calculated/delta_t_running/state";
    id(mqtt_client).publish(stateTopic, value, strlen(value), 0, true);
}

void publishCompressorActive()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[5]);
    
    // Check if compressor value is valid
    if (std::isnan(lastVerdichterValue)) {
        return; // Silently skip if value invalid
    }
    
    // Compressor is active if value > 2
    bool isActive = (lastVerdichterValue > 2.0);
    
    // Publish state to MQTT
    const char* state = isActive ? "on" : "off";
    const char* stateTopic = "heatingpump/calculated/compressor_active/state";
    id(mqtt_client).publish(stateTopic, state, strlen(state), 0, true);
    // Note: Delta T running is published separately by scheduler
}

// Helper: Check if pattern appears anywhere in text (case-insensitive substring match)
// Get or create cached UID for a signal (eliminates repeated string ops)
inline std::string getOrCreateUID(const CanMember &cm, const char* signalName) {
    // Create cache key
    char cacheKey[256];
    snprintf(cacheKey, sizeof(cacheKey), "%u:%s", (unsigned)cm.CanId, signalName);
    
    // Check cache first
    auto it = uidCache.find(cacheKey);
    if (it != uidCache.end()) {
        return it->second;
    }
    
    // Generate UID if not cached (no need to lookup CanMember - already have it)
    char uid[128];
    snprintf(uid, sizeof(uid), "stiebel_%s_%s", cm.Name, signalName);
    std::string result(uid);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '_');
    
    // Store in cache
    uidCache[cacheKey] = result;
    return result;
}

// Publish MQTT Discovery config for a signal
void publishMqttDiscovery(const CanMember &cm, const ElsterIndex *ei) {
    
    // Get cached UID (avoids repeated string operations)
    std::string uid = getOrCreateUID(cm, ei->Name);
    
    // Note: Caller is responsible for checking discoveredSignals and inserting uid
    // This function just publishes the MQTT discovery message
    
    // Get friendly name: use ei->friendlyName or fallback to ei->Name
    const char* friendlyName = (ei->hasMetadata && ei->friendlyName) ? ei->friendlyName : ei->Name;
    
    // Get metadata: use ei fields if available, otherwise fall back to type defaults
    const char *component, *deviceClass, *unit, *stateClass, *icon;
    const char *payloadOn = nullptr, *payloadOff = nullptr;
    
    if (ei->hasMetadata && ei->haComponent) {
        // Use metadata from ElsterTable
        component = ei->haComponent;
        deviceClass = ei->haDeviceClass ? ei->haDeviceClass : "";
        unit = ei->unit ? ei->unit : "";
        stateClass = ei->stateClass ? ei->stateClass : "";
        icon = ei->icon ? ei->icon : "";
        payloadOn = ei->payloadOn;
        payloadOff = ei->payloadOff;
    } else {
        // Fall back to type-based defaults
        getTypeDefaults((ElsterType)ei->Type, component, deviceClass, unit, stateClass, icon);
        // For binary sensors, set default payloads
        if (strcmp(component, "binary_sensor") == 0) {
            payloadOn = "on";
            payloadOff = "off";
        }
    }
    
    // Build discovery topic
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/%s/heatingpump/%s/config", component, uid.c_str());
    
    // Build state topic
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    
    // Build JSON payload efficiently using ostringstream
    std::ostringstream payload;
    payload << "{\"name\":\"" << friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"availability_topic\":\"heatingpump/status\"";
    
    // For binary sensors, specify payload values
    if (strcmp(component, "binary_sensor") == 0 && payloadOn && payloadOff) {
        payload << ",\"payload_on\":\"" << payloadOn << "\","
                << "\"payload_off\":\"" << payloadOff << "\"";
    }
    
    // Add optional fields only if non-empty
    if (deviceClass[0] != '\0') {
        payload << ",\"device_class\":\"" << deviceClass << "\"";
    }
    if (unit[0] != '\0') {
        payload << ",\"unit_of_measurement\":\"" << unit << "\"";
    }
    // State class only for numeric sensors
    if (stateClass[0] != '\0') {
        bool isNumericType = (ei->Type == et_dec_val || 
                              ei->Type == et_cent_val || 
                              ei->Type == et_mil_val || 
                              ei->Type == et_byte ||
                              ei->Type == et_double_val ||
                              ei->Type == et_triple_val ||
                              ei->Type == et_little_endian);
        if (isNumericType) {
            payload << ",\"state_class\":\"" << stateClass << "\"";
        }
    }
    if (icon[0] != '\0') {
        payload << ",\"icon\":\"" << icon << "\"";
    }
    
    // Device info - create individual device per CAN member as sub-device
    // Main device ID for the heat pump
    const char* mainDeviceId = "stiebel_eltron_" HA_DEVICE_MODEL_STR;
    
    // Create unique device ID for this CAN member
    char canMemberDeviceId[64];
    snprintf(canMemberDeviceId, sizeof(canMemberDeviceId), "stiebel_%s", cm.Name);
    
    // Convert CAN member name to friendly German name
    const char* canMemberFriendlyName = cm.Name;
    if (strcmp(cm.Name, "KESSEL") == 0) canMemberFriendlyName = "Kessel";
    else if (strcmp(cm.Name, "MANAGER") == 0) canMemberFriendlyName = "Manager";
    else if (strcmp(cm.Name, "HEIZMODUL") == 0) canMemberFriendlyName = "Heizmodul";
    else if (strcmp(cm.Name, "FEHLERSPEICHER") == 0) canMemberFriendlyName = "Fehlerspeicher";
    else if (strcmp(cm.Name, "MIXER1") == 0) canMemberFriendlyName = "Mischer 1";
    else if (strcmp(cm.Name, "MIXER2") == 0) canMemberFriendlyName = "Mischer 2";
    else if (strcmp(cm.Name, "WMZ1") == 0) canMemberFriendlyName = "Wärmemengenzähler 1";
    else if (strcmp(cm.Name, "WMZ2") == 0) canMemberFriendlyName = "Wärmemengenzähler 2";
    
    payload << ",\"device\":{\"identifiers\":[\"" << canMemberDeviceId << "\"],"
            << "\"name\":\"" << canMemberFriendlyName << "\","
            << "\"via_device\":\"" << mainDeviceId << "\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    // Publish discovery message with retain flag
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    
    ESP_LOGI("MQTT", "Discovery published for %s", friendlyName);
}

// Republish all MQTT discoveries (for periodic refresh)
void republishAllDiscoveries() {
    ESP_LOGI("MQTT", "Republishing all MQTT discoveries (%d signals)", discoveredSignals.size());
    
    // Create a copy of discovered signals to iterate over, filtering out blacklisted signals
    std::set<std::string> signalsToRepublish;
    int blacklistedCount = 0;
    
    for (const auto& uid : discoveredSignals) {
        // Extract signal name from UID (format: "can_id_signalName")
        // Find last underscore to get signal name
        size_t lastUnderscore = uid.find_last_of('_');
        if (lastUnderscore != std::string::npos) {
            std::string signalName = uid.substr(lastUnderscore + 1);
            
            // Skip blacklisted signals
            if (isPermanentlyBlacklisted(signalName.c_str())) {
                blacklistedCount++;
                ESP_LOGD("MQTT", "Skipping blacklisted signal during republish: %s", signalName.c_str());
                continue;
            }
        }
        
        signalsToRepublish.insert(uid);
    }
    
    if (blacklistedCount > 0) {
        ESP_LOGI("MQTT", "Filtered out %d blacklisted signals during republish", blacklistedCount);
    }
    
    // Clear the set and re-add only non-blacklisted signals
    discoveredSignals = signalsToRepublish;
    
    // Force republish by reading all known signals
    // This will trigger processAndUpdate which calls publishMqttDiscovery
    for (const auto& signal : signalsToRepublish) {
        ESP_LOGD("MQTT", "Marked for republish: %s", signal.c_str());
    }
    
    // Republish all calculated sensor discoveries using unified system
    publishAllCalculatedSensorDiscoveries(true);
    
    ESP_LOGI("MQTT", "Discovery refresh complete - will republish as signals are received");
}

// Publish signal state to MQTT
void publishMqttState(const CanMember &cm, const ElsterIndex *ei, const std::string &value) {
    // Input validation
    if (!ei || !ei->Name || value.empty()) {
        ESP_LOGW("MQTT", "Invalid signal data, skipping state publish");
        return;
    }
    
    // Build state topic with bounds checking
    char stateTopic[128];
    int written = snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    if (written >= sizeof(stateTopic)) {
        ESP_LOGW("MQTT", "Topic too long for %s/%s, truncated", cm.Name, ei->Name);
    }
    
    // Publish state with retain flag
    id(mqtt_client).publish(stateTopic, value.c_str(), value.length(), 0, true);
}

// Diagnostics removed for simplification

// Track which COP values we have valid data for
static std::unordered_map<std::string, float> copEnergyValues;

// Publish MQTT discovery for COP sensors
void publishCOPDiscovery() {
    static bool discoveryPublished = false;
    if (discoveryPublished) return;
    
    // COP WW
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/cop_ww/config";
        std::ostringstream payload;
        payload << "{\"name\":\"" LNAME_COP_WW "\","
                << "\"unique_id\":\"stiebel_cop_ww\","
                << "\"state_topic\":\"heatingpump/calculated/cop_ww/state\","
                << "\"icon\":\"mdi:water-boiler\","
                << "\"state_class\":\"measurement\","
                << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
        std::string payloadStr = payload.str();
        id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    }
    
    // COP Heizung
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/cop_heiz/config";
        std::ostringstream payload;
        payload << "{\"name\":\"" LNAME_COP_HEIZ "\","
                << "\"unique_id\":\"stiebel_cop_heiz\","
                << "\"state_topic\":\"heatingpump/calculated/cop_heiz/state\","
                << "\"icon\":\"mdi:radiator\","
                << "\"state_class\":\"measurement\","
                << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
        std::string payloadStr = payload.str();
        id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    }
    
    // COP Gesamt
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/cop_gesamt/config";
        std::ostringstream payload;
        payload << "{\"name\":\"" LNAME_COP_GESAMT "\","
                << "\"unique_id\":\"stiebel_cop_gesamt\","
                << "\"state_topic\":\"heatingpump/calculated/cop_gesamt/state\","
                << "\"icon\":\"mdi:chart-line\","
                << "\"state_class\":\"measurement\","
                << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
        std::string payloadStr = payload.str();
        id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    }
    
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for COP sensors");
}

// Store energy value when received for COP calculation
void storeCOPEnergyValue(const char* signalName, const std::string &value) {
    // Validate the string contains a valid number (no exceptions available)
    if (value.empty()) {
        ESP_LOGW("COP", "Empty value for %s", signalName);
        return;
    }
    
    // Check for valid numeric characters
    bool hasDigit = false;
    bool isValid = true;
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c >= '0' && c <= '9') {
            hasDigit = true;
        } else if (c != '.' && c != '-' && c != '+' && c != ' ') {
            isValid = false;
            break;
        }
    }
    
    if (!hasDigit || !isValid) {
        ESP_LOGW("COP", "Invalid numeric value for %s: %s", signalName, value.c_str());
        return;
    }
    
    // Parse the value (std::stof may still fail but won't throw in this context)
    float fval = std::atof(value.c_str());
    copEnergyValues[signalName] = fval;
    ESP_LOGD("COP", "Stored %s = %.3f", signalName, fval);
}

// Vorwaertsdeklaration: publiziert kombinierte Energie-Gesamtwerte (MWh*1000 + kWh)
void publishCombinedEnergySensors();

// Calculate and publish COP values if all required data is available
void updateCOPCalculations() {
    publishCOPDiscovery();
    
    // COP WW: (WAERMEERTRAG_WW_SUM + WAERMEERTRAG_2WE_WW_SUM) / EL_AUFNAHMELEISTUNG_WW_SUM
    if (copEnergyValues.find("WAERMEERTRAG_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_SUM_MWH") != copEnergyValues.end()) {
        
        float el_ww = copEnergyValues["EL_AUFNAHMELEISTUNG_WW_SUM_MWH"];
        if (el_ww > 0.001f) { // Avoid division by zero
            float waerme_ww = copEnergyValues["WAERMEERTRAG_WW_SUM_MWH"] + copEnergyValues["WAERMEERTRAG_2WE_WW_SUM_MWH"];
            float cop_ww = waerme_ww / el_ww;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_ww);
            const char* stateTopic = "heatingpump/calculated/cop_ww/state";
            id(mqtt_client).publish(stateTopic, valueStr, strlen(valueStr), 0, true);
            ESP_LOGI("COP", "COP WW: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_ww, waerme_ww, el_ww);
        }
    }
    
    // COP Heizung: (WAERMEERTRAG_HEIZ_SUM + WAERMEERTRAG_2WE_HEIZ_SUM) / EL_AUFNAHMELEISTUNG_HEIZ_SUM
    if (copEnergyValues.find("WAERMEERTRAG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH") != copEnergyValues.end()) {
        
        float el_heiz = copEnergyValues["EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH"];
        if (el_heiz > 0.001f) { // Avoid division by zero
            float waerme_heiz = copEnergyValues["WAERMEERTRAG_HEIZ_SUM_MWH"] + copEnergyValues["WAERMEERTRAG_2WE_HEIZ_SUM_MWH"];
            float cop_heiz = waerme_heiz / el_heiz;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_heiz);
            const char* stateTopic = "heatingpump/calculated/cop_heiz/state";
            id(mqtt_client).publish(stateTopic, valueStr, strlen(valueStr), 0, true);
            ESP_LOGI("COP", "COP Heizung: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_heiz, waerme_heiz, el_heiz);
        }
    }
    
    // COP Gesamt: (all WAERMEERTRAG) / (all EL_AUFNAHMELEISTUNG)
    if (copEnergyValues.find("WAERMEERTRAG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_SUM_MWH") != copEnergyValues.end()) {
        
        float el_total = copEnergyValues["EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH"] + copEnergyValues["EL_AUFNAHMELEISTUNG_WW_SUM_MWH"];
        if (el_total > 0.001f) { // Avoid division by zero
            float waerme_total = copEnergyValues["WAERMEERTRAG_HEIZ_SUM_MWH"] + copEnergyValues["WAERMEERTRAG_2WE_HEIZ_SUM_MWH"] +
                               copEnergyValues["WAERMEERTRAG_WW_SUM_MWH"] + copEnergyValues["WAERMEERTRAG_2WE_WW_SUM_MWH"];
            float cop_gesamt = waerme_total / el_total;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_gesamt);
            const char* stateTopic = "heatingpump/calculated/cop_gesamt/state";
            id(mqtt_client).publish(stateTopic, valueStr, strlen(valueStr), 0, true);
            ESP_LOGI("COP", "COP Gesamt: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_gesamt, waerme_total, el_total);
        }
    }

    // === KOMBINIERTE ENERGIE-GESAMTWERTE (MWh*1000 + kWh) ===
    // Zusaetzliche Sensoren; die Einzelwerte (MWh/kWh) bleiben fuer Kompatibilitaet.
    publishCombinedEnergySensors();
}

// ============================================================================
// DEGLITCH FUER KOMBINIERTE ENERGIE-GESAMTWERTE
// ============================================================================
// Die Gesamtwerte werden als MWh*1000 + kWh berechnet. Die Einzelregister liegen
// in einem nur nach Signalnamen geschluesselten Cache, daher koennen MWh und kWh
// kurzzeitig von unterschiedlichen CAN-Membern stammen (oder einen ungueltigen
// Marker wie 0x8000 -> 32768000 tragen). Das erzeugt transiente Spitzen von ca.
// +/-1000 kWh (nachts, wenn das Register aktualisiert/umlaeuft) sowie kleinere.
// Echte Energiezaehler wachsen nur, deshalb:
//   - kleine, nicht sinkende Schritte werden sofort akzeptiert,
//   - jede Verkleinerung oder ein Sprung > MAX_SMALL_STEP gilt als verdaechtig;
//     der letzte gute Wert wird weiter gesendet, bis der neue Wert CONFIRM_MS
//     lang stabil bleibt (ein Glitch verschwindet lange vorher).
static bool sanitizeEnergyTotal(const char* key, float raw, float& out) {
    static std::unordered_map<std::string, float> stableValue;    // zuletzt akzeptierter Wert
    static std::unordered_map<std::string, float> pendingValue;   // verdaechtiger Kandidat
    static std::unordered_map<std::string, unsigned long> pendingSince;

    const float TOL_EPS = 0.5f;                 // Toleranz "gleicher Wert"
    const float MAX_SMALL_STEP = 20.0f;         // normales Wachstum (kWh), sofort ok
    const float TOL = 25.0f;                    // Naehe eines Kandidaten zu seiner Region
    const unsigned long CONFIRM_MS = 900000UL;  // 15 min stabile Bestaetigung

    // Klar ungueltige Marker (0x8000 -> 32768000, -255, ...) verwerfen.
    // Rueckgabe = "ein publizierbarer Wert liegt in out vor".
    if (raw <= -1000.0f || raw >= 1000000.0f) {
        auto s = stableValue.find(key);
        if (s != stableValue.end()) { out = s->second; return true; }
        return false;   // noch kein stabiler Wert -> nicht publizieren
    }

    auto it = stableValue.find(key);
    if (it == stableValue.end()) {              // ersten Wert uebernehmen
        stableValue[key] = raw;
        out = raw;
        return true;
    }
    const float last = it->second;

    // Normales, nicht sinkendes Wachstum -> sofort akzeptieren
    if (raw >= last - TOL_EPS && raw <= last + MAX_SMALL_STEP) {
        stableValue[key] = raw;
        pendingValue.erase(key);
        pendingSince.erase(key);
        out = raw;
        return true;
    }

    // Verdaechtig: Verkleinerung oder grosser Sprung -> erst nach Stablezeit uebernehmen
    auto p = pendingValue.find(key);
    float diff = (p != pendingValue.end()) ? (p->second - raw) : 0.0f;
    if (diff < 0.0f) diff = -diff;
    if (p == pendingValue.end() || diff > TOL) {
        pendingValue[key] = raw;
        pendingSince[key] = millis();
    } else if ((millis() - pendingSince[key]) >= CONFIRM_MS) {
        stableValue[key] = raw;
        pendingValue.erase(key);
        pendingSince.erase(key);
        out = raw;
        return true;
    }

    out = last;   // letzten guten Wert weiter senden
    return true;
}

// ============================================================================
// GLEITENDER ENERGIE-GESAMTWERT (NVS-Sockel + Tagesanteil, ohne Rollover-Doppelzaehlung)
// ============================================================================
// Der Geraete-Gesamtzahler (SUM_MWH/SUM_KWH) aktualisiert sich nur einmal pro
// Nacht; die Tageszaehler (TAG_KWH/TAG_WH) laufen den Tag ueber. Der gleitende
// Gesamtwert = SUM_MWH*1000 + SUM_KWH + TAG_KWH + TAG_WH/1000 ist daher
// fortlaufend. Damit sich SUM und TAG nicht mit den gleichnamigen Registern
// anderer Member vermischen, werden die Energiewerte zusaetzlich member-scharf
// gespeichert und fuer den WPL17 ausschliesslich aus HEIZMODUL_WPL (0x514)
// gelesen.
static std::unordered_map<std::string, float> copEnergyByMember;   // "MEMBER|SIGNAL"

inline void storeCOPEnergyValueMember(const char* memberName, const char* signalName, const std::string &value) {
    if (value.empty()) return;
    bool hasDigit = false, isValid = true;
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c >= '0' && c <= '9') hasDigit = true;
        else if (c != '.' && c != '-' && c != '+' && c != ' ') { isValid = false; break; }
    }
    if (!hasDigit || !isValid) return;
    copEnergyByMember[std::string(memberName) + "|" + signalName] = std::atof(value.c_str());
}

inline bool getEnergyForMember(const char* memberName, const char* signalName, float& out) {
    auto it = copEnergyByMember.find(std::string(memberName) + "|" + signalName);
    if (it == copEnergyByMember.end()) return false;
    out = it->second;
    return true;
}

inline bool getSlidingTotalKwh(const char* memberName, const char* sumMwh, const char* sumKwh,
                               const char* tagKwh, const char* tagWh, float& out) {
    float sm, sk, tk, tw;
    if (getEnergyForMember(memberName, sumMwh, sm) && getEnergyForMember(memberName, sumKwh, sk) &&
        getEnergyForMember(memberName, tagKwh, tk) && getEnergyForMember(memberName, tagWh, tw)) {
        out = sm * 1000.0f + sk + tk + tw / 1000.0f;
        return true;
    }
    return false;
}

// NVS-persistierter SUM-Sockel: total = Sockel + Tagesanteil. Die WPL17 liefert
// die SUM-Register nur einmal nachts bzw. nicht auf Anforderung; der Sockel
// uebersteht daher Reboots und der Gesamtwert bleibt fortlaufend.
struct EnergyBaseStorage { uint32_t magic; float v[4]; };
static ESPPreferenceObject energyBasePref;
static EnergyBaseStorage energyBase = {0x454E4B42u, {0, 0, 0, 0}};
static bool energyBaseReady = false;

inline void ensureEnergyBase() {
    if (energyBaseReady) return;
    energyBaseReady = true;
    energyBasePref = global_preferences->make_preference<EnergyBaseStorage>(0x454E4B42u, true);
    EnergyBaseStorage tmp;
    if (energyBasePref.load(&tmp) && tmp.magic == 0x454E4B42u) energyBase = tmp;
}

// Zustand fuer den naechtlichen Rollover. SUM und TAG aktualisieren sich zeitlich
// versetzt: SUM springt ~23:49 auf den neuen Wert (der den abgelaufenen Tag bereits
// enthaelt), TAG wird erst ~23:51 auf 0 gesetzt. In diesem Fenster wuerde SUM + TAG
// den Vortag doppelt zaehlen ("Mitternachts-Peak"). Deshalb wird der beim SUM-Wechsel
// aktuelle Tagesanteil als "gefaltet" gemerkt und nicht erneut addiert; beim
// Tagesreset (TAG faellt auf ~0) ist er abgegolten.
static float energyTagLast[4]   = {0.0f, 0.0f, 0.0f, 0.0f};   // letzter Tagesanteil (kWh)
static float energyTagFolded[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // in SUM bereits enthaltener Anteil
static bool  energyTagValid[4]  = {false, false, false, false};

// slot: 0=EL Heizung, 1=EL Warmwasser, 2=Waerme Heizung, 3=Waerme Warmwasser
inline bool wplBaseTotal(int slot, const char* sumMwh, const char* sumKwh,
                         const char* tagKwh, const char* tagWh, float& out) {
    ensureEnergyBase();
    float sm, sk, tk, tw;

    // Tagesanteil (TAG_KWH + TAG_WH/1000) lesen. Er ist innerhalb eines Tages
    // monoton und faellt nur beim Tageswechsel (~23:51) auf 0 zurueck.
    bool haveTag = getEnergyForMember("HEIZMODUL_WPL", tagKwh, tk) &&
                   getEnergyForMember("HEIZMODUL_WPL", tagWh, tw);
    float tag = haveTag ? (tk + tw / 1000.0f) : 0.0f;

    // Tageswechsel erkannt -> der gefaltete Anteil ist abgegolten.
    // Wichtig: TAG_KWH und TAG_WH setzen NICHT exakt gleichzeitig zurueck. Im
    // Zwischenzyklus (ein Zaehler schon neu, der andere noch alt) faellt der
    // Tagesanteil nur um <1 kWh - das ist KEIN Tageswechsel. Ein echter Reset
    // faellt dagegen auf ~0 bzw. um den ganzen Tag. Sonst wuerde der noch alte
    // Zaehleranteil erneut addiert und erzeugt weiter einen Peak.
    const float TAG_RESET_NEAR = 0.5f;   // Tagesanteil praktisch bei 0
    const float TAG_RESET_DROP = 1.0f;   // oder Rueckgang um mehr als 1 kWh
    if (haveTag && energyTagValid[slot] &&
        (tag < TAG_RESET_NEAR || tag < energyTagLast[slot] - TAG_RESET_DROP)) {
        energyTagFolded[slot] = 0.0f;
    }
    if (haveTag) { energyTagLast[slot] = tag; energyTagValid[slot] = true; }

    // SUM-Sockel (MWh*1000 + kWh) lesen. Bei Aenderung (naechtlicher Rollover)
    // ist der aktuelle Tagesanteil in SUM bereits enthalten -> merken, damit er
    // nicht doppelt addiert wird.
    if (getEnergyForMember("HEIZMODUL_WPL", sumMwh, sm) && getEnergyForMember("HEIZMODUL_WPL", sumKwh, sk)) {
        float base = sm * 1000.0f + sk;
        if (base > 0.0f && base != energyBase.v[slot]) {   // nur bei Aenderung schreiben (NVS-Schonung)
            if (energyBase.v[slot] > 0.0f && haveTag) energyTagFolded[slot] = tag;
            energyBase.v[slot] = base;
            energyBasePref.save(&energyBase);
        }
    }
    if (energyBase.v[slot] <= 0.0f) return false;
    out = energyBase.v[slot];
    if (haveTag) {
        float add = tag - energyTagFolded[slot];
        out += (add > 0.0f) ? add : 0.0f;   // nie negativ -> kein Ruecksprung
    }
    return true;
}

// Publiziert kombinierte Gesamt-kWh-Werte aus den einzelnen MWh- und kWh-Registern.
// Formel: Gesamt_kWh = MWh_Reg * 1000 + kWh_Reg
// Die Einzelregister (MWh, kWh) werden weiterhin separat veroeffentlicht.
void publishCombinedEnergySensors() {
    // Gesamtwert = SUM (MWh*1000 + kWh) + heutiger Tagesanteil (kWh + Wh/1000),
    // gelesen vom Member HEIZMODUL_WPL; Fallback auf den Namen-gekeyten SUM
    // anderer Modelle (ohne WPL-Member).
    // Fuer WPL17 ausschliesslich den member-scharfen gleitenden Wert nutzen
    // (kein Legacy-Fallback), sonst wuerde der Deglitch den niedrigeren,
    // korrekten Startwert als "Verkleinerung" verwerfen.
    // Fuer WPL17 ausschliesslich den member-scharfen gleitenden Wert
    // (SUM_MWH*1000 + SUM_KWH + TAG_KWH + TAG_WH/1000) aus HEIZMODUL_WPL nutzen.
    // Damit ist der totale Zaehler fortlaufend (kein Sprung beim naechtlichen
    // Rollover). Die SUM-Register werden dazu mit FREQ_1MIN gelesen.
    const bool useMember = (strcmp(HA_DEVICE_MODEL_STR, "wpl17") == 0);
    auto publishTotal = [useMember](const CalculatedSensorConfig& cfg, const char* topic, const char* key, int slot,
                           const char* sumMwh, const char* sumKwh, const char* tagKwh, const char* tagWh) {
        float rawTotal; bool have = false;
        if (wplBaseTotal(slot, sumMwh, sumKwh, tagKwh, tagWh, rawTotal)) {
            have = true;   // WPL-Basis (NVS-Sockel + Tagesanteil), sobald SUM bekannt
        } else if (copEnergyValues.find(sumMwh) != copEnergyValues.end() &&
                   copEnergyValues.find(sumKwh) != copEnergyValues.end()) {
            rawTotal = copEnergyValues[sumMwh] * 1000.0f + copEnergyValues[sumKwh];
            have = true;   // Fallback: Geraete-SUM (naechtlich aktualisiert)
        }
        if (!have) return;
        float total;
        if (!sanitizeEnergyTotal(key, rawTotal, total)) return;   // kein Wert -> nicht senden
        char valueStr[32];
        snprintf(valueStr, sizeof(valueStr), "%.3f", total);
        publishCalculatedSensorDiscovery(cfg, false);
        id(mqtt_client).publish(topic, valueStr, strlen(valueStr), 0, true);
        ESP_LOGI("ENERGIE", "Gesamt (%s): %.3f kWh", key, total);
    };
    publishTotal(calculatedSensors[10], "heatingpump/calculated/energie_heiz_total_kwh/state", "combined_energie_heiz", 0,
                 "EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH",
                 "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH");
    publishTotal(calculatedSensors[11], "heatingpump/calculated/energie_ww_total_kwh/state", "combined_energie_ww", 1,
                 "EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "EL_AUFNAHMELEISTUNG_WW_SUM_KWH",
                 "EL_AUFNAHMELEISTUNG_WW_TAG_KWH", "EL_AUFNAHMELEISTUNG_WW_TAG_WH");
    publishTotal(calculatedSensors[12], "heatingpump/calculated/waerme_heiz_total_kwh/state", "combined_waerme_heiz", 2,
                 "WAERMEERTRAG_HEIZ_SUM_MWH", "WAERMEERTRAG_HEIZ_SUM_KWH",
                 "WAERMEERTRAG_HEIZ_TAG_KWH", "WAERMEERTRAG_HEIZ_TAG_WH");
    publishTotal(calculatedSensors[13], "heatingpump/calculated/waerme_ww_total_kwh/state", "combined_waerme_ww", 3,
                 "WAERMEERTRAG_WW_SUM_MWH", "WAERMEERTRAG_WW_SUM_KWH",
                 "WAERMEERTRAG_WW_TAG_KWH", "WAERMEERTRAG_WW_TAG_WH");

    // === AKTUELLE ELEKTRISCHE LEISTUNG (W) + ABGELEITETER BETRIEBSSTATUS ===
    // Beides aus den feinen Tageszaehlern EL_*_TAG_WH (1 Wh Aufloesung):
    //  - Leistung = (Wh_neu - Wh_alt) / (t_neu - t_alt) * 3600 -> W
    //  - Betriebsstatus: welcher der beiden getrennten Verbrauchszaehler steigt,
    //    verraet den Betrieb (so trennt die WP selbst Heizen und Warmwasser).
    //    Das WP_STATUS-Bitfeld des Geraets ist auf der WPL17 unbrauchbar (immer 0).
    if (copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_TAG_WH") != copEnergyValues.end()) {

        static bool init = false;
        static float lastHeizWh = 0.0f, lastWwWh = 0.0f;
        static float accHeizWh = 0.0f, accWwWh = 0.0f;
        static uint32_t windowStartMs = 0;
        static uint32_t lastStatusMs = 0;
        static const char* opState = "Aus";

        float heizWh = copEnergyValues["EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH"];
        float wwWh   = copEnergyValues["EL_AUFNAHMELEISTUNG_WW_TAG_WH"];
        uint32_t nowMs = millis();

        if (init) {
            float dH = heizWh - lastHeizWh; if (dH > 0.0f) accHeizWh += dH;
            float dW = wwWh   - lastWwWh;   if (dW > 0.0f) accWwWh   += dW;
        } else {
            windowStartMs = nowMs; lastStatusMs = nowMs;
        }
        lastHeizWh = heizWh; lastWwWh = wwWh; init = true;

        // Verdichter: 0.8 = steht, 2.4 = laeuft (Heizmodul)
        bool running = (lastVerdichterValue > 1.5f);

        // Betriebsstatus live (alle 30 s): laeuft/steht aus dem Verdichter,
        // Heizen/Warmwasser aus den Energie-Deltas der letzten Minuten.
        if (nowMs - lastStatusMs >= 30000UL) {
            const char* ns;
            float sum = accHeizWh + accWwWh;
            if (!running) {
                ns = "Aus";
            } else if (copEnergyValues.find("KUEHLZUSTAND") != copEnergyValues.end() &&
                       copEnergyValues["KUEHLZUSTAND"] > 0.5f) {
                ns = "Kuehlen";
            } else if (sum > 0.0f && accWwWh > accHeizWh) {
                ns = "Warmwasser";
            } else if (sum > 0.0f) {
                ns = "Heizen";
            } else {
                ns = (strcmp(opState, "Warmwasser") == 0) ? "Warmwasser" : "Heizen";
            }
            opState = ns;
            publishCalculatedSensorDiscovery(calculatedSensors[15], false);
            id(mqtt_client).publish("heatingpump/calculated/betriebsstatus/state", opState, strlen(opState), 0, true);
            lastStatusMs = nowMs;
        }

        // Leistung als 5-Minuten-Mittel (die TAG-Zaehler springen grob)
        if (nowMs - windowStartMs >= 300000UL) {
            float dtSec = (nowMs - windowStartMs) / 1000.0f;
            float pHeiz = running ? accHeizWh / dtSec * 3600.0f : 0.0f;
            float pWw   = running ? accWwWh   / dtSec * 3600.0f : 0.0f;
            char v[16], ph[16], pw[16];
            snprintf(v, sizeof(v), "%.0f", pHeiz + pWw);
            snprintf(ph, sizeof(ph), "%.0f", pHeiz);
            snprintf(pw, sizeof(pw), "%.0f", pWw);
            publishCalculatedSensorDiscovery(calculatedSensors[14], false);
            id(mqtt_client).publish("heatingpump/calculated/el_leistung_w/state", v, strlen(v), 0, true);
            publishCalculatedSensorDiscovery(calculatedSensors[20], false);
            id(mqtt_client).publish("heatingpump/calculated/el_leistung_heiz_w/state", ph, strlen(ph), 0, true);
            publishCalculatedSensorDiscovery(calculatedSensors[21], false);
            id(mqtt_client).publish("heatingpump/calculated/el_leistung_ww_w/state", pw, strlen(pw), 0, true);
            ESP_LOGI("ENERGIE", "Leistung: %.0f W (Heiz %.0f, WW %.0f) in %.0fs", pHeiz + pWw, pHeiz, pWw, dtSec);
            accHeizWh = 0.0f; accWwWh = 0.0f; windowStartMs = nowMs;
        }
    }

    // === TAGES-ENERGIEWERTE: kWh-Anteil + Wh/1000 (Wh-Aufloesung) ===
    // Aktualisieren laufend und fallen ~Mitternacht auf 0 zurueck. HA behandelt
    // das bei state_class total_increasing korrekt (Reset = Zaehlerneustart) und
    // nutzt diese Werte als Basis fuer utility_meter (Tag/Monat/Jahr).
    {
        auto pubDaily = [](const char* kwhKey, const char* whKey, const CalculatedSensorConfig& cfg, const char* topic) {
            auto a = copEnergyValues.find(kwhKey);
            auto b = copEnergyValues.find(whKey);
            if (a == copEnergyValues.end() || b == copEnergyValues.end()) return;
            float daily = a->second + b->second / 1000.0f;
            char s[24];
            snprintf(s, sizeof(s), "%.3f", daily);
            publishCalculatedSensorDiscovery(cfg, false);
            id(mqtt_client).publish(topic, s, strlen(s), 0, true);
        };
        pubDaily("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH", calculatedSensors[16], "heatingpump/calculated/energie_heiz_tag_kwh/state");
        pubDaily("EL_AUFNAHMELEISTUNG_WW_TAG_KWH",   "EL_AUFNAHMELEISTUNG_WW_TAG_WH",   calculatedSensors[17], "heatingpump/calculated/energie_ww_tag_kwh/state");
        pubDaily("WAERMEERTRAG_HEIZ_TAG_KWH",        "WAERMEERTRAG_HEIZ_TAG_WH",        calculatedSensors[18], "heatingpump/calculated/waerme_heiz_tag_kwh/state");
        pubDaily("WAERMEERTRAG_WW_TAG_KWH",          "WAERMEERTRAG_WW_TAG_WH",          calculatedSensors[19], "heatingpump/calculated/waerme_ww_tag_kwh/state");
    }
}

// Validation removed for simplification - all values are published as-is
// Invalid values like -255, -32768, etc. will be visible in Home Assistant
// Users can filter these in HA automations/dashboards if needed

// ============================================================================
// ESPHOME IO ADAPTER FOR SG READY CONTROLLER
// ============================================================================

class EspHomeSgReadyIO : public ISgReadyIO {
public:
    static ESPPreferenceObject nvsDhw;
    static ESPPreferenceObject nvsRoom;
    static ESPPreferenceObject nvsActive;

    void writeCanSignal(const char* signalName, const char* value) override {
        writeSignal(&CanMembers[cm_manager], signalName, value);
        delay(100);
        readSignal(&CanMembers[cm_manager], signalName);
    }

    void publishMqtt(const char* topic, const char* value) override {
        id(mqtt_client).publish(topic, value, strlen(value), 0, true);
    }

    void saveBoostState(float dhw, float room, bool active) override {
        nvsDhw.save(&dhw);
        nvsRoom.save(&room);
        nvsActive.save(&active);
        ESP_LOGD("NVS", "Saved boost state: DHW=%.1f, Room=%.1f, active=%d",
                 dhw, room, (int)active);
    }

    bool loadBoostState(float& dhw, float& room, bool& active) override {
        bool ok = nvsActive.load(&active);
        nvsDhw.load(&dhw);
        nvsRoom.load(&room);
        return ok;
    }
};

ESPPreferenceObject EspHomeSgReadyIO::nvsDhw;
ESPPreferenceObject EspHomeSgReadyIO::nvsRoom;
ESPPreferenceObject EspHomeSgReadyIO::nvsActive;

static EspHomeSgReadyIO sgReadyIO;
static SgReadyController sgReadyController(sgReadyIO);

// Called from on_boot lambda in common.yaml
void initBoostStatePrefs() {
    EspHomeSgReadyIO::nvsDhw    = global_preferences->make_preference<float>(0xB007BA5E, true);
    EspHomeSgReadyIO::nvsRoom   = global_preferences->make_preference<float>(0xB007BA5F, true);
    EspHomeSgReadyIO::nvsActive = global_preferences->make_preference<bool> (0xB007BA60, true);
    ESP_LOGI("NVS", "Boost state preferences initialized");
}

// Called from on_boot lambda in common.yaml
void loadBoostState() {
    sgReadyController.loadFromNvs();
    currentSgReadyState = sgReadyController.currentState();
    ESP_LOGI("NVS", "Boost state loaded, active=%d", (int)sgReadyController.isActive());
}

// Called from MQTT on_message lambda in common.yaml
void applySgReadyState(int state) {
    ESP_LOGI("SG_READY", "Applying SG Ready state %d", state);
    if (sgReadyController.applyState(state)) {
        currentSgReadyState = sgReadyController.currentState();
    } else {
        ESP_LOGW("SG_READY", "Invalid SG Ready state: %d", state);
    }
}

// Accessor helpers used by YAML sensor lambdas and MQTT on_connect block
inline float sgReadyBoostState3Value() { return sgReadyController.boostState3(); }
inline float sgReadyBoostState4Value() { return sgReadyController.boostState4(); }
inline void  setSgReadyBoost3(float v) { sgReadyController.setBoostState3(v); }
inline void  setSgReadyBoost4(float v) { sgReadyController.setBoostState4(v); }

// Publishes the current SG Ready state and boost values (retained). Called on
// boot, on MQTT connect and periodically, so the retained values survive a
// broker restart instead of being published only once on connect.
inline void publishSgReadyState() {
#ifdef HA_DISABLE_SG_READY_true
    return;
#endif
    int idx = currentSgReadyState - 1;
    if (idx < 0 || idx > 3) idx = 1;  // fall back to "2 - Normal"
    const char* s = sgReadyOptions[idx];
    id(mqtt_client).publish("heatingpump/MANAGER/SG_READY_STATE/state", s, strlen(s), 0, true);
    char b3[16], b4[16];
    snprintf(b3, sizeof(b3), "%.1f", sgReadyBoostState3Value());
    snprintf(b4, sizeof(b4), "%.1f", sgReadyBoostState4Value());
    id(mqtt_client).publish("heatingpump/MANAGER/SG_READY_BOOST_STATE3/state", b3, strlen(b3), 0, true);
    id(mqtt_client).publish("heatingpump/MANAGER/SG_READY_BOOST_STATE4/state", b4, strlen(b4), 0, true);
}

// ============================================================================
// COMPILE-TIME STRING HASHING FOR FAST SIGNAL DISPATCH
// ============================================================================

// Compile-time DJB2 hash function (constexpr for C++11 compatibility)
constexpr uint32_t hash_impl(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : hash_impl(str + 1, ((hash << 5) + hash) + static_cast<uint32_t>(*str));
}

constexpr uint32_t hash(const char* str) {
    return hash_impl(str);
}

// Runtime hash for signal names (inline for performance)
inline uint32_t hash_runtime(const char* str) {
    uint32_t h = 5381;
    while (*str) {
        h = ((h << 5) + h) + static_cast<uint32_t>(*str);
        str++;
    }
    return h;
}

// Pre-computed compile-time hashes for all monitored signals
constexpr uint32_t HASH_JAHR = hash("JAHR");
constexpr uint32_t HASH_MONAT = hash("MONAT");
constexpr uint32_t HASH_TAG = hash("TAG");
constexpr uint32_t HASH_STUNDE = hash("STUNDE");
constexpr uint32_t HASH_MINUTE = hash("MINUTE");
constexpr uint32_t HASH_SEKUNDE = hash("SEKUNDE");
constexpr uint32_t HASH_SOMMERBETRIEB = hash("SOMMERBETRIEB");
constexpr uint32_t HASH_WPVORLAUFIST = hash("WPVORLAUFIST");
constexpr uint32_t HASH_RUECKLAUFISTTEMP = hash("RUECKLAUFISTTEMP");
constexpr uint32_t HASH_VERDICHTER = hash("VERDICHTER");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ = hash("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW = hash("EL_AUFNAHMELEISTUNG_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_2WE_WW = hash("WAERMEERTRAG_2WE_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_2WE_HEIZ = hash("WAERMEERTRAG_2WE_HEIZ_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW = hash("WAERMEERTRAG_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ = hash("WAERMEERTRAG_HEIZ_SUM_MWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ_KWH = hash("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW_KWH = hash("EL_AUFNAHMELEISTUNG_WW_SUM_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ_KWH = hash("WAERMEERTRAG_HEIZ_SUM_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW_KWH = hash("WAERMEERTRAG_WW_SUM_KWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH = hash("EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW_TAG_WH = hash("EL_AUFNAHMELEISTUNG_WW_TAG_WH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH = hash("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW_TAG_KWH = hash("EL_AUFNAHMELEISTUNG_WW_TAG_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ_TAG_WH = hash("WAERMEERTRAG_HEIZ_TAG_WH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ_TAG_KWH = hash("WAERMEERTRAG_HEIZ_TAG_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW_TAG_WH = hash("WAERMEERTRAG_WW_TAG_WH");
constexpr uint32_t HASH_WAERMEERTRAG_WW_TAG_KWH = hash("WAERMEERTRAG_WW_TAG_KWH");



void updateSensor(const CanMember &cm, const ElsterIndex *ei, const std::string &value)
{
    // Skip empty values
    if (value.empty()) {
        return;
    }
    
    // Invert boolean logic for EVU_SPERRE_AKTIV signal
    // CAN: 1 = lock inactive (off), 0 = lock active (on)
    // MQTT: "off" = lock inactive, "on" = lock active
    std::string publishValue = value;
    if (strcmp(ei->Name, "EVU_SPERRE_AKTIV") == 0) {
        if (value == "on") {
            publishValue = "off";
        } else if (value == "off") {
            publishValue = "on";
        }
    }
    
    // Check if discovery is needed
    std::string uid = getOrCreateUID(cm, ei->Name);
    bool needsDiscovery = (discoveredSignals.find(uid) == discoveredSignals.end());
    
    if (needsDiscovery) {
        // Mark as discovered and publish discovery immediately
        discoveredSignals.insert(uid);
        publishMqttDiscovery(cm, ei);
    }
    
    // Publish state
    publishMqttState(cm, ei, publishValue);
    
    // Fast signal dispatch using compile-time hash (O(1) switch/jump table)
    const char* signalName = ei->Name;
    uint32_t signalHash = hash_runtime(signalName);
    
    switch (signalHash) {
        case HASH_JAHR: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastJahr = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated JAHR: %d", lastJahr);
            }
            break;
        }
        
        case HASH_MONAT: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastMonat = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated MONAT: %d", lastMonat);
            }
            break;
        }
        
        case HASH_TAG: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastTag = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated TAG: %d", lastTag);
                publishDate();
            }
            break;
        }
        
        case HASH_STUNDE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastStunde = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated STUNDE: %d", lastStunde);
            }
            break;
        }
        
        case HASH_MINUTE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastMinute = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated MINUTE: %d", lastMinute);
                publishTime();
            }
            break;
        }
        
        case HASH_SEKUNDE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastSekunde = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated SEKUNDE: %d", lastSekunde);
            }
            break;
        }
        
        case HASH_SOMMERBETRIEB:
            publishBetriebsart(value);
            break;
        
        case HASH_WPVORLAUFIST: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastWpVorlaufIst = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse WPVORLAUFIST value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_RUECKLAUFISTTEMP: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastRuecklaufIstTemp = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse RUECKLAUFISTTEMP value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_VERDICHTER: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastVerdichterValue = floatValue;
                // Compressor state will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse VERDICHTER value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ:
        case HASH_EL_AUFNAHMELEISTUNG_WW:
        case HASH_WAERMEERTRAG_2WE_WW:
        case HASH_WAERMEERTRAG_2WE_HEIZ:
        case HASH_WAERMEERTRAG_WW:
        case HASH_WAERMEERTRAG_HEIZ:
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ_KWH:
        case HASH_EL_AUFNAHMELEISTUNG_WW_KWH:
        case HASH_WAERMEERTRAG_HEIZ_KWH:
        case HASH_WAERMEERTRAG_WW_KWH:
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH:
        case HASH_EL_AUFNAHMELEISTUNG_WW_TAG_WH:
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH:
        case HASH_EL_AUFNAHMELEISTUNG_WW_TAG_KWH:
        case HASH_WAERMEERTRAG_HEIZ_TAG_WH:
        case HASH_WAERMEERTRAG_HEIZ_TAG_KWH:
        case HASH_WAERMEERTRAG_WW_TAG_WH:
        case HASH_WAERMEERTRAG_WW_TAG_KWH:
            storeCOPEnergyValue(signalName, value);
            storeCOPEnergyValueMember(cm.Name, signalName, value);
            updateCOPCalculations();
            break;
        
        default:
            // Signal not monitored for calculated sensors - no action needed
            break;
    }
}

// Helper function to generate random number in range [min, max) using ESP32 hardware RNG
// esp_random() uses hardware entropy sources and doesn't need seeding
unsigned long getRandomInRange(unsigned long min, unsigned long max) {
    if (min >= max) return min;
    unsigned long range = max - min;
    return min + (esp_random() % range);
}

void publishCanDiagnostics() {
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) return;

    if (status.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGW("CAN_RECOVERY", "TWAI bus-off detected, initiating recovery");
        twai_initiate_recovery();
    } else if (status.state == TWAI_STATE_STOPPED) {
        // twai_initiate_recovery() leaves the controller STOPPED once recovery
        // completes (128 bus-free occurrences) — must explicitly restart.
        // STOPPED here always means "post-recovery": the boot-time STOPPED
        // window (before twai_start_v2() in setup_internal()) completes
        // long before this 30s-cadence diagnostic loop ever runs.
        ESP_LOGW("CAN_RECOVERY", "TWAI stopped after recovery, restarting");
        twai_start();
    }

    // Publish discovery (only once per UID — cached internally)
    publishCalculatedSensorDiscovery(calculatedSensors[6]);
    publishCalculatedSensorDiscovery(calculatedSensors[7]);
    publishCalculatedSensorDiscovery(calculatedSensors[8]);
    publishCalculatedSensorDiscovery(calculatedSensors[9]);

    char buf[16];

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.tx_error_counter);
    id(mqtt_client).publish("heatingpump/calculated/can_tec/state", buf, strlen(buf), 0, true);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.rx_error_counter);
    id(mqtt_client).publish("heatingpump/calculated/can_rec/state", buf, strlen(buf), 0, true);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.bus_error_count);
    id(mqtt_client).publish("heatingpump/calculated/can_bus_errors/state", buf, strlen(buf), 0, true);

    const char* stateStr;
    switch (status.state) {
        case TWAI_STATE_STOPPED:    stateStr = "Gestoppt";  break;
        case TWAI_STATE_RUNNING:    stateStr = "Läuft";     break;
        case TWAI_STATE_BUS_OFF:    stateStr = "Bus-Off";   break;
        case TWAI_STATE_RECOVERING: stateStr = "Erholung";  break;
        default:                    stateStr = "Unbekannt"; break;
    }
    id(mqtt_client).publish("heatingpump/calculated/can_state/state",
                            stateStr, strlen(stateStr), 0, true);
}

// Process calculated sensor updates with frequency-based scheduling
// This function should be called regularly from the main loop
void processCalculatedSensors() {
    unsigned long now = millis();
    
    // Initialize scheduled update times on first run (with random offsets to spread load)
    static bool initialized = false;
    if (!initialized && requestManagerStarted) {
        // Initialize all calculated sensor schedules with random offsets
        ESP_LOGI("CALC_SCHED", "Initializing calculated sensor schedules with random offsets");
        
        // Delta T sensors (30s frequency)
        unsigned long deltaT_interval = CALC_DELTA_T_FREQUENCY * 1000UL;
        nextDeltaTUpdate = now + getRandomInRange(0, deltaT_interval + 1);
        
        // Compressor sensor (30s frequency)
        unsigned long compressor_interval = CALC_COMPRESSOR_FREQUENCY * 1000UL;
        nextCompressorUpdate = now + getRandomInRange(0, compressor_interval + 1);
        
        // Date/Time sensors (1min frequency)
        unsigned long datetime_interval = CALC_DATETIME_FREQUENCY * 1000UL;
        nextDateTimeUpdate = now + getRandomInRange(0, datetime_interval + 1);
        
        // Betriebsart sensor (1min frequency)
        unsigned long betriebsart_interval = CALC_BETRIEBSART_FREQUENCY * 1000UL;
        nextBetriebsartUpdate = now + getRandomInRange(0, betriebsart_interval + 1);

        // CAN diagnostics (30s frequency)
        unsigned long can_diag_interval = CALC_CAN_DIAG_FREQUENCY * 1000UL;
        nextCanDiagUpdate = now + getRandomInRange(0, can_diag_interval + 1);

        initialized = true;
        ESP_LOGI("CALC_SCHED", "Calculated sensor scheduler initialized");
    }
    
    if (!initialized) return; // Wait for request manager to start
    
    // Check and publish Delta T sensors
    if (now >= nextDeltaTUpdate) {
        publishDeltaTContinuous();
        publishDeltaTRunning();
        nextDeltaTUpdate = now + (CALC_DELTA_T_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
    
    // Check and publish Compressor sensor
    if (now >= nextCompressorUpdate) {
        publishCompressorActive();
        nextCompressorUpdate = now + (CALC_COMPRESSOR_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
    
    // Check and publish Date/Time sensors
    if (now >= nextDateTimeUpdate) {
        publishDate();
        publishTime();
        nextDateTimeUpdate = now + (CALC_DATETIME_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
    
    // Check and publish Betriebsart sensor (only if SOMMERBETRIEB value is available)
    if (now >= nextBetriebsartUpdate) {
        // Betriebsart requires a value parameter, so we skip auto-publishing
        // It will be published when SOMMERBETRIEB signal is received
        nextBetriebsartUpdate = now + (CALC_BETRIEBSART_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }

    // Check and publish CAN diagnostic sensors
    if (now >= nextCanDiagUpdate) {
        publishCanDiagnostics();
        nextCanDiagUpdate = now + (CALC_CAN_DIAG_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
}

// Timeout tracking removed for simplification
// Signals that don't respond will simply not update in Home Assistant

// Process signal request table with frequency-based scheduling
void processSignalRequests() {
    unsigned long now = millis();
    
    // PASSIVE LISTEN MODE: keine Requests senden
    if (disableCanRequests) {
        return;
    }
    
    // Startup delay: wait before starting signal requests
    if (!requestManagerStarted) {
        if (requestManagerStartTime == 0) {
            requestManagerStartTime = now;
            ESP_LOGI("REQUEST_MGR", "Starting signal request manager (%ds startup delay)", STARTUP_DELAY_MS / 1000);
            return;
        }
        
        if (now - requestManagerStartTime < STARTUP_DELAY_MS) {
            return; // Still in startup delay
        }
        
        requestManagerStarted = true;
        ESP_LOGI("REQUEST_MGR", "Signal request manager active - processing %d signal definitions", 
                 SIGNAL_REQUEST_COUNT);
        
        // Initialize all signal schedules with random offsets to spread out initial load
        ESP_LOGI("REQUEST_MGR", "Initializing signal schedules with random offsets to prevent burst");
        for (int i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
            const SignalRequest& req = signalRequests[i];
            const ElsterIndex* ei = GetElsterIndex(req.signalName);
            if (!ei || ei->Index == 0xFFFF) continue;
            
            // Skip blacklisted signals during initialization
            if (isPermanentlyBlacklisted(ei->Name)) continue;
            
            unsigned long intervalMs = req.frequency * 1000UL;
            
            if (req.member == cm_other) {
                // Schedule for all members
                const CanMember* allMembers[] = {
                    &CanMembers[cm_kessel],
                    &CanMembers[cm_manager],
                    &CanMembers[cm_heizmodul]
                };
                for (size_t m = 0; m < 3; m++) {
                    std::string key = std::string(allMembers[m]->Name) + "_" + ei->Name;
                    // Random offset between 0 and full interval using ESP32 hardware RNG
                    unsigned long randomOffset = getRandomInRange(0, intervalMs + 1);
                    nextRequestTime[key] = now + randomOffset;
                }
            } else {
                // Schedule for specific member
                const CanMember* member = &CanMembers[req.member];
                std::string key = std::string(member->Name) + "_" + ei->Name;
                // Random offset between 0 and full interval using ESP32 hardware RNG
                unsigned long randomOffset = getRandomInRange(0, intervalMs + 1);
                nextRequestTime[key] = now + randomOffset;
            }
        }
        ESP_LOGI("REQUEST_MGR", "Initialized %d signal schedules", nextRequestTime.size());
    }
    
    // Rate limiting: send up to MAX_REQUESTS_PER_ITERATION requests per iteration
    // Randomization in scheduling prevents bursts
    int requestsSentThisIteration = 0;
    
    // Round-robin processing: start from last position to prevent starvation of signals at end of array
    // Process all signals, wrapping around, until we hit rate limit or complete full cycle
    int processedCount = 0;
    int currentIndex = signalProcessingStartIndex;
    
    while (processedCount < SIGNAL_REQUEST_COUNT && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION) {
        const SignalRequest& req = signalRequests[currentIndex];
        processedCount++;
        
        const ElsterIndex* ei = GetElsterIndex(req.signalName);
        if (!ei || ei->Index == 0xFFFF) {
            // Move to next signal and continue
            currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
            continue; // Signal not found in table
        }
        
        // Skip blacklisted signals - don't request them on CAN bus
        if (isPermanentlyBlacklisted(ei->Name)) {
            currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
            continue; // Signal is blacklisted
        }
        
        unsigned long intervalMs = req.frequency * 1000UL;
        
        // Determine which members to request from
        if (req.member == cm_other) {
            // Request from all members (each tracked independently)
            const CanMember* allMembers[] = {
                &CanMembers[cm_kessel],
                &CanMembers[cm_manager],
                &CanMembers[cm_heizmodul]
            };
            
            // Track how many members we've sent to in this cm_other group to stagger them
            int sentInThisGroup = 0;
            
            for (const auto* member : allMembers) {
                // Don't break - just skip remaining members for this signal
                // The outer loop will continue with next signal
                if (requestsSentThisIteration >= MAX_REQUESTS_PER_ITERATION) {
                    break; // Hit rate limit for this iteration
                }
                
                // Use ei->Name (from ElsterTable) to match blacklist key format
                std::string key = std::string(member->Name) + "_" + ei->Name;
                
                // Get the next scheduled time for this signal
                unsigned long nextScheduled = nextRequestTime[key];
                
                // Check if this signal is overdue (current time >= scheduled time)
                if (now >= nextScheduled) {
                    // For cm_other: only send to ONE member per iteration to prevent bursts
                    // Other members will be checked in subsequent iterations
                    if (sentInThisGroup == 0) {
                        readSignal(member, ei);
                        requestsSentThisIteration++;
                        sentInThisGroup++;
                        
                        // Calculate next scheduled time with random offset (0 to 5% of interval)
                        // This keeps signals from synchronizing while staying close to target frequency
                        unsigned long maxJitter = (intervalMs / 20); // 5% of interval
                        if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
                        unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                        nextRequestTime[key] = now + intervalMs + randomDelay;
                        
                        // ESP_LOGD("REQUEST_MGR", "Sent %s, next in %lums", key.c_str(), intervalMs + randomDelay);
                    }
                    // Else: skip this member for now, will be checked next iteration
                }
            }
        } else {
            // Request from specific member
            const CanMember* member = &CanMembers[req.member];
            std::string key = std::string(member->Name) + "_" + ei->Name;
            
            // Get the next scheduled time for this signal
            unsigned long nextScheduled = nextRequestTime[key];
            
            // Check if this signal is overdue (current time >= scheduled time)
            if (now >= nextScheduled) {
                readSignal(member, ei);
                requestsSentThisIteration++;
                
                // Calculate next scheduled time with random offset (0 to 5% of interval)
                // This keeps signals from synchronizing while staying close to target frequency
                unsigned long maxJitter = (intervalMs / 20); // 5% of interval
                if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
                unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                nextRequestTime[key] = now + intervalMs + randomDelay;
                
                // ESP_LOGD("REQUEST_MGR", "Sent %s, next in %lums", key.c_str(), intervalMs + randomDelay);
            }
        }
        
        // Move to next signal in round-robin fashion (wrap around at end)
        currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
    }
}

void processAndUpdate(uint32_t can_id, std::vector<uint8_t> msg)
{
    std::string value;
    const CanMember *cm = nullptr;
    const ElsterIndex *ei = processCanMessage(msg, can_id, value, &cm);

    // Skip nodes that are not mapped in CanMembers and only broadcast
    // placeholder/unknown values (e.g. WPM3 display FES 0x100, mixer module 2
    // 0x601, redundant DHW duplicates). They fall back to the OTHER member.
    if (cm->CanId == 0x000)
    {
        return;
    }

    // Skip permanently blacklisted signals
    if (isPermanentlyBlacklisted(ei->Name))
    {
        return; // Reject before lookup, parsing, formatting, logging
    }

    updateSensor(*cm, ei, value);
    return;
}

void updateTime(CanMember cm, const char *str_time)
{
    ESP_LOGI("WRITE UHRZEIT VIA BUTTON", "%s", str_time);

    char stunde[3];
    char minute[3];
    char sekunde[3];

    strncpy(stunde, str_time, sizeof(stunde) - 1);
    stunde[2] = '\0';
    strncpy(minute, str_time + 3, sizeof(minute) - 1);
    minute[2] = '\0';
    strncpy(sekunde, str_time + 6, sizeof(sekunde) - 1);
    sekunde[2] = '\0';
    const char *cstunde = stunde;
    const char *cminute = minute;
    const char *csekunde = sekunde;
    ESP_LOGI("WRITE", "Stunde: %s, Minute: %s, Sekunde: %s", cstunde, cminute, csekunde);
    writeSignal(&cm, "STUNDE", cstunde);
    readSignal(&cm, "STUNDE");
    writeSignal(&cm, "MINUTE", cminute);
    readSignal(&cm, "MINUTE");
    writeSignal(&cm, "SEKUNDE", csekunde);
    readSignal(&cm, "SEKUNDE");
}

void updateDate(CanMember cm, const char *str_date)
{
    ESP_LOGI("WRITE DATUM VIA BUTTON", "%s", str_date);
    char year[3];
    char month[3];
    char day[3];

    strncpy(year, str_date + 2, sizeof(year) - 1);
    year[2] = '\0';
    strncpy(month, str_date + 5, sizeof(month) - 1);
    month[2] = '\0';
    strncpy(day, str_date + 8, sizeof(day) - 1);
    day[2] = '\0';

    const char *cyear = year;
    const char *cmonth = month;
    const char *cday = day;
    ESP_LOGI("WRITE", "Year: %s, Month: %s, Day: %s", cyear, cmonth, cday);
    writeSignal(&cm, "JAHR", cyear);
    readSignal(&cm, "JAHR");
    writeSignal(&cm, "MONAT", cmonth);
    readSignal(&cm, "MONAT");
    writeSignal(&cm, "TAG", cday);
    readSignal(&cm, "TAG");
}

#endif // !defined(HA_DUMMY_BUILD)

#endif // ha_stiebel_control_H


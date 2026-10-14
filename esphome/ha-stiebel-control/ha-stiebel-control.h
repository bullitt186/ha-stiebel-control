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

// Heap diagnostics for the periodic CONTAINERS log (native test build → 0).
#ifdef ESP_PLATFORM
extern "C" uint32_t esp_get_free_heap_size(void);
extern "C" uint32_t esp_get_minimum_free_heap_size(void);
#endif
inline uint32_t diagFreeHeap() {
#ifdef ESP_PLATFORM
    return esp_get_free_heap_size();
#else
    return 0;
#endif
}
inline uint32_t diagMinFreeHeap() {
#ifdef ESP_PLATFORM
    return esp_get_minimum_free_heap_size();
#else
    return 0;
#endif
}

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
        // WPM3 display/operating unit (CAN B): broadcasts the panel settings
        // (EINSTELL_SPEICHERSOLLTEMP / ..._SOLLTEMP2) and the display DHW
        // temperature. KESSEL/MANAGER do not answer reads for these registers,
        // so this node is the only source for them on the WPL 10 AC / WPM3.
        {"BEDIENMODUL_WPM3", 0x201},
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
        {"OTHER", 0x000}};

typedef enum
{
    // Die Reihenfolge muss mit CanMembers übereinstimmen!
    cm_kessel = 0,
    cm_bedienmodul_wpm3,
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
};

static const size_t CALCULATED_SENSOR_COUNT = sizeof(calculatedSensors) / sizeof(CalculatedSensorConfig);

// Named indices for calculatedSensors[] — keeps array-order programming safe
enum CalcSensorIndex {
    CALC_IDX_DATE = 0,
    CALC_IDX_TIME,
    CALC_IDX_BETRIEBSART,
    CALC_IDX_DELTA_T_CONTINUOUS,
    CALC_IDX_DELTA_T_RUNNING,
    CALC_IDX_COMPRESSOR_ACTIVE,
    CALC_IDX_CAN_TEC,
    CALC_IDX_CAN_REC,
    CALC_IDX_CAN_BUS_ERRORS,
    CALC_IDX_CAN_STATE,
    CALC_SENSOR_INDEX_COUNT
};
static_assert(CALC_SENSOR_INDEX_COUNT == CALCULATED_SENSOR_COUNT,
              "CalcSensorIndex enum is out of sync with calculatedSensors[]");

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
    bool onMainDevice = false;     // true = group the entity under the main device
};

static const WritableNumberConfig writableNumbers[] = {
    // Combined DHW (WW) normal setpoint — writes BOTH Einstell registers
    // (0x0013 + 0x0a06) to the display/operating unit (0x201), keeping them in
    // sync (the active setpoint follows the WPM3's internal mapping).
    {"WW_SOLL", LNAME_NUM_WW_SOLL,
     cm_manager, 20.0, 60.0, 1.0, "°C", "mdi:water-boiler", "temperature", true},

    // Room normal setpoint — writes RAUMSOLLTEMP_I (0x0005) to the manager.
    // Additional to the RAUMSOLLTEMP_I entity (same value, main device).
    {"RAUM_SOLL", LNAME_NUM_RAUM_SOLL,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature", true},

    // SG Ready DHW boost temperatures (not real CAN signals, handled internally)
    {"SG_READY_BOOST_STATE3", LNAME_NUM_SG_READY_BOOST3,
     cm_manager, 0.0, 10.0, 0.5, "°C", "mdi:thermometer-plus", "temperature"},

    {"SG_READY_BOOST_STATE4", LNAME_NUM_SG_READY_BOOST4,
     cm_manager, 0.0, 15.0, 0.5, "°C", "mdi:fire", "temperature"},

    // SG Ready room boost offsets (not real CAN signals, handled internally)
    {"SG_READY_ROOM_BOOST_STATE3", LNAME_NUM_SG_READY_ROOM_BOOST3,
     cm_manager, 0.0, 5.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"SG_READY_ROOM_BOOST_STATE4", LNAME_NUM_SG_READY_ROOM_BOOST4,
     cm_manager, 0.0, 5.0, 0.5, "°C", "mdi:fire", "temperature"},

    // Room temperature setpoints for heating circuits 1–3 and night/reduced mode
    {"RAUMSOLLTEMP_I", LNAME_NUM_RAUMSOLLTEMP_I,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_II", LNAME_NUM_RAUMSOLLTEMP_II,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_III", LNAME_NUM_RAUMSOLLTEMP_III,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_NACHT", LNAME_NUM_RAUMSOLLTEMP_NACHT,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer-outline", "temperature"}
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
    // Heat pump operating mode
    {"PROGRAMMSCHALTER", LNAME_SEL_PROGRAMMSCHALTER,
     cm_manager, programmschalterOptions, 6, "mdi:dip-switch"},
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

// ── Prebuilt poll schedule ───────────────────────────────────────────────────
// One slot per (requested signal x concrete member), built once at startup so
// the request loop is a plain indexed walk: no string-key hashing, no per-iteration
// heap allocations. For cm_other signals the 3 fan-out slots sit contiguously and
// share a groupId, preserving "only one group member sent per iteration".
struct PollSlot {
    const CanMember* member;   // concrete poll target
    const ElsterIndex* signal; // resolved from ElsterTable (never 0xFFFF/blacklisted)
    unsigned long intervalMs;
    unsigned long nextAt;      // runtime: next poll time
    int groupId;               // >0 shared by cm_other fan-out slots; 0 = independent
};
static std::vector<PollSlot> pollSlots;

// Track sensor values for calculated sensors
static float lastWpVorlaufIst = NAN;
static float lastRuecklaufIstTemp = NAN;
static float lastVerdichterValue = NAN;

// Current setpoints from the device — captured by the SG Ready boost so a
// boost/restore cycle returns to the ACTUAL values (not hardcoded defaults).
// Room setpoint: MANAGER poll (30s); active DHW setpoint: KESSEL poll (10min).
static float lastSpeicherSollTemp = NAN;
static float lastRaumSollTemp = NAN;
// DHW panel setting (broadcast by BEDIENMODUL_WPM3) — mirrored into WW_SOLL.
static float lastEinstellSollTemp = NAN;

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

// UID cache keyed by (CAN id, signal-name pointer). The name pointers are stable
// (string literals in the static ElsterTable), so lookups allocate nothing.
struct UidKey {
    uint32_t canId;
    const char* signal;
    bool operator==(const UidKey &o) const { return canId == o.canId && signal == o.signal; }
};
struct UidKeyHash {
    size_t operator()(const UidKey &k) const {
        return (size_t)k.canId ^ ((size_t)k.signal << 1);
    }
};
static std::unordered_map<UidKey, std::string, UidKeyHash> uidCache;

// Hard cap on distinct published signals. The WPM3 broadcasts many service
// registers over time (TEST_OBJEKT, ESTRICH_*, BIVALENZ*, …); each would
// otherwise add a permanent uid + HA entity and grow the heap in steps.
// Known signals keep updating; NEW ones are ignored once the cap is reached.
static const size_t MAX_PUBLISHED_SIGNALS = 300;

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

// ── Runtime container capacity hints ────────────────────────────────────────
// reserve() only pre-allocates buckets to avoid rehashing; it never limits
// growth. Some values are ESTIMATES (comment) — verify against the "CONTAINERS"
// size log (every 10 min) and the Free Heap sensor.
static const size_t UID_CACHE_RESERVE        = 128;                                 // ESTIMATE: responding signals × mapped members
static const size_t REQUEST_SCHEDULE_RESERVE = SIGNAL_REQUEST_COUNT * 3;            // request table upper bound (cm_other polls up to 3 members)
static const size_t COP_VALUES_RESERVE       = 16;                                  // ESTIMATE: small fixed set of energy-sum keys

// Runtime state for signal request manager
static bool requestManagerStarted = false;
static unsigned long requestManagerStartTime = 0;

// ============================================================================
// CAN BUS HELPER FUNCTIONS
// ============================================================================

// Pure format validators (no ESPHome dependencies — compiled in tests too).
inline bool isDigitChar(char c) { return c >= '0' && c <= '9'; }

// Strict format validation so updateTime/updateDate never do partial writes
// (e.g. only "STUNDE" when the HA text sensor delivered a truncated payload).
// Accepted: "HH:MM:SS" (8 chars) / "YYYY-MM-DD" (10 chars).
inline bool isValidTimeString(const char* s)
{
    if (!s || strlen(s) != 8) return false;
    if (s[2] != ':' || s[5] != ':') return false;
    for (int i = 0; i < 8; i++) {
        if (i == 2 || i == 5) continue;
        if (!isDigitChar(s[i])) return false;
    }
    return (s[0]-'0')*10 + (s[1]-'0') < 24
        && (s[3]-'0')*10 + (s[4]-'0') < 60
        && (s[6]-'0')*10 + (s[7]-'0') < 60;
}

inline bool isValidDateString(const char* s)
{
    if (!s || strlen(s) != 10) return false;
    if (s[4] != '-' || s[7] != '-') return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isDigitChar(s[i])) return false;
    }
    return true;
}

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

// Explicit fallback node for CAN IDs not in the member table. Using a dedicated
// constant (instead of "the last array element") keeps the fallback independent
// of the CanMembers[] ordering.
static const CanMember UNKNOWN_NODE = {"OTHER", 0x0000};

const CanMember &lookupCanMember(uint32_t canId)
{
    const size_t count = sizeof(CanMembers) / sizeof(CanMember);
    for (size_t i = 0; i < count; ++i) {
        if (CanMembers[i].CanId == canId) {
            return CanMembers[i];
        }
    }
    // Fallback: explicit OTHER node (skipped by processAndUpdate via CanId == 0x0000)
    return UNKNOWN_NODE;
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
    // Always report the origin member, even for short/invalid frames, so the
    // caller never dereferences a null pointer.
    const CanMember &cm = lookupCanMember(can_id);
    *outCanMember = &cm;

    // Return if the message is too small
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const ElsterIndex *ei;
    uint8_t byte1;
    uint8_t byte2;
    char charValue[64];  // must fit longest decoded string (e.g. "Bereitschaftsbetrieb", "not used time domain")

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
        SetDoubleType(charValue, sizeof(charValue), ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    case et_triple_val:
        SetDoubleType(charValue, sizeof(charValue), ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    default:
        SetValueType(charValue, sizeof(charValue), ei->Type, static_cast<int>(byte2 + (byte1 << 8)));
        break;
    }

    uint16_t rawIdx = (msg[2] == 0xfa) ? (uint16_t)(msg[4] + (msg[3] << 8)) : (uint16_t)msg[2];
    ESP_LOGI("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(idx=0x%04x, %s)", cm.Name, (unsigned)can_id, ei->Name, charValue, (unsigned)rawIdx, ElsterTypeStr[ei->Type]);

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

// Publish to MQTT with throttled failure logging. A failed publish (e.g. broker
// temporarily offline) is reported at most every 5 s instead of being silently
// swallowed — the return value of ESPHome's publish() is bool.
static uint32_t lastMqttFailLogTs = 0;
inline void publishMqttChecked(const char* topic, const char* payload, size_t len,
                               const uint8_t qos = 0, const bool retain = true) {
    const bool ok = id(mqtt_client).publish(topic, payload, len, qos, retain);
    if (!ok) {
        const uint32_t now = millis();
        if (now - lastMqttFailLogTs > 5000U) {
            lastMqttFailLogTs = now;
            ESP_LOGW("MQTT", "publish failed to '%s': %.*s", topic, (int)len, payload);
        }
    }
}
// std::string-topic overload (matches ESPHome's publish overload set)
inline void publishMqttChecked(const std::string &topic, const char* payload, size_t len,
                               const uint8_t qos = 0, const bool retain = true) {
    publishMqttChecked(topic.c_str(), payload, len, qos, retain);
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
    publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    ESP_LOGI("MQTT", "Discovery published for calculated sensor: %s", config.name);
}

// Publish all calculated sensor discoveries (used during startup and republish)
void publishAllCalculatedSensorDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredCalculatedSensors.clear();
        ESP_LOGI("MQTT", "Republishing all calculated sensor discoveries");
    }
    
    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; i++) {
        publishCalculatedSensorDiscovery(calculatedSensors[i], forceRepublish);
    }
}

// ============================================================================
// MQTT WRITABLE NUMBER (for temperature setpoints, etc.)
// ============================================================================

// Publish MQTT discovery for a writable number entity
void publishWritableNumberDiscovery(const WritableNumberConfig& config, bool forceRepublish = false) {
    // Get CanMember for building topics
    const CanMember* cm = &CanMembers[config.member];
    
    // Build unique ID
    char uniqueId[128];
    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm->Name, config.signalName);
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
            << "\"mode\":\"box\","
            << "\"unit_of_measurement\":\"" << config.unit << "\","
            << "\"device_class\":\"" << config.deviceClass << "\","
            << "\"icon\":\"" << config.icon << "\",";
    
    // SG Ready controls and explicitly flagged controls go to the main device
    bool isSgReady = (strncmp(config.signalName, "SG_READY_", 9) == 0);
    
    if (isSgReady || config.onMainDevice) {
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
    publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

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
        ESP_LOGI("MQTT_DISC", "Publishing writable number %d: %s", i, writableNumbers[i].signalName);
        publishWritableNumberDiscovery(writableNumbers[i], forceRepublish);
    }
    ESP_LOGI("MQTT_DISC", "Completed publishing writable number discoveries");
}

// ============================================================================
// WRITABLE SELECT DISCOVERY (MQTT Select entities)
// ============================================================================

void publishWritableSelectDiscovery(const WritableSelectConfig& config, bool forceRepublish = false) {
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
    publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

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

// By-name lookup wrappers — avoid brittle positional array indexing in YAML handlers
void publishWritableNumberDiscoveryByName(const char* signalName, bool forceRepublish = false) {
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        if (strcmp(writableNumbers[i].signalName, signalName) == 0) {
            publishWritableNumberDiscovery(writableNumbers[i], forceRepublish);
            return;
        }
    }
    ESP_LOGW("MQTT", "No writable number for signal '%s'", signalName);
}

void publishWritableSelectDiscoveryByName(const char* signalName, bool forceRepublish = false) {
    for (size_t i = 0; i < WRITABLE_SELECT_COUNT; i++) {
        if (strcmp(writableSelects[i].signalName, signalName) == 0) {
            publishWritableSelectDiscovery(writableSelects[i], forceRepublish);
            return;
        }
    }
    ESP_LOGW("MQTT", "No writable select for signal '%s'", signalName);
}

// ============================================================================
// Note: Datetime controls are now implemented using Home Assistant input_datetime
// helpers synced via text_sensor platform and triggered by buttons.
// MQTT datetime discovery is not supported by Home Assistant.
// ============================================================================

void publishDate()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_DATE]);
    
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
    publishMqttChecked(stateTopic, datum.c_str(), datum.length(), 0, true);
    ESP_LOGI("CALC", "Published date: %s (Jahr=%d, Monat=%d, Tag=%d)", datum.c_str(), ijahr, imonat, itag);
}

void publishTime()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_TIME]);
    
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
    publishMqttChecked(stateTopic, zeit, strlen(zeit), 0, true);
    ESP_LOGI("CALC", "Published time: %s (Stunde=%d, Minute=%d, Sekunde=%d)", zeit, istunde, iminute, isekunde);
}

// Datetime states are now handled by Home Assistant input_datetime helpers
// Read by ESPHome text_sensors and written via button triggers

void publishBetriebsart(const std::string& sommerBetriebValue)
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_BETRIEBSART]);
    
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
    publishMqttChecked(stateTopic, betriebsart.c_str(), betriebsart.length(), 0, true);
    // ESP_LOGD("CALC", "Published Betriebsart: %s (SOMMERBETRIEB=%s)", betriebsart.c_str(), sommerBetriebValue.c_str());
}

void publishDeltaTContinuous()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_DELTA_T_CONTINUOUS]);
    
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
    publishMqttChecked(stateTopic, value, strlen(value), 0, true);
}

void publishDeltaTRunning()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_DELTA_T_RUNNING]);
    
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
    publishMqttChecked(stateTopic, value, strlen(value), 0, true);
}

void publishCompressorActive()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COMPRESSOR_ACTIVE]);
    
    // Check if compressor value is valid
    if (std::isnan(lastVerdichterValue)) {
        return; // Silently skip if value invalid
    }
    
    // Compressor is active if value > 2
    bool isActive = (lastVerdichterValue > 2.0);
    
    // Publish state to MQTT
    const char* state = isActive ? "on" : "off";
    const char* stateTopic = "heatingpump/calculated/compressor_active/state";
    publishMqttChecked(stateTopic, state, strlen(state), 0, true);
    // Note: Delta T running is published separately by scheduler
}

// Helper: Check if pattern appears anywhere in text (case-insensitive substring match)
// Get or create cached UID for a signal (eliminates repeated string ops)
// Get or create cached UID for a signal (no per-call string allocation).
// Returns a reference into the cache — valid until the entry is erased
// (unordered_map references stay valid across rehash/insert).
inline const std::string& getOrCreateUID(const CanMember &cm, const char* signalName) {
    UidKey key{cm.CanId, signalName};

    // Check cache first (no temporary string constructed)
    auto it = uidCache.find(key);
    if (it != uidCache.end()) {
        return it->second;
    }

    // Generate UID if not cached
    char uid[128];
    snprintf(uid, sizeof(uid), "stiebel_%s_%s", cm.Name, signalName);
    std::string result(uid);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '_');

    // Store in cache and return reference to the stored value
    auto res = uidCache.emplace(key, std::move(result));
    return res.first->second;
}

// Publish MQTT Discovery config for a signal
void publishMqttDiscovery(const CanMember &cm, const ElsterIndex *ei) {
    
    // Get cached UID (avoids repeated string operations)
    const std::string& uid = getOrCreateUID(cm, ei->Name);
    
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
    publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    
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
    publishMqttChecked(stateTopic, value.c_str(), value.length(), 0, true);
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
        publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
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
        publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
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
        publishMqttChecked(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
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
            publishMqttChecked(stateTopic, valueStr, strlen(valueStr), 0, true);
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
            publishMqttChecked(stateTopic, valueStr, strlen(valueStr), 0, true);
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
            publishMqttChecked(stateTopic, valueStr, strlen(valueStr), 0, true);
            ESP_LOGI("COP", "COP Gesamt: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_gesamt, waerme_total, el_total);
        }
    }
}

// Validation removed for simplification - all values are published as-is
// Invalid values like -255, -32768, etc. will be visible in Home Assistant
// Users can filter these in HA automations/dashboards if needed

// ============================================================================
// ESPHOME IO ADAPTER FOR SG READY CONTROLLER
// ============================================================================

// Route a write + read-back to the CAN member that actually owns the register.
// WPM3 (wpl10ac): the Einstell-DHW registers live on the display/operating unit
// (BEDIENMODUL_WPM3, 0x201) — the MANAGER ignores writes to them and answers
// reads with "not available" (0x8000 → -255). Everything else goes to the
// manager. Non-blocking: both calls only enqueue CAN frames (FIFO TX queue).
inline void writeCanSignalRouted(const char* signalName, const char* value) {
    const CanMember* member = &CanMembers[cm_manager];
#ifdef HA_MODEL_wpl10ac
    if (strcmp(signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0 ||
        strcmp(signalName, "EINSTELL_SPEICHERSOLLTEMP2") == 0) {
        member = &CanMembers[cm_bedienmodul_wpm3];
    }
#endif
    // Track the DHW setting locally so the WW_SOLL state stays stable even though
    // the WPM3 does not apply Einstell changes to the active setpoint.
    if (strcmp(signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0) {
        char* endPtr;
        float v = strtof(value, &endPtr);
        if (endPtr != value && *endPtr == '\0') lastEinstellSollTemp = v;
    }
    writeSignal(member, signalName, value);
    readSignal(member, signalName);
}

class EspHomeSgReadyIO : public ISgReadyIO {
public:
    static ESPPreferenceObject nvsDhw;
    static ESPPreferenceObject nvsRoom;
    static ESPPreferenceObject nvsActive;

    void writeCanSignal(const char* signalName, const char* value) override {
        writeCanSignalRouted(signalName, value);
    }

    // Current setpoints tracked from the CAN polls (see updateSensor dispatch).
    // Unknown values (never seen) are reported as 0.0 so the controller falls
    // back to its defaults.
    bool readCurrentSetpoints(float& dhwTemp, float& roomTemp) override {
        // Prefer the DHW SETTING (lastEinstellSollTemp) as the boost baseline so
        // a restore does not overwrite a user-set value with the stale active
        // setpoint. Fall back to the active setpoint (lastSpeicherSollTemp).
        dhwTemp  = !std::isnan(lastEinstellSollTemp) ? lastEinstellSollTemp
                 : (std::isnan(lastSpeicherSollTemp) ? 0.0f : lastSpeicherSollTemp);
        roomTemp = std::isnan(lastRaumSollTemp) ? 0.0f : lastRaumSollTemp;
        return true;
    }

    void publishMqtt(const char* topic, const char* value) override {
        publishMqttChecked(topic, value, strlen(value), 0, true);
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

// Wall time of the last SG Ready command (state or boost) — refreshed by any
// control input, consumed by the watchdog below.
static uint32_t lastSgReadyCommandTs = 0;

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
    lastSgReadyCommandTs = millis();
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
inline float sgReadyRoomBoost3Value() { return sgReadyController.roomBoost3(); }
inline float sgReadyRoomBoost4Value() { return sgReadyController.roomBoost4(); }
inline void  setSgReadyBoost3(float v) { lastSgReadyCommandTs = millis(); sgReadyController.setBoostState3(v); }
inline void  setSgReadyBoost4(float v) { lastSgReadyCommandTs = millis(); sgReadyController.setBoostState4(v); }
inline void  setSgReadyRoomBoost3(float v) { lastSgReadyCommandTs = millis(); sgReadyController.setRoomBoost3(v); }
inline void  setSgReadyRoomBoost4(float v) { lastSgReadyCommandTs = millis(); sgReadyController.setRoomBoost4(v); }

// Watchdog against a dead control source: only rescues from a stuck BOOST (3/4).
// Standby (EVU lock) is a deliberate long-term state and stays as-is. If a boost
// is active for longer than SG_READY_WATCHDOG_MS without any command refreshing
// it, fall back to Normal (heat pump keeps working, protected against a
// permanently boosted state even if the primary control is gone).
void checkSgReadyWatchdog() {
    const uint32_t now = millis();
    if (lastSgReadyCommandTs == 0) return;  // never commanded on this boot
    const int st = sgReadyController.currentState();
    if (st == SgReadyController::STATE_NORMAL) return;
    if (st == SgReadyController::STATE_EVU_LOCK) return;  // standby: intended, no auto-recover
    if (now - lastSgReadyCommandTs < SgReadyController::SG_READY_WATCHDOG_MS) return;
    ESP_LOGW("SG_READY_WATCHDOG", "Boost active without command for %lus — returning to Normal",
             (unsigned long)(SgReadyController::SG_READY_WATCHDOG_MS / 1000UL));
    applySgReadyState(SgReadyController::STATE_NORMAL);
}

// Republish the SG Ready control states (retained). Unlike polled CAN signals,
// these topics are otherwise only written on connect or on state change — a
// broker restart that loses retained messages would leave HA showing the
// controls as unavailable until the next change. Called on MQTT connect and
// periodically (see common.yaml) so the topics always self-heal.
void publishSgReadyStates() {
    const char* stateStr = SgReadyController::stateLabel(sgReadyController.currentState());
    publishMqttChecked("heatingpump/MANAGER/SG_READY_STATE/state", stateStr, strlen(stateStr), 0, true);

    char boost3Str[16], boost4Str[16];
    snprintf(boost3Str, sizeof(boost3Str), "%.1f", sgReadyBoostState3Value());
    snprintf(boost4Str, sizeof(boost4Str), "%.1f", sgReadyBoostState4Value());
    publishMqttChecked("heatingpump/MANAGER/SG_READY_BOOST_STATE3/state", boost3Str, strlen(boost3Str), 0, true);
    publishMqttChecked("heatingpump/MANAGER/SG_READY_BOOST_STATE4/state", boost4Str, strlen(boost4Str), 0, true);

    char rboost3Str[16], rboost4Str[16];
    snprintf(rboost3Str, sizeof(rboost3Str), "%.1f", sgReadyRoomBoost3Value());
    snprintf(rboost4Str, sizeof(rboost4Str), "%.1f", sgReadyRoomBoost4Value());
    publishMqttChecked("heatingpump/MANAGER/SG_READY_ROOM_BOOST_STATE3/state", rboost3Str, strlen(rboost3Str), 0, true);
    publishMqttChecked("heatingpump/MANAGER/SG_READY_ROOM_BOOST_STATE4/state", rboost4Str, strlen(rboost4Str), 0, true);

    // Mirror the normal setpoints into WW_SOLL / RAUM_SOLL. The DHW value is the
    // STORED setting (lastEinstellSollTemp — updated by our writes and by panel
    // change broadcasts) so it stays stable; the active setpoint is the fallback.
    float wwSoll = !std::isnan(lastEinstellSollTemp) ? lastEinstellSollTemp : lastSpeicherSollTemp;
    if (!std::isnan(wwSoll)) {
        char b[16];
        snprintf(b, sizeof(b), "%.1f", wwSoll);
        publishMqttChecked("heatingpump/MANAGER/WW_SOLL/state", b, strlen(b), 0, true);
    }
    if (!std::isnan(lastRaumSollTemp)) {
        char b[16];
        snprintf(b, sizeof(b), "%.1f", lastRaumSollTemp);
        publishMqttChecked("heatingpump/MANAGER/RAUM_SOLL/state", b, strlen(b), 0, true);
    }
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
constexpr uint32_t HASH_RAUMSOLLTEMP_I = hash("RAUMSOLLTEMP_I");
constexpr uint32_t HASH_SPEICHERSOLLTEMP = hash("SPEICHERSOLLTEMP");
constexpr uint32_t HASH_EINSTELL_SPEICHERSOLLTEMP = hash("EINSTELL_SPEICHERSOLLTEMP");
constexpr uint32_t HASH_RUECKLAUFISTTEMP = hash("RUECKLAUFISTTEMP");
constexpr uint32_t HASH_VERDICHTER = hash("VERDICHTER");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ = hash("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW = hash("EL_AUFNAHMELEISTUNG_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_2WE_WW = hash("WAERMEERTRAG_2WE_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_2WE_HEIZ = hash("WAERMEERTRAG_2WE_HEIZ_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW = hash("WAERMEERTRAG_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ = hash("WAERMEERTRAG_HEIZ_SUM_MWH");

// Sentinel values from the Elster decoder meaning "not available / unknown":
//   0x8000 → "-255", unknown bool → "?", et_dev_nr "not present" → "--",
//   et_default 0x8000 → "-32768".
// These are skipped so Home Assistant does not accumulate ghost entities that
// are permanently unavailable (and COP/calculated values never use them).
inline bool isInvalidSignalValue(const std::string &value) {
    return value == "-255" || value == "?" || value == "--" || value == "-32768";
}

void updateSensor(const CanMember &cm, const ElsterIndex *ei, const std::string &value)
{
    // Skip empty values
    if (value.empty()) {
        return;
    }

    // Skip invalid sentinel values (see isInvalidSignalValue)
    if (isInvalidSignalValue(value)) {
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
    
    // Bound memory: ignore NEW signals once the cap is reached (see
    // MAX_PUBLISHED_SIGNALS). Already-known signals keep updating normally.
    {
        UidKey probe{cm.CanId, ei->Name};
        if (uidCache.find(probe) == uidCache.end() && discoveredSignals.size() >= MAX_PUBLISHED_SIGNALS) {
            return;
        }
    }

    // Check if discovery is needed
    const std::string& uid = getOrCreateUID(cm, ei->Name);
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

        case HASH_RAUMSOLLTEMP_I: {
            // Track current room setpoint — the SG Ready boost captures this
            // as the "original" value so a restore returns to the real value.
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastRaumSollTemp = floatValue;
            }
            break;
        }

        case HASH_SPEICHERSOLLTEMP: {
            // Track current active DHW setpoint — the SG Ready boost captures
            // this as the "original" value so a restore returns to the real value.
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastSpeicherSollTemp = floatValue;
            }
            break;
        }

        case HASH_EINSTELL_SPEICHERSOLLTEMP: {
            // Panel DHW setting (broadcast by BEDIENMODUL_WPM3) — mirrored into
            // the WW_SOLL entity so it shows the real setpoint.
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastEinstellSollTemp = floatValue;
            }
            break;
        }

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
            storeCOPEnergyValue(signalName, value);
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
        if (twai_initiate_recovery() != ESP_OK) {
            ESP_LOGE("CAN_RECOVERY", "twai_initiate_recovery() failed — bus may stay off");
        }
    } else if (status.state == TWAI_STATE_STOPPED) {
        // twai_initiate_recovery() leaves the controller STOPPED once recovery
        // completes (128 bus-free occurrences) — must explicitly restart.
        // STOPPED here always means "post-recovery": the boot-time STOPPED
        // window (before twai_start_v2() in setup_internal()) completes
        // long before this 30s-cadence diagnostic loop ever runs.
        ESP_LOGW("CAN_RECOVERY", "TWAI stopped after recovery, restarting");
        if (twai_start() != ESP_OK) {
            ESP_LOGE("CAN_RECOVERY", "twai_start() failed — controller not restarted");
        }
    }

    // Publish discovery (only once per UID — cached internally)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_TEC]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_REC]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_BUS_ERRORS]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_STATE]);

    char buf[16];

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.tx_error_counter);
    publishMqttChecked("heatingpump/calculated/can_tec/state", buf, strlen(buf), 0, true);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.rx_error_counter);
    publishMqttChecked("heatingpump/calculated/can_rec/state", buf, strlen(buf), 0, true);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.bus_error_count);
    publishMqttChecked("heatingpump/calculated/can_bus_errors/state", buf, strlen(buf), 0, true);

    const char* stateStr;
    switch (status.state) {
        case TWAI_STATE_STOPPED:    stateStr = "Gestoppt";  break;
        case TWAI_STATE_RUNNING:    stateStr = "Läuft";     break;
        case TWAI_STATE_BUS_OFF:    stateStr = "Bus-Off";   break;
        case TWAI_STATE_RECOVERING: stateStr = "Erholung";  break;
        default:                    stateStr = "Unbekannt"; break;
    }
    publishMqttChecked("heatingpump/calculated/can_state/state",
                            stateStr, strlen(stateStr), 0, true);
}

// Process calculated sensor updates with frequency-based scheduling
// This function should be called regularly from the main loop
void processCalculatedSensors() {
    unsigned long now = millis();

    // SG Ready watchdog — must run regularly regardless of other schedules.
    checkSgReadyWatchdog();
    
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

    // Pre-allocate runtime containers once (avoids rehashing during operation);
    // then log their sizes every 10 min so unbounded growth is visible.
    static bool containersReserved = false;
    if (!containersReserved) {
        containersReserved = true;
        uidCache.reserve(UID_CACHE_RESERVE);
        copEnergyValues.reserve(COP_VALUES_RESERVE);
        ESP_LOGI("CONTAINERS", "Runtime containers reserved");
    }
    static uint32_t lastContainerLog = 0;
    if (now - lastContainerLog > 600000U) {
        lastContainerLog = now;
        ESP_LOGI("CONTAINERS", "uidCache=%zu discoveredSignals=%zu pollSlots=%zu copEnergy=%zu freeHeap=%u minFreeHeap=%u",
                 (size_t)uidCache.size(), (size_t)discoveredSignals.size(),
                 (size_t)pollSlots.size(), (size_t)copEnergyValues.size(),
                 (unsigned)diagFreeHeap(), (unsigned)diagMinFreeHeap());
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
        
        // Prebuild the poll schedule once: resolved signal pointers + concrete
        // members + intervals, with random initial offsets to spread the load.
        // The hot loop below is then a plain indexed walk (no string building,
        // no map lookups, no heap allocations).
        pollSlots.reserve(REQUEST_SCHEDULE_RESERVE);
        for (int i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
            const SignalRequest& req = signalRequests[i];
            const ElsterIndex* ei = GetElsterIndex(req.signalName);
            if (!ei || ei->Index == 0xFFFF) continue;
            
            // Skip blacklisted signals during initialization
            if (isPermanentlyBlacklisted(ei->Name)) continue;
            
            unsigned long intervalMs = req.frequency * 1000UL;
            
            if (req.member == cm_other) {
                // Fan-out to the fixed "all members" trio; the 3 slots are pushed
                // contiguously and share groupId = i+1 (unique per signal).
                const CanMember* allMembers[] = {
                    &CanMembers[cm_kessel],
                    &CanMembers[cm_manager],
                    &CanMembers[cm_heizmodul]
                };
                for (const auto* member : allMembers) {
                    // Random offset between 0 and full interval using ESP32 hardware RNG
                    unsigned long randomOffset = getRandomInRange(0, intervalMs + 1);
                    pollSlots.push_back({member, ei, intervalMs, now + randomOffset, i + 1});
                }
            } else {
                // Schedule for specific member
                const CanMember* member = &CanMembers[req.member];
                // Random offset between 0 and full interval using ESP32 hardware RNG
                unsigned long randomOffset = getRandomInRange(0, intervalMs + 1);
                pollSlots.push_back({member, ei, intervalMs, now + randomOffset, 0});
            }
        }

        // Poll the SG-Ready capture-relevant setpoints immediately instead of
        // spreading them over their (up to 10 min) interval, so a boost applied
        // shortly after boot captures the REAL device values rather than falling
        // back to the hardcoded defaults.
        for (auto& slot : pollSlots) {
            if (strcmp(slot.signal->Name, "RAUMSOLLTEMP_I") == 0 ||
                strcmp(slot.signal->Name, "SPEICHERSOLLTEMP") == 0) {
                slot.nextAt = now + getRandomInRange(0, 2000);
            }
        }

        ESP_LOGI("REQUEST_MGR", "Initialized %d poll slots", (int)pollSlots.size());
    }
    
    // Rate limiting: send up to MAX_REQUESTS_PER_ITERATION requests per iteration.
    // Randomization in scheduling prevents bursts.
    int requestsSentThisIteration = 0;
    const int slotCount = (int)pollSlots.size();
    if (slotCount == 0) {
        return;
    }

    // Walk the prebuilt schedule as a plain indexed array. lastGroupSentThisIteration
    // enforces that a cm_other fan-out group sends at most one member per iteration.
    int processedCount = 0;
    int currentIndex = 0;
    int lastGroupSentThisIteration = 0;

    while (processedCount < slotCount && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION) {
        PollSlot &slot = pollSlots[currentIndex];
        processedCount++;

        if (slot.groupId != 0 && slot.groupId == lastGroupSentThisIteration) {
            currentIndex = (currentIndex + 1) % slotCount;
            continue; // This group already sent a member this iteration -> defer
        }

        if (now >= slot.nextAt) {
            readSignal(slot.member, slot.signal);
            requestsSentThisIteration++;
            lastGroupSentThisIteration = slot.groupId;

            // Calculate next poll time with random offset (0 to 5% of interval) to
            // keep signals from synchronizing while staying close to target frequency
            unsigned long maxJitter = (slot.intervalMs / 20); // 5% of interval
            if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
            slot.nextAt = now + slot.intervalMs + getRandomInRange(0, maxJitter + 1);
        }

        currentIndex = (currentIndex + 1) % slotCount;
    }
}

void processAndUpdate(uint32_t can_id, const std::vector<uint8_t> &msg)
{
    // Ignore short/invalid frames (would decode to garbage)
    if (msg.size() < 7)
    {
        return;
    }

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
    if (!isValidTimeString(str_time)) {
        ESP_LOGW("SET_TIME", "Invalid time \"%s\" (expected \"HH:MM:SS\") — refusing (no partial writes)",
                 str_time ? str_time : "(null)");
        return;
    }
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
    if (!isValidDateString(str_date)) {
        ESP_LOGW("SET_DATE", "Invalid date \"%s\" (expected \"YYYY-MM-DD\") — refusing (no partial writes)",
                 str_date ? str_date : "(null)");
        return;
    }
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

inline void updateDateTime(const char *dts)
{
    if (!dts || !*dts) {
        ESP_LOGW("SET_DATETIME", "Empty timestamp payload — refusing");
        return;
    }

    // Trim surrounding whitespace — MQTT clients may append CR/LF or send tabs,
    // and copy/paste can inject non-breaking spaces (U+00A0).
    const char *p = dts;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t' || p[n-1] == '\r' || p[n-1] == '\n')) n--;
    char buf[32];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, p, n);
    buf[n] = '\0';

    if (n != 19 || buf[10] != ' ') {
        // Diagnostic: hex dump first 32 bytes so invisible characters are visible
        char hex[65];
        const size_t hn = (n > 32) ? 32 : n;
        for (size_t i = 0; i < hn; i++)
            snprintf(hex + i * 2, 3, "%02x", (unsigned char)buf[i]);
        hex[hn * 2] = '\0';
        ESP_LOGW("SET_DATETIME", "Invalid timestamp \"%s\" (len=%zu, hex=%s, expected \"YYYY-MM-DD HH:MM:SS\") — refusing",
                 buf, n, hex);
        return;
    }

    // Validate date (chars 0-9) and time (chars 11-18) separately — the helpers
    // check exact lengths, so the full timestamp must be split first.
    char dateBuf[11];
    memcpy(dateBuf, buf, 10);
    dateBuf[10] = '\0';
    bool dateOk = isValidDateString(dateBuf);
    bool timeOk = isValidTimeString(buf + 11);
    if (!dateOk || !timeOk) {
        ESP_LOGW("SET_DATETIME", "Invalid timestamp \"%s\" (dateOk=%d, timeOk=%d) — refusing",
                 buf, (int)dateOk, (int)timeOk);
        return;
    }
    ESP_LOGI("SET_DATETIME", "Setting WPM date/time to \"%s\"", buf);
    updateDate(CanMembers[cm_manager], dateBuf);
    updateTime(CanMembers[cm_manager], buf + 11);
}

#endif // !defined(HA_DUMMY_BUILD)

#endif // ha_stiebel_control_H


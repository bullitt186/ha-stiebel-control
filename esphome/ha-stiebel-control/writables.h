/*
 * writables.h - writable numbers and selects (tables, which model gets which entity) and their
 * discovery, including the climate entities built on them (#109). Commands: set_commands.h.
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef WRITABLES_H
#define WRITABLES_H

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include "config.h"
#include "language_select.h"
#include "KElsterTable.h"
#include "can_members.h"
#include "signal_route.h"
#include "mqtt_publish.h"
#include "discovery.h"
#include "can_io.h"           // readSignalRaw() with switch byte, write filter hook (#79)
#include "request_manager.h"  // extra step hook (#79)

// ============================================================================
// WRITABLE NUMBER CONFIGURATION (for MQTT Number entities)
// ============================================================================

struct WritableNumberConfig {
    const char* signalName;        // CAN signal name like "EINSTELL_SPEICHERSOLLTEMP"
    const char* friendlyName;      // Display name in Home Assistant
    CanMemberType member;          // Default owner; model routing may override it
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
     cm_kessel_canb, 0.0, 2.0, 0.05, "", "mdi:chart-line", ""},

    // WPMW II parameters (WPL13E only, see writableNumberApplies). Indices found by a
    // bus scan of the manager; ranges from the WPMW II commissioning list.
    // Display parameter 42 "Reglerdynamik" (0xfdb0). The table resolves the index to
    // its first name, SCHALTWERKDYNAMIKZEIT, so the state topic uses that name.
    {"SCHALTWERKDYNAMIKZEIT", LNAME_NUM_REGLERDYNAMIK,
     cm_manager, 1.0, 30.0, 1.0, "", "mdi:tune-vertical", ""},

    // Display parameters 27/33 bivalence point heating / hot water (0x01ac/0x01ad)
    {"BIVALENTPARALLELTEMPERATUR_HZG", LNAME_NUM_BIVALENZ_HZG,
     cm_manager, -20.0, 30.0, 0.5, "°C", "mdi:thermometer-alert", "temperature"},

    {"BIVALENTPARALLELTEMPERATUR_WW", LNAME_NUM_BIVALENZ_WW,
     cm_manager, -20.0, 30.0, 0.5, "°C", "mdi:thermometer-alert", "temperature"},

    // Display parameter 43 "Stillstandzeit" (0xfdb1), confirmed by a change on the display.
    // The manual allows 1-120 min but no less than 20 min in normal operation.
    {"STILLSTANDZEIT", LNAME_NUM_STILLSTANDZEIT,
     cm_manager, 20.0, 120.0, 1.0, "min", "mdi:timer-sand", "duration"},

    // Display parameter 10, summer mode AUSSENTEMP (0x01bf), confirmed by a change on the
    // display; 0xfdb5 SOMMERBETRIEB_TEMP did not move.
    {"SW_AUSSENTEMP", LNAME_NUM_SOMMERBETRIEB_TEMP,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:sun-thermometer", "temperature"},

    // SG Ready room setpoint boost of heating circuit 1 (handled internally, 0 = off)
    {"SG_READY_ROOM_BOOST_STATE3", LNAME_NUM_SG_READY_ROOM_BOOST3,
     cm_manager, 0.0, 5.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"SG_READY_ROOM_BOOST_STATE4", LNAME_NUM_SG_READY_ROOM_BOOST4,
     cm_manager, 0.0, 5.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    // WPMW II heating circuit 2 (mixer circuit): the manager keeps its settings on the virtual
    // member MISCHERMODUL_3 (0x602). Display "Raumtemp. 2" Tag/Absenk and "Heizkurve 2";
    // the Tag value moved 20.0 -> 20.5 on 0x602 after a change on the display (WPL13E).
    {"RAUMSOLLTEMP_I", LNAME_NUM_HK2_RAUMSOLL_TAG,
     cm_mischermodul_3, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},
    {"RAUMSOLLTEMP_NACHT", LNAME_NUM_HK2_RAUMSOLL_NACHT,
     cm_mischermodul_3, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer-outline", "temperature"},
    {"HEIZKURVE", LNAME_NUM_HK2_HEIZKURVE,
     cm_mischermodul_3, 0.1, 3.0, 0.05, "", "mdi:chart-bell-curve", ""}
};

static const size_t WRITABLE_NUMBER_COUNT = sizeof(writableNumbers) / sizeof(WritableNumberConfig);

// WPMW II settings exposed as writable numbers (manager parameters 10, 27, 33, 42, 43)
static bool isWpmw2Parameter(const char* sig) {
    static const char* const params[] = {"SCHALTWERKDYNAMIKZEIT",
                                         "BIVALENTPARALLELTEMPERATUR_HZG", "BIVALENTPARALLELTEMPERATUR_WW",
                                         "STILLSTANDZEIT", "SW_AUSSENTEMP"};
    for (const char* p : params)
        if (strcmp(sig, p) == 0) return true;
    return false;
}

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

// Operating mode in SG Ready state 2 (#85). The labels are the German protocol values.
static const char* sgNormalModeOptions[] = {
    LNAME_OPT_TAGBETRIEB,
    LNAME_OPT_AUTOMATIK,
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
     cm_manager, sgReadyOptions, 4, "mdi:solar-power"},

    // Operating mode written in SG Ready state 2
    {"SG_READY_NORMAL_MODE", LNAME_SEL_SG_READY_NORMAL_MODE,
     cm_manager, sgNormalModeOptions, 2, "mdi:home-clock"}
};

static const size_t WRITABLE_SELECT_COUNT = sizeof(writableSelects) / sizeof(WritableSelectConfig);

// Track which writable numbers have been discovered
static std::set<std::string> discoveredWritableNumbers;

// Track which writable selects have been discovered
static std::set<std::string> discoveredWritableSelects;

// ============================================================================
// MQTT WRITABLE NUMBER (for temperature setpoints, etc.)
// ============================================================================

// Discovery follows state ownership, which may differ from the write target.
static const CanMember* getWritableNumberMember(const WritableNumberConfig& config) {
    // WPMW II: the heating curve lives on the MANAGER, not on KESSEL_CANB
    // WPMW II heating circuit 2 settings stay on MISCHERMODUL_3 (no model routing)
    if (config.member == cm_mischermodul_3)
        return &CanMembers[cm_mischermodul_3];
    if (isWpmw2Model() && strcmp(config.signalName, "HEIZKURVE") == 0)
        return &CanMembers[cm_manager];
    const SignalRoute route = resolveSignalRoute(
        GetElsterIndex(config.signalName)->Index, config.member);
    return &CanMembers[route.stateMember];
}

// ============================================================================
// DEVICE LIMITS OF WRITABLE NUMBERS (#79)
// Read once per writable number with the switch bytes 0xF8 (min) and 0xF9 (max),
// after the request manager has started and one request at a time. The range that applies is the
// intersection of the device range and writableNumbers[] (static ranges are deliberate bounds).
// Until both answers are in, on 0x8000, an empty intersection or no answer, writableNumbers[]
// applies. Entries that never got an answer are asked again every NUMBER_LIMITS_RETRY_MS.
// ============================================================================
// Static: answered but unusable (0x8000, implausible, no overlap), stays. NoAnswer: timeout, asked again later.
enum class NumberLimitState : uint8_t { Pending, WaitMin, WaitMax, Device, Static, NoAnswer };

struct NumberLimits {
    NumberLimitState state;
    uint8_t tries;           // requests sent for the current switch byte
    unsigned long sentMs;    // time of the last request
    float min;               // valid when state == Device: device range cut to the static one
    float max;
};

static NumberLimits numberLimits[WRITABLE_NUMBER_COUNT] = {};
static int numberLimitsActive = -1;           // entry waiting for an answer, -1 = none
static unsigned long numberLimitsStartMs = 0; // first request not before this time (0 = not set yet)
static unsigned long numberLimitsLastSendMs = 0;

static constexpr unsigned long NUMBER_LIMITS_START_DELAY_MS = 60000;  // after the request manager started
static constexpr unsigned long NUMBER_LIMITS_GAP_MS = 2000;           // between two limit requests
static constexpr unsigned long NUMBER_LIMITS_TIMEOUT_MS = 3000;       // per request
static constexpr uint8_t NUMBER_LIMITS_MAX_TRIES = 2;                 // per switch byte
static constexpr unsigned long NUMBER_LIMITS_RETRY_MS = 3600000;      // ask again after no answer

inline int writableNumberIndex(const WritableNumberConfig& config) {
    if (&config < writableNumbers || &config >= writableNumbers + WRITABLE_NUMBER_COUNT) return -1;
    return static_cast<int>(&config - writableNumbers);
}

// Row of a write: by name and member, because some names exist twice (heating circuit 2 on
// MISCHERMODUL_3). Member = discovery member, table owner or write route of the row.
inline int writableNumberIndexFor(const CanMember* cm, const char* signal) {
    if (!cm) return -1;
    const ElsterIndex* ei = GetElsterIndex(signal);
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        const WritableNumberConfig& c = writableNumbers[i];
        if (strcmp(c.signalName, signal) != 0) continue;
        if (getWritableNumberMember(c)->CanId == cm->CanId || CanMembers[c.member].CanId == cm->CanId)
            return static_cast<int>(i);
        if (ei && CanMembers[resolveSignalRoute(ei->Index, c.member).writeMember].CanId == cm->CanId)
            return static_cast<int>(i);
    }
    return -1;
}

// Range that applies now: the device range (cut to the static one) once both answers are in,
// else the static one.
inline float writableNumberMin(size_t i) {
    return numberLimits[i].state == NumberLimitState::Device ? numberLimits[i].min : writableNumbers[i].min;
}
inline float writableNumberMax(size_t i) {
    return numberLimits[i].state == NumberLimitState::Device ? numberLimits[i].max : writableNumbers[i].max;
}

// Clamps a numeric write of a writable number to the device range, so values from automations
// or plain MQTT cannot leave it either. Only once the device range is known: with the static
// range (before the answers, fallback) writes stay as they were. Returns false if unchanged.
bool clampToNumberLimits(const CanMember* cm, const ElsterIndex* ei, const char* in, char* out, size_t outSize) {
    const int i = writableNumberIndexFor(cm, ei->Name);
    if (i < 0 || numberLimits[i].state != NumberLimitState::Device) return false;
    char* end;
    const float v = strtof(in, &end);
    if (end == in || *end != '\0' || std::isnan(v)) return false;   // not a number (e.g. mode text)
    const float lo = writableNumberMin(i), hi = writableNumberMax(i);
    const float c = v < lo ? lo : (v > hi ? hi : v);
    if (c == v) return false;
    snprintf(out, outSize, "%g", c);
    ESP_LOGW("writeSignal()", "%s: %s outside %g..%g, writing %s", ei->Name, in, lo, hi, out);
    return true;
}

// Publish MQTT discovery for a writable number entity
void publishWritableNumberDiscovery(const WritableNumberConfig& config, bool forceRepublish = false) {
#ifdef HA_READ_ONLY_true
    return;  // read_only: no writable entities in HA
#endif
#ifdef HA_DISABLE_SG_READY_true
    if (strncmp(config.signalName, "SG_READY_", 9) == 0) return;  // SG Ready disabled via substitution
#endif
    // Get CanMember for building topics (modellabhaengig fuer WPL17)
    const CanMember* cm = getWritableNumberMember(config);
    
    // Build unique ID. HEIZKURVE and the WPMW II parameters get a "_set" suffix so they
    // never collide with the read-only signal sensor of the same name.
    char uniqueId[128];
    if (strcmp(config.signalName, "HEIZKURVE") == 0 || isWpmw2Parameter(config.signalName)) {
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
             "homeassistant/number/" HA_TOPIC_ROOT "/%s/config", uid.c_str());
    
    // Build command topic: heatingpump/<MEMBER>/<SIGNAL>/set
    char commandTopic[128];
    snprintf(commandTopic, sizeof(commandTopic), HA_TOPIC_ROOT "/%s/%s/set", cm->Name, config.signalName);
    
    // Build state topic: heatingpump/<MEMBER>/<SIGNAL>/state
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), HA_TOPIC_ROOT "/%s/%s/state", cm->Name, config.signalName);

    // Range: read from the device once both limit answers are in (#79), else the static one
    const int limitIdx = writableNumberIndex(config);
    const float rangeMin = limitIdx >= 0 ? writableNumberMin(limitIdx) : config.min;
    const float rangeMax = limitIdx >= 0 ? writableNumberMax(limitIdx) : config.max;
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << defaultEntityIdJson("number", uid)
            << "\"command_topic\":\"" << commandTopic << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"min\":" << rangeMin << ","
            << "\"max\":" << rangeMax << ","
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
    
    // SG Ready controls belong to the heat pump device, the others to their CAN member
    payload << (isSgReady ? std::string(HA_MAIN_DEVICE_JSON) : memberDeviceJson(cm->Name)) << "}";

    // Publish discovery message
    std::string payloadStr = payload.str();
    mqttPublish(discoveryTopic, payloadStr.c_str(), payloadStr.length());

    ESP_LOGD("MQTT", "Discovery published for writable number: %s", config.friendlyName);
}

// Publish all writable number discoveries
// ============================================================================
// MQTT CLIMATE (thermostat cards) - in addition to the number/select entities,
// reusing their /set handlers and the model-specific CAN member mapping.
// ============================================================================
static const WritableNumberConfig* findWritableNumber(const char* signal) {
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++)
        if (strcmp(writableNumbers[i].signalName, signal) == 0) return &writableNumbers[i];
    return nullptr;
}

// The climates only heat and have no mode command. Without a mode state HA shows them as "off"
// (thermostat card "Aus"); the mode follows the availability of the ESP instead.
#define CLIMATE_MODE_STATE_JSON \
    "\"mode_state_topic\":\"" HA_TOPIC_ROOT "/status\"," \
    "\"mode_state_template\":\"{{ 'heat' if value == 'online' else 'off' }}\","

static bool writableNumberApplies(const WritableNumberConfig& config);

// WPMW II: heating circuit of the remote control FE 7 (manager FERNBEDIENUNGSZUORDNUNG, display
// parameter 22 "AUSWAHL FE": 1 = HK1, 2 = HK2; 0 = not read yet or no FE). The FE has the only
// room sensor; the manager mirrors its value on every virtual member (WPL13E, FE on HK2: 0x601,
// 0x602, 0x301 and 0x302 followed one heated sensor). Its room temperature belongs to the climate
// of that circuit only; the firmware reads it from 0x602.
static uint8_t remoteControlCircuit = 0;

inline std::string roomActualTopicJson(uint8_t circuit) {
    if (!isWpmw2Model() || remoteControlCircuit != circuit) return "";
    return "\"current_temperature_topic\":\"" HA_TOPIC_ROOT "/MISCHERMODUL_3/RAUMISTTEMP/state\",";
}

void publishClimateDiscoveries() {
#ifdef HA_READ_ONLY_true
    return;
#endif
    const char* device = HA_MAIN_DEVICE_JSON;
    const bool wpl17 = kModel.canBRoutes;   // DHW state on KESSEL_CANB
    const bool dhwActualOnCanb = wpl17 || kModel.dhwActualOnKesselCanb;

    // Heating: room setpoint (day) + operating mode (Programmschalter) as presets
    if (const WritableNumberConfig* room = findWritableNumber("RAUMSOLLTEMP_I")) {
        const CanMember* cm = getWritableNumberMember(*room);
        std::ostringstream p;
        p << "{\"name\":\"" << LNAME_CLIMATE_HEIZUNG << "\",\"unique_id\":\"stiebel_climate_heizung\","
          << defaultEntityIdJson("climate", "stiebel_climate_heizung")
          << "\"modes\":[\"heat\"],\"temperature_unit\":\"C\"," << CLIMATE_MODE_STATE_JSON
          << "\"min_temp\":" << writableNumberMin(writableNumberIndex(*room)) << ",\"max_temp\":" << writableNumberMax(writableNumberIndex(*room)) << ",\"temp_step\":" << room->step << ","
          << "\"temperature_command_topic\":\"" HA_TOPIC_ROOT "/" << cm->Name << "/RAUMSOLLTEMP_I/set\","
          << "\"temperature_state_topic\":\"" HA_TOPIC_ROOT "/" << cm->Name << "/RAUMSOLLTEMP_I/state\",";
        p << roomActualTopicJson(1) << "\"preset_modes\":[";
        for (size_t i = 0; i < sizeof(programmschalterOptions) / sizeof(programmschalterOptions[0]); i++)
            p << (i ? "," : "") << "\"" << programmschalterOptions[i] << "\"";
        p << "],\"preset_mode_command_topic\":\"" HA_TOPIC_ROOT "/MANAGER/PROGRAMMSCHALTER/set\","
          << "\"preset_mode_state_topic\":\"" HA_TOPIC_ROOT "/MANAGER/PROGRAMMSCHALTER/state\","
          << "\"action_topic\":\"" HA_TOPIC_ROOT "/calculated/compressor_active/state\","
          << "\"action_template\":\"{{ 'heating' if value == 'on' else 'idle' }}\","
          << "\"availability_topic\":\"" HA_TOPIC_ROOT "/status\",\"icon\":\"mdi:radiator\"," << device << "}";
        std::string s = p.str();
        mqttPublish("homeassistant/climate/" HA_TOPIC_ROOT "/stiebel_climate_heizung/config", s.c_str(), s.length());
    }

    // WPMW II heating circuit 2 (mixer circuit, virtual member 0x602): day room setpoint and
    // room actual temperature. The operating mode is shared and stays on the heating climate.
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        const WritableNumberConfig& hk2 = writableNumbers[i];
        if (hk2.member != cm_mischermodul_3 || strcmp(hk2.signalName, "RAUMSOLLTEMP_I") != 0 ||
            !writableNumberApplies(hk2))
            continue;
        std::ostringstream p;
        p << "{\"name\":\"" << LNAME_CLIMATE_HEIZUNG_HK2 << "\",\"unique_id\":\"stiebel_climate_heizung_hk2\","
          << defaultEntityIdJson("climate", "stiebel_climate_heizung_hk2")
          << "\"modes\":[\"heat\"],\"temperature_unit\":\"C\"," << CLIMATE_MODE_STATE_JSON
          << "\"min_temp\":" << writableNumberMin(i) << ",\"max_temp\":" << writableNumberMax(i) << ",\"temp_step\":" << hk2.step << ","
          << "\"temperature_command_topic\":\"" HA_TOPIC_ROOT "/MISCHERMODUL_3/RAUMSOLLTEMP_I/set\","
          << "\"temperature_state_topic\":\"" HA_TOPIC_ROOT "/MISCHERMODUL_3/RAUMSOLLTEMP_I/state\","
          << roomActualTopicJson(2)
          << "\"action_topic\":\"" HA_TOPIC_ROOT "/calculated/compressor_active/state\","
          << "\"action_template\":\"{{ 'heating' if value == 'on' else 'idle' }}\","
          << "\"availability_topic\":\"" HA_TOPIC_ROOT "/status\",\"icon\":\"mdi:heating-coil\"," << device << "}";
        std::string s = p.str();
        mqttPublish("homeassistant/climate/" HA_TOPIC_ROOT "/stiebel_climate_heizung_hk2/config", s.c_str(), s.length());
        break;
    }

    // Hot water: tank actual temperature + DHW setpoint
    if (const WritableNumberConfig* dhw = findWritableNumber("EINSTELL_SPEICHERSOLLTEMP")) {
        const CanMember* cm = getWritableNumberMember(*dhw);
        std::ostringstream p;
        p << "{\"name\":\"" << LNAME_CLIMATE_WARMWASSER << "\",\"unique_id\":\"stiebel_climate_warmwasser\","
          << defaultEntityIdJson("climate", "stiebel_climate_warmwasser")
          << "\"modes\":[\"heat\"],\"temperature_unit\":\"C\"," << CLIMATE_MODE_STATE_JSON
          << "\"min_temp\":" << writableNumberMin(writableNumberIndex(*dhw)) << ",\"max_temp\":" << writableNumberMax(writableNumberIndex(*dhw)) << ",\"temp_step\":" << dhw->step << ","
          << "\"temperature_command_topic\":\"" HA_TOPIC_ROOT "/" << cm->Name << "/EINSTELL_SPEICHERSOLLTEMP/set\","
          << "\"temperature_state_topic\":\"" HA_TOPIC_ROOT "/" << cm->Name << "/EINSTELL_SPEICHERSOLLTEMP/state\","
          << "\"current_temperature_topic\":\"" HA_TOPIC_ROOT "/" << (dhwActualOnCanb ? "KESSEL_CANB" : "KESSEL") << "/SPEICHERISTTEMP/state\","
          << "\"availability_topic\":\"" HA_TOPIC_ROOT "/status\",\"icon\":\"mdi:water-boiler\"," << device << "}";
        std::string s = p.str();
        mqttPublish("homeassistant/climate/" HA_TOPIC_ROOT "/stiebel_climate_warmwasser/config", s.c_str(), s.length());
    }
}

// Whether the model gets a writable number entity for this signal
static bool writableNumberApplies(const WritableNumberConfig& config) {
    const char* sig = config.signalName;
    // Single-circuit models: no RAUMSOLLTEMP_II/III entities (-255 only).
    if ((strcmp(sig, "RAUMSOLLTEMP_II") == 0 || strcmp(sig, "RAUMSOLLTEMP_III") == 0) && kModel.singleHeatingCircuit)
        return false;
    // Heating circuit 2 settings on MISCHERMODUL_3 (0x602): WPMW II writes checked on the display.
    if (config.member == cm_mischermodul_3)
        return kModel.wpmw2WritesVerified;
    if (strcmp(sig, "HEIZKURVE") == 0 && !kModel.heatingCurveWritable())
        return false;
    // WPMW II parameters 10/27/33/42/43 (WPL13E numbering)
    if (isWpmw2Parameter(sig) && !kModel.wpmw2WritesVerified)
        return false;
    return true;
}

// By name: the first entry with this signal (the circuit 1 / manager one)
static bool writableNumberApplies(const char* sig) {
    const WritableNumberConfig* config = findWritableNumber(sig);
    return config && writableNumberApplies(*config);
}

void publishAllWritableNumberDiscoveries(bool forceRepublish = false) {
    publishClimateDiscoveries();
    if (forceRepublish) {
        discoveredWritableNumbers.clear();
        ESP_LOGD("MQTT", "Republishing all writable number discoveries");
    }
    
    ESP_LOGD("MQTT_DISC", "Publishing %d writable number discoveries", WRITABLE_NUMBER_COUNT);
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        [[maybe_unused]] const char* sig = writableNumbers[i].signalName;   // logs only
        if (!writableNumberApplies(writableNumbers[i])) {
            ESP_LOGD("MQTT_DISC", "Skipping %s on %s", sig, HA_DEVICE_MODEL_STR);
            continue;
        }
        ESP_LOGD("MQTT_DISC", "Publishing writable number %d: %s", i, sig);
        publishWritableNumberDiscovery(writableNumbers[i], forceRepublish);
    }
    ESP_LOGD("MQTT_DISC", "Completed publishing writable number discoveries");
}

// ----------------------------------------------------------------------------
// Device limits (#79): request scheduling and answers
// ----------------------------------------------------------------------------

// Entries that are read from the device: real CAN signals that apply to this model.
static bool numberLimitsWanted(size_t i) {
#ifdef HA_READ_ONLY_true
    return false;   // no writable entities
#endif
    const char* sig = writableNumbers[i].signalName;
    if (strncmp(sig, "SG_READY_", 9) == 0) return false;   // virtual, not on the bus
    // By row, not by name: RAUMSOLLTEMP_I also exists for heating circuit 2 on MISCHERMODUL_3
    if (!writableNumberApplies(writableNumbers[i])) return false;
    return GetElsterIndex(sig) != &ElsterTable[0];
}

// Raw limit -> value in the signal's unit. False for 0x8000 ("not provided") and non-numbers.
static bool decodeNumberLimit(const ElsterIndex* ei, uint16_t raw, float& out) {
    if (raw == 0x8000) return false;
    char buf[ELSTER_VALUE_BUF_SIZE];
    SetValueType(buf, ei->Type, raw);
    char* end;
    out = strtof(buf, &end);
    return end != buf && *end == '\0';
}

static void numberLimitsSend(size_t i, uint8_t switchByte, unsigned long now) {
    const WritableNumberConfig& c = writableNumbers[i];
    readSignalRaw(getWritableNumberMember(c), GetElsterIndex(c.signalName)->Index, c.signalName, switchByte);
    numberLimits[i].sentMs = now;
    numberLimits[i].tries++;
    numberLimitsLastSendMs = now;
}

static void numberLimitsUseStatic(size_t i, [[maybe_unused]] const char* why, bool retryLater = false) {
    numberLimits[i].state = retryLater ? NumberLimitState::NoAnswer : NumberLimitState::Static;   // sentMs = last request
    numberLimitsActive = -1;
    ESP_LOGI("LIMITS", "%s: %s, keeping %g..%g", writableNumbers[i].signalName, why,
             writableNumbers[i].min, writableNumbers[i].max);
}

// Both answers in: 0x8000 on one side keeps that side static; nothing usable, min >= max or an
// empty intersection with the static range keeps the static range. Otherwise the discovery is
// published again with the intersection (lo = max, hi = min).
static void numberLimitsFinish(size_t i, bool haveMin, float devMin, bool haveMax, float devMax) {
    if (!haveMin && !haveMax) { numberLimitsUseStatic(i, "no device limits"); return; }
    const WritableNumberConfig& c = writableNumbers[i];
    const float rawLo = haveMin ? devMin : c.min;
    const float rawHi = haveMax ? devMax : c.max;
    if (!(rawLo < rawHi)) { numberLimitsUseStatic(i, "implausible device range"); return; }
    const float lo = rawLo > c.min ? rawLo : c.min;
    const float hi = rawHi < c.max ? rawHi : c.max;
    if (!(lo < hi)) {
        ESP_LOGW("LIMITS", "%s: device range %g..%g does not overlap static %g..%g", c.signalName, rawLo, rawHi, c.min, c.max);
        numberLimitsUseStatic(i, "no overlap");
        return;
    }
    numberLimits[i].min = lo;
    numberLimits[i].max = hi;
    numberLimits[i].state = NumberLimitState::Device;
    numberLimitsActive = -1;
    ESP_LOGI("LIMITS", "%s: device range %g..%g, static %g..%g, using %g..%g", c.signalName, rawLo, rawHi, c.min, c.max, lo, hi);
    publishWritableNumberDiscovery(c, true);
    if (strcmp(c.signalName, "RAUMSOLLTEMP_I") == 0 || strcmp(c.signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0)
        publishClimateDiscoveries();
}

static float numberLimitsPendingMin[WRITABLE_NUMBER_COUNT];
static bool numberLimitsPendingHaveMin[WRITABLE_NUMBER_COUNT];

// Back to "nothing read yet" (unit tests); with keepStatic the limits are never requested.
void resetNumberLimits(bool keepStatic = false) {
    for (auto& l : numberLimits)
        l = NumberLimits{keepStatic ? NumberLimitState::Static : NumberLimitState::Pending, 0, 0, 0.0f, 0.0f};
    numberLimitsActive = -1;
    numberLimitsStartMs = 0;
    numberLimitsLastSendMs = 0;
}

// One step from the request manager: at most one limit request, spaced by NUMBER_LIMITS_GAP_MS.
void numberLimitsStep(unsigned long now) {
    if (numberLimitsStartMs == 0) { numberLimitsStartMs = now + NUMBER_LIMITS_START_DELAY_MS; return; }
    if (static_cast<int32_t>(now - numberLimitsStartMs) < 0) return;
    if (numberLimitsLastSendMs != 0 && now - numberLimitsLastSendMs < NUMBER_LIMITS_GAP_MS) return;

    if (numberLimitsActive >= 0) {
        const size_t i = static_cast<size_t>(numberLimitsActive);
        NumberLimits& L = numberLimits[i];
        if (L.tries == 0) {   // min answered, max not asked yet
            numberLimitsSend(i, ELSTER_SWITCH_MAX, now);
            return;
        }
        if (now - L.sentMs < NUMBER_LIMITS_TIMEOUT_MS) return;
        if (L.tries >= NUMBER_LIMITS_MAX_TRIES) { numberLimitsUseStatic(i, "no answer, asking again later", true); return; }
        numberLimitsSend(i, L.state == NumberLimitState::WaitMin ? ELSTER_SWITCH_MIN : ELSTER_SWITCH_MAX, now);
        return;
    }

    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++)   // ask again where nobody answered
        if (numberLimits[i].state == NumberLimitState::NoAnswer && now - numberLimits[i].sentMs >= NUMBER_LIMITS_RETRY_MS)
            numberLimits[i].state = NumberLimitState::Pending;
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        if (numberLimits[i].state != NumberLimitState::Pending) continue;
        if (!numberLimitsWanted(i)) { numberLimits[i].state = NumberLimitState::Static; continue; }
        numberLimits[i].state = NumberLimitState::WaitMin;
        numberLimits[i].tries = 0;
        numberLimitsActive = static_cast<int>(i);
        numberLimitsSend(i, ELSTER_SWITCH_MIN, now);
        return;
    }
}

// Answer to a 0xF8/0xF9 request (long layout). Returns true if the frame was a limit answer;
// it is never handed to updateSensor(), the limit is not a measured value.
bool handleLimitAnswer(uint32_t can_id, const std::vector<uint8_t>& msg) {
    if ((msg[0] & 0x0F) != 2 || (msg[2] != ELSTER_SWITCH_MIN && msg[2] != ELSTER_SWITCH_MAX)) return false;
    if (numberLimitsActive < 0) return true;   // someone else's limit read
    const size_t i = static_cast<size_t>(numberLimitsActive);
    const WritableNumberConfig& c = writableNumbers[i];
    const ElsterIndex* ei = GetElsterIndex(c.signalName);
    const uint16_t idx = static_cast<uint16_t>((msg[3] << 8) | msg[4]);
    if (getWritableNumberMember(c)->CanId != can_id || ei->Index != idx) return true;
    const uint16_t raw = static_cast<uint16_t>((msg[5] << 8) | msg[6]);
    NumberLimits& L = numberLimits[i];
    float v = 0.0f;
    if (msg[2] == ELSTER_SWITCH_MIN && L.state == NumberLimitState::WaitMin) {
        numberLimitsPendingHaveMin[i] = decodeNumberLimit(ei, raw, v);
        numberLimitsPendingMin[i] = v;
        L.state = NumberLimitState::WaitMax;
        L.tries = 0;   // the next step asks for the maximum
    } else if (msg[2] == ELSTER_SWITCH_MAX && L.state == NumberLimitState::WaitMax && L.tries > 0) {
        const bool haveMax = decodeNumberLimit(ei, raw, v);
        numberLimitsFinish(i, numberLimitsPendingHaveMin[i], numberLimitsPendingMin[i], haveMax, v);
    }
    return true;
}

// Hooks into the lower modules (#79): the clamp for writeSignal()/queuePacedWrite() and the limit
// requests after each request manager pass. Installed at static initialisation (one translation
// unit), so they are active before the first write or poll.
[[maybe_unused]] static const bool numberLimitsHooksInstalled = [] {
    writeValueFilter = clampToNumberLimits;
    requestManagerExtraStep = numberLimitsStep;
    return true;
}();

// ============================================================================
// WRITABLE SELECT DISCOVERY (MQTT Select entities)
// ============================================================================

void publishWritableSelectDiscovery(const WritableSelectConfig& config, bool forceRepublish = false) {
#ifdef HA_READ_ONLY_true
    return;  // read_only: no writable entities in HA
#endif
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
    discoveryTopicStream << "homeassistant/select/" HA_TOPIC_ROOT "/" << uniqueIdStr << "/config";
    std::string discoveryTopic = discoveryTopicStream.str();
    
    // Build command and state topics
    std::ostringstream commandTopicStream;
    commandTopicStream << HA_TOPIC_ROOT "/" << cm->Name << "/" << config.signalName << "/set";
    std::string commandTopic = commandTopicStream.str();
    
    std::ostringstream stateTopicStream;
    stateTopicStream << HA_TOPIC_ROOT "/" << cm->Name << "/" << config.signalName << "/state";
    std::string stateTopic = stateTopicStream.str();
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uniqueIdStr << "\","
            << defaultEntityIdJson("select", uniqueIdStr)
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
    
    // SG Ready controls belong to the heat pump device, the others to their CAN member
    payload << (isSgReady ? std::string(HA_MAIN_DEVICE_JSON) : memberDeviceJson(cm->Name)) << "}";

    // Publish discovery message
    std::string payloadStr = payload.str();
    mqttPublish(discoveryTopic.c_str(), payloadStr.c_str(), payloadStr.length());

    // Mark as discovered
    discoveredWritableSelects.insert(uniqueIdStr);
    
    ESP_LOGD("MQTT", "Discovery published for writable select: %s", config.friendlyName);
}

// Publish all writable select discoveries
void publishAllWritableSelectDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredWritableSelects.clear();
        ESP_LOGD("MQTT", "Republishing all writable select discoveries");
    }
    
    ESP_LOGD("MQTT_DISC", "Publishing %d writable select discoveries", WRITABLE_SELECT_COUNT);
    for (size_t i = 0; i < WRITABLE_SELECT_COUNT; i++) {
        ESP_LOGD("MQTT_DISC", "Publishing writable select %d: %s", i, writableSelects[i].signalName);
        publishWritableSelectDiscovery(writableSelects[i], forceRepublish);
    }
    ESP_LOGD("MQTT_DISC", "Completed publishing writable select discoveries");
}

#endif  // WRITABLES_H

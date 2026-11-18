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
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << defaultEntityIdJson("number", uid)
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

    // Heating: room setpoint (day) + operating mode (Programmschalter) as presets
    if (const WritableNumberConfig* room = findWritableNumber("RAUMSOLLTEMP_I")) {
        const CanMember* cm = getWritableNumberMember(*room);
        std::ostringstream p;
        p << "{\"name\":\"" << LNAME_CLIMATE_HEIZUNG << "\",\"unique_id\":\"stiebel_climate_heizung\","
          << defaultEntityIdJson("climate", "stiebel_climate_heizung")
          << "\"modes\":[\"heat\"],\"temperature_unit\":\"C\"," << CLIMATE_MODE_STATE_JSON
          << "\"min_temp\":" << room->min << ",\"max_temp\":" << room->max << ",\"temp_step\":" << room->step << ","
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
          << "\"min_temp\":" << hk2.min << ",\"max_temp\":" << hk2.max << ",\"temp_step\":" << hk2.step << ","
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
          << "\"min_temp\":" << dhw->min << ",\"max_temp\":" << dhw->max << ",\"temp_step\":" << dhw->step << ","
          << "\"temperature_command_topic\":\"" HA_TOPIC_ROOT "/" << cm->Name << "/EINSTELL_SPEICHERSOLLTEMP/set\","
          << "\"temperature_state_topic\":\"" HA_TOPIC_ROOT "/" << cm->Name << "/EINSTELL_SPEICHERSOLLTEMP/state\","
          << "\"current_temperature_topic\":\"" HA_TOPIC_ROOT "/" << (wpl17 ? "KESSEL_CANB" : "KESSEL") << "/SPEICHERISTTEMP/state\","
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
        const char* sig = writableNumbers[i].signalName;
        if (!writableNumberApplies(writableNumbers[i])) {
            ESP_LOGD("MQTT_DISC", "Skipping %s on %s", sig, HA_DEVICE_MODEL_STR);
            continue;
        }
        ESP_LOGD("MQTT_DISC", "Publishing writable number %d: %s", i, sig);
        publishWritableNumberDiscovery(writableNumbers[i], forceRepublish);
    }
    ESP_LOGD("MQTT_DISC", "Completed publishing writable number discoveries");
}

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

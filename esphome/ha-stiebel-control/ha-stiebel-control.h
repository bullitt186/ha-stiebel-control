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
        {"MANAGER", 0x480},
        {"HEIZMODUL", 0x500},
        {"BUSKOPPLER", 0x580},
        {"MISCHERMODUL_1", 0x600},
        {"MISCHERMODUL_2", 0x601},
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
    cm_atez,
    cm_bedienmodul_1,
    cm_bedienmodul_2,
    cm_bedienmodul_3,
    cm_bedienmodul_4,
    cm_raumfernfuehler,
    cm_manager,
    cm_heizmodul,
    cm_buskoppler,
    cm_mischermodul_1,
    cm_mischermodul_2,
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
};

static const CalculatedSensorConfig calculatedSensors[] = {
    // Date sensor
    {"stiebel_calculated_date", "Datum", "heatingpump/calculated/date/state", 
     "sensor", "", "", "", "mdi:calendar", "", ""},
    
    // Time sensor
    {"stiebel_calculated_time", "Uhrzeit", "heatingpump/calculated/time/state", 
     "sensor", "", "", "", "mdi:clock", "", ""},
    
    // Betriebsart sensor
    {"stiebel_calculated_betriebsart", "Betriebsart", "heatingpump/calculated/betriebsart/state", 
     "sensor", "", "", "", "mdi:cog", "", ""},
    
    // Delta T continuous
    {"stiebel_calculated_delta_t_continuous", "Delta T WP (kontinuierlich)", "heatingpump/calculated/delta_t_continuous/state", 
     "sensor", "temperature", "K", "measurement", "mdi:thermometer", "", ""},
    
    // Delta T running (only when compressor active)
    {"stiebel_calculated_delta_t_running", "Delta T WP (nur bei Verdichter an)", "heatingpump/calculated/delta_t_running/state", 
     "sensor", "temperature", "K", "measurement", "mdi:thermometer-chevron-up", "", ""},
    
    // Compressor active binary sensor
    {"stiebel_calculated_compressor_active", "WP Verdichter aktiv", "heatingpump/calculated/compressor_active/state", 
     "binary_sensor", "running", "", "", "mdi:engine", "on", "off"}
};

static const size_t CALCULATED_SENSOR_COUNT = sizeof(calculatedSensors) / sizeof(CalculatedSensorConfig);

// ============================================================================
// RUNTIME STATE TRACKING
// ============================================================================

// Track which signals have been discovered
static std::set<std::string> discoveredSignals;

// Track which calculated sensors have been discovered
static std::set<std::string> discoveredCalculatedSensors;

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

// Include device-specific signal request table (must come AFTER SignalRequest is defined)
#include "signal_requests_wpl13e.h"

// Runtime state for signal request manager
static bool requestManagerStarted = false;
static unsigned long requestManagerStartTime = 0;

// ============================================================================
// CAN BUS HELPER FUNCTIONS
// ============================================================================

// Only compile function implementations when building with ESPHome framework
// Skip when compiling ha-dummy.cpp or other standalone contexts
#if !defined(HA_DUMMY_BUILD)

/**
 * Generate CAN read ID from member CAN ID
 */
std::vector<uint8_t> generate_read_id(unsigned short can_id)
{
    std::vector<uint8_t> read_id;
    uint8_t address = (can_id & 0x780) / 8;
    uint8_t first_byte_read = (address & 0xF0) + 1;
    uint8_t second_byte = can_id & 7;

    read_id.push_back(first_byte_read);
    read_id.push_back(second_byte);

    return read_id;
}

std::vector<uint8_t> generate_write_id(unsigned short can_id)
{
    std::vector<uint8_t> write_id;
    uint8_t address = (can_id & 0x780) / 8;
    uint8_t first_byte_write = (address & 0xF0);
    uint8_t second_byte = can_id & 7;

    write_id.push_back(first_byte_write);
    write_id.push_back(second_byte);

    return write_id;
}

const CanMember &lookupCanMember(uint32_t canId)
{
    for (const CanMember &member : CanMembers)
    {
        if (member.CanId == canId)
        {
            return member;
        }
    }
    // Return the last element if member is not found
    return CanMembers[sizeof(CanMembers) / sizeof(CanMembers[0]) - 1];
}

CanMemberType lookupCanMemberType(uint32_t canId)
{
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
    {
        if (CanMembers[i].CanId == canId)
        {
            return static_cast<CanMemberType>(i);
        }
    }
    return cm_other; // Return cm_other if member is not found
}

// Check if signal is blacklisted (now stored in ElsterIndex)
inline bool isPermanentlyBlacklisted(const char* signalName) {
    const ElsterIndex* ei = GetElsterIndex(signalName);
    return ei->isBlacklisted;
}

// Helper: Get type-based defaults for Home Assistant metadata
inline void getTypeDefaults(ElsterType type, const char*& component, const char*& deviceClass, const char*& unit, const char*& stateClass, const char*& icon) {
    switch(type) {
        case et_dec_val:
        case et_cent_val:
        case et_mil_val:
            component = "sensor"; deviceClass = "temperature"; unit = "°C"; stateClass = "measurement"; icon = "mdi:thermometer";
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

const ElsterIndex *processCanMessage(const std::vector<uint8_t> &msg, uint32_t can_id, std::string &signalValue)
{
    // Return if the message is too small
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const CanMember &cm = lookupCanMember(can_id);

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

    ESP_LOGI("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(%s)", cm.Name, cm.CanId, ei->Name, charValue, ElsterTypeStr[ei->Type]);

    signalValue = charValue;
    return ei;
}

void readSignal(const CanMember *cm, const ElsterIndex *ei)
{
    constexpr bool use_extended_id = false; // No use of extended ID
    const uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    const uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    std::vector<uint8_t> readId = generate_read_id(cm->CanId);

    if (IndexByte1 == 0x00)
    {
        data = {readId[0],
                readId[1],
                IndexByte2,
                0x00,
                0x00,
                0x00,
                0x00};
    }
    else
    {
        data = {readId[0],
                readId[1],
                0xFA,
                IndexByte1,
                IndexByte2,
                0x00,
                0x00};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, cm->Name, cm->CanId, readId[0], readId[1], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("readSignal()", "%s", logmsg);

    id(my_mcp2515).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
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
    uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    std::vector<uint8_t> writeId = generate_write_id(cm->CanId);

    if (IndexByte1 == 0x00)
    {
        data = {writeId[0],
                writeId[1],
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF),
                0x00,
                0x00};
    }
    else
    {
        data = {writeId[0],
                writeId[1],
                0xfa,
                IndexByte1,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF)};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             ei->Name, ei->Index, writeValue, cm->Name, cm->CanId, writeId[0], writeId[1],
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("writeSignal()", "%s", logmsg);

    id(my_mcp2515).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void writeSignal(const CanMember *cm, const char *elsterName, const char *&str)
{
    writeSignal(cm, GetElsterIndex(elsterName), str);
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
    
    // Add device info
    payload << ",\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    
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

std::string formatNumber(int number, int width)
{
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << number;
    return oss.str();
}

void publishTime()
{
    // Validate that all values have been received
    if (lastStunde < 0 || lastMinute < 0 || lastSekunde < 0) {
        ESP_LOGW("CALC", "Cannot publish time: sensors not initialized (Stunde=%d, Minute=%d, Sekunde=%d)", 
                 lastStunde, lastMinute, lastSekunde);
        return;
    }
    
    int istunde = lastStunde;
    int iminute = lastMinute;
    int isekunde = lastSekunde;
    
    // Log raw values for debugging
    // ESP_LOGD("CALC", "Time values: Stunde=%d, Minute=%d, Sekunde=%d", istunde, iminute, isekunde);
    
    // Validate ranges
    if (istunde < 0 || istunde > 23 || iminute < 0 || iminute > 59 || isekunde < 0 || isekunde > 59) {
        ESP_LOGW("CALC", "Time values out of range: Stunde=%d, Minute=%d, Sekunde=%d", istunde, iminute, isekunde);
        return;
    }
    
    // Format time components
    std::string stunde = formatNumber(istunde, 2);
    std::string minute = formatNumber(iminute, 2);
    std::string sekunde = formatNumber(isekunde, 2);

    std::string zeit = stunde + ":" + minute + ":" + sekunde;
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/time/state";
    id(mqtt_client).publish(stateTopic, zeit.c_str(), zeit.length(), 0, true);
    ESP_LOGI("CALC", "Published time: %s (Stunde=%d, Minute=%d, Sekunde=%d)", zeit.c_str(), istunde, iminute, isekunde);
}

void publishBetriebsart(const std::string& sommerBetriebValue)
{
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
inline std::string getOrCreateUID(uint32_t can_id, const char* signalName) {
    // Create cache key
    char cacheKey[256];
    snprintf(cacheKey, sizeof(cacheKey), "%u:%s", can_id, signalName);
    
    // Check cache first
    auto it = uidCache.find(cacheKey);
    if (it != uidCache.end()) {
        return it->second;
    }
    
    // Generate UID if not cached
    const CanMember &cm = lookupCanMember(can_id);
    char uid[128];
    snprintf(uid, sizeof(uid), "stiebel_%s_%s", cm.Name, signalName);
    std::string result(uid);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '_');
    
    // Store in cache
    uidCache[cacheKey] = result;
    return result;
}

// Publish main device (parent for all CAN member sub-devices)
inline void publishMainDevice() {
    const char* mainDeviceId = "stiebel_eltron_wpl13e";
    
    // Publish a sensor to register the main device
    const char* discoveryTopic = "homeassistant/sensor/heatingpump/main_device/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"Wärmepumpe Status\","
            << "\"unique_id\":\"" << mainDeviceId << "_status\","
            << "\"state_topic\":\"heatingpump/status\","
            << "\"icon\":\"mdi:heat-pump\","
            << "\"device\":{\"identifiers\":[\"" << mainDeviceId << "\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"model\":\"WPL 13 E\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    
    ESP_LOGI("MQTT", "Main device published: Stiebel Eltron Wärmepumpe");
}

// Publish MQTT Discovery config for a signal
void publishMqttDiscovery(uint32_t can_id, const ElsterIndex *ei) {
    const CanMember &cm = lookupCanMember(can_id);
    
    // Get cached UID (avoids repeated string operations)
    std::string uid = getOrCreateUID(can_id, ei->Name);
    
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
    const char* mainDeviceId = "stiebel_eltron_wpl13e";
    
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
void publishMqttState(uint32_t can_id, const ElsterIndex *ei, const std::string &value) {
    // Input validation
    if (!ei || !ei->Name || value.empty()) {
        ESP_LOGW("MQTT", "Invalid signal data, skipping state publish");
        return;
    }
    
    const CanMember &cm = lookupCanMember(can_id);
    
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
        payload << "{\"name\":\"COP Warmwasser\","
                << "\"unique_id\":\"stiebel_cop_ww\","
                << "\"state_topic\":\"heatingpump/calculated/cop_ww/state\","
                << "\"icon\":\"mdi:water-boiler\","
                << "\"state_class\":\"measurement\","
                << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
        std::string payloadStr = payload.str();
        id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    }
    
    // COP Heizung
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/cop_heiz/config";
        std::ostringstream payload;
        payload << "{\"name\":\"COP Heizung\","
                << "\"unique_id\":\"stiebel_cop_heiz\","
                << "\"state_topic\":\"heatingpump/calculated/cop_heiz/state\","
                << "\"icon\":\"mdi:radiator\","
                << "\"state_class\":\"measurement\","
                << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
        std::string payloadStr = payload.str();
        id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    }
    
    // COP Gesamt
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/cop_gesamt/config";
        std::ostringstream payload;
        payload << "{\"name\":\"COP Gesamt\","
                << "\"unique_id\":\"stiebel_cop_gesamt\","
                << "\"state_topic\":\"heatingpump/calculated/cop_gesamt/state\","
                << "\"icon\":\"mdi:chart-line\","
                << "\"state_class\":\"measurement\","
                << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
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
}

// Validation removed for simplification - all values are published as-is
// Invalid values like -255, -32768, etc. will be visible in Home Assistant
// Users can filter these in HA automations/dashboards if needed

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



void updateSensor(uint32_t can_id, const ElsterIndex *ei, const std::string &value)
{
    const CanMember &cm = lookupCanMember(can_id);
    
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
    
    // Check if discovery is needed (fast check only - don't publish yet)
    std::string uid = getOrCreateUID(can_id, ei->Name);
    bool needsDiscovery = (discoveredSignals.find(uid) == discoveredSignals.end());
    
    if (needsDiscovery) {
        // Mark as discovered to avoid repeated checks
        discoveredSignals.insert(uid);
        
        // Schedule discovery publishing for next loop iteration (non-blocking)
        // Discovery will be published via interval component or deferred call
        // For now, publish immediately but this could be optimized further
        publishMqttDiscovery(can_id, ei);
    }
    
    // Publish state immediately (this is fast - just MQTT publish)
    publishMqttState(can_id, ei, publishValue);
    
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
}

// Timeout tracking removed for simplification
// Signals that don't respond will simply not update in Home Assistant

// Process signal request table with frequency-based scheduling
void processSignalRequests() {
    unsigned long now = millis();
    
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

void identifyCanMembers()
{
    ESP_LOGI("identifyCanMembers()", "Identifying CAN Members...");
    readSignal(&CanMembers[cm_heizmodul], GetElsterIndex("GERAETE_ID"));

    ESP_LOGI("identifyCanMembers()", "Identified CAN Members!");
    return;
}

void processAndUpdate(uint32_t can_id, std::vector<uint8_t> msg)
{
 
    std::string value;
    const ElsterIndex *ei = processCanMessage(msg, can_id, value);

    // Skip permanently blacklisted signals
    if (isPermanentlyBlacklisted(ei->Name))
    {
        return; // Reject before lookup, parsing, formatting, logging
    }

    updateSensor(can_id, ei, value);
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


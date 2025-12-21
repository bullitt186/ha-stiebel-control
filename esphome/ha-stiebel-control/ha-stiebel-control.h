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

// Signal configuration structure for MQTT auto-discovery
typedef struct {
    const char* namePattern;     // Pattern to match signal name (use * for wildcard)
    const char* deviceClass;     // HA device class (temperature, energy, power, etc.)
    const char* unit;            // Unit of measurement
    const char* icon;            // MDI icon
    const char* stateClass;      // measurement, total, total_increasing
} SignalConfig;

// Configurable signal mapping table - EDIT THIS to customize sensors
static const SignalConfig signalMappings[] = {
    
    // Temperature sensors
    {"TEMP", "temperature", "°C", "mdi:thermometer", "measurement"},
    
    // Energy sensors
    {"KWH", "energy", "kWh", "mdi:lightning-bolt", "total_increasing"},
    {"MWH", "energy", "MWh", "mdi:lightning-bolt", "total_increasing"},
    {"WH", "energy", "Wh", "mdi:lightning-bolt", "total_increasing"},

        // Date/Time components (must be before wildcard patterns that could match)
    {"JAHR", "", "", "mdi:calendar", "measurement"},
    {"MONAT", "", "", "mdi:calendar", "measurement"},
    {"TAG", "", "", "mdi:calendar", "measurement"},
    {"STUNDE", "", "", "mdi:clock", "measurement"},
    {"MINUTE", "", "", "mdi:clock", "measurement"},
    {"SEKUNDE", "", "", "mdi:clock", "measurement"},

    // Power sensors
    {"*LEISTUNG*", "power", "W", "mdi:flash", "measurement"},
    
    // Pressure sensors
    {"DRUCK", "pressure", "bar", "mdi:gauge", "measurement"},
    
    // Flow/Volume sensors
    {"*VOLUMENSTROM*", "volume_flow_rate", "l/min", "mdi:pump", "measurement"},
    {"*DURCHFLUSS*", "volume_flow_rate", "l/min", "mdi:water-pump", "measurement"},
    {"DURCHFLUSSMENGE*", "volume", "l", "mdi:gauge", "total_increasing"},
    
    // Electrical sensors
    {"*SPANNUNG*", "voltage", "V", "mdi:sine-wave", "measurement"},
    {"*STROM*", "current", "A", "mdi:current-ac", "measurement"},
    {"*FREQUENZ*", "frequency", "Hz", "mdi:sine-wave", "measurement"},
    
    // Speed/RPM sensors
    {"DREHZAHL", "frequency", "rpm", "mdi:fan", "measurement"},
    
    // Humidity sensor
    {"FEUCHTE*", "humidity", "%", "mdi:water-percent", "measurement"},
    
    // Duration/Time sensors
    {"ZEIT", "duration", "min", "mdi:clock", "measurement"},
    {"DAUER", "duration", "min", "mdi:timer", "measurement"},
    {"LZ", "duration", "h", "mdi:timer", "total_increasing"},
    {"STILLSTANDZEIT*", "duration", "h", "mdi:timer-off", "total_increasing"},
    {"*ZEIT*", "duration", "min", "mdi:clock", "measurement"},
    {"*DAUER*", "duration", "min", "mdi:timer", "measurement"},
    
    // Percentage sensors
    {"MODGRAD*", "power_factor", "%", "mdi:percent", "measurement"},
    
    // Version/Config (no unit)
    {"SOFTWARE_VERSION", "", "", "mdi:application-cog", ""},
    {"SOFTWARE_NUMMER", "", "", "mdi:application-cog", ""},
    {"GERAETE_ID", "", "", "mdi:identifier", ""},
    {"FIRMWARE*", "", "", "mdi:chip", ""},
    
    // Status indicators
    {"*STATUS*", "", "", "mdi:information", "measurement"},
    {"*SPERRE*", "", "", "mdi:lock", ""},
    {"*PUMPE*", "", "", "mdi:pump", ""},
    {"*BRENNER*", "", "", "mdi:fire", ""},
    {"*MISCHER*", "", "", "mdi:valve", ""},
    {"*VENTIL*", "", "", "mdi:valve", ""},
    {"*RELAIS*", "", "", "mdi:electric-switch", ""},
    {"VERDICHTER*", "", "", "mdi:air-conditioner", ""},
    
    // Cooling/Heating mode indicators
    {"*KUEHLUNG*", "", "", "mdi:snowflake", ""},
    {"*HEIZ*", "", "", "mdi:radiator", ""},
    {"*BETRIEB*", "", "", "mdi:cog", ""},
    
    // Default fallback (must be last)
    {"*", "", "", "mdi:flash", "measurement"}
};

// ============================================================================
// RUNTIME STATE TRACKING
// ============================================================================

// Track which signals have been discovered
static std::set<std::string> discoveredSignals;

// Track signals that returned invalid values (blacklisted until reboot)
static std::set<std::string> blacklistedSignals;

// Track consecutive invalid value counts per signal (reset on valid value)
static std::unordered_map<std::string, int> invalidSignalCounts;

// Track pending requests with timestamps (to detect no-response)
static std::unordered_map<std::string, unsigned long> pendingRequests;

// Track consecutive no-response counts per signal
static std::unordered_map<std::string, int> noResponseCounts;

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

// Pattern matching cache to avoid repeated expandSignalName() and matchesPattern() calls
static std::unordered_map<std::string, const SignalConfig*> signalConfigCache;

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

// Check if signal is in permanent blacklist
// Only checks signal name (after underscore in "MEMBER_SIGNAL" format)
inline bool isPermanentlyBlacklisted(const char* signalName) {
    for (size_t i = 0; i < sizeof(PERMANENT_BLACKLIST) / sizeof(PERMANENT_BLACKLIST[0]); i++) {
        if (strcmp(signalName, PERMANENT_BLACKLIST[i]) == 0) {
            return true;
        }
    }
    return false;
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

    // Only log non-blacklisted signals to reduce blocking during CAN bursts
    // Blacklisted signals are already filtered in processAndUpdate() early check
    if (!isPermanentlyBlacklisted(ei->Name))
    {
        ESP_LOGI("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(%s)", cm.Name, cm.CanId, ei->Name, charValue, ElsterTypeStr[ei->Type]);
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

    // Mark request as pending with current timestamp
    std::string key = std::string(cm->Name) + "_" + ei->Name;
    pendingRequests[key] = millis();

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

// Publish MQTT discovery for calculated date sensor
void publishDateDiscovery(bool forceRepublish = false) {
    static bool discoveryPublished = false;
    if (discoveryPublished && !forceRepublish) return;
    if (forceRepublish) discoveryPublished = false;
    
    const char* discoveryTopic = "homeassistant/sensor/heatingpump/calculated_date/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"Datum\","
            << "\"unique_id\":\"stiebel_calculated_date\","
            << "\"state_topic\":\"heatingpump/calculated/date/state\","
            << "\"icon\":\"mdi:calendar\","
            << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for calculated date sensor");
}

void publishDate()
{
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
    
    // Publish discovery first
    publishDateDiscovery();
    
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

// Publish MQTT discovery for calculated time sensor
void publishTimeDiscovery(bool forceRepublish = false) {
    static bool discoveryPublished = false;
    if (discoveryPublished && !forceRepublish) return;
    if (forceRepublish) discoveryPublished = false;
    
    const char* discoveryTopic = "homeassistant/sensor/heatingpump/calculated_time/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"Zeit\","
            << "\"unique_id\":\"stiebel_calculated_time\","
            << "\"state_topic\":\"heatingpump/calculated/time/state\","
            << "\"icon\":\"mdi:clock-outline\","
            << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for calculated time sensor");
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
    
    // Publish discovery first
    publishTimeDiscovery();
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/time/state";
    id(mqtt_client).publish(stateTopic, zeit.c_str(), zeit.length(), 0, true);
    ESP_LOGI("CALC", "Published time: %s (Stunde=%d, Minute=%d, Sekunde=%d)", zeit.c_str(), istunde, iminute, isekunde);
}

// Publish MQTT discovery for calculated Betriebsart sensor
void publishBetriebsartDiscovery(bool forceRepublish = false) {
    static bool discoveryPublished = false;
    if (discoveryPublished && !forceRepublish) return;
    if (forceRepublish) discoveryPublished = false;
    
    const char* discoveryTopic = "homeassistant/sensor/heatingpump/calculated_betriebsart/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"Betriebsart\","
            << "\"unique_id\":\"stiebel_calculated_betriebsart\","
            << "\"state_topic\":\"heatingpump/calculated/betriebsart/state\","
            << "\"icon\":\"mdi:cog\","
            << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for calculated Betriebsart sensor");
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
    
    // Publish discovery first
    publishBetriebsartDiscovery();
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/betriebsart/state";
    id(mqtt_client).publish(stateTopic, betriebsart.c_str(), betriebsart.length(), 0, true);
    // ESP_LOGD("CALC", "Published Betriebsart: %s (SOMMERBETRIEB=%s)", betriebsart.c_str(), sommerBetriebValue.c_str());
}

// Publish MQTT discovery for calculated Delta T continuous sensor
void publishDeltaTContinuousDiscovery(bool forceRepublish = false) {
    static bool discoveryPublished = false;
    if (discoveryPublished && !forceRepublish) return;
    if (forceRepublish) discoveryPublished = false;
    
    const char* discoveryTopic = "homeassistant/sensor/heatingpump/calculated_delta_t_continuous/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"Delta T WP (kontinuierlich)\","
            << "\"unique_id\":\"stiebel_calculated_delta_t_continuous\","
            << "\"state_topic\":\"heatingpump/calculated/delta_t_continuous/state\","
            << "\"unit_of_measurement\":\"K\","
            << "\"device_class\":\"temperature\","
            << "\"state_class\":\"measurement\","
            << "\"icon\":\"mdi:thermometer\","
            << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for Delta T continuous sensor");
}

void publishDeltaTContinuous()
{
    // Check if both temperature values are valid
    if (std::isnan(lastWpVorlaufIst) || std::isnan(lastRuecklaufIstTemp)) {
        return; // Silently skip if values not ready
    }
    
    // Validate temperature range (sanity check)
    if (lastWpVorlaufIst < -50 || lastRuecklaufIstTemp < -50) {
        return; // Silently skip invalid ranges
    }
    
    float deltaT = lastWpVorlaufIst - lastRuecklaufIstTemp;
    
    // Publish discovery first (but only once - already cached)
    publishDeltaTContinuousDiscovery();
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = "heatingpump/calculated/delta_t_continuous/state";
    id(mqtt_client).publish(stateTopic, value, strlen(value), 0, true);
}

// Publish MQTT discovery for calculated Delta T running sensor
void publishDeltaTRunningDiscovery(bool forceRepublish = false) {
    static bool discoveryPublished = false;
    if (discoveryPublished && !forceRepublish) return;
    if (forceRepublish) discoveryPublished = false;
    
    const char* discoveryTopic = "homeassistant/sensor/heatingpump/calculated_delta_t_running/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"Delta T WP (nur bei Verdichter an)\","
            << "\"unique_id\":\"stiebel_calculated_delta_t_running\","
            << "\"state_topic\":\"heatingpump/calculated/delta_t_running/state\","
            << "\"unit_of_measurement\":\"K\","
            << "\"device_class\":\"temperature\","
            << "\"state_class\":\"measurement\","
            << "\"icon\":\"mdi:thermometer-chevron-up\","
            << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for Delta T running sensor");
}

void publishDeltaTRunning()
{
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
    
    // Publish discovery first (but only once - already cached)
    publishDeltaTRunningDiscovery();
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = "heatingpump/calculated/delta_t_running/state";
    id(mqtt_client).publish(stateTopic, value, strlen(value), 0, true);
}

// Publish MQTT discovery for calculated Compressor Active binary sensor
void publishCompressorActiveDiscovery(bool forceRepublish = false) {
    static bool discoveryPublished = false;
    if (discoveryPublished && !forceRepublish) return;
    if (forceRepublish) discoveryPublished = false;
    
    const char* discoveryTopic = "homeassistant/binary_sensor/heatingpump/calculated_compressor_active/config";
    
    std::ostringstream payload;
    payload << "{\"name\":\"WP Verdichter aktiv\","
            << "\"unique_id\":\"stiebel_calculated_compressor_active\","
            << "\"state_topic\":\"heatingpump/calculated/compressor_active/state\","
            << "\"device_class\":\"running\","
            << "\"payload_on\":\"on\","
            << "\"payload_off\":\"off\","
            << "\"icon\":\"mdi:engine\","
            << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    discoveryPublished = true;
    ESP_LOGI("MQTT", "Discovery published for Compressor Active sensor");
}

void publishCompressorActive()
{
    // Check if compressor value is valid
    if (std::isnan(lastVerdichterValue)) {
        return; // Silently skip if value invalid
    }
    
    // Compressor is active if value > 2
    bool isActive = (lastVerdichterValue > 2.0);
    
    // Publish discovery first (but only once - already cached)
    publishCompressorActiveDiscovery();
    
    // Publish state to MQTT
    const char* state = isActive ? "on" : "off";
    const char* stateTopic = "heatingpump/calculated/compressor_active/state";
    id(mqtt_client).publish(stateTopic, state, strlen(state), 0, true);
    // Note: Delta T running is published separately by scheduler
}

// Helper: Check if pattern appears anywhere in text (case-insensitive substring match)
bool matchesPattern(const char* text, const char* pattern) {
    std::string t(text);
    std::string p(pattern);
    
    // Convert to uppercase for case-insensitive matching
    std::transform(t.begin(), t.end(), t.begin(), ::toupper);
    std::transform(p.begin(), p.end(), p.begin(), ::toupper);
    
    // Check if pattern appears anywhere in text
    return t.find(p) != std::string::npos;
}

// Abbreviation list sorted by length (longest first)
static const struct { const char* abbrev; const char* full; } abbrevList[] = {
    {"AUFNAHMELEISTUNG", "Aufnahmeleistung"},  // 16 chars
    {"LUEFTUNGSSTUFE", "Lüftungsstufe"},  // 14 chars
    {"LEISTUNGSZWANG", "Leistungszwang"},   // 14 chars
    {"FEHLERMELDUNG", "Fehlermeldung"},// 13 chars
    {"VOLUMENSTROM", "Volumenstrom"},  // 12 chars
    {"QUELLENPUMPE", "Quellenpumpe"},  // 12 chars
    {"STUETZSTELLE", "Stützstelle"},   // 12 chars
    {"HILFSKESSEL", "Hilfskessel"},    // 11 chars
    {"BETRIEBSART", "Betriebsart"},    // 11 chars
    {"VERDAMPFER", "Verdampfer"},      // 10 chars
    {"VERDICHTER", "Verdichter"},      // 10 chars
    {"DURCHFLUSS", "Durchfluss"},      // 10 chars
    {"TEMPERATUR", "Temperatur"},      // 10 chars
    {"TEMPORALE", "Temporale"},        // 9 chars
    {"RUECKLAUF", "Rücklauf"},         // 9 chars
    {"LAUFZEIT", "Laufzeit"},          // 8 chars
    {"EINSTELL", "Einstellung"},       // 8 chars
    {"LEISTUNG", "Leistung"},          // 8 chars
    {"KUEHLUNG", "Kühlung"},           // 8 chars
    {"BIVALENT", "Bivalent"},          // 8 chars
    {"PARALLEL", "Parallel"},          // 8 chars
    {"FREQUENZ", "Frequenz"},          // 8 chars
    {"DREHZAHL", "Drehzahl"},          // 8 chars
    {"SPEICHER", "Speicher"},          // 8 chars
    {"SPANNUNG", "Spannung"},          // 8 chars
    {"VORLAUF", "Vorlauf"},            // 7 chars
    {"SAMMLER", "Sammler"},            // 7 chars
    {"BETRIEB", "Betrieb"},            // 7 chars
    {"HEIZUNG", "Heizung"},            // 7 chars
    {"ERTRAG", "Ertrag"},              // 6 chars
    {"AUSSEN", "Außen"},               // 6 chars
    {"MINUTE", "Minute"},               // 6 chars
    {"SOCKEL", "Sockel"},              // 6 chars
    {"KESSEL", "Kessel"},              // 6 chars
    {"DAUER", "Dauer"},                // 5 chars
    {"DRUCK", "Druck"},                // 5 chars
    {"STROM", "Strom"},                // 5 chars
    {"LUEFT", "Lüftung"},              // 5 chars
    {"PUMPE", "Pumpe"},                // 5 chars
    {"VERD", "Verdichter"},            // 4 chars
    {"TEMP", "Temperatur"},            // 4 chars
    {"HEIZ", "Heizung"},               // 4 chars
    {"RAUM", "Raum"},                  // 4 chars
    {"SOLL", "Soll"},                  // 4 chars
    {"MAX", "Maximum"},                // 3 chars
    {"MIN", "Minimum"},                // 3 chars
    {"SUM", "Summe"},                  // 3 chars
    {"TAG", "Tag"},                    // 3 chars
    {"IST", "Ist"},                    // 3 chars
    {"FKT", "Funktion"},               // 3 chars
    {"HZG", "Heizung"},                // 3 chars
    {"WW", "Warmwasser"},              // 2 chars
    {"WP", "Wärmepumpe"},              // 2 chars
    {"EL", "Elektrisch"},              // 2 chars
    {"LZ", "Laufzeit"}                 // 2 chars

};

// Recursive helper to split a signal name fragment
inline std::string splitFragment(const std::string& fragment) {
    // Stop if fragment is too short
    if (fragment.length() <= 1) {
        return fragment;
    }
    
    // Convert to uppercase for case-insensitive search
    std::string upperFragment = fragment;
    std::transform(upperFragment.begin(), upperFragment.end(), upperFragment.begin(), ::toupper);
    
    // Try each abbreviation (longest first)
    for (size_t i = 0; i < sizeof(abbrevList)/sizeof(abbrevList[0]); i++) {
        const char* abbrev = abbrevList[i].abbrev;
        size_t abbrevLen = strlen(abbrev);
        
        // Search for this abbreviation in the uppercase fragment
        size_t pos = upperFragment.find(abbrev);
        if (pos != std::string::npos) {
            // Found! Split into left, match, right (use original fragment for extraction)
            std::string left = fragment.substr(0, pos);
            std::string match = fragment.substr(pos, abbrevLen);  // Preserve original case
            std::string right = fragment.substr(pos + abbrevLen);
            
            // Recursively process left and right parts
            std::string processedLeft = splitFragment(left);
            std::string processedRight = splitFragment(right);
            
            // Combine with spaces
            std::string result;
            if (!processedLeft.empty()) {
                result = processedLeft + " ";
            }
            result += match;
            if (!processedRight.empty()) {
                result += " " + processedRight;
            }
            
            return result;
        }
    }
    
    // No match found, return fragment as-is
    return fragment;
}

// Expand signal name by splitting concatenated abbreviations and adding TEMP suffix
// E.g., "WPVORLAUFIST" -> "WP VORLAUF IST TEMP"
inline std::string expandSignalName(const char* signalName) {
    std::string name(signalName);
    
    // Step 1: Replace underscores with spaces
    std::replace(name.begin(), name.end(), '_', ' ');
    
    // Step 2-4: Process each space-separated token
    std::istringstream tokenStream(name);
    std::string token;
    std::ostringstream result;
    bool first = true;
    
    while (tokenStream >> token) {
        if (!first) result << " ";
        first = false;
        
        // Recursively split this token
        result << splitFragment(token);
    }
    
    name = result.str();
    
    // Step 5: Trim and clean up excess whitespace
    size_t start = name.find_first_not_of(" ");
    size_t end = name.find_last_not_of(" ");
    if (start != std::string::npos) {
        name = name.substr(start, end - start + 1);
    } else {
        name = "";
    }
    
    // Reduce multiple spaces to single space
    size_t spacePos = 0;
    while ((spacePos = name.find("  ", spacePos)) != std::string::npos) {
        name.replace(spacePos, 2, " ");
    }
    
    // Add "TEMP" suffix if name ends with "SOLL" or "IST" (temperature indicators)
    if (name.length() >= 4) {
        std::string upperName = name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
        
        if (upperName.substr(upperName.length() - 4) == "SOLL" || 
            upperName.substr(upperName.length() - 3) == "IST") {
            // Check if TEMP is not already present
            if (upperName.find("TEMP") == std::string::npos) {
                name += " TEMP";
            }
        }
    }
    
    return name;
}

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

// Get signal configuration based on expanded name and type
inline const SignalConfig* getSignalConfig(const char* signalName, ElsterType type) {
    // Create cache key from signal name and type
    char cacheKey[256];
    snprintf(cacheKey, sizeof(cacheKey), "%s:%d", signalName, (int)type);
    
    // Check cache first (O(1) lookup)
    auto cacheIt = signalConfigCache.find(cacheKey);
    if (cacheIt != signalConfigCache.end()) {
        return cacheIt->second;
    }
    
    // Cache miss - do expensive pattern matching
    // First expand the signal name
    std::string expandedName = expandSignalName(signalName);
    
    // Convert to uppercase for pattern matching
    std::string upperExpanded = expandedName;
    std::transform(upperExpanded.begin(), upperExpanded.end(), upperExpanded.begin(), ::toupper);
    
    // Try to find matching pattern against expanded name
    const SignalConfig* result = nullptr;
    for (const auto& config : signalMappings) {
        if (matchesPattern(upperExpanded.c_str(), config.namePattern)) {
            ESP_LOGD("PATTERN", "Signal '%s' (expanded: '%s') matched pattern '%s'", 
                     signalName, expandedName.c_str(), config.namePattern);
            result = &config;
            break;
        }
    }
    
    // If no match, use default (last entry)
    if (result == nullptr) {
        ESP_LOGW("PATTERN", "Signal '%s' (expanded: '%s') using default pattern (no match found)", 
                 signalName, expandedName.c_str());
        result = &signalMappings[sizeof(signalMappings)/sizeof(SignalConfig) - 1];
    }
    
    // Cache the result before returning
    signalConfigCache[cacheKey] = result;
    return result;
}

// Generate friendly name from signal name (kept in German as requested)
inline std::string getFriendlyName(const char* signalName, const char* canMemberName) {
    // Step 1: Expand signal name (split concatenated words, add TEMP suffix)
    std::string name = expandSignalName(signalName);
    
    // Step 2: Apply abbreviation expansions to the separated words
    for (size_t i = 0; i < sizeof(abbrevList)/sizeof(abbrevList[0]); i++) {
        const char* abbrev = abbrevList[i].abbrev;
        const char* full = abbrevList[i].full;
        const size_t abbrevLen = strlen(abbrev);
        const size_t fullLen = strlen(full);
        size_t pos = 0;
        
        // Replace whole-word matches only (surrounded by spaces or start/end)
        while ((pos = name.find(abbrev, pos)) != std::string::npos) {
            bool hasBefore = (pos == 0 || name[pos - 1] == ' ');
            bool hasAfter = (pos + abbrevLen >= name.length() || name[pos + abbrevLen] == ' ');
            
            if (hasBefore && hasAfter) {
                name.replace(pos, abbrevLen, full);
                pos += fullLen;
            } else {
                pos += abbrevLen;
            }
        }
    }
    
    // Step 3: Convert to title case
    bool capitalizeNext = true;
    for (size_t i = 0; i < name.length(); i++) {
        if (capitalizeNext && std::isalpha(name[i])) {
            name[i] = ::toupper(name[i]);
            capitalizeNext = false;
        } else {
            name[i] = ::tolower(name[i]);
            if (name[i] == ' ') capitalizeNext = true;
        }
    }
    
    // Step 4: Convert German character sequences to proper umlauts
    // This handles any remaining cases where abbreviations weren't fully replaced
    size_t pos = 0;
    while ((pos = name.find("Ae", pos)) != std::string::npos) {
        name.replace(pos, 2, "Ä");
        pos += 2; // UTF-8 Ä is 2 bytes
    }
    pos = 0;
    while ((pos = name.find("Oe", pos)) != std::string::npos) {
        name.replace(pos, 2, "Ö");
        pos += 2;
    }
    pos = 0;
    while ((pos = name.find("Ue", pos)) != std::string::npos) {
        name.replace(pos, 2, "Ü");
        pos += 2;
    }
    pos = 0;
    while ((pos = name.find("ae", pos)) != std::string::npos) {
        name.replace(pos, 2, "ä");
        pos += 2;
    }
    pos = 0;
    while ((pos = name.find("oe", pos)) != std::string::npos) {
        name.replace(pos, 2, "ö");
        pos += 2;
    }
    pos = 0;
    while ((pos = name.find("ue", pos)) != std::string::npos) {
        name.replace(pos, 2, "ü");
        pos += 2;
    }
    
    return name;
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
    const SignalConfig* config = getSignalConfig(ei->Name, (ElsterType)ei->Type);
    
    // Get cached UID (avoids repeated string operations)
    std::string uid = getOrCreateUID(can_id, ei->Name);
    
    // Check if already discovered
    if (discoveredSignals.find(uid) != discoveredSignals.end()) {
        return; // Already published discovery
    }
    discoveredSignals.insert(uid);
    
    // Determine component type based on ElsterType
    const char* component = "sensor";
    if (ei->Type == et_bool || ei->Type == et_little_bool) {
        component = "binary_sensor";
    }
    
    // Build discovery topic
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/%s/heatingpump/%s/config", component, uid.c_str());
    
    // Build state topic
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    
    // Generate friendly name
    std::string friendlyName = getFriendlyName(ei->Name, cm.Name);
    
    // Build JSON payload efficiently using ostringstream
    std::ostringstream payload;
    payload << "{\"name\":\"" << friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"availability_topic\":\"heatingpump/status\"";
    
    // For binary sensors, specify payload values
    if (ei->Type == et_bool || ei->Type == et_little_bool) {
        payload << ",\"payload_on\":\"on\","
                << "\"payload_off\":\"off\"";
    }
    
    // Add optional fields only if specified
    if (config->deviceClass[0] != '\0') {
        payload << ",\"device_class\":\"" << config->deviceClass << "\"";
    }
    if (config->unit[0] != '\0') {
        payload << ",\"unit_of_measurement\":\"" << config->unit << "\"";
    }
    // State class only for numeric sensors (not binary, text, enum, time, date, or ID types)
    if (config->stateClass[0] != '\0') {
        bool isNumericType = (ei->Type == et_dec_val || 
                              ei->Type == et_cent_val || 
                              ei->Type == et_mil_val || 
                              ei->Type == et_byte ||
                              ei->Type == et_double_val ||
                              ei->Type == et_triple_val ||
                              ei->Type == et_little_endian);
        if (isNumericType) {
            payload << ",\"state_class\":\"" << config->stateClass << "\"";
        }
    }
    if (config->icon[0] != '\0') {
        payload << ",\"icon\":\"" << config->icon << "\"";
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
    
    ESP_LOGI("MQTT", "Discovery published for %s", friendlyName.c_str());
}

// Republish all MQTT discoveries (for periodic refresh)
void republishAllDiscoveries() {
    ESP_LOGI("MQTT", "Republishing all MQTT discoveries (%d signals)", discoveredSignals.size());
    
    // Create a copy of discovered signals to iterate over
    std::set<std::string> signalsToRepublish = discoveredSignals;
    
    // Clear the set so signals can be republished
    discoveredSignals.clear();
    
    // Force republish by reading all known signals
    // This will trigger processAndUpdate which calls publishMqttDiscovery
    for (const auto& signal : signalsToRepublish) {
        ESP_LOGD("MQTT", "Marked for republish: %s", signal.c_str());
    }
    
    // Reset calculated sensor discovery flags and republish
    ESP_LOGI("MQTT", "Republishing calculated sensor discoveries");
    publishDateDiscovery(true);
    publishTimeDiscovery(true);
    publishBetriebsartDiscovery(true);
    publishDeltaTContinuousDiscovery(true);
    publishDeltaTRunningDiscovery(true);
    publishCompressorActiveDiscovery(true);
    
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

// Publish blacklist diagnostics sensors to Home Assistant
void publishBlacklistDiagnostics() {
    // 1. Blacklisted signals sensor (permanent blocks)
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/blacklisted_signals/config";
        const char* stateTopic = "heatingpump/diagnostics/blacklisted_signals/state";
        const char* attributesTopic = "heatingpump/diagnostics/blacklisted_signals/attributes";
        
        // Discovery message
        std::ostringstream discovery;
        discovery << "{\"name\":\"Blacklisted Signals\","
                  << "\"unique_id\":\"stiebel_blacklisted_signals\","
                  << "\"state_topic\":\"" << stateTopic << "\","
                  << "\"json_attributes_topic\":\"" << attributesTopic << "\","
                  << "\"icon\":\"mdi:block-helper\","
                  << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
                  << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                  << "\"manufacturer\":\"Stiebel Eltron\"}}";
        
        std::string discoveryStr = discovery.str();
        id(mqtt_client).publish(discoveryTopic, discoveryStr.c_str(), discoveryStr.length(), 0, true);
        
        // State: count of blacklisted signals
        std::string stateStr = std::to_string(blacklistedSignals.size());
        id(mqtt_client).publish(stateTopic, stateStr.c_str(), stateStr.length(), 0, true);
        
        // Attributes: list of blacklisted signals with reason
        std::ostringstream attributes;
        attributes << "{\"signals\":[";
        bool first = true;
        for (const auto& signal : blacklistedSignals) {
            if (!first) attributes << ",";
            first = false;
            
            // Extract member and signal name
            size_t pos = signal.find('_');
            std::string member = (pos != std::string::npos) ? signal.substr(0, pos) : "unknown";
            std::string signalName = (pos != std::string::npos) ? signal.substr(pos + 1) : signal;
            
            // Determine reason (check both maps)
            std::string reason = "unknown";
            if (invalidSignalCounts.find(signal) != invalidSignalCounts.end() && 
                invalidSignalCounts.at(signal) >= 3) {
                reason = "invalid_values";
            } else if (noResponseCounts.find(signal) != noResponseCounts.end() && 
                       noResponseCounts.at(signal) >= 3) {
                reason = "no_response";
            }
            
            attributes << "{\"key\":\"" << signal << "\","
                      << "\"member\":\"" << member << "\","
                      << "\"signal\":\"" << signalName << "\","
                      << "\"reason\":\"" << reason << "\"}";
        }
        attributes << "],\"count\":" << blacklistedSignals.size() << "}";
        
        std::string attributesStr = attributes.str();
        id(mqtt_client).publish(attributesTopic, attributesStr.c_str(), attributesStr.length(), 0, true);
    }
    
    // 2. Invalid value counts sensor
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/invalid_value_signals/config";
        const char* stateTopic = "heatingpump/diagnostics/invalid_value_signals/state";
        const char* attributesTopic = "heatingpump/diagnostics/invalid_value_signals/attributes";
        
        // Discovery message
        std::ostringstream discovery;
        discovery << "{\"name\":\"Invalid Value Signals\","
                  << "\"unique_id\":\"stiebel_invalid_value_signals\","
                  << "\"state_topic\":\"" << stateTopic << "\","
                  << "\"json_attributes_topic\":\"" << attributesTopic << "\","
                  << "\"icon\":\"mdi:alert-circle\","
                  << "\"device\":{\"identifiers\":[\"stiebel_eltron_wpl13e\"],"
                  << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                  << "\"manufacturer\":\"Stiebel Eltron\"}}";
        
        std::string discoveryStr = discovery.str();
        id(mqtt_client).publish(discoveryTopic, discoveryStr.c_str(), discoveryStr.length(), 0, true);
        
        // State: count of signals with invalid values
        std::string stateStr = std::to_string(invalidSignalCounts.size());
        id(mqtt_client).publish(stateTopic, stateStr.c_str(), stateStr.length(), 0, true);
        
        // Attributes: signals with invalid value counts
        std::ostringstream attributes;
        attributes << "{\"signals\":[";
        bool first = true;
        for (const auto& entry : invalidSignalCounts) {
            if (!first) attributes << ",";
            first = false;
            
            // Extract member and signal name
            size_t pos = entry.first.find('_');
            std::string member = (pos != std::string::npos) ? entry.first.substr(0, pos) : "unknown";
            std::string signalName = (pos != std::string::npos) ? entry.first.substr(pos + 1) : entry.first;
            
            attributes << "{\"key\":\"" << entry.first << "\","
                      << "\"member\":\"" << member << "\","
                      << "\"signal\":\"" << signalName << "\","
                      << "\"count\":" << entry.second << ","
                      << "\"status\":\"" << (entry.second >= BLACKLIST_INVALID_THRESHOLD ? "blacklisted" : "warning") << "\"}";
        }
        attributes << "],\"count\":" << invalidSignalCounts.size() << "}";
        
        std::string attributesStr = attributes.str();
        id(mqtt_client).publish(attributesTopic, attributesStr.c_str(), attributesStr.length(), 0, true);
    }
    
    // 3. No-response counts sensor
    {
        const char* discoveryTopic = "homeassistant/sensor/heatingpump/no_response_signals/config";
        const char* stateTopic = "heatingpump/diagnostics/no_response_signals/state";
        const char* attributesTopic = "heatingpump/diagnostics/no_response_signals/attributes";
        
        // Discovery message
        std::ostringstream discovery;
        discovery << "{\"name\":\"No Response Signals\","
                  << "\"unique_id\":\"stiebel_no_response_signals\","
                  << "\"state_topic\":\"" << stateTopic << "\","
                  << "\"json_attributes_topic\":\"" << attributesTopic << "\","
                  << "\"icon\":\"mdi:connection\","
                  << "\"device\":{\"identifiers\":[\"" << MAIN_DEVICE_ID << "\"],"
                  << "\"name\":\"" << MAIN_DEVICE_NAME << "\","
                  << "\"manufacturer\":\"Stiebel Eltron\"}}";
        
        std::string discoveryStr = discovery.str();
        id(mqtt_client).publish(discoveryTopic, discoveryStr.c_str(), discoveryStr.length(), 0, true);
        
        // State: count of signals with no responses
        std::string stateStr = std::to_string(noResponseCounts.size());
        id(mqtt_client).publish(stateTopic, stateStr.c_str(), stateStr.length(), 0, true);
        
        // Attributes: signals with no-response counts
        std::ostringstream attributes;
        attributes << "{\"signals\":[";
        bool first = true;
        for (const auto& entry : noResponseCounts) {
            if (!first) attributes << ",";
            first = false;
            
            // Extract member and signal name
            size_t pos = entry.first.find('_');
            std::string member = (pos != std::string::npos) ? entry.first.substr(0, pos) : "unknown";
            std::string signalName = (pos != std::string::npos) ? entry.first.substr(pos + 1) : entry.first;
            
            attributes << "{\"key\":\"" << entry.first << "\","
                      << "\"member\":\"" << member << "\","
                      << "\"signal\":\"" << signalName << "\","
                      << "\"count\":" << entry.second << ","
                      << "\"status\":\"" << (entry.second >= BLACKLIST_TIMEOUT_THRESHOLD ? "blacklisted" : "warning") << "\"}";
        }
        attributes << "],\"count\":" << noResponseCounts.size() << "}";
        
        std::string attributesStr = attributes.str();
        id(mqtt_client).publish(attributesTopic, attributesStr.c_str(), attributesStr.length(), 0, true);
    }
    
    ESP_LOGI("DIAGNOSTICS", "Published blacklist diagnostics: %d blacklisted, %d invalid, %d no-response",
             blacklistedSignals.size(), invalidSignalCounts.size(), noResponseCounts.size());
}

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

// Check if value is invalid/unsupported
bool isInvalidValue(const ElsterIndex *ei, const std::string &value) {
    if (value.empty()) return true;
    
    // Check for common invalid string values (but allow valid state strings)
    if (value == "SNA" || value == "---" || value == "N/A") return true;
    
    // Allow common valid state strings (case-insensitive check)
    std::string lowerValue = value;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
    if (lowerValue == "on" || lowerValue == "off" || 
        lowerValue == "ein" || lowerValue == "aus" ||
        lowerValue == "true" || lowerValue == "false" ||
        lowerValue == "yes" || lowerValue == "no" ||
        lowerValue == "ja" || lowerValue == "nein") {
        return false; // Valid state string
    }
    
    // Check numeric types for invalid values
    if (ei->Type == et_byte || ei->Type == et_cent_val || ei->Type == et_dec_val || 
        ei->Type == et_double_val || ei->Type == et_triple_val) {
        
        // Check if string contains valid numeric characters
        bool hasDigit = false;
        bool hasDecimal = false;
        for (size_t i = 0; i < value.length(); i++) {
            char c = value[i];
            if (c >= '0' && c <= '9') {
                hasDigit = true;
            } else if (c == '.') {
                hasDecimal = true;
            } else if (c != '-' && c != ' ' && c != '+') {
                return true; // Invalid character for number
            }
        }
        if (!hasDigit) return true; // No digits found
        
        // Check for exact invalid string values (more reliable than float comparison)
        if (value == "-255" || value == "-32768" || value == "32767" ||
            value == "-327.68" || value == "327.68" || value == "-327.67" || value == "327.67") {
            return true;
        }
        
        // Parse value and check for invalid ranges
        float fval = std::stof(value);
        if (fval < -300.0f || fval > 1000.0f) {
            return true;
        }
        
        // For values near common invalid markers, use tolerance check
        const float epsilon = 0.01f;
        if (std::abs(fval - (-255.0f)) < epsilon || 
            std::abs(fval - (-32768.0f)) < epsilon ||
            std::abs(fval - 32767.0f) < epsilon) {
            return true;
        }
    }
    
    return false;
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



void updateSensor(uint32_t can_id, const ElsterIndex *ei, const std::string &value)
{
    const CanMember &cm = lookupCanMember(can_id);
    
    // Early blacklist check - check signal name against permanent blacklist first (fastest)
    // This avoids string concatenation for blacklisted signals
    if (isPermanentlyBlacklisted(ei->Name)) {
        return; // Don't process permanently blacklisted signals
    }
    
    // Build key for dynamic blacklist check
    std::string key = std::string(cm.Name) + "_" + ei->Name;
    
    // Check dynamic blacklist (invalid signals)
    if (blacklistedSignals.find(key) != blacklistedSignals.end()) {
        return; // Don't process dynamically blacklisted signals
    }
    
    // Response received - remove from pending requests and reset no-response counter
    if (pendingRequests.find(key) != pendingRequests.end()) {
        pendingRequests.erase(key);
    }
    if (noResponseCounts.find(key) != noResponseCounts.end()) {
        noResponseCounts.erase(key);
    }
    
    // Check for invalid values and track consecutive failures
    if (isInvalidValue(ei, value)) {
        // Increment invalid count for this signal
        invalidSignalCounts[key]++;
        
        // Only blacklist after BLACKLIST_INVALID_THRESHOLD consecutive invalid values
        if (invalidSignalCounts[key] >= BLACKLIST_INVALID_THRESHOLD) {
#if BLACKLIST_ENABLED
            if (blacklistedSignals.find(key) == blacklistedSignals.end()) {
                blacklistedSignals.insert(key);
                ESP_LOGW("BLACKLIST", "Signal %s from %s returned %d consecutive invalid values (last: '%s') - blacklisted",
                         ei->Name, cm.Name, invalidSignalCounts[key], value.c_str());
                
                // Remove from Home Assistant by sending empty discovery message
                std::string uid = getOrCreateUID(can_id, ei->Name);
                
                const char* component = (ei->Type == et_bool || ei->Type == et_little_bool) ? "binary_sensor" : "sensor";
                char discoveryTopic[256];
                snprintf(discoveryTopic, sizeof(discoveryTopic), 
                         "homeassistant/%s/heatingpump/%s/config", component, uid.c_str());
                
                // Send empty payload to remove entity from HA
                id(mqtt_client).publish(discoveryTopic, "", 0, 0, true);
                
                // Clear retained state value
                char stateTopic[128];
                snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
                id(mqtt_client).publish(stateTopic, "", 0, 0, true);
                
                ESP_LOGI("BLACKLIST", "Removed discovery and state for %s from Home Assistant", uid.c_str());
                
                // Update diagnostics sensors
                publishBlacklistDiagnostics();
            }
#else
            ESP_LOGW("BLACKLIST", "Signal %s from %s returned %d consecutive invalid values (last: '%s') - blacklisting disabled",
                     ei->Name, cm.Name, invalidSignalCounts[key], value.c_str());
#endif
        } else {
            ESP_LOGD("BLACKLIST", "Signal %s from %s invalid (%d/%d): '%s'",
                     ei->Name, cm.Name, invalidSignalCounts[key], BLACKLIST_INVALID_THRESHOLD, value.c_str());
        }
        return; // Don't publish invalid values
    }
    
    // Valid value received - remove from blacklist if present
    if (blacklistedSignals.find(key) != blacklistedSignals.end()) {
        blacklistedSignals.erase(key);
        ESP_LOGI("BLACKLIST", "Signal %s from %s recovered with valid value '%s' - removed from blacklist",
                 ei->Name, cm.Name, value.c_str());
        
        // Clear discovery cache so it gets republished
        char uniqueId[128];
        snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm.Name, ei->Name);
        std::string uid(uniqueId);
        std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
        std::replace(uid.begin(), uid.end(), ' ', '_');
        discoveredSignals.erase(uid);
        
        // Update diagnostics sensors
        publishBlacklistDiagnostics();
    }
    
    // Reset invalid and no-response counters
    if (invalidSignalCounts.find(key) != invalidSignalCounts.end()) {
        invalidSignalCounts.erase(key);
    }
    if (noResponseCounts.find(key) != noResponseCounts.end()) {
        noResponseCounts.erase(key);
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
    publishMqttState(can_id, ei, value);
    
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

// Check for timed-out requests (no response received)
void checkPendingRequests() {
    unsigned long now = millis();
    
    // Create list of timed-out requests
    std::vector<std::string> timedOut;
    for (auto it = pendingRequests.begin(); it != pendingRequests.end(); ++it) {
        if (now - it->second > CAN_REQUEST_TIMEOUT_MS) {
            timedOut.push_back(it->first);
        }
    }
    
    // Process timed-out requests
    for (const auto& key : timedOut) {
        pendingRequests.erase(key);
        noResponseCounts[key]++;
        
        // Blacklist after N consecutive no-responses
        if (noResponseCounts[key] >= BLACKLIST_TIMEOUT_THRESHOLD) {
            if (blacklistedSignals.find(key) == blacklistedSignals.end()) {
                blacklistedSignals.insert(key);
                // Parse key to get member and signal name
                size_t underscorePos = key.find('_');
                if (underscorePos != std::string::npos) {
                    std::string memberName = key.substr(0, underscorePos);
                    std::string signalName = key.substr(underscorePos + 1);
                    ESP_LOGW("BLACKLIST", "Signal %s from %s: no response after %d attempts - blacklisted",
                             signalName.c_str(), memberName.c_str(), noResponseCounts[key]);
                    // Remove from Home Assistant
                    char uniqueId[128];
                    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s", key.c_str());
                    std::string uid(uniqueId);
                    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
                    char discoveryTopic[256];
                    snprintf(discoveryTopic, sizeof(discoveryTopic), 
                             "homeassistant/sensor/heatingpump/%s/config", uid.c_str());
                    id(mqtt_client).publish(discoveryTopic, "", 0, 0, true);
                    // Also try binary_sensor
                    snprintf(discoveryTopic, sizeof(discoveryTopic), 
                             "homeassistant/binary_sensor/heatingpump/%s/config", uid.c_str());
                    id(mqtt_client).publish(discoveryTopic, "", 0, 0, true);
                    // Clear retained state value
                    char stateTopic[128];
                    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", memberName.c_str(), signalName.c_str());
                    id(mqtt_client).publish(stateTopic, "", 0, 0, true);
                    // Update diagnostics sensors
                    publishBlacklistDiagnostics();
                }
            }
        } else {
            // Parse key for logging
            size_t underscorePos = key.find('_');
            if (underscorePos != std::string::npos) {
                std::string memberName = key.substr(0, underscorePos);
                std::string signalName = key.substr(underscorePos + 1);
                ESP_LOGD("NO_RESPONSE", "Signal %s from %s: no response (%d/3)",
                         signalName.c_str(), memberName.c_str(), noResponseCounts[key]);
            }
        }
    }
    
    if (!timedOut.empty()) {
        ESP_LOGI("NO_RESPONSE", "Detected %d timed-out requests", timedOut.size());
    }
}

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
                    if (blacklistedSignals.find(key) == blacklistedSignals.end()) {
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
                    } else {
                        // ESP_LOGD("REQUEST_MGR", "Skipping blacklisted signal: %s", key.c_str());
                        // Reschedule for retry later (for recovery)
                        unsigned long maxJitter = (intervalMs / 20);
                        if (maxJitter < 500) maxJitter = 500;
                        unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                        nextRequestTime[key] = now + intervalMs + randomDelay;
                    }
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
                if (blacklistedSignals.find(key) == blacklistedSignals.end()) {
                    readSignal(member, ei);
                    requestsSentThisIteration++;
                    
                    // Calculate next scheduled time with random offset (0 to 5% of interval)
                    // This keeps signals from synchronizing while staying close to target frequency
                    unsigned long maxJitter = (intervalMs / 20); // 5% of interval
                    if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
                    unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                    nextRequestTime[key] = now + intervalMs + randomDelay;
                    
                    // ESP_LOGD("REQUEST_MGR", "Sent %s, next in %lums", key.c_str(), intervalMs + randomDelay);
                } else {
                    // ESP_LOGD("REQUEST_MGR", "Skipping blacklisted signal: %s", key.c_str());
                    // Reschedule for retry later (for recovery)
                    unsigned long maxJitter = (intervalMs / 20);
                    if (maxJitter < 500) maxJitter = 500;
                    unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                    nextRequestTime[key] = now + intervalMs + randomDelay;
                }
            }
        }
        
        // Move to next signal in round-robin fashion (wrap around at end)
        currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
    }
    
    // Update starting position for next iteration
    signalProcessingStartIndex = currentIndex;
    
    if (requestsSentThisIteration > 0) {
        ESP_LOGV("REQUEST_MGR", "Sent %d requests this iteration", requestsSentThisIteration);
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
    // Early blacklist check - extract signal name and check before expensive processing
    if (msg.size() >= 7)
    {
        const ElsterIndex *ei_check;
        if (msg[2] == 0xfa)
        {
            ei_check = GetElsterIndex(msg[4] + (msg[3] << 8));
        }
        else
        {
            ei_check = GetElsterIndex(msg[2]);
        }
        
        // Skip permanently blacklisted signals immediately
        if (isPermanentlyBlacklisted(ei_check->Name))
        {
            return; // Reject before lookup, parsing, formatting, logging
        }
    }
    
    std::string value;
    const ElsterIndex *ei = processCanMessage(msg, can_id, value);
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


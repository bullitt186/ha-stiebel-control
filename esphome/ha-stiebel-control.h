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
#include "ElsterTable.h"
#include "KElsterTable.h"
#include <sstream>
#include <iomanip>
#include <set>
#include <map>

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
    {"*TEMP*", "temperature", "°C", "mdi:thermometer", "measurement"},
    {"VERDAMPFERTEMP", "temperature", "°C", "mdi:snowflake", "measurement"},
    {"HEISSGAS_TEMP", "temperature", "°C", "mdi:fire", "measurement"},
    {"ABGASTEMP", "temperature", "°C", "mdi:smoke", "measurement"},
    {"TAUPUNKT_TEMP", "temperature", "°C", "mdi:water-percent", "measurement"},
    
    // Energy sensors
    {"*ENERGIE*", "energy", "kWh", "mdi:lightning-bolt", "total_increasing"},
    {"*_KWH", "energy", "kWh", "mdi:lightning-bolt", "total_increasing"},
    {"*_MWH", "energy", "MWh", "mdi:lightning-bolt", "total_increasing"},
    {"*_WH", "energy", "Wh", "mdi:lightning-bolt", "total_increasing"},
    {"WAERMEERTRAG*", "energy", "kWh", "mdi:fire", "total_increasing"},
    {"EL_AUFNAHMELEISTUNG*", "energy", "kWh", "mdi:transmission-tower", "total_increasing"},
    {"*ERTRAG*", "energy", "kWh", "mdi:solar-power", "total_increasing"},
    
    // Power sensors
    {"*LEISTUNG*", "power", "W", "mdi:flash", "measurement"},
    
    // Pressure sensors
    {"*DRUCK*", "pressure", "bar", "mdi:gauge", "measurement"},
    {"WASSERDRUCK", "pressure", "bar", "mdi:water-pump", "measurement"},
    {"MASCHINENDRUCK", "pressure", "bar", "mdi:hydraulic-oil-level", "measurement"},
    
    // Flow/Volume sensors
    {"*VOLUMENSTROM*", "volume_flow_rate", "l/min", "mdi:pump", "measurement"},
    {"*DURCHFLUSS*", "volume_flow_rate", "l/min", "mdi:water-pump", "measurement"},
    {"DURCHFLUSSMENGE*", "volume", "l", "mdi:gauge", "total_increasing"},
    
    // Electrical sensors
    {"*SPANNUNG*", "voltage", "V", "mdi:sine-wave", "measurement"},
    {"*STROM*", "current", "A", "mdi:current-ac", "measurement"},
    {"*FREQUENZ*", "frequency", "Hz", "mdi:sine-wave", "measurement"},
    
    // Speed/RPM sensors
    {"*DREHZAHL*", "frequency", "rpm", "mdi:fan", "measurement"},
    {"GEBLAESEDREHZAHL", "frequency", "rpm", "mdi:fan", "measurement"},
    
    // Humidity sensor
    {"FEUCHTE*", "humidity", "%", "mdi:water-percent", "measurement"},
    
    // Duration/Time sensors
    {"LAUFZEIT*", "duration", "h", "mdi:timer", "total_increasing"},
    {"LZ_*", "duration", "h", "mdi:timer", "total_increasing"},
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

// Track which signals have been discovered
static std::set<std::string> discoveredSignals;

// Track signals that returned invalid values (blacklisted until reboot)
static std::set<std::string> blacklistedSignals;

// Track consecutive invalid value counts per signal (reset on valid value)
static std::map<std::string, int> invalidSignalCounts;

// Track pending requests with timestamps (to detect no-response)
static std::map<std::string, unsigned long> pendingRequests;

// Track consecutive no-response counts per signal
static std::map<std::string, int> noResponseCounts;

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

void publishDate()
{
    int ijahr = static_cast<int>(id(JAHR).state);
    std::string jahr;
    if (ijahr >= 0 && ijahr < 99)
    {
        jahr = (ijahr < 10) ? "0" + std::to_string(ijahr) : std::to_string(ijahr);
    }
    else
    {
        jahr = "00";
    }

    int imonat = static_cast<int>(id(MONAT).state);
    std::string monat;
    if (imonat > 0 && imonat <= 12)
    {
        monat = (imonat < 10) ? "0" + std::to_string(imonat) : std::to_string(imonat);
    }
    else
    {
        monat = "00";
    }

    int itag = static_cast<int>(id(TAG).state);
    std::string tag;
    if (itag > 0 && itag <= 31)
    {
        tag = (itag < 10) ? "0" + std::to_string(itag) : std::to_string(itag);
    }
    else
    {
        tag = "00";
    }

    std::string datum = "20" + jahr + "-" + monat + "-" + tag;
    id(DATUM).publish_state(datum);
}

std::string formatNumber(int number, int width)
{
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << number;
    return oss.str();
}

void publishTime()
{
    int istunde = (int)id(STUNDE).state;
    std::string stunde = formatNumber(istunde, 2);

    int iminute = (int)id(MINUTE).state;
    std::string minute = formatNumber(iminute, 2);

    int isekunde = (int)id(SEKUNDE).state;
    std::string sekunde = formatNumber(isekunde, 2);

    id(ZEIT).publish_state(stunde + ":" + minute + ":" + sekunde);
}

// Helper: Check if string matches pattern (supports * wildcard)
bool matchesPattern(const char* text, const char* pattern) {
    std::string t(text);
    std::string p(pattern);
    
    // Convert to uppercase for case-insensitive matching
    std::transform(t.begin(), t.end(), t.begin(), ::toupper);
    std::transform(p.begin(), p.end(), p.begin(), ::toupper);
    
    size_t pos = p.find('*');
    if (pos == std::string::npos) {
        return t == p; // No wildcard, exact match
    }
    
    // Check prefix and suffix
    std::string prefix = p.substr(0, pos);
    std::string suffix = p.substr(pos + 1);
    
    if (prefix.length() > 0 && t.find(prefix) != 0) return false;
    if (suffix.length() > 0 && t.rfind(suffix) != t.length() - suffix.length()) return false;
    
    return true;
}

// Get signal configuration based on name and type
const SignalConfig* getSignalConfig(const char* signalName, ElsterType type) {
    // Try to find matching pattern
    for (const auto& config : signalMappings) {
        if (matchesPattern(signalName, config.namePattern)) {
            return &config;
        }
    }
    // Return default (last entry)
    return &signalMappings[sizeof(signalMappings)/sizeof(SignalConfig) - 1];
}

// Abbreviation expansion lookup table
static const struct {
    const char* abbrev;
    const char* full;
} abbreviations[] = {
    {"TEMP", "Temperatur"},
    {"WW", "Warmwasser"},
    {"IST", "Ist"},
    {"SOLL", "Soll"},
    {"MAX", "Maximum"},
    {"MIN", "Minimum"},
    {"SUM", "Summe"},
    {"TAG", "Tag"},
    {"HEIZ", "Heizung"},
    {"KUEHLUNG", "Kühlung"},
    {"VERD", "Verdichter"},
    {"EL", "Elektrisch"},
    {"LEISTUNG", "Leistung"},
    {"DRUCK", "Druck"},
    {"VORLAUF", "Vorlauf"},
    {"RUECKLAUF", "Rücklauf"},
    {"AUSSEN", "Außen"},
    {"RAUM", "Raum"},
    {"SPEICHER", "Speicher"},
    {"KESSEL", "Kessel"},
    {"SAMMLER", "Sammler"},
    {"LAUFZEIT", "Laufzeit"},
    {"ERTRAG", "Ertrag"},
    {"BETRIEB", "Betrieb"},
    {"PUMPE", "Pumpe"},
    {"DURCHFLUSS", "Durchfluss"},
    {"VOLUMENSTROM", "Volumenstrom"},
    {"SPANNUNG", "Spannung"},
    {"STROM", "Strom"},
    {"FREQUENZ", "Frequenz"},
    {"DREHZAHL", "Drehzahl"}
};

// Generate friendly name from signal name (kept in German as requested)
std::string getFriendlyName(const char* signalName, const char* canMemberName) {
    std::string name(signalName);
    
    // Keep German signal names, replace underscores with spaces
    std::replace(name.begin(), name.end(), '_', ' ');
    
    // Apply abbreviation expansions efficiently
    for (size_t i = 0; i < sizeof(abbreviations)/sizeof(abbreviations[0]); i++) {
        const char* abbrev = abbreviations[i].abbrev;
        const char* full = abbreviations[i].full;
        const size_t abbrevLen = strlen(abbrev);
        const size_t fullLen = strlen(full);
        size_t pos = 0;
        
        // Check at beginning (with space after)
        if (name.length() > abbrevLen && name.compare(0, abbrevLen, abbrev) == 0 && name[abbrevLen] == ' ') {
            name.replace(0, abbrevLen, full);
            pos = fullLen;
        }
        
        // Check in middle (space before and after)
        while ((pos = name.find(abbrev, pos)) != std::string::npos) {
            bool hasBefore = (pos > 0 && name[pos - 1] == ' ');
            bool hasAfter = (pos + abbrevLen < name.length() && name[pos + abbrevLen] == ' ');
            if (hasBefore && hasAfter) {
                name.replace(pos, abbrevLen, full);
                pos += fullLen;
            } else {
                pos += abbrevLen;
            }
        }
        
        // Check at end (with space before)
        if (name.length() > abbrevLen && name.compare(name.length() - abbrevLen, abbrevLen, abbrev) == 0 &&
            name[name.length() - abbrevLen - 1] == ' ') {
            name.replace(name.length() - abbrevLen, abbrevLen, full);
        }
    }
    
    // Convert to title case efficiently: lowercase all, then capitalize first letter of each word
    // Note: CAN member name is NOT added as prefix since the device hierarchy already shows it
    bool capitalizeNext = true;
    for (size_t i = 0; i < name.length(); i++) {
        if (capitalizeNext && std::isalpha(name[i])) {
            name[i] = ::toupper(::tolower(name[i]));
            capitalizeNext = false;
        } else {
            name[i] = ::tolower(name[i]);
            if (name[i] == ' ') capitalizeNext = true;
        }
    }
    
    return name;
}

// Publish main device (parent for all CAN member sub-devices)
void publishMainDevice() {
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
    
    // Generate unique ID (lowercase, no spaces)
    char uniqueId[128];
    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm.Name, ei->Name);
    std::string uid(uniqueId);
    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
    std::replace(uid.begin(), uid.end(), ' ', '_');
    
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
    
    // Add optional fields only if specified
    if (config->deviceClass[0] != '\0') {
        payload << ",\"device_class\":\"" << config->deviceClass << "\"";
    }
    if (config->unit[0] != '\0') {
        payload << ",\"unit_of_measurement\":\"" << config->unit << "\"";
    }
    if (config->stateClass[0] != '\0') {
        payload << ",\"state_class\":\"" << config->stateClass << "\"";
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
    
    // Publish state
    id(mqtt_client).publish(stateTopic, value);
}

void update_COP_WW()
{
    float cop_ww = (id(WAERMEERTRAG_WW_SUM).state + id(WAERMEERTRAG_2WE_WW_SUM).state) / id(EL_AUFNAHMELEISTUNG_WW_SUM).state;
    id(COP_WW).publish_state(cop_ww);
}

void update_COP_HEIZ()
{
    float cop_heiz = (id(WAERMEERTRAG_HEIZ_SUM).state + id(WAERMEERTRAG_2WE_HEIZ_SUM).state) / id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).state;
    id(COP_HEIZ).publish_state(cop_heiz);
}

void update_COP_GESAMT()
{
    float cop_gesamt = (id(WAERMEERTRAG_HEIZ_SUM).state + id(WAERMEERTRAG_2WE_HEIZ_SUM).state + id(WAERMEERTRAG_WW_SUM).state + id(WAERMEERTRAG_2WE_WW_SUM).state) / (id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).state + id(EL_AUFNAHMELEISTUNG_WW_SUM).state);
    id(COP_GESAMT).publish_state(cop_gesamt);
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

void updateSensor(uint32_t can_id, const ElsterIndex *ei, const std::string &value)
{
    const CanMember &cm = lookupCanMember(can_id);
    std::string key = std::string(cm.Name) + "_" + ei->Name;
    
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
        
        // Only blacklist after 3 consecutive invalid values
        if (invalidSignalCounts[key] >= 3) {
            if (blacklistedSignals.find(key) == blacklistedSignals.end()) {
                blacklistedSignals.insert(key);
                ESP_LOGW("BLACKLIST", "Signal %s from %s returned %d consecutive invalid values (last: '%s') - blacklisted",
                         ei->Name, cm.Name, invalidSignalCounts[key], value.c_str());
                
                // Remove from Home Assistant by sending empty discovery message
                char uniqueId[128];
                snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm.Name, ei->Name);
                std::string uid(uniqueId);
                std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
                std::replace(uid.begin(), uid.end(), ' ', '_');
                
                const char* component = (ei->Type == et_bool || ei->Type == et_little_bool) ? "binary_sensor" : "sensor";
                char discoveryTopic[256];
                snprintf(discoveryTopic, sizeof(discoveryTopic), 
                         "homeassistant/%s/heatingpump/%s/config", component, uid.c_str());
                
                // Send empty payload to remove entity from HA
                id(mqtt_client).publish(discoveryTopic, "", 0, 0, true);
                ESP_LOGI("BLACKLIST", "Removed discovery for %s from Home Assistant", uid.c_str());
            }
        } else {
            ESP_LOGD("BLACKLIST", "Signal %s from %s invalid (%d/3): '%s'",
                     ei->Name, cm.Name, invalidSignalCounts[key], value.c_str());
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
    }
    
    // Reset invalid and no-response counters
    if (invalidSignalCounts.find(key) != invalidSignalCounts.end()) {
        invalidSignalCounts.erase(key);
    }
    if (noResponseCounts.find(key) != noResponseCounts.end()) {
        noResponseCounts.erase(key);
    }
    
    // Publish to MQTT (discovery + state) for valid signals only
    publishMqttDiscovery(can_id, ei);
    publishMqttState(can_id, ei, value);
}

// Check for timed-out requests (no response received)
void checkPendingRequests() {
    const unsigned long TIMEOUT_MS = 5000; // 5 second timeout
    unsigned long now = millis();
    
    // Create list of timed-out requests
    std::vector<std::string> timedOut;
    for (auto it = pendingRequests.begin(); it != pendingRequests.end(); ++it) {
        if (now - it->second > TIMEOUT_MS) {
            timedOut.push_back(it->first);
        }
    }
    
    // Process timed-out requests
    for (const auto& key : timedOut) {
        pendingRequests.erase(key);
        noResponseCounts[key]++;
        
        // Blacklist after 3 consecutive no-responses
        if (noResponseCounts[key] >= 3) {
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
    id(STUNDE).update();
    writeSignal(&cm, "MINUTE", cminute);
    id(MINUTE).update();
    writeSignal(&cm, "SEKUNDE", csekunde);
    id(SEKUNDE).update();
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
    id(JAHR).update();
    writeSignal(&cm, "MONAT", cmonth);
    id(MONAT).update();
    writeSignal(&cm, "TAG", cday);
    id(TAG).update();
}

#endif

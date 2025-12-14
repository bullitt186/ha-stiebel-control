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
    
    // Apply abbreviation expansions (word boundaries respected)
    for (size_t i = 0; i < sizeof(abbreviations)/sizeof(abbreviations[0]); i++) {
        const char* abbrev = abbreviations[i].abbrev;
        const char* full = abbreviations[i].full;
        size_t abbrevLen = strlen(abbrev);
        size_t fullLen = strlen(full);
        size_t pos = 0;
        
        // Check at beginning
        if (name.find(std::string(abbrev) + " ") == 0) {
            name.replace(0, abbrevLen, full);
        }
        
        // Check in middle
        std::string search = std::string(" ") + abbrev + " ";
        std::string replace = std::string(" ") + full + " ";
        while ((pos = name.find(search, pos)) != std::string::npos) {
            name.replace(pos, search.length(), replace);
            pos += replace.length();
        }
        
        // Check at end
        if (name.length() >= abbrevLen) {
            if (name.substr(name.length() - abbrevLen) == abbrev) {
                name.replace(name.length() - abbrevLen, abbrevLen, full);
            }
        }
    }
    
    // Add CAN member prefix for context
    std::string prefix(canMemberName);
    if (prefix != "PC" && prefix != "OTHER") {
        // Convert prefix to title case
        std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
        if (!prefix.empty()) {
            prefix[0] = ::toupper(prefix[0]);
        }
        name = prefix + " " + name;
    }
    
    // Convert to title case: lowercase all, then capitalize first letter of each word
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    bool capitalizeNext = true;
    for (size_t i = 0; i < name.length(); i++) {
        if (capitalizeNext && std::isalpha(name[i])) {
            name[i] = ::toupper(name[i]);
            capitalizeNext = false;
        } else if (name[i] == ' ') {
            capitalizeNext = true;
        }
    }
    
    return name;
}

// Publish MQTT Discovery config for a signal
void publishMqttDiscovery(uint32_t can_id, const ElsterIndex *ei) {
    const CanMember &cm = lookupCanMember(can_id);
    const SignalConfig* config = getSignalConfig(ei->Name, (ElsterType)ei->Type);
    
    // Generate unique ID
    char uniqueId[128];
    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm.Name, ei->Name);
    std::string uid(uniqueId);
    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
    
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
             "homeassistant/%s/heatingpump/%s/config", component, uniqueId);
    
    // Build state topic
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    
    // Generate friendly name
    std::string friendlyName = getFriendlyName(ei->Name, cm.Name);
    
    // Build JSON payload
    std::string payload = "{";
    payload += "\"name\":\"" + friendlyName + "\",";
    payload += "\"unique_id\":\"" + uid + "\",";
    payload += "\"state_topic\":\"" + std::string(stateTopic) + "\",";
    
    // Device info
    payload += "\"device\":{";
    payload += "\"identifiers\":[\"stiebel_eltron_wpl13e\"],";
    payload += "\"name\":\"Stiebel Eltron Heat Pump\",";
    payload += "\"model\":\"WPL 13 E\",";
    payload += "\"manufacturer\":\"Stiebel Eltron\"";
    payload += "},";
    
    // Add device class if specified
    if (strlen(config->deviceClass) > 0) {
        payload += "\"device_class\":\"" + std::string(config->deviceClass) + "\",";
    }
    
    // Add unit if specified
    if (strlen(config->unit) > 0) {
        payload += "\"unit_of_measurement\":\"" + std::string(config->unit) + "\",";
    }
    
    // Add state class if specified
    if (strlen(config->stateClass) > 0) {
        payload += "\"state_class\":\"" + std::string(config->stateClass) + "\",";
    }
    
    // Add icon
    payload += "\"icon\":\"" + std::string(config->icon) + "\"";
    
    payload += "}";
    
    // Publish discovery message
    id(mqtt_client).publish(discoveryTopic, payload, 0, true); // QoS 0, retain true
    
    ESP_LOGI("MQTT Discovery", "Published: %s -> %s", uniqueId, friendlyName.c_str());
}

// Publish signal state to MQTT
void publishMqttState(uint32_t can_id, const ElsterIndex *ei, const std::string &value) {
    const CanMember &cm = lookupCanMember(can_id);
    
    // Build state topic
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    
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

void updateSensor(uint32_t can_id, const ElsterIndex *ei, std::string value)
{
    CanMemberType cmt = lookupCanMemberType(can_id);
    
    // Publish to MQTT (discovery + state) for ALL signals
    publishMqttDiscovery(can_id, ei);
    publishMqttState(can_id, ei, value);

    if (ei->Name == "AUSSENTEMP")
    {
        id(AUSSENTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "EINSTELL_SPEICHERSOLLTEMP")
    {
        id(EINSTELL_SPEICHERSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "EINSTELL_SPEICHERSOLLTEMP2")
    {
        id(EINSTELL_SPEICHERSOLLTEMP2).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "HILFSKESSELSOLL")
    {
        id(HILFSKESSELSOLL).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "KESSELSOLLTEMP")
    {
        id(KESSELSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SPEICHERISTTEMP")
    {
        id(SPEICHERISTTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SPEICHERSOLLTEMP")
    {
        id(SPEICHERSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SPEICHER_OBEN_TEMP")
    {
        id(SPEICHER_OBEN_TEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SPEICHER_UNTEN_TEMP")
    {
        id(SPEICHER_UNTEN_TEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "VORLAUFISTTEMP")
    {
        switch (cmt)
        {
        case cm_kessel:
            id(VORLAUFISTTEMP_KESSEL).publish_state(std::stof(value));
            return;
        case cm_manager:
            id(VORLAUFISTTEMP_MANAGER).publish_state(std::stof(value));
            return;
        }
        return;
    }
    if (ei->Name == "VORLAUFSOLLTEMP")
    {
        id(VORLAUFSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "GERAETE_ID")
    {
        switch (cmt)
        {
        case cm_kessel:
            id(GERAETE_ID_KESSEL).publish_state(value);
            break;
        case cm_manager:
            id(GERAETE_ID_MANAGER).publish_state(value);
            break;
        case cm_heizmodul:
            id(GERAETE_ID_HEIZMODUL).publish_state(value);
            break;
        }
        return;
    }
    if (ei->Name == "JAHR")
    {
        id(JAHR).publish_state(std::stoi(value));
        publishDate();
        return;
    }
    if (ei->Name == "LAUFZEIT_VERD_BEI_SPEICHERBEDARF")
    {
        id(LAUFZEIT_VERD_BEI_SPEICHERBEDARF).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "LUEFT_PASSIVKUEHLUNG_UEBER_FORTLUEFTER")
    {
        id(LUEFT_PASSIVKUEHLUNG_UEBER_FORTLUEFTER).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "MINUTE")
    {
        id(MINUTE).publish_state(std::stoi(value));
        publishTime();
        return;
    }
    if (ei->Name == "MONAT")
    {
        id(MONAT).publish_state(std::stoi(value));
        publishDate();
        return;
    }
    if (ei->Name == "PROGRAMMSCHALTER")
    {
        id(PROGRAMMSCHALTER).publish_state(value);
        return;
    }
    if (ei->Name == "RAUMISTTEMP")
    {
        id(RAUMISTTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_I")
    {
        id(RAUMSOLLTEMP_I).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_II")
    {
        id(RAUMSOLLTEMP_II).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_III")
    {
        id(RAUMSOLLTEMP_III).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_NACHT")
    {
        id(RAUMSOLLTEMP_NACHT).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SEKUNDE")
    {
        id(SEKUNDE).publish_state(std::stoi(value));
        publishTime();
        return;
    }
    if (ei->Name == "SAMMLERISTTEMP")
    {
        id(SAMMLERISTTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SAMMLERSOLLTEMP")
    {
        id(SAMMLERSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SOFTWARE_VERSION")
    {
        id(SOFTWARE_VERSION).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "SOMMERBETRIEB")
    {
        id(SOMMERBETRIEB).publish_state(value);
        return;
    }
    if (ei->Name == "STUNDE")
    {
        id(STUNDE).publish_state(std::stoi(value));
        publishTime();
        return;
    }
    if (ei->Name == "TAG")
    {
        id(TAG).publish_state(std::stoi(value));
        publishDate();
        return;
    }
    if (ei->Name == "TEILVORRANG_WW")
    {
        id(TEILVORRANG_WW).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "TEMPORALE_LUEFTUNGSSTUFE_DAUER")
    {
        id(TEMPORALE_LUEFTUNGSSTUFE_DAUER).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "VERDICHTER")
    {
        switch (cmt)
        {
        case cm_heizmodul:
            id(VERDICHTER).publish_state(std::stof(value));
            break;
        }
        return;
    }
    if (ei->Name == "VERSTELLTE_RAUMSOLLTEMP")
    {
        id(VERSTELLTE_RAUMSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "WP_STATUS")
    {
        id(WP_STATUS).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "WW_ECO")
    {
        id(WW_ECO).publish_state(value);
        return;
    }
    if (ei->Name == "EVU_SPERRE_AKTIV")
    {
        id(EVU_SPERRE_AKTIV_MANAGER).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "VERDAMPFERTEMP")
    {
        id(VERDAMPFERTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RUECKLAUFISTTEMP")
    {
        switch (cmt)
        {
        case cm_heizmodul:
            id(RUECKLAUFISTTEMP_HEIZMODUL).publish_state(std::stof(value));
            break;
        case cm_manager:
            if (std::stof(value) > 0) {
                id(RUECKLAUFISTTEMP_MANAGER).publish_state(std::stof(value));
            }
            break;
        }
        return;
    }
    if (ei->Name == "WPVORLAUFIST")
    {
        switch (cmt)
        {
        case cm_heizmodul:
            id(WPVORLAUFIST_HEIZMODUL).publish_state(std::stof(value));
            break;
        case cm_manager:
            id(WPVORLAUFIST_MANAGER).publish_state(std::stof(value));
            break;
        }
        return;

    }
    if (ei->Name == "ABTAUUNGAKTIV")
    {
        id(ABTAUUNGAKTIV).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "BETRIEBSART_WP")
    {
        id(BETRIEBSART_WP).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_HEIZ_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_SUM_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_SUM_KWH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_WW_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_WW_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_TAG_WH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_WW_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_WW_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "LZ_VERD_1_2_WW_BETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_2_WW_BETRIEB_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_2_WW_BETRIEB_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_WW_BETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_WW_BETRIEB_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_WW_BETRIEB_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_2_WW_BETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_2_WW_BETRIEB_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_2_WW_BETRIEB_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "SOFTWARE_NUMMER")
    {
        switch (cmt)
        {
        case cm_manager:
            id(SOFTWARE_NUMMER_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(SOFTWARE_NUMMER_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_HEIZ_SUM_KWH")
    {
        id(WAERMEERTRAG_2WE_HEIZ_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_2WE_HEIZ_SUM).publish_state(id(WAERMEERTRAG_2WE_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_2WE_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_WW_SUM_KWH")
    {
        id(WAERMEERTRAG_2WE_WW_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_2WE_WW_SUM).publish_state(id(WAERMEERTRAG_2WE_WW_SUM_MWH).state + id(WAERMEERTRAG_2WE_WW_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_SUM_KWH")
    {
        id(WAERMEERTRAG_HEIZ_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_HEIZ_SUM).publish_state(id(WAERMEERTRAG_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_TAG_WH")
    {
        id(WAERMEERTRAG_HEIZ_TAG_WH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_HEIZ_TAG).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);
        id(WAERMEERTRAG_HEIZ_INCREASING).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_SUM_KWH")
    {
        id(WAERMEERTRAG_WW_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_WW_SUM).publish_state(id(WAERMEERTRAG_WW_SUM_MWH).state + id(WAERMEERTRAG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_TAG_WH")
    {
        id(WAERMEERTRAG_WW_TAG_WH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_WW_TAG).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);
        id(WAERMEERTRAG_WW_INCREASING).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_HEIZ_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_SUM_MWH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_SUM_MWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_WW_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_WW_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_TAG_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_WW_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_WW_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_HEIZ_SUM_MWH")
    {
        id(WAERMEERTRAG_2WE_HEIZ_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_2WE_HEIZ_SUM).publish_state(id(WAERMEERTRAG_2WE_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_2WE_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_WW_SUM_MWH")
    {
        id(WAERMEERTRAG_2WE_WW_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_2WE_WW_SUM).publish_state(id(WAERMEERTRAG_2WE_WW_SUM_MWH).state + id(WAERMEERTRAG_2WE_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_SUM_MWH")
    {
        id(WAERMEERTRAG_HEIZ_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_HEIZ_SUM).publish_state(id(WAERMEERTRAG_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_TAG_KWH")
    {
        id(WAERMEERTRAG_HEIZ_TAG_KWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_HEIZ_TAG).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);
        id(WAERMEERTRAG_HEIZ_INCREASING).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_SUM_MWH")
    {
        id(WAERMEERTRAG_WW_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_WW_SUM).publish_state(id(WAERMEERTRAG_WW_SUM_MWH).state + id(WAERMEERTRAG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_TAG_KWH")
    {
        id(WAERMEERTRAG_WW_TAG_KWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_WW_TAG).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);
        id(WAERMEERTRAG_WW_INCREASING).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "LZ_VERD_1_2_HEIZBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_2_HEIZBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_2_HEIZBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_2_KUEHLBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_2_KUEHLBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_2_KUEHLBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_HEIZBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_HEIZBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_HEIZBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_KUEHLBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_KUEHLBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_KUEHLBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_2_HEIZBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_2_HEIZBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_2_HEIZBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_2_KUEHLBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_2_KUEHLBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_2_KUEHLBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    
    // Log unmatched signal for identification
    const CanMember &cm = lookupCanMember(can_id);
    ESP_LOGW("updateSensor()", "UNMATCHED SIGNAL: %s (0x%04x) from %s (0x%02x) = %s (%s)", 
             ei->Name, ei->Index, cm.Name, cm.CanId, value.c_str(), ElsterTypeStr[ei->Type]);
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

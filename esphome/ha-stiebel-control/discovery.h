/*
 * discovery.h - Home Assistant MQTT discovery: device blocks, default entity IDs and the
 * discovery payloads of raw signals and calculated sensors (#95, #96, #109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include "config.h"
#include "language_select.h"
#include "KElsterTable.h"
#include "can_members.h"
#include "signal_route.h"
#include "mqtt_publish.h"

// ============================================================================
// HA DEVICE BLOCKS: defined once for every discovery payload (#95)
// ============================================================================
#define HA_MAIN_DEVICE_ID "stiebel_eltron_" HA_DEVICE_MODEL_STR
#define HA_MAIN_DEVICE_JSON "\"device\":{\"identifiers\":[\"" HA_MAIN_DEVICE_ID "\"]," \
                            "\"name\":\"Stiebel Eltron Wärmepumpe\",\"manufacturer\":\"Stiebel Eltron\"}"

// Language-independent entity ID for a fresh install (#96): <component>.<root>_<unique_id
// without "stiebel_">, lower case, e.g. sensor.heatingpump_manager_aussentemp. Registered
// entities keep their IDs.
inline std::string defaultEntityIdJson(const char* component, const std::string& uniqueId) {
    // An entity ID allows only a-z, 0-9 and "_": the hyphen of a device name like heat-pump becomes "_"
    std::string id = std::string(HA_TOPIC_ROOT "_") +
                     (uniqueId.rfind("stiebel_", 0) == 0 ? uniqueId.substr(8) : uniqueId);
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return c == '-' ? '_' : (char)::tolower(c); });
    return std::string("\"default_entity_id\":\"") + component + "." + id + "\",";
}

// Device of one CAN member, linked to the heat pump device
inline std::string memberDeviceJson(const char* member) {
    static const struct { const char* name; const char* label; } labels[] = {
        {"KESSEL", "Kessel"}, {"MANAGER", "Manager"}, {"HEIZMODUL", "Heizmodul"},
        {"FEHLERSPEICHER", "Fehlerspeicher"}, {"MIXER1", "Mischer 1"}, {"MIXER2", "Mischer 2"},
        {"WMZ1", "Wärmemengenzähler 1"}, {"WMZ2", "Wärmemengenzähler 2"}};
    const char* label = member;
    for (const auto& l : labels)
        if (strcmp(member, l.name) == 0) label = l.label;
    return std::string("\"device\":{\"identifiers\":[\"stiebel_") + member + "\"],\"name\":\"" + label +
           "\",\"via_device\":\"" HA_MAIN_DEVICE_ID "\",\"manufacturer\":\"Stiebel Eltron\"}";
}

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
    // true = announced with its first value only (needs model-specific registers), so
    // models without them get no dead HA entity
    bool onDemand = false;
    // Discovery topic segment if it differs from uniqueId (kept for existing retained configs)
    const char* objectId = nullptr;
};

// WPMW II: the same signal is a different measuring point on the manager side (KESSEL,
// WPM terminals) and in the heat pump module (HEIZMODUL, IWS) (#46). nullptr = table name.
static const char* wpmw2MemberName(const char* member, const char* signal) {
    if (!isWpmw2Model()) return nullptr;
    struct Entry { const char* member; const char* signal; const char* name; };
    static const Entry names[] = {
        // WPMW II manual "INFO IWS": air intake at the unit, not the control outdoor sensor
        {"HEIZMODUL", "AUSSENTEMP",       LNAME_LUFTANSAUGTEMP},
        {"HEIZMODUL", "WPVORLAUFIST",     LNAME_WP_VORLAUF_IWS},
        {"HEIZMODUL", "RUECKLAUFISTTEMP", LNAME_WP_RUECKLAUF_IWS},
        {"KESSEL",    "WPVORLAUFIST",     LNAME_WP_VORLAUF_WPM},       // terminal X2/2
        {"KESSEL",    "RUECKLAUFISTTEMP", LNAME_HK1_REGELFUEHLER},     // X2/3, control sensor HC1
        {"KESSEL",    "VORLAUFISTTEMP",   LNAME_HK2_MISCHER_VORLAUF},  // X2/7, mixer flow HC2
        {"KESSEL",    "KESSELSOLLTEMP",   LNAME_HK1_RUECKLAUF_SOLL},
        // Heating circuit 2 settings on the manager's virtual member 0x602
        {"MISCHERMODUL_3", "RAUMSOLLTEMP_I",     LNAME_HK2_RAUMSOLL_TAG},
        {"MISCHERMODUL_3", "RAUMSOLLTEMP_NACHT", LNAME_HK2_RAUMSOLL_NACHT},
        {"MISCHERMODUL_3", "HEIZKURVE",          LNAME_HK2_HEIZKURVE},
        // Room actual temperature of the remote control FE 7 (one sensor, mirrored on the
        // virtual members) and the circuit it is assigned to (parameter 22 "AUSWAHL FE")
        {"MISCHERMODUL_3", "RAUMISTTEMP",        LNAME_RAUMIST_FE},
        {"MISCHERMODUL_3", "VERSTELLTE_RAUMSOLLTEMP", LNAME_RAUMSOLL_WIRKSAM_FE},
        {"MANAGER", "FERNBEDIENUNGSZUORDNUNG",   LNAME_FE_ZUORDNUNG},
    };
    for (const Entry& e : names)
        if (strcmp(member, e.member) == 0 && strcmp(signal, e.signal) == 0) return e.name;
    return nullptr;
}

// Raw signals whose discovery config has been published: signalKey -> (member, signal),
// so republishAllDiscoveries() can send the configs again without waiting for values.
struct DiscoveredSignal {
    const CanMember* cm;
    const ElsterIndex* ei;
};
static std::map<uint64_t, DiscoveredSignal> discoveredSignals;

// Track which calculated sensors have been discovered
static std::set<std::string> discoveredCalculatedSensors;

// UID cache to avoid repeated string operations on every signal update
static std::unordered_map<std::string, std::string> uidCache;

// Helper: Get type-based defaults for Home Assistant metadata
inline void getTypeDefaults(ElsterType type, const char*& component, const char*& deviceClass, const char*& unit, const char*& stateClass, const char*& icon) {
    switch(type) {
        case et_dec_val:
        case et_cent_val:
            component = "sensor"; deviceClass = "temperature"; unit = "°C"; stateClass = "measurement"; icon = "mdi:thermometer";
            break;
        case et_mil_val:
        case et_hours:
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
        case et_err_status:
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
             "homeassistant/%s/" HA_TOPIC_ROOT "/%s/config", config.component,
             config.objectId ? config.objectId : config.uniqueId);
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.name << "\","
            << "\"unique_id\":\"" << config.uniqueId << "\","
            << defaultEntityIdJson(config.component, config.uniqueId)
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
    payload << "," HA_MAIN_DEVICE_JSON "}";

    // Publish discovery message
    std::string payloadStr = payload.str();
    mqttPublish(discoveryTopic, payloadStr.c_str(), payloadStr.length());

    ESP_LOGD("MQTT", "Discovery published for calculated sensor: %s", config.name);
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

// Value types that decode to a number (SetValueType() in KElsterTable.cpp). Only these may carry
// a state_class; HA rejects one on a text state. et_default is a plain integer (%d).
inline bool isNumericElsterType(uint8_t type) {
    return type == et_default || type == et_dec_val || type == et_cent_val || type == et_mil_val ||
           type == et_hours || type == et_byte || type == et_double_val || type == et_triple_val ||
           type == et_little_endian;
}

// State class a raw signal is announced with: its ElsterMetaTable row, else the type defaults;
// "" for non-numeric types. Used by the discovery and by updateSensor() (-255 on counters).
inline const char* signalStateClass(const ElsterIndex* ei) {
    if (!isNumericElsterType(ei->Type)) return "";
    const ElsterMeta* meta = GetElsterMeta(ei);
    if (meta && meta->haComponent) return meta->stateClass ? meta->stateClass : "";
    const char *component, *deviceClass, *unit, *stateClass, *icon;
    getTypeDefaults((ElsterType)ei->Type, component, deviceClass, unit, stateClass, icon);
    return stateClass;
}

// Publish MQTT Discovery config for a signal
void publishMqttDiscovery(const CanMember &cm, const ElsterIndex *ei) {
    
    // Get cached UID (avoids repeated string operations)
    std::string uid = getOrCreateUID(cm, ei->Name);
    
    // Note: Caller is responsible for checking discoveredSignals and inserting uid
    // This function just publishes the MQTT discovery message
    
    // HA metadata lives in a side table (#57); NULL for rows without metadata
    const ElsterMeta* meta = GetElsterMeta(ei);
    // Get friendly name: use meta->friendlyName or fallback to ei->Name
    const char* friendlyName = (meta && meta->friendlyName) ? meta->friendlyName : ei->Name;
    if (const char* perMember = wpmw2MemberName(cm.Name, ei->Name)) friendlyName = perMember;
    
    // Get metadata: use ei fields if available, otherwise fall back to type defaults
    const char *component, *deviceClass, *unit, *stateClass, *icon;
    const char *payloadOn = nullptr, *payloadOff = nullptr;
    
    if (meta && meta->haComponent) {
        // Use metadata from ElsterMetaTable
        component = meta->haComponent;
        deviceClass = meta->haDeviceClass ? meta->haDeviceClass : "";
        unit = meta->unit ? meta->unit : "";
        stateClass = meta->stateClass ? meta->stateClass : "";
        icon = meta->icon ? meta->icon : "";
        payloadOn = meta->payloadOn;
        payloadOff = meta->payloadOff;
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
             "homeassistant/%s/" HA_TOPIC_ROOT "/%s/config", component, uid.c_str());
    
    // Build state topic
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), HA_TOPIC_ROOT "/%s/%s/state", cm.Name, ei->Name);
    
    // Build JSON payload efficiently using ostringstream
    std::ostringstream payload;
    payload << "{\"name\":\"" << friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << defaultEntityIdJson(component, uid)
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"availability_topic\":\"" HA_TOPIC_ROOT "/status\"";
    
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
    if (stateClass[0] != '\0' && isNumericElsterType(ei->Type)) {
        payload << ",\"state_class\":\"" << stateClass << "\"";
    }
    if (icon[0] != '\0') {
        payload << ",\"icon\":\"" << icon << "\"";
    }
    if (strcmp(ei->Name, "FEHLERMELDUNG") == 0) {
        payload << ",\"json_attributes_topic\":\"" HA_TOPIC_ROOT "/" << cm.Name << "/FEHLERMELDUNG/attributes\"";
    }
    
    // Device info - create individual device per CAN member as sub-device
    // Main device ID for the heat pump
    
    // Create unique device ID for this CAN member
    payload << "," << memberDeviceJson(cm.Name) << "}";
    
    // Publish discovery message with retain flag
    std::string payloadStr = payload.str();
    mqttPublish(discoveryTopic, payloadStr.c_str(), payloadStr.length());
    
    ESP_LOGD("MQTT", "Discovery published for %s", friendlyName);
}

#endif  // DISCOVERY_H

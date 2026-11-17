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
#include "model_profile.h"
#include "language_select.h"
#include "sg_ready_controller.h"
#include "dhw_program.h"
// TWAI status/recovery for the built-in CAN controller (esp32_can). Under ESP-IDF the
// TWAI driver is only linked when esp32_can is used; MCP2515 builds (classic ESP32)
// have no <driver/twai.h> and skip the TWAI diagnostics (see publishCanDiagnostics()).
#if __has_include(<driver/twai.h>)
#include <driver/twai.h>
#define HA_HAS_TWAI 1
#endif
#include <sstream>
#include <iomanip>
#include <set>
#include <map>
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <functional>
#include <algorithm>
#include <tuple>
#include <cmath>
#include "can_members.h"   // CanMember, CanMembers[], CanMemberType, SignalRequest

// Model-specific register ownership. Requires CanMemberType from the main header.
#include <cstring>

struct SignalRoute {
    CanMemberType writeMember;
    CanMemberType readMember;
    CanMemberType stateMember;
};

inline SignalRoute resolveSignalRoute(const ModelProfile& model, uint16_t index,
                                     CanMemberType fallback) {
    // WPL9 test candidate: both DHW registers use CAN-B for writes and feedback.
    // Valid live replies and actual WPM write acceptance still need capture.
    if (model.canBDhwRoutes && (index == 0x0013 || index == 0x0a06))
        return {cm_kessel_canb, cm_kessel_canb, cm_kessel_canb};
    // WPL17: DHW setpoints and the heating curve live on KESSEL_CANB.
    if (model.canBRoutes && (index == 0x0013 || index == 0x0a06 || index == 0x010e))
        return {cm_kessel_canb, cm_kessel_canb, cm_kessel_canb};
    return {fallback, fallback, fallback};
}
inline SignalRoute resolveSignalRoute(uint16_t index, CanMemberType fallback) {
    return resolveSignalRoute(kModel, index, fallback);
}


// ============================================================================
// MQTT AUTO-DISCOVERY CONFIGURATION
// ============================================================================

// ============================================================================
// CALCULATED SENSOR DISCOVERY CONFIGURATION
// ============================================================================

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

static const CalculatedSensorConfig calculatedSensors[] = {
    // Date sensor
    {"stiebel_calculated_date", LNAME_CALC_DATE, HA_TOPIC_ROOT "/calculated/date/state",
     "sensor", "", "", "", "mdi:calendar", "", "", "", true},

    // Time sensor
    {"stiebel_calculated_time", LNAME_CALC_TIME, HA_TOPIC_ROOT "/calculated/time/state",
     "sensor", "", "", "", "mdi:clock", "", "", "", true},

    // Delta T continuous
    {"stiebel_calculated_delta_t_continuous", LNAME_CALC_DELTA_T_CONTINUOUS, HA_TOPIC_ROOT "/calculated/delta_t_continuous/state",
     "sensor", "temperature", "K", "measurement", "mdi:thermometer", "", "", "", true},

    // Delta T running (only when compressor active)
    {"stiebel_calculated_delta_t_running", LNAME_CALC_DELTA_T_RUNNING, HA_TOPIC_ROOT "/calculated/delta_t_running/state",
     "sensor", "temperature", "K", "measurement", "mdi:thermometer-chevron-up", "", "", "", true},

    // Compressor active binary sensor
    {"stiebel_calculated_compressor_active", LNAME_CALC_COMPRESSOR_ACTIVE, HA_TOPIC_ROOT "/calculated/compressor_active/state",
     "binary_sensor", "running", "", "", "mdi:engine", "on", "off", "", true},

    // CAN bus diagnostic sensors (TWAI / ESP32-S3; silently inactive on MCP2515 builds)
    {"stiebel_calculated_can_tec",        LNAME_CALC_CAN_TEC,        HA_TOPIC_ROOT "/calculated/can_tec/state",
     "sensor", "", "", "measurement", "mdi:alert-network", "", "", "diagnostic", false},
    {"stiebel_calculated_can_rec",        LNAME_CALC_CAN_REC,        HA_TOPIC_ROOT "/calculated/can_rec/state",
     "sensor", "", "", "measurement", "mdi:alert-network-outline", "", "", "diagnostic", false},
    {"stiebel_calculated_can_bus_errors", LNAME_CALC_CAN_BUS_ERRORS, HA_TOPIC_ROOT "/calculated/can_bus_errors/state",
     "sensor", "", "", "total_increasing", "mdi:network-off", "", "", "diagnostic", false},
    {"stiebel_calculated_can_state",      LNAME_CALC_CAN_STATE,      HA_TOPIC_ROOT "/calculated/can_state/state",
     "sensor", "", "", "", "mdi:can", "", "", "diagnostic", false},

    // Kombinierte Energie-Gesamtwerte (MWh*1000 + kWh), zusaetzlich zu den Einzelwerten
    {"stiebel_calculated_energie_heiz_total_kwh", LNAME_CALC_ENERGIE_HEIZ_TOTAL_KWH, HA_TOPIC_ROOT "/calculated/energie_heiz_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true, true},
    {"stiebel_calculated_energie_ww_total_kwh", LNAME_CALC_ENERGIE_WW_TOTAL_KWH, HA_TOPIC_ROOT "/calculated/energie_ww_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true, true},
    {"stiebel_calculated_waerme_heiz_total_kwh", LNAME_CALC_WAERME_HEIZ_TOTAL_KWH, HA_TOPIC_ROOT "/calculated/waerme_heiz_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, true},
    {"stiebel_calculated_waerme_ww_total_kwh", LNAME_CALC_WAERME_WW_TOTAL_KWH, HA_TOPIC_ROOT "/calculated/waerme_ww_total_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, true},

    // Aktuelle elektrische Leistung (W) — aus den feinen Tageszaehlern (EL_*_TAG_WH)
    // abgeleitet (Differenz der Wh-Zaehler pro Zeitintervall). Kein Stromzaehler noetig;
    // die WP zaehlt ihren Stromverbrauch selbst.
    {"stiebel_calculated_el_leistung_w", LNAME_CALC_EL_LEISTUNG_W, HA_TOPIC_ROOT "/calculated/el_leistung_w/state",
     "sensor", "power", "W", "measurement", "mdi:lightning-bolt", "", "", "", true, true},

    // Abgeleiteter Betriebsstatus: "Aus" | "Heizen" | "Warmwasser" | "Kuehlen".
    // Ersetzt das (auf der WPL17 unbrauchbare) WP_STATUS-Bitfeld, siehe Ableitung in
    // publishCombinedEnergySensors().
    {"stiebel_calculated_betriebsstatus", LNAME_CALC_BETRIEBSSTATUS, HA_TOPIC_ROOT "/calculated/betriebsstatus/state",
     "sensor", "", "", "", "mdi:heat-pump", "", "", "", true, true},

    // Kombinierte TAGES-Energiewerte (kWh-Anteil + Wh/1000 -> Wh-Aufloesung).
    // Die Tageszaehler aktualisieren laufend und fallen um Mitternacht auf 0
    // zurueck; der Geraete-Gesamtzahler springt nur einmal pro Nacht. In HA ist
    // state_class total_increasing korrekt (Reset wird als Zaehlerneustart
    // behandelt) und als Basis fuer utility_meter (Tag/Monat/Jahr) geeignet.
    {"stiebel_calculated_energie_heiz_tag_kwh", LNAME_CALC_ENERGIE_HEIZ_TAG_KWH, HA_TOPIC_ROOT "/calculated/energie_heiz_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true, true},
    {"stiebel_calculated_energie_ww_tag_kwh", LNAME_CALC_ENERGIE_WW_TAG_KWH, HA_TOPIC_ROOT "/calculated/energie_ww_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true, true},
    {"stiebel_calculated_waerme_heiz_tag_kwh", LNAME_CALC_WAERME_HEIZ_TAG_KWH, HA_TOPIC_ROOT "/calculated/waerme_heiz_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, true},
    {"stiebel_calculated_waerme_ww_tag_kwh", LNAME_CALC_WAERME_WW_TAG_KWH, HA_TOPIC_ROOT "/calculated/waerme_ww_tag_kwh/state",
     "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, true},

    // Getrennte elektrische Leistung (W) aus den jeweiligen TAG_WH-Zaehlern.
    // Damit ist ein evtl. Riemann-Integral oder eine Leistungsanzeige pro
    // Betriebsart (Heizen / Warmwasser) in HA moeglich.
    {"stiebel_calculated_el_leistung_heiz_w", LNAME_CALC_EL_LEISTUNG_HEIZ_W, HA_TOPIC_ROOT "/calculated/el_leistung_heiz_w/state",
     "sensor", "power", "W", "measurement", "mdi:lightning-bolt", "", "", "", true, true},
    {"stiebel_calculated_el_leistung_ww_w", LNAME_CALC_EL_LEISTUNG_WW_W, HA_TOPIC_ROOT "/calculated/el_leistung_ww_w/state",
     "sensor", "power", "W", "measurement", "mdi:lightning-bolt", "", "", "", true, true},

    // Summer mode as the WPMW II decides it (append only: indices above are hardcoded)
    {"stiebel_calculated_sommerbetrieb_aktiv", LNAME_CALC_SOMMERBETRIEB_AKTIV, HA_TOPIC_ROOT "/calculated/sommerbetrieb_aktiv/state",
     "binary_sensor", "", "", "", "mdi:weather-sunny", "on", "off", "", true, true},
    {"stiebel_calculated_aussentemp_gedaempft", LNAME_CALC_AUSSENTEMP_GEDAEMPFT, HA_TOPIC_ROOT "/calculated/aussentemp_gedaempft/state",
     "sensor", "temperature", "°C", "measurement", "mdi:thermometer-lines", "", "", "", true, true},

    // Control deviation of the WPM II: KESSEL return sensor (X2/3 = control sensor HK1)
    // minus the return setpoint. The compressor starts at about -0.7 K and stops at
    // about +2.6 K (measured on a WPL 13 E, five runs), so the value shows the
    // switching state directly.
    {"stiebel_calculated_regelabweichung", LNAME_CALC_REGELABWEICHUNG, HA_TOPIC_ROOT "/calculated/regelabweichung/state",
     "sensor", "temperature", "K", "measurement", "mdi:swap-vertical", "", "", "", true, true},

    // More CAN/MQTT diagnostics (#55). CAN values come from twai_get_status_info()
    // and stay silent on MCP2515 builds; the MQTT counter is published on both.
    {"stiebel_calculated_can_rx_missed",  LNAME_CALC_CAN_RX_MISSED,  HA_TOPIC_ROOT "/calculated/can_rx_missed/state",
     "sensor", "", "", "total_increasing", "mdi:message-alert-outline", "", "", "diagnostic", false, true},
    {"stiebel_calculated_can_rx_overrun", LNAME_CALC_CAN_RX_OVERRUN, HA_TOPIC_ROOT "/calculated/can_rx_overrun/state",
     "sensor", "", "", "total_increasing", "mdi:message-alert", "", "", "diagnostic", false, true},
    {"stiebel_calculated_can_rx_queue",   LNAME_CALC_CAN_RX_QUEUE,   HA_TOPIC_ROOT "/calculated/can_rx_queue/state",
     "sensor", "", "", "measurement", "mdi:tray-full", "", "", "diagnostic", false, true},
    {"stiebel_calculated_mqtt_publish_failures", LNAME_CALC_MQTT_PUBLISH_FAILURES, HA_TOPIC_ROOT "/calculated/mqtt_publish_failures/state",
     "sensor", "", "", "total_increasing", "mdi:message-off-outline", "", "", "diagnostic", false, true},

    // Compressor starts and runtime today (#49), from compressor_active, debounced
    {"stiebel_calculated_verdichter_starts_heute", LNAME_CALC_VERDICHTER_STARTS_HEUTE, HA_TOPIC_ROOT "/calculated/verdichter_starts_heute/state",
     "sensor", "", "", "total_increasing", "mdi:counter", "", "", "", true, true},
    {"stiebel_calculated_verdichter_laufzeit_heute", LNAME_CALC_VERDICHTER_LAUFZEIT_HEUTE, HA_TOPIC_ROOT "/calculated/verdichter_laufzeit_heute/state",
     "sensor", "duration", "min", "total_increasing", "mdi:timer-outline", "", "", "", true, true},
    {"stiebel_calculated_verdichter_laufzeit_mittel", LNAME_CALC_VERDICHTER_LAUFZEIT_MITTEL, HA_TOPIC_ROOT "/calculated/verdichter_laufzeit_mittel/state",
     "sensor", "duration", "min", "measurement", "mdi:timer-sand", "", "", "", true, true},

    // Defrost running, from ZEITDAUER_LETZTE_ABTAUUNG (0 while a defrost runs, #48)
    {"stiebel_calculated_abtauung_laeuft", LNAME_CALC_ABTAUUNG_LAEUFT, HA_TOPIC_ROOT "/calculated/abtauung_laeuft/state",
     "binary_sensor", "running", "", "", "mdi:snowflake-melt", "on", "off", "", true, true},

    // COP (from the energy counters, announced with the first COP value). Their existing
    // discovery topics .../cop_ww/config etc. are kept (objectId).
    {"stiebel_cop_ww", LNAME_COP_WW, HA_TOPIC_ROOT "/calculated/cop_ww/state",
     "sensor", "", "", "measurement", "mdi:water-boiler", "", "", "", true, true, "cop_ww"},
    {"stiebel_cop_heiz", LNAME_COP_HEIZ, HA_TOPIC_ROOT "/calculated/cop_heiz/state",
     "sensor", "", "", "measurement", "mdi:radiator", "", "", "", true, true, "cop_heiz"},
    {"stiebel_cop_gesamt", LNAME_COP_GESAMT, HA_TOPIC_ROOT "/calculated/cop_gesamt/state",
     "sensor", "", "", "measurement", "mdi:chart-line", "", "", "", true, true, "cop_gesamt"}
};

static const size_t CALCULATED_SENSOR_COUNT = sizeof(calculatedSensors) / sizeof(CalculatedSensorConfig);
// Indices of entries used by name (the table is append-only, indices are hardcoded)
// Row of each calculated sensor, in table order (#95: no literal indices)
enum CalcIdx : size_t {
    CALC_IDX_DATE,
    CALC_IDX_TIME,
    CALC_IDX_DELTA_T_CONTINUOUS,
    CALC_IDX_DELTA_T_RUNNING,
    CALC_IDX_COMPRESSOR_ACTIVE,
    CALC_IDX_CAN_TEC,
    CALC_IDX_CAN_REC,
    CALC_IDX_CAN_BUS_ERRORS,
    CALC_IDX_CAN_STATE,
    CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH,
    CALC_IDX_ENERGIE_WW_TOTAL_KWH,
    CALC_IDX_WAERME_HEIZ_TOTAL_KWH,
    CALC_IDX_WAERME_WW_TOTAL_KWH,
    CALC_IDX_EL_LEISTUNG_W,
    CALC_IDX_BETRIEBSSTATUS,
    CALC_IDX_ENERGIE_HEIZ_TAG_KWH,
    CALC_IDX_ENERGIE_WW_TAG_KWH,
    CALC_IDX_WAERME_HEIZ_TAG_KWH,
    CALC_IDX_WAERME_WW_TAG_KWH,
    CALC_IDX_EL_LEISTUNG_HEIZ_W,
    CALC_IDX_EL_LEISTUNG_WW_W,
    CALC_IDX_SOMMERBETRIEB_AKTIV,
    CALC_IDX_AUSSENTEMP_GEDAEMPFT,
    CALC_IDX_REGELABWEICHUNG,
    CALC_IDX_CAN_RX_MISSED,
    CALC_IDX_CAN_RX_OVERRUN,
    CALC_IDX_CAN_RX_QUEUE,
    CALC_IDX_MQTT_PUBLISH_FAILURES,
    CALC_IDX_VERDICHTER_STARTS_HEUTE,
    CALC_IDX_VERDICHTER_LAUFZEIT_HEUTE,
    CALC_IDX_VERDICHTER_LAUFZEIT_MITTEL,
    CALC_IDX_ABTAUUNG_LAEUFT,
    CALC_IDX_COP_WW,
    CALC_IDX_COP_HEIZ,
    CALC_IDX_COP_GESAMT,
    CALC_IDX_COUNT
};
static_assert(CALCULATED_SENSOR_COUNT == CALC_IDX_COUNT, "calculatedSensors and CalcIdx must have the same rows");

// Decode buffer for SetValueType(): its longest texts are 20 characters plus the
// terminator ("not used time domain", "ERR MIN UEBERHITZUNG").
static constexpr size_t ELSTER_VALUE_BUF_SIZE = 32;

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

// Models with a WPMW II manager. Writes stay limited per model in writableNumberApplies().
static constexpr bool isWpmw2Model() { return kModel.wpmw2; }

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

// ============================================================================
// WRITABLE DATETIME CONFIGURATION (for MQTT Datetime entities)
// ============================================================================
// RUNTIME STATE TRACKING
// ============================================================================


// DJB2 hash of a signal name (same function as hash_runtime() below)
inline uint32_t elsterNameHash(const char* str) {
    uint32_t h = 5381;
    while (*str) h = ((h << 5) + h) + static_cast<uint32_t>(*str++);
    return h;
}

// Key of a (CAN member, signal name) pair without building a string: member CAN ID
// in the upper 32 bits, name hash below. Same identity as the former "MEMBER_SIGNAL"
// string keys and the stiebel_<member>_<signal> uid (ElsterTable names have no
// hash collisions, see test "signalKey: ElsterTable names have distinct hashes").
inline uint64_t signalKey(const CanMember& cm, uint32_t nameHash) {
    return (static_cast<uint64_t>(cm.CanId) << 32) | nameHash;
}
inline uint64_t signalKey(const CanMember& cm, const char* signalName) {
    return signalKey(cm, elsterNameHash(signalName));
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

// Track which writable numbers have been discovered
static std::set<std::string> discoveredWritableNumbers;

// Track which writable selects have been discovered
static std::set<std::string> discoveredWritableSelects;

// Track next scheduled request time per signalKey (member + signal)
static std::unordered_map<uint64_t, unsigned long> nextRequestTime;

// Consecutive -255 (0x8000, "not available") answers per signalKey.
static std::unordered_map<uint64_t, uint8_t> invalidStreak;

// Back off signals the model does not provide: from the 3rd consecutive -255 the
// poll interval doubles per answer, capped at 6 h. A valid value resets the streak.
static constexpr unsigned long BACKOFF_MAX_MS = 6UL * 3600UL * 1000UL;
inline unsigned long nextBackoffMs(unsigned long baseMs, uint8_t streak) {
    if (streak < 3) return baseMs;
    unsigned shift = streak - 2;
    if (shift > 16 || (baseMs << shift) > BACKOFF_MAX_MS) return baseMs > BACKOFF_MAX_MS ? baseMs : BACKOFF_MAX_MS;
    return baseMs << shift;
}

inline unsigned long backoffIntervalMs(uint64_t key, unsigned long baseMs) {
    auto it = invalidStreak.find(key);
    return nextBackoffMs(baseMs, it == invalidStreak.end() ? 0 : it->second);
}

// Track starting position for round-robin signal processing (prevents starvation)
static int signalProcessingStartIndex = 0;

// Track sensor values for calculated sensors
// Delta T uses the heat pump's own flow/return sensors (IWS = HEIZMODUL, B1/B2). They sit in the
// same water circuit; the WPM sensors of the KESSEL member (X2/2, X2/3) measure elsewhere (X2/3 is the
// heating circuit control sensor in the buffer) and are only a fallback for models without IWS values.
static float lastWpVorlaufIst = NAN;       // HEIZMODUL WPVORLAUFIST
static float lastRuecklaufIstTemp = NAN;   // HEIZMODUL RUECKLAUFISTTEMP
static float lastWpVorlaufKessel = NAN;    // KESSEL WPVORLAUFIST (fallback only)
static float lastVerdichterValue = NAN;
// Last DHW / room setpoint read from the SG Ready route: baseline of a boost (#85)
static float lastSgDhwSetpoint = NAN;
static float lastSgRoomSetpoint = NAN;
// KESSEL PUMPENSTATUS; -1 = not received yet. Bit 0x4000 is the hot water charging pump (WPMW II).
static int lastPumpenstatus = -1;
inline bool isHotWaterCharging(int pumpenstatus) { return (pumpenstatus & 0x4000) != 0; }
// A utility lock cuts power to the WPMW II heat pump module, so VERDICHTER would keep its last
// value. 0 = no frame yet.
static uint32_t lastHeizmodulFrameMs = 0;
static const uint32_t HEIZMODUL_SILENCE_MS = 60000UL;
inline bool isHeizmodulSilent(uint32_t now, uint32_t lastFrame) {
    return lastFrame != 0 && (now - lastFrame) > HEIZMODUL_SILENCE_MS;
}
// Control deviation inputs: KESSEL member only (X2/3 control sensor and its setpoint)
static float lastKesselRuecklaufIst = NAN;
static float lastKesselSollTemp = NAN;

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
static unsigned long nextCanDiagUpdate = 0;

// UID cache to avoid repeated string operations on every signal update
static std::unordered_map<std::string, std::string> uidCache;

// ============================================================================
// SIGNAL REQUEST CONFIGURATION
// ============================================================================



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
// Elster frame header: data[0] = (id & 0x780) >> 3 plus the message type, data[1] = the lower
// 7 bits of the receiver id. With only 3 bits a request to 0x514 (HEIZMODUL_WPL) went to 0x504.
CanIdBytes generate_read_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>((address & 0xF0) + 1), static_cast<uint8_t>(can_id & 0x7F)};
}

CanIdBytes generate_write_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>(address & 0xF0), static_cast<uint8_t>(can_id & 0x7F)};
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
    char charValue[ELSTER_VALUE_BUF_SIZE];

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

    // Per-frame log: DEBUG only (compiled out at the default INFO level)
    ESP_LOGD("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(idx=0x%04x, raw=0x%04x, %s)", cm.Name, (unsigned)can_id, ei->Name, charValue,
             (unsigned)((msg[2] == 0xfa) ? (msg[4] + (msg[3] << 8)) : msg[2]), (unsigned)(byte2 + (byte1 << 8)), ElsterTypeToName(ei->Type));

    signalValue = charValue;
    return ei;
}

// ============================================================================
// MQTT PUBLISH: failure counter, publish-on-change state cache (#55)
// ============================================================================

// Publishes the MQTT client refused while connected (idf_send_async queue full).
// Attempts while disconnected are not counted: they are sent again after the connect.
// Published as diagnostic sensor mqtt_publish_failures.
static uint32_t mqttPublishFailures = 0;

inline bool mqttPublish(const char* topic, const char* payload, size_t len, bool retain = true) {
    bool ok = id(mqtt_client).publish(topic, payload, len, 0, retain);
    if (!ok && id(mqtt_client).is_connected()) mqttPublishFailures++;
    return ok;
}

// One entry per state topic. Raw signals keep member and signal name (the topic
// heatingpump/<member>/<signal>/state is rebuilt on demand), calculated sensors keep
// their static topic string in `topic` with `signal` == nullptr. No heap per publish:
// the value is copied into a fixed buffer, entries are only added for new topics.
struct StateCacheEntry {
    uint32_t hash;
    const char* topic;      // calculated: full topic; raw: member name
    const char* signal;     // raw: signal name; calculated: nullptr
    uint32_t lastPublishMs;
    bool pending;           // newest value not sent yet (new, changed, failed or forced)
    char value[23];
};
static_assert(sizeof(StateCacheEntry::value) >= 16, "state value buffer too small");
// Sorted by hash (binary search); the republish cursor walks it by position.
static std::vector<StateCacheEntry> stateCache;
static bool stateRepublishActive = false;
static size_t stateRepublishPos = 0;
static uint32_t stateSuppressedCount = 0;   // unchanged values not sent (debug/tests)

inline uint32_t rawStateHash(const CanMember& cm, const char* signalName) {
    return elsterNameHash(signalName) ^ (static_cast<uint32_t>(cm.CanId) * 0x9E3779B1u);
}

inline bool stateEntryMatches(const StateCacheEntry& e, const char* topicOrMember, const char* signal) {
    if (signal == nullptr || e.signal == nullptr) {
        return signal == e.signal && (e.topic == topicOrMember || strcmp(e.topic, topicOrMember) == 0);
    }
    return (e.signal == signal || strcmp(e.signal, signal) == 0) &&
           (e.topic == topicOrMember || strcmp(e.topic, topicOrMember) == 0);
}

inline StateCacheEntry* stateCacheFind(uint32_t hash, const char* topicOrMember, const char* signal,
                                       size_t* insertPos = nullptr) {
    auto it = std::lower_bound(stateCache.begin(), stateCache.end(), hash,
                               [](const StateCacheEntry& e, uint32_t h) { return e.hash < h; });
    if (insertPos) *insertPos = static_cast<size_t>(it - stateCache.begin());
    for (; it != stateCache.end() && it->hash == hash; ++it)
        if (stateEntryMatches(*it, topicOrMember, signal)) return &*it;
    return nullptr;
}

inline const char* stateEntryTopic(const StateCacheEntry& e, char* buf, size_t len) {
    if (!e.signal) return e.topic;
    snprintf(buf, len, HA_TOPIC_ROOT "/%s/%s/state", e.topic, e.signal);
    return buf;
}

static int stateDeadbandTenths = STATE_DEADBAND_TENTHS;   // variable for the tests

// True if `value` of a measured temperature is too close to the last published one
inline bool withinDeadband(const char* signal, const char* published, const char* value) {
    static const char* const kDeadbandSignals[] = {
        "AUSSENTEMP", "SPEICHERISTTEMP", "RUECKLAUFISTTEMP", "VORLAUFISTTEMP", "WPVORLAUFIST"};
    if (stateDeadbandTenths <= 0 || !signal || !published[0]) return false;
    bool listed = false;
    for (const char* name : kDeadbandSignals) {
        if (strcmp(signal, name) == 0) { listed = true; break; }
    }
    if (!listed) return false;
    char* endPublished;
    char* endValue;
    const float a = strtof(published, &endPublished);
    const float b = strtof(value, &endValue);
    if (endPublished == published || *endPublished || endValue == value || *endValue) return false;
    return std::lround(std::fabs(a - b) * 10.0f) < stateDeadbandTenths;
}

// Publishes `value` on change; an unchanged value goes out again after
// STATE_HEARTBEAT_MS. topicOrMember/signal must have static lifetime (string
// literals, ElsterTable/CanMembers names). Returns true if the value was sent.
inline bool publishStateCached(uint32_t hash, const char* topicOrMember, const char* signal,
                               const char* topic, const char* value) {
    const size_t len = strlen(value);
    const uint32_t now = millis();
    size_t pos = 0;
    StateCacheEntry* e = stateCacheFind(hash, topicOrMember, signal, &pos);
    if (!e) {
        if (len >= sizeof(e->value) || stateCache.size() >= STATE_CACHE_MAX_ENTRIES) {
            return id(mqtt_client).is_connected() && mqttPublish(topic, value, len);   // untracked
        }
        if (stateCache.capacity() == 0) stateCache.reserve(128);   // typical model: 100-200 topics
        StateCacheEntry fresh{hash, topicOrMember, signal, now, true, {0}};
        e = &*stateCache.insert(stateCache.begin() + pos, fresh);
        if (stateRepublishActive && pos < stateRepublishPos) stateRepublishPos++;
    }
    if (len >= sizeof(e->value)) {
        // value grew beyond the buffer (rare): publish untracked, remember a marker
        e->value[0] = '\0';
        e->pending = true;
        return id(mqtt_client).is_connected() && mqttPublish(topic, value, len);
    }
    if (strcmp(e->value, value) != 0 && !withinDeadband(signal, e->value, value)) {
        memcpy(e->value, value, len + 1);
        e->pending = true;
    }
    if (!e->pending && (now - e->lastPublishMs) < STATE_HEARTBEAT_MS) {
        stateSuppressedCount++;
        return false;
    }
    // While disconnected keep the value pending; republishAllStates() sends it on connect
    if (!id(mqtt_client).is_connected()) return false;
    if (mqttPublish(topic, value, len)) {
        memcpy(e->value, value, len + 1);   // a heartbeat may carry a value held back by the deadband
        e->pending = false;
        e->lastPublishMs = now;
        return true;
    }
    return false;
}

// Calculated sensor state (topic is a string literal / CalculatedSensorConfig field)
inline bool publishCalculatedState(const char* topic, const char* value) {
    return publishStateCached(elsterNameHash(topic), topic, nullptr, topic, value);
}

// The next value of this raw signal is published even if unchanged (write echo:
// HA expects the read-back after a command, also when the device kept the old value).
inline void forceNextStatePublish(const CanMember& cm, const ElsterIndex* ei) {
    if (StateCacheEntry* e = stateCacheFind(rawStateHash(cm, ei->Name), cm.Name, ei->Name)) e->pending = true;
}

// Called on every MQTT (re)connect: a broker restart without persistence would
// otherwise leave constant values empty until their heartbeat.
// Right after an MQTT connect ESPHome queues its subscriptions and the states of the
// native entities. The async publish queue holds only 30 messages, so anything sent
// in the same moment is dropped. Our own republishing waits until this has drained.
static constexpr uint32_t MQTT_CONNECT_SETTLE_MS = 3000;
static uint32_t mqttConnectedAtMs = 0;
static bool mqttConnectSeen = false;
inline void noteMqttConnected() { mqttConnectedAtMs = millis(); mqttConnectSeen = true; }
inline bool mqttSettling() {
    return mqttConnectSeen && millis() - mqttConnectedAtMs < MQTT_CONNECT_SETTLE_MS;
}

inline void republishAllStates() {
    stateRepublishActive = !stateCache.empty();
    stateRepublishPos = 0;
}

// Sends up to STATE_REPUBLISH_BATCH cached states (250 ms interval in common.yaml).
// Waits while MQTT is down and while the discovery republish is still running.
inline bool discoveryRepublishActive();
inline void stateRepublishStep() {
    if (!stateRepublishActive || !id(mqtt_client).is_connected() || discoveryRepublishActive() ||
        mqttSettling()) return;
    char buf[128];
    const uint32_t now = millis();
    for (int n = 0; n < STATE_REPUBLISH_BATCH && stateRepublishPos < stateCache.size(); n++) {
        StateCacheEntry& e = stateCache[stateRepublishPos++];
        if (e.value[0] == '\0') continue;   // untracked long value
        const char* topic = stateEntryTopic(e, buf, sizeof(buf));
        if (mqttPublish(topic, e.value, strlen(e.value))) {
            e.pending = false;
            e.lastPublishMs = now;
        }
    }
    if (stateRepublishPos >= stateCache.size()) {
        stateRepublishActive = false;
        ESP_LOGI("MQTT", "State republish complete (%u topics)", (unsigned)stateCache.size());
    }
}

inline void resetStateCache() {
    stateCache.clear();
    stateRepublishActive = false;
    stateRepublishPos = 0;
    stateSuppressedCount = 0;
}

// ============================================================================
// NON-BLOCKING READ-BACK AFTER A WRITE (replaces delay(100) in the SG Ready path)
// ============================================================================
struct PendingReadback {
    const CanMember* cm;
    const ElsterIndex* ei;
    uint32_t dueMs;
};
static constexpr size_t READBACK_QUEUE_LEN = 16;
static constexpr uint32_t READBACK_DELAY_MS = 100;
// Second read after every write: the WPMW II often still answers the old value right
// after a write, about 4 s later the new one (#68).
static constexpr uint32_t READBACK_SETTLE_MS = 4000;
static PendingReadback pendingReadbacks[READBACK_QUEUE_LEN];
static size_t pendingReadbackCount = 0;

void readSignal(const CanMember *cm, const ElsterIndex *ei);

// Queues a read request delayMs after a write. A second request for the same signal
// within a second replaces the first; the early and the settle read stay separate.
inline void scheduleReadback(const CanMember* cm, const ElsterIndex* ei,
                             uint32_t delayMs = READBACK_DELAY_MS) {
    const uint32_t due = millis() + delayMs;
    for (size_t i = 0; i < pendingReadbackCount; i++) {
        const int32_t apart = static_cast<int32_t>(pendingReadbacks[i].dueMs - due);
        if (pendingReadbacks[i].cm == cm && pendingReadbacks[i].ei == ei &&
            apart > -1000 && apart < 1000) {
            pendingReadbacks[i].dueMs = due;
            return;
        }
    }
    if (pendingReadbackCount == READBACK_QUEUE_LEN) {
        readSignal(cm, ei);   // queue full (never seen): read at once instead of losing it
        return;
    }
    pendingReadbacks[pendingReadbackCount++] = {cm, ei, due};
}

// Sends at most one due read-back per call (250 ms interval in common.yaml), so a
// SG Ready change with three writes does not burst three reads onto the bus.
inline void pendingReadbackStep() {
    const uint32_t now = millis();
    for (size_t i = 0; i < pendingReadbackCount; i++) {
        if (static_cast<int32_t>(now - pendingReadbacks[i].dueMs) < 0) continue;
        PendingReadback r = pendingReadbacks[i];
        for (size_t j = i + 1; j < pendingReadbackCount; j++) pendingReadbacks[j - 1] = pendingReadbacks[j];
        pendingReadbackCount--;
        readSignal(r.cm, r.ei);
        return;
    }
}

// Read request for a raw Elster index (also indices unknown to ElsterTable, bus scan)
void readSignalRaw(const CanMember *cm, uint16_t index, [[maybe_unused]] const char *name = "?")
{
    constexpr bool use_extended_id = false; // No use of extended ID
    const uint8_t IndexByte1 = static_cast<uint8_t>(index >> 8);
    const uint8_t IndexByte2 = static_cast<uint8_t>(index & 0xFF);
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

    ESP_LOGD("readSignal()", "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             name, index, cm->Name, (unsigned)cm->CanId, readId.first, readId.second,
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void readSignal(const CanMember *cm, const ElsterIndex *ei)
{
    readSignalRaw(cm, ei->Index, ei->Name);
}

void readSignal(const CanMember *cm, const char *elsterName)
{
    readSignal(cm, GetElsterIndex(elsterName));
    return;
}

void writeSignal(const CanMember *cm, const ElsterIndex *ei, const char *&str)
{
#ifdef HA_READ_ONLY_true
    ESP_LOGW("writeSignal()", "read_only: not writing \"%s\" = \"%s\" to %s", ei->Name, str, cm->Name);
    return;
#endif
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
    // Command echo: publish the read-back even if the value did not change
    forceNextStatePublish(*cm, ei);
    // The immediate read-back may still return the old value; read again once settled
    scheduleReadback(cm, ei, READBACK_SETTLE_MS);
}

void writeSignal(const CanMember *cm, const char *elsterName, const char *str)
{
    const char* strCopy = str;  // Create a non-const pointer we can pass by reference
    writeSignal(cm, GetElsterIndex(elsterName), strCopy);
    return;
}

// Use the same route for MQTT commands and the WPL9 SG Ready adapter.
// Explicit read/writeSignal calls keep their original low-level semantics.
void writeConfiguredSignal(CanMemberType fallback, const char* name, const char* value) {
    const ElsterIndex* ei = GetElsterIndex(name);
    const SignalRoute route = resolveSignalRoute(ei->Index, fallback);
    writeSignal(&CanMembers[route.writeMember], name, value);
    readSignal(&CanMembers[route.readMember], ei);
}

// ============================================================================
// PACED WRITES (SG Ready, DHW program)
// ============================================================================
// The WPMW II manager loses a write that follows the previous one within milliseconds:
// on 13.11.2026 the second of three back-to-back SG Ready writes was lost twice (#85).
// Queued writes go out one at a time with a gap. Each is checked against the settled
// read-back and sent once more if the heat pump still reports another value.
static constexpr uint32_t PACED_WRITE_GAP_MS = 1500;
static constexpr uint32_t PACED_WRITE_VERIFY_MS = READBACK_SETTLE_MS + 1500;
static constexpr uint8_t PACED_WRITE_TRIES = 2;
static constexpr size_t PACED_WRITE_QUEUE_LEN = 32;
struct PacedWrite {
    const CanMember* writeCm;
    const CanMember* readCm;
    const ElsterIndex* ei;
    char value[24];
    uint8_t tries;          // 0 = waiting, else sends so far
    uint32_t sentMs;
    bool haveReadBack;
    char readBack[24];
};
static PacedWrite pacedWrites[PACED_WRITE_QUEUE_LEN];
static size_t pacedWriteCount = 0;
static uint32_t pacedLastSendMs = 0;
static bool pacedSentBefore = false;

inline bool queuePacedWrite(const CanMember* writeCm, const CanMember* readCm,
                            const ElsterIndex* ei, const char* value) {
    if (!ei) return false;
#ifdef HA_READ_ONLY_true
    ESP_LOGW("WRITE_Q", "read_only: not writing \"%s\" = \"%s\"", ei->Name, value);
    return false;
#endif
    // A newer value for a write still waiting replaces it (the one in flight stays)
    for (size_t i = 0; i < pacedWriteCount; i++) {
        PacedWrite& p = pacedWrites[i];
        if (p.tries == 0 && p.ei == ei && p.writeCm == writeCm) {
            snprintf(p.value, sizeof(p.value), "%s", value);
            return true;
        }
    }
    if (pacedWriteCount == PACED_WRITE_QUEUE_LEN) {
        ESP_LOGW("WRITE_Q", "queue full, dropping %s = %s", ei->Name, value);
        return false;
    }
    PacedWrite& p = pacedWrites[pacedWriteCount++];
    p = PacedWrite{};
    p.writeCm = writeCm;
    p.readCm = readCm;
    p.ei = ei;
    snprintf(p.value, sizeof(p.value), "%s", value);
    ESP_LOGI("WRITE_Q", "queued %s %s = %s", writeCm->Name, ei->Name, value);
    return true;
}

// Same encoded value? Compares what would go on the bus ("53" = "53.0"); a time domain
// read back as "not used time domain" equals an empty value.
inline bool pacedValueMatches(const ElsterIndex* ei, const char* want, const char* got) {
    std::string g(got);
    if (ei->Type == et_time_domain && g.find(':') == std::string::npos) g.clear();
    const char* a = want;
    const char* b = g.c_str();
    const int ea = TranslateString(a, ei->Type);
    const int eb = TranslateString(b, ei->Type);
    if (ea == -1 || eb == -1) return strcmp(want, g.c_str()) == 0;
    return ea == eb;
}

// Called for every decoded value: keeps the newest read-back of the write in flight.
inline void pacedWriteNoteValue(const CanMember& cm, const ElsterIndex* ei, const std::string& value) {
    if (pacedWriteCount == 0) return;
    PacedWrite& p = pacedWrites[0];
    if (p.tries == 0 || p.ei->Index != ei->Index || p.readCm->CanId != cm.CanId || value == "-255") return;
    snprintf(p.readBack, sizeof(p.readBack), "%s", value.c_str());
    p.haveReadBack = true;
}

inline void sendPacedWrite(PacedWrite& p) {
    const char* v = p.value;
    writeSignal(p.writeCm, p.ei, v);   // also schedules the settled read-back on writeCm
    if (p.readCm != p.writeCm) scheduleReadback(p.readCm, p.ei, READBACK_SETTLE_MS);
    p.tries++;
    p.sentMs = millis();
    p.haveReadBack = false;
    pacedLastSendMs = p.sentMs;
    pacedSentBefore = true;
}

// Called every 250 ms (common.yaml): sends the next write once the gap has passed and
// checks the one in flight once its settled read-back is due.
inline void pacedWriteStep() {
    if (pacedWriteCount == 0) return;
    const uint32_t now = millis();
    PacedWrite& p = pacedWrites[0];
    if (p.tries == 0) {
        if (pacedSentBefore && now - pacedLastSendMs < PACED_WRITE_GAP_MS) return;
        sendPacedWrite(p);
        return;
    }
    if (now - p.sentMs < PACED_WRITE_VERIFY_MS) return;
    const bool ok = p.haveReadBack && pacedValueMatches(p.ei, p.value, p.readBack);
    if (!ok && p.tries < PACED_WRITE_TRIES) {
        ESP_LOGW("WRITE_Q", "%s %s = %s not confirmed (read \"%s\"), sending again",
                 p.writeCm->Name, p.ei->Name, p.value, p.haveReadBack ? p.readBack : "nothing");
        sendPacedWrite(p);
        return;
    }
    if (ok) {
        ESP_LOGI("WRITE_Q", "%s %s = %s confirmed", p.writeCm->Name, p.ei->Name, p.value);
    } else {
        ESP_LOGW("WRITE_Q", "%s %s = %s failed after %u sends (read \"%s\")", p.writeCm->Name, p.ei->Name,
                 p.value, (unsigned)p.tries, p.haveReadBack ? p.readBack : "nothing");
    }
    for (size_t j = 1; j < pacedWriteCount; j++) pacedWrites[j - 1] = pacedWrites[j];
    pacedWriteCount--;
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

// Publish all calculated sensor discoveries (used during startup and republish).
// Rows marked onDemand need model-specific registers; they announce themselves on their
// first value, so models without them get no dead HA entities.

void publishAllCalculatedSensorDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        ESP_LOGD("MQTT", "Republishing all calculated sensor discoveries");
    }

    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; i++) {
        if (calculatedSensors[i].onDemand &&
            discoveredCalculatedSensors.find(calculatedSensors[i].uniqueId) == discoveredCalculatedSensors.end()) {
            continue;   // not seen a value yet
        }
        publishCalculatedSensorDiscovery(calculatedSensors[i], forceRepublish);
    }
}

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
    const char* stateTopic = HA_TOPIC_ROOT "/calculated/date/state";
    publishCalculatedState(stateTopic, datum.c_str());
    ESP_LOGD("CALC", "Published date: %s (Jahr=%d, Monat=%d, Tag=%d)", datum.c_str(), ijahr, imonat, itag);
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
    // Values are validated above; the % 100 lets the compiler see they fit (-Wformat-truncation)
    snprintf(zeit, sizeof(zeit), "%02u:%02u:%02u", (unsigned)istunde % 100u, (unsigned)iminute % 100u, (unsigned)isekunde % 100u);
    
    // Publish state to MQTT
    const char* stateTopic = HA_TOPIC_ROOT "/calculated/time/state";
    publishCalculatedState(stateTopic, zeit);
    ESP_LOGD("CALC", "Published time: %s (Stunde=%d, Minute=%d, Sekunde=%d)", zeit, istunde, iminute, isekunde);
}

// Datetime states are now handled by Home Assistant input_datetime helpers
// Read by ESPHome text_sensors and written via button triggers

// Select flow/return for Delta T from ONE measuring circuit, never a mix:
// 1. IWS pair (HEIZMODUL), 2. WPM pair (KESSEL) only if the IWS delivered neither value.
static bool deltaTInputs(float& vorlauf, float& ruecklauf)
{
    if (!std::isnan(lastWpVorlaufIst) && !std::isnan(lastRuecklaufIstTemp)) {
        vorlauf = lastWpVorlaufIst;
        ruecklauf = lastRuecklaufIstTemp;
    } else if (std::isnan(lastWpVorlaufIst) && std::isnan(lastRuecklaufIstTemp) &&
               !std::isnan(lastWpVorlaufKessel) && !std::isnan(lastKesselRuecklaufIst)) {
        vorlauf = lastWpVorlaufKessel;
        ruecklauf = lastKesselRuecklaufIst;
    } else {
        return false;   // not ready, or IWS delivered only one of the two values so far
    }
    // -255 marks "no sensor" on the bus
    return vorlauf >= -50 && ruecklauf >= -50;
}

void publishDeltaTContinuous()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_DELTA_T_CONTINUOUS]);
    
    float vorlauf, ruecklauf;
    if (!deltaTInputs(vorlauf, ruecklauf)) {
        return; // Silently skip: values not ready or invalid
    }
    
    float deltaT = vorlauf - ruecklauf;
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = HA_TOPIC_ROOT "/calculated/delta_t_continuous/state";
    publishCalculatedState(stateTopic, value);
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
    
    float vorlauf, ruecklauf;
    if (!deltaTInputs(vorlauf, ruecklauf)) {
        return; // Silently skip: values not ready or invalid
    }
    
    float deltaT = vorlauf - ruecklauf;
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = HA_TOPIC_ROOT "/calculated/delta_t_running/state";
    publishCalculatedState(stateTopic, value);
}

// Compressor starts and runtime per day (#49). An off phase shorter than
// COMPRESSOR_MIN_OFF_MS does not end a run, so a short gap is not a new start.
// The day is the heat pump's own date (TAG); the counts live in RAM and start
// from 0 after an ESP reboot.
static constexpr uint32_t COMPRESSOR_MIN_OFF_MS = 120000UL;
struct CompressorDayStats {
    int day = -1;
    uint32_t starts = 0;
    uint32_t runtimeMs = 0;
    bool running = false;          // debounced: true from a start until a long off phase
    bool lastSampleOn = false;
    bool haveSample = false;
    uint32_t lastSampleMs = 0;
    uint32_t offSinceMs = 0;
};
static CompressorDayStats compressorStats;

inline void updateCompressorStats(CompressorDayStats& s, bool on, uint32_t now, int day) {
    if (day > 0 && s.day > 0 && day != s.day) {   // new day: a run across midnight counts again
        s.starts = (on && s.running) ? 1 : 0;
        s.runtimeMs = 0;
    }
    if (day > 0) s.day = day;
    if (s.haveSample && s.lastSampleOn && on) s.runtimeMs += now - s.lastSampleMs;
    if (on) {
        if (!s.running) { s.running = true; s.starts++; }
    } else {
        if (!s.haveSample || s.lastSampleOn) s.offSinceMs = now;
        if (s.running && now - s.offSinceMs >= COMPRESSOR_MIN_OFF_MS) s.running = false;
    }
    s.lastSampleOn = on;
    s.lastSampleMs = now;
    s.haveSample = true;
}

inline void publishCompressorStats() {
    char v[16];
    snprintf(v, sizeof(v), "%u", (unsigned)compressorStats.starts);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_VERDICHTER_STARTS_HEUTE], false);
    publishCalculatedState(calculatedSensors[CALC_IDX_VERDICHTER_STARTS_HEUTE].stateTopic, v);
    const float minutes = compressorStats.runtimeMs / 60000.0f;
    snprintf(v, sizeof(v), "%.1f", minutes);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_VERDICHTER_LAUFZEIT_HEUTE], false);
    publishCalculatedState(calculatedSensors[CALC_IDX_VERDICHTER_LAUFZEIT_HEUTE].stateTopic, v);
    if (compressorStats.starts > 0) {
        snprintf(v, sizeof(v), "%.1f", minutes / compressorStats.starts);
        publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_VERDICHTER_LAUFZEIT_MITTEL], false);
        publishCalculatedState(calculatedSensors[CALC_IDX_VERDICHTER_LAUFZEIT_MITTEL].stateTopic, v);
    }
}

void publishCompressorActive()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COMPRESSOR_ACTIVE]);

    if (isWpmw2Model() && isHeizmodulSilent(millis(), lastHeizmodulFrameMs)) {
        lastVerdichterValue = 0.0f;
    }
    
    // Check if compressor value is valid
    if (std::isnan(lastVerdichterValue)) {
        return; // Silently skip if value invalid
    }
    
    // Compressor is active if value > 2
    bool isActive = (lastVerdichterValue > 2.0);
    
    // Publish state to MQTT
    const char* state = isActive ? "on" : "off";
    const char* stateTopic = HA_TOPIC_ROOT "/calculated/compressor_active/state";
    publishCalculatedState(stateTopic, state);
    // Note: Delta T running is published separately by scheduler

    updateCompressorStats(compressorStats, isActive, millis(), lastTag);
    publishCompressorStats();
}

void publishRegelabweichung()
{
    if (std::isnan(lastKesselRuecklaufIst) || std::isnan(lastKesselSollTemp)) {
        return;
    }
    // Sanity range: -255 marks "no sensor" on the bus
    if (lastKesselRuecklaufIst < -50 || lastKesselSollTemp < -50) {
        return;
    }
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_REGELABWEICHUNG]);

    char value[16];
    snprintf(value, sizeof(value), "%.2f", lastKesselRuecklaufIst - lastKesselSollTemp);
    const char* stateTopic = HA_TOPIC_ROOT "/calculated/regelabweichung/state";
    publishCalculatedState(stateTopic, value);
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
    if (stateClass[0] != '\0') {
        bool isNumericType = (ei->Type == et_dec_val || 
                              ei->Type == et_cent_val || 
                              ei->Type == et_mil_val || 
                              ei->Type == et_hours ||
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

// ============================================================================
// DISCOVERY REPUBLISH (MQTT connect, Home Assistant birth message, manual trigger)
// ============================================================================
// All discovery configs are retained, so a republish only matters after the broker
// or HA lost them. republishAllDiscoveries() just (re)starts a cursor; the configs go
// out a few per call of discoveryRepublishStep() (common.yaml interval), so neither
// the loop nor the MQTT outbound queue sees a burst of ~100 retained payloads.
enum DiscoveryRepublishPhase : uint8_t {
    DR_IDLE = 0, DR_NUMBERS, DR_SELECTS, DR_CLIMATE, DR_CALCULATED, DR_SIGNALS
};
static DiscoveryRepublishPhase discoveryRepublishPhase = DR_IDLE;
static size_t discoveryRepublishPos = 0;
static std::vector<DiscoveredSignal> discoveryRepublishQueue;
static bool copDiscoveryPublished = false;
static constexpr size_t DISCOVERY_REPUBLISH_BATCH = 2;

inline void dhwProgramRepublish();   // DHW time program, defined below

void republishAllDiscoveries() {
    // Snapshot: signals discovered after this point publish their own config anyway.
    discoveryRepublishQueue.clear();
    discoveryRepublishQueue.reserve(discoveredSignals.size());
    for (const auto& kv : discoveredSignals) discoveryRepublishQueue.push_back(kv.second);
    discoveryRepublishPhase = DR_NUMBERS;
    discoveryRepublishPos = 0;
    copDiscoveryPublished = false;   // COP configs follow with the next COP update
    dhwProgramRepublish();           // DHW program: config and text again
    ESP_LOGI("MQTT", "Republishing MQTT discoveries (%u signals)", (unsigned)discoveryRepublishQueue.size());
}

inline bool discoveryRepublishActive() { return discoveryRepublishPhase != DR_IDLE; }

// Publishes up to DISCOVERY_REPUBLISH_BATCH discovery configs (climate counts as one).
void discoveryRepublishStep() {
    if (discoveryRepublishPhase == DR_IDLE || !id(mqtt_client).is_connected() || mqttSettling()) return;
    size_t budget = DISCOVERY_REPUBLISH_BATCH;
    while (budget > 0 && discoveryRepublishPhase != DR_IDLE) {
        const size_t i = discoveryRepublishPos++;
        switch (discoveryRepublishPhase) {
        case DR_NUMBERS:
            if (i >= WRITABLE_NUMBER_COUNT) { discoveryRepublishPhase = DR_SELECTS; discoveryRepublishPos = 0; continue; }
            if (!writableNumberApplies(writableNumbers[i])) continue;
            publishWritableNumberDiscovery(writableNumbers[i], true);
            break;
        case DR_SELECTS:
            if (i >= WRITABLE_SELECT_COUNT) { discoveryRepublishPhase = DR_CLIMATE; discoveryRepublishPos = 0; continue; }
            publishWritableSelectDiscovery(writableSelects[i], true);
            break;
        case DR_CLIMATE:
            if (i >= 1) { discoveryRepublishPhase = DR_CALCULATED; discoveryRepublishPos = 0; continue; }
            publishClimateDiscoveries();
            break;
        case DR_CALCULATED:
            if (i >= CALCULATED_SENSOR_COUNT) { discoveryRepublishPhase = DR_SIGNALS; discoveryRepublishPos = 0; continue; }
            // Energy/power sensors announce themselves on their first value (see above)
            if (calculatedSensors[i].onDemand &&
                discoveredCalculatedSensors.find(calculatedSensors[i].uniqueId) == discoveredCalculatedSensors.end())
                continue;
            publishCalculatedSensorDiscovery(calculatedSensors[i], true);
            break;
        case DR_SIGNALS:
            if (i >= discoveryRepublishQueue.size()) {
                ESP_LOGI("MQTT", "Discovery republish complete");
                discoveryRepublishPhase = DR_IDLE;
                discoveryRepublishPos = 0;
                discoveryRepublishQueue.clear();
                discoveryRepublishQueue.shrink_to_fit();
                continue;
            }
            publishMqttDiscovery(*discoveryRepublishQueue[i].cm, discoveryRepublishQueue[i].ei);
            break;
        default:
            discoveryRepublishPhase = DR_IDLE;
            continue;
        }
        budget--;
    }
}

// Publish signal state to MQTT (retained; on change, unchanged values as heartbeat)
void publishMqttState(const CanMember &cm, const ElsterIndex *ei, const char *value) {
    // Input validation
    if (!ei || !ei->Name || !value || !*value) {
        ESP_LOGW("MQTT", "Invalid signal data, skipping state publish");
        return;
    }
    
    // Build state topic with bounds checking
    char stateTopic[128];
    int written = snprintf(stateTopic, sizeof(stateTopic), HA_TOPIC_ROOT "/%s/%s/state", cm.Name, ei->Name);
    if (written >= (int)sizeof(stateTopic)) {
        ESP_LOGW("MQTT", "Topic too long for %s/%s, truncated", cm.Name, ei->Name);
    }
    
    publishStateCached(rawStateHash(cm, ei->Name), cm.Name, ei->Name, stateTopic, value);
}

inline void publishMqttState(const CanMember &cm, const ElsterIndex *ei, const std::string &value) {
    publishMqttState(cm, ei, value.c_str());
}

// Diagnostics removed for simplification

// Track which COP values we have valid data for
static std::unordered_map<std::string, float> copEnergyValues;

// Publish MQTT discovery for COP sensors
void publishCOPDiscovery() {
    if (copDiscoveryPublished) return;
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_WW]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_HEIZ]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_GESAMT]);
    copDiscoveryPublished = true;
    ESP_LOGD("MQTT", "Discovery published for COP sensors");
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

// Energy in MWh incl. the kWh remainder register when the model provides it
// (e.g. WPL13E: SUM_MWH is whole MWh only, SUM_KWH holds the 0..999 kWh part).
static float copMWh(const std::string& base) {
    float v = copEnergyValues[base + "_SUM_MWH"];
    auto kwh = copEnergyValues.find(base + "_SUM_KWH");
    // 0..999 only: invalid markers (e.g. 0x8000) must not skew the COP
    if (kwh != copEnergyValues.end() && kwh->second >= 0.0f && kwh->second < 1000.0f) v += kwh->second / 1000.0f;
    return v;
}

// Models with kWh remainder registers: wait until all four are known, otherwise
// the COP jumps around after boot while only some remainders have arrived.
static bool copKwhIncomplete() {
    static const char* keys[] = {"WAERMEERTRAG_HEIZ_SUM_KWH", "WAERMEERTRAG_WW_SUM_KWH",
                                 "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", "EL_AUFNAHMELEISTUNG_WW_SUM_KWH"};
    int have = 0;
    for (const char* k : keys) have += copEnergyValues.count(k);
    return have > 0 && have < 4;
}

// Calculate and publish COP values if all required data is available
void updateCOPCalculations() {
    publishCOPDiscovery();
    if (copKwhIncomplete()) {
        publishCombinedEnergySensors();
        return;
    }
    
    // COP WW: (WAERMEERTRAG_WW_SUM + WAERMEERTRAG_2WE_WW_SUM) / EL_AUFNAHMELEISTUNG_WW_SUM
    if (copEnergyValues.find("WAERMEERTRAG_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_SUM_MWH") != copEnergyValues.end()) {
        
        float el_ww = copMWh("EL_AUFNAHMELEISTUNG_WW");
        if (el_ww > 0.001f) { // Avoid division by zero
            float waerme_ww = copMWh("WAERMEERTRAG_WW") + copMWh("WAERMEERTRAG_2WE_WW");
            float cop_ww = waerme_ww / el_ww;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_ww);
            const char* stateTopic = calculatedSensors[CALC_IDX_COP_WW].stateTopic;
            publishCalculatedState(stateTopic, valueStr);
            ESP_LOGD("COP", "COP WW: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_ww, waerme_ww, el_ww);
        }
    }
    
    // COP Heizung: (WAERMEERTRAG_HEIZ_SUM + WAERMEERTRAG_2WE_HEIZ_SUM) / EL_AUFNAHMELEISTUNG_HEIZ_SUM
    if (copEnergyValues.find("WAERMEERTRAG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH") != copEnergyValues.end()) {
        
        float el_heiz = copMWh("EL_AUFNAHMELEISTUNG_HEIZ");
        if (el_heiz > 0.001f) { // Avoid division by zero
            float waerme_heiz = copMWh("WAERMEERTRAG_HEIZ") + copMWh("WAERMEERTRAG_2WE_HEIZ");
            float cop_heiz = waerme_heiz / el_heiz;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_heiz);
            const char* stateTopic = calculatedSensors[CALC_IDX_COP_HEIZ].stateTopic;
            publishCalculatedState(stateTopic, valueStr);
            ESP_LOGD("COP", "COP Heizung: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_heiz, waerme_heiz, el_heiz);
        }
    }
    
    // COP Gesamt: (all WAERMEERTRAG) / (all EL_AUFNAHMELEISTUNG)
    if (copEnergyValues.find("WAERMEERTRAG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_SUM_MWH") != copEnergyValues.end()) {
        
        float el_total = copMWh("EL_AUFNAHMELEISTUNG_HEIZ") + copMWh("EL_AUFNAHMELEISTUNG_WW");
        if (el_total > 0.001f) { // Avoid division by zero
            float waerme_total = copMWh("WAERMEERTRAG_HEIZ") + copMWh("WAERMEERTRAG_2WE_HEIZ") +
                               copMWh("WAERMEERTRAG_WW") + copMWh("WAERMEERTRAG_2WE_WW");
            float cop_gesamt = waerme_total / el_total;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_gesamt);
            const char* stateTopic = calculatedSensors[CALC_IDX_COP_GESAMT].stateTopic;
            publishCalculatedState(stateTopic, valueStr);
            ESP_LOGD("COP", "COP Gesamt: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_gesamt, waerme_total, el_total);
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

    // Toleranz "gleicher Wert": SUM fuehrt nur ganze kWh, beim Tageswechsel faellt
    // der Wh-Anteil weg. Solche Ruecksprunge bleiben unsichtbar (der Wert bleibt stehen).
    const float TOL_EPS = 1.0f;
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
        const float kept = (raw > last) ? raw : last;
        stableValue[key] = kept;
        pendingValue.erase(key);
        pendingSince.erase(key);
        out = kept;
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

// Member mit den Energiezaehlern: WPL17 0x514, WPMW II 0x500.
inline const char* energyMemberName() { return isWpmw2Model() ? "HEIZMODUL" : "HEIZMODUL_WPL"; }

// slot: 0=EL Heizung, 1=EL Warmwasser, 2=Waerme Heizung, 3=Waerme Warmwasser
inline bool wplBaseTotal(int slot, const char* sumMwh, const char* sumKwh,
                         const char* tagKwh, const char* tagWh, float& out) {
    ensureEnergyBase();
    float sm, sk, tk, tw;

    // Tagesanteil (TAG_KWH + TAG_WH/1000) lesen. Er ist innerhalb eines Tages
    // monoton und faellt nur beim Tageswechsel (~23:51) auf 0 zurueck.
    const char* member = energyMemberName();
    bool haveTag = getEnergyForMember(member, tagKwh, tk) &&
                   getEnergyForMember(member, tagWh, tw);
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
    if (getEnergyForMember(member, sumMwh, sm) && getEnergyForMember(member, sumKwh, sk)) {
        float base = sm * 1000.0f + sk;
        if (base > 0.0f && base != energyBase.v[slot]) {   // nur bei Aenderung schreiben (NVS-Schonung)
            if (energyBase.v[slot] > 0.0f && haveTag) energyTagFolded[slot] = tag;
            energyBase.v[slot] = base;
            energyBasePref.save(&energyBase);
        }
    }
    if (energyBase.v[slot] <= 0.0f) return false;
    // Nach einem Neustart kommt der NVS-Sockel vor dem Tagesanteil: ohne ihn waere der
    // Wert um den heutigen Verbrauch zu klein (Ruecksprung). Dann lieber nichts senden.
    if (!haveTag) return false;
    out = energyBase.v[slot];
    float add = tag - energyTagFolded[slot];
    out += (add > 0.0f) ? add : 0.0f;   // nie negativ -> kein Ruecksprung
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
    const bool useMember = kModel.perMemberEnergy;
    auto publishTotal = [useMember](const CalculatedSensorConfig& cfg, const char* topic, const char* key, int slot,
                           const char* sumMwh, const char* sumKwh, const char* tagKwh, const char* tagWh) {
        float rawTotal; bool have = false;
        if (wplBaseTotal(slot, sumMwh, sumKwh, tagKwh, tagWh, rawTotal)) {
            have = true;   // WPL-Basis (NVS-Sockel + Tagesanteil), sobald SUM bekannt
        } else if (energyBase.v[slot] <= 0.0f &&   // Geraet ohne Tageszaehler-Sockel
                   copEnergyValues.find(sumMwh) != copEnergyValues.end() &&
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
        publishCalculatedState(topic, valueStr);
        ESP_LOGD("ENERGIE", "Gesamt (%s): %.3f kWh", key, total);
    };
    publishTotal(calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH], HA_TOPIC_ROOT "/calculated/energie_heiz_total_kwh/state", "combined_energie_heiz", 0,
                 "EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH",
                 "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH");
    publishTotal(calculatedSensors[CALC_IDX_ENERGIE_WW_TOTAL_KWH], HA_TOPIC_ROOT "/calculated/energie_ww_total_kwh/state", "combined_energie_ww", 1,
                 "EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "EL_AUFNAHMELEISTUNG_WW_SUM_KWH",
                 "EL_AUFNAHMELEISTUNG_WW_TAG_KWH", "EL_AUFNAHMELEISTUNG_WW_TAG_WH");
    publishTotal(calculatedSensors[CALC_IDX_WAERME_HEIZ_TOTAL_KWH], HA_TOPIC_ROOT "/calculated/waerme_heiz_total_kwh/state", "combined_waerme_heiz", 2,
                 "WAERMEERTRAG_HEIZ_SUM_MWH", "WAERMEERTRAG_HEIZ_SUM_KWH",
                 "WAERMEERTRAG_HEIZ_TAG_KWH", "WAERMEERTRAG_HEIZ_TAG_WH");
    publishTotal(calculatedSensors[CALC_IDX_WAERME_WW_TOTAL_KWH], HA_TOPIC_ROOT "/calculated/waerme_ww_total_kwh/state", "combined_waerme_ww", 3,
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
            } else if (isWpmw2Model() && lastPumpenstatus >= 0) {
                // WPMW II: die TAG_WH-Zaehler springen nur stuendlich, die Ladepumpe zeigt
                // den Warmwasserbetrieb sofort an.
                ns = isHotWaterCharging(lastPumpenstatus) ? "Warmwasser" : "Heizen";
            } else if (sum > 0.0f && accWwWh > accHeizWh) {
                ns = "Warmwasser";
            } else if (sum > 0.0f) {
                ns = "Heizen";
            } else {
                ns = (strcmp(opState, "Warmwasser") == 0) ? "Warmwasser" : "Heizen";
            }
            opState = ns;
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_BETRIEBSSTATUS], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/betriebsstatus/state", opState);
            lastStatusMs = nowMs;
        }

        // Leistung als 5-Minuten-Mittel (die TAG-Zaehler springen grob). Auf dem WPMW II
        // stehen sie waehrend eines Laufs still, eine Live-Leistung waere dort falsch.
        if (!isWpmw2Model() && nowMs - windowStartMs >= 300000UL) {
            float dtSec = (nowMs - windowStartMs) / 1000.0f;
            float pHeiz = running ? accHeizWh / dtSec * 3600.0f : 0.0f;
            float pWw   = running ? accWwWh   / dtSec * 3600.0f : 0.0f;
            char v[16], ph[16], pw[16];
            snprintf(v, sizeof(v), "%.0f", pHeiz + pWw);
            snprintf(ph, sizeof(ph), "%.0f", pHeiz);
            snprintf(pw, sizeof(pw), "%.0f", pWw);
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_W], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/el_leistung_w/state", v);
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_HEIZ_W], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/el_leistung_heiz_w/state", ph);
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_WW_W], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/el_leistung_ww_w/state", pw);
            ESP_LOGD("ENERGIE", "Leistung: %.0f W (Heiz %.0f, WW %.0f) in %.0fs", pHeiz + pWw, pHeiz, pWw, dtSec);
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
            publishCalculatedState(topic, s);
        };
        pubDaily("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH", calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TAG_KWH], HA_TOPIC_ROOT "/calculated/energie_heiz_tag_kwh/state");
        pubDaily("EL_AUFNAHMELEISTUNG_WW_TAG_KWH",   "EL_AUFNAHMELEISTUNG_WW_TAG_WH",   calculatedSensors[CALC_IDX_ENERGIE_WW_TAG_KWH], HA_TOPIC_ROOT "/calculated/energie_ww_tag_kwh/state");
        pubDaily("WAERMEERTRAG_HEIZ_TAG_KWH",        "WAERMEERTRAG_HEIZ_TAG_WH",        calculatedSensors[CALC_IDX_WAERME_HEIZ_TAG_KWH], HA_TOPIC_ROOT "/calculated/waerme_heiz_tag_kwh/state");
        pubDaily("WAERMEERTRAG_WW_TAG_KWH",          "WAERMEERTRAG_WW_TAG_WH",          calculatedSensors[CALC_IDX_WAERME_WW_TAG_KWH], HA_TOPIC_ROOT "/calculated/waerme_ww_tag_kwh/state");
    }
}

// Validation removed for simplification - all values are published as-is
// Invalid values like -255, -32768, etc. will be visible in Home Assistant
// Users can filter these in HA automations/dashboards if needed

// ============================================================================
// ESPHOME IO ADAPTER FOR SG READY CONTROLLER
// ============================================================================

// SG Ready state label: the select options (lang_base.h / lang_en.h) for 1-4. The only
// place that maps a state to its text (#93).
inline const char* sgReadyStateLabel(int state) {
    if (state >= SgReadyController::STATE_EVU_LOCK && state <= SgReadyController::STATE_FORCED)
        return sgReadyOptions[state - 1];
    return state == SgReadyController::STATE_DISABLED ? LNAME_SG_DEAKTIVIERT : LNAME_SG_UNBEKANNT;
}

class EspHomeSgReadyIO : public ISgReadyIO {
public:
    const char* stateLabel(int state) override { return sgReadyStateLabel(state); }

    static ESPPreferenceObject nvsDhw;
    static ESPPreferenceObject nvsRoom;
    static ESPPreferenceObject nvsActive;

    // Writes go through the paced queue (one at a time, checked by read-back, #85).
    // WPL9 keeps its CAN-B route.
    void writeCanSignal(const char* signalName, const char* value) override {
        const ElsterIndex* ei = GetElsterIndex(signalName);
        SignalRoute route = {cm_manager, cm_manager, cm_manager};
        if (kModel.canBDhwRoutes)
            route = resolveSignalRoute(ei->Index, cm_manager);
        queuePacedWrite(&CanMembers[route.writeMember], &CanMembers[route.readMember], ei, value);
    }

    bool currentSetpoint(const char* signalName, float& value) override {
        const float v = strcmp(signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0 ? lastSgDhwSetpoint
                      : strcmp(signalName, "RAUMSOLLTEMP_I") == 0 ? lastSgRoomSetpoint : NAN;
        if (std::isnan(v)) return false;
        value = v;
        return true;
    }

    // SG Ready states always go out (not through the publish-on-change cache)
    void publishMqtt(const char* topic, const char* value) override {
        mqttPublish(topic, value, strlen(value));
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

// Configured boost amounts (DHW and room, states 3 and 4), kept across reboots.
struct SgBoostStorage { uint32_t magic; float v[4]; };
static constexpr uint32_t SG_BOOST_MAGIC = 0x5B005701u;
static ESPPreferenceObject sgBoostPref;

inline void saveSgBoosts() {
    SgBoostStorage s{SG_BOOST_MAGIC, {sgReadyController.boostState3(), sgReadyController.boostState4(),
                                      sgReadyController.roomBoostState3(), sgReadyController.roomBoostState4()}};
    sgBoostPref.save(&s);
}

// SG Ready state-2 mode (#85), kept across reboots
static constexpr uint32_t SG_NORMAL_MODE_KEY = 0x5B005703u;
static ESPPreferenceObject sgNormalModePref;

// Called from on_boot lambda in common.yaml
void initBoostStatePrefs() {
    EspHomeSgReadyIO::nvsDhw    = global_preferences->make_preference<float>(0xB007BA5E, true);
    EspHomeSgReadyIO::nvsRoom   = global_preferences->make_preference<float>(0xB007BA5F, true);
    EspHomeSgReadyIO::nvsActive = global_preferences->make_preference<bool> (0xB007BA60, true);
    sgBoostPref = global_preferences->make_preference<SgBoostStorage>(SG_BOOST_MAGIC, true);
    SgBoostStorage s;
    if (sgBoostPref.load(&s) && s.magic == SG_BOOST_MAGIC) {
        sgReadyController.setBoostState3(s.v[0]);
        sgReadyController.setBoostState4(s.v[1]);
        sgReadyController.setRoomBoostState3(s.v[2]);
        sgReadyController.setRoomBoostState4(s.v[3]);
    }
    sgNormalModePref = global_preferences->make_preference<bool>(SG_NORMAL_MODE_KEY, true);
    bool automatik = false;
    if (sgNormalModePref.load(&automatik)) sgReadyController.setNormalAutomatik(automatik, false);
    ESP_LOGI("NVS", "Boost state preferences initialized");
}

// Called from on_boot lambda in common.yaml
void loadBoostState() {
    sgReadyController.loadFromNvs();
    ESP_LOGI("NVS", "Boost state loaded, active=%d", (int)sgReadyController.isActive());
}

// Called from MQTT on_message lambda in common.yaml
void applySgReadyState(int state) {
    ESP_LOGI("SG_READY", "Applying SG Ready state %d", state);
    if (!sgReadyController.applyState(state)) {
        ESP_LOGW("SG_READY", "Invalid SG Ready state: %d", state);
    }
}

// Accessor helpers used by YAML sensor lambdas and MQTT on_connect block
inline float sgReadyBoostState3Value() { return sgReadyController.boostState3(); }
inline float sgReadyBoostState4Value() { return sgReadyController.boostState4(); }
inline void  setSgReadyBoost3(float v) { sgReadyController.setBoostState3(v); saveSgBoosts(); }
inline void  setSgReadyBoost4(float v) { sgReadyController.setBoostState4(v); saveSgBoosts(); }

// Room boost command (heatingpump/MANAGER/SG_READY_ROOM_BOOST_STATE<n>/set): range 0-5 K,
// stored and published back. Returns false for an invalid value.
inline bool handleSgReadyRoomBoost(int state, const std::string& payload) {
    char* end;
    const float v = strtof(payload.c_str(), &end);
    if (end == payload.c_str() || *end != '\0' || v < 0.0f || v > 5.0f) return false;
    if (state == 3) sgReadyController.setRoomBoostState3(v);
    else sgReadyController.setRoomBoostState4(v);
    saveSgBoosts();
    char topic[96], value[16];
    snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/MANAGER/SG_READY_ROOM_BOOST_STATE%d/state", state);
    snprintf(value, sizeof(value), "%.1f",
             state == 3 ? sgReadyController.roomBoostState3() : sgReadyController.roomBoostState4());
    mqttPublish(topic, value, strlen(value));
    return true;
}

// Publishes the current SG Ready state and boost values (retained). Called on
// boot, on MQTT connect and periodically, so the retained values survive a
// broker restart instead of being published only once on connect.
// Returns false if a message could not be queued (caller retries).
inline bool publishSgReadyState() {
#ifdef HA_DISABLE_SG_READY_true
    return true;
#endif
    int st = sgReadyController.currentState();
    if (st < SgReadyController::STATE_EVU_LOCK || st > SgReadyController::STATE_FORCED)
        st = SgReadyController::STATE_NORMAL;
    const char* s = sgReadyStateLabel(st);
    bool ok = mqttPublish(HA_TOPIC_ROOT "/MANAGER/SG_READY_STATE/state", s, strlen(s));
    char b3[16], b4[16];
    snprintf(b3, sizeof(b3), "%.1f", sgReadyBoostState3Value());
    snprintf(b4, sizeof(b4), "%.1f", sgReadyBoostState4Value());
    ok &= mqttPublish(HA_TOPIC_ROOT "/MANAGER/SG_READY_BOOST_STATE3/state", b3, strlen(b3));
    ok &= mqttPublish(HA_TOPIC_ROOT "/MANAGER/SG_READY_BOOST_STATE4/state", b4, strlen(b4));
    char r3[16], r4[16];
    snprintf(r3, sizeof(r3), "%.1f", sgReadyController.roomBoostState3());
    snprintf(r4, sizeof(r4), "%.1f", sgReadyController.roomBoostState4());
    ok &= mqttPublish(HA_TOPIC_ROOT "/MANAGER/SG_READY_ROOM_BOOST_STATE3/state", r3, strlen(r3));
    ok &= mqttPublish(HA_TOPIC_ROOT "/MANAGER/SG_READY_ROOM_BOOST_STATE4/state", r4, strlen(r4));
    const char* mode = sgReadyController.normalAutomatik() ? sgNormalModeOptions[1] : sgNormalModeOptions[0];
    ok &= mqttPublish(HA_TOPIC_ROOT "/MANAGER/SG_READY_NORMAL_MODE/state", mode, strlen(mode));
    return ok;
}

// SG Ready state after a connect: sent once the connect burst has drained, and
// again one second later if a message did not fit into the queue.
static bool sgReadyPublishPending = false;
static uint32_t sgReadyPublishRetryMs = 0;
inline void scheduleSgReadyPublish() { sgReadyPublishPending = true; sgReadyPublishRetryMs = 0; }
inline void sgReadyPublishStep() {
    if (!sgReadyPublishPending || !id(mqtt_client).is_connected() || mqttSettling()) return;
    const uint32_t now = millis();
    if (sgReadyPublishRetryMs != 0 && static_cast<int32_t>(now - sgReadyPublishRetryMs) < 0) return;
    if (publishSgReadyState()) sgReadyPublishPending = false;
    else sgReadyPublishRetryMs = now + 1000;
}

// MQTT command for the SG Ready state-2 mode (#85). In state 2 the mode is written at once.
inline bool handleSgReadyNormalMode(const std::string& payload) {
    bool automatik;
    if (payload == sgNormalModeOptions[1]) automatik = true;
    else if (payload == sgNormalModeOptions[0]) automatik = false;
    else return false;
    sgReadyController.setNormalAutomatik(automatik);
    sgNormalModePref.save(&automatik);
    ESP_LOGI("SG_READY", "State 2 mode: %s", sgReadyController.normalMode());
    scheduleSgReadyPublish();
    return true;
}

// ============================================================================
// TIME PROGRAMS (WPMW II): DHW (#82) and heating circuit 1
// ============================================================================
// One text per day in Home Assistant, from the three periods <prefix><day>(_SCHALT_2/3) on the
// manager. Read on all WPMW II models; writable where the write was tested on the display.
struct DhwProgramDay {
    std::string period[dhwprog::PERIODS];
    uint8_t known = 0;          // bit per period read
    bool discovered = false;
    std::string published;      // last text sent
    uint32_t publishedMs = 0;
};
struct TimeProgramDef {
    CanMemberType member;       // bus member holding the program
    const char* prefix;         // signal name prefix
    const char* topic;          // heatingpump/<topic>/<day>/state|set
    const char* uid;            // unique_id prefix
    const char* names[dhwprog::DAYS];
};
static constexpr int TP_DHW = 0, TP_HEATING = 1, TP_HEATING2 = 2, TP_COUNT = 3;
static const TimeProgramDef TIME_PROGRAMS[TP_COUNT] = {
    {cm_manager, dhwprog::PREFIX_DHW, "dhw_program", "stiebel_ww_programm_",
     {LNAME_WWPROG_MO, LNAME_WWPROG_DI, LNAME_WWPROG_MI, LNAME_WWPROG_DO,
      LNAME_WWPROG_FR, LNAME_WWPROG_SA, LNAME_WWPROG_SO}},
    {cm_manager, dhwprog::PREFIX_HEATING, "heating_program", "stiebel_heiz_programm_",
     {LNAME_HZPROG_MO, LNAME_HZPROG_DI, LNAME_HZPROG_MI, LNAME_HZPROG_DO,
      LNAME_HZPROG_FR, LNAME_HZPROG_SA, LNAME_HZPROG_SO}},
    // Heating circuit 2: the manager keeps its program as HEIZPROG_1 on MISCHERMODUL_3 (0x602);
    // HEIZPROG_2 on the manager does not change circuit 2 (WPL13E, 0x602 kept 06:00-22:00)
    {cm_mischermodul_3, dhwprog::PREFIX_HEATING, "heating_program_2", "stiebel_heiz_programm_hk2_",
     {LNAME_HZ2PROG_MO, LNAME_HZ2PROG_DI, LNAME_HZ2PROG_MI, LNAME_HZ2PROG_DO,
      LNAME_HZ2PROG_FR, LNAME_HZ2PROG_SA, LNAME_HZ2PROG_SO}},
};
static DhwProgramDay timeProgram[TP_COUNT][dhwprog::DAYS];
static DhwProgramDay (&dhwProgram)[dhwprog::DAYS] = timeProgram[TP_DHW];
static const char* const TIME_PROGRAM_KEYS[dhwprog::DAYS] = {"mo", "di", "mi", "do", "fr", "sa", "so"};
static constexpr uint32_t DHW_PROGRAM_HEARTBEAT_MS = 30UL * 60UL * 1000UL;

inline bool dhwProgramWritable() {
#ifdef HA_READ_ONLY_true
    return false;
#endif
    return isWpmw2Model() && kModel.wpmw2WritesVerified;
}

inline bool publishTimeProgramDiscovery(int prog, int day) {
    const TimeProgramDef& def = TIME_PROGRAMS[prog];
    const bool writable = dhwProgramWritable();
    const char* key = TIME_PROGRAM_KEYS[day];
    char uid[48], topic[112];
    snprintf(uid, sizeof(uid), "%s%s", def.uid, key);
    snprintf(topic, sizeof(topic), "homeassistant/%s/" HA_TOPIC_ROOT "/%s/config", writable ? "text" : "sensor", uid);
    std::string p = std::string("{\"name\":\"") + def.names[day] + "\",\"unique_id\":\"" + uid + "\"," +
                    defaultEntityIdJson(writable ? "text" : "sensor", uid) + "\"state_topic\":\"" HA_TOPIC_ROOT "/" + def.topic + "/" + key + "/state\",";
    if (writable)
        p += std::string("\"command_topic\":\"" HA_TOPIC_ROOT "/") + def.topic + "/" + key + "/set\",\"max\":40,";
    p += "\"icon\":\"mdi:calendar-clock\"," HA_MAIN_DEVICE_JSON "}";
    return mqttPublish(topic, p.c_str(), p.size());
}

inline void timeProgramPublish(int prog, int day, bool force) {
    DhwProgramDay& d = timeProgram[prog][day];
    if (d.known != 0x7) return;
    if (!d.discovered) d.discovered = publishTimeProgramDiscovery(prog, day);
    const std::string text = dhwprog::dayText(d.period);
    const uint32_t now = millis();
    if (!force && text == d.published && now - d.publishedMs < DHW_PROGRAM_HEARTBEAT_MS) return;
    char topic[96];
    snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/%s/%s/state", TIME_PROGRAMS[prog].topic, TIME_PROGRAM_KEYS[day]);
    if (mqttPublish(topic, text.c_str(), text.size())) {
        d.published = text;
        d.publishedMs = now;
    }
}

// Config and text go out again with the next poll of each day (not all at once: the MQTT
// queue is short right after an HA birth message).
inline void dhwProgramRepublish() {
    for (auto& prog : timeProgram)
        for (auto& d : prog) {
            d.discovered = false;
            d.published.clear();
        }
}

// A program period from its member? Then it updates that day's text (no raw sensor).
inline bool timeProgramUpdate(const CanMember& cm, const char* signal, const std::string& value) {
    for (int prog = 0; prog < TP_COUNT; prog++) {
        if (CanMembers[TIME_PROGRAMS[prog].member].CanId != cm.CanId) continue;
        int day, slot;
        if (!dhwprog::parseSignalName(signal, day, slot, TIME_PROGRAMS[prog].prefix)) continue;
        if (value == "-255") return true;   // not provided by this manager
        DhwProgramDay& d = timeProgram[prog][day];
        d.period[slot] = dhwprog::normalizeDecoded(value);
        d.known |= static_cast<uint8_t>(1u << slot);
        timeProgramPublish(prog, day, false);
        return true;
    }
    return false;
}

// MQTT command "11:00-17:00" / "06:30-09:00,11:00-17:00" / "-" for one day. Invalid input
// writes nothing and sends the current program back, so HA shows what the heat pump has.
inline bool handleTimeProgramCommand(int prog, int day, const std::string& payload) {
    if (prog < 0 || prog >= TP_COUNT || day < 0 || day >= dhwprog::DAYS) return false;
    if (!dhwProgramWritable()) {
        ESP_LOGW("PROGRAM", "time programs are read-only on this model");
        return false;
    }
    std::vector<dhwprog::Period> periods;
    std::string err;
    if (!dhwprog::parsePeriods(payload, periods, err)) {
        ESP_LOGW("PROGRAM", "%s%s: \"%s\" rejected: %s", TIME_PROGRAMS[prog].prefix, dhwprog::DAY_CODES[day],
                 payload.c_str(), err.c_str());
        timeProgramPublish(prog, day, true);
        return false;
    }
    DhwProgramDay& d = timeProgram[prog][day];
    for (int slot = 0; slot < dhwprog::PERIODS; slot++) {
        const std::string want = slot < static_cast<int>(periods.size()) ? dhwprog::formatPeriod(periods[slot]) : "";
        if ((d.known & (1u << slot)) && d.period[slot] == want) continue;   // unchanged
        const std::string name = dhwprog::signalName(day, slot, TIME_PROGRAMS[prog].prefix);
        const CanMember* member = &CanMembers[TIME_PROGRAMS[prog].member];
        queuePacedWrite(member, member, GetElsterIndex(name.c_str()), want.c_str());
    }
    return true;
}
inline bool handleDhwProgramCommand(int day, const std::string& payload) {
    return handleTimeProgramCommand(TP_DHW, day, payload);
}
inline bool handleHeatingProgramCommand(int day, const std::string& payload) {
    return handleTimeProgramCommand(TP_HEATING, day, payload);
}
inline bool handleHeating2ProgramCommand(int day, const std::string& payload) {
    return handleTimeProgramCommand(TP_HEATING2, day, payload);
}

// ============================================================================
// MQTT SET COMMANDS: one entry point for every "<root>/<A>/<B>/set" topic (#92)
// ============================================================================
// <A>/<B> is either <MEMBER>/<SIGNAL> (writable numbers and selects, looked up by signal
// name and member) or <program>/<day> (time programs). Adding or removing a writable
// entity only touches its table row. common.yaml subscribes "<root>/+/+/set" once.
// Topic root: HA_TOPIC_ROOT in config.h, from the device_name substitution (#102).
// Set from common.yaml: refreshes the native SG Ready entities after a change
static std::function<void()> onSgReadyChanged;

inline void notifySgReadyChanged() {
    scheduleSgReadyPublish();
    if (onSgReadyChanged) onSgReadyChanged();
}

inline bool memberFromName(const std::string& name, CanMemberType& out) {
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
        if (name == CanMembers[i].Name) { out = static_cast<CanMemberType>(i); return true; }
    return false;
}

// Topic member accepted for a number: its discovery member, its table owner, and on WPL9
// the former KESSEL topics of the DHW setpoints.
inline bool numberAcceptsMember(const WritableNumberConfig& c, CanMemberType member) {
    if (getWritableNumberMember(c) == &CanMembers[member] || c.member == member) return true;
    return member == cm_kessel && kModel.canBDhwRoutes &&
           (strcmp(c.signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0 ||
            strcmp(c.signalName, "EINSTELL_SPEICHERSOLLTEMP2") == 0);
}

inline bool handleNumberCommand(const WritableNumberConfig& c, CanMemberType member, const std::string& payload) {
    if (!writableNumberApplies(c)) {
        ESP_LOGW("MQTT_CMD", "%s is not writable on %s", c.signalName, HA_DEVICE_MODEL_STR);
        return false;
    }
    publishWritableNumberDiscovery(c);
    char* end;
    const float v = strtof(payload.c_str(), &end);
    if (end == payload.c_str() || *end != '\0' || v < c.min - 0.001f || v > c.max + 0.001f) {
        ESP_LOGW("MQTT_CMD", "%s: \"%s\" outside %.2f..%.2f", c.signalName, payload.c_str(), c.min, c.max);
        return false;
    }
    // SG Ready values are kept on the ESP, not written to the heat pump
    if (strcmp(c.signalName, "SG_READY_BOOST_STATE3") == 0) { setSgReadyBoost3(v); notifySgReadyChanged(); return true; }
    if (strcmp(c.signalName, "SG_READY_BOOST_STATE4") == 0) { setSgReadyBoost4(v); notifySgReadyChanged(); return true; }
    if (strcmp(c.signalName, "SG_READY_ROOM_BOOST_STATE3") == 0 || strcmp(c.signalName, "SG_READY_ROOM_BOOST_STATE4") == 0) {
        const bool ok = handleSgReadyRoomBoost(c.signalName[25] == '3' ? 3 : 4, payload);
        if (ok) notifySgReadyChanged();
        return ok;
    }
    writeConfiguredSignal(member, c.signalName, payload.c_str());
    return true;
}

inline bool handleSelectCommand(const WritableSelectConfig& c, const std::string& payload) {
    publishWritableSelectDiscovery(c);
    if (strcmp(c.signalName, "SG_READY_STATE") == 0) {
        const int state = SgReadyController::parseStateString(payload);
        if (state < SgReadyController::STATE_EVU_LOCK || state > SgReadyController::STATE_FORCED) {
            ESP_LOGW("MQTT_CMD", "Invalid SG Ready state: %s", payload.c_str());
            return false;
        }
        applySgReadyState(state);
        if (onSgReadyChanged) onSgReadyChanged();
        return true;
    }
    if (strcmp(c.signalName, "SG_READY_NORMAL_MODE") == 0) {
        const bool ok = handleSgReadyNormalMode(payload);
        if (!ok) ESP_LOGW("MQTT_CMD", "Invalid SG Ready state 2 mode: %s", payload.c_str());
        return ok;
    }
    const char* v = payload.c_str();
    writeSignal(&CanMembers[c.member], GetElsterIndex(c.signalName), v);
    readSignal(&CanMembers[c.member], c.signalName);
    return true;
}

// "<root>/<A>/<B>/set" → number, select or time program. Returns false if nothing matched
// or the value was rejected.
inline bool handleSetCommand(const std::string& topic, const std::string& payload) {
    std::vector<std::string> parts;
    size_t pos = 0;
    while (true) {
        const size_t slash = topic.find('/', pos);
        parts.push_back(topic.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos));
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    if (parts.size() != 4 || parts[0] != HA_TOPIC_ROOT || parts[3] != "set") return false;
    ESP_LOGI("MQTT_CMD", "%s/%s = %s", parts[1].c_str(), parts[2].c_str(), payload.c_str());
    for (int prog = 0; prog < TP_COUNT; prog++) {
        if (parts[1] != TIME_PROGRAMS[prog].topic) continue;
        for (int day = 0; day < dhwprog::DAYS; day++)
            if (parts[2] == TIME_PROGRAM_KEYS[day]) return handleTimeProgramCommand(prog, day, payload);
        return false;
    }
    CanMemberType member;
    if (!memberFromName(parts[1], member)) {
        ESP_LOGW("MQTT_CMD", "Unknown member in %s", topic.c_str());
        return false;
    }
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        const WritableNumberConfig& c = writableNumbers[i];
        if (parts[2] == c.signalName && numberAcceptsMember(c, member)) return handleNumberCommand(c, member, payload);
    }
    for (size_t i = 0; i < WRITABLE_SELECT_COUNT; i++) {
        const WritableSelectConfig& c = writableSelects[i];
        if (parts[2] == c.signalName && c.member == member) return handleSelectCommand(c, payload);
    }
    ESP_LOGW("MQTT_CMD", "No writable entity for %s", topic.c_str());
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
constexpr uint32_t HASH_WPVORLAUFIST = hash("WPVORLAUFIST");
constexpr uint32_t HASH_RUECKLAUFISTTEMP = hash("RUECKLAUFISTTEMP");
constexpr uint32_t HASH_VERDICHTER = hash("VERDICHTER");
constexpr uint32_t HASH_PUMPENSTATUS = hash("PUMPENSTATUS");
constexpr uint32_t HASH_EINSTELL_SPEICHERSOLLTEMP = hash("EINSTELL_SPEICHERSOLLTEMP");
constexpr uint32_t HASH_RAUMSOLLTEMP_I = hash("RAUMSOLLTEMP_I");
constexpr uint32_t HASH_ZEITDAUER_LETZTE_ABTAUUNG = hash("ZEITDAUER_LETZTE_ABTAUUNG");
constexpr uint32_t HASH_FEHLERMELDUNG = hash("FEHLERMELDUNG");
constexpr uint32_t HASH_EVU_SPERRE_AKTIV = hash("EVU_SPERRE_AKTIV");
constexpr uint32_t HASH_FERNBEDIENUNGSZUORDNUNG = hash("FERNBEDIENUNGSZUORDNUNG");
constexpr uint32_t HASH_SOMMERBETRIEB = hash("SOMMERBETRIEB");
constexpr uint32_t HASH_SW_AUSSENTEMP = hash("SW_AUSSENTEMP");
constexpr uint32_t HASH_GEBAEUDEART = hash("GEBAEUDEART");
constexpr uint32_t HASH_AUSSENTEMP = hash("AUSSENTEMP");
constexpr uint32_t HASH_KESSELSOLLTEMP = hash("KESSELSOLLTEMP");
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



// WPL13E: the WPMW II polls the IWS runtime block itself, so these answers appear
// on the bus although we don't request them. Single-compressor unit without
// cooling: compressor 1 and cooling counters are only 0 / -255 (HA history).
// (The former "MANAGER x = 0.0" copies were read requests, filtered in processAndUpdate.)
static bool isIgnoredOnWpl13e(const char* name) {
    if (!kModel.ignoreUnusedIwsRuntime) return false;
    return strncmp(name, "LZ_VERD_", 8) == 0 && (strstr(name, "VERD_1") || strstr(name, "KUEHL"));
}

// ============================================================================
// SUMMER MODE (WPMW II manual, parameter 10): with the automatic on, heating
// stops while the damped outside temperature is >= the threshold, and resumes
// 1 K below it. Damping = averaging over 24/48/72 h (GEBAEUDEART 1/2/3); we use
// an exponential moving average and keep it in NVS across reboots.
// ============================================================================
inline float emaStep(float ema, float x, float dtHours, float tauHours) {
    if (std::isnan(ema)) return x;
    float a = dtHours / (tauHours + dtHours);
    return ema + a * (x - ema);
}

inline bool summerDecision(bool automatic, float ema, float threshold, bool wasActive, float hysteresis = 1.0f) {
    if (!automatic || std::isnan(ema) || std::isnan(threshold)) return false;
    return wasActive ? ema >= threshold - hysteresis : ema >= threshold;
}

struct SummerState {
    float ema = NAN;
    float threshold = NAN;
    int automatic = -1;          // -1 unknown, 0 off, 1 on
    int buildingType = 1;        // GEBAEUDEART 1..3
    bool active = false;
    unsigned long lastUpdateMs = 0, lastSaveMs = 0;
};
static SummerState summer;
static ESPPreferenceObject nvsSummerEma;

void initSummerPrefs() {
    nvsSummerEma = global_preferences->make_preference<float>(0x50AA0E3A, true);
    float v;
    if (nvsSummerEma.load(&v) && !std::isnan(v) && v > -50.0f && v < 60.0f) summer.ema = v;
}

static const CalculatedSensorConfig* findCalculatedSensor(const char* uid) {
    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; i++)
        if (strcmp(calculatedSensors[i].uniqueId, uid) == 0) return &calculatedSensors[i];
    return nullptr;
}

void publishSummerState() {
    if (std::isnan(summer.threshold) || summer.automatic < 0 || std::isnan(summer.ema)) return;
    summer.active = summerDecision(summer.automatic == 1, summer.ema, summer.threshold, summer.active);
    const CalculatedSensorConfig* active = findCalculatedSensor("stiebel_calculated_sommerbetrieb_aktiv");
    const CalculatedSensorConfig* damped = findCalculatedSensor("stiebel_calculated_aussentemp_gedaempft");
    publishCalculatedSensorDiscovery(*active, false);
    publishCalculatedSensorDiscovery(*damped, false);
    const char* st = summer.active ? "on" : "off";
    publishCalculatedState(active->stateTopic, st);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", summer.ema);
    publishCalculatedState(damped->stateTopic, buf);
}

// Outside temperature of the controller's own sensor (KESSEL on WPMW II, not the
// IWS air intake). Only used once the model provides the threshold.
void updateSummerOutsideTemp(float t, unsigned long now) {
    if (std::isnan(summer.threshold)) return;
    float dtHours = summer.lastUpdateMs ? (now - summer.lastUpdateMs) / 3600000.0f : 0.0f;
    summer.lastUpdateMs = now;
    summer.ema = emaStep(summer.ema, t, dtHours, 24.0f * summer.buildingType);
    if (now - summer.lastSaveMs > 30UL * 60UL * 1000UL) {
        nvsSummerEma.save(&summer.ema);
        summer.lastSaveMs = now;
    }
    publishSummerState();
}

// Short remedy for a decoded fault text (WPMW II manual fault list), shown as
// attribute of the "Störung" sensor. ponytail: keyword match, not per code.
const char* faultRemedy(const std::string& fault) {
    std::string up(fault);
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    auto has = [&](const char* k) { return up.find(k) != std::string::npos; };
    if (fault == "Keine Stoerung") return "";
    // Specific entries first (WPMW II manual fault list); "HD-SENSOR-MAX" is the high
    // pressure limit, not a sensor fault, so it must not fall through to "SENSOR".
    if (has("HD-SENSOR-MAX")) return "Hochdruck-Grenze erreicht: Heizungs-Volumenstrom zu klein oder Raumtemperatur/Heizkurve zu hoch.";
    if (has("LEISTUNG")) return "Hochdruck baut sich nicht auf: Phasenfolge und Sicherungen pruefen.";
    if (has("ABTAU")) return "Wasservolumenstrom oder Wassertemperatur beim Abtauen zu niedrig.";
    if (has("HEISSGAS")) return "Heissgas zu hoch: Einspritz-/Expansionsventil pruefen (Fachhandwerker).";
    if (has("SENSOR") || has("FUEHLER") || has("ERR T") || has("ERR AT")) return "Fuehler, Leitung und Steckverbindungen pruefen, ggf. tauschen.";
    if (has("DRUCK") || up == "ND") return "Volumenstrom und Fuehlerankopplung pruefen; bei Wiederholung Kaeltekreis pruefen lassen.";
    if (has("INV")) return "Inverterstoerung: Fachhandwerker.";
    return "Siehe Fehlerliste im Handbuch des Waermepumpenmanagers.";
}

void updateSensor(const CanMember &cm, const ElsterIndex *ei, const std::string &value)
{
    // Skip empty values
    if (value.empty()) {
        return;
    }
    pacedWriteNoteValue(cm, ei, value);
    if (isWpmw2Model() && timeProgramUpdate(cm, ei->Name, value)) return;   // shown as one text per day
    if (isIgnoredOnWpl13e(ei->Name)) return;
    if (isWpmw2Model() && strcmp(cm.Name, "HEIZMODUL") == 0) lastHeizmodulFrameMs = millis();
    // Bool fields carrying other bits decode to "?"; HA rejects that payload for a
    // binary sensor (log warning per poll) and keeps the last state anyway. Seen on
    // WPL13E: ABTAUUNGAKTIV = 0x0200 while the compressor runs (no defrost).
    if (value == "?" && (ei->Type == et_bool || ei->Type == et_little_bool)) return;

    // Per frame: no std::string temporaries. The name hash drives the member/signal
    // key (discovery, back-off) and the dispatch switch below.
    const char* signalName = ei->Name;
    const uint32_t signalHash = hash_runtime(signalName);
    const uint64_t key = signalKey(cm, signalHash);

    // -255 = the device does not provide this value: count it for the poll
    // back-off and create no HA entity until a real value has been seen.
    if (value == "-255") {
        uint8_t& streak = invalidStreak[key];
        if (streak < 255) streak++;
        if (streak >= 3) {
            ESP_LOGD("BACKOFF", "%s_%s streak=%u", cm.Name, signalName, streak);
        }
        if (discoveredSignals.find(key) == discoveredSignals.end()) return;
    } else if (!invalidStreak.empty()) {
        invalidStreak.erase(key);
    }

    // Invert boolean logic for EVU_SPERRE_AKTIV signal
    // CAN: 1 = lock inactive (off), 0 = lock active (on)
    // MQTT: "off" = lock inactive, "on" = lock active
    const char* publishValue = value.c_str();
    if (signalHash == HASH_EVU_SPERRE_AKTIV && strcmp(signalName, "EVU_SPERRE_AKTIV") == 0) {
        if (value == "on") {
            publishValue = "off";
        } else if (value == "off") {
            publishValue = "on";
        }
    }
    
    // Check if discovery is needed: mark as discovered and publish discovery immediately
    if (discoveredSignals.find(key) == discoveredSignals.end()) {
        discoveredSignals.emplace(key, DiscoveredSignal{&cm, ei});
        publishMqttDiscovery(cm, ei);
    }
    
    // Publish state (on change / heartbeat)
    publishMqttState(cm, ei, publishValue);
    
    // FE 7 assignment changed: the room temperature moves to the other circuit's climate
    if (signalHash == HASH_FERNBEDIENUNGSZUORDNUNG && isWpmw2Model() && strcmp(cm.Name, "MANAGER") == 0) {
        const long circuit = strtol(value.c_str(), nullptr, 10);
        const uint8_t next = (circuit == 1 || circuit == 2) ? static_cast<uint8_t>(circuit) : 0;
        if (next != remoteControlCircuit) {
            remoteControlCircuit = next;
            publishClimateDiscoveries();
        }
    }

    // Fast signal dispatch using compile-time hash (O(1) switch/jump table)
    
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
        
        case HASH_WPVORLAUFIST: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                // Delta T: only the heat pump's own sensors (IWS); other members are not mixed in
                if (strcmp(cm.Name, "HEIZMODUL") == 0) lastWpVorlaufIst = floatValue;
                else if (strcmp(cm.Name, "KESSEL") == 0) lastWpVorlaufKessel = floatValue;
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
                if (strcmp(cm.Name, "HEIZMODUL") == 0) lastRuecklaufIstTemp = floatValue;
                if (strcmp(cm.Name, "KESSEL") == 0) lastKesselRuecklaufIst = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse RUECKLAUFISTTEMP value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_KESSELSOLLTEMP: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (strcmp(cm.Name, "KESSEL") == 0 && endPtr != value.c_str() && *endPtr == '\0') {
                lastKesselSollTemp = floatValue;
            }
            break;
        }

        case HASH_SOMMERBETRIEB:
            summer.automatic = (value == "on") ? 1 : (value == "off") ? 0 : -1;
            publishSummerState();
            break;

        case HASH_SW_AUSSENTEMP: {   // threshold, display parameter 10 AUSSENTEMP
            float v = strtof(value.c_str(), nullptr);
            if (v >= 5.0f && v <= 35.0f) { summer.threshold = v; publishSummerState(); }
            break;
        }

        case HASH_GEBAEUDEART: {
            int v = atoi(value.c_str());
            if (v >= 1 && v <= 3) summer.buildingType = v;
            break;
        }

        case HASH_AUSSENTEMP:
            if (strcmp(cm.Name, "KESSEL") == 0 && value != "-255")
                updateSummerOutsideTemp(strtof(value.c_str(), nullptr), millis());
            break;

        case HASH_FEHLERMELDUNG: {
            char topic[96], buf[192];
            snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/%s/FEHLERMELDUNG/attributes", cm.Name);
            snprintf(buf, sizeof(buf), "{\"abhilfe\":\"%s\"}", faultRemedy(value));
            mqttPublish(topic, buf, strlen(buf));
            break;
        }

        case HASH_ZEITDAUER_LETZTE_ABTAUUNG:
            // The IWS resets the last defrost duration to 0 when a defrost starts and
            // writes the new duration when it ends, so 0 marks a running defrost (#48).
            // ABTAUUNGAKTIV stayed off through three defrosts on a WPL13E.
            if (strcmp(cm.Name, "HEIZMODUL") == 0 && value != "-255") {
                publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_ABTAUUNG_LAEUFT], false);
                publishCalculatedState(calculatedSensors[CALC_IDX_ABTAUUNG_LAEUFT].stateTopic,
                                       atoi(value.c_str()) == 0 ? "on" : "off");
            }
            break;

        case HASH_EINSTELL_SPEICHERSOLLTEMP:
        case HASH_RAUMSOLLTEMP_I: {
            SignalRoute route = {cm_manager, cm_manager, cm_manager};
            if (kModel.canBDhwRoutes)
                route = resolveSignalRoute(ei->Index, cm_manager);
            if (CanMembers[route.readMember].CanId != cm.CanId) break;
            char* endPtr;
            const float v = strtof(value.c_str(), &endPtr);
            if (endPtr == value.c_str() || *endPtr != '\0' || v < -50.0f) break;
            if (strcmp(signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0) lastSgDhwSetpoint = v;
            else if (strcmp(signalName, "RAUMSOLLTEMP_I") == 0) lastSgRoomSetpoint = v;
            break;
        }

        case HASH_PUMPENSTATUS:
            if (strcmp(cm.Name, "KESSEL") == 0 && value != "-255") lastPumpenstatus = atoi(value.c_str());
            break;

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
#ifndef HA_HAS_TWAI
    return;  // MCP2515 build without the TWAI driver: no TWAI status to report
#else
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
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_TEC]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_REC]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_BUS_ERRORS]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_STATE]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_RX_MISSED]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_RX_OVERRUN]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_RX_QUEUE]);

    char buf[16];
    auto pub = [&buf](const char* topic, uint32_t v) {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
        publishCalculatedState(topic, buf);
    };
    pub(HA_TOPIC_ROOT "/calculated/can_tec/state", status.tx_error_counter);
    pub(HA_TOPIC_ROOT "/calculated/can_rec/state", status.rx_error_counter);
    pub(HA_TOPIC_ROOT "/calculated/can_bus_errors/state", status.bus_error_count);
    // Frames lost because the RX queue was full / the hardware FIFO overran, and
    // frames waiting in the RX queue right now (loop latency indicator)
    pub(calculatedSensors[CALC_IDX_CAN_RX_MISSED].stateTopic, status.rx_missed_count);
    pub(calculatedSensors[CALC_IDX_CAN_RX_OVERRUN].stateTopic, status.rx_overrun_count);
    pub(calculatedSensors[CALC_IDX_CAN_RX_QUEUE].stateTopic, status.msgs_to_rx);

    const char* stateStr;
    switch (status.state) {
        case TWAI_STATE_STOPPED:    stateStr = "Gestoppt";  break;
        case TWAI_STATE_RUNNING:    stateStr = "Läuft";     break;
        case TWAI_STATE_BUS_OFF:    stateStr = "Bus-Off";   break;
        case TWAI_STATE_RECOVERING: stateStr = "Erholung";  break;
        default:                    stateStr = "Unbekannt"; break;
    }
    publishCalculatedState(HA_TOPIC_ROOT "/calculated/can_state/state", stateStr);
#endif  // HA_HAS_TWAI
}

// Failed MQTT publishes since boot (both CAN variants, not tied to TWAI)
void publishMqttDiagnostics() {
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_MQTT_PUBLISH_FAILURES]);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)mqttPublishFailures);
    publishCalculatedState(calculatedSensors[CALC_IDX_MQTT_PUBLISH_FAILURES].stateTopic, buf);
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
        publishRegelabweichung();
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
    
    // Check and publish CAN diagnostic sensors
    if (now >= nextCanDiagUpdate) {
        publishCanDiagnostics();
        publishMqttDiagnostics();
        nextCanDiagUpdate = now + (CALC_CAN_DIAG_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
}

// Timeout tracking removed for simplification
// Signals that don't respond will simply not update in Home Assistant

// Process signal request table with frequency-based scheduling
// ============================================================================
// BUS SCAN (MQTT heatingpump/scan/start "<MEMBER> [from to]", heatingpump/scan/stop)
// ============================================================================
// Asks one CAN member for every ElsterTable index (or a hex range) and publishes
// each index that answers with a real value (not 0x8000) to
// heatingpump/scan/<MEMBER>/<idx> (retained). One request per SCAN_GAP_MS; normal
// polling pauses meanwhile. Results go out at once, nothing is buffered.
static constexpr unsigned long SCAN_GAP_MS = 150;
static constexpr unsigned long SCAN_TAIL_MS = 1000;  // wait for the last answers
struct ScanState {
    bool active = false;
    const CanMember* cm = nullptr;
    bool table = true;
    uint32_t begin = 0, pos = 0, end = 0;
    unsigned long lastSend = 0, startMs = 0;
    uint16_t hits = 0;
};
static ScanState scanState;

void publishScanStatus(const char* state) {
    char buf[160];
    uint32_t total = scanState.end - scanState.begin + 1;
    uint32_t done = scanState.pos - scanState.begin;
    snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"member\":\"%s\",\"done\":%u,\"total\":%u,\"hits\":%u,\"seconds\":%lu}",
             state, scanState.cm ? scanState.cm->Name : "", (unsigned)done, (unsigned)total, scanState.hits,
             (millis() - scanState.startMs) / 1000UL);
    mqttPublish(HA_TOPIC_ROOT "/scan/status", buf, strlen(buf));
}

// payload: "HEIZMODUL" (all table indices) or "HEIZMODUL 0x0000 0x0fff" (range)
void startScan(const std::string& payload) {
    char member[24] = {0};
    unsigned from = 0, to = 0;
    int n = sscanf(payload.c_str(), "%23s %x %x", member, &from, &to);
    const CanMember* cm = nullptr;
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
        if (i != cm_pc && strcasecmp(CanMembers[i].Name, member) == 0) cm = &CanMembers[i];
    if (n < 1 || !cm || (n == 3 && (to < from || to > 0xFFFF))) {
        ESP_LOGW("SCAN", "Invalid scan request \"%s\" (expected \"<MEMBER> [from to]\")", payload.c_str());
        return;
    }
    scanState = ScanState{};
    scanState.active = true;
    scanState.cm = cm;
    scanState.table = (n != 3);
    scanState.begin = scanState.pos = scanState.table ? 0 : from;
    scanState.end = scanState.table ? ElsterTableCount - 1 : to;
    scanState.startMs = millis();
    ESP_LOGI("SCAN", "Scanning %s (%s), normal polling paused", cm->Name, scanState.table ? "table" : "range");
    publishScanStatus("running");
}

void stopScan(const char* state = "stopped") {
    if (!scanState.active) return;
    scanState.active = false;
    ESP_LOGI("SCAN", "Scan %s: %u hits", state, scanState.hits);
    publishScanStatus(state);
}

// Called every SCAN_GAP_MS from common.yaml
void scanStep() {
    if (!scanState.active || disableCanRequests) return;
    unsigned long now = millis();
    if (now - scanState.lastSend < SCAN_GAP_MS) return;
    if (scanState.pos > scanState.end) {
        if (now - scanState.lastSend >= SCAN_TAIL_MS) stopScan("done");
        return;
    }
    uint16_t idx = scanState.table ? ElsterTable[scanState.pos].Index : static_cast<uint16_t>(scanState.pos);
    scanState.pos++;
    readSignalRaw(scanState.cm, idx, "SCAN");
    scanState.lastSend = now;
    if (scanState.pos % 100 == 0) publishScanStatus("running");
}

// Returns true if the frame was a scan answer (then it is not processed further)
bool handleScanFrame(const CanMember& cm, const ElsterIndex* ei, const std::vector<uint8_t>& msg, const std::string& value) {
    if (!scanState.active || &cm != scanState.cm || (msg[0] & 0x0F) != 2) return false;
    bool longIdx = (msg[2] == 0xfa);
    uint16_t idx = longIdx ? (msg[3] << 8) | msg[4] : msg[2];
    uint16_t raw = longIdx ? (msg[5] << 8) | msg[6] : (msg[3] << 8) | msg[4];
    if (raw != 0x8000) {
        scanState.hits++;
        char topic[96], buf[128];
        snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/scan/%s/0x%04x", cm.Name, idx);
        snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"raw\":%u,\"value\":\"%s\"}",
                 ei->Index == idx ? ei->Name : "", raw, value.c_str());
        mqttPublish(topic, buf, strlen(buf));
    }
    return true;
}

// ============================================================================
// FAULT LIST (display "48 FEHLERLISTE"): the manager keeps its last 20 faults in
// FEHLERFELD_0..139 (0x0b00..0x0b8b), 7 fields per entry: minute, hour, day,
// month, year (2 digits), unused, fault code (ErrorList). Verified on a WPL13E
// against the display. Read slowly (1 field/s) every 30 min, published as one
// sensor: newest fault as state, all entries as JSON attribute.
// ============================================================================
static constexpr uint16_t FAULT_FIELD_FIRST = 0x0b00;
static constexpr int FAULT_ENTRIES = 20, FAULT_FIELDS = 7, FAULT_FIELD_COUNT = FAULT_ENTRIES * FAULT_FIELDS;
static constexpr unsigned long FAULT_CYCLE_MS = 30UL * 60UL * 1000UL;
struct FaultListState {
    uint16_t raw[FAULT_FIELD_COUNT] = {0};
    bool seen[FAULT_FIELD_COUNT] = {false};
    int pos = -1;                       // next field to request, -1 = idle
    unsigned long nextCycle = 60000UL;  // first cycle 1 min after boot
    unsigned long publishAt = 0;
};
static FaultListState faultList;

struct FaultEntry { int year, month, day, hour, minute, code; };

// Decode the 20 entries from the raw fields, newest first. Entries with
// implausible dates or code 0 are skipped.
std::vector<FaultEntry> decodeFaultList(const uint16_t raw[], const bool seen[]) {
    std::vector<FaultEntry> out;
    for (int e = 0; e < FAULT_ENTRIES; e++) {
        const int b = e * FAULT_FIELDS;
        bool all = true;
        for (int f = 0; f < FAULT_FIELDS; f++) all = all && seen[b + f] && raw[b + f] != 0x8000;
        if (!all) continue;
        FaultEntry x{2000 + raw[b + 4], raw[b + 3], raw[b + 2], raw[b + 1], raw[b + 0], raw[b + 6]};
        if (x.code == 0 || x.month < 1 || x.month > 12 || x.day < 1 || x.day > 31 || x.hour > 23 || x.minute > 59) continue;
        out.push_back(x);
    }
    std::sort(out.begin(), out.end(), [](const FaultEntry& a, const FaultEntry& b) {
        return std::tie(a.year, a.month, a.day, a.hour, a.minute) > std::tie(b.year, b.month, b.day, b.hour, b.minute);
    });
    return out;
}

std::string faultText(int code) {
    char buf[32];
    SetValueType(buf, et_err_nr, static_cast<unsigned short>(code));
    return buf;
}

void publishFaultList() {
    std::vector<FaultEntry> entries = decodeFaultList(faultList.raw, faultList.seen);
    if (entries.empty()) return;   // model without this list, or nothing read yet
    static bool discovered = false;
    if (!discovered) {
        const std::string cfgStr = "{\"name\":\"" LNAME_CALC_FEHLERLISTE "\",\"unique_id\":\"stiebel_calculated_fehlerliste\"," +
            defaultEntityIdJson("sensor", "stiebel_calculated_fehlerliste") +
            "\"state_topic\":\"" HA_TOPIC_ROOT "/calculated/fehlerliste/state\","
            "\"json_attributes_topic\":\"" HA_TOPIC_ROOT "/calculated/fehlerliste/attributes\","
            "\"availability_topic\":\"" HA_TOPIC_ROOT "/status\",\"icon\":\"mdi:clipboard-alert-outline\","
            HA_MAIN_DEVICE_JSON "}";
        mqttPublish("homeassistant/sensor/" HA_TOPIC_ROOT "/stiebel_calculated_fehlerliste/config", cfgStr.c_str(), cfgStr.size());
        discovered = true;
    }
    std::ostringstream a;
    char t[24];
    const FaultEntry& n = entries.front();
    snprintf(t, sizeof(t), "%04d-%02d-%02d %02d:%02d", n.year, n.month, n.day, n.hour, n.minute);
    std::string newest = faultText(n.code);
    a << "{\"letzter_fehler_zeit\":\"" << t << "\",\"abhilfe\":\"" << faultRemedy(newest)
      << "\",\"anzahl\":" << entries.size() << ",\"eintraege\":[";
    for (size_t i = 0; i < entries.size(); i++) {
        const FaultEntry& x = entries[i];
        snprintf(t, sizeof(t), "%04d-%02d-%02d %02d:%02d", x.year, x.month, x.day, x.hour, x.minute);
        a << (i ? "," : "") << "{\"zeit\":\"" << t << "\",\"fehler\":\"" << faultText(x.code) << "\",\"code\":" << x.code << "}";
    }
    a << "]}";
    std::string attrs = a.str();
    mqttPublish(HA_TOPIC_ROOT "/calculated/fehlerliste/attributes", attrs.c_str(), attrs.length());
    mqttPublish(HA_TOPIC_ROOT "/calculated/fehlerliste/state", newest.c_str(), newest.length());
}

// Called every second from common.yaml; pauses during a bus scan.
void faultListStep() {
    unsigned long now = millis();
    if (faultList.publishAt && now >= faultList.publishAt) {
        faultList.publishAt = 0;
        publishFaultList();
    }
    if (scanState.active || disableCanRequests) return;
    if (faultList.pos < 0) {
        if (now < faultList.nextCycle) return;
        faultList.pos = 0;
    }
    readSignalRaw(&CanMembers[cm_manager], FAULT_FIELD_FIRST + faultList.pos, "FEHLERLISTE");
    if (++faultList.pos >= FAULT_FIELD_COUNT) {
        faultList.pos = -1;
        faultList.nextCycle = now + FAULT_CYCLE_MS;
        faultList.publishAt = now + 3000;   // let the last answers arrive
    }
}

// Returns true if the frame was a fault-list field (stored, not published as entity)
bool handleFaultListFrame(const CanMember& cm, const std::vector<uint8_t>& msg) {
    if (&cm != &CanMembers[cm_manager] || (msg[0] & 0x0F) != 2 || msg[2] != 0xfa) return false;
    uint16_t idx = (msg[3] << 8) | msg[4];
    if (idx < FAULT_FIELD_FIRST || idx >= FAULT_FIELD_FIRST + FAULT_FIELD_COUNT) return false;
    int i = idx - FAULT_FIELD_FIRST;
    faultList.raw[i] = (msg[5] << 8) | msg[6];
    faultList.seen[i] = true;
    return true;
}

// Request table entries resolved once when the request manager starts: the
// ElsterIndex pointer (nullptr = unknown/blacklisted, skipped) and the schedule key
// per member. Avoids two name searches and string keys per entry and second.
static const CanMemberType REQUEST_ALL_MEMBERS[] = {cm_kessel, cm_manager, cm_heizmodul};
static constexpr size_t REQUEST_ALL_MEMBER_COUNT = sizeof(REQUEST_ALL_MEMBERS) / sizeof(REQUEST_ALL_MEMBERS[0]);
struct ResolvedRequest {
    const ElsterIndex* ei;
    uint64_t key[REQUEST_ALL_MEMBER_COUNT];   // [0] only for a single member
};
static std::vector<ResolvedRequest> resolvedRequests;

void resolveSignalRequests() {
    resolvedRequests.assign(SIGNAL_REQUEST_COUNT, ResolvedRequest{nullptr, {0, 0, 0}});
    for (size_t i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
        const SignalRequest& req = signalRequests[i];
        const ElsterIndex* ei = GetElsterIndex(req.signalName);
        if (!ei || ei->Index == 0xFFFF || ei->isBlacklisted) continue;  // unknown or blacklisted
        ResolvedRequest& r = resolvedRequests[i];
        r.ei = ei;
        const uint32_t h = elsterNameHash(ei->Name);
        if (req.member == cm_other) {
            for (size_t m = 0; m < REQUEST_ALL_MEMBER_COUNT; m++)
                r.key[m] = signalKey(CanMembers[REQUEST_ALL_MEMBERS[m]], h);
        } else {
            r.key[0] = signalKey(CanMembers[req.member], h);
        }
    }
}

void processSignalRequests() {
    unsigned long now = millis();
    if (scanState.active) return;  // bus scan owns the request slot

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
        resolveSignalRequests();
        
        // Initialize all signal schedules with random offsets to spread out initial load
        ESP_LOGI("REQUEST_MGR", "Initializing signal schedules with random offsets to prevent burst");
        for (size_t i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
            const SignalRequest& req = signalRequests[i];
            const ResolvedRequest& r = resolvedRequests[i];
            if (!r.ei) continue;   // unknown or blacklisted
            
            unsigned long intervalMs = req.frequency * 1000UL;
            const size_t members = (req.member == cm_other) ? REQUEST_ALL_MEMBER_COUNT : 1;
            for (size_t m = 0; m < members; m++) {
                // Random offset between 0 and full interval using ESP32 hardware RNG
                nextRequestTime[r.key[m]] = now + getRandomInRange(0, intervalMs + 1);
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
    
    while (processedCount < (int)SIGNAL_REQUEST_COUNT && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION) {
        const SignalRequest& req = signalRequests[currentIndex];
        const ResolvedRequest& r = resolvedRequests[currentIndex];
        processedCount++;
        
        const ElsterIndex* ei = r.ei;
        if (!ei) {
            // Unknown or blacklisted: never requested on the CAN bus
            currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
            continue;
        }
        
        unsigned long intervalMs = req.frequency * 1000UL;
        // Calculate next scheduled time with random offset (0 to 5% of interval)
        // This keeps signals from synchronizing while staying close to target frequency
        unsigned long maxJitter = (intervalMs / 20); // 5% of interval
        if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
        
        // Determine which members to request from: cm_other = all members (each
        // tracked independently), but only ONE of them per iteration to prevent bursts
        const size_t members = (req.member == cm_other) ? REQUEST_ALL_MEMBER_COUNT : 1;
        for (size_t m = 0; m < members && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION; m++) {
            const CanMember* member = (req.member == cm_other) ? &CanMembers[REQUEST_ALL_MEMBERS[m]]
                                                               : &CanMembers[req.member];
            const uint64_t key = r.key[m];
            
            // Check if this signal is overdue (current time >= scheduled time)
            if (now >= nextRequestTime[key]) {
                readSignal(member, ei);
                requestsSentThisIteration++;
                unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                nextRequestTime[key] = now + backoffIntervalMs(key, intervalMs) + randomDelay;
                // Other members of a cm_other group are checked in later iterations
                break;
            }
        }
        
        // Move to next signal in round-robin fashion (wrap around at end)
        currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
    }

    // Continue after the last checked signal next time. Without this every call started
    // at index 0 and, with more due requests than one per second, the end of the table
    // was never polled.
    signalProcessingStartIndex = currentIndex;
}

void processAndUpdate(uint32_t can_id, const std::vector<uint8_t> &msg)
{
    // processCanMessage() leaves the member unset for short frames
    if (msg.size() < 7) return;
    // Elster telegram type = low nibble of byte 0 (0 write, 1 read request, 2 response).
    // A read request carries no value: decoding it attributed a fake 0 to the
    // requesting node (e.g. the WPMW II polling the IWS -> "MANAGER VERDICHTER 0.0").
    if ((msg[0] & 0x0F) == 1) return;

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

    if (handleScanFrame(*cm, ei, msg, value)) return;
    if (handleFaultListFrame(*cm, msg)) return;

    // Skip permanently blacklisted signals (flag of the decoded row, no name lookup)
    if (ei->isBlacklisted)
    {
        return; // Reject before lookup, parsing, formatting, logging
    }

    updateSensor(*cm, ei, value);
    return;
}

void updateTime(CanMember cm, const char *str_time)
{
    ESP_LOGI("WRITE UHRZEIT VIA BUTTON", "%s", str_time);

    // WPMW II: the manager ignores the single STUNDE/MINUTE/SEKUNDE writes but accepts UHRZEIT
    // (0x0009, hour and minute in one value); the heat pump takes the time over and the seconds
    // restart at 0. Confirmed on a TTL 33 with WPMW II, software 6518 (#68). While the clock menu
    // is open on the display, the display shows "--.--" until the menu is left.
    if (isWpmw2Model()) {
        char hhmm[6];
        strncpy(hhmm, str_time, 5);   // "HH:MM:SS" from input_datetime -> "HH:MM"
        hhmm[5] = '\0';
        ESP_LOGI("WRITE", "UHRZEIT: %s", hhmm);
        writeSignal(&cm, "UHRZEIT", hhmm);
        readSignal(&cm, "UHRZEIT");
        return;
    }

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

    // WPMW II: DATUM (0x000a, day and month in one value) works but resets JAHR to 0 (2000);
    // writing JAHR (two digits) right after DATUM fixes it, and the manager recalculates the
    // weekday itself. Confirmed on a TTL 33 with WPMW II, software 6518 (#68).
    if (isWpmw2Model()) {
        char ddmm[7];   // "YYYY-MM-DD" from input_datetime -> "DD.MM."
        ddmm[0] = str_date[8]; ddmm[1] = str_date[9]; ddmm[2] = '.';
        ddmm[3] = str_date[5]; ddmm[4] = str_date[6]; ddmm[5] = '.';
        ddmm[6] = '\0';
        char yy[3] = {str_date[2], str_date[3], '\0'};
        ESP_LOGI("WRITE", "DATUM: %s, JAHR: %s", ddmm, yy);
        writeSignal(&cm, "DATUM", ddmm);
        writeSignal(&cm, "JAHR", yy);
        readSignal(&cm, "DATUM");
        readSignal(&cm, "JAHR");
        return;
    }
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


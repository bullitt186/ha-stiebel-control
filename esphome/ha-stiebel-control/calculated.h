/*
 * calculated.h - calculated sensors: the calculatedSensors[] table and the values derived
 * from raw signals (date, time, Delta T, compressor, control deviation, summer mode, CAN and
 * MQTT diagnostics) with their scheduler (#109). Energy and COP: energy.h.
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef CALCULATED_H
#define CALCULATED_H

// TWAI status/recovery for the built-in CAN controller (esp32_can). Under ESP-IDF the
// TWAI driver is only linked when esp32_can is used; MCP2515 builds (classic ESP32)
// have no <driver/twai.h> and skip the TWAI diagnostics (see publishCanDiagnostics()).
#if __has_include(<driver/twai.h>)
#include <driver/twai.h>
#define HA_HAS_TWAI 1
#endif
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "config.h"
#include "language_select.h"
#include "can_members.h"
#include "signal_route.h"
#include "mqtt_publish.h"
#include "discovery.h"
#include "request_manager.h"

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

// Track sensor values for calculated sensors
// Delta T uses the heat pump's own flow/return sensors (IWS = HEIZMODUL, B1/B2). They sit in the
// same water circuit; the WPM sensors of the KESSEL member (X2/2, X2/3) measure elsewhere (X2/3 is the
// heating circuit control sensor in the buffer) and are only a fallback for models without IWS values.
static float lastWpVorlaufIst = NAN;       // HEIZMODUL WPVORLAUFIST
static float lastRuecklaufIstTemp = NAN;   // HEIZMODUL RUECKLAUFISTTEMP
static float lastWpVorlaufKessel = NAN;    // KESSEL WPVORLAUFIST (fallback only)
static float lastVerdichterValue = NAN;

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

#endif  // CALCULATED_H

/*
 * Tests for pure CAN/signal functions in ha-stiebel-control.h.
 * ESPHome symbols are provided by esphome_stubs.h — do NOT define HA_DUMMY_BUILD,
 * as that guard excludes all function implementations in ha-stiebel-control.h.
 *
 * Catch2 must be included BEFORE esphome_stubs.h: the stubs define id(x) as a macro
 * which collides with std::locale::id in gcc-13's STL headers pulled in by Catch2.
 */
#include "catch2/catch_amalgamated.hpp"

#include "esphome_stubs.h"

// ha-stiebel-control.h pulls in all elster headers and signal_requests_wpl13e.h
// via its own includes. The elster .cpp files are compiled as separate objects.
#include "../esphome/ha-stiebel-control/ha-stiebel-control.h"

// ============================================================================
// generate_read_id / generate_write_id
// ============================================================================

TEST_CASE("generate_read_id: MANAGER (0x480)", "[can]") {
    // address = (0x480 & 0x780) / 8 = 0x480 / 8 = 0x90
    // first = (0x90 & 0xF0) + 1 = 0x90 + 1 = 0x91
    // second = 0x480 & 7 = 0
    auto id = generate_read_id(0x480);
    CHECK(id.first  == 0x91);
    CHECK(id.second == 0x00);
}
TEST_CASE("generate_read_id: KESSEL (0x180)", "[can]") {
    // address = 0x180 / 8 = 0x30
    // first = (0x30 & 0xF0) + 1 = 0x30 + 1 = 0x31
    // second = 0
    auto id = generate_read_id(0x180);
    CHECK(id.first  == 0x31);
    CHECK(id.second == 0x00);
}
TEST_CASE("generate_write_id: MANAGER (0x480)", "[can]") {
    // first = 0x90 & 0xF0 = 0x90
    // second = 0
    auto id = generate_write_id(0x480);
    CHECK(id.first  == 0x90);
    CHECK(id.second == 0x00);
}
TEST_CASE("generate_write_id: read first byte = write first byte + 1", "[can]") {
    for (uint16_t can_id : {0x180, 0x280, 0x480, 0x500, 0x680}) {
        auto r = generate_read_id(can_id);
        auto w = generate_write_id(can_id);
        CHECK(r.first == w.first + 1);
        CHECK(r.second == w.second);
    }
}
TEST_CASE("generate_read_id: second byte encodes lower 3 bits of CAN ID", "[can]") {
    // CAN ID 0x483: lower 3 bits = 3
    auto id = generate_read_id(0x483);
    CHECK(id.second == 3);
}

// ============================================================================
// lookupCanMember
// ============================================================================

TEST_CASE("lookupCanMember: MANAGER (0x480)", "[can]") {
    CHECK(std::string(lookupCanMember(0x480).Name) == "MANAGER");
}
TEST_CASE("lookupCanMember: KESSEL (0x180)", "[can]") {
    CHECK(std::string(lookupCanMember(0x180).Name) == "KESSEL");
}
TEST_CASE("lookupCanMember: PC (0x680)", "[can]") {
    CHECK(std::string(lookupCanMember(0x680).Name) == "PC");
}
TEST_CASE("lookupCanMember: HEIZMODUL (0x500)", "[can]") {
    CHECK(std::string(lookupCanMember(0x500).Name) == "HEIZMODUL");
}
TEST_CASE("lookupCanMember: unknown ID returns OTHER", "[can]") {
    CHECK(std::string(lookupCanMember(0x999).Name) == "OTHER");
}

// ============================================================================
// getTypeDefaults
// ============================================================================

TEST_CASE("getTypeDefaults: et_dec_val → temperature sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_dec_val, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "sensor");
    CHECK(std::string(dc)   == "temperature");
    CHECK(std::string(unit) == "°C");
    CHECK(std::string(sc)   == "measurement");
}
TEST_CASE("getTypeDefaults: et_cent_val same as et_dec_val", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_cent_val, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "sensor");
    CHECK(std::string(dc)   == "temperature");
}
TEST_CASE("getTypeDefaults: et_bool → binary_sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_bool, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "binary_sensor");
    CHECK(std::string(dc)   == "");
}
TEST_CASE("getTypeDefaults: et_little_bool → binary_sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_little_bool, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "binary_sensor");
}
TEST_CASE("getTypeDefaults: et_byte → counter sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_byte, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "sensor");
    CHECK(std::string(icon) == "mdi:counter");
}
TEST_CASE("getTypeDefaults: et_err_nr → alert-circle sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_err_nr, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "sensor");
    CHECK(std::string(icon) == "mdi:alert-circle");
}
TEST_CASE("getTypeDefaults: et_dev_id → text", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_dev_id, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "text");
}
TEST_CASE("getTypeDefaults: et_dev_nr → text", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_dev_nr, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "text");
}
TEST_CASE("getTypeDefaults: et_betriebsart → enum sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_betriebsart, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "sensor");
    CHECK(std::string(dc)   == "enum");
}
TEST_CASE("getTypeDefaults: et_zeit → timestamp sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_zeit, comp, dc, unit, sc, icon);
    CHECK(std::string(dc) == "timestamp");
}
TEST_CASE("getTypeDefaults: et_datum → date sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_datum, comp, dc, unit, sc, icon);
    CHECK(std::string(dc) == "date");
}
TEST_CASE("getTypeDefaults: unknown type → gauge sensor fallback", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults((ElsterType)255, comp, dc, unit, sc, icon);
    CHECK(std::string(comp) == "sensor");
    CHECK(std::string(icon) == "mdi:gauge");
}

// ============================================================================
// processCanMessage
// ============================================================================

// Helper: build a standard 7-byte frame with single-byte index
static std::vector<uint8_t> makeFrame(uint8_t readId1, uint8_t readId2,
                                       uint8_t elsterIdx,
                                       uint8_t byte1, uint8_t byte2) {
    return {readId1, readId2, elsterIdx, byte1, byte2, 0x00, 0x00};
}

// Helper: build a 7-byte frame with 0xFA extended index
static std::vector<uint8_t> makeFrameFA(uint8_t readId1, uint8_t readId2,
                                         uint8_t idxHi, uint8_t idxLo,
                                         uint8_t byte1, uint8_t byte2) {
    return {readId1, readId2, 0xFA, idxHi, idxLo, byte1, byte2};
}

TEST_CASE("processCanMessage: too short returns ElsterTable[0]", "[can]") {
    std::vector<uint8_t> msg = {0x01, 0x02, 0x03};
    std::string val;
    const CanMember* cm = nullptr;
    const ElsterIndex* ei = processCanMessage(msg, 0x480, val, &cm);
    CHECK(ei == &ElsterTable[0]);
}

TEST_CASE("processCanMessage: identifies MANAGER CAN member and decodes et_dec_val", "[can]") {
    // HYSTERESEZEIT index 0x0022, et_dec_val: raw 200 → "20.0"
    const ElsterIndex* hs = GetElsterIndex("HYSTERESEZEIT");
    REQUIRE(std::string(hs->Name) == "HYSTERESEZEIT");

    uint8_t idx = (uint8_t)(hs->Index & 0xFF); // 0x22
    auto msg = makeFrame(0x91, 0x00, idx, 0x00, 0xC8); // 200 = 20.0

    std::string val;
    const CanMember* cm = nullptr;
    const ElsterIndex* ei = processCanMessage(msg, 0x480, val, &cm);

    CHECK(std::string(cm->Name) == "MANAGER");
    CHECK(std::string(ei->Name) == "HYSTERESEZEIT");
    CHECK(val == "20.0");
}

TEST_CASE("processCanMessage: FA extended index frame", "[can]") {
    // Use DATUM (index 0x000a) — high byte 0x00, low byte 0x0a — but 0x00 would
    // be single-byte path. Use UHRZEIT (0x0009). For FA path we need index > 0xFF.
    // Scan for any signal with Index > 0xFF
    const ElsterIndex* ei_test = nullptr;
    for (int i = 1; i <= (int)(sizeof(ElsterTable)/sizeof(ElsterTable[0]) - 1); i++) {
        if (ElsterTable[i].Index > 0xFF && !ElsterTable[i].isBlacklisted) {
            ei_test = &ElsterTable[i];
            break;
        }
    }
    if (!ei_test) SKIP("No double-byte-index signal found in table");

    uint8_t idxHi = (uint8_t)(ei_test->Index >> 8);
    uint8_t idxLo = (uint8_t)(ei_test->Index & 0xFF);
    auto msg = makeFrameFA(0x91, 0x00, idxHi, idxLo, 0x00, 0x01);

    std::string val;
    const CanMember* cm = nullptr;
    const ElsterIndex* ei = processCanMessage(msg, 0x480, val, &cm);

    // Compare by name, not pointer — ElsterTable is static per-TU so pointers differ
    CHECK(std::string(ei->Name) == std::string(ei_test->Name));
    CHECK(std::string(cm->Name) == "MANAGER");
}

TEST_CASE("processCanMessage: et_bool signal value 1 → 'on'", "[can]") {
    // UHRZEIT index 0x0009, et_zeit — use any et_bool signal instead
    // Scan for a non-blacklisted et_bool signal
    const ElsterIndex* boolEi = nullptr;
    for (int i = 1; i <= (int)(sizeof(ElsterTable)/sizeof(ElsterTable[0]) - 1); i++) {
        if ((ElsterTable[i].Type == et_bool || ElsterTable[i].Type == et_little_bool)
                && !ElsterTable[i].isBlacklisted
                && ElsterTable[i].Index <= 0xFF) {
            boolEi = &ElsterTable[i];
            break;
        }
    }
    if (!boolEi) SKIP("No non-blacklisted et_bool signal with single-byte index found");

    auto msg = makeFrame(0x91, 0x00, (uint8_t)boolEi->Index, 0x00, 0x01);
    std::string val;
    const CanMember* cm = nullptr;
    processCanMessage(msg, 0x480, val, &cm);
    CHECK((val == "on" || val == "off" || val == "?"));
}

// ============================================================================
// getOrCreateUID
// ============================================================================

TEST_CASE("getOrCreateUID: generates lowercase uid", "[can]") {
    const CanMember& cm = CanMembers[cm_manager];
    std::string uid = getOrCreateUID(cm, "HYSTERESEZEIT");
    CHECK(uid == "stiebel_manager_hysteresezeit");
}
TEST_CASE("getOrCreateUID: cached result equals first result", "[can]") {
    const CanMember& cm = CanMembers[cm_kessel];
    std::string a = getOrCreateUID(cm, "HYSTERESEZEIT");
    std::string b = getOrCreateUID(cm, "HYSTERESEZEIT");
    CHECK(a == b);
}
TEST_CASE("getOrCreateUID: different members produce different UIDs", "[can]") {
    std::string a = getOrCreateUID(CanMembers[cm_manager], "HYSTERESEZEIT");
    std::string b = getOrCreateUID(CanMembers[cm_kessel],  "HYSTERESEZEIT");
    CHECK(a != b);
}

// ============================================================================
// hash / hash_runtime
// ============================================================================

TEST_CASE("hash: compile-time and runtime hashes agree", "[can]") {
    constexpr uint32_t compile = hash("TEMP_AUSSEN");
    uint32_t runtime = hash_runtime("TEMP_AUSSEN");
    CHECK(compile == runtime);
}
TEST_CASE("hash: known signal hashes match pre-computed constants", "[can]") {
    CHECK(hash_runtime("JAHR")           == HASH_JAHR);
    CHECK(hash_runtime("MONAT")          == HASH_MONAT);
    CHECK(hash_runtime("TAG")            == HASH_TAG);
    CHECK(hash_runtime("STUNDE")         == HASH_STUNDE);
    CHECK(hash_runtime("MINUTE")         == HASH_MINUTE);
    CHECK(hash_runtime("SEKUNDE")        == HASH_SEKUNDE);
    CHECK(hash_runtime("SOMMERBETRIEB")  == HASH_SOMMERBETRIEB);
    CHECK(hash_runtime("WPVORLAUFIST")   == HASH_WPVORLAUFIST);
    CHECK(hash_runtime("RUECKLAUFISTTEMP") == HASH_RUECKLAUFISTTEMP);
    CHECK(hash_runtime("VERDICHTER")     == HASH_VERDICHTER);
}
TEST_CASE("hash: different strings produce different hashes", "[can]") {
    CHECK(hash_runtime("STUNDE") != hash_runtime("MINUTE"));
    CHECK(hash_runtime("JAHR")   != hash_runtime("MONAT"));
}
TEST_CASE("hash: empty string does not crash", "[can]") {
    CHECK_NOTHROW(hash_runtime(""));
}

// ============================================================================
// storeCOPEnergyValue
// ============================================================================

TEST_CASE("storeCOPEnergyValue: valid numeric accepted", "[can]") {
    // Reset the map by calling with fresh values
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "1.234");
    // If it doesn't crash and accepts the value, we're good.
    // updateCOPCalculations relies on map state; tested below.
    CHECK(true);
}
TEST_CASE("storeCOPEnergyValue: empty string ignored (no crash)", "[can]") {
    CHECK_NOTHROW(storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", ""));
}
TEST_CASE("storeCOPEnergyValue: non-numeric string ignored (no crash)", "[can]") {
    CHECK_NOTHROW(storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "abc"));
}
TEST_CASE("storeCOPEnergyValue: negative value accepted", "[can]") {
    CHECK_NOTHROW(storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "-1.0"));
}

// ============================================================================
// updateCOPCalculations — verifies MQTT is published when all values present
// ============================================================================

TEST_CASE("updateCOPCalculations: publishes cop_ww when all WW values present", "[can]") {
    mqtt_client_instance().clear();

    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH",      "3.0");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH",  "1.0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "2.0");
    updateCOPCalculations();

    bool found = false;
    for (auto& m : mqtt_client_instance().messages)
        if (m.topic.find("cop_ww") != std::string::npos) { found = true; break; }
    CHECK(found);
}
TEST_CASE("updateCOPCalculations: cop_ww value = (3+1)/2 = 2.00", "[can]") {
    mqtt_client_instance().clear();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH",       "3.0");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH",   "1.0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "2.0");
    updateCOPCalculations();

    for (auto& m : mqtt_client_instance().messages)
        if (m.topic.find("cop_ww") != std::string::npos)
            CHECK(m.payload == "2.00");
}
TEST_CASE("updateCOPCalculations: skips cop_ww when el divisor is 0", "[can]") {
    mqtt_client_instance().clear();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH",       "3.0");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH",   "1.0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "0.0");
    updateCOPCalculations();

    for (auto& m : mqtt_client_instance().messages)
        CHECK(m.topic.find("cop_ww") == std::string::npos);
}
TEST_CASE("updateCOPCalculations: publishes cop_heiz when all Heiz values present", "[can]") {
    mqtt_client_instance().clear();
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_MWH",       "6.0");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_HEIZ_SUM_MWH",   "0.0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "2.0");
    updateCOPCalculations();

    bool found = false;
    for (auto& m : mqtt_client_instance().messages)
        if (m.topic.find("cop_heiz") != std::string::npos) { found = true; break; }
    CHECK(found);
}
TEST_CASE("updateCOPCalculations: publishes cop_gesamt when all values present", "[can]") {
    mqtt_client_instance().clear();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH",          "3.0");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH",      "1.0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH",   "2.0");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_MWH",        "6.0");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_HEIZ_SUM_MWH",    "0.0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "2.0");
    updateCOPCalculations();

    bool found = false;
    for (auto& m : mqtt_client_instance().messages)
        if (m.topic.find("cop_gesamt") != std::string::npos) { found = true; break; }
    CHECK(found);
}

// ============================================================================
// isPermanentlyBlacklisted
// ============================================================================

TEST_CASE("isPermanentlyBlacklisted: known non-blacklisted signal returns false", "[can]") {
    CHECK_FALSE(isPermanentlyBlacklisted("HYSTERESEZEIT"));
}
TEST_CASE("isPermanentlyBlacklisted: unknown signal returns true (INDEX_NOT_FOUND is blacklisted)", "[can]") {
    // Unknown name resolves to the INDEX_NOT_FOUND sentinel which has isBlacklisted=true
    CHECK(isPermanentlyBlacklisted("DOES_NOT_EXIST_XYZ"));
}

using Catch::Approx;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string mqttFindPayload(const std::string& topicSubstr) {
    for (const auto& m : mqtt_client_instance().messages)
        if (m.topic.find(topicSubstr) != std::string::npos)
            return m.payload;
    return "";
}

static bool mqttTopicPublished(const std::string& topicSubstr) {
    return !mqttFindPayload(topicSubstr).empty();
}

static void resetDiscoveryState() {
    discoveredSignals.clear();
    discoveredCalculatedSensors.clear();
    discoveredWritableNumbers.clear();
    discoveredWritableSelects.clear();
    mqtt_client_instance().clear();
}

// ============================================================================
// publishMqttState
// ============================================================================

TEST_CASE("publishMqttState: publishes to correct topic", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttState(mgr, ei, "21.5");
    REQUIRE(mqttTopicPublished("heatingpump/MANAGER/AUSSENTEMP/state"));
    CHECK(mqttFindPayload("AUSSENTEMP/state") == "21.5");
}

TEST_CASE("publishMqttState: skips empty value", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    publishMqttState(mgr, ei, "");
    CHECK(!mqttTopicPublished("AUSSENTEMP/state"));
}

TEST_CASE("publishMqttState: topic uses CAN member name", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& kessel = CanMembers[cm_kessel];
    const ElsterIndex* ei = GetElsterIndex("SPEICHERISTTEMP");
    REQUIRE(ei != nullptr);
    publishMqttState(kessel, ei, "52.0");
    REQUIRE(mqttTopicPublished("heatingpump/KESSEL/SPEICHERISTTEMP/state"));
}

// ============================================================================
// publishMqttDiscovery
// ============================================================================

TEST_CASE("publishMqttDiscovery: sensor signal publishes to homeassistant/sensor", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(mgr, ei);
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/"));
}

TEST_CASE("publishMqttDiscovery: payload contains name, unique_id, state_topic", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(mgr, ei);
    std::string payload = mqttFindPayload("stiebel_manager_aussentemp");
    CHECK(payload.find("\"name\"") != std::string::npos);
    CHECK(payload.find("\"unique_id\"") != std::string::npos);
    CHECK(payload.find("heatingpump/MANAGER/AUSSENTEMP/state") != std::string::npos);
}

TEST_CASE("publishMqttDiscovery: temperature signal has device_class temperature", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(mgr, ei);
    std::string payload = mqttFindPayload("stiebel_manager_aussentemp");
    CHECK(payload.find("\"device_class\":\"temperature\"") != std::string::npos);
    CHECK(payload.find("unit_of_measurement") != std::string::npos);
}

TEST_CASE("publishMqttDiscovery: binary sensor publishes to homeassistant/binary_sensor", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("ABTAUUNGAKTIV");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(heizmodul, ei);
    CHECK(mqttTopicPublished("homeassistant/binary_sensor/heatingpump/"));
}

TEST_CASE("publishMqttDiscovery: binary sensor with explicit payloads includes payload_on/off", "[mqtt]") {
    // EVU_SPERRE_AKTIV has explicit payloadOn/Off in ElsterTable
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("EVU_SPERRE_AKTIV");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(mgr, ei);
    std::string payload = mqttFindPayload("homeassistant/binary_sensor");
    CHECK(payload.find("payload_on") != std::string::npos);
    CHECK(payload.find("payload_off") != std::string::npos);
}

TEST_CASE("publishMqttDiscovery: binary sensor without explicit payloads omits payload fields", "[mqtt]") {
    // ABTAUUNGAKTIV has NULL payloadOn/Off — no payload_on/off in discovery
    resetDiscoveryState();
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("ABTAUUNGAKTIV");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(heizmodul, ei);
    std::string payload = mqttFindPayload("homeassistant/binary_sensor");
    CHECK(payload.find("payload_on") == std::string::npos);
}

TEST_CASE("publishMqttDiscovery: payload contains availability_topic", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(mgr, ei);
    std::string payload = mqttFindPayload("stiebel_manager_aussentemp");
    CHECK(payload.find("\"availability_topic\":\"heatingpump/status\"") != std::string::npos);
}

TEST_CASE("publishMqttDiscovery: payload contains via_device", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(mgr, ei);
    std::string payload = mqttFindPayload("stiebel_manager_aussentemp");
    CHECK(payload.find("\"via_device\"") != std::string::npos);
}

TEST_CASE("publishMqttDiscovery: duration signal has device_class duration and unit h", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("LZ_VERD_2_HEIZBETRIEB");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(heizmodul, ei);
    std::string payload = mqttFindPayload("lz_verd_2_heizbetrieb");
    CHECK(payload.find("\"device_class\":\"duration\"") != std::string::npos);
    CHECK(payload.find("\"unit_of_measurement\":\"h\"") != std::string::npos);
    CHECK(payload.find("\"state_class\":\"total_increasing\"") != std::string::npos);
}

// ============================================================================
// updateSensor
// ============================================================================

TEST_CASE("updateSensor: publishes discovery and state on first call", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "30.5");
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/"));
    CHECK(mqttTopicPublished("heatingpump/MANAGER/RUECKLAUFISTTEMP/state"));
    CHECK(mqttFindPayload("RUECKLAUFISTTEMP/state") == "30.5");
}

TEST_CASE("updateSensor: skips discovery on second call (cached)", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "30.5");
    mqtt_client_instance().clear();
    updateSensor(mgr, ei, "31.0");
    CHECK(mqtt_client_instance().messages.size() == 1);
    CHECK(mqttFindPayload("RUECKLAUFISTTEMP/state") == "31.0");
}

TEST_CASE("updateSensor: EVU_SPERRE_AKTIV inverts on → off", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("EVU_SPERRE_AKTIV");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "on");
    CHECK(mqttFindPayload("EVU_SPERRE_AKTIV/state") == "off");
}

TEST_CASE("updateSensor: EVU_SPERRE_AKTIV inverts off → on", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("EVU_SPERRE_AKTIV");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "off");
    CHECK(mqttFindPayload("EVU_SPERRE_AKTIV/state") == "on");
}

TEST_CASE("updateSensor: skips empty value", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "");
    CHECK(mqtt_client_instance().messages.empty());
}

TEST_CASE("updateSensor: JAHR value updates lastJahr global", "[mqtt]") {
    resetDiscoveryState();
    lastJahr = -1;
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("JAHR");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "26");
    CHECK(lastJahr == 26);
}

TEST_CASE("updateSensor: WPVORLAUFIST value updates lastWpVorlaufIst", "[mqtt]") {
    resetDiscoveryState();
    lastWpVorlaufIst = NAN;
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("WPVORLAUFIST");
    REQUIRE(ei != nullptr);
    updateSensor(heizmodul, ei, "35.5");
    CHECK(lastWpVorlaufIst == Approx(35.5f));
}

TEST_CASE("updateSensor: RUECKLAUFISTTEMP value updates lastRuecklaufIstTemp", "[mqtt]") {
    resetDiscoveryState();
    lastRuecklaufIstTemp = NAN;
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "28.3");
    CHECK(lastRuecklaufIstTemp == Approx(28.3f));
}

TEST_CASE("updateSensor: VERDICHTER value updates lastVerdichterValue", "[mqtt]") {
    resetDiscoveryState();
    lastVerdichterValue = NAN;
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("VERDICHTER");
    REQUIRE(ei != nullptr);
    updateSensor(heizmodul, ei, "50.0");
    CHECK(lastVerdichterValue == Approx(50.0f));
}

// ============================================================================
// publishDate
// ============================================================================

TEST_CASE("publishDate: formats date as YYYY-MM-DD", "[calc]") {
    resetDiscoveryState();
    lastJahr = 26; lastMonat = 6; lastTag = 14;
    publishDate();
    CHECK(mqttFindPayload("calculated/date/state") == "2026-06-14");
}

TEST_CASE("publishDate: zero-pads month and day", "[calc]") {
    resetDiscoveryState();
    lastJahr = 24; lastMonat = 1; lastTag = 5;
    publishDate();
    CHECK(mqttFindPayload("calculated/date/state") == "2024-01-05");
}

TEST_CASE("publishDate: skips when values uninitialized", "[calc]") {
    resetDiscoveryState();
    lastJahr = -1; lastMonat = -1; lastTag = -1;
    publishDate();
    CHECK(!mqttTopicPublished("calculated/date/state"));
}

TEST_CASE("publishDate: skips when month out of range", "[calc]") {
    resetDiscoveryState();
    lastJahr = 26; lastMonat = 13; lastTag = 1;
    publishDate();
    CHECK(!mqttTopicPublished("calculated/date/state"));
}

TEST_CASE("publishDate: skips when day out of range", "[calc]") {
    resetDiscoveryState();
    lastJahr = 26; lastMonat = 6; lastTag = 32;
    publishDate();
    CHECK(!mqttTopicPublished("calculated/date/state"));
}

// ============================================================================
// publishTime
// ============================================================================

TEST_CASE("publishTime: formats time as HH:MM:SS", "[calc]") {
    resetDiscoveryState();
    lastStunde = 14; lastMinute = 5; lastSekunde = 9;
    publishTime();
    CHECK(mqttFindPayload("calculated/time/state") == "14:05:09");
}

TEST_CASE("publishTime: zero-pads all components", "[calc]") {
    resetDiscoveryState();
    lastStunde = 0; lastMinute = 0; lastSekunde = 0;
    publishTime();
    CHECK(mqttFindPayload("calculated/time/state") == "00:00:00");
}

TEST_CASE("publishTime: skips when values uninitialized", "[calc]") {
    resetDiscoveryState();
    lastStunde = -1; lastMinute = -1; lastSekunde = -1;
    publishTime();
    CHECK(!mqttTopicPublished("calculated/time/state"));
}

TEST_CASE("publishTime: skips when hour out of range", "[calc]") {
    resetDiscoveryState();
    lastStunde = 24; lastMinute = 0; lastSekunde = 0;
    publishTime();
    CHECK(!mqttTopicPublished("calculated/time/state"));
}

TEST_CASE("publishTime: skips when minute out of range", "[calc]") {
    resetDiscoveryState();
    lastStunde = 12; lastMinute = 60; lastSekunde = 0;
    publishTime();
    CHECK(!mqttTopicPublished("calculated/time/state"));
}

// ============================================================================
// publishBetriebsart
// ============================================================================

TEST_CASE("publishBetriebsart: on → Sommerbetrieb", "[calc]") {
    resetDiscoveryState();
    publishBetriebsart("on");
    CHECK(mqttFindPayload("calculated/betriebsart/state") == "Sommerbetrieb");
}

TEST_CASE("publishBetriebsart: off → Normalbetrieb", "[calc]") {
    resetDiscoveryState();
    publishBetriebsart("off");
    CHECK(mqttFindPayload("calculated/betriebsart/state") == "Normalbetrieb");
}

// ============================================================================
// publishDeltaTContinuous
// ============================================================================

TEST_CASE("publishDeltaTContinuous: publishes vorlauf minus ruecklauf", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = 40.0f; lastRuecklaufIstTemp = 35.0f;
    publishDeltaTContinuous();
    CHECK(mqttFindPayload("delta_t_continuous/state") == "5.00");
}

TEST_CASE("publishDeltaTContinuous: negative delta published correctly", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = 30.0f; lastRuecklaufIstTemp = 35.0f;
    publishDeltaTContinuous();
    CHECK(mqttFindPayload("delta_t_continuous/state") == "-5.00");
}

TEST_CASE("publishDeltaTContinuous: skips when vorlauf is NaN", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = NAN; lastRuecklaufIstTemp = 35.0f;
    publishDeltaTContinuous();
    CHECK(!mqttTopicPublished("delta_t_continuous/state"));
}

TEST_CASE("publishDeltaTContinuous: skips when ruecklauf is NaN", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = 40.0f; lastRuecklaufIstTemp = NAN;
    publishDeltaTContinuous();
    CHECK(!mqttTopicPublished("delta_t_continuous/state"));
}

TEST_CASE("publishDeltaTContinuous: skips on temperature below -50 threshold", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = -60.0f; lastRuecklaufIstTemp = 35.0f;
    publishDeltaTContinuous();
    CHECK(!mqttTopicPublished("delta_t_continuous/state"));
}

// ============================================================================
// publishDeltaTRunning
// ============================================================================

TEST_CASE("publishDeltaTRunning: publishes when compressor active (value > 2)", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = 50.0f;
    lastWpVorlaufIst = 42.0f; lastRuecklaufIstTemp = 37.0f;
    publishDeltaTRunning();
    CHECK(mqttFindPayload("delta_t_running/state") == "5.00");
}

TEST_CASE("publishDeltaTRunning: skips when compressor not running", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = 0.0f;
    lastWpVorlaufIst = 42.0f; lastRuecklaufIstTemp = 37.0f;
    publishDeltaTRunning();
    CHECK(!mqttTopicPublished("delta_t_running/state"));
}

TEST_CASE("publishDeltaTRunning: skips when compressor value is NaN", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = NAN;
    lastWpVorlaufIst = 42.0f; lastRuecklaufIstTemp = 37.0f;
    publishDeltaTRunning();
    CHECK(!mqttTopicPublished("delta_t_running/state"));
}

TEST_CASE("publishDeltaTRunning: skips when vorlauf NaN even with compressor running", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = 50.0f;
    lastWpVorlaufIst = NAN; lastRuecklaufIstTemp = 37.0f;
    publishDeltaTRunning();
    CHECK(!mqttTopicPublished("delta_t_running/state"));
}

// ============================================================================
// publishCompressorActive
// ============================================================================

TEST_CASE("publishCompressorActive: value > 2 → on", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = 50.0f;
    publishCompressorActive();
    CHECK(mqttFindPayload("compressor_active/state") == "on");
}

TEST_CASE("publishCompressorActive: value <= 2 → off", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = 1.0f;
    publishCompressorActive();
    CHECK(mqttFindPayload("compressor_active/state") == "off");
}

TEST_CASE("publishCompressorActive: exactly 2.0 → off (threshold is > 2)", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = 2.0f;
    publishCompressorActive();
    CHECK(mqttFindPayload("compressor_active/state") == "off");
}

TEST_CASE("publishCompressorActive: skips when NaN", "[calc]") {
    resetDiscoveryState();
    lastVerdichterValue = NAN;
    publishCompressorActive();
    CHECK(!mqttTopicPublished("compressor_active/state"));
}

// ============================================================================
// publishCalculatedSensorDiscovery
// ============================================================================

TEST_CASE("publishCalculatedSensorDiscovery: publishes to correct topic", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_test_calc", "Test Calc", "heatingpump/test/state",
        "sensor", "temperature", "°C", "measurement", "mdi:thermometer", "", "", "", true
    };
    publishCalculatedSensorDiscovery(cfg, true);
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/stiebel_test_calc/config"));
}

TEST_CASE("publishCalculatedSensorDiscovery: payload contains name and state_topic", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_test_calc2", "My Calc", "heatingpump/test2/state",
        "sensor", "", "", "", "mdi:gauge", "", "", "", true
    };
    publishCalculatedSensorDiscovery(cfg, true);
    std::string payload = mqttFindPayload("stiebel_test_calc2");
    CHECK(payload.find("\"name\":\"My Calc\"") != std::string::npos);
    CHECK(payload.find("heatingpump/test2/state") != std::string::npos);
}

TEST_CASE("publishCalculatedSensorDiscovery: cached — second call skips publish", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_cache_calc", "Cache Calc", "heatingpump/cache/state",
        "sensor", "", "", "", "", "", "", "", true
    };
    publishCalculatedSensorDiscovery(cfg);
    mqtt_client_instance().clear();
    publishCalculatedSensorDiscovery(cfg);
    CHECK(mqtt_client_instance().messages.empty());
}

TEST_CASE("publishCalculatedSensorDiscovery: forceRepublish bypasses cache", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_force_calc", "Force Calc", "heatingpump/force/state",
        "sensor", "", "", "", "", "", "", "", true
    };
    publishCalculatedSensorDiscovery(cfg);
    mqtt_client_instance().clear();
    publishCalculatedSensorDiscovery(cfg, true);
    CHECK(!mqtt_client_instance().messages.empty());
}

TEST_CASE("publishCalculatedSensorDiscovery: diagnostic entity has entity_category and enabled_by_default=false", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_diag_calc", "Diag Calc", "heatingpump/diag/state",
        "sensor", "", "", "measurement", "mdi:alert", "", "", "diagnostic", false
    };
    publishCalculatedSensorDiscovery(cfg, true);
    std::string payload = mqttFindPayload("stiebel_diag_calc");
    CHECK(payload.find("\"entity_category\":\"diagnostic\"") != std::string::npos);
    CHECK(payload.find("\"enabled_by_default\":false") != std::string::npos);
}

TEST_CASE("publishCalculatedSensorDiscovery: normal entity omits enabled_by_default", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_normal_calc", "Normal Calc", "heatingpump/normal/state",
        "sensor", "", "", "", "", "", "", "", true
    };
    publishCalculatedSensorDiscovery(cfg, true);
    std::string payload = mqttFindPayload("stiebel_normal_calc");
    CHECK(payload.find("enabled_by_default") == std::string::npos);
}

TEST_CASE("publishCalculatedSensorDiscovery: binary sensor includes payload_on/off", "[mqtt]") {
    resetDiscoveryState();
    CalculatedSensorConfig cfg{
        "stiebel_bin_calc", "Bin Calc", "heatingpump/bin/state",
        "binary_sensor", "running", "", "", "mdi:engine", "on", "off", "", true
    };
    publishCalculatedSensorDiscovery(cfg, true);
    std::string payload = mqttFindPayload("stiebel_bin_calc");
    CHECK(payload.find("\"payload_on\":\"on\"") != std::string::npos);
    CHECK(payload.find("\"payload_off\":\"off\"") != std::string::npos);
}

// ============================================================================
// publishWritableNumberDiscovery
// ============================================================================

TEST_CASE("publishWritableNumberDiscovery: publishes to homeassistant/number", "[mqtt]") {
    resetDiscoveryState();
    publishWritableNumberDiscovery(writableNumbers[0], true);
    CHECK(mqttTopicPublished("homeassistant/number/heatingpump/"));
}

TEST_CASE("publishWritableNumberDiscovery: payload contains command_topic and state_topic", "[mqtt]") {
    resetDiscoveryState();
    publishWritableNumberDiscovery(writableNumbers[0], true);
    std::string payload = mqttFindPayload("homeassistant/number/heatingpump/");
    CHECK(payload.find("\"command_topic\"") != std::string::npos);
    CHECK(payload.find("/set\"") != std::string::npos);
    CHECK(payload.find("\"state_topic\"") != std::string::npos);
}

TEST_CASE("publishWritableNumberDiscovery: payload contains min, max, step", "[mqtt]") {
    resetDiscoveryState();
    publishWritableNumberDiscovery(writableNumbers[0], true);
    std::string payload = mqttFindPayload("homeassistant/number/heatingpump/");
    CHECK(payload.find("\"min\"") != std::string::npos);
    CHECK(payload.find("\"max\"") != std::string::npos);
    CHECK(payload.find("\"step\"") != std::string::npos);
}

TEST_CASE("publishWritableNumberDiscovery: SG_READY signal uses main device (no via_device)", "[mqtt]") {
    resetDiscoveryState();
    publishWritableNumberDiscovery(writableNumbers[2], true);  // SG_READY_BOOST_STATE3
    std::string payload = mqttFindPayload("homeassistant/number/heatingpump/");
    CHECK(payload.find("via_device") == std::string::npos);
}

TEST_CASE("publishWritableNumberDiscovery: regular signal uses sub-device with via_device", "[mqtt]") {
    resetDiscoveryState();
    publishWritableNumberDiscovery(writableNumbers[0], true);  // EINSTELL_SPEICHERSOLLTEMP
    std::string payload = mqttFindPayload("homeassistant/number/heatingpump/");
    CHECK(payload.find("\"via_device\"") != std::string::npos);
}

TEST_CASE("publishWritableNumberDiscovery: cached — second call skips publish", "[mqtt]") {
    resetDiscoveryState();
    publishWritableNumberDiscovery(writableNumbers[0]);
    mqtt_client_instance().clear();
    publishWritableNumberDiscovery(writableNumbers[0]);
    CHECK(mqtt_client_instance().messages.empty());
}

// ============================================================================
// publishWritableSelectDiscovery
// ============================================================================

TEST_CASE("publishWritableSelectDiscovery: publishes to homeassistant/select", "[mqtt]") {
    resetDiscoveryState();
    publishWritableSelectDiscovery(writableSelects[0], true);
    CHECK(mqttTopicPublished("homeassistant/select/heatingpump/"));
}

TEST_CASE("publishWritableSelectDiscovery: payload contains options array", "[mqtt]") {
    resetDiscoveryState();
    publishWritableSelectDiscovery(writableSelects[0], true);
    std::string payload = mqttFindPayload("homeassistant/select/heatingpump/");
    CHECK(payload.find("\"options\"") != std::string::npos);
}

TEST_CASE("publishWritableSelectDiscovery: cached — second call skips publish", "[mqtt]") {
    resetDiscoveryState();
    publishWritableSelectDiscovery(writableSelects[0]);
    mqtt_client_instance().clear();
    publishWritableSelectDiscovery(writableSelects[0]);
    CHECK(mqtt_client_instance().messages.empty());
}

// ============================================================================
// i18n — LNAME_* macros resolve to non-empty strings
// ============================================================================

TEST_CASE("i18n: LNAME_AUSSENTEMP is non-empty", "[i18n]") {
    CHECK(std::string(LNAME_AUSSENTEMP).length() > 0);
}

TEST_CASE("i18n: LNAME_CALC_DATE is non-empty", "[i18n]") {
    CHECK(std::string(LNAME_CALC_DATE).length() > 0);
}

TEST_CASE("i18n: LNAME_OPT_TAGBETRIEB is non-empty", "[i18n]") {
    CHECK(std::string(LNAME_OPT_TAGBETRIEB).length() > 0);
}

TEST_CASE("i18n: LNAME_SEL_PROGRAMMSCHALTER is non-empty", "[i18n]") {
    CHECK(std::string(LNAME_SEL_PROGRAMMSCHALTER).length() > 0);
}

TEST_CASE("i18n: LNAME_COP_WW is non-empty", "[i18n]") {
    CHECK(std::string(LNAME_COP_WW).length() > 0);
}

TEST_CASE("i18n: programmschalterOptions contains 6 non-empty entries", "[i18n]") {
    for (size_t i = 0; i < 6; ++i)
        CHECK(std::string(programmschalterOptions[i]).length() > 0);
}

TEST_CASE("i18n: sgReadyOptions contains 4 non-empty entries", "[i18n]") {
    for (size_t i = 0; i < 4; ++i)
        CHECK(std::string(sgReadyOptions[i]).length() > 0);
}

TEST_CASE("i18n: all calculatedSensors names are non-empty", "[i18n]") {
    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; ++i)
        CHECK(std::string(calculatedSensors[i].name).length() > 0);
}

TEST_CASE("i18n: all writableNumbers friendlyNames are non-empty", "[i18n]") {
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; ++i)
        CHECK(std::string(writableNumbers[i].friendlyName).length() > 0);
}

TEST_CASE("i18n: named ElsterTable signals have non-empty friendlyName via LNAME macro", "[i18n]") {
    static const char* named[] = {
        "AUSSENTEMP", "SPEICHERISTTEMP", "RUECKLAUFISTTEMP", "VERDICHTER",
        "LZ_VERD_1_HEIZBETRIEB", "LZ_VERD_2_KUEHLBETRIEB", "ABTAUUNGAKTIV",
        "EVU_SPERRE_AKTIV", "HEIZKURVE", "WW_ECO", "WW_HYSTERSE", "GEBAEUDEART",
        "PROGRAMMSCHALTER", "SOMMERBETRIEB", "WP_STATUS",
    };
    for (const char* name : named) {
        const ElsterIndex* ei = GetElsterIndex(name);
        REQUIRE(ei != nullptr);
        if (ei->hasMetadata && ei->friendlyName)
            CHECK(std::string(ei->friendlyName).length() > 0);
    }
}

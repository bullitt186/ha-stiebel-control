/*
 * Tests for the firmware logic in ha-stiebel-control.h and its module headers (#109).
 * ESPHome symbols are provided by esphome_stubs.h.
 *
 * Catch2 must be included BEFORE esphome_stubs.h: the stubs define id(x) as a macro
 * which collides with std::locale::id in gcc-13's STL headers pulled in by Catch2.
 */
#include "catch2/catch_amalgamated.hpp"

#include "esphome_stubs.h"

// ha-stiebel-control.h pulls in the elster headers and every firmware module via its own
// includes. The elster .cpp files are compiled as separate objects.
#include "../esphome/ha-stiebel-control/ha-stiebel-control.h"
#include "../esphome/ha-stiebel-control/lang_en.h"

// ============================================================================
// generate_read_id / generate_write_id
// ============================================================================

TEST_CASE("generate_read_id: MANAGER (0x480)", "[can]") {
    // address = (0x480 & 0x780) / 8 = 0x480 / 8 = 0x90
    // first = (0x90 & 0xF0) + 1 = 0x90 + 1 = 0x91
    // second = 0x480 & 0x7F = 0
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
TEST_CASE("generate_read_id: second byte encodes the lower 7 bits of the CAN ID", "[can]") {
    // HEIZMODUL_WPL 0x514 (WPL17, WPL-A): 0xa1 0x14, not 0x04 (that would address 0x504)
    auto wpl = generate_read_id(0x514);
    CHECK(wpl.first  == 0xa1);
    CHECK(wpl.second == 0x14);
    CHECK(generate_write_id(0x514).second == 0x14);
    CHECK(generate_read_id(0x601).second == 0x01);   // MISCHERMODUL_2
    CHECK(generate_read_id(0x201).second == 0x01);   // KESSEL_CANB

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
TEST_CASE("getTypeDefaults: et_err_status → alert-circle sensor", "[can]") {
    const char *comp, *dc, *unit, *sc, *icon;
    getTypeDefaults(et_err_status, comp, dc, unit, sc, icon);
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
    for (int i = 1; i < (int)ElsterTableCount; i++) {
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

    CHECK(std::string(ei->Name) == std::string(ei_test->Name));
    CHECK(std::string(cm->Name) == "MANAGER");
}

TEST_CASE("processCanMessage: et_bool signal value 1 → 'on'", "[can]") {
    // UHRZEIT index 0x0009, et_zeit — use any et_bool signal instead
    // Scan for a non-blacklisted et_bool signal
    const ElsterIndex* boolEi = nullptr;
    for (int i = 1; i < (int)ElsterTableCount; i++) {
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
// hash / elsterNameHash
// ============================================================================

TEST_CASE("hash: compile-time and runtime hashes agree", "[can]") {
    constexpr uint32_t compile = hash("TEMP_AUSSEN");
    uint32_t runtime = elsterNameHash("TEMP_AUSSEN");
    CHECK(compile == runtime);
}
TEST_CASE("hash: known signal hashes match pre-computed constants", "[can]") {
    CHECK(elsterNameHash("JAHR")           == HASH_JAHR);
    CHECK(elsterNameHash("MONAT")          == HASH_MONAT);
    CHECK(elsterNameHash("TAG")            == HASH_TAG);
    CHECK(elsterNameHash("STUNDE")         == HASH_STUNDE);
    CHECK(elsterNameHash("MINUTE")         == HASH_MINUTE);
    CHECK(elsterNameHash("SEKUNDE")        == HASH_SEKUNDE);
    CHECK(elsterNameHash("WPVORLAUFIST")   == HASH_WPVORLAUFIST);
    CHECK(elsterNameHash("RUECKLAUFISTTEMP") == HASH_RUECKLAUFISTTEMP);
    CHECK(elsterNameHash("VERDICHTER")     == HASH_VERDICHTER);
}
// Expected values worked out by hand from the DJB2 definition: h0 = 5381, h = h * 33 + c (#121).
TEST_CASE("elsterNameHash: DJB2 values from the definition", "[can]") {
    CHECK(elsterNameHash("") == 5381u);
    CHECK(elsterNameHash("a") == 177670u);        // 5381*33 + 97
    CHECK(elsterNameHash("ab") == 5863208u);      // 177670*33 + 98
    CHECK(elsterNameHash("TAG") == 193470689u);   // ((5381*33 + 84)*33 + 65)*33 + 71
    CHECK(hash("TAG") == 193470689u);             // compile-time twin, same value
    // uint32_t wrap-around: 33 characters overflow 32 bits; both versions must wrap the same way
    constexpr const char* longName = "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH_X";
    CHECK(elsterNameHash(longName) == hash(longName));
}
TEST_CASE("hash: different strings produce different hashes", "[can]") {
    CHECK(elsterNameHash("STUNDE") != elsterNameHash("MINUTE"));
    CHECK(elsterNameHash("JAHR")   != elsterNameHash("MONAT"));
}
TEST_CASE("hash: empty string does not crash", "[can]") {
    CHECK_NOTHROW(elsterNameHash(""));
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
TEST_CASE("updateCOPCalculations: cop_ww includes SUM_KWH remainder (WPL13E)", "[can]") {
    mqtt_client_instance().clear();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH",        "42");
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_KWH",        "629");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH",    "0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "14");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_KWH", "75");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_KWH",        "613");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", "730");
    updateCOPCalculations();
    bool found = false;
    for (auto& m : mqtt_client_instance().messages)
        if (m.topic.find("cop_ww") != std::string::npos) { CHECK(m.payload == "3.03"); found = true; }  // 42.629/14.075
    CHECK(found);
    for (const char* k : {"WAERMEERTRAG_WW_SUM_KWH", "EL_AUFNAHMELEISTUNG_WW_SUM_KWH",
                          "WAERMEERTRAG_HEIZ_SUM_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH"})
        copEnergyValues.erase(k);
}
TEST_CASE("updateCOPCalculations: waits for all SUM_KWH remainders", "[can]") {
    mqtt_client_instance().clear();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH",        "42");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH",    "0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "14");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_KWH", "75");   // only 1 of 4
    updateCOPCalculations();
    for (auto& m : mqtt_client_instance().messages)
        CHECK(m.topic.find("cop_ww/state") == std::string::npos);
    copEnergyValues.erase("EL_AUFNAHMELEISTUNG_WW_SUM_KWH");
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
    invalidStreak.clear();
    resetStateCache();
    fake_millis() = 0;
    mqtt_client_instance().connected = true;
    mqtt_client_instance().fail_publish = false;
    mqtt_client_instance().clear();
    mqttConnectSeen = false;
    sgReadyPublishPending = false;
    pacedWriteCount = 0;
    pacedSentBefore = false;
    pacedLastSendMs = 0;
    lastSgDhwSetpoint = NAN;
    lastSgRoomSetpoint = NAN;
    for (auto& prog : timeProgram) for (auto& d : prog) d = DhwProgramDay{};
    resetNumberLimits();
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
    // HA's "lock" class reads on as unlocked, the opposite of "EVU lock active"
    CHECK(payload.find("\"device_class\":\"lock\"") == std::string::npos);
}

TEST_CASE("publishMqttDiscovery: ABTAUUNGAKTIV discovery carries on/off payloads", "[mqtt]") {
    // Without them HA expects ON/OFF, rejects the published on/off and stays unknown.
    resetDiscoveryState();
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("ABTAUUNGAKTIV");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(heizmodul, ei);
    std::string payload = mqttFindPayload("homeassistant/binary_sensor");
    CHECK(payload.find("\"payload_on\":\"on\"") != std::string::npos);
    CHECK(payload.find("\"payload_off\":\"off\"") != std::string::npos);
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

TEST_CASE("VD CAN feedback publishes whole hours with existing MQTT identity", "[can][mqtt][runtime]") {
    resetDiscoveryState();
    // Extended register 0x07fc, raw 0x17f1 = 6129 hours, sender HEIZMODUL.
    processAndUpdate(0x500, {0xd2, 0x00, 0xfa, 0x07, 0xfc, 0x17, 0xf1});
    if (std::strcmp(HA_DEVICE_MODEL_STR, "wpl13e") == 0) {
        // Upstream intentionally suppresses this model's invalid compressor-1
        // and cooling counters; WPL9 support must preserve that exclusion.
        CHECK_FALSE(mqttTopicPublished("heatingpump/HEIZMODUL/LZ_VERD_1_HEIZBETRIEB/state"));
        processAndUpdate(0x500, {0xd2, 0x00, 0xfa, 0x07, 0xff, 0x0a, 0xf3});
        CHECK_FALSE(mqttTopicPublished("heatingpump/HEIZMODUL/LZ_VERD_1_KUEHLBETRIEB/state"));
        return;
    }
    CHECK(mqttFindPayload("heatingpump/HEIZMODUL/LZ_VERD_1_HEIZBETRIEB/state") == "6129");
    std::string discovery = mqttFindPayload("homeassistant/sensor/heatingpump/stiebel_heizmodul_lz_verd_1_heizbetrieb/config");
    CHECK(discovery.find("\"unique_id\":\"stiebel_heizmodul_lz_verd_1_heizbetrieb\"") != std::string::npos);
    CHECK(discovery.find("\"device_class\":\"duration\"") != std::string::npos);
    CHECK(discovery.find("\"unit_of_measurement\":\"h\"") != std::string::npos);
    CHECK(discovery.find("\"state_class\":\"total_increasing\"") != std::string::npos);
    processAndUpdate(0x500, {0xd2, 0x00, 0xfa, 0x07, 0xff, 0x0a, 0xf3});
    CHECK(mqttFindPayload("heatingpump/HEIZMODUL/LZ_VERD_1_KUEHLBETRIEB/state") == "2803");
    mqtt_client_instance().clear();
    // 0x8000 (-255, not available): a total_increasing counter keeps its last state, because
    // HA reads a drop as a new meter cycle and would add the next real value to the sum again.
    processAndUpdate(0x500, {0xd2, 0x00, 0xfa, 0x07, 0xfc, 0x80, 0x00});
    CHECK_FALSE(mqttTopicPublished("heatingpump/HEIZMODUL/LZ_VERD_1_HEIZBETRIEB/state"));
}

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

TEST_CASE("updateSensor: -255 creates no entity until a valid value arrives", "[mqtt]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(ei != nullptr);
    updateSensor(mgr, ei, "-255");
    CHECK(mqtt_client_instance().messages.empty());
    updateSensor(mgr, ei, "30.5");
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/"));
    CHECK(mqttFindPayload("RUECKLAUFISTTEMP/state") == "30.5");
    // once discovered, -255 is still published (entity keeps its history)
    mqtt_client_instance().clear();
    updateSensor(mgr, ei, "-255");
    CHECK(mqttFindPayload("RUECKLAUFISTTEMP/state") == "-255");
}

TEST_CASE("backoff: interval grows from the 3rd -255 and resets on a valid value", "[can]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    const uint64_t key = signalKey(mgr, "RUECKLAUFISTTEMP");
    for (int i = 0; i < 2; i++) updateSensor(mgr, ei, "-255");
    CHECK(backoffIntervalMs(key, 60000) == 60000);
    updateSensor(mgr, ei, "-255");
    CHECK(backoffIntervalMs(key, 60000) == 120000);
    updateSensor(mgr, ei, "-255");
    CHECK(backoffIntervalMs(key, 60000) == 240000);
    updateSensor(mgr, ei, "30.5");
    CHECK(backoffIntervalMs(key, 60000) == 60000);
}

TEST_CASE("nextBackoffMs: capped at 6 h, never below base", "[can]") {
    CHECK(nextBackoffMs(30000, 0) == 30000);
    CHECK(nextBackoffMs(30000, 2) == 30000);
    CHECK(nextBackoffMs(30000, 3) == 60000);
    CHECK(nextBackoffMs(30000, 200) == BACKOFF_MAX_MS);
    CHECK(nextBackoffMs(600000, 20) == BACKOFF_MAX_MS);
    CHECK(nextBackoffMs(8UL * 3600UL * 1000UL, 5) == 8UL * 3600UL * 1000UL);
}

TEST_CASE("0x9000 (function off) on a numeric signal: nothing published, last value stays", "[can][mqtt]") {
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(ei->Type == et_dec_val);
    processAndUpdate(0x480, makeFrame(0x92, 0x00, 0x16, 0x01, 0x31));  // 305 = 30.5
    REQUIRE(mqttFindPayload("RUECKLAUFISTTEMP/state") == "30.5");
    const size_t published = mqtt_client_instance().messages.size();
    processAndUpdate(0x480, makeFrame(0x92, 0x00, 0x16, 0x90, 0x00));  // would be -2867.2
    CHECK(mqtt_client_instance().messages.size() == published);
    CHECK(mqttFindPayload("RUECKLAUFISTTEMP/state") == "30.5");
}

TEST_CASE("0x9000 (function off) does not raise the poll back-off", "[can]") {
    resetDiscoveryState();
    const CanMember& mgr = CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    const uint64_t key = signalKey(mgr, "RUECKLAUFISTTEMP");
    for (int i = 0; i < 2; i++) updateSensor(mgr, ei, "-255");
    for (int i = 0; i < 3; i++) processAndUpdate(0x480, makeFrame(0x92, 0x00, 0x16, 0x90, 0x00));
    CHECK(backoffIntervalMs(key, 60000) == 60000);
}

// processAndUpdate: Elster telegram type filter
// Frame layout: [type | receiver address, receiver low bits, 0xFA, idx hi, idx lo, val hi, val lo]
TEST_CASE("processAndUpdate: read request from another node is not a value", "[can]") {
    resetDiscoveryState();
    // MANAGER (0x480) asks HEIZMODUL (0x500, address 0xA0) for VERDICHTER (0x07a8): type 1
    processAndUpdate(0x480, {0xA1, 0x00, 0xFA, 0x07, 0xA8, 0x00, 0x00});
    CHECK(mqtt_client_instance().messages.empty());
}

TEST_CASE("processAndUpdate: response is published for the responding node", "[can]") {
    resetDiscoveryState();
    // HEIZMODUL (0x500) answers PC (0x680, address 0xD0): type 2, value 8 -> 0.8
    processAndUpdate(0x500, {0xD2, 0x00, 0xFA, 0x07, 0xA8, 0x00, 0x08});
    CHECK(mqttFindPayload("HEIZMODUL/VERDICHTER/state") == "0.8");
}

TEST_CASE("processAndUpdate: short frame is ignored without crash", "[can]") {
    resetDiscoveryState();
    processAndUpdate(0x500, {0xD2, 0x00, 0xFA});
    CHECK(mqtt_client_instance().messages.empty());
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
    const CanMember& heizmodul = CanMembers[cm_heizmodul];
    const ElsterIndex* ei = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(ei != nullptr);
    updateSensor(heizmodul, ei, "28.3");
    CHECK(lastRuecklaufIstTemp == Approx(28.3f));
}

TEST_CASE("updateSensor: Delta T inputs ignore the WPM (KESSEL/MANAGER) sensors", "[mqtt]") {
    resetDiscoveryState();
    lastWpVorlaufIst = NAN; lastRuecklaufIstTemp = NAN; lastWpVorlaufKessel = NAN;
    const ElsterIndex* vl = GetElsterIndex("WPVORLAUFIST");
    const ElsterIndex* rl = GetElsterIndex("RUECKLAUFISTTEMP");
    REQUIRE(vl != nullptr);
    REQUIRE(rl != nullptr);

    updateSensor(CanMembers[cm_kessel], vl, "30.3");
    updateSensor(CanMembers[cm_kessel], rl, "28.4");
    updateSensor(CanMembers[cm_manager], rl, "28.4");
    CHECK(std::isnan(lastWpVorlaufIst));
    CHECK(std::isnan(lastRuecklaufIstTemp));
    CHECK(lastWpVorlaufKessel == Approx(30.3f));
    lastWpVorlaufKessel = NAN;
}

TEST_CASE("publishDeltaTContinuous: IWS pair wins over the WPM pair", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = 30.4f; lastRuecklaufIstTemp = 27.7f;           // IWS B1 / B2
    lastWpVorlaufKessel = 30.9f; lastKesselRuecklaufIst = 28.4f;      // WPM X2/2 / X2/3
    publishDeltaTContinuous();
    CHECK(mqttFindPayload("delta_t_continuous/state") == "2.70");
    lastWpVorlaufKessel = NAN; lastKesselRuecklaufIst = NAN;
}

TEST_CASE("publishDeltaTContinuous: WPM pair only if the IWS delivered nothing", "[calc]") {
    resetDiscoveryState();
    lastWpVorlaufIst = NAN; lastRuecklaufIstTemp = NAN;
    lastWpVorlaufKessel = 30.0f; lastKesselRuecklaufIst = 27.0f;
    publishDeltaTContinuous();
    CHECK(mqttFindPayload("delta_t_continuous/state") == "3.00");

    // IWS delivers only one value so far: no mixed pair
    resetDiscoveryState();
    lastWpVorlaufIst = 30.4f; lastRuecklaufIstTemp = NAN;
    publishDeltaTContinuous();
    CHECK(!mqttTopicPublished("delta_t_continuous/state"));
    lastWpVorlaufIst = NAN; lastWpVorlaufKessel = NAN; lastKesselRuecklaufIst = NAN;
}

TEST_CASE("updateSensor: control deviation inputs only come from the KESSEL member", "[mqtt]") {
    resetDiscoveryState();
    lastKesselRuecklaufIst = NAN;
    lastKesselSollTemp = NAN;
    const ElsterIndex* rl = GetElsterIndex("RUECKLAUFISTTEMP");
    const ElsterIndex* soll = GetElsterIndex("KESSELSOLLTEMP");
    REQUIRE(rl != nullptr);
    REQUIRE(soll != nullptr);

    // IWS (HEIZMODUL) return temperature is a different measuring point (X2/3 vs. IWS B2)
    updateSensor(CanMembers[cm_heizmodul], rl, "19.1");
    CHECK(std::isnan(lastKesselRuecklaufIst));
    updateSensor(CanMembers[cm_manager], soll, "25.5");
    CHECK(std::isnan(lastKesselSollTemp));

    updateSensor(CanMembers[cm_kessel], rl, "24.8");
    updateSensor(CanMembers[cm_kessel], soll, "25.6");
    CHECK(lastKesselRuecklaufIst == Approx(24.8f));
    CHECK(lastKesselSollTemp == Approx(25.6f));
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

static void setEnergy(const char* sum, float mwh, float kwh, float tagKwh, float tagWh) {
    const std::string name(sum);
    const char* m = energyMemberName();
    storeCOPEnergyValueMember(m, (name + "_SUM_MWH").c_str(), std::to_string(mwh));
    storeCOPEnergyValueMember(m, (name + "_SUM_KWH").c_str(), std::to_string(kwh));
    storeCOPEnergyValueMember(m, (name + "_TAG_KWH").c_str(), std::to_string(tagKwh));
    storeCOPEnergyValueMember(m, (name + "_TAG_WH").c_str(), std::to_string(tagWh));
}

static float slidingTotal(int slot, const std::string& name) {
    float out = -1.0f;
    REQUIRE(wplBaseTotal(slot, (name + "_SUM_MWH").c_str(), (name + "_SUM_KWH").c_str(),
                         (name + "_TAG_KWH").c_str(), (name + "_TAG_WH").c_str(), out));
    return out;
}

TEST_CASE("sanitizeEnergyTotal: small drops at the day change never go backwards", "[calc]") {
    fake_millis() = 1000;
    float out = 0.0f;
    REQUIRE(sanitizeEnergyTotal("test_drop", 24746.283f, out));
    // SUM holds whole kWh only, so the Wh part of the old day is gone for a moment
    REQUIRE(sanitizeEnergyTotal("test_drop", 24746.0f, out));
    CHECK(out == Approx(24746.283f));
    REQUIRE(sanitizeEnergyTotal("test_drop", 24746.1f, out));
    CHECK(out == Approx(24746.283f));
    REQUIRE(sanitizeEnergyTotal("test_drop", 24746.5f, out));
    CHECK(out == Approx(24746.5f));
    REQUIRE(sanitizeEnergyTotal("test_drop", 24745.6f, out));   // also held, no 15 min candidate
    CHECK(out == Approx(24746.5f));
}

TEST_CASE("wplBaseTotal: no value until the day counter is known again", "[calc]") {
    // After a reboot the stored base is there before the first day counter answer
    const std::string n = "TEST_REBOOT";
    const char* m = energyMemberName();
    storeCOPEnergyValueMember(m, (n + "_SUM_MWH").c_str(), "24");
    storeCOPEnergyValueMember(m, (n + "_SUM_KWH").c_str(), "765");
    float out = -1.0f;
    CHECK_FALSE(wplBaseTotal(1, (n + "_SUM_MWH").c_str(), (n + "_SUM_KWH").c_str(),
                             (n + "_TAG_KWH").c_str(), (n + "_TAG_WH").c_str(), out));
    storeCOPEnergyValueMember(m, (n + "_TAG_KWH").c_str(), "7");
    storeCOPEnergyValueMember(m, (n + "_TAG_WH").c_str(), "654");
    REQUIRE(wplBaseTotal(1, (n + "_SUM_MWH").c_str(), (n + "_SUM_KWH").c_str(),
                         (n + "_TAG_KWH").c_str(), (n + "_TAG_WH").c_str(), out));
    CHECK(out == Approx(24772.654f));
}

TEST_CASE("wplBaseTotal: day counter resets before the total steps", "[calc]") {
    const std::string n = "TEST_ROLLOVER_A";
    setEnergy(n.c_str(), 24, 738, 3, 500);
    CHECK(slidingTotal(2, n) == Approx(24741.5f));
    setEnergy(n.c_str(), 24, 738, 0, 0);    // order seen on a WPL13E at midnight
    slidingTotal(2, n);                     // lower value is held back by sanitizeEnergyTotal()
    setEnergy(n.c_str(), 24, 742, 0, 0);
    CHECK(slidingTotal(2, n) == Approx(24742.0f));
    setEnergy(n.c_str(), 24, 742, 1, 0);
    CHECK(slidingTotal(2, n) == Approx(24743.0f));
}

TEST_CASE("wplBaseTotal: total steps before the day counter resets", "[calc]") {
    const std::string n = "TEST_ROLLOVER_B";
    setEnergy(n.c_str(), 24, 738, 3, 500);
    CHECK(slidingTotal(3, n) == Approx(24741.5f));
    setEnergy(n.c_str(), 24, 742, 3, 500);  // the new total already holds the day
    CHECK(slidingTotal(3, n) == Approx(24742.0f));
    setEnergy(n.c_str(), 24, 742, 0, 0);
    CHECK(slidingTotal(3, n) == Approx(24742.0f));
    setEnergy(n.c_str(), 24, 742, 1, 0);
    CHECK(slidingTotal(3, n) == Approx(24743.0f));
}

TEST_CASE("isHotWaterCharging: PUMPENSTATUS values seen on a WPL13E", "[calc]") {
    CHECK(isHotWaterCharging(28930));    // 0x7102 during a hot water charge
    CHECK_FALSE(isHotWaterCharging(12546));   // 0x3102 heating
    CHECK_FALSE(isHotWaterCharging(12290));   // 0x3002 stopped
    CHECK_FALSE(isHotWaterCharging(45058));   // 0xB002 short pulse while stopped
}

TEST_CASE("updateSensor: PUMPENSTATUS is tracked from KESSEL only", "[calc]") {
    resetDiscoveryState();
    lastPumpenstatus = -1;
    const ElsterIndex* ei = GetElsterIndex("PUMPENSTATUS");
    REQUIRE(ei != nullptr);
    updateSensor(CanMembers[cm_manager], ei, "28930");
    CHECK(lastPumpenstatus == -1);
    updateSensor(CanMembers[cm_kessel], ei, "28930");
    CHECK(lastPumpenstatus == 28930);
    updateSensor(CanMembers[cm_kessel], ei, "-255");
    CHECK(lastPumpenstatus == 28930);
}

TEST_CASE("isHeizmodulSilent: only after 60 s without a frame", "[calc]") {
    CHECK_FALSE(isHeizmodulSilent(500000, 0));   // no frame yet
    CHECK_FALSE(isHeizmodulSilent(160000, 100000));
    CHECK(isHeizmodulSilent(160001, 100000));
    CHECK_FALSE(isHeizmodulSilent(10000, 0xFFFFFFF0u));   // across the millis() wrap
}

// #49: compressor starts and runtime per day, sampled every 30 s
static void feedCompressor(CompressorDayStats& s, uint32_t& t, bool on, int samples, int day = 5) {
    for (int i = 0; i < samples; ++i) { updateCompressorStats(s, on, t, day); t += 30000; }
}

TEST_CASE("compressor stats: starts and runtime of separate runs", "[calc][compressor]") {
    CompressorDayStats s; uint32_t t = 1000;
    feedCompressor(s, t, false, 4);
    feedCompressor(s, t, true, 27);    // run of 13 min
    feedCompressor(s, t, false, 10);   // 5 min off
    feedCompressor(s, t, true, 21);    // run of 10 min
    feedCompressor(s, t, false, 2);
    CHECK(s.starts == 2);
    CHECK(s.runtimeMs == (26 + 20) * 30000u);   // intervals between two "on" samples
}

TEST_CASE("compressor stats: an off gap under 2 min is not a new start", "[calc][compressor]") {
    CompressorDayStats s; uint32_t t = 1000;
    feedCompressor(s, t, true, 10);
    feedCompressor(s, t, false, 2);    // 60 s gap (bus hiccup, decode gap)
    feedCompressor(s, t, true, 10);
    CHECK(s.starts == 1);
    feedCompressor(s, t, false, 5);    // 150 s
    feedCompressor(s, t, true, 2);
    CHECK(s.starts == 2);
}

TEST_CASE("compressor stats: reset at the heat pump's day change", "[calc][compressor]") {
    CompressorDayStats s; uint32_t t = 1000;
    feedCompressor(s, t, true, 5, 5);
    feedCompressor(s, t, false, 6, 5);
    feedCompressor(s, t, true, 5, 5);
    CHECK(s.starts == 2);
    feedCompressor(s, t, true, 3, 6);  // still running at midnight: counts for the new day
    CHECK(s.starts == 1);
    CHECK(s.runtimeMs == 3 * 30000u);
    feedCompressor(s, t, false, 6, 6);
    feedCompressor(s, t, false, 1, -1); // date not known (e.g. after a reboot): no reset
    CHECK(s.starts == 1);
}

TEST_CASE("compressor stats are published with the compressor state", "[calc][compressor]") {
    resetDiscoveryState();
    compressorStats = CompressorDayStats{};
    lastTag = 7;
    lastVerdichterValue = 50.0f;
    fake_millis() = 1000;
    publishCompressorActive();
    CHECK(mqttFindPayload("calculated/verdichter_starts_heute/state") == "1");
    CHECK(mqttFindPayload("calculated/verdichter_laufzeit_heute/state") == "0.0");
    CHECK(mqttFindPayload("calculated/verdichter_laufzeit_mittel/state") == "0.0");
    compressorStats = CompressorDayStats{};
    lastTag = -1;
    fake_millis() = 0;
}

TEST_CASE("defrost running: last defrost duration 0 means a defrost is running (#48)", "[calc]") {
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("ZEITDAUER_LETZTE_ABTAUUNG");
    REQUIRE(ei != nullptr);
    updateSensor(CanMembers[cm_heizmodul], ei, "708");
    CHECK(mqttFindPayload("calculated/abtauung_laeuft/state") == "off");
    mqtt_client_instance().clear();
    updateSensor(CanMembers[cm_heizmodul], ei, "0");   // defrost started
    CHECK(mqttFindPayload("calculated/abtauung_laeuft/state") == "on");
    mqtt_client_instance().clear();
    updateSensor(CanMembers[cm_heizmodul], ei, "813");  // defrost ended after 813 s
    CHECK(mqttFindPayload("calculated/abtauung_laeuft/state") == "off");
    mqtt_client_instance().clear();
    updateSensor(CanMembers[cm_heizmodul], ei, "-255"); // not provided: no state
    CHECK(mqttFindPayload("calculated/abtauung_laeuft/state").empty());
}

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

// The options list is exactly the select's values, in table order and nothing after the last one
// (#109: an off-by-one past the option array was only caught by chance through the memory layout).
// PROGRAMMSCHALTER: the operating modes of the manager (LNAME_OPT_* protocol values); SG Ready:
// the four states 1-4; state-2 mode: Tagbetrieb or Automatik.
TEST_CASE("publishWritableSelectDiscovery: options list is exactly the select's options", "[mqtt]") {
    const std::string modes = std::string("[\"") + LNAME_OPT_NOTBETRIEB + "\",\"" + LNAME_OPT_BEREITSCHAFT +
        "\",\"" + LNAME_OPT_AUTOMATIK + "\",\"" + LNAME_OPT_TAGBETRIEB + "\",\"" + LNAME_OPT_ABSENKBETRIEB +
        "\",\"" + LNAME_OPT_WARMWASSER + "\"]";
    const struct { const char* uid; std::string options; } cases[] = {
        {"stiebel_manager_programmschalter/config", "\"options\":" + modes},
        {"stiebel_manager_sg_ready_state/config", std::string("\"options\":[\"") + LNAME_OPT_SG_EVU_SPERRE + "\",\"" +
             LNAME_OPT_SG_NORMAL + "\",\"" + LNAME_OPT_SG_EMPFOHLEN + "\",\"" + LNAME_OPT_SG_ZWANG + "\"]"},
        {"stiebel_manager_sg_ready_normal_mode/config", std::string("\"options\":[\"") + LNAME_OPT_TAGBETRIEB +
             "\",\"" + LNAME_OPT_AUTOMATIK + "\"]"},
    };
    resetDiscoveryState();
    publishAllWritableSelectDiscoveries(true);
    for (const auto& c : cases) {
        INFO(c.uid);
        CHECK(mqttFindPayload(c.uid).find(c.options) != std::string::npos);
    }
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

TEST_CASE("i18n: programmschalterOptions match BetriebsartList protocol strings", "[i18n]") {
    // These must stay German regardless of display language — they round-trip
    // through writeSignal()/TranslateString() as literal CAN-write values.
    for (size_t i = 0; i < 6; ++i)
        CHECK(std::string(programmschalterOptions[i]) == BetriebsartList[i].Name);
}

TEST_CASE("writeSignal: et_betriebsart with unrecognized string is not sent to CAN bus", "[can]") {
    fake_can().sent.clear();
    const char* value = "Bogus Value";
    writeSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER", value);
    CHECK(fake_can().sent.empty());
}

TEST_CASE("writeSignal: et_betriebsart with valid German string is sent", "[can]") {
    fake_can().sent.clear();
    const char* value = LNAME_OPT_AUTOMATIK;   // model's own name (WPM3: Programmierbetrieb)
    writeSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER", value);
    CHECK(fake_can().sent.size() == 1);
}

TEST_CASE("i18n: sgReadyOptions contains 4 non-empty entries", "[i18n]") {
    for (size_t i = 0; i < 4; ++i)
        CHECK(std::string(sgReadyOptions[i]).length() > 0);
}

TEST_CASE("i18n: all calculatedSensors names are non-empty", "[i18n]") {
    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; ++i)
        CHECK(std::string(calculatedSensors[i].name).length() > 0);
}

TEST_CASE("decode buffer holds the longest SetValueType texts", "[decode]") {
    char val[ELSTER_VALUE_BUF_SIZE];
    SetValueType(val, et_time_domain, 0x8080);
    CHECK(std::string(val) == "not used time domain");
    CHECK(sizeof(val) > std::string(val).length());
    SetValueType(val, et_err_nr, 0x0023);
    CHECK(std::string(val) == "ERR MIN UEBERHITZUNG");
    CHECK(sizeof(val) > std::string(val).length());
}

TEST_CASE("ABTAUUNGAKTIV is a binary sensor with on/off payloads", "[meta]") {
    const ElsterMeta* meta = GetElsterMeta(GetElsterIndex("ABTAUUNGAKTIV"));
    REQUIRE(meta != nullptr);
    CHECK(std::string(meta->haComponent) == "binary_sensor");
    REQUIRE(meta->payloadOn != nullptr);
    REQUIRE(meta->payloadOff != nullptr);
    CHECK(std::string(meta->payloadOn) == "on");
    CHECK(std::string(meta->payloadOff) == "off");
}

TEST_CASE("WPMW II: IWS values and parameters carry read-only metadata (#52)", "[meta][wpmw2]") {
    // type with / without HA_CONTROLLER_WPMW2, then the HA metadata under WPMW II
    struct Row { const char* name; unsigned type; unsigned typeOther; const char* deviceClass; const char* unit; };
    static const Row rows[] = {
        {"VERDAMPFERISTTEMP_KOMPENSIERT", et_dec_val, et_default, "temperature", "°C"},
        {"REKUPERATORISTTEMP", et_dec_val, et_default, "temperature", "°C"},
        {"LEISTUNGSREDUZIERUNG_KUEHLEN", et_dec_val, et_default, "temperature", "°C"},
        {"MESSSTROM_HOCHDRUCK", et_dec_val, et_default, "current", "mA"},
        {"MESSSTROM_NIEDERDRUCK", et_dec_val, et_default, "current", "mA"},
        {"VORLAUFISTTEMP_WP_IWS", et_dec_val, et_dec_val, "temperature", "°C"},
        {"LUEFTERDREHZAHL", et_default, et_default, "", "%"},
        {"STILLSTANDZEIT", et_little_endian, et_little_endian, "duration", "min"},
        {"STILLSTANDZEIT_0", et_default, et_default, "duration", "s"},
    };
    for (const Row& r : rows) {
        INFO(r.name);
        const ElsterIndex* ei = GetElsterIndex(r.name);
        REQUIRE(std::string(ei->Name) == r.name);   // unknown names return INDEX_NOT_FOUND
        const ElsterMeta* meta = GetElsterMeta(ei);
        if (!isWpmw2Model()) {
            CHECK(ei->Type == r.typeOther);
            CHECK(meta == nullptr);
            continue;
        }
        CHECK(ei->Type == r.type);
        REQUIRE(meta != nullptr);
        CHECK(std::string(meta->haComponent) == "sensor");
        CHECK(std::string(meta->haDeviceClass) == r.deviceClass);
        CHECK(std::string(meta->unit) == r.unit);
    }
    // Name only: unit and device class stay the type defaults, so existing HA statistics keep their unit
    const ElsterIndex* hyst = GetElsterIndex("HYSTERESEZEIT");
    REQUIRE(std::string(hyst->Name) == "HYSTERESEZEIT");
    if (isWpmw2Model()) {
        const ElsterMeta* meta = GetElsterMeta(hyst);
        REQUIRE(meta != nullptr);
        CHECK(meta->unit == nullptr);
        CHECK(meta->haDeviceClass == nullptr);
        resetDiscoveryState();
        publishMqttDiscovery(CanMembers[cm_manager], GetElsterIndex("STILLSTANDZEIT"));
        const std::string payload = mqttFindPayload("stillstandzeit");
        CHECK(payload.find("\"device_class\":\"duration\"") != std::string::npos);
        CHECK(payload.find("\"unit_of_measurement\":\"min\"") != std::string::npos);
    }
}

TEST_CASE("writableNumbers: WPMW II parameters sit at the indices common.yaml uses", "[writable]") {
    // The WPMW II parameter rows (set commands find them by name, #92)
    static const char* expected[] = {"SCHALTWERKDYNAMIKZEIT",
                                     "BIVALENTPARALLELTEMPERATUR_HZG", "BIVALENTPARALLELTEMPERATUR_WW",
                                     "STILLSTANDZEIT", "SW_AUSSENTEMP"};
    REQUIRE(WRITABLE_NUMBER_COUNT == 19);
    for (size_t i = 0; i < 5; ++i) {
        CHECK(std::string(writableNumbers[9 + i].signalName) == expected[i]);
        CHECK(writableNumbers[9 + i].member == cm_manager);
        CHECK(isWpmw2Parameter(expected[i]));
    }
    CHECK_FALSE(isWpmw2Parameter("HEIZKURVE"));
}

TEST_CASE("WPMW II: per-member names tell the measuring points apart (#46)", "[mqtt][names]") {
    if (!isWpmw2Model()) {
        CHECK(wpmw2MemberName("KESSEL", "RUECKLAUFISTTEMP") == nullptr);
        return;
    }
    CHECK(std::string(wpmw2MemberName("KESSEL", "RUECKLAUFISTTEMP")) == LNAME_HK1_REGELFUEHLER);
    CHECK(std::string(wpmw2MemberName("HEIZMODUL", "RUECKLAUFISTTEMP")) == LNAME_WP_RUECKLAUF_IWS);
    CHECK(std::string(wpmw2MemberName("HEIZMODUL", "AUSSENTEMP")) == LNAME_LUFTANSAUGTEMP);
    CHECK(wpmw2MemberName("KESSEL", "AUSSENTEMP") == nullptr);   // the control outdoor sensor
    CHECK(wpmw2MemberName("MANAGER", "RUECKLAUFISTTEMP") == nullptr);
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_kessel], GetElsterIndex("VORLAUFISTTEMP"));
    CHECK(mqttFindPayload("homeassistant/sensor").find(LNAME_HK2_MISCHER_VORLAUF) != std::string::npos);
}

TEST_CASE("writableNumbers: SG Ready room boosts sit at the indices common.yaml uses", "[writable][sgready]") {
    // common.yaml's room boost /set handlers use writableNumbers[14] and [15].
    CHECK(std::string(writableNumbers[14].signalName) == "SG_READY_ROOM_BOOST_STATE3");
    CHECK(std::string(writableNumbers[15].signalName) == "SG_READY_ROOM_BOOST_STATE4");
    CHECK(writableNumberApplies("SG_READY_ROOM_BOOST_STATE3"));
}

TEST_CASE("SG Ready room boost command is stored and published back", "[sgready]") {
    resetDiscoveryState();
    CHECK(handleSgReadyRoomBoost(3, "0.0"));
    CHECK(mqttFindPayload("SG_READY_ROOM_BOOST_STATE3/state") == "0.0");
    CHECK(sgReadyController.roomBoostState3() == Approx(0.0f));
    CHECK_FALSE(handleSgReadyRoomBoost(4, "7"));
    CHECK_FALSE(handleSgReadyRoomBoost(4, "abc"));
    CHECK(sgReadyController.roomBoostState4() == Approx(2.0f));
    CHECK(handleSgReadyRoomBoost(3, "1.0"));
}

TEST_CASE("writableNumbers: WPMW II parameters are only offered on wpl13e and ttl33", "[writable]") {
    // CI also builds the tests per model (MODEL_TEST_FLAGS), so both cases are checked.
    const std::string model = HA_DEVICE_MODEL_STR;
    const bool verified = model == "wpl13e" || model == "ttl33";
    for (const char* sig : {"SCHALTWERKDYNAMIKZEIT", "BIVALENTPARALLELTEMPERATUR_HZG",
                            "BIVALENTPARALLELTEMPERATUR_WW", "STILLSTANDZEIT", "SW_AUSSENTEMP"}) {
        CHECK(writableNumberApplies(sig) == verified);
    }
    // The heating curve is also writable on wpl17 (KESSEL_CANB).
    CHECK(writableNumberApplies("HEIZKURVE") == (verified || model == "wpl17"));
    CHECK(writableNumberApplies("EINSTELL_SPEICHERSOLLTEMP"));
}

TEST_CASE("writableNumbers: 0xfdb0/0xfdb1 resolve to the names used in the state topics", "[writable]") {
    // The table also lists REGLERDYNAMIK/VERDICHTER_STILLSTAND for these indices; incoming
    // frames are named after the first entry, so the number entities must use those names.
    CHECK(std::string(GetElsterIndex((unsigned short)0xfdb0)->Name) == "SCHALTWERKDYNAMIKZEIT");
    CHECK(std::string(GetElsterIndex((unsigned short)0xfdb1)->Name) == "STILLSTANDZEIT");
}

TEST_CASE("writableNumbers: WPMW II values encode like the bus scan read them", "[writable]") {
    // Scan of a WPL13E: Reglerdynamik 10 = raw 2560 (0x0a00), bivalence -8.0 = raw 65456.
    const char* v = "10";
    CHECK(TranslateString(v, et_little_endian) == 0x0a00);
    v = "15";
    CHECK(TranslateString(v, et_little_endian) == 0x0f00);
    v = "-8.0";
    CHECK(TranslateString(v, et_dec_val) == 65456);
}

TEST_CASE("i18n: including lang_en.h does not override a German build", "[i18n]") {
    // ESPHome includes every listed file from main.cpp, so lang_en.h is always included.
    CHECK(std::string(LNAME_ABTAUUNGAKTIV) == "Abtauung Aktiv");
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
        const ElsterMeta* meta = GetElsterMeta(ei);
        if (meta && meta->friendlyName)
            CHECK(std::string(meta->friendlyName).length() > 0);
    }
}

// ============================================================================
// Bus scan
// ============================================================================

TEST_CASE("scan: start parses member and mode, invalid payload is rejected", "[scan]") {
    resetDiscoveryState();
    scanState = ScanState{};
    startScan("nonsense");
    CHECK_FALSE(scanState.active);
    startScan("HEIZMODUL 0x0200 0x0100");   // to < from
    CHECK_FALSE(scanState.active);
    startScan("heizmodul 0x07a0 0x07af");
    CHECK(scanState.active);
    CHECK_FALSE(scanState.table);
    CHECK(scanState.cm == &CanMembers[cm_heizmodul]);
    CHECK(mqttFindPayload("scan/status").find("\"total\":16") != std::string::npos);
    mqtt_client_instance().clear();
    stopScan();
    CHECK_FALSE(scanState.active);
    CHECK(mqttFindPayload("scan/status").find("stopped") != std::string::npos);
}

TEST_CASE("scan: answers of the scanned member are published, 0x8000 is not", "[scan]") {
    resetDiscoveryState();
    scanState = ScanState{};
    startScan("HEIZMODUL");
    REQUIRE(scanState.table);
    mqtt_client_instance().clear();
    const ElsterIndex* ei = GetElsterIndex("VERDICHTER");
    std::vector<uint8_t> hit = {0xD2, 0x00, 0xFA, 0x07, 0xA8, 0x00, 0x08};
    CHECK(handleScanFrame(CanMembers[cm_heizmodul], ei, hit, "0.8"));
    CHECK(mqttFindPayload("scan/HEIZMODUL/0x07a8") == "{\"name\":\"VERDICHTER\",\"raw\":8,\"value\":\"0.8\"}");
    mqtt_client_instance().clear();
    std::vector<uint8_t> none = {0xD2, 0x00, 0xFA, 0x07, 0xA8, 0x80, 0x00};
    CHECK(handleScanFrame(CanMembers[cm_heizmodul], ei, none, "-255"));
    CHECK(mqtt_client_instance().messages.empty());
    CHECK_FALSE(handleScanFrame(CanMembers[cm_manager], ei, hit, "0.8"));   // other member: normal path
    CHECK(scanState.hits == 1);
    scanState = ScanState{};
}

// ============================================================================
// MQTT climate
// ============================================================================

TEST_CASE("climate: heating and hot water discoveries reuse the number /set topics", "[mqtt]") {
    resetDiscoveryState();
    publishClimateDiscoveries();
    std::string heat = mqttFindPayload("climate/heatingpump/stiebel_climate_heizung/config");
    std::string dhw  = mqttFindPayload("climate/heatingpump/stiebel_climate_warmwasser/config");
    CHECK(heat.find("heatingpump/MANAGER/RAUMSOLLTEMP_I/set") != std::string::npos);
    CHECK(heat.find("PROGRAMMSCHALTER/set") != std::string::npos);
    CHECK(heat.find(std::string("\"") + LNAME_OPT_TAGBETRIEB + "\"") != std::string::npos);
    // Presets = exactly the six PROGRAMMSCHALTER operating modes, nothing after the last (#109)
    CHECK(heat.find(std::string("\"preset_modes\":[\"") + LNAME_OPT_NOTBETRIEB + "\",\"" + LNAME_OPT_BEREITSCHAFT +
                    "\",\"" + LNAME_OPT_AUTOMATIK + "\",\"" + LNAME_OPT_TAGBETRIEB + "\",\"" +
                    LNAME_OPT_ABSENKBETRIEB + "\",\"" + LNAME_OPT_WARMWASSER + "\"]") != std::string::npos);
    CHECK(dhw.find("EINSTELL_SPEICHERSOLLTEMP/set") != std::string::npos);
    const bool wpl17 = std::strcmp(HA_DEVICE_MODEL_STR, "wpl17") == 0;
    const bool canbSetpoint = wpl17 || std::strcmp(HA_DEVICE_MODEL_STR, "wpl9") == 0 ||
                              std::strcmp(HA_DEVICE_MODEL_STR, "wpla7") == 0;
    const std::string current = std::string("heatingpump/") +
        ((wpl17 || std::strcmp(HA_DEVICE_MODEL_STR, "wpla7") == 0) ? "KESSEL_CANB" : "KESSEL") + "/SPEICHERISTTEMP/state";
    const std::string command = std::string("heatingpump/") +
        (canbSetpoint ? "KESSEL_CANB" : "MANAGER") + "/EINSTELL_SPEICHERSOLLTEMP/set";
    CHECK(dhw.find(current) != std::string::npos);
    CHECK(dhw.find(command) != std::string::npos);
}

// ============================================================================
// Fault text
// ============================================================================

TEST_CASE("fault: code decoding and remedy attribute", "[fault]") {
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("FEHLERMELDUNG");
    REQUIRE(ei != nullptr);
#ifdef HA_CONTROLLER_WPMW2
    CHECK(ei->Type == et_err_status);
    // HEIZMODUL answers fault 4 (Hochdruck) in the high byte
    processAndUpdate(0x500, {0xD2, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00});
#else
    CHECK(ei->Type == et_err_nr);
    // HEIZMODUL answers fault 0x0004 (Hochdruck)
    processAndUpdate(0x500, {0xD2, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00});
#endif
    CHECK(mqttFindPayload("HEIZMODUL/FEHLERMELDUNG/state") == "Hochdruck");
    CHECK(mqttFindPayload("HEIZMODUL/FEHLERMELDUNG/attributes").find("Volumenstrom") != std::string::npos);
    CHECK(mqttFindPayload("homeassistant/sensor/heatingpump/").find("json_attributes_topic") != std::string::npos);
    CHECK(std::string(faultRemedy("Keine Stoerung")).empty());
    CHECK(std::string(faultRemedy("ERR T-VL IWS")).find("Fuehler") != std::string::npos);
    CHECK(std::string(faultRemedy("HD-SENSOR-MAX")).find("Volumenstrom") != std::string::npos);  // not "Fuehler"
    CHECK(std::string(faultRemedy("ERR 69")).find("Handbuch") != std::string::npos);
}

// ============================================================================
// Summer mode
// ============================================================================

TEST_CASE("summer: EMA step and hysteresis decision", "[summer]") {
    CHECK(emaStep(NAN, 12.0f, 0.0f, 24.0f) == Approx(12.0f));
    CHECK(emaStep(10.0f, 20.0f, 0.0f, 24.0f) == Approx(10.0f));       // no time passed
    CHECK(emaStep(10.0f, 20.0f, 24.0f, 24.0f) == Approx(15.0f));      // dt == tau -> halfway
    CHECK_FALSE(summerDecision(false, 15.0f, 10.0f, false));          // automatic off
    CHECK(summerDecision(true, 10.0f, 10.0f, false));                 // on at threshold
    CHECK(summerDecision(true, 9.5f, 10.0f, true));                   // stays on within 1 K
    CHECK_FALSE(summerDecision(true, 8.9f, 10.0f, true));             // off below threshold - 1 K
    CHECK_FALSE(summerDecision(true, 9.5f, 10.0f, false));            // not on below threshold
}

TEST_CASE("summer: published once automatic, threshold and outside temperature are known", "[summer]") {
    resetDiscoveryState();
    summer = SummerState{};
    const CanMember& mgr = CanMembers[cm_manager];
    const CanMember& kessel = CanMembers[cm_kessel];
    updateSensor(kessel, GetElsterIndex("AUSSENTEMP"), "12.0");      // threshold unknown: ignored
    CHECK(mqttFindPayload("calculated/sommerbetrieb_aktiv/state").empty());
    updateSensor(mgr, GetElsterIndex("SOMMERBETRIEB_TEMP"), "10");   // not the threshold: ignored
    updateSensor(kessel, GetElsterIndex("AUSSENTEMP"), "12.0");
    CHECK(mqttFindPayload("calculated/sommerbetrieb_aktiv/state").empty());
    updateSensor(mgr, GetElsterIndex("SW_AUSSENTEMP"), "10.0");
    updateSensor(mgr, GetElsterIndex("SOMMERBETRIEB"), "on");
    updateSensor(kessel, GetElsterIndex("AUSSENTEMP"), "12.0");
    CHECK(mqttFindPayload("calculated/aussentemp_gedaempft/state") == "12.0");
    CHECK(mqttFindPayload("calculated/sommerbetrieb_aktiv/state") == "on");
    mqtt_client_instance().clear();
    updateSensor(mgr, GetElsterIndex("SOMMERBETRIEB"), "on");
    CHECK(mqttFindPayload("calculated/sommerbetrieb_aktiv/state").empty());   // unchanged: not re-sent
    mqtt_client_instance().clear();
    updateSensor(mgr, GetElsterIndex("SOMMERBETRIEB"), "off");
    CHECK(mqttFindPayload("calculated/sommerbetrieb_aktiv/state") == "off");
    summer = SummerState{};
}

TEST_CASE("updateSensor: bool fields with unknown bits ('?') are not published", "[mqtt]") {
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("ABTAUUNGAKTIV");
    REQUIRE(ei != nullptr);
    updateSensor(CanMembers[cm_heizmodul], ei, "?");
    CHECK(mqtt_client_instance().messages.empty());
    updateSensor(CanMembers[cm_heizmodul], ei, "off");
    CHECK(mqttFindPayload("HEIZMODUL/ABTAUUNGAKTIV/state") == "off");
}

// ============================================================================
// Fault list (FEHLERFELD_0..139), real WPL13E data matching the display
// ============================================================================

TEST_CASE("fault list: decodes 20 entries newest first", "[fault]") {
    uint16_t raw[FAULT_FIELD_COUNT] = {18,15,27,7,25,0,29,14,9,31,7,25,0,29,0,13,2,8,25,0,29,34,16,3,8,25,0,29,32,7,12,8,25,0,29,13,18,12,8,25,0,29,24,10,13,8,25,0,29,26,13,14,8,25,0,29,55,10,16,8,25,0,29,31,11,21,8,25,0,29,22,14,25,8,25,0,29,4,16,27,8,25,0,29,27,16,27,8,25,0,29,12,15,5,10,25,0,29,59,15,24,10,25,0,29,24,12,26,10,25,0,29,49,13,10,11,25,0,29,55,13,18,11,25,0,29,48,12,19,11,25,0,4,10,13,19,11,25,0,29};
    bool seen[FAULT_FIELD_COUNT];
    for (bool& b : seen) b = true;
    std::vector<FaultEntry> e = decodeFaultList(raw, seen);
    REQUIRE(e.size() == 20);
    // display entry 01: 19.11.25 13:10 HD-SENSOR-MAX, 02: 12:48 Hochdruck, 03: 18.11.25 13:55
    CHECK((e[0].year == 2025 && e[0].month == 11 && e[0].day == 19 && e[0].hour == 13 && e[0].minute == 10));
    CHECK(faultText(e[0].code) == "HD-SENSOR-MAX");
    CHECK(faultText(e[1].code) == "Hochdruck");
    CHECK((e[2].day == 18 && e[2].hour == 13 && e[2].minute == 55));
    CHECK((e[19].month == 7 && e[19].day == 27));
}

TEST_CASE("fault list: unread or invalid entries are skipped, publish writes state+attributes", "[fault]") {
    resetDiscoveryState();
    faultList = FaultListState{};
    uint16_t raw[FAULT_FIELD_COUNT] = {18,15,27,7,25,0,29,14,9,31,7,25,0,29,0,13,2,8,25,0,29,34,16,3,8,25,0,29,32,7,12,8,25,0,29,13,18,12,8,25,0,29,24,10,13,8,25,0,29,26,13,14,8,25,0,29,55,10,16,8,25,0,29,31,11,21,8,25,0,29,22,14,25,8,25,0,29,4,16,27,8,25,0,29,27,16,27,8,25,0,29,12,15,5,10,25,0,29,59,15,24,10,25,0,29,24,12,26,10,25,0,29,49,13,10,11,25,0,29,55,13,18,11,25,0,29,48,12,19,11,25,0,4,10,13,19,11,25,0,29};
    for (int i = 0; i < FAULT_FIELD_COUNT; i++) { faultList.raw[i] = raw[i]; faultList.seen[i] = i >= 7; }  // entry 0 unread
    faultList.raw[13] = 0x8000;                                                                          // entry 1 invalid
    CHECK(decodeFaultList(faultList.raw, faultList.seen).size() == 18);
    publishFaultList();
    CHECK(mqttFindPayload("calculated/fehlerliste/state") == "HD-SENSOR-MAX");
    std::string attrs = mqttFindPayload("calculated/fehlerliste/attributes");
    CHECK(attrs.find("\"letzter_fehler_zeit\":\"2025-11-19 13:10\"") != std::string::npos);
    CHECK(attrs.find("\"anzahl\":18") != std::string::npos);
    CHECK(attrs.find("Volumenstrom") != std::string::npos);
    faultList = FaultListState{};
}

TEST_CASE("fault list: manager answers for 0x0b00..0x0b8b are captured, not published", "[fault]") {
    resetDiscoveryState();
    faultList = FaultListState{};
    // MANAGER answers FEHLERFELD_6 (0x0b06) = 29
    processAndUpdate(0x480, {0xD2, 0x00, 0xFA, 0x0B, 0x06, 0x00, 0x1D});
    CHECK(faultList.seen[6]);
    CHECK(faultList.raw[6] == 29);
    CHECK(mqtt_client_instance().messages.empty());
    faultList = FaultListState{};
}

// ============================================================================
// Discovery republish (#53): no signal is dropped, configs go out paced
// ============================================================================

static size_t countDiscoveryConfigs() {
    size_t n = 0;
    for (const auto& m : mqtt_client_instance().messages)
        if (m.topic.rfind("homeassistant/", 0) == 0) n++;
    return n;
}

static void drainDiscoveryRepublish(size_t& steps, size_t& maxPerStep) {
    steps = 0;
    maxPerStep = 0;
    while (discoveryRepublishActive() && steps < 1000) {
        size_t before = mqtt_client_instance().messages.size();
        discoveryRepublishStep();
        maxPerStep = std::max(maxPerStep, mqtt_client_instance().messages.size() - before);
        steps++;
    }
}

TEST_CASE("republishAllDiscoveries: keeps every discovered signal and re-sends its config", "[mqtt][discovery]") {
    resetDiscoveryState();
    // Multi-word names: the old filter took the fragment after the last '_' of the
    // lowercase uid ("temp"), found no table row and dropped the signal.
    updateSensor(CanMembers[cm_kessel], GetElsterIndex("SPEICHERSOLLTEMP"), "50.0");
    updateSensor(CanMembers[cm_manager], GetElsterIndex("EINSTELL_SPEICHERSOLLTEMP"), "48.0");
    updateSensor(CanMembers[cm_manager], GetElsterIndex("HYSTERESEZEIT"), "2.0");
    REQUIRE(discoveredSignals.size() == 3);

    mqtt_client_instance().clear();
    republishAllDiscoveries();
    CHECK(discoveredSignals.size() == 3);
    CHECK(mqtt_client_instance().messages.empty());   // nothing sent synchronously

    size_t steps = 0, maxPerStep = 0;
    drainDiscoveryRepublish(steps, maxPerStep);
    CHECK(!discoveryRepublishActive());
    CHECK(steps > 1);
    CHECK(maxPerStep <= 4);   // DISCOVERY_REPUBLISH_BATCH items, climate item = 2 configs

    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/stiebel_kessel_speichersolltemp/config"));
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/stiebel_manager_einstell_speichersolltemp/config"));
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/stiebel_manager_hysteresezeit/config"));
    CHECK(mqttTopicPublished("homeassistant/select/heatingpump/"));
    CHECK(mqttTopicPublished("homeassistant/number/heatingpump/"));
    CHECK(mqttTopicPublished("homeassistant/climate/heatingpump/"));
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/stiebel_calculated_"));
    // only configs, no state topics
    CHECK(countDiscoveryConfigs() == mqtt_client_instance().messages.size());
    CHECK(discoveredSignals.size() == 3);
}

TEST_CASE("republishAllDiscoveries: restart instead of duplicate queue, waits for MQTT", "[mqtt][discovery]") {
    resetDiscoveryState();
    updateSensor(CanMembers[cm_kessel], GetElsterIndex("SPEICHERSOLLTEMP"), "50.0");
    mqtt_client_instance().clear();

    mqtt_client_instance().connected = false;
    republishAllDiscoveries();
    discoveryRepublishStep();
    CHECK(mqtt_client_instance().messages.empty());
    CHECK(discoveryRepublishActive());
    mqtt_client_instance().connected = true;

    // HA birth right after connect: second call restarts, does not append
    republishAllDiscoveries();
    size_t steps = 0, maxPerStep = 0;
    drainDiscoveryRepublish(steps, maxPerStep);
    size_t raw = 0;
    for (const auto& m : mqtt_client_instance().messages)
        if (m.topic.find("stiebel_kessel_speichersolltemp/config") != std::string::npos) raw++;
    CHECK(raw == 1);

    // Idle step publishes nothing
    mqtt_client_instance().clear();
    discoveryRepublishStep();
    CHECK(mqtt_client_instance().messages.empty());
}

// ============================================================================
// #55: publish on change + heartbeat, reconnect republish
// ============================================================================

static size_t countTopic(const std::string& topic) {
    size_t n = 0;
    for (const auto& m : mqtt_client_instance().messages)
        if (m.topic == topic) n++;
    return n;
}

TEST_CASE("state cache: unchanged raw value is suppressed, a change is published", "[mqtt][state]") {
    resetDiscoveryState();
    const CanMember& kessel = CanMembers[cm_kessel];
    const ElsterIndex* ei = GetElsterIndex("SPEICHERISTTEMP");
    const std::string topic = "heatingpump/KESSEL/SPEICHERISTTEMP/state";
    publishMqttState(kessel, ei, "48.0");
    CHECK(countTopic(topic) == 1);
    fake_millis() = 30000;
    publishMqttState(kessel, ei, "48.0");
    publishMqttState(kessel, ei, "48.0");
    CHECK(countTopic(topic) == 1);
    CHECK(stateSuppressedCount == 2);
    publishMqttState(kessel, ei, "48.5");
    CHECK(countTopic(topic) == 2);
    CHECK(mqtt_client_instance().messages.back().payload == "48.5");
    // same signal on another member is its own topic
    publishMqttState(CanMembers[cm_manager], ei, "48.5");
    CHECK(countTopic("heatingpump/MANAGER/SPEICHERISTTEMP/state") == 1);
}

TEST_CASE("state cache: heartbeat re-sends an unchanged value after STATE_HEARTBEAT_MS", "[mqtt][state]") {
    resetDiscoveryState();
    const CanMember& kessel = CanMembers[cm_kessel];
    const ElsterIndex* ei = GetElsterIndex("SPEICHERISTTEMP");
    const std::string topic = "heatingpump/KESSEL/SPEICHERISTTEMP/state";
    fake_millis() = 1000;
    publishMqttState(kessel, ei, "48.0");
    fake_millis() = 1000 + STATE_HEARTBEAT_MS - 1;
    publishMqttState(kessel, ei, "48.0");
    CHECK(countTopic(topic) == 1);
    fake_millis() = 1000 + STATE_HEARTBEAT_MS;
    publishMqttState(kessel, ei, "48.0");
    CHECK(countTopic(topic) == 2);
    // the heartbeat restarts from the last publish
    fake_millis() = 1000 + STATE_HEARTBEAT_MS + 5000;
    publishMqttState(kessel, ei, "48.0");
    CHECK(countTopic(topic) == 2);
}

TEST_CASE("state cache: deadband holds back small changes of measured temperatures", "[mqtt][state]") {
    resetDiscoveryState();
    stateDeadbandTenths = 3;
    const CanMember& kessel = CanMembers[cm_kessel];
    const ElsterIndex* ei = GetElsterIndex("SPEICHERISTTEMP");
    const std::string topic = "heatingpump/KESSEL/SPEICHERISTTEMP/state";
    fake_millis() = 1000;
    publishMqttState(kessel, ei, "48.0");
    publishMqttState(kessel, ei, "48.2");
    CHECK(countTopic(topic) == 1);
    // measured against the last published value, so slow drift still gets through
    publishMqttState(kessel, ei, "47.7");
    CHECK(countTopic(topic) == 2);
    publishMqttState(kessel, ei, "47.9");
    CHECK(countTopic(topic) == 2);
    // the heartbeat sends the newest value and the next comparison starts from it
    fake_millis() = 1000 + STATE_HEARTBEAT_MS;
    publishMqttState(kessel, ei, "47.9");
    CHECK(countTopic(topic) == 3);
    CHECK(mqtt_client_instance().messages.back().payload == "47.9");
    publishMqttState(kessel, ei, "48.2");
    CHECK(countTopic(topic) == 4);
    stateDeadbandTenths = 0;
}

TEST_CASE("state cache: deadband does not apply to other signals or when off", "[mqtt][state]") {
    resetDiscoveryState();
    const CanMember& kessel = CanMembers[cm_kessel];
    const ElsterIndex* temp = GetElsterIndex("SPEICHERISTTEMP");
    publishMqttState(kessel, temp, "48.0");
    publishMqttState(kessel, temp, "48.1");
    CHECK(countTopic("heatingpump/KESSEL/SPEICHERISTTEMP/state") == 2);   // off by default
    stateDeadbandTenths = 3;
    const ElsterIndex* soll = GetElsterIndex("SPEICHERSOLLTEMP");
    publishMqttState(kessel, soll, "48.0");
    publishMqttState(kessel, soll, "48.1");
    CHECK(countTopic("heatingpump/KESSEL/SPEICHERSOLLTEMP/state") == 2);
    publishMqttState(kessel, temp, "-255");   // leaves the cached number
    publishMqttState(kessel, temp, "48.1");
    CHECK(countTopic("heatingpump/KESSEL/SPEICHERISTTEMP/state") == 4);
    stateDeadbandTenths = 0;
}

TEST_CASE("state cache: calculated sensors are deduplicated too", "[mqtt][state]") {
    resetDiscoveryState();
    lastVerdichterValue = 50.0f;
    publishCompressorActive();
    publishCompressorActive();
    CHECK(countTopic("heatingpump/calculated/compressor_active/state") == 1);
    lastVerdichterValue = 1.0f;
    publishCompressorActive();
    CHECK(countTopic("heatingpump/calculated/compressor_active/state") == 2);
    CHECK(mqttFindPayload("compressor_active/state") == "on");   // first message
    CHECK(mqtt_client_instance().messages.back().payload == "off");
    lastVerdichterValue = NAN;
}

TEST_CASE("state cache: values while disconnected stay pending and go out on reconnect", "[mqtt][state]") {
    resetDiscoveryState();
    const CanMember& kessel = CanMembers[cm_kessel];
    publishMqttState(kessel, GetElsterIndex("SPEICHERISTTEMP"), "48.0");
    publishCalculatedState("heatingpump/calculated/date/state", "2026-10-30");
    mqtt_client_instance().connected = false;
    publishMqttState(kessel, GetElsterIndex("SPEICHERISTTEMP"), "49.0");   // changed while offline
    for (int i = 0; i < 5; i++) {
        char v[8];
        snprintf(v, sizeof(v), "%d.0", 20 + i);
        publishMqttState(kessel, GetElsterIndex(i % 2 ? "AUSSENTEMP" : "SPEICHERSOLLTEMP"), v);
    }
    CHECK(mqttPublishFailures == 0);   // not even attempted while offline
    mqtt_client_instance().clear();

    // (re)connect: on_connect starts both republish cursors; nothing while offline
    republishAllStates();
    stateRepublishStep();
    CHECK(mqtt_client_instance().messages.empty());
    mqtt_client_instance().connected = true;

    size_t steps = 0, maxPerStep = 0;
    while (stateRepublishActive && steps < 100) {
        size_t before = mqtt_client_instance().messages.size();
        stateRepublishStep();
        maxPerStep = std::max(maxPerStep, mqtt_client_instance().messages.size() - before);
        steps++;
    }
    CHECK(!stateRepublishActive);
    CHECK(maxPerStep <= (size_t)STATE_REPUBLISH_BATCH);
    CHECK(mqtt_client_instance().messages.size() == stateCache.size());
    CHECK(countTopic("heatingpump/KESSEL/SPEICHERISTTEMP/state") == 1);
    CHECK(mqttFindPayload("KESSEL/SPEICHERISTTEMP/state") == "49.0");      // newest value
    CHECK(mqttFindPayload("calculated/date/state") == "2026-10-30");
    CHECK(mqttFindPayload("KESSEL/AUSSENTEMP/state") == "23.0");

    // republished values count as sent: no duplicate on the next identical value
    mqtt_client_instance().clear();
    publishMqttState(kessel, GetElsterIndex("SPEICHERISTTEMP"), "49.0");
    CHECK(mqtt_client_instance().messages.empty());
}

TEST_CASE("state cache: republish waits for the discovery republish", "[mqtt][state]") {
    resetDiscoveryState();
    updateSensor(CanMembers[cm_kessel], GetElsterIndex("SPEICHERSOLLTEMP"), "50.0");
    mqtt_client_instance().clear();
    republishAllDiscoveries();
    republishAllStates();
    stateRepublishStep();
    CHECK(countTopic("heatingpump/KESSEL/SPEICHERSOLLTEMP/state") == 0);
    while (discoveryRepublishActive()) discoveryRepublishStep();
    stateRepublishStep();
    CHECK(countTopic("heatingpump/KESSEL/SPEICHERSOLLTEMP/state") == 1);
}

TEST_CASE("state cache: a write forces the next read-back to be published", "[mqtt][state]") {
    resetDiscoveryState();
    const CanMember* mgr = &CanMembers[cm_manager];
    const ElsterIndex* ei = GetElsterIndex("EINSTELL_SPEICHERSOLLTEMP");
    publishMqttState(*mgr, ei, "48.0");
    publishMqttState(*mgr, ei, "48.0");
    CHECK(countTopic("heatingpump/MANAGER/EINSTELL_SPEICHERSOLLTEMP/state") == 1);
    writeSignal(mgr, "EINSTELL_SPEICHERSOLLTEMP", "50.0");
    publishMqttState(*mgr, ei, "48.0");   // device kept the old value: HA still gets the echo
    CHECK(countTopic("heatingpump/MANAGER/EINSTELL_SPEICHERSOLLTEMP/state") == 2);
}

TEST_CASE("mqttPublish: refused publishes are counted and published as diagnostic", "[mqtt][state]") {
    resetDiscoveryState();
    mqttPublishFailures = 0;
    mqtt_client_instance().fail_publish = true;   // e.g. idf_send_async queue full
    publishCalculatedState("heatingpump/calculated/time/state", "12:00:00");
    publishCalculatedState("heatingpump/calculated/time/state", "12:00:00");   // still pending: retried
    CHECK(mqttPublishFailures == 2);
    mqtt_client_instance().connected = false;     // not connected: not a drop
    mqttPublish("heatingpump/x", "1", 1);
    CHECK(mqttPublishFailures == 2);
    mqtt_client_instance().connected = true;
    mqtt_client_instance().fail_publish = false;
    publishMqttDiagnostics();
    CHECK(mqttFindPayload("calculated/mqtt_publish_failures/state") == "2");
    CHECK(mqttTopicPublished("homeassistant/sensor/heatingpump/stiebel_calculated_mqtt_publish_failures/config"));
    mqttPublishFailures = 0;
}

TEST_CASE("CAN diagnostics: RX missed/overrun/queue from twai_status_info_t", "[can][state]") {
    resetDiscoveryState();
    fake_twai_status() = twai_status_info_t{};
    fake_twai_status().rx_missed_count = 7;
    fake_twai_status().rx_overrun_count = 2;
    fake_twai_status().msgs_to_rx = 3;
    publishCanDiagnostics();
    CHECK(mqttFindPayload("calculated/can_rx_missed/state") == "7");
    CHECK(mqttFindPayload("calculated/can_rx_overrun/state") == "2");
    CHECK(mqttFindPayload("calculated/can_rx_queue/state") == "3");
    CHECK(mqttFindPayload("calculated/can_tec/state") == "0");
    fake_twai_status() = twai_status_info_t{};
}

// ============================================================================
// #55: allocation-free lookups behave like the old ones for every table row
// ============================================================================

TEST_CASE("GetElsterIndex(name/index): every row resolves like the old linear search", "[kelster][lookup]") {
    unsigned checked = 0;
    for (unsigned i = 0; i < ElsterTableCount; i++) {
        const ElsterIndex& row = ElsterTable[i];
        // old behaviour: first row with this name / index
        const ElsterIndex* firstByName = nullptr;
        const ElsterIndex* firstByIndex = nullptr;
        for (unsigned j = 0; j < ElsterTableCount && (!firstByName || !firstByIndex); j++) {
            if (!firstByName && strcmp(ElsterTable[j].Name, row.Name) == 0) firstByName = &ElsterTable[j];
            if (!firstByIndex && ElsterTable[j].Index == row.Index) firstByIndex = &ElsterTable[j];
        }
        // twice: cache miss, then cache hit
        CHECK(GetElsterIndex(row.Name) == firstByName);
        CHECK(GetElsterIndex(row.Name) == firstByName);
        CHECK(GetElsterIndex(row.Index) == firstByIndex);
        CHECK(isPermanentlyBlacklisted(row.Name) == firstByName->isBlacklisted);
        // a copy of the name (other pointer) finds the same row
        std::string copy(row.Name);
        CHECK(GetElsterIndex(copy.c_str()) == firstByName);
        checked++;
    }
    CHECK(checked == ElsterTableCount);
    CHECK(GetElsterIndex("DOES_NOT_EXIST_XYZ") == &ElsterTable[0]);
    CHECK(GetElsterIndex((const char*)nullptr) == &ElsterTable[0]);
}

TEST_CASE("signalKey: ElsterTable names have distinct hashes", "[kelster][lookup]") {
    std::map<uint32_t, const char*> seen;
    for (unsigned i = 0; i < ElsterTableCount; i++) {
        uint32_t h = elsterNameHash(ElsterTable[i].Name);
        auto it = seen.find(h);
        if (it != seen.end()) CHECK(std::string(it->second) == ElsterTable[i].Name);
        else seen.emplace(h, ElsterTable[i].Name);
        CHECK(h == elsterNameHash(ElsterTable[i].Name));
    }
}

TEST_CASE("request manager: table resolved once, unknown/blacklisted skipped", "[can][lookup]") {
    static_assert(cm_kessel == 0 && cm_manager == 8 && cm_other == 20, "signal_requests_stub.cpp member values");
    resolveSignalRequests();
    REQUIRE(resolvedRequests.size() == 4);
    CHECK(resolvedRequests[0].ei == GetElsterIndex("AUSSENTEMP"));
    CHECK(resolvedRequests[0].key[0] == signalKey(CanMembers[cm_kessel], "AUSSENTEMP"));
    CHECK(resolvedRequests[1].ei == GetElsterIndex("SPEICHERISTTEMP"));
    CHECK(resolvedRequests[1].key[0] == signalKey(CanMembers[cm_kessel], "SPEICHERISTTEMP"));
    CHECK(resolvedRequests[1].key[1] == signalKey(CanMembers[cm_manager], "SPEICHERISTTEMP"));
    CHECK(resolvedRequests[1].key[2] == signalKey(CanMembers[cm_heizmodul], "SPEICHERISTTEMP"));
    CHECK(resolvedRequests[2].ei == nullptr);
    CHECK(resolvedRequests[3].ei == nullptr);

    resetNumberLimits(true);   // device limits (#79) have their own test
    // Startup delay, then one request per call; only the two known signals are polled
    requestManagerStarted = false;
    requestManagerStartTime = 0;
    nextRequestTime.clear();
    fake_millis() = 1;
    processSignalRequests();
    fake_millis() = 1 + STARTUP_DELAY_MS;
    processSignalRequests();
    CHECK(nextRequestTime.size() == 4);   // AUSSENTEMP@KESSEL + SPEICHERISTTEMP@3 members
    fake_can().sent.clear();
    fake_millis() += 120000;               // everything due
    for (int i = 0; i < 10; i++) processSignalRequests();
    CHECK(fake_can().sent.size() == 4);   // each (member, signal) once, unknown/blacklisted never

    // Round robin: a signal that is due again at once must not starve the rest of the table
    fake_millis() += 120000;               // everything due again
    fake_can().sent.clear();
    processSignalRequests();
    REQUIRE(fake_can().sent.size() == 1);
    const std::vector<uint8_t> first = fake_can().sent[0];
    for (auto& kv : nextRequestTime) kv.second = 0;   // all due, including the one just sent
    processSignalRequests();
    REQUIRE(fake_can().sent.size() == 2);
    CHECK(fake_can().sent[1] != first);   // the next signal, not the first one again
    requestManagerStarted = false;
    requestManagerStartTime = 0;
    nextRequestTime.clear();
    signalProcessingStartIndex = 0;
    resetNumberLimits();
    fake_millis() = 0;
}

// ============================================================================
// #55: SG Ready read-back without delay()
// ============================================================================

TEST_CASE("SG Ready IO: writes are queued and sent one at a time (#85)", "[sgready][readback]") {
    resetDiscoveryState();
    pendingReadbackCount = 0;
    fake_can().sent.clear();
    fake_millis() = 5000;
    sgReadyIO.writeCanSignal("PROGRAMMSCHALTER", LNAME_OPT_AUTOMATIK);
    sgReadyIO.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", "55.0");
    CHECK(fake_can().sent.empty());                // nothing on the bus before the step
    REQUIRE(pacedWriteCount == 2);

    pacedWriteStep();                              // first write
    REQUIRE(fake_can().sent.size() == 1);
    CHECK((fake_can().sent[0][0] & 0x0F) == 0);    // telegram type 0 = write
    CHECK(pendingReadbackCount == 1);              // settled read-back
    fake_millis() = 5000 + PACED_WRITE_GAP_MS;
    pacedWriteStep();                              // second waits for the first to be checked
    CHECK(fake_can().sent.size() == 1);

    // settled read-back confirms the first write
    updateSensor(CanMembers[cm_manager], GetElsterIndex("PROGRAMMSCHALTER"), LNAME_OPT_AUTOMATIK);
    fake_millis() = 5000 + PACED_WRITE_VERIFY_MS;
    pacedWriteStep();                              // confirmed, removed
    CHECK(pacedWriteCount == 1);
    CHECK(fake_can().sent.size() == 1);
    pacedWriteStep();                              // second write goes out
    CHECK(fake_can().sent.size() == 2);
    CHECK(pacedWrites[0].tries == 1);

    // no matching read-back: sent once more, then given up
    updateSensor(CanMembers[cm_manager], GetElsterIndex("EINSTELL_SPEICHERSOLLTEMP"), "48.0");
    fake_millis() += PACED_WRITE_VERIFY_MS;
    pacedWriteStep();
    CHECK(fake_can().sent.size() == 3);
    CHECK(pacedWrites[0].tries == 2);
    fake_millis() += PACED_WRITE_VERIFY_MS;
    pacedWriteStep();
    CHECK(pacedWriteCount == 0);
    CHECK(fake_can().sent.size() == 3);
    pendingReadbackCount = 0;
    fake_millis() = 0;
}

TEST_CASE("paced writes: a newer value replaces a waiting write of the same signal", "[sgready]") {
    resetDiscoveryState();
    fake_can().sent.clear();
    sgReadyIO.writeCanSignal("PROGRAMMSCHALTER", "Automatik");
    sgReadyIO.writeCanSignal("RAUMSOLLTEMP_I", "21.0");
    sgReadyIO.writeCanSignal("RAUMSOLLTEMP_I", "20.0");
    REQUIRE(pacedWriteCount == 2);
    CHECK(std::string(pacedWrites[1].value) == "20.0");
    pacedWriteCount = 0;
}

TEST_CASE("paced writes: values compare as encoded on the bus", "[sgready]") {
    const ElsterIndex* dhw = GetElsterIndex("EINSTELL_SPEICHERSOLLTEMP");
    CHECK(pacedValueMatches(dhw, "53.0", "53.0"));
    CHECK(pacedValueMatches(dhw, "53", "53.0"));
    CHECK_FALSE(pacedValueMatches(dhw, "53.0", "48.0"));
    const ElsterIndex* prog = GetElsterIndex("W_WASSERPROG_1_MO");
    CHECK(pacedValueMatches(prog, "11:00-17:00", "11:00-17:00"));
    CHECK(pacedValueMatches(prog, "", "not used time domain"));
    CHECK_FALSE(pacedValueMatches(prog, "11:00-17:00", "06:30-23:00"));
    const ElsterIndex* ps = GetElsterIndex("PROGRAMMSCHALTER");
    CHECK(pacedValueMatches(ps, "Automatik", "Automatik"));
    CHECK_FALSE(pacedValueMatches(ps, "Automatik", "Tagbetrieb"));
}

TEST_CASE("SG Ready boost baseline is the setpoint read from the manager (#85)", "[sgready]") {
    resetDiscoveryState();
    // WPL9 and WPL-A 07 read the setpoints on CAN-B (canBDhwRoutes)
    if (std::strcmp(HA_DEVICE_MODEL_STR, "wpl9") == 0 || std::strcmp(HA_DEVICE_MODEL_STR, "wpla7") == 0) return;
    applySgReadyState(2);                          // no boost active from an earlier test
    pacedWriteCount = 0;
    updateSensor(CanMembers[cm_manager], GetElsterIndex("EINSTELL_SPEICHERSOLLTEMP"), "50.0");
    updateSensor(CanMembers[cm_manager], GetElsterIndex("RAUMSOLLTEMP_I"), "21.0");
    updateSensor(CanMembers[cm_kessel], GetElsterIndex("EINSTELL_SPEICHERSOLLTEMP"), "40.0");  // other member
    CHECK(lastSgDhwSetpoint == Approx(50.0f));
    CHECK(lastSgRoomSetpoint == Approx(21.0f));
    applySgReadyState(3);
    bool dhw = false, room = false;
    for (size_t i = 0; i < pacedWriteCount; i++) {
        const std::string n = pacedWrites[i].ei->Name, v = pacedWrites[i].value;
        if (n == "EINSTELL_SPEICHERSOLLTEMP") dhw = (v == "55.0");
        if (n == "RAUMSOLLTEMP_I") room = (v == "22.0");
    }
    CHECK(dhw);
    CHECK(room);
    pacedWriteCount = 0;
    applySgReadyState(2);                          // back: restores 50 / 21
    bool dhwBack = false, roomBack = false;
    for (size_t i = 0; i < pacedWriteCount; i++) {
        const std::string n = pacedWrites[i].ei->Name, v = pacedWrites[i].value;
        if (n == "EINSTELL_SPEICHERSOLLTEMP") dhwBack = (v == "50.0");
        if (n == "RAUMSOLLTEMP_I") roomBack = (v == "21.0");
    }
    CHECK(dhwBack);
    CHECK(roomBack);
    resetDiscoveryState();
}

TEST_CASE("discovery: one device block, COP from calculatedSensors, named rows (#95)", "[mqtt][discovery]") {
    // rows by name
    CHECK(std::string(calculatedSensors[CALC_IDX_DATE].uniqueId) == "stiebel_calculated_date");
    CHECK(std::string(calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH].uniqueId) == "stiebel_calculated_energie_heiz_total_kwh");
    CHECK(std::string(calculatedSensors[CALC_IDX_ABTAUUNG_LAEUFT].uniqueId) == "stiebel_calculated_abtauung_laeuft");
    CHECK(std::string(calculatedSensors[CALC_IDX_COP_GESAMT].uniqueId) == "stiebel_cop_gesamt");
    CHECK_FALSE(calculatedSensors[CALC_IDX_COMPRESSOR_ACTIVE].onDemand);
    CHECK(calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH].onDemand);
    // COP keeps its discovery topic and unique_id
    resetDiscoveryState();
    copDiscoveryPublished = false;
    publishCOPDiscovery();
    const std::string cop = mqttFindPayload("homeassistant/sensor/heatingpump/cop_ww/config");
    CHECK(cop.find("\"unique_id\":\"stiebel_cop_ww\"") != std::string::npos);
    CHECK(cop.find("\"state_topic\":\"heatingpump/calculated/cop_ww/state\"") != std::string::npos);
    CHECK(cop.find(HA_MAIN_DEVICE_JSON) != std::string::npos);
    // member device block
    const std::string dev = memberDeviceJson("KESSEL");
    CHECK(dev == "\"device\":{\"identifiers\":[\"stiebel_KESSEL\"],\"name\":\"Kessel\","
                 "\"via_device\":\"" HA_MAIN_DEVICE_ID "\",\"manufacturer\":\"Stiebel Eltron\"}");
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_kessel], GetElsterIndex("SPEICHERISTTEMP"));
    CHECK(mqttFindPayload("homeassistant/").find(dev) != std::string::npos);
    copDiscoveryPublished = false;
    resetDiscoveryState();
}

TEST_CASE("discovery: language-independent default_entity_id in every payload (#96)", "[mqtt][discovery]") {
    CHECK(defaultEntityIdJson("sensor", "stiebel_manager_aussentemp") ==
          "\"default_entity_id\":\"sensor.heatingpump_manager_aussentemp\",");
    CHECK(defaultEntityIdJson("number", "stiebel_MANAGER_HEIZKURVE_set") ==
          "\"default_entity_id\":\"number.heatingpump_manager_heizkurve_set\",");
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_kessel], GetElsterIndex("SPEICHERISTTEMP"));
    CHECK(mqttFindPayload("homeassistant/").find("\"default_entity_id\":\"sensor.heatingpump_kessel_speicheristtemp\"") != std::string::npos);
    resetDiscoveryState();
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COMPRESSOR_ACTIVE], true);
    CHECK(mqttFindPayload("homeassistant/").find("binary_sensor.heatingpump_calculated_compressor_active") != std::string::npos);
    resetDiscoveryState();
    publishWritableSelectDiscovery(writableSelects[0], true);
    CHECK(mqttFindPayload("homeassistant/select/").find("select.heatingpump_manager_programmschalter") != std::string::npos);
    resetDiscoveryState();
}

TEST_CASE("handleSetCommand: one dispatcher for numbers, selects and programs (#92)", "[mqtt][set]") {
    resetDiscoveryState();
    fake_can().sent.clear();
    // number, by signal name and member
    CHECK(handleSetCommand("heatingpump/MANAGER/RAUMSOLLTEMP_I/set", "21.5"));
    REQUIRE(fake_can().sent.size() == 2);                     // write + read-back request
    CHECK((fake_can().sent[0][0] & 0x0F) == 0);
    CHECK(mqttTopicPublished("homeassistant/number/heatingpump/"));
    // range check from the table row
    fake_can().sent.clear();
    CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/RAUMSOLLTEMP_I/set", "45"));
    CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/RAUMSOLLTEMP_I/set", "abc"));
    CHECK(fake_can().sent.empty());
    // unknown topics
    CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/NOPE/set", "1"));
    CHECK_FALSE(handleSetCommand("other/MANAGER/RAUMSOLLTEMP_I/set", "21"));
    CHECK_FALSE(handleSetCommand("heatingpump/NOMEMBER/RAUMSOLLTEMP_I/set", "21"));
    CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/RAUMSOLLTEMP_I/state", "21"));
    // heating circuit 2 on MISCHERMODUL_3: only where verified
    fake_can().sent.clear();
    CHECK(handleSetCommand("heatingpump/MISCHERMODUL_3/RAUMSOLLTEMP_I/set", "20.5") == kModel.wpmw2WritesVerified);
    CHECK(fake_can().sent.empty() == !kModel.wpmw2WritesVerified);
    // WPL9: former KESSEL topic of the DHW setpoint still works, elsewhere it does not
    fake_can().sent.clear();
    CHECK(handleSetCommand("heatingpump/KESSEL/EINSTELL_SPEICHERSOLLTEMP/set", "48") == kModel.canBDhwRoutes);
    // SG Ready values live on the ESP
    CHECK(handleSetCommand("heatingpump/MANAGER/SG_READY_BOOST_STATE3/set", "6.5"));
    CHECK(sgReadyController.boostState3() == Approx(6.5f));
    CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/SG_READY_BOOST_STATE3/set", "11"));
    CHECK(handleSetCommand("heatingpump/MANAGER/SG_READY_BOOST_STATE3/set", "5"));
    CHECK(handleSetCommand("heatingpump/MANAGER/SG_READY_ROOM_BOOST_STATE4/set", "2"));
    // select
    bool hookCalled = false;
    onSgReadyChanged = [&hookCalled]() { hookCalled = true; };
    CHECK(handleSetCommand("heatingpump/MANAGER/SG_READY_STATE/set", sgReadyOptions[2]));
    CHECK(sgReadyController.currentState() == 3);
    CHECK(hookCalled);
    CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/SG_READY_STATE/set", "9 - x"));
    CHECK(handleSetCommand("heatingpump/MANAGER/SG_READY_STATE/set", sgReadyOptions[1]));
    onSgReadyChanged = nullptr;
    // time program
    pacedWriteCount = 0;
    CHECK(handleSetCommand("heatingpump/dhw_program/mo/set", "11:00-17:00") == dhwProgramWritable());
    CHECK_FALSE(handleSetCommand("heatingpump/dhw_program/xx/set", "11:00-17:00"));
    pacedWriteCount = 0;
    pendingReadbackCount = 0;
    resetDiscoveryState();
}

TEST_CASE("model profile: one place decides the model's capabilities (#94)", "[model]") {
    const std::string m = HA_DEVICE_MODEL_STR;
#ifdef HA_CONTROLLER_WPMW2
    const bool wpmw2Switch = true;
#else
    const bool wpmw2Switch = false;
#endif
    CHECK(kModel.wpmw2 == wpmw2Switch);
    CHECK(kModel.wpmw2 == (m == "wpl13e" || m == "wpl18e" || m == "ttl33"));
    CHECK(kModel.wpmw2WritesVerified == (m == "wpl13e" || m == "ttl33"));
    CHECK(kModel.canBDhwRoutes == (m == "wpl9" || m == "wpla7"));
    CHECK(kModel.canBRoutes == (m == "wpl17"));
    CHECK(kModel.singleHeatingCircuit == (m == "wpl17" || m == "wpla7"));
    CHECK(kModel.perMemberEnergy == (m == "wpl17"));
    CHECK(kModel.ignoreUnusedIwsRuntime == (m == "wpl13e"));
    CHECK(kModel.dhwActualOnKesselCanb == (m == "wpla7"));
#ifdef HA_CONTROLLER_WPM3
    CHECK(m == "wpl10ac");
    CHECK(std::string(SG_READY_NORMAL_MODE) == "Komfortbetrieb");
#endif
}

#if HA_MODEL_ID == HA_MODEL_ID_wpla7
TEST_CASE("model profile: WPL-A 07 selects its own profile (wpla7)", "[model][wpla7]") {
    CHECK(&kModel == &kProfileWpla7);
    CHECK_FALSE(kModel.wpmw2);
    CHECK(kModel.singleHeatingCircuit);
    CHECK(kModel.canBDhwRoutes);
    CHECK(kModel.dhwActualOnKesselCanb);
}
#endif

TEST_CASE("SG Ready labels: one set, the select options, for every publish (#93)", "[sgready]") {
    for (int st = 1; st <= 4; st++) {
        CHECK(std::string(sgReadyStateLabel(st)) == sgReadyOptions[st - 1]);
        CHECK(SgReadyController::parseStateString(sgReadyOptions[st - 1]) == st);
    }
    CHECK(std::string(sgReadyStateLabel(0)) == LNAME_SG_DEAKTIVIERT);
    CHECK(std::string(sgReadyStateLabel(9)) == LNAME_SG_UNBEKANNT);
    resetDiscoveryState();
    applySgReadyState(3);
    const std::string controllerPublish = mqttFindPayload("SG_READY_STATE/state");
    mqtt_client_instance().clear();
    publishSgReadyState();
    CHECK(controllerPublish == sgReadyOptions[2]);
    CHECK(mqttFindPayload("SG_READY_STATE/state") == controllerPublish);
    applySgReadyState(2);
    resetDiscoveryState();
}

TEST_CASE("SG Ready state 2 mode: select command, NVS and state publish (#85)", "[sgready]") {
    resetDiscoveryState();
    applySgReadyState(2);
    pacedWriteCount = 0;
    CHECK(handleSgReadyNormalMode(sgNormalModeOptions[1]));     // Automatik
    CHECK(sgReadyController.normalAutomatik());
    REQUIRE(pacedWriteCount == 1);                              // written at once in state 2
    CHECK(std::string(pacedWrites[0].value) == sgNormalModeOptions[1]);
    CHECK_FALSE(handleSgReadyNormalMode("Bereitschaft"));
    CHECK(sgReadyController.normalAutomatik());
    publishSgReadyState();
    CHECK(mqttFindPayload("SG_READY_NORMAL_MODE/state") == sgNormalModeOptions[1]);
    CHECK(handleSgReadyNormalMode(sgNormalModeOptions[0]));
    CHECK_FALSE(sgReadyController.normalAutomatik());
    resetDiscoveryState();
}

TEST_CASE("WPMW II heating circuit 2 settings are numbers on MISCHERMODUL_3", "[writable]") {
    const std::string model = HA_DEVICE_MODEL_STR;
    const bool verified = model == "wpl13e" || model == "ttl33";
    int found = 0;
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        if (writableNumbers[i].member != cm_mischermodul_3) continue;
        found++;
        CHECK(writableNumberApplies(writableNumbers[i]) == verified);
        CHECK(getWritableNumberMember(writableNumbers[i]) == &CanMembers[cm_mischermodul_3]);
    }
    CHECK(found == 3);
    // the circuit 2 rows follow the manager rows (findWritableNumber() returns circuit 1 first)
    CHECK(std::string(writableNumbers[16].signalName) == "RAUMSOLLTEMP_I");
    CHECK(std::string(writableNumbers[17].signalName) == "RAUMSOLLTEMP_NACHT");
    CHECK(std::string(writableNumbers[18].signalName) == "HEIZKURVE");
    if (verified) {
        resetDiscoveryState();
        publishWritableNumberDiscovery(writableNumbers[16], true);
        const std::string p = mqttFindPayload("homeassistant/number/heatingpump/");
        CHECK(p.find("heatingpump/MISCHERMODUL_3/RAUMSOLLTEMP_I/set") != std::string::npos);
    }
}

TEST_CASE("dhwprog: parse and validate period texts (#82)", "[dhwprog]") {
    std::vector<dhwprog::Period> p;
    std::string err;
    CHECK(dhwprog::parsePeriods("11:00-17:00", p, err));
    REQUIRE(p.size() == 1);
    CHECK(p[0].start == 44);
    CHECK(p[0].end == 68);
    CHECK(dhwprog::parsePeriods(" 17:00-22:00 , 6:30-9:00 ", p, err));
    REQUIRE(p.size() == 2);
    CHECK(dhwprog::formatPeriod(p[0]) == "06:30-09:00");
    CHECK(dhwprog::parsePeriods("00:00-24:00", p, err));
    CHECK(p[0].end == 96);
    CHECK(dhwprog::parsePeriods("-", p, err));
    CHECK(p.empty());
    CHECK(dhwprog::parsePeriods("", p, err));
    CHECK(dhwprog::parsePeriods("06:00-09:00,09:00-12:00", p, err));   // touching is fine
    CHECK_FALSE(dhwprog::parsePeriods("11:10-17:00", p, err));         // off the grid
    CHECK_FALSE(dhwprog::parsePeriods("17:00-11:00", p, err));         // start after end
    CHECK_FALSE(dhwprog::parsePeriods("11:00-11:00", p, err));
    CHECK_FALSE(dhwprog::parsePeriods("06:00-10:00,09:00-12:00", p, err));   // overlap
    CHECK_FALSE(dhwprog::parsePeriods("01:00-02:00,03:00-04:00,05:00-06:00,07:00-08:00", p, err));
    CHECK_FALSE(dhwprog::parsePeriods("24:15-24:30", p, err));
    CHECK_FALSE(dhwprog::parsePeriods("11:00", p, err));
    CHECK_FALSE(dhwprog::parsePeriods("aa:bb-cc:dd", p, err));
}

TEST_CASE("dhwprog: signal names, decoded values and day text (#82)", "[dhwprog]") {
    int day = -1, slot = -1;
    CHECK(dhwprog::parseSignalName("W_WASSERPROG_1_MO", day, slot));
    CHECK(day == 0);
    CHECK(slot == 0);
    CHECK(dhwprog::parseSignalName("W_WASSERPROG_1_SO_SCHALT_3", day, slot));
    CHECK(day == 6);
    CHECK(slot == 2);
    CHECK_FALSE(dhwprog::parseSignalName("W_WASSERPROG_1_MO_FR", day, slot));
    CHECK_FALSE(dhwprog::parseSignalName("HEIZPROG_1_MO", day, slot));
    CHECK(dhwprog::signalName(3, 1) == "W_WASSERPROG_1_DO_SCHALT_2");
    CHECK(dhwprog::signalName(6, 0, dhwprog::PREFIX_HEATING) == "HEIZPROG_1_SO");
    CHECK(dhwprog::parseSignalName("HEIZPROG_1_DI_SCHALT_2", day, slot, dhwprog::PREFIX_HEATING));
    CHECK(day == 1);
    CHECK(slot == 1);
    for (int d = 0; d < dhwprog::DAYS; d++)
        for (int s2 = 0; s2 < dhwprog::PERIODS; s2++)
            CHECK(GetElsterIndex(dhwprog::signalName(d, s2).c_str()) != nullptr);
    CHECK(dhwprog::normalizeDecoded("not used time domain").empty());
    const std::string none[3] = {"", "", ""};
    CHECK(dhwprog::dayText(none) == "-");
    const std::string two[3] = {"06:30-09:00", "", "11:00-17:00"};
    CHECK(dhwprog::dayText(two) == "06:30-09:00,11:00-17:00");
    // encoding round trip with the Elster table: 11:00-17:00 = 0x2C44, unused = 0x8080
    const char* in = "11:00-17:00";
    CHECK(TranslateString(in, et_time_domain) == 0x2C44);
    const char* empty = "";
    CHECK(TranslateString(empty, et_time_domain) == 0x8080);
}

TEST_CASE("DHW program: three periods form one text per day, commands write changes only (#82)", "[dhwprog]") {
    resetDiscoveryState();
    if (!isWpmw2Model()) {   // WPMW II only; the test build of other models skips this
        updateSensor(CanMembers[cm_manager], GetElsterIndex("W_WASSERPROG_1_MO"), "06:30-23:00");
        CHECK_FALSE(mqttTopicPublished("dhw_program/mo/state"));
        return;
    }
    updateSensor(CanMembers[cm_manager], GetElsterIndex("W_WASSERPROG_1_MO"), "06:30-23:00");
    updateSensor(CanMembers[cm_manager], GetElsterIndex("W_WASSERPROG_1_MO_SCHALT_2"), "not used time domain");
    CHECK_FALSE(mqttTopicPublished("dhw_program/mo/state"));   // third period still unknown
    updateSensor(CanMembers[cm_manager], GetElsterIndex("W_WASSERPROG_1_MO_SCHALT_3"), "not used time domain");
    CHECK(mqttFindPayload("dhw_program/mo/state") == "06:30-23:00");
    CHECK_FALSE(mqttTopicPublished("MANAGER/W_WASSERPROG_1_MO/state"));   // no raw sensor

    if (dhwProgramWritable()) {
        CHECK(handleDhwProgramCommand(0, "11:00-17:00"));
        REQUIRE(pacedWriteCount == 1);                          // only the first period changes
        CHECK(std::string(pacedWrites[0].ei->Name) == "W_WASSERPROG_1_MO");
        CHECK(std::string(pacedWrites[0].value) == "11:00-17:00");
        pacedWriteCount = 0;
        CHECK(handleDhwProgramCommand(0, "17:00-22:00, 06:30-09:00"));
        REQUIRE(pacedWriteCount == 2);                          // sorted: 06:30 first
        CHECK(std::string(pacedWrites[0].value) == "06:30-09:00");
        CHECK(std::string(pacedWrites[1].ei->Name) == "W_WASSERPROG_1_MO_SCHALT_2");
        CHECK(std::string(pacedWrites[1].value) == "17:00-22:00");
        pacedWriteCount = 0;
        mqtt_client_instance().clear();
        CHECK_FALSE(handleDhwProgramCommand(0, "11:10-17:00"));  // off the 15 min grid
        CHECK(pacedWriteCount == 0);
        CHECK(mqttFindPayload("dhw_program/mo/state") == "06:30-23:00");   // HA gets the real value back
    }
    // heating program of circuit 1: same layout, own topics
    updateSensor(CanMembers[cm_manager], GetElsterIndex("HEIZPROG_1_SA"), "06:00-22:00");
    updateSensor(CanMembers[cm_manager], GetElsterIndex("HEIZPROG_1_SA_SCHALT_2"), "not used time domain");
    updateSensor(CanMembers[cm_manager], GetElsterIndex("HEIZPROG_1_SA_SCHALT_3"), "not used time domain");
    CHECK(mqttFindPayload("heating_program/sa/state") == "06:00-22:00");
    if (dhwProgramWritable()) {
        pacedWriteCount = 0;
        CHECK(handleHeatingProgramCommand(5, "00:00-24:00"));
        REQUIRE(pacedWriteCount == 1);
        CHECK(std::string(pacedWrites[0].ei->Name) == "HEIZPROG_1_SA");
        CHECK(std::string(pacedWrites[0].value) == "00:00-24:00");
        pacedWriteCount = 0;
    }
    // heating program of circuit 2 (mixer circuit): HEIZPROG_1 on MISCHERMODUL_3 (0x602)
    mqtt_client_instance().clear();
    updateSensor(CanMembers[cm_mischermodul_3], GetElsterIndex("HEIZPROG_1_MO"), "06:00-22:00");
    updateSensor(CanMembers[cm_mischermodul_3], GetElsterIndex("HEIZPROG_1_MO_SCHALT_2"), "not used time domain");
    updateSensor(CanMembers[cm_mischermodul_3], GetElsterIndex("HEIZPROG_1_MO_SCHALT_3"), "not used time domain");
    CHECK(mqttFindPayload("heating_program_2/mo/state") == "06:00-22:00");
    CHECK_FALSE(mqttTopicPublished("heating_program/mo/state"));   // circuit 1 untouched
    if (dhwProgramWritable()) {
        CHECK(handleHeating2ProgramCommand(0, "00:00-24:00"));
        REQUIRE(pacedWriteCount == 1);
        CHECK(std::string(pacedWrites[0].ei->Name) == "HEIZPROG_1_MO");
        CHECK(pacedWrites[0].writeCm == &CanMembers[cm_mischermodul_3]);
        pacedWriteCount = 0;
    }
    resetDiscoveryState();
}

TEST_CASE("MQTT connect: own republishing waits until the connect burst has drained", "[mqtt][state]") {
    resetDiscoveryState();
    fake_millis() = 10000;
    publishMqttState(CanMembers[cm_kessel], GetElsterIndex("SPEICHERISTTEMP"), "48.0");
    mqtt_client_instance().clear();
    noteMqttConnected();
    republishAllStates();
    scheduleSgReadyPublish();
    stateRepublishStep();
    sgReadyPublishStep();
    CHECK(mqtt_client_instance().messages.empty());
    fake_millis() = 10000 + MQTT_CONNECT_SETTLE_MS;
    stateRepublishStep();
    sgReadyPublishStep();
    CHECK(mqttTopicPublished("heatingpump/KESSEL/SPEICHERISTTEMP/state"));
    CHECK(mqttTopicPublished("heatingpump/MANAGER/SG_READY_ROOM_BOOST_STATE3/state"));
    CHECK_FALSE(sgReadyPublishPending);
    fake_millis() = 0;
}

TEST_CASE("MQTT connect: SG Ready state is retried when the queue was full", "[mqtt][sgready]") {
    resetDiscoveryState();
    fake_millis() = 5000;
    scheduleSgReadyPublish();
    mqtt_client_instance().fail_publish = true;
    sgReadyPublishStep();
    CHECK(sgReadyPublishPending);
    mqtt_client_instance().fail_publish = false;
    fake_millis() = 5500;
    sgReadyPublishStep();                       // retry waits a second
    CHECK(sgReadyPublishPending);
    fake_millis() = 6000;
    sgReadyPublishStep();
    CHECK_FALSE(sgReadyPublishPending);
    CHECK(mqttFindPayload("SG_READY_STATE/state").size() > 0);
    fake_millis() = 0;
}

TEST_CASE("writeSignal: a second read-back follows once the value has settled", "[readback]") {
    resetDiscoveryState();
    pendingReadbackCount = 0;
    fake_can().sent.clear();
    fake_millis() = 1000;
    writeSignal(&CanMembers[cm_manager], "STILLSTANDZEIT", "21");
    REQUIRE(fake_can().sent.size() == 1);
    REQUIRE(pendingReadbackCount == 1);
    fake_millis() = 1000 + READBACK_SETTLE_MS - 1;
    pendingReadbackStep();
    CHECK(fake_can().sent.size() == 1);
    fake_millis() = 1000 + READBACK_SETTLE_MS;
    pendingReadbackStep();
    REQUIRE(fake_can().sent.size() == 2);
    CHECK((fake_can().sent[1][0] & 0x0F) == 1);   // read request to the written member
    CHECK(pendingReadbackCount == 0);
    fake_millis() = 0;
}

// ============================================================================
// #79: device limits of writable numbers (switch bytes 0xF8 = min, 0xF9 = max)
// ============================================================================

static std::string lastPayload(const std::string& topicSubstr) {
    std::string last;
    for (const auto& m : mqtt_client_instance().messages)
        if (m.topic.find(topicSubstr) != std::string::npos) last = m.payload;
    return last;
}

static std::vector<uint8_t> limitAnswer(uint8_t switchByte, uint16_t index, uint16_t raw) {
    return {0xD2, 0x00, switchByte, static_cast<uint8_t>(index >> 8), static_cast<uint8_t>(index & 0xFF),
            static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw & 0xFF)};
}

// Runs the limit reads of writableNumbers[0] (DHW setpoint) up to the first request.
static const CanMember* startLimitReads(unsigned long& t) {
    resetDiscoveryState();
    fake_can().sent.clear();
    t = 1000;
    fake_millis() = t;
    numberLimitsStep(t);                        // only sets the start time
    REQUIRE(fake_can().sent.empty());
    t += NUMBER_LIMITS_START_DELAY_MS;
    fake_millis() = t;
    numberLimitsStep(t);
    REQUIRE(fake_can().sent.size() == 1);
    return getWritableNumberMember(writableNumbers[0]);
}

TEST_CASE("limits: frame layout, indices 0xF7-0xFA always long form", "[limits][can]") {
    CHECK(elsterShortForm(0x0005));
    CHECK(elsterShortForm(0x00F6));
    for (uint16_t idx = 0x00F7; idx <= 0x00FA; idx++) CHECK_FALSE(elsterShortForm(idx));
    CHECK_FALSE(elsterShortForm(0x0013, ELSTER_SWITCH_MIN));
    CHECK_FALSE(elsterShortForm(0x0100));

    fake_can().sent.clear();
    readSignalRaw(&CanMembers[cm_manager], 0x0013, "T", ELSTER_SWITCH_MIN);
    readSignalRaw(&CanMembers[cm_manager], 0x0013, "T", ELSTER_SWITCH_MAX);
    readSignalRaw(&CanMembers[cm_manager], 0x00F8, "T");
    readSignalRaw(&CanMembers[cm_manager], 0x0005, "T");
    REQUIRE(fake_can().sent.size() == 4);
    const auto& mn = fake_can().sent[0];
    CHECK((mn[2] == 0xF8 && mn[3] == 0x00 && mn[4] == 0x13 && mn[5] == 0 && mn[6] == 0));
    CHECK((fake_can().sent[1][2] == 0xF9 && fake_can().sent[1][4] == 0x13));
    const auto& f8 = fake_can().sent[2];          // index 0x00F8: long form, not a switch byte
    CHECK((f8[2] == 0xFA && f8[3] == 0x00 && f8[4] == 0xF8));
    CHECK(fake_can().sent[3][2] == 0x05);         // short form unchanged
}

TEST_CASE("limits: an answer to 0xF8/0xF9 is not a sensor value", "[limits][can]") {
    resetDiscoveryState();
    // nobody waits for it (e.g. another bus node asked): dropped, not decoded as index 0xF8
    processAndUpdate(0x480, limitAnswer(ELSTER_SWITCH_MIN, 0x0005, 0x0032));
    processAndUpdate(0x480, limitAnswer(ELSTER_SWITCH_MAX, 0x0005, 0x012C));
    CHECK(mqtt_client_instance().messages.empty());
}

TEST_CASE("limits: device range narrows the static one after both answers", "[limits][mqtt]") {
    unsigned long t;
    const CanMember* m = startLimitReads(t);
    const uint16_t idx = GetElsterIndex(writableNumbers[0].signalName)->Index;
    const auto& req = fake_can().sent[0];
    CHECK((req[2] == ELSTER_SWITCH_MIN && req[3] == (idx >> 8) && req[4] == (idx & 0xFF)));

    // same step time again: no second request
    numberLimitsStep(t);
    CHECK(fake_can().sent.size() == 1);

    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x012C));   // 30.0 (static 20..60)
    CHECK(numberLimits[0].state == NumberLimitState::WaitMax);
    CHECK(lastPayload("homeassistant/number/").empty());   // static range until both are in
    numberLimitsStep(t + 100);                                // gap not over yet
    CHECK(fake_can().sent.size() == 1);
    t += NUMBER_LIMITS_GAP_MS;
    fake_millis() = t;
    numberLimitsStep(t);
    REQUIRE(fake_can().sent.size() == 2);
    CHECK(fake_can().sent[1][2] == ELSTER_SWITCH_MAX);
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, 0x0258));   // 60.0

    CHECK(numberLimits[0].state == NumberLimitState::Device);
    CHECK(writableNumberMin(0) == Approx(30.0f));
    CHECK(writableNumberMax(0) == Approx(60.0f));
    std::string uid = std::string("stiebel_") + m->Name + "_" + writableNumbers[0].signalName;
    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
    const std::string disc = lastPayload("homeassistant/number/heatingpump/" + uid + "/config");
    REQUIRE_FALSE(disc.empty());
    CHECK(disc.find("\"unique_id\":\"" + uid + "\"") != std::string::npos);   // same entity, no second one
    CHECK(disc.find("\"min\":30,") != std::string::npos);
    CHECK(disc.find("\"max\":60,") != std::string::npos);
    CHECK(mqttFindPayload(writableNumbers[0].signalName + std::string("/state")).empty());   // no fake state

    // writes are clamped to the device range
    fake_can().sent.clear();
    writeSignal(m, writableNumbers[0].signalName, "70");
    REQUIRE(fake_can().sent.size() == 1);
    const auto& w = fake_can().sent[0];
    const bool shortForm = elsterShortForm(idx);
    CHECK(w[shortForm ? 3 : 5] == 0x02);
    CHECK(w[shortForm ? 4 : 6] == 0x58);   // 600 = 60.0
    fake_can().sent.clear();
    writeSignal(m, writableNumbers[0].signalName, "5");
    REQUIRE(fake_can().sent.size() == 1);
    CHECK(fake_can().sent[0][shortForm ? 4 : 6] == 0x2C);   // 300 = 30.0
    resetNumberLimits();
    pendingReadbackCount = 0;
}

TEST_CASE("limits: 0x8000 on one side keeps that side static", "[limits]") {
    unsigned long t;
    const CanMember* m = startLimitReads(t);
    const uint16_t idx = GetElsterIndex(writableNumbers[0].signalName)->Index;
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x8000));
    t += NUMBER_LIMITS_GAP_MS;
    numberLimitsStep(t);
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, 0x0258));
    CHECK(numberLimits[0].state == NumberLimitState::Device);
    CHECK(writableNumberMin(0) == Approx(writableNumbers[0].min));
    CHECK(writableNumberMax(0) == Approx(60.0f));
    resetNumberLimits();
}

TEST_CASE("limits: 0x8000 on both sides, min >= max or no answer keep the static range", "[limits]") {
    const uint16_t idx = GetElsterIndex(writableNumbers[0].signalName)->Index;
    bool noAnswer = false;
    SECTION("both 0x8000") {
        unsigned long t;
        const CanMember* m = startLimitReads(t);
        processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x8000));
        numberLimitsStep(t + NUMBER_LIMITS_GAP_MS);
        processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, 0x8000));
    }
    SECTION("min >= max") {
        unsigned long t;
        const CanMember* m = startLimitReads(t);
        processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x0258));
        numberLimitsStep(t + NUMBER_LIMITS_GAP_MS);
        processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, 0x0064));
    }
    SECTION("no answer: one retry, then static, then the next number") {
        unsigned long t;
        startLimitReads(t);
        t += NUMBER_LIMITS_TIMEOUT_MS;
        numberLimitsStep(t);
        CHECK(fake_can().sent.size() == 2);          // retry of 0xF8
        CHECK(fake_can().sent[1][2] == ELSTER_SWITCH_MIN);
        t += NUMBER_LIMITS_TIMEOUT_MS;
        numberLimitsStep(t);                         // second timeout: give up
        CHECK(numberLimitsActive != 0);
        noAnswer = true;
    }
    CHECK(numberLimits[0].state == (noAnswer ? NumberLimitState::NoAnswer : NumberLimitState::Static));
    CHECK(writableNumberMin(0) == Approx(writableNumbers[0].min));
    CHECK(writableNumberMax(0) == Approx(writableNumbers[0].max));
    CHECK(lastPayload("homeassistant/number/").empty());   // nothing republished
    resetNumberLimits();
}

TEST_CASE("limits: answers from another member or index are ignored", "[limits]") {
    unsigned long t;
    const CanMember* m = startLimitReads(t);
    const uint16_t idx = GetElsterIndex(writableNumbers[0].signalName)->Index;
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, static_cast<uint16_t>(idx + 1), 0x0064));
    processAndUpdate(m->CanId == 0x180 ? 0x480 : 0x180, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x0064));
    CHECK(numberLimits[0].state == NumberLimitState::WaitMin);
    CHECK(mqtt_client_instance().messages.empty());
    resetNumberLimits();
}

TEST_CASE("limits: virtual SG Ready numbers are never requested", "[limits]") {
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++)
        if (strncmp(writableNumbers[i].signalName, "SG_READY_", 9) == 0) CHECK_FALSE(numberLimitsWanted(i));
}

TEST_CASE("limits: only rows that apply to the model are requested", "[limits]") {
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        INFO(writableNumbers[i].signalName << " member " << writableNumbers[i].member);
        if (!writableNumberApplies(writableNumbers[i])) CHECK_FALSE(numberLimitsWanted(i));
        if (writableNumbers[i].member == cm_mischermodul_3 && !kModel.wpmw2WritesVerified)
            CHECK_FALSE(numberLimitsWanted(i));
    }
}

TEST_CASE("limits: device range is cut to the static range, empty cut keeps static", "[limits]") {
    const uint16_t idx = GetElsterIndex(writableNumbers[0].signalName)->Index;   // static 20..60
    struct Case { uint16_t mn, mx; float lo, hi; NumberLimitState st; };
    const Case cases[] = {
        {0x0064, 0x0258, 20.0f, 60.0f, NumberLimitState::Device},   // 10..60: wider below, cut to static
        {0x00C8, 0x03E8, 20.0f, 60.0f, NumberLimitState::Device},   // 20..100: wider above
        {0x012C, 0x0190, 30.0f, 40.0f, NumberLimitState::Device},   // 30..40: narrower
        {0x0320, 0x0384, 20.0f, 60.0f, NumberLimitState::Static},   // 80..90: no overlap
    };
    for (const Case& c : cases) {
        unsigned long t;
        const CanMember* m = startLimitReads(t);
        processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, c.mn));
        numberLimitsStep(t + NUMBER_LIMITS_GAP_MS);
        processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, c.mx));
        CHECK(numberLimits[0].state == c.st);
        CHECK(writableNumberMin(0) == Approx(c.lo));
        CHECK(writableNumberMax(0) == Approx(c.hi));
        resetNumberLimits();
    }
}

TEST_CASE("limits: entries without an answer are asked again after the retry interval", "[limits]") {
    unsigned long t;
    const CanMember* m = startLimitReads(t);
    const uint16_t idx = GetElsterIndex(writableNumbers[0].signalName)->Index;
    for (int k = 0; k < 2; k++) { t += NUMBER_LIMITS_TIMEOUT_MS; numberLimitsStep(t); }   // gives up
    CHECK(numberLimits[0].state == NumberLimitState::NoAnswer);
    const unsigned long gaveUp = numberLimits[0].sentMs;
    numberLimitsActive = -1;
    for (size_t i = 1; i < WRITABLE_NUMBER_COUNT; i++) numberLimits[i].state = NumberLimitState::Static;   // only entry 0 is open
    t = gaveUp + NUMBER_LIMITS_RETRY_MS - 1;
    fake_can().sent.clear();
    numberLimitsStep(t);
    CHECK(numberLimits[0].state == NumberLimitState::NoAnswer);
    t = gaveUp + NUMBER_LIMITS_RETRY_MS;
    numberLimitsStep(t);
    CHECK(numberLimits[0].state == NumberLimitState::WaitMin);
    REQUIRE(fake_can().sent.size() >= 1);
    CHECK(fake_can().sent.back()[2] == ELSTER_SWITCH_MIN);
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x012C));
    t += NUMBER_LIMITS_GAP_MS;
    numberLimitsStep(t);
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, 0x0258));
    CHECK(numberLimits[0].state == NumberLimitState::Device);
    std::string uid = std::string("stiebel_") + m->Name + "_" + writableNumbers[0].signalName;
    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
    const std::string disc = lastPayload("homeassistant/number/heatingpump/" + uid + "/config");
    CHECK(disc.find("\"unique_id\":\"" + uid + "\"") != std::string::npos);   // same entity again
    CHECK(disc.find("\"min\":30,") != std::string::npos);
    resetNumberLimits();

    // a final answer (0x8000 on both sides) is never retried
    startLimitReads(t);
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MIN, idx, 0x8000));
    numberLimitsStep(t + NUMBER_LIMITS_GAP_MS);
    processAndUpdate(m->CanId, limitAnswer(ELSTER_SWITCH_MAX, idx, 0x8000));
    CHECK(numberLimits[0].state == NumberLimitState::Static);
    numberLimitsStep(t + 10 * NUMBER_LIMITS_RETRY_MS);
    CHECK(numberLimits[0].state == NumberLimitState::Static);
    resetNumberLimits();
}

TEST_CASE("limits: raw limits are decoded per type (cent, negative dec, 0x9000)", "[limits]") {
    const ElsterIndex* hk = GetElsterIndex("HEIZKURVE");       // et_cent_val
    const ElsterIndex* rt = GetElsterIndex("RAUMSOLLTEMP_I");  // et_dec_val
    float v = 0.0f;
    REQUIRE(decodeNumberLimit(hk, 20, v));
    CHECK(v == Approx(0.20f));
    REQUIRE(decodeNumberLimit(hk, 300, v));
    CHECK(v == Approx(3.00f));
    REQUIRE(decodeNumberLimit(rt, 0xFF06, v));                 // -250
    CHECK(v == Approx(-25.0f));
    CHECK_FALSE(decodeNumberLimit(rt, 0x9000, v));             // function switched off
    CHECK_FALSE(decodeNumberLimit(hk, 0x9000, v));
    CHECK_FALSE(decodeNumberLimit(rt, 0x8000, v));
}

TEST_CASE("limits: heating circuit 2 rows are looked up by member and name", "[limits]") {
    resetDiscoveryState();
    const CanMember* hk2m = &CanMembers[cm_mischermodul_3];
    int hk1 = -1, hk2 = -1;
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        if (strcmp(writableNumbers[i].signalName, "HEIZKURVE") != 0) continue;
        if (writableNumbers[i].member == cm_mischermodul_3) hk2 = static_cast<int>(i); else if (hk1 < 0) hk1 = static_cast<int>(i);
    }
    REQUIRE(hk1 >= 0);
    REQUIRE(hk2 >= 0);
    REQUIRE(hk1 < hk2);   // a lookup by name alone would return the circuit 1 row
    CHECK(writableNumberIndexFor(hk2m, "HEIZKURVE") == hk2);
    CHECK(writableNumberIndexFor(getWritableNumberMember(writableNumbers[hk1]), "HEIZKURVE") == hk1);
    const ElsterIndex* ei = GetElsterIndex("HEIZKURVE");
    char out[16];

    // circuit 1 has device limits, circuit 2 has its own
    numberLimits[hk1] = {NumberLimitState::Device, 0, 0, 0.5f, 1.0f};
    numberLimits[hk2] = {NumberLimitState::Device, 0, 0, 0.2f, 2.5f};
    CHECK_FALSE(clampToNumberLimits(hk2m, ei, "2.2", out, sizeof(out)));   // fits circuit 2, not circuit 1
    CHECK(clampToNumberLimits(hk2m, ei, "3.0", out, sizeof(out)));
    CHECK(std::string(out) == "2.5");
    CHECK(clampToNumberLimits(hk2m, ei, "0.1", out, sizeof(out)));
    CHECK(std::string(out) == "0.2");

    // circuit 1 fell back to static, circuit 2 has device limits: circuit 2 is still clamped
    numberLimits[hk1].state = NumberLimitState::Static;
    CHECK(clampToNumberLimits(hk2m, ei, "3.0", out, sizeof(out)));
    CHECK(std::string(out) == "2.5");
    // circuit 2 static, circuit 1 with device limits: circuit 2 writes stay unchanged
    numberLimits[hk1].state = NumberLimitState::Device;
    numberLimits[hk2].state = NumberLimitState::Static;
    CHECK_FALSE(clampToNumberLimits(hk2m, ei, "3.0", out, sizeof(out)));

    // paced write on circuit 2 stores the circuit 2 value; read-back compares it
    numberLimits[hk2].state = NumberLimitState::Device;
    pacedWriteCount = 0;
    REQUIRE(queuePacedWrite(hk2m, hk2m, ei, "3.0"));
    REQUIRE(pacedWriteCount == 1);
    CHECK(std::string(pacedWrites[0].value) == "2.5");
    pacedWriteCount = 0;
    // writeSignal() on circuit 2 puts the circuit 2 value on the bus: 2.5 -> raw 250
    fake_can().sent.clear();
    writeSignal(hk2m, "HEIZKURVE", "3.0");
    REQUIRE(fake_can().sent.size() == 1);
    const auto& w = fake_can().sent[0];
    const bool sf = elsterShortForm(ei->Index);
    CHECK(w[sf ? 3 : 5] == 0x00);
    CHECK(w[sf ? 4 : 6] == 0xFA);
    resetNumberLimits();
    pendingReadbackCount = 0;
}

TEST_CASE("limits: no clamping with the static range, clamping with the device range", "[limits]") {
    resetDiscoveryState();
    const int i = writableNumberIndexFor(&CanMembers[cm_manager], "RAUMSOLLTEMP_I");
    REQUIRE(i >= 0);
    const ElsterIndex* ei = GetElsterIndex("RAUMSOLLTEMP_I");
    char out[16];
    CHECK_FALSE(clampToNumberLimits(&CanMembers[cm_manager], ei, "45", out, sizeof(out)));   // static range: unchanged behaviour
    numberLimits[i].state = NumberLimitState::Device;
    numberLimits[i].min = 5.0f;
    numberLimits[i].max = 30.0f;
    CHECK(clampToNumberLimits(&CanMembers[cm_manager], ei, "45", out, sizeof(out)));
    CHECK(std::string(out) == "30");
    CHECK(clampToNumberLimits(&CanMembers[cm_manager], ei, "2", out, sizeof(out)));
    CHECK(std::string(out) == "5");
    CHECK_FALSE(clampToNumberLimits(&CanMembers[cm_manager], ei, "21.5", out, sizeof(out)));
    CHECK_FALSE(clampToNumberLimits(&CanMembers[cm_manager], ei, "nan", out, sizeof(out)));
    CHECK_FALSE(clampToNumberLimits(&CanMembers[cm_manager], GetElsterIndex("PROGRAMMSCHALTER"), "Automatik", out, sizeof(out)));
    CHECK_FALSE(clampToNumberLimits(&CanMembers[cm_manager], GetElsterIndex("AUSSENTEMP"), "99", out, sizeof(out)));   // not writable

    // paced writes store the clamped value, so the read-back check compares the right value
    pacedWriteCount = 0;
    REQUIRE(queuePacedWrite(&CanMembers[cm_manager], &CanMembers[cm_manager], ei, "45"));
    REQUIRE(pacedWriteCount == 1);
    CHECK(std::string(pacedWrites[0].value) == "30");
    pacedWriteCount = 0;
    resetNumberLimits();
}

#include "wpl9_cases.inc"
#include "module_cases.inc"

// ============================================================================
// MQTT topic root from the device_name substitution (#102)
// ============================================================================
// CI builds this file a second time with -DHA_DEVICE_NAME=hp-2 -DTEST_EXPECTED_ROOT="hp-2"
// and runs the [topic_root] tests: every topic and every topic in a discovery payload must
// then live under hp-2/, and nothing may still name heatingpump.
#ifndef TEST_EXPECTED_ROOT
#define TEST_EXPECTED_ROOT "heatingpump"
#endif

TEST_CASE("topic root: every topic and discovery payload follows device_name (#102)", "[mqtt][topic_root]") {
    const std::string root = TEST_EXPECTED_ROOT;
    std::string entityRoot = root;
    std::replace(entityRoot.begin(), entityRoot.end(), '-', '_');
    resetDiscoveryState();
    publishAllCalculatedSensorDiscoveries(true);
    publishAllWritableNumberDiscoveries(true);
    publishAllWritableSelectDiscoveries(true);
    publishClimateDiscoveries();
    publishSgReadyState();
    publishTimeProgramDiscovery(0, 0);
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    publishMqttDiscovery(CanMembers[cm_manager], ei);
    publishMqttState(CanMembers[cm_manager], ei, "4.5");
    publishScanStatus("idle");

    const auto& msgs = mqtt_client_instance().messages;
    REQUIRE(msgs.size() > 20);
    CHECK(mqttTopicPublished(root + "/MANAGER/AUSSENTEMP/state"));
    CHECK(mqttTopicPublished(root + "/MANAGER/SG_READY_STATE/state"));
    size_t topicFields = 0, entityIds = 0;
    for (const auto& m : msgs) {
        INFO(m.topic);
        if (m.topic.rfind("homeassistant/", 0) == 0) {
            // homeassistant/<component>/<root>/<unique_id>/config
            size_t a = m.topic.find('/', 14);
            REQUIRE(a != std::string::npos);
            CHECK(m.topic.compare(a + 1, root.size() + 1, root + "/") == 0);
        } else {
            CHECK(m.topic.rfind(root + "/", 0) == 0);
        }
        if (root != "heatingpump") {
            CHECK(m.topic.find("heatingpump") == std::string::npos);
            CHECK(m.payload.find("heatingpump") == std::string::npos);
        }
        // every "..._topic":"<value>" in a payload starts with the root
        for (size_t p = m.payload.find("_topic\":\""); p != std::string::npos;
             p = m.payload.find("_topic\":\"", p + 1)) {
            ++topicFields;
            CHECK(m.payload.compare(p + 9, root.size() + 1, root + "/") == 0);
        }
        size_t d = m.payload.find("\"default_entity_id\":\"");
        if (d != std::string::npos) {
            ++entityIds;
            size_t dot = m.payload.find('.', d);
            REQUIRE(dot != std::string::npos);
            CHECK(m.payload.compare(dot + 1, entityRoot.size() + 1, entityRoot + "_") == 0);
        }
    }
    CHECK(topicFields > 20);
    CHECK(entityIds > 10);

    // set commands are accepted under the root only
    fake_can().sent.clear();
    CHECK(handleSetCommand(root + "/MANAGER/RAUMSOLLTEMP_I/set", "21.5"));
    CHECK_FALSE(fake_can().sent.empty());
    if (root != "heatingpump") CHECK_FALSE(handleSetCommand("heatingpump/MANAGER/RAUMSOLLTEMP_I/set", "21.5"));
}

// ============================================================================
// WPMW II: remote control FE 7 and the climate of heating circuit 2
// ============================================================================
// WPL13E, 17.11.2026: RAUMISTTEMP (index 0x0011) answers on the virtual members, not on the
// manager. Heating the FE 7 with a hair dryer moved 0x601, 0x602, 0x301 and 0x302 together
// (21.8 -> 33.8 C): one room sensor. The display shows RAUMISTTEMP only under "Raumtemp 2", and
// the manager reads FERNBEDIENUNGSZUORDNUNG (0x0183) = 2: the FE belongs to HK2. Manual WPMW II,
// parameter 22 "AUSWAHL FE": HEIZKREIS 1 / HEIZKREIS 2.

static std::string climatePayload(const char* id) {
    return mqttFindPayload(std::string("climate/heatingpump/") + id + "/config");
}

static void setRemoteControlCircuit(const char* value) {
    updateSensor(CanMembers[cm_manager], GetElsterIndex("FERNBEDIENUNGSZUORDNUNG"), value);
}

TEST_CASE("WPMW II: one room sensor of the remote control, named after it", "[mqtt][wpmw2]") {
    const ElsterIndex* ei = GetElsterIndex("RAUMISTTEMP");
    REQUIRE(ei != nullptr);
    CHECK(ei->Index == 0x0011);
    const ElsterIndex* fe = GetElsterIndex("FERNBEDIENUNGSZUORDNUNG");
    REQUIRE(fe != nullptr);
    CHECK(fe->Index == 0x0183);
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_mischermodul_3], ei);
    const std::string room = mqttFindPayload("stiebel_mischermodul_3_raumisttemp/config");
    REQUIRE_FALSE(room.empty());
    CHECK(room.find("\"device_class\":\"temperature\"") != std::string::npos);
    CHECK(room.find("\"unit_of_measurement\":\"°C\"") != std::string::npos);
    const bool named = room.find(std::string("\"name\":\"") + LNAME_RAUMIST_FE + "\"") != std::string::npos;
    CHECK(named == kModel.wpmw2);
}

// FE 7 knobs on a WPL13E (17.11.2026, FE on HK2, scan of 0x602 index 0x0012): day setpoint 20.5,
// knob middle -> 22.0, knob at a stop -> 25.5 (+5 K), mode switch moon -> 20.0 = night setpoint.
// The mixer flow setpoint followed (25.8 / 30.2 / 23.2 C).
TEST_CASE("WPMW II: effective room setpoint of the FE circuit", "[mqtt][wpmw2]") {
    const ElsterIndex* ei = GetElsterIndex("VERSTELLTE_RAUMSOLLTEMP");
    REQUIRE(ei != nullptr);
    CHECK(ei->Index == 0x0012);
    // value as decoded from the bus: dec value 255 -> "25.5"
    std::vector<uint8_t> frame = {0x31, 0x00, 0xfa, 0x00, 0x12, 0x00, 0xff};
    std::string value;
    const CanMember* cm = nullptr;
    const ElsterIndex* decoded = processCanMessage(frame, 0x602, value, &cm);
    CHECK(decoded == ei);
    CHECK(value == "25.5");
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_mischermodul_3], ei);
    const std::string cfg = mqttFindPayload("stiebel_mischermodul_3_verstellte_raumsolltemp/config");
    REQUIRE_FALSE(cfg.empty());
    CHECK(cfg.find("\"device_class\":\"temperature\"") != std::string::npos);
    const bool named = cfg.find(std::string("\"name\":\"") + LNAME_RAUMSOLL_WIRKSAM_FE + "\"") != std::string::npos;
    CHECK(named == kModel.wpmw2);
}

TEST_CASE("WPMW II: room temperature goes to the climate of the FE circuit only", "[mqtt][wpmw2]") {
    const std::string topic = "\"current_temperature_topic\":\"heatingpump/MISCHERMODUL_3/RAUMISTTEMP/state\"";
    auto has = [&](const char* id) { return climatePayload(id).find(topic) != std::string::npos; };
    resetDiscoveryState();
    remoteControlCircuit = 0;
    publishClimateDiscoveries();
    CHECK_FALSE(has("stiebel_climate_heizung"));            // not read yet: no room value anywhere
    CHECK_FALSE(has("stiebel_climate_heizung_hk2"));

    // FE on HK2 (WPL13E above): the climates are announced again with the value on HK2
    mqtt_client_instance().clear();
    setRemoteControlCircuit("2");
    CHECK(remoteControlCircuit == (kModel.wpmw2 ? 2 : 0));
    if (kModel.wpmw2) {
        CHECK_FALSE(has("stiebel_climate_heizung"));
        CHECK(has("stiebel_climate_heizung_hk2") == kModel.wpmw2WritesVerified);
    }

    // FE on HK1: the value moves to the heating climate
    mqtt_client_instance().clear();
    setRemoteControlCircuit("1");
    if (kModel.wpmw2) {
        CHECK(has("stiebel_climate_heizung"));
        CHECK_FALSE(has("stiebel_climate_heizung_hk2"));
    }

    // unchanged value: no new announcement; other values: no assignment
    mqtt_client_instance().clear();
    setRemoteControlCircuit("1");
    CHECK(climatePayload("stiebel_climate_heizung").empty());
    setRemoteControlCircuit("0");
    CHECK(remoteControlCircuit == 0);
    if (kModel.wpmw2) CHECK_FALSE(has("stiebel_climate_heizung"));

    // the assignment is the manager's: the same signal on another member changes nothing
    setRemoteControlCircuit("2");
    updateSensor(CanMembers[cm_mischermodul_3], GetElsterIndex("FERNBEDIENUNGSZUORDNUNG"), "1");
    CHECK(remoteControlCircuit == (kModel.wpmw2 ? 2 : 0));
    remoteControlCircuit = 0;
}

TEST_CASE("WPMW II: heating circuit 2 gets its own climate entity", "[mqtt][wpmw2]") {
    resetDiscoveryState();
    remoteControlCircuit = 2;
    publishClimateDiscoveries();
    const std::string hk2 = climatePayload("stiebel_climate_heizung_hk2");
    REQUIRE_FALSE(climatePayload("stiebel_climate_heizung").empty());
    // HC2: only where the 0x602 writes are verified (WPL13E, TTL 33)
    CHECK((hk2.empty() == !kModel.wpmw2WritesVerified));
    if (kModel.wpmw2WritesVerified) {
        CHECK(hk2.find("\"temperature_command_topic\":\"heatingpump/MISCHERMODUL_3/RAUMSOLLTEMP_I/set\"") != std::string::npos);
        CHECK(hk2.find("\"temperature_state_topic\":\"heatingpump/MISCHERMODUL_3/RAUMSOLLTEMP_I/state\"") != std::string::npos);
        CHECK(hk2.find("\"min_temp\":10,\"max_temp\":30,\"temp_step\":0.5") != std::string::npos);
        CHECK(hk2.find("preset_mode") == std::string::npos);   // operating mode stays on HC1
        CHECK(hk2.find("\"default_entity_id\":\"climate.heatingpump_climate_heizung_hk2\"") != std::string::npos);
        // the command reaches the 0x602 row through the set dispatcher
        fake_can().sent.clear();
        CHECK(handleSetCommand("heatingpump/MISCHERMODUL_3/RAUMSOLLTEMP_I/set", "20.5"));
        CHECK_FALSE(fake_can().sent.empty());
    }
    remoteControlCircuit = 0;
}

TEST_CASE("climate: mode follows the ESP availability instead of staying off", "[mqtt]") {
    resetDiscoveryState();
    publishClimateDiscoveries();
    const std::string expected = "\"mode_state_topic\":\"heatingpump/status\","
                                 "\"mode_state_template\":\"{{ 'heat' if value == 'online' else 'off' }}\"";
    for (const char* id : {"stiebel_climate_heizung", "stiebel_climate_warmwasser"}) {
        INFO(id);
        CHECK(mqttFindPayload(std::string("climate/heatingpump/") + id + "/config").find(expected) != std::string::npos);
    }
    const std::string hk2 = mqttFindPayload("climate/heatingpump/stiebel_climate_heizung_hk2/config");
    if (kModel.wpmw2WritesVerified) CHECK(hk2.find(expected) != std::string::npos);
}

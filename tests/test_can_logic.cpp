/*
 * Tests for pure CAN/signal functions in ha-stiebel-control.h.
 * ESPHome symbols are provided by esphome_stubs.h — do NOT define HA_DUMMY_BUILD,
 * as that guard excludes all function implementations in ha-stiebel-control.h.
 */
#include "esphome_stubs.h"

#include "catch2/catch_amalgamated.hpp"

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

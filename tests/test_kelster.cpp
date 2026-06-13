#include "catch2/catch_amalgamated.hpp"
#include "../esphome/ha-stiebel-control/elster/NTypes.h"
#include "../esphome/ha-stiebel-control/elster/NUtils.h"
#include "../esphome/ha-stiebel-control/elster/ElsterTable.h"
#include "../esphome/ha-stiebel-control/elster/KElsterTable.h"

// ============================================================================
// SetValueType
// ============================================================================
TEST_CASE("SetValueType: 0x8000 sentinel returns -255", "[kelster]") {
    char val[32];
    SetValueType(val, et_dec_val, 0x8000);
    CHECK(std::string(val) == "-255");
}
TEST_CASE("SetValueType: et_dec_val formats with 1 decimal", "[kelster]") {
    char val[32];
    SetValueType(val, et_dec_val, 0x00C8); // 200 raw = 20.0°C
    CHECK(std::string(val) == "20.0");
}
TEST_CASE("SetValueType: et_dec_val negative value", "[kelster]") {
    char val[32];
    // -5.0 → signed short -50 → cast to unsigned short = 0xFFCE
    SetValueType(val, et_dec_val, (unsigned short)(short)(-50));
    CHECK(std::string(val) == "-5.0");
}
TEST_CASE("SetValueType: et_cent_val formats with 2 decimals", "[kelster]") {
    char val[32];
    SetValueType(val, et_cent_val, 314); // 3.14
    CHECK(std::string(val) == "3.14");
}
TEST_CASE("SetValueType: et_mil_val formats with 3 decimals", "[kelster]") {
    char val[32];
    SetValueType(val, et_mil_val, 1000); // 1.000
    CHECK(std::string(val) == "1.000");
}
TEST_CASE("SetValueType: et_byte signed", "[kelster]") {
    char val[32];
    SetValueType(val, et_byte, 255); // signed char 255 = -1
    CHECK(std::string(val) == "-1");
}
TEST_CASE("SetValueType: et_bool on", "[kelster]") {
    char val[32];
    SetValueType(val, et_bool, 1);
    CHECK(std::string(val) == "on");
}
TEST_CASE("SetValueType: et_bool off", "[kelster]") {
    char val[32];
    SetValueType(val, et_bool, 0);
    CHECK(std::string(val) == "off");
}
TEST_CASE("SetValueType: et_bool unknown", "[kelster]") {
    char val[32];
    SetValueType(val, et_bool, 42);
    CHECK(std::string(val) == "?");
}
TEST_CASE("SetValueType: et_little_bool on", "[kelster]") {
    char val[32];
    SetValueType(val, et_little_bool, 0x0100);
    CHECK(std::string(val) == "on");
}
TEST_CASE("SetValueType: et_little_bool off", "[kelster]") {
    char val[32];
    SetValueType(val, et_little_bool, 0);
    CHECK(std::string(val) == "off");
}
TEST_CASE("SetValueType: et_zeit formats HH:MM", "[kelster]") {
    char val[32];
    // Value = (hour) | (minute << 8) — hour in low byte, minute in high byte
    SetValueType(val, et_zeit, (unsigned short)(14 | (30 << 8)));
    CHECK(std::string(val) == "14:30");
}
TEST_CASE("SetValueType: et_datum formats DD.MM.", "[kelster]") {
    char val[32];
    // Value = (day << 8) | month
    SetValueType(val, et_datum, (unsigned short)((15 << 8) | 6));
    CHECK(std::string(val) == "15.06.");
}
TEST_CASE("SetValueType: et_dev_nr below 0x80 adds 1", "[kelster]") {
    char val[32];
    SetValueType(val, et_dev_nr, 0);
    CHECK(std::string(val) == "1");
}
TEST_CASE("SetValueType: et_dev_nr >= 0x80 returns --", "[kelster]") {
    char val[32];
    SetValueType(val, et_dev_nr, 0x80);
    CHECK(std::string(val) == "--");
}
TEST_CASE("SetValueType: et_dev_id formats N-NN", "[kelster]") {
    char val[32];
    SetValueType(val, et_dev_id, (unsigned short)((2 << 8) | 5));
    CHECK(std::string(val) == "2-05");
}
TEST_CASE("SetValueType: et_default formats as signed int", "[kelster]") {
    char val[32];
    SetValueType(val, et_default, (unsigned short)(short)(-1));
    CHECK(std::string(val) == "-1");
}
TEST_CASE("SetValueType: et_little_endian swaps bytes", "[kelster]") {
    char val[32];
    // little endian: result = (value >> 8) + 256*(value & 0xff)
    // value = 0x0102 → (0x01) + 256*(0x02) = 1 + 512 = 513
    SetValueType(val, et_little_endian, 0x0102);
    CHECK(std::string(val) == "513");
}

// ============================================================================
// SetDoubleType
// ============================================================================
TEST_CASE("SetDoubleType: et_double_val 3 decimals", "[kelster]") {
    char val[32];
    SetDoubleType(val, et_double_val, 3.141);
    CHECK(std::string(val) == "3.141");
}
TEST_CASE("SetDoubleType: et_triple_val 6 decimals", "[kelster]") {
    char val[32];
    SetDoubleType(val, et_triple_val, 1.234567);
    // sprintf %g may trim trailing zeros — just check prefix
    std::string s(val);
    CHECK(s.find("1.234567") != std::string::npos);
}
TEST_CASE("SetDoubleType: default type uses %g", "[kelster]") {
    char val[32];
    SetDoubleType(val, et_default, 100.0);
    CHECK(std::string(val) == "100");
}

// ============================================================================
// GetElsterType
// ============================================================================
TEST_CASE("GetElsterType: known type name round-trips", "[kelster]") {
    CHECK(GetElsterType("et_dec_val")  == et_dec_val);
    CHECK(GetElsterType("et_bool")     == et_bool);
    CHECK(GetElsterType("et_betriebsart") == et_betriebsart);
}
TEST_CASE("GetElsterType: unknown name returns et_default", "[kelster]") {
    CHECK(GetElsterType("garbage") == et_default);
    CHECK(GetElsterType(nullptr)   == et_default);
}

// ============================================================================
// ElsterTypeToName
// ============================================================================
TEST_CASE("ElsterTypeToName: known index returns name", "[kelster]") {
    CHECK(std::string(ElsterTypeToName(et_dec_val)) == "et_dec_val");
    CHECK(std::string(ElsterTypeToName(et_bool))    == "et_bool");
}
TEST_CASE("ElsterTypeToName: out-of-range returns et_default string", "[kelster]") {
    CHECK(std::string(ElsterTypeToName(999)) == "et_default");
}

// ============================================================================
// GetElsterIndex by name
// ============================================================================
TEST_CASE("GetElsterIndex by name: known signal found", "[kelster]") {
    const ElsterIndex* ei = GetElsterIndex("HYSTERESEZEIT");
    CHECK(std::string(ei->Name) == "HYSTERESEZEIT");
}
TEST_CASE("GetElsterIndex by name: unknown returns INDEX_NOT_FOUND sentinel", "[kelster]") {
    const ElsterIndex* ei = GetElsterIndex("DOES_NOT_EXIST");
    CHECK(std::string(ei->Name) == "INDEX_NOT_FOUND");
}
TEST_CASE("GetElsterIndex by name: cache hit returns same pointer", "[kelster]") {
    const ElsterIndex* a = GetElsterIndex("HYSTERESEZEIT");
    const ElsterIndex* b = GetElsterIndex("HYSTERESEZEIT");
    CHECK(a == b);
}

// ============================================================================
// GetElsterIndex by index
// ============================================================================
TEST_CASE("GetElsterIndex by index: round-trip via HYSTERESEZEIT", "[kelster]") {
    const ElsterIndex* byName = GetElsterIndex("HYSTERESEZEIT");
    const ElsterIndex* byIndex = GetElsterIndex(byName->Index);
    // Both are from the same TU's KElsterTable.cpp, so pointer equality holds here
    CHECK(byIndex == byName);
}
TEST_CASE("GetElsterIndex by index: unknown index returns INDEX_NOT_FOUND sentinel", "[kelster]") {
    const ElsterIndex* ei = GetElsterIndex((unsigned short)0xDEAD);
    CHECK(std::string(ei->Name) == "INDEX_NOT_FOUND");
}

// ============================================================================
// TranslateString — round-trip: SetValueType output → TranslateString input
// ============================================================================
TEST_CASE("TranslateString: et_dec_val round-trip 20.0", "[kelster]") {
    char val[32];
    SetValueType(val, et_dec_val, 200); // 200/10 = 20.0
    const char* p = val;
    int result = TranslateString(p, et_dec_val);
    CHECK(result == 200);
}
TEST_CASE("TranslateString: et_dec_val round-trip negative -5.0", "[kelster]") {
    char val[32];
    unsigned short raw = (unsigned short)(short)(-50); // -5.0
    SetValueType(val, et_dec_val, raw);
    const char* p = val;
    int result = TranslateString(p, et_dec_val);
    CHECK((short)result == -50);
}
TEST_CASE("TranslateString: et_bool on → 1", "[kelster]") {
    const char* s = "on";
    CHECK(TranslateString(s, et_bool) == 1);
}
TEST_CASE("TranslateString: et_bool off → 0", "[kelster]") {
    const char* s = "off";
    CHECK(TranslateString(s, et_bool) == 0);
}
TEST_CASE("TranslateString: et_little_bool on → 0x0100", "[kelster]") {
    const char* s = "on";
    CHECK(TranslateString(s, et_little_bool) == 0x0100);
}
TEST_CASE("TranslateString: et_little_bool off → 0", "[kelster]") {
    const char* s = "off";
    CHECK(TranslateString(s, et_little_bool) == 0);
}
TEST_CASE("TranslateString: et_betriebsart Automatik", "[kelster]") {
    const char* s = "Automatik";
    int result = TranslateString(s, et_betriebsart);
    CHECK(result >= 0); // Valid betriebsart index
}
TEST_CASE("TranslateString: et_default parses integer", "[kelster]") {
    const char* s = "42";
    CHECK(TranslateString(s, et_default) == 42);
}
TEST_CASE("TranslateString: et_default negative", "[kelster]") {
    const char* s = "-1";
    int r = TranslateString(s, et_default);
    CHECK((short)r == -1);
}
TEST_CASE("TranslateString: et_cent_val round-trip 3.14", "[kelster]") {
    // 3.14 * 100 = 314
    const char* s = "3.14";
    CHECK(TranslateString(s, et_cent_val) == 314);
}
TEST_CASE("TranslateString: et_mil_val round-trip 1.000", "[kelster]") {
    const char* s = "1.000";
    CHECK(TranslateString(s, et_mil_val) == 1000);
}
TEST_CASE("TranslateString: et_zeit 14:30", "[kelster]") {
    const char* s = "14:30";
    int r = TranslateString(s, et_zeit);
    // hour in low byte, minute in high byte: (30 << 8) | 14 = 0x1E0E
    CHECK(r == (int)((30 << 8) | 14));
}
TEST_CASE("TranslateString: et_datum 15.06.", "[kelster]") {
    const char* s = "15.06.";
    int r = TranslateString(s, et_datum);
    // (day << 8) | month = (15 << 8) | 6 = 0x0F06
    CHECK(r == (int)((15 << 8) | 6));
}
TEST_CASE("TranslateString: invalid input returns -1", "[kelster]") {
    const char* s = "notanumber";
    CHECK(TranslateString(s, et_dec_val) == -1);
}
TEST_CASE("TranslateString: et_datum invalid day 32 returns -1", "[kelster]") {
    const char* s = "32.01.";
    CHECK(TranslateString(s, et_datum) == -1);
}
TEST_CASE("TranslateString: et_datum Feb 29 returns -1", "[kelster]") {
    const char* s = "29.02.";
    CHECK(TranslateString(s, et_datum) == -1);
}
TEST_CASE("TranslateString: et_zeit 24:00 returns -1 (24:mm only valid when mm==0 but hour<24 required)", "[kelster]") {
    // Code path: Get_Time accepts h==24&&m==0, but TranslateString requires hour<24 to encode.
    // So 24:00 returns -1.
    const char* s = "24:00";
    CHECK(TranslateString(s, et_zeit) == -1);
}
TEST_CASE("TranslateString: et_zeit 24:01 returns -1", "[kelster]") {
    const char* s = "24:01";
    CHECK(TranslateString(s, et_zeit) == -1);
}
TEST_CASE("TranslateString: et_time_domain empty string → 0x8080", "[kelster]") {
    const char* s = "";
    CHECK(TranslateString(s, et_time_domain) == 0x8080);
}
TEST_CASE("TranslateString: et_time_domain valid range", "[kelster]") {
    const char* s = "08:00-16:00";
    int r = TranslateString(s, et_time_domain);
    CHECK(r > 0);
}
TEST_CASE("TranslateString: leading spaces are skipped", "[kelster]") {
    const char* s = "   42";
    CHECK(TranslateString(s, et_default) == 42);
}

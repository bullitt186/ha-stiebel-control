#include "catch2/catch_amalgamated.hpp"
#include "../esphome/ha-stiebel-control/elster/NTypes.h"
#include "../esphome/ha-stiebel-control/elster/NUtils.h"

using namespace NUtils;

// ============================================================================
// GetHexDigit
// ============================================================================
TEST_CASE("GetHexDigit: 0-9 return 0-9", "[nutils]") {
    CHECK(GetHexDigit('0') == 0);
    CHECK(GetHexDigit('9') == 9);
}
TEST_CASE("GetHexDigit: A-F return 10-15", "[nutils]") {
    CHECK(GetHexDigit('A') == 10);
    CHECK(GetHexDigit('F') == 15);
}
TEST_CASE("GetHexDigit: a-f return 10-15", "[nutils]") {
    CHECK(GetHexDigit('a') == 10);
    CHECK(GetHexDigit('f') == 15);
}
TEST_CASE("GetHexDigit: non-hex returns -1", "[nutils]") {
    CHECK(GetHexDigit('g') == -1);
    CHECK(GetHexDigit('z') == -1);
    CHECK(GetHexDigit(' ') == -1);
}

// ============================================================================
// GetHex
// ============================================================================
TEST_CASE("GetHex: parses hex digits, advances pointer", "[nutils]") {
    const char* s = "1A2B";
    CHECK(GetHex(s) == 0x1A2B);
    CHECK(*s == '\0');
}
TEST_CASE("GetHex: stops at non-hex", "[nutils]") {
    const char* s = "FF rest";
    CHECK(GetHex(s) == 0xFF);
    CHECK(*s == ' ');
}
TEST_CASE("GetHex: null pointer returns 0", "[nutils]") {
    const char* s = nullptr;
    CHECK(GetHex(s) == 0);
}
TEST_CASE("GetHex: empty string returns 0", "[nutils]") {
    const char* s = "";
    CHECK(GetHex(s) == 0);
}

// ============================================================================
// GetInt (pointer-advancing overload)
// ============================================================================
TEST_CASE("GetInt: decimal positive", "[nutils]") {
    const char* s = "123";
    TInt64 res;
    CHECK(GetInt(s, res));
    CHECK(res == 123);
    CHECK(*s == '\0');
}
TEST_CASE("GetInt: decimal negative", "[nutils]") {
    const char* s = "-42";
    TInt64 res;
    CHECK(GetInt(s, res));
    CHECK(res == -42);
}
TEST_CASE("GetInt: hex with 0x prefix", "[nutils]") {
    const char* s = "0xFF";
    TInt64 res;
    CHECK(GetInt(s, res));
    CHECK(res == 255);
}
TEST_CASE("GetInt: hex with $ prefix", "[nutils]") {
    const char* s = "$1F";
    TInt64 res;
    CHECK(GetInt(s, res));
    CHECK(res == 31);
}
TEST_CASE("GetInt: empty string returns false", "[nutils]") {
    const char* s = "";
    TInt64 res;
    CHECK_FALSE(GetInt(s, res));
}
TEST_CASE("GetInt: non-numeric returns false", "[nutils]") {
    const char* s = "abc";
    TInt64 res;
    CHECK_FALSE(GetInt(s, res));
}
TEST_CASE("GetInt: null pointer returns false", "[nutils]") {
    const char* s = nullptr;
    TInt64 res;
    CHECK_FALSE(GetInt(s, res));
}
TEST_CASE("GetInt: stops at non-digit", "[nutils]") {
    const char* s = "10:30";
    TInt64 res;
    CHECK(GetInt(s, res));
    CHECK(res == 10);
    CHECK(*s == ':');
}

// ============================================================================
// GetInt (length-returning overload)
// ============================================================================
TEST_CASE("GetInt length overload: returns chars consumed", "[nutils]") {
    TInt64 res;
    CHECK(GetInt(res, "42") == 2);
    CHECK(res == 42);
}
TEST_CASE("GetInt length overload: null returns 0", "[nutils]") {
    TInt64 res;
    CHECK(GetInt(res, nullptr) == 0);
}

// ============================================================================
// GetDouble
// ============================================================================
TEST_CASE("GetDouble: integer value", "[nutils]") {
    const char* s = "42";
    double res;
    CHECK(GetDouble(s, res));
    CHECK(res == Catch::Approx(42.0));
}
TEST_CASE("GetDouble: decimal value", "[nutils]") {
    const char* s = "3.14";
    double res;
    CHECK(GetDouble(s, res));
    CHECK(res == Catch::Approx(3.14));
}
TEST_CASE("GetDouble: negative value", "[nutils]") {
    const char* s = "-1.5";
    double res;
    CHECK(GetDouble(s, res));
    CHECK(res == Catch::Approx(-1.5));
}
TEST_CASE("GetDouble: non-numeric returns false", "[nutils]") {
    const char* s = "abc";
    double res;
    CHECK_FALSE(GetDouble(s, res));
}
TEST_CASE("GetDouble: dot without trailing digit returns false", "[nutils]") {
    const char* s = "1.";
    double res;
    // After the dot there's no digit — should fail
    CHECK_FALSE(GetDouble(s, res));
}
TEST_CASE("GetDouble: zero", "[nutils]") {
    const char* s = "0.0";
    double res;
    CHECK(GetDouble(s, res));
    CHECK(res == Catch::Approx(0.0));
}

#include "catch2/catch_amalgamated.hpp"
#include "../esphome/ha-stiebel-control/sg_ready_controller.h"

#include <vector>
#include <string>

using Catch::Approx;

// ============================================================================
// TEST DOUBLE — records all side-effects for assertions
// ============================================================================

struct RecordedCall {
    std::string type;   // "can", "mqtt", "save", "load"
    std::string key;
    std::string value;
};

struct FakeIO : ISgReadyIO {
    std::vector<RecordedCall> calls;

    // NVS state for load/save
    float  savedDhw    = 0.0f;
    float  savedRoom   = 0.0f;
    bool   savedActive = false;
    bool   hasData     = false;

    void writeCanSignal(const char* signal, const char* value) override {
        calls.push_back({"can", signal, value});
    }

    void publishMqtt(const char* topic, const char* value) override {
        calls.push_back({"mqtt", topic, value});
    }

    void saveBoostState(float dhw, float room, bool active) override {
        savedDhw    = dhw;
        savedRoom   = room;
        savedActive = active;
        hasData     = true;
        calls.push_back({"save", "", active ? "1" : "0"});
    }

    bool loadBoostState(float& dhw, float& room, bool& active) override {
        if (!hasData) return false;
        dhw    = savedDhw;
        room   = savedRoom;
        active = savedActive;
        return true;
    }

    // Current device setpoints (0.0 = unknown) for captureOriginals tests
    float currentDhw  = 0.0f;
    float currentRoom = 0.0f;
    bool  hasCurrent  = false;

    bool readCurrentSetpoints(float& dhw, float& room) override {
        dhw  = currentDhw;
        room = currentRoom;
        return hasCurrent;
    }

    // Helpers for assertions
    bool hadCan(const std::string& signal, const std::string& value = "") const {
        for (auto& c : calls)
            if (c.type == "can" && c.key == signal && (value.empty() || c.value == value))
                return true;
        return false;
    }

    bool hadMqtt(const std::string& value) const {
        for (auto& c : calls)
            if (c.type == "mqtt" && c.value == value)
                return true;
        return false;
    }

    int countCan(const std::string& signal) const {
        int n = 0;
        for (auto& c : calls)
            if (c.type == "can" && c.key == signal) n++;
        return n;
    }

    std::string lastCan(const std::string& signal) const {
        std::string last;
        for (auto& c : calls)
            if (c.type == "can" && c.key == signal)
                last = c.value;
        return last;
    }

    void reset() { calls.clear(); }
};

// ============================================================================
// TESTS
// ============================================================================

TEST_CASE("parseStateString", "[sg_ready]") {
    CHECK(SgReadyController::parseStateString("1 - EVU Sperre") == 1);
    CHECK(SgReadyController::parseStateString("2 - Normal")     == 2);
    CHECK(SgReadyController::parseStateString("3 - Empfohlen")  == 3);
    CHECK(SgReadyController::parseStateString("4 - Zwang")      == 4);
    CHECK(SgReadyController::parseStateString("garbage")        == 0);
    CHECK(SgReadyController::parseStateString("")               == 0);
}

TEST_CASE("stateLabel", "[sg_ready]") {
    CHECK(std::string(SgReadyController::stateLabel(1)) == "1 - EVU Sperre");
    CHECK(std::string(SgReadyController::stateLabel(2)) == "2 - Normal");
    CHECK(std::string(SgReadyController::stateLabel(3)) == "3 - Empfohlen");
    CHECK(std::string(SgReadyController::stateLabel(4)) == "4 - Zwang");
    CHECK(std::string(SgReadyController::stateLabel(0)) == "Unbekannt");
}

TEST_CASE("applyState rejects invalid states", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    CHECK(ctrl.applyState(0)  == false);
    CHECK(ctrl.applyState(5)  == false);
    CHECK(ctrl.applyState(-1) == false);
    CHECK(io.calls.empty());
}

TEST_CASE("state 1 (EVU Lock): sets Bereitschaft, does not restore if not active", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    ctrl.applyState(SgReadyController::STATE_EVU_LOCK);

    CHECK(ctrl.currentState() == 1);
    CHECK(ctrl.isActive() == true);
    CHECK(io.hadCan("PROGRAMMSCHALTER", "Bereitschaft"));
    // No restore calls because sgReadyActive was false and originalDhw/Room are 0
    CHECK(io.countCan("EINSTELL_SPEICHERSOLLTEMP") == 0);
    CHECK(io.countCan("RAUMSOLLTEMP_I") == 0);
    CHECK(io.hadMqtt("1 - EVU Sperre"));
}

TEST_CASE("state 2 (Normal): no-op when not active", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    ctrl.applyState(SgReadyController::STATE_NORMAL);

    CHECK(ctrl.isActive() == false);
    // With no active boost, should not write any CAN signals
    CHECK(io.countCan("PROGRAMMSCHALTER") == 0);
    CHECK(io.hadMqtt("2 - Normal"));
}

TEST_CASE("state 3 (Recommended): boosts DHW and room temp", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(5.0f);

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);

    CHECK(ctrl.isActive() == true);
    CHECK(io.hadCan("PROGRAMMSCHALTER", "Tagbetrieb"));
    // DHW: 48.0 + 5.0 = 53.0
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "53.0");
    // Room: 20.0 + 1.0 = 21.0
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "21.0");
    CHECK(io.hadMqtt("3 - Empfohlen"));
}

TEST_CASE("state 4 (Forced): boosts DHW and room temp more", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState4(8.0f);

    ctrl.applyState(SgReadyController::STATE_FORCED);

    CHECK(ctrl.isActive() == true);
    CHECK(io.hadCan("PROGRAMMSCHALTER", "Tagbetrieb"));
    // DHW: 48.0 + 8.0 = 56.0
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "56.0");
    // Room: 20.0 + 2.0 = 22.0
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "22.0");
    CHECK(io.hadMqtt("4 - Zwang"));
}

TEST_CASE("DHW boost is capped at 60°C", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState4(15.0f);  // 48 + 15 = 63, should be capped at 60

    ctrl.applyState(SgReadyController::STATE_FORCED);

    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "60.0");
}

TEST_CASE("room boost is capped at 25°C", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    // Make original room temp 24.5 — state 4 would push to 26.5, should cap at 25
    // We can't directly set originalRoomTemp without going through NVS; use NVS load
    io.savedDhw    = 48.0f;
    io.savedRoom   = 24.5f;
    io.savedActive = true;
    io.hasData     = true;
    ctrl.loadFromNvs();

    ctrl.applyState(SgReadyController::STATE_FORCED);  // +2.0 room boost

    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "25.0");
}

TEST_CASE("setRoomBoost3/4 configure the room boost offset", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    io.currentDhw = 45.0f;
    io.currentRoom = 21.0f;
    io.hasCurrent = true;

    ctrl.setRoomBoost3(3.0f);
    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "24.0");  // 21.0 + 3.0
    CHECK(ctrl.roomBoost3() == Approx(3.0f));

    io.reset();
    ctrl.setRoomBoost4(4.5f);
    ctrl.applyState(SgReadyController::STATE_FORCED);
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "25.0");  // 21.0 + 4.5 capped at 25.0
    CHECK(ctrl.roomBoost4() == Approx(4.5f));
}

TEST_CASE("room boost can be disabled (0 = no room write)", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    io.currentDhw = 45.0f;
    io.currentRoom = 21.0f;
    io.hasCurrent = true;

    ctrl.setRoomBoost3(0.0f);
    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);
    CHECK(io.countCan("RAUMSOLLTEMP_I") == 0);
}

TEST_CASE("state 3 → state 2: restores original DHW and room temps", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(5.0f);

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);
    io.reset();  // clear calls, keep saved state

    ctrl.applyState(SgReadyController::STATE_NORMAL);

    // Should restore DHW to 48.0
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "48.0");
    // Should restore room to 20.0
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "20.0");
    CHECK(io.hadCan("PROGRAMMSCHALTER", "Tagbetrieb"));
    CHECK(ctrl.isActive() == false);
}

TEST_CASE("captureOriginals uses the device's current setpoints", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(5.0f);

    // Device reports: active DHW setpoint 40.0, room setpoint 21.0
    io.currentDhw = 40.0f;
    io.currentRoom = 21.0f;
    io.hasCurrent = true;

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);

    // Boost is based on the REAL values: 40.0 + 5.0 = 45.0, 21.0 + 1.0 = 22.0
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "45.0");
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "22.0");

    io.reset();
    ctrl.applyState(SgReadyController::STATE_NORMAL);

    // Restore returns to the REAL original values, not the 48/20 defaults
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "40.0");
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "21.0");
}

TEST_CASE("captureOriginals falls back to defaults when setpoints unknown", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(5.0f);

    // Unknown setpoints (never polled) — hasCurrent false, values 0.0
    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);

    // Fallback: DEFAULT_DHW_TEMP 48.0 + 5.0 = 53.0, DEFAULT_ROOM_TEMP 20.0 + 1.0 = 21.0
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "53.0");
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "21.0");
}

TEST_CASE("captureOriginals uses partial setpoints, default for the rest", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(5.0f);

    // Only the room setpoint is known (DHW never polled)
    io.currentDhw = 0.0f;
    io.currentRoom = 22.5f;
    io.hasCurrent = true;

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);

    // DHW: default 48.0 + 5.0 = 53.0; room: real 22.5 + 1.0 = 23.5
    CHECK(io.lastCan("EINSTELL_SPEICHERSOLLTEMP") == "53.0");
    CHECK(io.lastCan("RAUMSOLLTEMP_I") == "23.5");
}

TEST_CASE("state 1 → state 2: restores temps if active", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(5.0f);

    // Activate via state 3 first to set originals
    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);
    io.reset();

    // Then go to state 1 (EVU lock) — should restore what was boosted
    ctrl.applyState(SgReadyController::STATE_EVU_LOCK);
    io.reset();

    // Now back to normal — state was EVU lock, active=true, but originals already cleared
    // EVU lock already restored temps; state 2 should still run Tagbetrieb
    ctrl.applyState(SgReadyController::STATE_NORMAL);
    CHECK(io.hadCan("PROGRAMMSCHALTER", "Tagbetrieb"));
    CHECK(ctrl.isActive() == false);
}

TEST_CASE("NVS load restores active boost state across reboot", "[sg_ready]") {
    FakeIO io;
    io.savedDhw    = 48.0f;
    io.savedRoom   = 20.0f;
    io.savedActive = true;
    io.hasData     = true;

    SgReadyController ctrl(io);
    ctrl.loadFromNvs();

    CHECK(ctrl.isActive()        == true);
    CHECK(ctrl.originalDhwTemp() == Approx(48.0f));
    CHECK(ctrl.originalRoomTemp() == Approx(20.0f));
}

TEST_CASE("NVS load: inactive state is ignored", "[sg_ready]") {
    FakeIO io;
    io.savedDhw    = 48.0f;
    io.savedRoom   = 20.0f;
    io.savedActive = false;
    io.hasData     = true;

    SgReadyController ctrl(io);
    ctrl.loadFromNvs();

    CHECK(ctrl.isActive() == false);
}

TEST_CASE("NVS load: zero temps fall back to defaults", "[sg_ready]") {
    FakeIO io;
    io.savedDhw    = 0.0f;
    io.savedRoom   = 0.0f;
    io.savedActive = true;
    io.hasData     = true;

    SgReadyController ctrl(io);
    ctrl.loadFromNvs();

    CHECK(ctrl.originalDhwTemp()  == Approx(SgReadyController::DEFAULT_DHW_TEMP));
    CHECK(ctrl.originalRoomTemp() == Approx(SgReadyController::DEFAULT_ROOM_TEMP));
}

TEST_CASE("setBoostState3 clamps invalid values", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    ctrl.setBoostState3(-1.0f);
    CHECK(ctrl.boostState3() == Approx(5.0f));  // unchanged

    ctrl.setBoostState3(11.0f);
    CHECK(ctrl.boostState3() == Approx(5.0f));  // unchanged

    ctrl.setBoostState3(3.5f);
    CHECK(ctrl.boostState3() == Approx(3.5f));  // accepted
}

TEST_CASE("setBoostState4 clamps invalid values", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    ctrl.setBoostState4(-0.5f);
    CHECK(ctrl.boostState4() == Approx(8.0f));  // unchanged

    ctrl.setBoostState4(16.0f);
    CHECK(ctrl.boostState4() == Approx(8.0f));  // unchanged

    ctrl.setBoostState4(7.0f);
    CHECK(ctrl.boostState4() == Approx(7.0f));  // accepted
}

TEST_CASE("state 3 with zero boost: skips DHW write", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);
    ctrl.setBoostState3(0.0f);

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);

    CHECK(io.countCan("EINSTELL_SPEICHERSOLLTEMP") == 0);
    CHECK(io.hadCan("PROGRAMMSCHALTER", "Tagbetrieb"));  // mode still set
}

TEST_CASE("boost state is persisted after state 3", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);

    CHECK(io.savedActive == true);
    CHECK(io.savedDhw    == Approx(SgReadyController::DEFAULT_DHW_TEMP));
    CHECK(io.savedRoom   == Approx(SgReadyController::DEFAULT_ROOM_TEMP));
}

TEST_CASE("boost state is cleared after state 2", "[sg_ready]") {
    FakeIO io;
    SgReadyController ctrl(io);

    ctrl.applyState(SgReadyController::STATE_RECOMMENDED);
    ctrl.applyState(SgReadyController::STATE_NORMAL);

    CHECK(io.savedActive == false);
}

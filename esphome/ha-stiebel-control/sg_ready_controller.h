/*
 * SgReadyController — pure business logic for SG Ready state machine.
 *
 * No ESPHome dependencies. All side-effects (CAN writes, MQTT publishes,
 * NVS persistence) are injected via the ISgReadyIO interface so this class
 * can be unit-tested on the host without hardware.
 */

#ifndef SG_READY_CONTROLLER_H
#define SG_READY_CONTROLLER_H

#include <string>
#include <cstring>

// ============================================================================
// IO INTERFACE — injected in production, stubbed in tests
// ============================================================================

class ISgReadyIO {
public:
    virtual ~ISgReadyIO() = default;

    // Write a CAN signal value (e.g. "PROGRAMMSCHALTER" = "Tagbetrieb")
    virtual void writeCanSignal(const char* signalName, const char* value) = 0;

    // Publish a value to an MQTT state topic
    virtual void publishMqtt(const char* topic, const char* value) = 0;

    // Persist boost state to NVS
    virtual void saveBoostState(float dhwTemp, float roomTemp, bool active) = 0;

    // Load boost state from NVS; returns false if nothing stored
    virtual bool loadBoostState(float& dhwTemp, float& roomTemp, bool& active) = 0;
};

// ============================================================================
// CONTROLLER
// ============================================================================

class SgReadyController {
public:
    // SG Ready state constants
    static constexpr int STATE_DISABLED = 0;
    static constexpr int STATE_EVU_LOCK  = 1;
    static constexpr int STATE_NORMAL    = 2;
    static constexpr int STATE_RECOMMENDED = 3;
    static constexpr int STATE_FORCED    = 4;

    // Default setpoint fallbacks (used when originals were never captured)
    static constexpr float DEFAULT_DHW_TEMP  = 48.0f;
    static constexpr float DEFAULT_ROOM_TEMP = 20.0f;

    // Hardcoded room boost offsets per state
    static constexpr float ROOM_BOOST_STATE3 = 1.0f;
    static constexpr float ROOM_BOOST_STATE4 = 2.0f;

    // Temperature ceiling for DHW boost
    static constexpr float MAX_DHW_TEMP  = 60.0f;
    static constexpr float MAX_ROOM_TEMP = 25.0f;

    struct Config {
        float boostState3 = 5.0f;   // DHW boost for state 3 (°C)
        float boostState4 = 8.0f;   // DHW boost for state 4 (°C)
    };

    struct State {
        int   currentState    = STATE_NORMAL;
        bool  active          = false;
        float originalDhwTemp = 0.0f;
        float originalRoomTemp = 0.0f;
    };

    explicit SgReadyController(ISgReadyIO& io) : io_(io) {}

    // Called on boot: restore persisted boost state from NVS
    void loadFromNvs() {
        float dhw = 0.0f, room = 0.0f;
        bool active = false;
        if (io_.loadBoostState(dhw, room, active) && active) {
            state_.originalDhwTemp  = (dhw  > 0.0f) ? dhw  : DEFAULT_DHW_TEMP;
            state_.originalRoomTemp = (room > 0.0f) ? room : DEFAULT_ROOM_TEMP;
            state_.active = true;
        }
    }

    // Apply a new SG Ready state (1–4). Returns false for invalid state.
    bool applyState(int newState) {
        if (newState < STATE_EVU_LOCK || newState > STATE_FORCED) {
            return false;
        }

        state_.currentState = newState;

        // Publish new state to MQTT
        char topic[128];
        snprintf(topic, sizeof(topic), "heatingpump/MANAGER/SG_READY_STATE/state");
        io_.publishMqtt(topic, stateLabel(newState));

        switch (newState) {
            case STATE_EVU_LOCK:   applyEvuLock();    break;
            case STATE_NORMAL:     applyNormal();     break;
            case STATE_RECOMMENDED: applyRecommended(); break;
            case STATE_FORCED:     applyForced();     break;
        }
        return true;
    }

    // Update configurable boost amounts
    void setBoostState3(float boost) {
        if (boost >= 0.0f && boost <= 10.0f) {
            config_.boostState3 = boost;
        }
    }

    void setBoostState4(float boost) {
        if (boost >= 0.0f && boost <= 15.0f) {
            config_.boostState4 = boost;
        }
    }

    // Getters for inspection / sensor publishing
    int   currentState()    const { return state_.currentState; }
    bool  isActive()        const { return state_.active; }
    float boostState3()     const { return config_.boostState3; }
    float boostState4()     const { return config_.boostState4; }
    float originalDhwTemp() const { return state_.originalDhwTemp; }
    float originalRoomTemp() const { return state_.originalRoomTemp; }

    // Human-readable label for a state value
    static const char* stateLabel(int state) {
        switch (state) {
            case STATE_EVU_LOCK:    return "1 - EVU Sperre";
            case STATE_NORMAL:     return "2 - Normal";
            case STATE_RECOMMENDED: return "3 - Empfohlen";
            case STATE_FORCED:     return "4 - Zwang";
            default:               return "Unbekannt";
        }
    }

    static const char* stateLabelFromString(const std::string& s) {
        // Parse "1 - ...", "2 - ...", etc.
        if (s.rfind("1 - ", 0) == 0) return stateLabel(STATE_EVU_LOCK);
        if (s.rfind("2 - ", 0) == 0) return stateLabel(STATE_NORMAL);
        if (s.rfind("3 - ", 0) == 0) return stateLabel(STATE_RECOMMENDED);
        if (s.rfind("4 - ", 0) == 0) return stateLabel(STATE_FORCED);
        return nullptr;
    }

    // Parse "1 - EVU Sperre" → 1, etc. Returns 0 on failure.
    static int parseStateString(const std::string& s) {
        if (s.rfind("1 - ", 0) == 0) return STATE_EVU_LOCK;
        if (s.rfind("2 - ", 0) == 0) return STATE_NORMAL;
        if (s.rfind("3 - ", 0) == 0) return STATE_RECOMMENDED;
        if (s.rfind("4 - ", 0) == 0) return STATE_FORCED;
        return 0;
    }

private:
    ISgReadyIO& io_;
    Config config_;
    State  state_;

    void captureOriginals() {
        if (!state_.active || state_.originalDhwTemp == 0.0f) {
            state_.originalDhwTemp = DEFAULT_DHW_TEMP;
        }
        if (!state_.active || state_.originalRoomTemp == 0.0f) {
            state_.originalRoomTemp = DEFAULT_ROOM_TEMP;
        }
    }

    void restoreDhw() {
        if (state_.active && state_.originalDhwTemp > 0.0f) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", state_.originalDhwTemp);
            io_.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", buf);
            state_.originalDhwTemp = 0.0f;
        }
    }

    void restoreRoom() {
        if (state_.active && state_.originalRoomTemp > 0.0f) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", state_.originalRoomTemp);
            io_.writeCanSignal("RAUMSOLLTEMP_I", buf);
            state_.originalRoomTemp = 0.0f;
        }
    }

    void persistState() {
        io_.saveBoostState(state_.originalDhwTemp, state_.originalRoomTemp, state_.active);
    }

    void applyEvuLock() {
        restoreDhw();
        restoreRoom();
        state_.active = true;
        persistState();
        io_.writeCanSignal("PROGRAMMSCHALTER", "Bereitschaft");
    }

    void applyNormal() {
        if (state_.active) {
            restoreDhw();
            restoreRoom();
            io_.writeCanSignal("PROGRAMMSCHALTER", "Tagbetrieb");
            state_.active = false;
            persistState();
        }
    }

    void applyRecommended() {
        captureOriginals();
        state_.active = true;
        persistState();

        io_.writeCanSignal("PROGRAMMSCHALTER", "Tagbetrieb");

        if (config_.boostState3 > 0.1f) {
            float boosted = state_.originalDhwTemp + config_.boostState3;
            if (boosted > MAX_DHW_TEMP) boosted = MAX_DHW_TEMP;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", boosted);
            io_.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", buf);
        }

        float boostedRoom = state_.originalRoomTemp + ROOM_BOOST_STATE3;
        if (boostedRoom > MAX_ROOM_TEMP) boostedRoom = MAX_ROOM_TEMP;
        char roomBuf[16];
        snprintf(roomBuf, sizeof(roomBuf), "%.1f", boostedRoom);
        io_.writeCanSignal("RAUMSOLLTEMP_I", roomBuf);
    }

    void applyForced() {
        captureOriginals();
        state_.active = true;
        persistState();

        io_.writeCanSignal("PROGRAMMSCHALTER", "Tagbetrieb");

        if (config_.boostState4 > 0.1f) {
            float boosted = state_.originalDhwTemp + config_.boostState4;
            if (boosted > MAX_DHW_TEMP) boosted = MAX_DHW_TEMP;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", boosted);
            io_.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", buf);
        }

        float boostedRoom = state_.originalRoomTemp + ROOM_BOOST_STATE4;
        if (boostedRoom > MAX_ROOM_TEMP) boostedRoom = MAX_ROOM_TEMP;
        char roomBuf[16];
        snprintf(roomBuf, sizeof(roomBuf), "%.1f", boostedRoom);
        io_.writeCanSignal("RAUMSOLLTEMP_I", roomBuf);
    }
};

#endif // SG_READY_CONTROLLER_H

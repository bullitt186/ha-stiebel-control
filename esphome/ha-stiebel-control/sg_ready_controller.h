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
#include "model_profile.h"

// Operating mode written by SG Ready when boosting back to normal operation.
// The WPM3 (WPL 10 AC) calls this mode "Komfortbetrieb", older WPM2 "Tagbetrieb".
#ifdef HA_CONTROLLER_WPM3
#define SG_READY_NORMAL_MODE "Komfortbetrieb"
#define SG_READY_AUTO_MODE   "Programmierbetrieb"
#else
#define SG_READY_NORMAL_MODE "Tagbetrieb"
#define SG_READY_AUTO_MODE   "Automatik"
#endif

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

    // Display label of a state ("1 - EVU Sperre" ...). Injected so the labels live only in the
    // language files and every publish uses the same set as the select options (#93).
    virtual const char* stateLabel(int state) = 0;

    // Last value read from the heat pump for a setpoint signal (EINSTELL_SPEICHERSOLLTEMP,
    // RAUMSOLLTEMP_I); false if none was read yet. Used as the boost baseline (#85).
    virtual bool currentSetpoint(const char* /*signalName*/, float& /*value*/) { return false; }
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

    // Default setpoint fallbacks (used when the heat pump's setpoint was not read yet)
    static constexpr float DEFAULT_DHW_TEMP  = 48.0f;
    static constexpr float DEFAULT_ROOM_TEMP = 20.0f;

    // Temperature ceiling for DHW boost
    static constexpr float MAX_DHW_TEMP  = 60.0f;
    static constexpr float MAX_ROOM_TEMP = 25.0f;

    struct Config {
        float boostState3 = 5.0f;   // DHW boost for state 3 (°C)
        float boostState4 = 8.0f;   // DHW boost for state 4 (°C)
        // Room setpoint boost of heating circuit 1 (K); 0 = room setpoint untouched
        float roomBoostState3 = 1.0f;
        float roomBoostState4 = 2.0f;
        // Operating mode in state 2: false = SG_READY_NORMAL_MODE (Tagbetrieb, default),
        // true = SG_READY_AUTO_MODE (Automatik: time programs and setpoint 2 apply, #85)
        bool normalAutomatik = false;
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
        snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/MANAGER/SG_READY_STATE/state");
        io_.publishMqtt(topic, io_.stateLabel(newState));

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

    void setRoomBoostState3(float boost) {
        if (boost >= 0.0f && boost <= 5.0f) {
            config_.roomBoostState3 = boost;
        }
    }

    // Mode for state 2. With apply, in state 2 the new mode is written at once
    // (not when restoring the setting from NVS on boot).
    void setNormalAutomatik(bool automatik, bool apply = true) {
        config_.normalAutomatik = automatik;
        if (apply && state_.currentState == STATE_NORMAL)
            io_.writeCanSignal("PROGRAMMSCHALTER", normalMode());
    }
    bool normalAutomatik() const { return config_.normalAutomatik; }
    const char* normalMode() const {
        return config_.normalAutomatik ? SG_READY_AUTO_MODE : SG_READY_NORMAL_MODE;
    }

    void setRoomBoostState4(float boost) {
        if (boost >= 0.0f && boost <= 5.0f) {
            config_.roomBoostState4 = boost;
        }
    }

    // Getters for inspection / sensor publishing
    int   currentState()    const { return state_.currentState; }
    bool  isActive()        const { return state_.active; }
    float boostState3()     const { return config_.boostState3; }
    float boostState4()     const { return config_.boostState4; }
    float roomBoostState3() const { return config_.roomBoostState3; }
    float roomBoostState4() const { return config_.roomBoostState4; }
    float originalDhwTemp() const { return state_.originalDhwTemp; }
    float originalRoomTemp() const { return state_.originalRoomTemp; }

    // "1 - EVU Sperre" / "1 - Grid Lock" → 1, etc.: the number prefix, in any language.
    // Returns 0 on failure.
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

    // Baseline = the setpoint last read from the heat pump, so a boost returns to what the
    // user had set; the constants only apply before the first read (#85).
    float baseline(const char* signal, float fallback, float minValid, float maxValid) {
        float v = 0.0f;
        if (io_.currentSetpoint(signal, v) && v >= minValid && v <= maxValid) return v;
        return fallback;
    }

    // The room original is only taken when the room setpoint is boosted, so a
    // room boost of 0 never writes the room setpoint on the way back either.
    // While a boost is active the originals stay (the read values are boosted then).
    void captureOriginals(bool boostRoom) {
        if (!state_.active || state_.originalDhwTemp == 0.0f) {
            state_.originalDhwTemp = baseline("EINSTELL_SPEICHERSOLLTEMP", DEFAULT_DHW_TEMP, 20.0f, MAX_DHW_TEMP);
        }
        if (boostRoom && (!state_.active || state_.originalRoomTemp == 0.0f)) {
            state_.originalRoomTemp = baseline("RAUMSOLLTEMP_I", DEFAULT_ROOM_TEMP, 10.0f, MAX_ROOM_TEMP);
        }
    }

    void boostRoom(float boost) {
        if (boost <= 0.05f) {
            restoreRoom();   // only writes if an earlier state had boosted it
            return;
        }
        float boostedRoom = state_.originalRoomTemp + boost;
        if (boostedRoom > MAX_ROOM_TEMP) boostedRoom = MAX_ROOM_TEMP;
        char roomBuf[16];
        snprintf(roomBuf, sizeof(roomBuf), "%.1f", boostedRoom);
        io_.writeCanSignal("RAUMSOLLTEMP_I", roomBuf);
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
            io_.writeCanSignal("PROGRAMMSCHALTER", normalMode());
            state_.active = false;
            persistState();
        }
    }

    void applyRecommended() {
        captureOriginals(config_.roomBoostState3 > 0.05f);
        state_.active = true;
        persistState();

        io_.writeCanSignal("PROGRAMMSCHALTER", SG_READY_NORMAL_MODE);

        if (config_.boostState3 > 0.1f) {
            float boosted = state_.originalDhwTemp + config_.boostState3;
            if (boosted > MAX_DHW_TEMP) boosted = MAX_DHW_TEMP;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", boosted);
            io_.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", buf);
        }

        boostRoom(config_.roomBoostState3);
    }

    void applyForced() {
        captureOriginals(config_.roomBoostState4 > 0.05f);
        state_.active = true;
        persistState();

        io_.writeCanSignal("PROGRAMMSCHALTER", SG_READY_NORMAL_MODE);

        if (config_.boostState4 > 0.1f) {
            float boosted = state_.originalDhwTemp + config_.boostState4;
            if (boosted > MAX_DHW_TEMP) boosted = MAX_DHW_TEMP;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", boosted);
            io_.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", buf);
        }

        boostRoom(config_.roomBoostState4);
    }
};

#endif // SG_READY_CONTROLLER_H

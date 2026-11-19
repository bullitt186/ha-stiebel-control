/*
 * sg_ready_io.h - SG Ready glue: the ESPHome adapter of SgReadyController (paced writes, NVS,
 * MQTT) and the SG Ready commands (#85, #93, #109). State machine: sg_ready_controller.h.
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef SG_READY_IO_H
#define SG_READY_IO_H

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include "config.h"
#include "language_select.h"
#include "can_members.h"
#include "signal_route.h"
#include "sg_ready_controller.h"
#include "mqtt_publish.h"
#include "paced_writes.h"
#include "writables.h"

// Last DHW / room setpoint read from the SG Ready route: baseline of a boost (#85)
static float lastSgDhwSetpoint = NAN;
static float lastSgRoomSetpoint = NAN;

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

// Set from common.yaml: refreshes the native SG Ready entities after a change
static std::function<void()> onSgReadyChanged;

inline void notifySgReadyChanged() {
    scheduleSgReadyPublish();
    if (onSgReadyChanged) onSgReadyChanged();
}

#endif  // SG_READY_IO_H

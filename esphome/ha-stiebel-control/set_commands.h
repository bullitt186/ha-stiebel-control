/*
 * set_commands.h - the MQTT set dispatcher: every <root>/<A>/<B>/set command goes to a
 * writable number, a select or a time program (#92, #109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef SET_COMMANDS_H
#define SET_COMMANDS_H

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "config.h"
#include "can_members.h"
#include "can_io.h"
#include "writables.h"
#include "sg_ready_io.h"
#include "time_programs.h"

// ============================================================================
// MQTT SET COMMANDS: one entry point for every "<root>/<A>/<B>/set" topic (#92)
// ============================================================================
// <A>/<B> is either <MEMBER>/<SIGNAL> (writable numbers and selects, looked up by signal
// name and member) or <program>/<day> (time programs). Adding or removing a writable
// entity only touches its table row. common.yaml subscribes "<root>/+/+/set" once.
// Topic root: HA_TOPIC_ROOT in config.h, from the device_name substitution (#102).

inline bool memberFromName(const std::string& name, CanMemberType& out) {
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
        if (name == CanMembers[i].Name) { out = static_cast<CanMemberType>(i); return true; }
    return false;
}

// Topic member accepted for a number: its discovery member, its table owner, and on WPL9
// the former KESSEL topics of the DHW setpoints.
inline bool numberAcceptsMember(const WritableNumberConfig& c, CanMemberType member) {
    if (getWritableNumberMember(c) == &CanMembers[member] || c.member == member) return true;
    return member == cm_kessel && kModel.canBDhwRoutes &&
           (strcmp(c.signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0 ||
            strcmp(c.signalName, "EINSTELL_SPEICHERSOLLTEMP2") == 0);
}

inline bool handleNumberCommand(const WritableNumberConfig& c, CanMemberType member, const std::string& payload) {
    if (!writableNumberApplies(c)) {
        ESP_LOGW("MQTT_CMD", "%s is not writable on %s", c.signalName, HA_DEVICE_MODEL_STR);
        return false;
    }
    publishWritableNumberDiscovery(c);
    char* end;
    const float v = strtof(payload.c_str(), &end);
    // Learned device range (#79) if known, else the static one
    const int limitIdx = writableNumberIndex(c);
    const float rangeMin = limitIdx >= 0 ? writableNumberMin(limitIdx) : c.min;
    const float rangeMax = limitIdx >= 0 ? writableNumberMax(limitIdx) : c.max;
    if (end == payload.c_str() || *end != '\0' || v < rangeMin - 0.001f || v > rangeMax + 0.001f) {
        ESP_LOGW("MQTT_CMD", "%s: \"%s\" outside %.2f..%.2f", c.signalName, payload.c_str(), rangeMin, rangeMax);
        return false;
    }
    // SG Ready values are kept on the ESP, not written to the heat pump
    if (strcmp(c.signalName, "SG_READY_BOOST_STATE3") == 0) { setSgReadyBoost3(v); notifySgReadyChanged(); return true; }
    if (strcmp(c.signalName, "SG_READY_BOOST_STATE4") == 0) { setSgReadyBoost4(v); notifySgReadyChanged(); return true; }
    if (strcmp(c.signalName, "SG_READY_ROOM_BOOST_STATE3") == 0 || strcmp(c.signalName, "SG_READY_ROOM_BOOST_STATE4") == 0) {
        const bool ok = handleSgReadyRoomBoost(c.signalName[25] == '3' ? 3 : 4, payload);
        if (ok) notifySgReadyChanged();
        return ok;
    }
    writeConfiguredSignal(member, c.signalName, payload.c_str());
    return true;
}

inline bool handleSelectCommand(const WritableSelectConfig& c, const std::string& payload) {
    publishWritableSelectDiscovery(c);
    if (strcmp(c.signalName, "SG_READY_STATE") == 0) {
        const int state = SgReadyController::parseStateString(payload);
        if (state < SgReadyController::STATE_EVU_LOCK || state > SgReadyController::STATE_FORCED) {
            ESP_LOGW("MQTT_CMD", "Invalid SG Ready state: %s", payload.c_str());
            return false;
        }
        applySgReadyState(state);
        if (onSgReadyChanged) onSgReadyChanged();
        return true;
    }
    if (strcmp(c.signalName, "SG_READY_NORMAL_MODE") == 0) {
        const bool ok = handleSgReadyNormalMode(payload);
        if (!ok) ESP_LOGW("MQTT_CMD", "Invalid SG Ready state 2 mode: %s", payload.c_str());
        return ok;
    }
    const char* v = payload.c_str();
    writeSignal(&CanMembers[c.member], GetElsterIndex(c.signalName), v);
    readSignal(&CanMembers[c.member], c.signalName);
    return true;
}

// "<root>/<A>/<B>/set" → number, select or time program. Returns false if nothing matched
// or the value was rejected.
inline bool handleSetCommand(const std::string& topic, const std::string& payload) {
    std::vector<std::string> parts;
    size_t pos = 0;
    while (true) {
        const size_t slash = topic.find('/', pos);
        parts.push_back(topic.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos));
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    if (parts.size() != 4 || parts[0] != HA_TOPIC_ROOT || parts[3] != "set") return false;
    ESP_LOGI("MQTT_CMD", "%s/%s = %s", parts[1].c_str(), parts[2].c_str(), payload.c_str());
    for (int prog = 0; prog < TP_COUNT; prog++) {
        if (parts[1] != TIME_PROGRAMS[prog].topic) continue;
        for (int day = 0; day < dhwprog::DAYS; day++)
            if (parts[2] == TIME_PROGRAM_KEYS[day]) return handleTimeProgramCommand(prog, day, payload);
        return false;
    }
    CanMemberType member;
    if (!memberFromName(parts[1], member)) {
        ESP_LOGW("MQTT_CMD", "Unknown member in %s", topic.c_str());
        return false;
    }
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        const WritableNumberConfig& c = writableNumbers[i];
        if (parts[2] == c.signalName && numberAcceptsMember(c, member)) return handleNumberCommand(c, member, payload);
    }
    for (size_t i = 0; i < WRITABLE_SELECT_COUNT; i++) {
        const WritableSelectConfig& c = writableSelects[i];
        if (parts[2] == c.signalName && c.member == member) return handleSelectCommand(c, payload);
    }
    ESP_LOGW("MQTT_CMD", "No writable entity for %s", topic.c_str());
    return false;
}

#endif  // SET_COMMANDS_H

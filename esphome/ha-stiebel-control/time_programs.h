/*
 * time_programs.h - WPMW II time programs in Home Assistant: one text per day, published from
 * the read periods and written through the paced queue (#82, #109). Parsing: dhw_program.h.
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef TIME_PROGRAMS_H
#define TIME_PROGRAMS_H

#include <cstdio>
#include <string>
#include <vector>
#include "config.h"
#include "language_select.h"
#include "can_members.h"
#include "signal_route.h"
#include "dhw_program.h"
#include "mqtt_publish.h"
#include "discovery.h"
#include "paced_writes.h"

// ============================================================================
// TIME PROGRAMS (WPMW II): DHW (#82) and heating circuit 1
// ============================================================================
// One text per day in Home Assistant, from the three periods <prefix><day>(_SCHALT_2/3) on the
// manager. Read on all WPMW II models; writable where the write was tested on the display.
struct DhwProgramDay {
    std::string period[dhwprog::PERIODS];
    uint8_t known = 0;          // bit per period read
    bool discovered = false;
    std::string published;      // last text sent
    uint32_t publishedMs = 0;
};
struct TimeProgramDef {
    CanMemberType member;       // bus member holding the program
    const char* prefix;         // signal name prefix
    const char* topic;          // heatingpump/<topic>/<day>/state|set
    const char* uid;            // unique_id prefix
    const char* names[dhwprog::DAYS];
};
static constexpr int TP_DHW = 0, TP_HEATING = 1, TP_HEATING2 = 2, TP_COUNT = 3;
static const TimeProgramDef TIME_PROGRAMS[TP_COUNT] = {
    {cm_manager, dhwprog::PREFIX_DHW, "dhw_program", "stiebel_ww_programm_",
     {LNAME_WWPROG_MO, LNAME_WWPROG_DI, LNAME_WWPROG_MI, LNAME_WWPROG_DO,
      LNAME_WWPROG_FR, LNAME_WWPROG_SA, LNAME_WWPROG_SO}},
    {cm_manager, dhwprog::PREFIX_HEATING, "heating_program", "stiebel_heiz_programm_",
     {LNAME_HZPROG_MO, LNAME_HZPROG_DI, LNAME_HZPROG_MI, LNAME_HZPROG_DO,
      LNAME_HZPROG_FR, LNAME_HZPROG_SA, LNAME_HZPROG_SO}},
    // Heating circuit 2: the manager keeps its program as HEIZPROG_1 on MISCHERMODUL_3 (0x602);
    // HEIZPROG_2 on the manager does not change circuit 2 (WPL13E, 0x602 kept 06:00-22:00)
    {cm_mischermodul_3, dhwprog::PREFIX_HEATING, "heating_program_2", "stiebel_heiz_programm_hk2_",
     {LNAME_HZ2PROG_MO, LNAME_HZ2PROG_DI, LNAME_HZ2PROG_MI, LNAME_HZ2PROG_DO,
      LNAME_HZ2PROG_FR, LNAME_HZ2PROG_SA, LNAME_HZ2PROG_SO}},
};
static DhwProgramDay timeProgram[TP_COUNT][dhwprog::DAYS];
static DhwProgramDay (&dhwProgram)[dhwprog::DAYS] = timeProgram[TP_DHW];
static const char* const TIME_PROGRAM_KEYS[dhwprog::DAYS] = {"mo", "di", "mi", "do", "fr", "sa", "so"};
static constexpr uint32_t DHW_PROGRAM_HEARTBEAT_MS = 30UL * 60UL * 1000UL;

inline bool dhwProgramWritable() {
#ifdef HA_READ_ONLY_true
    return false;
#endif
    return isWpmw2Model() && kModel.wpmw2WritesVerified;
}

inline bool publishTimeProgramDiscovery(int prog, int day) {
    const TimeProgramDef& def = TIME_PROGRAMS[prog];
    const bool writable = dhwProgramWritable();
    const char* key = TIME_PROGRAM_KEYS[day];
    char uid[48], topic[112];
    snprintf(uid, sizeof(uid), "%s%s", def.uid, key);
    snprintf(topic, sizeof(topic), "homeassistant/%s/" HA_TOPIC_ROOT "/%s/config", writable ? "text" : "sensor", uid);
    std::string p = std::string("{\"name\":\"") + def.names[day] + "\",\"unique_id\":\"" + uid + "\"," +
                    defaultEntityIdJson(writable ? "text" : "sensor", uid) + "\"state_topic\":\"" HA_TOPIC_ROOT "/" + def.topic + "/" + key + "/state\",";
    if (writable)
        p += std::string("\"command_topic\":\"" HA_TOPIC_ROOT "/") + def.topic + "/" + key + "/set\",\"max\":40,";
    p += "\"icon\":\"mdi:calendar-clock\"," HA_MAIN_DEVICE_JSON "}";
    return mqttPublish(topic, p.c_str(), p.size());
}

inline void timeProgramPublish(int prog, int day, bool force) {
    DhwProgramDay& d = timeProgram[prog][day];
    if (d.known != 0x7) return;
    if (!d.discovered) d.discovered = publishTimeProgramDiscovery(prog, day);
    const std::string text = dhwprog::dayText(d.period);
    const uint32_t now = millis();
    if (!force && text == d.published && now - d.publishedMs < DHW_PROGRAM_HEARTBEAT_MS) return;
    char topic[96];
    snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/%s/%s/state", TIME_PROGRAMS[prog].topic, TIME_PROGRAM_KEYS[day]);
    if (mqttPublish(topic, text.c_str(), text.size())) {
        d.published = text;
        d.publishedMs = now;
    }
}

// Config and text go out again with the next poll of each day (not all at once: the MQTT
// queue is short right after an HA birth message).
inline void dhwProgramRepublish() {
    for (auto& prog : timeProgram)
        for (auto& d : prog) {
            d.discovered = false;
            d.published.clear();
        }
}

// A program period from its member? Then it updates that day's text (no raw sensor).
inline bool timeProgramUpdate(const CanMember& cm, const char* signal, const std::string& value) {
    for (int prog = 0; prog < TP_COUNT; prog++) {
        if (CanMembers[TIME_PROGRAMS[prog].member].CanId != cm.CanId) continue;
        int day, slot;
        if (!dhwprog::parseSignalName(signal, day, slot, TIME_PROGRAMS[prog].prefix)) continue;
        if (value == "-255") return true;   // not provided by this manager
        DhwProgramDay& d = timeProgram[prog][day];
        d.period[slot] = dhwprog::normalizeDecoded(value);
        d.known |= static_cast<uint8_t>(1u << slot);
        timeProgramPublish(prog, day, false);
        return true;
    }
    return false;
}

// MQTT command "11:00-17:00" / "06:30-09:00,11:00-17:00" / "-" for one day. Invalid input
// writes nothing and sends the current program back, so HA shows what the heat pump has.
inline bool handleTimeProgramCommand(int prog, int day, const std::string& payload) {
    if (prog < 0 || prog >= TP_COUNT || day < 0 || day >= dhwprog::DAYS) return false;
    if (!dhwProgramWritable()) {
        ESP_LOGW("PROGRAM", "time programs are read-only on this model");
        return false;
    }
    std::vector<dhwprog::Period> periods;
    std::string err;
    if (!dhwprog::parsePeriods(payload, periods, err)) {
        ESP_LOGW("PROGRAM", "%s%s: \"%s\" rejected: %s", TIME_PROGRAMS[prog].prefix, dhwprog::DAY_CODES[day],
                 payload.c_str(), err.c_str());
        timeProgramPublish(prog, day, true);
        return false;
    }
    DhwProgramDay& d = timeProgram[prog][day];
    for (int slot = 0; slot < dhwprog::PERIODS; slot++) {
        const std::string want = slot < static_cast<int>(periods.size()) ? dhwprog::formatPeriod(periods[slot]) : "";
        if ((d.known & (1u << slot)) && d.period[slot] == want) continue;   // unchanged
        const std::string name = dhwprog::signalName(day, slot, TIME_PROGRAMS[prog].prefix);
        const CanMember* member = &CanMembers[TIME_PROGRAMS[prog].member];
        queuePacedWrite(member, member, GetElsterIndex(name.c_str()), want.c_str());
    }
    return true;
}
inline bool handleDhwProgramCommand(int day, const std::string& payload) {
    return handleTimeProgramCommand(TP_DHW, day, payload);
}
inline bool handleHeatingProgramCommand(int day, const std::string& payload) {
    return handleTimeProgramCommand(TP_HEATING, day, payload);
}
inline bool handleHeating2ProgramCommand(int day, const std::string& payload) {
    return handleTimeProgramCommand(TP_HEATING2, day, payload);
}

#endif  // TIME_PROGRAMS_H

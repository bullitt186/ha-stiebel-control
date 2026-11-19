/*
 * fault_list.h - fault list of the manager (display "48 FEHLERLISTE") and the remedy texts
 * of a fault (#109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef FAULT_LIST_H
#define FAULT_LIST_H

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include "config.h"
#include "language_select.h"
#include "KElsterTable.h"
#include "can_members.h"
#include "can_io.h"
#include "bus_scan.h"
#include "mqtt_publish.h"
#include "discovery.h"

// Short remedy for a decoded fault text (WPMW II manual fault list), shown as
// attribute of the "Störung" sensor. ponytail: keyword match, not per code.
const char* faultRemedy(const std::string& fault) {
    std::string up(fault);
    std::transform(up.begin(), up.end(), up.begin(), ::toupper);
    auto has = [&](const char* k) { return up.find(k) != std::string::npos; };
    if (fault == "Keine Stoerung") return "";
    // Specific entries first (WPMW II manual fault list); "HD-SENSOR-MAX" is the high
    // pressure limit, not a sensor fault, so it must not fall through to "SENSOR".
    if (has("HD-SENSOR-MAX")) return "Hochdruck-Grenze erreicht: Heizungs-Volumenstrom zu klein oder Raumtemperatur/Heizkurve zu hoch.";
    if (has("LEISTUNG")) return "Hochdruck baut sich nicht auf: Phasenfolge und Sicherungen pruefen.";
    if (has("ABTAU")) return "Wasservolumenstrom oder Wassertemperatur beim Abtauen zu niedrig.";
    if (has("HEISSGAS")) return "Heissgas zu hoch: Einspritz-/Expansionsventil pruefen (Fachhandwerker).";
    if (has("SENSOR") || has("FUEHLER") || has("ERR T") || has("ERR AT")) return "Fuehler, Leitung und Steckverbindungen pruefen, ggf. tauschen.";
    if (has("DRUCK") || up == "ND") return "Volumenstrom und Fuehlerankopplung pruefen; bei Wiederholung Kaeltekreis pruefen lassen.";
    if (has("INV")) return "Inverterstoerung: Fachhandwerker.";
    return "Siehe Fehlerliste im Handbuch des Waermepumpenmanagers.";
}

// ============================================================================
// FAULT LIST (display "48 FEHLERLISTE"): the manager keeps its last 20 faults in
// FEHLERFELD_0..139 (0x0b00..0x0b8b), 7 fields per entry: minute, hour, day,
// month, year (2 digits), unused, fault code (ErrorList). Verified on a WPL13E
// against the display. Read slowly (1 field/s) every 30 min, published as one
// sensor: newest fault as state, all entries as JSON attribute.
// ============================================================================
static constexpr uint16_t FAULT_FIELD_FIRST = 0x0b00;
static constexpr int FAULT_ENTRIES = 20, FAULT_FIELDS = 7, FAULT_FIELD_COUNT = FAULT_ENTRIES * FAULT_FIELDS;
static constexpr unsigned long FAULT_CYCLE_MS = 30UL * 60UL * 1000UL;
struct FaultListState {
    uint16_t raw[FAULT_FIELD_COUNT] = {0};
    bool seen[FAULT_FIELD_COUNT] = {false};
    int pos = -1;                       // next field to request, -1 = idle
    unsigned long nextCycle = 60000UL;  // first cycle 1 min after boot
    unsigned long publishAt = 0;
};
static FaultListState faultList;

struct FaultEntry { int year, month, day, hour, minute, code; };

// Decode the 20 entries from the raw fields, newest first. Entries with
// implausible dates or code 0 are skipped.
std::vector<FaultEntry> decodeFaultList(const uint16_t raw[], const bool seen[]) {
    std::vector<FaultEntry> out;
    for (int e = 0; e < FAULT_ENTRIES; e++) {
        const int b = e * FAULT_FIELDS;
        bool all = true;
        for (int f = 0; f < FAULT_FIELDS; f++) all = all && seen[b + f] && raw[b + f] != 0x8000;
        if (!all) continue;
        FaultEntry x{2000 + raw[b + 4], raw[b + 3], raw[b + 2], raw[b + 1], raw[b + 0], raw[b + 6]};
        if (x.code == 0 || x.month < 1 || x.month > 12 || x.day < 1 || x.day > 31 || x.hour > 23 || x.minute > 59) continue;
        out.push_back(x);
    }
    std::sort(out.begin(), out.end(), [](const FaultEntry& a, const FaultEntry& b) {
        return std::tie(a.year, a.month, a.day, a.hour, a.minute) > std::tie(b.year, b.month, b.day, b.hour, b.minute);
    });
    return out;
}

std::string faultText(int code) {
    char buf[32];
    SetValueType(buf, et_err_nr, static_cast<unsigned short>(code));
    return buf;
}

void publishFaultList() {
    std::vector<FaultEntry> entries = decodeFaultList(faultList.raw, faultList.seen);
    if (entries.empty()) return;   // model without this list, or nothing read yet
    static bool discovered = false;
    if (!discovered) {
        const std::string cfgStr = "{\"name\":\"" LNAME_CALC_FEHLERLISTE "\",\"unique_id\":\"stiebel_calculated_fehlerliste\"," +
            defaultEntityIdJson("sensor", "stiebel_calculated_fehlerliste") +
            "\"state_topic\":\"" HA_TOPIC_ROOT "/calculated/fehlerliste/state\","
            "\"json_attributes_topic\":\"" HA_TOPIC_ROOT "/calculated/fehlerliste/attributes\","
            "\"availability_topic\":\"" HA_TOPIC_ROOT "/status\",\"icon\":\"mdi:clipboard-alert-outline\","
            HA_MAIN_DEVICE_JSON "}";
        mqttPublish("homeassistant/sensor/" HA_TOPIC_ROOT "/stiebel_calculated_fehlerliste/config", cfgStr.c_str(), cfgStr.size());
        discovered = true;
    }
    std::ostringstream a;
    char t[24];
    const FaultEntry& n = entries.front();
    snprintf(t, sizeof(t), "%04d-%02d-%02d %02d:%02d", n.year, n.month, n.day, n.hour, n.minute);
    std::string newest = faultText(n.code);
    a << "{\"letzter_fehler_zeit\":\"" << t << "\",\"abhilfe\":\"" << faultRemedy(newest)
      << "\",\"anzahl\":" << entries.size() << ",\"eintraege\":[";
    for (size_t i = 0; i < entries.size(); i++) {
        const FaultEntry& x = entries[i];
        snprintf(t, sizeof(t), "%04d-%02d-%02d %02d:%02d", x.year, x.month, x.day, x.hour, x.minute);
        a << (i ? "," : "") << "{\"zeit\":\"" << t << "\",\"fehler\":\"" << faultText(x.code) << "\",\"code\":" << x.code << "}";
    }
    a << "]}";
    std::string attrs = a.str();
    mqttPublish(HA_TOPIC_ROOT "/calculated/fehlerliste/attributes", attrs.c_str(), attrs.length());
    mqttPublish(HA_TOPIC_ROOT "/calculated/fehlerliste/state", newest.c_str(), newest.length());
}

// Called every second from common.yaml; pauses during a bus scan.
void faultListStep() {
    unsigned long now = millis();
    if (faultList.publishAt && now >= faultList.publishAt) {
        faultList.publishAt = 0;
        publishFaultList();
    }
    if (scanState.active || disableCanRequests) return;
    if (faultList.pos < 0) {
        if (now < faultList.nextCycle) return;
        faultList.pos = 0;
    }
    readSignalRaw(&CanMembers[cm_manager], FAULT_FIELD_FIRST + faultList.pos, "FEHLERLISTE");
    if (++faultList.pos >= FAULT_FIELD_COUNT) {
        faultList.pos = -1;
        faultList.nextCycle = now + FAULT_CYCLE_MS;
        faultList.publishAt = now + 3000;   // let the last answers arrive
    }
}

// Returns true if the frame was a fault-list field (stored, not published as entity)
bool handleFaultListFrame(const CanMember& cm, const std::vector<uint8_t>& msg) {
    if (&cm != &CanMembers[cm_manager] || (msg[0] & 0x0F) != 2 || msg[2] != 0xfa) return false;
    uint16_t idx = (msg[3] << 8) | msg[4];
    if (idx < FAULT_FIELD_FIRST || idx >= FAULT_FIELD_FIRST + FAULT_FIELD_COUNT) return false;
    int i = idx - FAULT_FIELD_FIRST;
    faultList.raw[i] = (msg[5] << 8) | msg[6];
    faultList.seen[i] = true;
    return true;
}

#endif  // FAULT_LIST_H

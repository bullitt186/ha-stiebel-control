/*
 * bus_scan.h - bus scan of one CAN member (MQTT <root>/scan/start, <root>/scan/stop) (#109).
 *
 * Start payload on <root>/scan/start: "MEMBER|0xID [0xLO 0xHI] [fa|f8|f9]"
 *   - Tokens are separated by white space.
 *   - MEMBER: a name of CanMembers[] (case-insensitive, PC excluded), or a hex CAN id 0x000..0x7FF
 *     (not the id of PC). A hex id that belongs to a table member scans that member under its
 *     name. Any other hex id scans the target "0x%03x" (lower case) and publishes under that name.
 *   - Without a range the scan covers every ElsterTable index, for a hex id too (decision: a hex
 *     id only selects the member, the range stays optional as for names).
 *   - Range: exactly two hex numbers LO HI, both <= 0xFFFF, HI >= LO; it replaces the table.
 *   - Switch byte: fa (value, default), f8 (minimum) or f9 (maximum), at most one, any position.
 *   - An invalid payload is logged and changes nothing; a valid start replaces a running scan.
 * Answers with a value (not 0x8000) are published retained to <root>/scan/<MEMBER>/0x<idx>,
 * for f8/f9 to .../0x<idx>/f8 or .../0x<idx>/f9.
 *
 * Members scan (<root>/scan/members, only with can_debug: "true", see startMemberScan()) asks
 * every CAN id of a range for one index; its answers go to <root>/scan/members/0x<id>.
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef BUS_SCAN_H
#define BUS_SCAN_H

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <strings.h>
#include <vector>
#include "ElsterTable.h"
#include "can_io.h"
#include "mqtt_publish.h"

// ============================================================================
// BUS SCAN (MQTT heatingpump/scan/start, heatingpump/scan/stop, see the header comment)
// ============================================================================
// One request per SCAN_GAP_MS; normal polling pauses meanwhile. Results go out at once,
// nothing is buffered.
static constexpr unsigned long SCAN_GAP_MS = 150;
static constexpr unsigned long SCAN_TAIL_MS = 1000;  // wait for the last answers
// Index of a members scan when the payload gives none
static constexpr uint16_t SCAN_MEMBER_DEFAULT_INDEX = 0x000b;

struct ScanState {
    bool active = false;
    const CanMember* cm = nullptr;      // scanned member (&target for an unknown hex id); nullptr in members mode
    bool table = true;
    uint32_t begin = 0, pos = 0, end = 0;   // indices (table/range) or CAN ids (members)
    unsigned long lastSend = 0, startMs = 0;
    uint16_t hits = 0;
    uint8_t sw = ELSTER_SWITCH_VALUE;   // switch byte of the requests: fa, f8 or f9
    bool members = false;               // members scan: pos is the CAN id to ask
    uint16_t memberIndex = SCAN_MEMBER_DEFAULT_INDEX;
    char targetName[8] = {0};           // "0x7ff" of a hex id without table entry
    CanMember target{nullptr, 0};       // set after the struct copy in startScan()
};
static ScanState scanState;

inline const char* scanModeName() {
    return scanState.members ? "members" : (scanState.table ? "table" : "range");
}

void publishScanStatus(const char* state) {
    char buf[192];
    uint32_t total = scanState.end - scanState.begin + 1;
    uint32_t done = scanState.pos - scanState.begin;
    snprintf(buf, sizeof(buf),
             "{\"state\":\"%s\",\"mode\":\"%s\",\"member\":\"%s\",\"done\":%u,\"total\":%u,\"hits\":%u,\"seconds\":%lu}",
             state, scanModeName(), scanState.cm ? scanState.cm->Name : "", (unsigned)done, (unsigned)total,
             scanState.hits, (millis() - scanState.startMs) / 1000UL);
    mqttPublish(HA_TOPIC_ROOT "/scan/status", buf, strlen(buf));
}

// Splits at white space
inline std::vector<std::string> splitScanTokens(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// "0x1A" or "1A" (hex, prefix optional) with value <= max
inline bool parseScanHex(const std::string& tok, uint32_t max, uint32_t& out) {
    const char* p = tok.c_str();
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    if (!isxdigit(static_cast<unsigned char>(*p))) return false;
    char* end = nullptr;
    const unsigned long v = strtoul(p, &end, 16);
    if (*end != '\0' || v > max) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

// Table member by name (case-insensitive), PC excluded
inline const CanMember* findScanMemberByName(const std::string& name) {
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
        if (i != cm_pc && strcasecmp(CanMembers[i].Name, name.c_str()) == 0) return &CanMembers[i];
    return nullptr;
}

// Table member by CAN id, PC excluded
inline const CanMember* findScanMemberById(uint32_t canId) {
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
        if (i != cm_pc && CanMembers[i].CanId == canId) return &CanMembers[i];
    return nullptr;
}

void stopScan(const char* state = "stopped") {
    if (!scanState.active) return;
    scanState.active = false;
    ESP_LOGI("SCAN", "Scan %s: %u hits", state, scanState.hits);
    publishScanStatus(state);
}

// Payload "MEMBER|0xID [0xLO 0xHI] [fa|f8|f9]", see the header comment
void startScan(const std::string& payload) {
    const std::vector<std::string> tok = splitScanTokens(payload);
    const CanMember* member = nullptr;   // table member; nullptr for a hex id without table entry
    uint32_t targetId = 0;
    bool ok = !tok.empty();
    if (ok) {
        const bool hexId = tok[0].size() > 1 && tok[0][0] == '0' && (tok[0][1] == 'x' || tok[0][1] == 'X');
        if (hexId) {
            ok = parseScanHex(tok[0], 0x7FF, targetId) && targetId != CanMembers[cm_pc].CanId;
            if (ok) member = findScanMemberById(targetId);
        } else {
            member = findScanMemberByName(tok[0]);
            ok = member != nullptr;
        }
    }

    uint8_t sw = ELSTER_SWITCH_VALUE;
    bool swGiven = false;
    std::vector<uint32_t> nums;   // range LO HI, empty for the table
    for (size_t i = 1; ok && i < tok.size(); ++i) {
        std::string lower = tok[i];
        for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (lower == "fa" || lower == "f8" || lower == "f9") {
            ok = !swGiven;
            swGiven = true;
            sw = (lower == "fa") ? ELSTER_SWITCH_VALUE : (lower == "f8") ? ELSTER_SWITCH_MIN : ELSTER_SWITCH_MAX;
        } else {
            uint32_t v = 0;
            ok = parseScanHex(tok[i], 0xFFFF, v);
            if (ok) nums.push_back(v);
        }
    }
    ok = ok && (nums.empty() || (nums.size() == 2 && nums[1] >= nums[0]));
    if (!ok) {
        ESP_LOGW("SCAN", "Invalid scan request \"%s\" (expected \"MEMBER|0xID [0xLO 0xHI] [fa|f8|f9]\")", payload.c_str());
        return;
    }

    const bool table = nums.empty();
    scanState = ScanState{};
    scanState.active = true;
    scanState.table = table;
    scanState.sw = sw;
    scanState.begin = scanState.pos = table ? 0 : nums[0];
    scanState.end = table ? ElsterTableCount - 1 : nums[1];
    // Pointers into scanState only after the struct copy above
    if (member) {
        scanState.cm = member;
    } else {
        snprintf(scanState.targetName, sizeof(scanState.targetName), "0x%03x", static_cast<unsigned>(targetId));
        scanState.target = CanMember{scanState.targetName, targetId};
        scanState.cm = &scanState.target;
    }
    scanState.startMs = millis();
    ESP_LOGI("SCAN", "Scanning %s (%s, switch 0x%02x), normal polling paused", scanState.cm->Name, scanModeName(),
             static_cast<unsigned>(scanState.sw));
    publishScanStatus("running");
}

#ifdef HA_CAN_DEBUG_true
// Members scan, MQTT <root>/scan/members, payload "[0xLO 0xHI] [0xIDX]": asks every CAN id of
// the range (default 0x000..0x7FF) for one index (fa layout) and publishes the answers to
// <root>/scan/members/0x<id>. Payload: no token = all ids and the default index, one token =
// index, two = range, three = range and index. Invalid payload: logged, nothing changes.
void startMemberScan(const std::string& payload) {
    const std::vector<std::string> tok = splitScanTokens(payload);
    uint32_t lo = 0, hi = 0x7FF, idx = SCAN_MEMBER_DEFAULT_INDEX;
    bool ok = tok.size() <= 3;
    if (ok && tok.size() == 1) {
        ok = parseScanHex(tok[0], 0xFFFF, idx);
    } else if (ok && tok.size() >= 2) {
        ok = parseScanHex(tok[0], 0x7FF, lo) && parseScanHex(tok[1], 0x7FF, hi) && lo <= hi;
        if (ok && tok.size() == 3) ok = parseScanHex(tok[2], 0xFFFF, idx);
    }
    if (!ok) {
        ESP_LOGW("SCAN", "Invalid members scan request \"%s\" (expected \"[0xLO 0xHI] [0xIDX]\")", payload.c_str());
        return;
    }
    scanState = ScanState{};
    scanState.active = true;
    scanState.members = true;
    scanState.table = false;
    scanState.begin = scanState.pos = lo;
    scanState.end = hi;
    scanState.memberIndex = static_cast<uint16_t>(idx);
    scanState.startMs = millis();
    ESP_LOGI("SCAN", "Members scan 0x%03x-0x%03x, index 0x%04x, normal polling paused", (unsigned)lo, (unsigned)hi,
             (unsigned)idx);
    publishScanStatus("running");
}
#else
inline void startMemberScan(const std::string&) {
    ESP_LOGW("SCAN", "Member scan not built in (substitution can_debug: \"true\" builds it in)");
}
#endif  // HA_CAN_DEBUG_true

// Called every SCAN_GAP_MS from common.yaml
void scanStep() {
    if (!scanState.active || disableCanRequests) return;
    unsigned long now = millis();
    if (now - scanState.lastSend < SCAN_GAP_MS) return;
#ifdef HA_CAN_DEBUG_true
    if (scanState.members) {
        // Own id: skip without a request and without waiting
        while (scanState.pos <= scanState.end && scanState.pos == CanMembers[cm_pc].CanId) scanState.pos++;
    }
#endif
    if (scanState.pos > scanState.end) {
        if (now - scanState.lastSend >= SCAN_TAIL_MS) stopScan("done");
        return;
    }
    if (scanState.members) {
#ifdef HA_CAN_DEBUG_true
        CanMember probe{"?", scanState.pos};
        readSignalRaw(&probe, scanState.memberIndex, "MEMBERS", ELSTER_SWITCH_VALUE);
#endif
    } else {
        uint16_t idx = scanState.table ? ElsterTable[scanState.pos].Index : static_cast<uint16_t>(scanState.pos);
        readSignalRaw(scanState.cm, idx, "SCAN", scanState.sw);
    }
    scanState.pos++;
    scanState.lastSend = now;
    if (scanState.pos % 100 == 0) publishScanStatus("running");
}

#ifdef HA_CAN_DEBUG_true
// Members scan: answer [begin, end] from another id, index memberIndex (fa layout only)
inline bool handleMemberScanFrame(uint32_t canId, const std::vector<uint8_t>& msg) {
    if (canId < scanState.begin || canId > scanState.end || canId == CanMembers[cm_pc].CanId) return false;
    uint16_t idx = 0, raw = 0;
    if (msg[2] == ELSTER_SWITCH_MIN || msg[2] == ELSTER_SWITCH_MAX) return false;
    if (msg[2] == ELSTER_SWITCH_VALUE) {
        idx = static_cast<uint16_t>((msg[3] << 8) | msg[4]);
        raw = static_cast<uint16_t>((msg[5] << 8) | msg[6]);
    } else {
        idx = msg[2];
        raw = static_cast<uint16_t>((msg[3] << 8) | msg[4]);
    }
    if (idx != scanState.memberIndex) return false;
    scanState.hits++;
    char topic[112], buf[64];
    snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/scan/members/0x%03x", static_cast<unsigned>(canId));
    snprintf(buf, sizeof(buf), "{\"index\":\"0x%04x\",\"raw\":%u}", static_cast<unsigned>(idx),
             static_cast<unsigned>(raw));
    mqttPublish(topic, buf, strlen(buf));
    return true;
}
#endif

// True if the answer is addressed to the PC. Header bytes carry the target id (see generate_read_id()).
// Otherwise answers from the scanned member to other bus members would be reported as scan hits.
inline bool scanAnswerIsForPc(const std::vector<uint8_t>& msg) {
    const CanIdBytes pc = generate_read_id(CanMembers[cm_pc].CanId);
    return (msg[0] & 0xF0) == (pc.first & 0xF0) && msg[1] == pc.second;
}

// Returns true if the frame was a scan answer (then it is not processed further).
// Called from processAndUpdate() for every answer frame (type 2) of the bus.
bool handleScanFrame(uint32_t canId, const std::vector<uint8_t>& msg) {
    if (!scanState.active || msg.size() < 7 || (msg[0] & 0x0F) != 2) return false;
    if (!scanAnswerIsForPc(msg)) return false;
#ifdef HA_CAN_DEBUG_true
    if (scanState.members) return handleMemberScanFrame(canId, msg);
#endif
    if (!scanState.cm || canId != scanState.cm->CanId) return false;

    const uint8_t sw = scanState.sw;
    bool longLayout = true;
    if (sw == ELSTER_SWITCH_VALUE) {
        // Limit answers (f8/f9) belong to handleLimitAnswer() (#79)
        if (msg[2] == ELSTER_SWITCH_MIN || msg[2] == ELSTER_SWITCH_MAX) return false;
        longLayout = (msg[2] == ELSTER_SWITCH_VALUE);
    } else {
        if (msg[2] != sw) return false;
    }
    const uint16_t idx = longLayout ? static_cast<uint16_t>((msg[3] << 8) | msg[4]) : msg[2];
    const uint16_t raw = longLayout ? static_cast<uint16_t>((msg[5] << 8) | msg[6])
                                    : static_cast<uint16_t>((msg[3] << 8) | msg[4]);
    if (raw != 0x8000) {
        // Name and value from the decoder: f8/f9 answers are decoded as the long value layout
        std::vector<uint8_t> frame = msg;
        if (sw != ELSTER_SWITCH_VALUE) frame[2] = ELSTER_SWITCH_VALUE;
        std::string value;
        const CanMember* decoded = nullptr;
        const ElsterIndex* ei = processCanMessage(frame, canId, value, &decoded);
        scanState.hits++;
        const char* suffix = (sw == ELSTER_SWITCH_MIN) ? "/f8" : (sw == ELSTER_SWITCH_MAX) ? "/f9" : "";
        char topic[112], buf[128];
        snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/scan/%s/0x%04x%s", scanState.cm->Name, (unsigned)idx, suffix);
        snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"raw\":%u,\"value\":\"%s\"}",
                 ei->Index == idx ? ei->Name : "", (unsigned)raw, value.c_str());
        mqttPublish(topic, buf, strlen(buf));
    }
    return true;
}

#endif  // BUS_SCAN_H

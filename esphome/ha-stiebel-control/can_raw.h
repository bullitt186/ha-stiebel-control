/*
 * can_raw.h - raw read and write of one Elster index at any CAN id, for bus analysis (#109).
 *
 * MQTT, only with substitution can_debug: "true" (see can_debug.h):
 *   <root>/raw/read    payload "0xID 0xIDX [fa|f8|f9]"
 *   <root>/raw/write   payload "0xID 0xIDX 0xVALUE" (only with can_raw_write: "true", see common.yaml)
 *   <root>/raw/result  one JSON object per operation, not retained:
 *     {"op":"read","id":"0x180","index":"0x0013","switch":"fa","raw":"0x0208","status":"ok"}
 *     {"op":"write","id":"0x180","index":"0x0013","value":"0x0208","status":"verified","ack":4,"readback":"0x0208"}
 *
 * One operation at a time. A bus scan or another raw operation running makes a new request
 * "busy". Normal polling continues meanwhile. Frames reach this module through
 * handleBusAnalysisFrame() in processAndUpdate(), raw answers first, then the bus scan.
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef CAN_RAW_H
#define CAN_RAW_H

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "can_members.h"
#include "can_io.h"
#include "mqtt_publish.h"
#include "bus_scan.h"

#ifdef HA_CAN_DEBUG_true

static constexpr unsigned long RAW_TIMEOUT_MS = 1000;      // answer to a read request
static constexpr unsigned long RAW_ACK_WINDOW_MS = 1000;   // window for an ack frame after a write

enum class RawPhase : uint8_t { NONE, READ, ACK, READBACK };

struct RawOp {
    RawPhase phase = RawPhase::NONE;
    uint32_t target = 0;                 // CAN id of the member asked
    uint16_t index = 0;
    uint8_t sw = ELSTER_SWITCH_VALUE;    // read: switch byte of the request
    uint16_t value = 0;                  // write only
    unsigned long sentMs = 0;            // start of the current phase
    int ackType = -1;                    // write: msg_type of the first ack frame (2..5)
};
static RawOp rawOp;

inline bool rawOpActive() { return rawOp.phase != RawPhase::NONE; }

inline const char* rawSwitchName(uint8_t sw) {
    return sw == ELSTER_SWITCH_MIN ? "f8" : sw == ELSTER_SWITCH_MAX ? "f9" : "fa";
}

// "fa", "f8" or "f9" (any case) into sw
inline bool rawParseSwitch(const std::string& tok, uint8_t& sw) {
    std::string lower = tok;
    for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (lower == "fa") sw = ELSTER_SWITCH_VALUE;
    else if (lower == "f8") sw = ELSTER_SWITCH_MIN;
    else if (lower == "f9") sw = ELSTER_SWITCH_MAX;
    else return false;
    return true;
}

// Index and value of an answer to a request with switch byte sw (layouts as in bus_scan.h).
// False if the frame does not answer that request (other switch byte, limit answer).
inline bool rawDecodeAnswer(const std::vector<uint8_t>& msg, uint8_t sw, uint16_t& index, uint16_t& raw) {
    if (sw == ELSTER_SWITCH_VALUE) {
        if (msg[2] == ELSTER_SWITCH_MIN || msg[2] == ELSTER_SWITCH_MAX) return false;
        if (msg[2] == ELSTER_SWITCH_VALUE) {
            index = static_cast<uint16_t>((msg[3] << 8) | msg[4]);
            raw = static_cast<uint16_t>((msg[5] << 8) | msg[6]);
        } else {
            index = msg[2];
            raw = static_cast<uint16_t>((msg[3] << 8) | msg[4]);
        }
    } else {
        if (msg[2] != sw) return false;
        index = static_cast<uint16_t>((msg[3] << 8) | msg[4]);
        raw = static_cast<uint16_t>((msg[5] << 8) | msg[6]);
    }
    return true;
}

static void rawPublish(const char* json) {
    mqttPublish(HA_TOPIC_ROOT "/raw/result", json, strlen(json), false);
}

// Read result; raw is part of the JSON only with hasRaw (not for busy, timeout)
static void rawPublishRead(uint32_t target, uint16_t index, uint8_t sw, const char* status, bool hasRaw,
                           uint16_t raw) {
    char rawPart[24] = "";
    if (hasRaw) snprintf(rawPart, sizeof(rawPart), ",\"raw\":\"0x%04x\"", static_cast<unsigned>(raw));
    char buf[160];
    snprintf(buf, sizeof(buf),
             "{\"op\":\"read\",\"id\":\"0x%03x\",\"index\":\"0x%04x\",\"switch\":\"%s\"%s,\"status\":\"%s\"}",
             static_cast<unsigned>(target), static_cast<unsigned>(index), rawSwitchName(sw), rawPart, status);
    rawPublish(buf);
}

// Write result; ack < 0 and hasReadback false leave those fields out
static void rawPublishWrite(uint32_t target, uint16_t index, uint16_t value, const char* status, int ack,
                            bool hasReadback, uint16_t readback) {
    char ackPart[24] = "";
    char rbPart[24] = "";
    if (ack >= 0) snprintf(ackPart, sizeof(ackPart), ",\"ack\":%d", ack);
    if (hasReadback) snprintf(rbPart, sizeof(rbPart), ",\"readback\":\"0x%04x\"", static_cast<unsigned>(readback));
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"op\":\"write\",\"id\":\"0x%03x\",\"index\":\"0x%04x\",\"value\":\"0x%04x\",\"status\":\"%s\"%s%s}",
             static_cast<unsigned>(target), static_cast<unsigned>(index), static_cast<unsigned>(value), status,
             ackPart, rbPart);
    rawPublish(buf);
}

// End of a write: the read-back decides verified / mismatch, otherwise ack tells timeout / no_ack
static void rawFinishWrite(uint16_t readback, bool hasReadback) {
    const bool valueSeen = hasReadback && readback != 0x8000;
    const char* status;
    if (valueSeen) status = (readback == rawOp.value) ? "verified" : "mismatch";
    else status = rawOp.ackType >= 0 ? "timeout" : "no_ack";
    rawPublishWrite(rawOp.target, rawOp.index, rawOp.value, status, rawOp.ackType, valueSeen, readback);
    rawOp = RawOp{};
}

// <root>/raw/read: payload "0xID 0xIDX [fa|f8|f9]"
void rawReadCommand(const std::string& payload) {
    const std::vector<std::string> tok = splitScanTokens(payload);
    uint32_t target = 0, index = 0;
    uint8_t sw = ELSTER_SWITCH_VALUE;
    bool ok = (tok.size() == 2 || tok.size() == 3) && parseScanHex(tok[0], 0x7FF, target) &&
              parseScanHex(tok[1], 0xFFFF, index);
    if (ok && tok.size() == 3) ok = rawParseSwitch(tok[2], sw);
    if (!ok) {
        ESP_LOGW("RAW", "Invalid raw read \"%s\" (expected \"0xID 0xIDX [fa|f8|f9]\")", payload.c_str());
        return;
    }
    if (target == CanMembers[cm_pc].CanId) {
        ESP_LOGW("RAW", "Raw read of PC (0x%03x) refused", static_cast<unsigned>(target));
        return;
    }
    if (scanState.active || rawOpActive() || disableCanRequests) {
        ESP_LOGW("RAW", "Raw read refused: bus scan, raw operation or passive mode running");
        rawPublishRead(target, static_cast<uint16_t>(index), sw, "busy", false, 0);
        return;
    }
    rawOp = RawOp{};
    rawOp.phase = RawPhase::READ;
    rawOp.target = target;
    rawOp.index = static_cast<uint16_t>(index);
    rawOp.sw = sw;
    rawOp.sentMs = millis();
    CanMember probe{"?", target};
    readSignalRaw(&probe, rawOp.index, "RAW", sw);
}

// <root>/raw/write: payload "0xID 0xIDX 0xVALUE"
void rawWriteCommand(const std::string& payload) {
    const std::vector<std::string> tok = splitScanTokens(payload);
    uint32_t target = 0, index = 0, value = 0;
    const bool ok = tok.size() == 3 && parseScanHex(tok[0], 0x7FF, target) &&
                    parseScanHex(tok[1], 0xFFFF, index) && parseScanHex(tok[2], 0xFFFF, value);
    if (!ok) {
        ESP_LOGW("RAW", "Invalid raw write \"%s\" (expected \"0xID 0xIDX 0xVALUE\")", payload.c_str());
        return;
    }
    const uint16_t idx16 = static_cast<uint16_t>(index);
    const uint16_t val16 = static_cast<uint16_t>(value);
    const uint8_t lowByte = static_cast<uint8_t>(idx16 & 0xFF);
    // Checks in this order; every refusal publishes "rejected" and sends nothing
#ifndef HA_CAN_RAW_WRITE_true
    ESP_LOGW("RAW", "can_raw_write not enabled (substitution can_raw_write: \"true\")");
    rawPublishWrite(target, idx16, val16, "rejected", -1, false, 0);
    return;
#endif
#ifdef HA_READ_ONLY_true
    ESP_LOGW("RAW", "read_only: not writing raw 0x%04x to 0x%03x", static_cast<unsigned>(idx16),
             static_cast<unsigned>(target));
    rawPublishWrite(target, idx16, val16, "rejected", -1, false, 0);
    return;
#endif
    if (target == CanMembers[cm_pc].CanId) {
        ESP_LOGW("RAW", "Raw write to PC (0x%03x) refused", static_cast<unsigned>(target));
        rawPublishWrite(target, idx16, val16, "rejected", -1, false, 0);
        return;
    }
    // Index 0xF7..0xFA with value 0x8000: refused by the safety rule of the debug firmware
    if (lowByte >= 0xF7 && lowByte <= 0xFA && val16 == 0x8000) {
        ESP_LOGW("RAW", "Raw write of 0x8000 to index 0x%04x refused", static_cast<unsigned>(idx16));
        rawPublishWrite(target, idx16, val16, "rejected", -1, false, 0);
        return;
    }
    if (scanState.active || rawOpActive() || disableCanRequests) {
        ESP_LOGW("RAW", "Raw write refused: bus scan, raw operation or passive mode running");
        rawPublishWrite(target, idx16, val16, "rejected", -1, false, 0);
        return;
    }
    rawOp = RawOp{};
    rawOp.phase = RawPhase::ACK;
    rawOp.target = target;
    rawOp.index = idx16;
    rawOp.value = val16;
    rawOp.sentMs = millis();
    const std::vector<uint8_t> data = buildWriteFrame(static_cast<uint16_t>(target), idx16, val16);
    ESP_LOGI("RAW", "Raw write 0x%04x = 0x%04x to 0x%03x", static_cast<unsigned>(idx16),
             static_cast<unsigned>(val16), static_cast<unsigned>(target));
    id(my_can).send_data(CanMembers[cm_pc].CanId, false, data);
    notifyCanTx(CanMembers[cm_pc].CanId, data);
}

// Called every 50 ms (common.yaml): ack window, timeouts
void rawStep() {
    if (!rawOpActive()) return;
    const unsigned long now = millis();
    if (rawOp.phase == RawPhase::ACK) {
        if (now - rawOp.sentMs >= RAW_ACK_WINDOW_MS) {
            // Read-back after the ack window
            rawOp.phase = RawPhase::READBACK;
            rawOp.sentMs = now;
            CanMember probe{"?", rawOp.target};
            readSignalRaw(&probe, rawOp.index, "RAW", ELSTER_SWITCH_VALUE);
        }
        return;
    }
    if (now - rawOp.sentMs < RAW_TIMEOUT_MS) return;
    if (rawOp.phase == RawPhase::READ) {
        rawPublishRead(rawOp.target, rawOp.index, rawOp.sw, "timeout", false, 0);
        rawOp = RawOp{};
    } else {
        rawFinishWrite(0, false);
    }
}

// Raw answers and ack frames, before bus_scan.h. Returns true if the frame was consumed.
// Ack frames of a write (types 3, 4, 5, and type 2 with the same index) are only recorded.
bool handleRawFrame(uint32_t canId, const std::vector<uint8_t>& msg) {
    if (!rawOpActive() || msg.size() < 7) return false;
    const uint8_t type = msg[0] & 0x0F;
    if (rawOp.phase == RawPhase::ACK) {
        if (canId != rawOp.target) return false;
        if (rawOp.ackType < 0) {
            if (type == 3 || type == 4 || type == 5) {
                rawOp.ackType = type;
            } else if (type == 2 && scanAnswerIsForPc(msg)) {
                uint16_t index = 0, raw = 0;
                if (rawDecodeAnswer(msg, ELSTER_SWITCH_VALUE, index, raw) && index == rawOp.index) rawOp.ackType = 2;
            }
        }
        return false;
    }
    if (rawOp.phase != RawPhase::READ && rawOp.phase != RawPhase::READBACK) return false;
    if (canId != rawOp.target || type != 2 || !scanAnswerIsForPc(msg)) return false;
    const uint8_t sw = (rawOp.phase == RawPhase::READ) ? rawOp.sw : ELSTER_SWITCH_VALUE;
    uint16_t index = 0, raw = 0;
    if (!rawDecodeAnswer(msg, sw, index, raw) || index != rawOp.index) return false;
    if (rawOp.phase == RawPhase::READ) {
        rawPublishRead(rawOp.target, rawOp.index, rawOp.sw, raw == 0x8000 ? "unsupported" : "ok", true, raw);
        rawOp = RawOp{};
    } else {
        rawFinishWrite(raw, true);
    }
    return true;
}

#else
inline void rawReadCommand(const std::string&) {
    ESP_LOGW("RAW", "Raw read not built in (substitution can_debug: \"true\" builds it in)");
}
inline void rawWriteCommand(const std::string&) {
    ESP_LOGW("RAW", "Raw write not built in (substitution can_debug: \"true\" builds it in)");
}
inline void rawStep() {}
#endif  // HA_CAN_DEBUG_true

// One entry point for bus analysis answers, called from processAndUpdate(): raw operation first,
// so that its answer is not lost to the bus scan (the scan is idle while a raw operation runs).
#ifdef HA_CAN_DEBUG_true
bool handleBusAnalysisFrame(uint32_t canId, const std::vector<uint8_t>& msg) {
    if (handleRawFrame(canId, msg)) return true;
    return handleScanFrame(canId, msg);
}
#else
inline bool handleBusAnalysisFrame(uint32_t canId, const std::vector<uint8_t>& msg) {
    return handleScanFrame(canId, msg);
}
#endif

#endif  // CAN_RAW_H

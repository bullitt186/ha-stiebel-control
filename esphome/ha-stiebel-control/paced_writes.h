/*
 * paced_writes.h - queued writes for SG Ready and the time programs, one at a time and
 * checked by read-back (#85, #109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef PACED_WRITES_H
#define PACED_WRITES_H

#include <cstdio>
#include <cstring>
#include <string>
#include "can_io.h"

// ============================================================================
// PACED WRITES (SG Ready, DHW program)
// ============================================================================
// The WPMW II manager loses a write that follows the previous one within milliseconds:
// on 13.11.2026 the second of three back-to-back SG Ready writes was lost twice (#85).
// Queued writes go out one at a time with a gap. Each is checked against the settled
// read-back and sent once more if the heat pump still reports another value.
static constexpr uint32_t PACED_WRITE_GAP_MS = 1500;
static constexpr uint32_t PACED_WRITE_VERIFY_MS = READBACK_SETTLE_MS + 1500;
static constexpr uint8_t PACED_WRITE_TRIES = 2;
static constexpr size_t PACED_WRITE_QUEUE_LEN = 32;
struct PacedWrite {
    const CanMember* writeCm;
    const CanMember* readCm;
    const ElsterIndex* ei;
    char value[24];
    uint8_t tries;          // 0 = waiting, else sends so far
    uint32_t sentMs;
    bool haveReadBack;
    char readBack[24];
};
static PacedWrite pacedWrites[PACED_WRITE_QUEUE_LEN];
static size_t pacedWriteCount = 0;
static uint32_t pacedLastSendMs = 0;
static bool pacedSentBefore = false;

inline bool queuePacedWrite(const CanMember* writeCm, const CanMember* readCm,
                            const ElsterIndex* ei, const char* value) {
    if (!ei) return false;
#ifdef HA_READ_ONLY_true
    ESP_LOGW("WRITE_Q", "read_only: not writing \"%s\" = \"%s\"", ei->Name, value);
    return false;
#endif
    // Clamp here already, so the read-back is compared with the value that goes on the bus (#79)
    char clamped[16];
    value = filterWriteValue(writeCm, ei, value, clamped, sizeof(clamped));
    // A newer value for a write still waiting replaces it (the one in flight stays)
    for (size_t i = 0; i < pacedWriteCount; i++) {
        PacedWrite& p = pacedWrites[i];
        if (p.tries == 0 && p.ei == ei && p.writeCm == writeCm) {
            snprintf(p.value, sizeof(p.value), "%s", value);
            return true;
        }
    }
    if (pacedWriteCount == PACED_WRITE_QUEUE_LEN) {
        ESP_LOGW("WRITE_Q", "queue full, dropping %s = %s", ei->Name, value);
        return false;
    }
    PacedWrite& p = pacedWrites[pacedWriteCount++];
    p = PacedWrite{};
    p.writeCm = writeCm;
    p.readCm = readCm;
    p.ei = ei;
    snprintf(p.value, sizeof(p.value), "%s", value);
    ESP_LOGI("WRITE_Q", "queued %s %s = %s", writeCm->Name, ei->Name, value);
    return true;
}

// Same encoded value? Compares what would go on the bus ("53" = "53.0"); a time domain
// read back as "not used time domain" equals an empty value.
inline bool pacedValueMatches(const ElsterIndex* ei, const char* want, const char* got) {
    std::string g(got);
    if (ei->Type == et_time_domain && g.find(':') == std::string::npos) g.clear();
    const char* a = want;
    const char* b = g.c_str();
    const int ea = TranslateString(a, ei->Type);
    const int eb = TranslateString(b, ei->Type);
    if (ea == -1 || eb == -1) return strcmp(want, g.c_str()) == 0;
    return ea == eb;
}

// Called for every decoded value: keeps the newest read-back of the write in flight.
inline void pacedWriteNoteValue(const CanMember& cm, const ElsterIndex* ei, const std::string& value) {
    if (pacedWriteCount == 0) return;
    PacedWrite& p = pacedWrites[0];
    if (p.tries == 0 || p.ei->Index != ei->Index || p.readCm->CanId != cm.CanId || value == "-255") return;
    snprintf(p.readBack, sizeof(p.readBack), "%s", value.c_str());
    p.haveReadBack = true;
}

inline void sendPacedWrite(PacedWrite& p) {
    const char* v = p.value;
    writeSignal(p.writeCm, p.ei, v);   // also schedules the settled read-back on writeCm
    if (p.readCm != p.writeCm) scheduleReadback(p.readCm, p.ei, READBACK_SETTLE_MS);
    p.tries++;
    p.sentMs = millis();
    p.haveReadBack = false;
    pacedLastSendMs = p.sentMs;
    pacedSentBefore = true;
}

// Called every 250 ms (common.yaml): sends the next write once the gap has passed and
// checks the one in flight once its settled read-back is due.
inline void pacedWriteStep() {
    if (pacedWriteCount == 0) return;
    const uint32_t now = millis();
    PacedWrite& p = pacedWrites[0];
    if (p.tries == 0) {
        if (pacedSentBefore && now - pacedLastSendMs < PACED_WRITE_GAP_MS) return;
        sendPacedWrite(p);
        return;
    }
    if (now - p.sentMs < PACED_WRITE_VERIFY_MS) return;
    const bool ok = p.haveReadBack && pacedValueMatches(p.ei, p.value, p.readBack);
    if (!ok && p.tries < PACED_WRITE_TRIES) {
        ESP_LOGW("WRITE_Q", "%s %s = %s not confirmed (read \"%s\"), sending again",
                 p.writeCm->Name, p.ei->Name, p.value, p.haveReadBack ? p.readBack : "nothing");
        sendPacedWrite(p);
        return;
    }
    if (ok) {
        ESP_LOGI("WRITE_Q", "%s %s = %s confirmed", p.writeCm->Name, p.ei->Name, p.value);
    } else {
        ESP_LOGW("WRITE_Q", "%s %s = %s failed after %u sends (read \"%s\")", p.writeCm->Name, p.ei->Name,
                 p.value, (unsigned)p.tries, p.haveReadBack ? p.readBack : "nothing");
    }
    for (size_t j = 1; j < pacedWriteCount; j++) pacedWrites[j - 1] = pacedWrites[j];
    pacedWriteCount--;
}

#endif  // PACED_WRITES_H

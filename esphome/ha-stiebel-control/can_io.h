/*
 * can_io.h - Elster frames on the CAN bus: decode a received frame, send read and write
 * requests, the read-back after a write and the clock/date writes (#109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 * Sends with id(my_can).
 */

#ifndef CAN_IO_H
#define CAN_IO_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "ElsterTable.h"
#include "KElsterTable.h"
#include "can_members.h"
#include "signal_route.h"
#include "mqtt_publish.h"

// Decode buffer for SetValueType(): its longest texts are 20 characters plus the
// terminator ("not used time domain", "ERR MIN UEBERHITZUNG").
static constexpr size_t ELSTER_VALUE_BUF_SIZE = 32;

// PASSIVE LISTEN MODE: wenn true, sendet processSignalRequests() NICHTS.
// Wird von der YAML-Konfiguration (heatingpump_scan.yaml) per on_boot gesetzt,
// damit die Scan-Firmware rein passiv auf dem Bus lauscht.
static bool disableCanRequests = false;

/**
 * Simple struct for CAN ID bytes (avoids heap allocation)
 */
struct CanIdBytes {
    uint8_t first;
    uint8_t second;
};

/**
 * Generate CAN read ID from member CAN ID
 */
// Elster frame header: data[0] = (id & 0x780) >> 3 plus the message type, data[1] = the lower
// 7 bits of the receiver id. With only 3 bits a request to 0x514 (HEIZMODUL_WPL) went to 0x504.
CanIdBytes generate_read_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>((address & 0xF0) + 1), static_cast<uint8_t>(can_id & 0x7F)};
}

CanIdBytes generate_write_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>(address & 0xF0), static_cast<uint8_t>(can_id & 0x7F)};
}

const CanMember &lookupCanMember(uint32_t canId)
{
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i) {
        if (CanMembers[i].CanId == canId) {
            return CanMembers[i];
        }
    }
    // Return the last element if member is not found
    return CanMembers[sizeof(CanMembers) / sizeof(CanMembers[0]) - 1];
}

// Check if signal is permanently blacklisted (lookup in ElsterTable)
inline bool isPermanentlyBlacklisted(const char* signalName) {
    const ElsterIndex *ei = GetElsterIndex(signalName);
    return ei->isBlacklisted;
}

const ElsterIndex *processCanMessage(const std::vector<uint8_t> &msg, uint32_t can_id, std::string &signalValue, const CanMember **outCanMember)
{
    // Return if the message is too small
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const CanMember &cm = lookupCanMember(can_id);
    *outCanMember = &cm;  // Return CanMember to caller

    const ElsterIndex *ei;
    uint8_t byte1;
    uint8_t byte2;
    char charValue[ELSTER_VALUE_BUF_SIZE];

    if (msg[2] == 0xfa)
    {
        byte1 = msg[5];
        byte2 = msg[6];
        ei = GetElsterIndex(msg[4] + (msg[3] << 8));
    }
    else
    {
        byte1 = msg[3];
        byte2 = msg[4];
        ei = GetElsterIndex(msg[2]);
    }

    switch (ei->Type)
    {
    case et_double_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    case et_triple_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    default:
        SetValueType(charValue, ei->Type, static_cast<int>(byte2 + (byte1 << 8)));
        break;
    }

    // Per-frame log: DEBUG only (compiled out at the default INFO level)
    ESP_LOGD("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(idx=0x%04x, raw=0x%04x, %s)", cm.Name, (unsigned)can_id, ei->Name, charValue,
             (unsigned)((msg[2] == 0xfa) ? (msg[4] + (msg[3] << 8)) : msg[2]), (unsigned)(byte2 + (byte1 << 8)), ElsterTypeToName(ei->Type));

    signalValue = charValue;
    return ei;
}

// ============================================================================
// NON-BLOCKING READ-BACK AFTER A WRITE (replaces delay(100) in the SG Ready path)
// ============================================================================
struct PendingReadback {
    const CanMember* cm;
    const ElsterIndex* ei;
    uint32_t dueMs;
};
static constexpr size_t READBACK_QUEUE_LEN = 16;
static constexpr uint32_t READBACK_DELAY_MS = 100;
// Second read after every write: the WPMW II often still answers the old value right
// after a write, about 4 s later the new one (#68).
static constexpr uint32_t READBACK_SETTLE_MS = 4000;
static PendingReadback pendingReadbacks[READBACK_QUEUE_LEN];
static size_t pendingReadbackCount = 0;

void readSignal(const CanMember *cm, const ElsterIndex *ei);

// Queues a read request delayMs after a write. A second request for the same signal
// within a second replaces the first; the early and the settle read stay separate.
inline void scheduleReadback(const CanMember* cm, const ElsterIndex* ei,
                             uint32_t delayMs = READBACK_DELAY_MS) {
    const uint32_t due = millis() + delayMs;
    for (size_t i = 0; i < pendingReadbackCount; i++) {
        const int32_t apart = static_cast<int32_t>(pendingReadbacks[i].dueMs - due);
        if (pendingReadbacks[i].cm == cm && pendingReadbacks[i].ei == ei &&
            apart > -1000 && apart < 1000) {
            pendingReadbacks[i].dueMs = due;
            return;
        }
    }
    if (pendingReadbackCount == READBACK_QUEUE_LEN) {
        readSignal(cm, ei);   // queue full (never seen): read at once instead of losing it
        return;
    }
    pendingReadbacks[pendingReadbackCount++] = {cm, ei, due};
}

// Sends at most one due read-back per call (250 ms interval in common.yaml), so a
// SG Ready change with three writes does not burst three reads onto the bus.
inline void pendingReadbackStep() {
    const uint32_t now = millis();
    for (size_t i = 0; i < pendingReadbackCount; i++) {
        if (static_cast<int32_t>(now - pendingReadbacks[i].dueMs) < 0) continue;
        PendingReadback r = pendingReadbacks[i];
        for (size_t j = i + 1; j < pendingReadbackCount; j++) pendingReadbacks[j - 1] = pendingReadbacks[j];
        pendingReadbackCount--;
        readSignal(r.cm, r.ei);
        return;
    }
}

// Read request for a raw Elster index (also indices unknown to ElsterTable, bus scan)
// Elster switch byte in data[2]: 0xFA = current value, 0xF8 = minimum, 0xF9 = maximum (#79).
// The answer to 0xF8/0xF9 uses the long layout [id1, id2, switch, idx_hi, idx_lo, val_hi, val_lo].
static constexpr uint8_t ELSTER_SWITCH_MIN = 0xF8;
static constexpr uint8_t ELSTER_SWITCH_MAX = 0xF9;
static constexpr uint8_t ELSTER_SWITCH_VALUE = 0xFA;

// Short form [id1, id2, idx_lo, ...] only for a plain value read of an index below 0x100.
// Indices 0xF7..0xFA would be taken as a switch byte there, so they always use the long form.
inline bool elsterShortForm(uint16_t index, uint8_t switchByte = ELSTER_SWITCH_VALUE) {
    const uint8_t lo = static_cast<uint8_t>(index & 0xFF);
    return switchByte == ELSTER_SWITCH_VALUE && (index >> 8) == 0 && !(lo >= 0xF7 && lo <= 0xFA);
}

void readSignalRaw(const CanMember *cm, uint16_t index, [[maybe_unused]] const char *name = "?",
                   uint8_t switchByte = ELSTER_SWITCH_VALUE)
{
    constexpr bool use_extended_id = false; // No use of extended ID
    const uint8_t IndexByte1 = static_cast<uint8_t>(index >> 8);
    const uint8_t IndexByte2 = static_cast<uint8_t>(index & 0xFF);
    std::vector<uint8_t> data;
    CanIdBytes readId = generate_read_id(cm->CanId);

    if (elsterShortForm(index, switchByte))
    {
        data = {readId.first,
                readId.second,
                IndexByte2,
                0x00,
                0x00,
                0x00,
                0x00};
    }
    else
    {
        data = {readId.first,
                readId.second,
                switchByte,
                IndexByte1,
                IndexByte2,
                0x00,
                0x00};
    }

    ESP_LOGD("readSignal()", "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             name, index, cm->Name, (unsigned)cm->CanId, readId.first, readId.second,
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void readSignal(const CanMember *cm, const ElsterIndex *ei)
{
    readSignalRaw(cm, ei->Index, ei->Name);
}

void readSignal(const CanMember *cm, const char *elsterName)
{
    readSignal(cm, GetElsterIndex(elsterName));
    return;
}

// Filter for numeric writes (#79): writables.h installs the clamp to the device limits of the
// writable numbers. Injected instead of called, because the writable tables sit above this module.
// Returns true and fills out when it changed the value.
using WriteValueFilter = bool (*)(const CanMember* cm, const ElsterIndex* ei, const char* in,
                                  char* out, size_t outSize);
static WriteValueFilter writeValueFilter = nullptr;

inline const char* filterWriteValue(const CanMember* cm, const ElsterIndex* ei, const char* in,
                                    char* buf, size_t bufSize) {
    return (writeValueFilter && writeValueFilter(cm, ei, in, buf, bufSize)) ? buf : in;
}

void writeSignal(const CanMember *cm, const ElsterIndex *ei, const char *&strIn)
{
#ifdef HA_READ_ONLY_true
    ESP_LOGW("writeSignal()", "read_only: not writing \"%s\" = \"%s\" to %s", ei->Name, strIn, cm->Name);
    return;
#endif
    char clamped[16];
    const char* str = filterWriteValue(cm, ei, strIn, clamped, sizeof(clamped));
    bool use_extended_id = false;
    int writeValue = TranslateString(str, ei->Type);
    if (writeValue == -1) {
        ESP_LOGW("writeSignal()", "TranslateString failed for \"%s\" (input: \"%s\") — refusing to write", ei->Name, str);
        return;
    }
    uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    CanIdBytes writeId = generate_write_id(cm->CanId);

    if (elsterShortForm(ei->Index))
    {
        data = {writeId.first,
                writeId.second,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF),
                0x00,
                0x00};
    }
    else
    {
        data = {writeId.first,
                writeId.second,
                0xfa,
                IndexByte1,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF)};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             ei->Name, ei->Index, writeValue, cm->Name, (unsigned)cm->CanId, writeId.first, writeId.second,
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("writeSignal()", "%s", logmsg);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
    // Command echo: publish the read-back even if the value did not change
    forceNextStatePublish(*cm, ei);
    // The immediate read-back may still return the old value; read again once settled
    scheduleReadback(cm, ei, READBACK_SETTLE_MS);
}

void writeSignal(const CanMember *cm, const char *elsterName, const char *str)
{
    const char* strCopy = str;  // Create a non-const pointer we can pass by reference
    writeSignal(cm, GetElsterIndex(elsterName), strCopy);
    return;
}

// Use the same route for MQTT commands and the WPL9 SG Ready adapter.
// Explicit read/writeSignal calls keep their original low-level semantics.
void writeConfiguredSignal(CanMemberType fallback, const char* name, const char* value) {
    const ElsterIndex* ei = GetElsterIndex(name);
    const SignalRoute route = resolveSignalRoute(ei->Index, fallback);
    writeSignal(&CanMembers[route.writeMember], name, value);
    readSignal(&CanMembers[route.readMember], ei);
}

void updateTime(CanMember cm, const char *str_time)
{
    ESP_LOGI("WRITE UHRZEIT VIA BUTTON", "%s", str_time);

    // WPMW II: the manager ignores the single STUNDE/MINUTE/SEKUNDE writes but accepts UHRZEIT
    // (0x0009, hour and minute in one value); the heat pump takes the time over and the seconds
    // restart at 0. Confirmed on a TTL 33 with WPMW II, software 6518 (#68). While the clock menu
    // is open on the display, the display shows "--.--" until the menu is left.
    if (isWpmw2Model()) {
        char hhmm[6];
        strncpy(hhmm, str_time, 5);   // "HH:MM:SS" from input_datetime -> "HH:MM"
        hhmm[5] = '\0';
        ESP_LOGI("WRITE", "UHRZEIT: %s", hhmm);
        writeSignal(&cm, "UHRZEIT", hhmm);
        readSignal(&cm, "UHRZEIT");
        return;
    }

    char stunde[3];
    char minute[3];
    char sekunde[3];

    strncpy(stunde, str_time, sizeof(stunde) - 1);
    stunde[2] = '\0';
    strncpy(minute, str_time + 3, sizeof(minute) - 1);
    minute[2] = '\0';
    strncpy(sekunde, str_time + 6, sizeof(sekunde) - 1);
    sekunde[2] = '\0';
    const char *cstunde = stunde;
    const char *cminute = minute;
    const char *csekunde = sekunde;
    ESP_LOGI("WRITE", "Stunde: %s, Minute: %s, Sekunde: %s", cstunde, cminute, csekunde);
    writeSignal(&cm, "STUNDE", cstunde);
    readSignal(&cm, "STUNDE");
    writeSignal(&cm, "MINUTE", cminute);
    readSignal(&cm, "MINUTE");
    writeSignal(&cm, "SEKUNDE", csekunde);
    readSignal(&cm, "SEKUNDE");
}

void updateDate(CanMember cm, const char *str_date)
{
    ESP_LOGI("WRITE DATUM VIA BUTTON", "%s", str_date);

    // WPMW II: DATUM (0x000a, day and month in one value) works but resets JAHR to 0 (2000);
    // writing JAHR (two digits) right after DATUM fixes it, and the manager recalculates the
    // weekday itself. Confirmed on a TTL 33 with WPMW II, software 6518 (#68).
    if (isWpmw2Model()) {
        char ddmm[7];   // "YYYY-MM-DD" from input_datetime -> "DD.MM."
        ddmm[0] = str_date[8]; ddmm[1] = str_date[9]; ddmm[2] = '.';
        ddmm[3] = str_date[5]; ddmm[4] = str_date[6]; ddmm[5] = '.';
        ddmm[6] = '\0';
        char yy[3] = {str_date[2], str_date[3], '\0'};
        ESP_LOGI("WRITE", "DATUM: %s, JAHR: %s", ddmm, yy);
        writeSignal(&cm, "DATUM", ddmm);
        writeSignal(&cm, "JAHR", yy);
        readSignal(&cm, "DATUM");
        readSignal(&cm, "JAHR");
        return;
    }
    char year[3];
    char month[3];
    char day[3];

    strncpy(year, str_date + 2, sizeof(year) - 1);
    year[2] = '\0';
    strncpy(month, str_date + 5, sizeof(month) - 1);
    month[2] = '\0';
    strncpy(day, str_date + 8, sizeof(day) - 1);
    day[2] = '\0';

    const char *cyear = year;
    const char *cmonth = month;
    const char *cday = day;
    ESP_LOGI("WRITE", "Year: %s, Month: %s, Day: %s", cyear, cmonth, cday);
    writeSignal(&cm, "JAHR", cyear);
    readSignal(&cm, "JAHR");
    writeSignal(&cm, "MONAT", cmonth);
    readSignal(&cm, "MONAT");
    writeSignal(&cm, "TAG", cday);
    readSignal(&cm, "TAG");
}

#endif  // CAN_IO_H

/*
 * bus_scan.h - bus scan of one CAN member (MQTT <root>/scan/start, <root>/scan/stop) (#109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef BUS_SCAN_H
#define BUS_SCAN_H

#include <cstdio>
#include <cstring>
#include <string>
#include <strings.h>
#include <vector>
#include "ElsterTable.h"
#include "can_io.h"
#include "mqtt_publish.h"

// ============================================================================
// BUS SCAN (MQTT heatingpump/scan/start "<MEMBER> [from to]", heatingpump/scan/stop)
// ============================================================================
// Asks one CAN member for every ElsterTable index (or a hex range) and publishes
// each index that answers with a real value (not 0x8000) to
// heatingpump/scan/<MEMBER>/<idx> (retained). One request per SCAN_GAP_MS; normal
// polling pauses meanwhile. Results go out at once, nothing is buffered.
static constexpr unsigned long SCAN_GAP_MS = 150;
static constexpr unsigned long SCAN_TAIL_MS = 1000;  // wait for the last answers
struct ScanState {
    bool active = false;
    const CanMember* cm = nullptr;
    bool table = true;
    uint32_t begin = 0, pos = 0, end = 0;
    unsigned long lastSend = 0, startMs = 0;
    uint16_t hits = 0;
};
static ScanState scanState;

void publishScanStatus(const char* state) {
    char buf[160];
    uint32_t total = scanState.end - scanState.begin + 1;
    uint32_t done = scanState.pos - scanState.begin;
    snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"member\":\"%s\",\"done\":%u,\"total\":%u,\"hits\":%u,\"seconds\":%lu}",
             state, scanState.cm ? scanState.cm->Name : "", (unsigned)done, (unsigned)total, scanState.hits,
             (millis() - scanState.startMs) / 1000UL);
    mqttPublish(HA_TOPIC_ROOT "/scan/status", buf, strlen(buf));
}

// payload: "HEIZMODUL" (all table indices) or "HEIZMODUL 0x0000 0x0fff" (range)
void startScan(const std::string& payload) {
    char member[24] = {0};
    unsigned from = 0, to = 0;
    int n = sscanf(payload.c_str(), "%23s %x %x", member, &from, &to);
    const CanMember* cm = nullptr;
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
        if (i != cm_pc && strcasecmp(CanMembers[i].Name, member) == 0) cm = &CanMembers[i];
    if (n < 1 || !cm || (n == 3 && (to < from || to > 0xFFFF))) {
        ESP_LOGW("SCAN", "Invalid scan request \"%s\" (expected \"<MEMBER> [from to]\")", payload.c_str());
        return;
    }
    scanState = ScanState{};
    scanState.active = true;
    scanState.cm = cm;
    scanState.table = (n != 3);
    scanState.begin = scanState.pos = scanState.table ? 0 : from;
    scanState.end = scanState.table ? ElsterTableCount - 1 : to;
    scanState.startMs = millis();
    ESP_LOGI("SCAN", "Scanning %s (%s), normal polling paused", cm->Name, scanState.table ? "table" : "range");
    publishScanStatus("running");
}

void stopScan(const char* state = "stopped") {
    if (!scanState.active) return;
    scanState.active = false;
    ESP_LOGI("SCAN", "Scan %s: %u hits", state, scanState.hits);
    publishScanStatus(state);
}

// Called every SCAN_GAP_MS from common.yaml
void scanStep() {
    if (!scanState.active || disableCanRequests) return;
    unsigned long now = millis();
    if (now - scanState.lastSend < SCAN_GAP_MS) return;
    if (scanState.pos > scanState.end) {
        if (now - scanState.lastSend >= SCAN_TAIL_MS) stopScan("done");
        return;
    }
    uint16_t idx = scanState.table ? ElsterTable[scanState.pos].Index : static_cast<uint16_t>(scanState.pos);
    scanState.pos++;
    readSignalRaw(scanState.cm, idx, "SCAN");
    scanState.lastSend = now;
    if (scanState.pos % 100 == 0) publishScanStatus("running");
}

// Returns true if the frame was a scan answer (then it is not processed further)
bool handleScanFrame(const CanMember& cm, const ElsterIndex* ei, const std::vector<uint8_t>& msg, const std::string& value) {
    if (!scanState.active || &cm != scanState.cm || (msg[0] & 0x0F) != 2) return false;
    bool longIdx = (msg[2] == 0xfa);
    uint16_t idx = longIdx ? (msg[3] << 8) | msg[4] : msg[2];
    uint16_t raw = longIdx ? (msg[5] << 8) | msg[6] : (msg[3] << 8) | msg[4];
    if (raw != 0x8000) {
        scanState.hits++;
        char topic[96], buf[128];
        snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/scan/%s/0x%04x", cm.Name, idx);
        snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"raw\":%u,\"value\":\"%s\"}",
                 ei->Index == idx ? ei->Name : "", raw, value.c_str());
        mqttPublish(topic, buf, strlen(buf));
    }
    return true;
}

#endif  // BUS_SCAN_H

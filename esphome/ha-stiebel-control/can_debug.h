/*
 * can_debug.h - raw CAN frame stream over MQTT for bus analysis (off by default).
 *
 * Every received frame (before any filter of processAndUpdate()) and every frame this node
 * sends (after send_data()) is queued while the stream is on and published in batches:
 *
 *   <root>/can_debug/set     command "on" | "off" (not stored: after a restart the stream is off)
 *   <root>/can_debug/state   "on" | "off", retained
 *   <root>/can_debug/frames  JSON array, not retained, at most CAN_DEBUG_BATCH frames per message:
 *                            [{"t":<ms since boot>,"id":"0x180","d":"<data bytes hex>","tx":false},...]
 *
 * The frames are raw: tools decode them offline (sender id, Elster telegram type, index, value).
 * Hooks into can_io.h via canRxObserver/canTxObserver, installed at static initialisation like
 * the device limit hooks of writables.h (#79).
 */

#ifndef CAN_DEBUG_H
#define CAN_DEBUG_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "config.h"
#include "mqtt_publish.h"
#include "can_io.h"

static constexpr size_t CAN_DEBUG_QUEUE = 256;   // frames buffered between two publishes
static constexpr size_t CAN_DEBUG_BATCH = 32;    // frames per MQTT message

struct CanDebugFrame {
    uint32_t t;        // millis() when queued
    uint16_t id;       // 11-bit CAN id
    uint8_t dlc;
    uint8_t data[8];
    bool tx;           // true = sent by this node
};

static CanDebugFrame canDebugQueue[CAN_DEBUG_QUEUE];
static size_t canDebugHead = 0;            // oldest queued frame
static size_t canDebugCount = 0;
static uint32_t canDebugDropped = 0;       // frames lost because the queue was full (since "on")
static uint32_t canDebugDroppedLogged = 0;
static bool canDebugEnabled = false;
static bool canDebugStatePending = true;   // publish <root>/can_debug/state (boot, command)

inline void canDebugClear() {
    canDebugHead = 0;
    canDebugCount = 0;
    canDebugDropped = 0;
    canDebugDroppedLogged = 0;
}

inline void canDebugPush(uint32_t canId, const std::vector<uint8_t>& data, bool tx) {
    if (!canDebugEnabled) return;
    if (canDebugCount == CAN_DEBUG_QUEUE) {   // keep the older frames, drop the new one
        canDebugDropped++;
        return;
    }
    CanDebugFrame& f = canDebugQueue[(canDebugHead + canDebugCount) % CAN_DEBUG_QUEUE];
    f.t = static_cast<uint32_t>(millis());
    f.id = static_cast<uint16_t>(canId & 0x7FF);
    f.dlc = static_cast<uint8_t>(data.size() > 8 ? 8 : data.size());
    for (size_t i = 0; i < f.dlc; i++) f.data[i] = data[i];
    f.tx = tx;
    canDebugCount++;
}

inline void canDebugOnRx(uint32_t canId, const std::vector<uint8_t>& data) { canDebugPush(canId, data, false); }
inline void canDebugOnTx(uint32_t canId, const std::vector<uint8_t>& data) { canDebugPush(canId, data, true); }

// <root>/can_debug/set: "on" starts with an empty queue, "off" stops and discards the queue.
inline void canDebugCommand(const std::string& payload) {
    if (payload == "on") {
        canDebugClear();
        canDebugEnabled = true;
    } else if (payload == "off") {
        canDebugEnabled = false;
        canDebugClear();
    } else {
        ESP_LOGW("can_debug", "Unknown command '%s' (expected on/off)", payload.c_str());
        return;
    }
    ESP_LOGI("can_debug", "CAN frame stream %s", canDebugEnabled ? "on" : "off");
    canDebugStatePending = true;
}

// Formats up to maxFrames queued frames (oldest first) as JSON array into out.
// Returns the number of frames written; out holds "[]" when none fit.
inline size_t canDebugFormatBatch(char* out, size_t cap, size_t maxFrames) {
    size_t pos = 0;
    size_t n = 0;
    if (cap < 3) return 0;
    out[pos++] = '[';
    while (n < maxFrames && n < canDebugCount) {
        const CanDebugFrame& f = canDebugQueue[(canDebugHead + n) % CAN_DEBUG_QUEUE];
        char hex[17];
        for (size_t i = 0; i < f.dlc; i++) snprintf(hex + 2 * i, 3, "%02x", f.data[i]);
        hex[2 * f.dlc] = '\0';
        const int len = snprintf(out + pos, cap - pos, "%s{\"t\":%lu,\"id\":\"0x%03x\",\"d\":\"%s\",\"tx\":%s}",
                                 n > 0 ? "," : "", static_cast<unsigned long>(f.t), static_cast<unsigned>(f.id),
                                 hex, f.tx ? "true" : "false");
        if (len < 0 || pos + static_cast<size_t>(len) + 2 > cap) break;   // keep room for "]\0"
        pos += static_cast<size_t>(len);
        n++;
    }
    out[pos++] = ']';
    out[pos] = '\0';
    return n;
}

// Called from a short interval (common.yaml): state topic, then one batch of frames.
inline void canDebugStep() {
    if (canDebugStatePending) {
        const char* s = canDebugEnabled ? "on" : "off";
        if (mqttPublish(HA_TOPIC_ROOT "/can_debug/state", s, strlen(s), true)) canDebugStatePending = false;
    }
    if (canDebugDropped != canDebugDroppedLogged) {
        ESP_LOGW("can_debug", "%lu frames dropped (queue full)", static_cast<unsigned long>(canDebugDropped));
        canDebugDroppedLogged = canDebugDropped;
    }
    if (!canDebugEnabled || canDebugCount == 0) return;
    static char buf[CAN_DEBUG_BATCH * 64 + 4];
    const size_t n = canDebugFormatBatch(buf, sizeof(buf), CAN_DEBUG_BATCH);
    if (n == 0) return;
    // Not retained; on failure (e.g. not connected) the frames stay queued for the next call
    if (!mqttPublish(HA_TOPIC_ROOT "/can_debug/frames", buf, strlen(buf), false)) return;
    canDebugHead = (canDebugHead + n) % CAN_DEBUG_QUEUE;
    canDebugCount -= n;
}

[[maybe_unused]] static const bool canDebugHooksInstalled = [] {
    canRxObserver = canDebugOnRx;
    canTxObserver = canDebugOnTx;
    return true;
}();

#endif  // CAN_DEBUG_H

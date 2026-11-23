/*
 * can_debug.h - raw CAN frame stream over MQTT for bus analysis (opt-in, see below).
 *
 * Every received frame (before any filter of processAndUpdate()) and every frame this node
 * sends (after send_data()) is queued while the stream is on and published in batches:
 *
 *   <root>/can_debug/set     command "on" | "off" (not stored: after a restart the stream is off)
 *   <root>/can_debug/state   "on" | "off", retained
 *   <root>/can_debug/frames  JSON array, not retained, at most CAN_DEBUG_BATCH frames per message:
 *                            [{"t":<ms since boot>,"id":"0x180","d":"<data bytes hex>","tx":false},...]
 *
 * Independent of the CAN controller: RX comes from processAndUpdate() (on_frame of both
 * can_esp32.yaml and can_mcp2515.yaml), TX from the canbus send_data() calls in can_io.h.
 * Hooks into can_io.h via canRxObserver/canTxObserver, installed at static initialisation like
 * the device limit hooks of writables.h (#79).
 *
 * RAM: nothing while off. "on" allocates the queue (CAN_DEBUG_QUEUE_SIZE frames x 16 bytes,
 * substitution can_debug_queue_size) and the JSON buffer (CAN_DEBUG_BATCH x 64 bytes) on the
 * heap; "off" frees both. If the allocation fails the stream stays off.
 *
 * Opt-in: the stream and the observers are built in only with substitution can_debug: "true"
 * (-DHA_CAN_DEBUG_true). Default "false": canDebugCommand() only logs that it is not built in,
 * canDebugStep() is empty, and can_io.h compiles the observers out.
 */

#ifndef CAN_DEBUG_H
#define CAN_DEBUG_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include "config.h"
#include "mqtt_publish.h"
#include "can_io.h"

#ifdef HA_CAN_DEBUG_true

// Frames buffered between two publishes (build flag from substitution can_debug_queue_size)
#ifndef CAN_DEBUG_QUEUE_SIZE
#define CAN_DEBUG_QUEUE_SIZE 256
#endif
static constexpr size_t CAN_DEBUG_QUEUE = CAN_DEBUG_QUEUE_SIZE;
static_assert(CAN_DEBUG_QUEUE >= 8, "can_debug_queue_size must be at least 8");
// Frames per MQTT message
static constexpr size_t CAN_DEBUG_BATCH = CAN_DEBUG_QUEUE < 32 ? CAN_DEBUG_QUEUE : 32;
static constexpr size_t CAN_DEBUG_BUF_SIZE = CAN_DEBUG_BATCH * 64 + 4;   // one frame <= 63 chars

struct CanDebugFrame {
    uint32_t t;        // millis() when queued
    uint16_t id;       // 11-bit CAN id
    uint8_t dlc;
    uint8_t data[8];
    bool tx;           // true = sent by this node
};

static CanDebugFrame* canDebugQueue = nullptr;   // heap, only while on
static char* canDebugBuf = nullptr;              // heap, only while on
static size_t canDebugHead = 0;                  // oldest queued frame
static size_t canDebugCount = 0;
static uint32_t canDebugDropped = 0;             // frames lost because the queue was full (since "on")
static uint32_t canDebugDroppedLogged = 0;
static bool canDebugEnabled = false;
static bool canDebugStatePending = true;         // publish <root>/can_debug/state (boot, command)

inline void canDebugClear() {
    canDebugHead = 0;
    canDebugCount = 0;
    canDebugDropped = 0;
    canDebugDroppedLogged = 0;
}

inline void canDebugFree() {
    delete[] canDebugQueue;
    delete[] canDebugBuf;
    canDebugQueue = nullptr;
    canDebugBuf = nullptr;
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

// <root>/can_debug/set: "on" starts with an empty queue, "off" stops and frees the memory.
inline void canDebugCommand(const std::string& payload) {
    if (payload == "on") {
        if (canDebugQueue == nullptr) {
            canDebugQueue = new (std::nothrow) CanDebugFrame[CAN_DEBUG_QUEUE];
            canDebugBuf = new (std::nothrow) char[CAN_DEBUG_BUF_SIZE];
            if (canDebugQueue == nullptr || canDebugBuf == nullptr) {
                canDebugFree();
                ESP_LOGW("can_debug", "Not enough memory for %u frames, stream stays off "
                         "(lower can_debug_queue_size)", static_cast<unsigned>(CAN_DEBUG_QUEUE));
                return;
            }
        }
        canDebugClear();
        canDebugEnabled = true;
    } else if (payload == "off") {
        canDebugEnabled = false;
        canDebugClear();
        canDebugFree();
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
    if (out == nullptr || cap < 3) return 0;
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
    const size_t n = canDebugFormatBatch(canDebugBuf, CAN_DEBUG_BUF_SIZE, CAN_DEBUG_BATCH);
    if (n == 0) return;
    // Not retained; on failure (e.g. not connected) the frames stay queued for the next call
    if (!mqttPublish(HA_TOPIC_ROOT "/can_debug/frames", canDebugBuf, strlen(canDebugBuf), false)) return;
    canDebugHead = (canDebugHead + n) % CAN_DEBUG_QUEUE;
    canDebugCount -= n;
}

[[maybe_unused]] static const bool canDebugHooksInstalled = [] {
    canRxObserver = canDebugOnRx;
    canTxObserver = canDebugOnTx;
    return true;
}();

#else
inline void canDebugCommand(const std::string&) {
    ESP_LOGW("can_debug", "CAN frame stream not built in (substitution can_debug: \"true\" builds it in)");
}
inline void canDebugStep() {}
#endif  // HA_CAN_DEBUG_true
#endif  // CAN_DEBUG_H

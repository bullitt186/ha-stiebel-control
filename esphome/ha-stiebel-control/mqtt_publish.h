/*
 * mqtt_publish.h - MQTT publish with failure counter and the publish-on-change state cache (#55, #109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef MQTT_PUBLISH_H
#define MQTT_PUBLISH_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "config.h"
#include "can_members.h"
#include "KElsterTable.h"
#include "signal_route.h"

// ============================================================================
// MQTT PUBLISH: failure counter, publish-on-change state cache (#55)
// ============================================================================

// Publishes the MQTT client refused while connected (idf_send_async queue full).
// Attempts while disconnected are not counted: they are sent again after the connect.
// Published as diagnostic sensor mqtt_publish_failures.
static uint32_t mqttPublishFailures = 0;

inline bool mqttPublish(const char* topic, const char* payload, size_t len, bool retain = true) {
    bool ok = id(mqtt_client).publish(topic, payload, len, 0, retain);
    if (!ok && id(mqtt_client).is_connected()) mqttPublishFailures++;
    return ok;
}

// One entry per state topic. Raw signals keep member and signal name (the topic
// heatingpump/<member>/<signal>/state is rebuilt on demand), calculated sensors keep
// their static topic string in `topic` with `signal` == nullptr. No heap per publish:
// the value is copied into a fixed buffer, entries are only added for new topics.
struct StateCacheEntry {
    uint32_t hash;
    const char* topic;      // calculated: full topic; raw: member name
    const char* signal;     // raw: signal name; calculated: nullptr
    uint32_t lastPublishMs;
    bool pending;           // newest value not sent yet (new, changed, failed or forced)
    char value[23];
};
static_assert(sizeof(StateCacheEntry::value) >= 16, "state value buffer too small");
// Sorted by hash (binary search); the republish cursor walks it by position.
static std::vector<StateCacheEntry> stateCache;
static bool stateRepublishActive = false;
static size_t stateRepublishPos = 0;
static uint32_t stateSuppressedCount = 0;   // unchanged values not sent (debug/tests)

inline uint32_t rawStateHash(const CanMember& cm, const char* signalName) {
    return elsterNameHash(signalName) ^ (static_cast<uint32_t>(cm.CanId) * 0x9E3779B1u);
}

inline bool stateEntryMatches(const StateCacheEntry& e, const char* topicOrMember, const char* signal) {
    if (signal == nullptr || e.signal == nullptr) {
        return signal == e.signal && (e.topic == topicOrMember || strcmp(e.topic, topicOrMember) == 0);
    }
    return (e.signal == signal || strcmp(e.signal, signal) == 0) &&
           (e.topic == topicOrMember || strcmp(e.topic, topicOrMember) == 0);
}

inline StateCacheEntry* stateCacheFind(uint32_t hash, const char* topicOrMember, const char* signal,
                                       size_t* insertPos = nullptr) {
    auto it = std::lower_bound(stateCache.begin(), stateCache.end(), hash,
                               [](const StateCacheEntry& e, uint32_t h) { return e.hash < h; });
    if (insertPos) *insertPos = static_cast<size_t>(it - stateCache.begin());
    for (; it != stateCache.end() && it->hash == hash; ++it)
        if (stateEntryMatches(*it, topicOrMember, signal)) return &*it;
    return nullptr;
}

inline const char* stateEntryTopic(const StateCacheEntry& e, char* buf, size_t len) {
    if (!e.signal) return e.topic;
    snprintf(buf, len, HA_TOPIC_ROOT "/%s/%s/state", e.topic, e.signal);
    return buf;
}

static int stateDeadbandTenths = STATE_DEADBAND_TENTHS;   // variable for the tests

// True if `value` of a measured temperature is too close to the last published one
inline bool withinDeadband(const char* signal, const char* published, const char* value) {
    static const char* const kDeadbandSignals[] = {
        "AUSSENTEMP", "SPEICHERISTTEMP", "RUECKLAUFISTTEMP", "VORLAUFISTTEMP", "WPVORLAUFIST"};
    if (stateDeadbandTenths <= 0 || !signal || !published[0]) return false;
    bool listed = false;
    for (const char* name : kDeadbandSignals) {
        if (strcmp(signal, name) == 0) { listed = true; break; }
    }
    if (!listed) return false;
    char* endPublished;
    char* endValue;
    const float a = strtof(published, &endPublished);
    const float b = strtof(value, &endValue);
    if (endPublished == published || *endPublished || endValue == value || *endValue) return false;
    return std::lround(std::fabs(a - b) * 10.0f) < stateDeadbandTenths;
}

// Publishes `value` on change; an unchanged value goes out again after
// STATE_HEARTBEAT_MS. topicOrMember/signal must have static lifetime (string
// literals, ElsterTable/CanMembers names). Returns true if the value was sent.
inline bool publishStateCached(uint32_t hash, const char* topicOrMember, const char* signal,
                               const char* topic, const char* value) {
    const size_t len = strlen(value);
    const uint32_t now = millis();
    size_t pos = 0;
    StateCacheEntry* e = stateCacheFind(hash, topicOrMember, signal, &pos);
    if (!e) {
        if (len >= sizeof(e->value) || stateCache.size() >= STATE_CACHE_MAX_ENTRIES) {
            return id(mqtt_client).is_connected() && mqttPublish(topic, value, len);   // untracked
        }
        if (stateCache.capacity() == 0) stateCache.reserve(128);   // typical model: 100-200 topics
        StateCacheEntry fresh{hash, topicOrMember, signal, now, true, {0}};
        e = &*stateCache.insert(stateCache.begin() + pos, fresh);
        if (stateRepublishActive && pos < stateRepublishPos) stateRepublishPos++;
    }
    if (len >= sizeof(e->value)) {
        // value grew beyond the buffer (rare): publish untracked, remember a marker
        e->value[0] = '\0';
        e->pending = true;
        return id(mqtt_client).is_connected() && mqttPublish(topic, value, len);
    }
    if (strcmp(e->value, value) != 0 && !withinDeadband(signal, e->value, value)) {
        memcpy(e->value, value, len + 1);
        e->pending = true;
    }
    if (!e->pending && (now - e->lastPublishMs) < STATE_HEARTBEAT_MS) {
        stateSuppressedCount++;
        return false;
    }
    // While disconnected keep the value pending; republishAllStates() sends it on connect
    if (!id(mqtt_client).is_connected()) return false;
    if (mqttPublish(topic, value, len)) {
        memcpy(e->value, value, len + 1);   // a heartbeat may carry a value held back by the deadband
        e->pending = false;
        e->lastPublishMs = now;
        return true;
    }
    return false;
}

// Calculated sensor state (topic is a string literal / CalculatedSensorConfig field)
inline bool publishCalculatedState(const char* topic, const char* value) {
    return publishStateCached(elsterNameHash(topic), topic, nullptr, topic, value);
}

// The next value of this raw signal is published even if unchanged (write echo:
// HA expects the read-back after a command, also when the device kept the old value).
inline void forceNextStatePublish(const CanMember& cm, const ElsterIndex* ei) {
    if (StateCacheEntry* e = stateCacheFind(rawStateHash(cm, ei->Name), cm.Name, ei->Name)) e->pending = true;
}

// Called on every MQTT (re)connect: a broker restart without persistence would
// otherwise leave constant values empty until their heartbeat.
// Right after an MQTT connect ESPHome queues its subscriptions and the states of the
// native entities. The async publish queue holds only 30 messages, so anything sent
// in the same moment is dropped. Our own republishing waits until this has drained.
static constexpr uint32_t MQTT_CONNECT_SETTLE_MS = 3000;
static uint32_t mqttConnectedAtMs = 0;
static bool mqttConnectSeen = false;
inline void noteMqttConnected() { mqttConnectedAtMs = millis(); mqttConnectSeen = true; }
inline bool mqttSettling() {
    return mqttConnectSeen && millis() - mqttConnectedAtMs < MQTT_CONNECT_SETTLE_MS;
}

inline void republishAllStates() {
    stateRepublishActive = !stateCache.empty();
    stateRepublishPos = 0;
}

inline void resetStateCache() {
    stateCache.clear();
    stateRepublishActive = false;
    stateRepublishPos = 0;
    stateSuppressedCount = 0;
}

// Publish signal state to MQTT (retained; on change, unchanged values as heartbeat)
void publishMqttState(const CanMember &cm, const ElsterIndex *ei, const char *value) {
    // Input validation
    if (!ei || !ei->Name || !value || !*value) {
        ESP_LOGW("MQTT", "Invalid signal data, skipping state publish");
        return;
    }
    
    // Build state topic with bounds checking
    char stateTopic[128];
    int written = snprintf(stateTopic, sizeof(stateTopic), HA_TOPIC_ROOT "/%s/%s/state", cm.Name, ei->Name);
    if (written >= (int)sizeof(stateTopic)) {
        ESP_LOGW("MQTT", "Topic too long for %s/%s, truncated", cm.Name, ei->Name);
    }
    
    publishStateCached(rawStateHash(cm, ei->Name), cm.Name, ei->Name, stateTopic, value);
}

inline void publishMqttState(const CanMember &cm, const ElsterIndex *ei, const std::string &value) {
    publishMqttState(cm, ei, value.c_str());
}

#endif  // MQTT_PUBLISH_H

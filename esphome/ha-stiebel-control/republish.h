/*
 * republish.h - sends the retained discovery configs and the cached states again after an
 * MQTT connect or a Home Assistant birth message, a few per call (#109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef REPUBLISH_H
#define REPUBLISH_H

#include <cstdint>
#include <cstring>
#include <vector>
#include "mqtt_publish.h"
#include "discovery.h"
#include "writables.h"
#include "calculated.h"
#include "energy.h"
#include "time_programs.h"

// ============================================================================
// DISCOVERY REPUBLISH (MQTT connect, Home Assistant birth message, manual trigger)
// ============================================================================
// All discovery configs are retained, so a republish only matters after the broker
// or HA lost them. republishAllDiscoveries() just (re)starts a cursor; the configs go
// out a few per call of discoveryRepublishStep() (common.yaml interval), so neither
// the loop nor the MQTT outbound queue sees a burst of ~100 retained payloads.
enum DiscoveryRepublishPhase : uint8_t {
    DR_IDLE = 0, DR_NUMBERS, DR_SELECTS, DR_CLIMATE, DR_CALCULATED, DR_SIGNALS
};
static DiscoveryRepublishPhase discoveryRepublishPhase = DR_IDLE;
static size_t discoveryRepublishPos = 0;
static std::vector<DiscoveredSignal> discoveryRepublishQueue;
static constexpr size_t DISCOVERY_REPUBLISH_BATCH = 2;

void republishAllDiscoveries() {
    // Snapshot: signals discovered after this point publish their own config anyway.
    discoveryRepublishQueue.clear();
    discoveryRepublishQueue.reserve(discoveredSignals.size());
    for (const auto& kv : discoveredSignals) discoveryRepublishQueue.push_back(kv.second);
    discoveryRepublishPhase = DR_NUMBERS;
    discoveryRepublishPos = 0;
    copDiscoveryRepublish();         // COP configs follow with the next COP update
    dhwProgramRepublish();           // DHW program: config and text again
    ESP_LOGI("MQTT", "Republishing MQTT discoveries (%u signals)", (unsigned)discoveryRepublishQueue.size());
}

inline bool discoveryRepublishActive() { return discoveryRepublishPhase != DR_IDLE; }

// Publishes up to DISCOVERY_REPUBLISH_BATCH discovery configs (climate counts as one).
void discoveryRepublishStep() {
    if (discoveryRepublishPhase == DR_IDLE || !id(mqtt_client).is_connected() || mqttSettling()) return;
    size_t budget = DISCOVERY_REPUBLISH_BATCH;
    while (budget > 0 && discoveryRepublishPhase != DR_IDLE) {
        const size_t i = discoveryRepublishPos++;
        switch (discoveryRepublishPhase) {
        case DR_NUMBERS:
            if (i >= WRITABLE_NUMBER_COUNT) { discoveryRepublishPhase = DR_SELECTS; discoveryRepublishPos = 0; continue; }
            if (!writableNumberApplies(writableNumbers[i])) continue;
            publishWritableNumberDiscovery(writableNumbers[i], true);
            break;
        case DR_SELECTS:
            if (i >= WRITABLE_SELECT_COUNT) { discoveryRepublishPhase = DR_CLIMATE; discoveryRepublishPos = 0; continue; }
            publishWritableSelectDiscovery(writableSelects[i], true);
            break;
        case DR_CLIMATE:
            if (i >= 1) { discoveryRepublishPhase = DR_CALCULATED; discoveryRepublishPos = 0; continue; }
            publishClimateDiscoveries();
            break;
        case DR_CALCULATED:
            if (i >= CALCULATED_SENSOR_COUNT) { discoveryRepublishPhase = DR_SIGNALS; discoveryRepublishPos = 0; continue; }
            // onDemand rows announce themselves on their first value (calculated.h)
            if (calculatedSensors[i].onDemand &&
                discoveredCalculatedSensors.find(calculatedSensors[i].uniqueId) == discoveredCalculatedSensors.end())
                continue;
            publishCalculatedSensorDiscovery(calculatedSensors[i], true);
            break;
        case DR_SIGNALS:
            if (i >= discoveryRepublishQueue.size()) {
                ESP_LOGI("MQTT", "Discovery republish complete");
                discoveryRepublishPhase = DR_IDLE;
                discoveryRepublishPos = 0;
                discoveryRepublishQueue.clear();
                discoveryRepublishQueue.shrink_to_fit();
                continue;
            }
            publishMqttDiscovery(*discoveryRepublishQueue[i].cm, discoveryRepublishQueue[i].ei);
            break;
        default:
            discoveryRepublishPhase = DR_IDLE;
            continue;
        }
        budget--;
    }
}

// Sends up to STATE_REPUBLISH_BATCH cached states (250 ms interval in common.yaml).
// Waits while MQTT is down and while the discovery republish is still running.
inline void stateRepublishStep() {
    if (!stateRepublishActive || !id(mqtt_client).is_connected() || discoveryRepublishActive() ||
        mqttSettling()) return;
    char buf[128];
    const uint32_t now = millis();
    for (int n = 0; n < STATE_REPUBLISH_BATCH && stateRepublishPos < stateCache.size(); n++) {
        StateCacheEntry& e = stateCache[stateRepublishPos++];
        if (e.value[0] == '\0') continue;   // untracked long value
        const char* topic = stateEntryTopic(e, buf, sizeof(buf));
        if (mqttPublish(topic, e.value, strlen(e.value))) {
            e.pending = false;
            e.lastPublishMs = now;
        }
    }
    if (stateRepublishPos >= stateCache.size()) {
        stateRepublishActive = false;
        ESP_LOGI("MQTT", "State republish complete (%u topics)", (unsigned)stateCache.size());
    }
}

#endif  // REPUBLISH_H

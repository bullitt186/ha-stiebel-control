/*
 * request_manager.h - polls the model's signal request table (signal_requests_<model>.h):
 * schedules, round-robin, back-off for signals the model does not provide (#109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef REQUEST_MANAGER_H
#define REQUEST_MANAGER_H

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "config.h"
#include "can_members.h"
#include "signal_route.h"
#include "can_io.h"
#include "bus_scan.h"

// Extra step at the end of every processSignalRequests() call (#79: device limits of the
// writable numbers). Set by the module that needs it; nullptr = none.
static void (*requestManagerExtraStep)(unsigned long now) = nullptr;

// Track next scheduled request time per signalKey (member + signal)
static std::unordered_map<uint64_t, unsigned long> nextRequestTime;

// Consecutive -255 (0x8000, "not available") answers per signalKey.
static std::unordered_map<uint64_t, uint8_t> invalidStreak;

// Back off signals the model does not provide: from the 3rd consecutive -255 the
// poll interval doubles per answer, capped at 6 h. A valid value resets the streak.
static constexpr unsigned long BACKOFF_MAX_MS = 6UL * 3600UL * 1000UL;
inline unsigned long nextBackoffMs(unsigned long baseMs, uint8_t streak) {
    if (streak < 3) return baseMs;
    unsigned shift = streak - 2;
    if (shift > 16 || (baseMs << shift) > BACKOFF_MAX_MS) return baseMs > BACKOFF_MAX_MS ? baseMs : BACKOFF_MAX_MS;
    return baseMs << shift;
}

inline unsigned long backoffIntervalMs(uint64_t key, unsigned long baseMs) {
    auto it = invalidStreak.find(key);
    return nextBackoffMs(baseMs, it == invalidStreak.end() ? 0 : it->second);
}

// Track starting position for round-robin signal processing (prevents starvation)
static int signalProcessingStartIndex = 0;

// ============================================================================
// SIGNAL REQUEST CONFIGURATION
// ============================================================================

// Forward declarations for the model-specific signal request table.
// The actual definition comes from signal_requests_*.h, included by the
// model's YAML package (e.g. wpl13e.yaml via esphome: includes:).
extern const SignalRequest signalRequests[];
extern const size_t SIGNAL_REQUEST_COUNT_VALUE;
#define SIGNAL_REQUEST_COUNT SIGNAL_REQUEST_COUNT_VALUE

// Runtime state for signal request manager
static bool requestManagerStarted = false;
static unsigned long requestManagerStartTime = 0;

// Helper function to generate random number in range [min, max) using ESP32 hardware RNG
// esp_random() uses hardware entropy sources and doesn't need seeding
unsigned long getRandomInRange(unsigned long min, unsigned long max) {
    if (min >= max) return min;
    unsigned long range = max - min;
    return min + (esp_random() % range);
}

// Request table entries resolved once when the request manager starts: the
// ElsterIndex pointer (nullptr = unknown/blacklisted, skipped) and the schedule key
// per member. Avoids two name searches and string keys per entry and second.
static const CanMemberType REQUEST_ALL_MEMBERS[] = {cm_kessel, cm_manager, cm_heizmodul};
static constexpr size_t REQUEST_ALL_MEMBER_COUNT = sizeof(REQUEST_ALL_MEMBERS) / sizeof(REQUEST_ALL_MEMBERS[0]);
struct ResolvedRequest {
    const ElsterIndex* ei;
    uint64_t key[REQUEST_ALL_MEMBER_COUNT];   // [0] only for a single member
};
static std::vector<ResolvedRequest> resolvedRequests;

void resolveSignalRequests() {
    resolvedRequests.assign(SIGNAL_REQUEST_COUNT, ResolvedRequest{nullptr, {0, 0, 0}});
    for (size_t i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
        const SignalRequest& req = signalRequests[i];
        const ElsterIndex* ei = GetElsterIndex(req.signalName);
        if (!ei || ei->Index == 0xFFFF || ei->isBlacklisted) continue;  // unknown or blacklisted
        ResolvedRequest& r = resolvedRequests[i];
        r.ei = ei;
        const uint32_t h = elsterNameHash(ei->Name);
        if (req.member == cm_other) {
            for (size_t m = 0; m < REQUEST_ALL_MEMBER_COUNT; m++)
                r.key[m] = signalKey(CanMembers[REQUEST_ALL_MEMBERS[m]], h);
        } else {
            r.key[0] = signalKey(CanMembers[req.member], h);
        }
    }
}

void processSignalRequests() {
    unsigned long now = millis();
    if (scanState.active) return;  // bus scan owns the request slot

    // PASSIVE LISTEN MODE: keine Requests senden
    if (disableCanRequests) {
        return;
    }
    
    // Startup delay: wait before starting signal requests
    if (!requestManagerStarted) {
        if (requestManagerStartTime == 0) {
            requestManagerStartTime = now;
            ESP_LOGI("REQUEST_MGR", "Starting signal request manager (%ds startup delay)", STARTUP_DELAY_MS / 1000);
            return;
        }
        
        if (now - requestManagerStartTime < STARTUP_DELAY_MS) {
            return; // Still in startup delay
        }
        
        requestManagerStarted = true;
        ESP_LOGI("REQUEST_MGR", "Signal request manager active - processing %d signal definitions", 
                 SIGNAL_REQUEST_COUNT);
        resolveSignalRequests();
        
        // Initialize all signal schedules with random offsets to spread out initial load
        ESP_LOGI("REQUEST_MGR", "Initializing signal schedules with random offsets to prevent burst");
        for (size_t i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
            const SignalRequest& req = signalRequests[i];
            const ResolvedRequest& r = resolvedRequests[i];
            if (!r.ei) continue;   // unknown or blacklisted
            
            unsigned long intervalMs = req.frequency * 1000UL;
            const size_t members = (req.member == cm_other) ? REQUEST_ALL_MEMBER_COUNT : 1;
            for (size_t m = 0; m < members; m++) {
                // Random offset between 0 and full interval using ESP32 hardware RNG
                nextRequestTime[r.key[m]] = now + getRandomInRange(0, intervalMs + 1);
            }
        }
        ESP_LOGI("REQUEST_MGR", "Initialized %d signal schedules", nextRequestTime.size());
    }
    
    // Rate limiting: send up to MAX_REQUESTS_PER_ITERATION requests per iteration
    // Randomization in scheduling prevents bursts
    int requestsSentThisIteration = 0;
    
    // Round-robin processing: start from last position to prevent starvation of signals at end of array
    // Process all signals, wrapping around, until we hit rate limit or complete full cycle
    int processedCount = 0;
    int currentIndex = signalProcessingStartIndex;
    
    while (processedCount < (int)SIGNAL_REQUEST_COUNT && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION) {
        const SignalRequest& req = signalRequests[currentIndex];
        const ResolvedRequest& r = resolvedRequests[currentIndex];
        processedCount++;
        
        const ElsterIndex* ei = r.ei;
        if (!ei) {
            // Unknown or blacklisted: never requested on the CAN bus
            currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
            continue;
        }
        
        unsigned long intervalMs = req.frequency * 1000UL;
        // Calculate next scheduled time with random offset (0 to 5% of interval)
        // This keeps signals from synchronizing while staying close to target frequency
        unsigned long maxJitter = (intervalMs / 20); // 5% of interval
        if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
        
        // Determine which members to request from: cm_other = all members (each
        // tracked independently), but only ONE of them per iteration to prevent bursts
        const size_t members = (req.member == cm_other) ? REQUEST_ALL_MEMBER_COUNT : 1;
        for (size_t m = 0; m < members && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION; m++) {
            const CanMember* member = (req.member == cm_other) ? &CanMembers[REQUEST_ALL_MEMBERS[m]]
                                                               : &CanMembers[req.member];
            const uint64_t key = r.key[m];
            
            // Check if this signal is overdue (current time >= scheduled time)
            if (now >= nextRequestTime[key]) {
                readSignal(member, ei);
                requestsSentThisIteration++;
                unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                nextRequestTime[key] = now + backoffIntervalMs(key, intervalMs) + randomDelay;
                // Other members of a cm_other group are checked in later iterations
                break;
            }
        }
        
        // Move to next signal in round-robin fashion (wrap around at end)
        currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
    }

    // Continue after the last checked signal next time. Without this every call started
    // at index 0 and, with more due requests than one per second, the end of the table
    // was never polled.
    signalProcessingStartIndex = currentIndex;

    // Device limits of the writable numbers, one request at a time (#79). Injected by
    // writables.h, which owns those tables.
    if (requestManagerExtraStep) requestManagerExtraStep(now);
}

#endif  // REQUEST_MANAGER_H

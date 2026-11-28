#pragma once
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cstring>

inline bool wpl9ValidEnergyValue(const char* signal, const char* text) {
    char* end = nullptr;
    const float n = std::strtof(text, &end);
    const bool remainder = std::strstr(signal, "_SUM_KWH") || std::strstr(signal, "_TAG_WH");
    return end != text && *end == '\0' && std::isfinite(n) && n >= 0.0f &&
           n <= (remainder ? 999.0f : 65535.0f) && std::floor(n) == n && n != 32768.0f;
}

// Split counters are sampled within 15 seconds. A carry may be observed in
// either order. Confirm ambiguous upward carries without requiring the high
// word to stand still: thermal yield can advance several kWh per poll cycle.
namespace WpmDisplayRegisters {
constexpr uint16_t heatRuntime = 0x4efb, dhwRuntime = 0x4efd;
constexpr uint16_t defrostRuntime = 0x4f06, coolingRuntime = 0x07ff;
constexpr uint16_t startsThousands = 0x4ef0, startsRemainder = 0x4ef1;
}
struct Wpl9EnergyPair {
    float high = 0, low = 0, lastHigh = 0, lastLow = 0, pendingHigh = 0;
    float resetCandidate = 0;
    uint32_t highAt = 0, lowAt = 0, lastAt = 0, dayChangedAt = 0, lastResetAt = 0;
    int day = -1;
    bool freshHigh = false, freshLow = false, haveLast = false, carryPending = false;
    bool resetPending = false, dayChanged = false, haveReset = false;
    bool bootCandidate = false;
    uint8_t bootSamples = 0;

    bool update(bool isHigh, float value, uint32_t now, float& hi, float& lo,
                bool daily = true, int currentDay = -1) {
        if (!std::isfinite(value) || value < 0 || value == 32768 ||
            value > (isHigh ? 65535 : 999) || std::floor(value) != value) {
            freshHigh = freshLow = false;
            return false;
        }
        if (currentDay > 0) {
            if (day > 0 && currentDay != day) { dayChanged = true; dayChangedAt = now; }
            day = currentDay;
        }
        // Keep the observed date transition until a reset is confirmed: a CAN
        // outage can outlast the old 30-minute window, even with TAG_KWH == 0.
        if (isHigh) { high = value; highAt = now; freshHigh = true; }
        else { low = value; lowAt = now; freshLow = true; }
        if (!freshHigh || !freshLow) return false;
        if (now - highAt > 15000U || now - lowAt > 15000U) return false;
        freshHigh = freshLow = false;
        const float total = high * 1000.0f + low;
        const float previous = lastHigh * 1000.0f + lastLow;
        if (!daily && !haveLast) {
            if (bootSamples < 255) ++bootSamples;
            // SUM cannot decrease later. Never anchor it to one torn boot pair.
            // A lower correction or an ambiguous upward carry restarts confirmation.
            const bool consistent = bootCandidate && total >= previous &&
                                    !(high > lastHigh && low >= lastLow);
            lastHigh = high; lastLow = low;
            bootCandidate = true;
            if (!consistent) return false;
        }
        if (haveLast && total < previous) {
            // A decreasing remainder within the same high word is a torn
            // carry, never confirmation of a reset merely because repeated.
            // WPM may reset shortly before midnight: a drop of the high word
            // to zero can also start a reset candidate without a date change.
            const bool resetPossible = daily && (!haveReset || now - lastResetAt > 21600000U) &&
                                       (dayChanged || (high == 0 && lastHigh > 0));
            if (!resetPossible) return false;
            if (!resetPending || total < resetCandidate) {
                resetCandidate = total; resetPending = true;
                return false;
            }
            resetPending = dayChanged = false;
            haveReset = true; lastResetAt = now;
        // A high-only change within a minute may be a device-side torn carry.
        // At normal polling intervals, do not gate a legitimate multi-kWh yield.
        } else if (haveLast && high > lastHigh && low >= lastLow &&
                   (now - lastAt < 60000U || low >= 950)) {
            if (!carryPending || high < pendingHigh) {
                carryPending = true; pendingHigh = high;
                return false;
            }
        }
        carryPending = resetPending = false;
        haveLast = true;
        lastHigh = hi = high; lastLow = lo = low; lastAt = now;
        return true;
    }
};

// 0x4ef0 is thousands, 0x4ef1 the 0..999 remainder (WPM: 8*1000+941).
// Pair fresh samples; a rollover may be observed between the two CAN reads.
struct Wpl9StartCounter {
    uint32_t thousands = 0, remainder = 0, last = 0, candidate = 0;
    uint32_t thousandsAt = 0, remainderAt = 0;
    bool freshThousands = false, freshRemainder = false, haveLast = false, haveCandidate = false;

    bool update(bool kilo, const char* text, uint32_t now, uint32_t& out) {
        char* end = nullptr;
        if (!text || text[0] < '0' || text[0] > '9') {
            freshThousands = freshRemainder = false;
            return false;
        }
        unsigned long value = std::strtoul(text, &end, 10);
        if (*end || value > (kilo ? 65535UL : 999UL) || value == 0x8000UL) {
            freshThousands = freshRemainder = false;
            return false;
        }
        if (kilo) { thousands = value; thousandsAt = now; freshThousands = true; }
        else { remainder = value; remainderAt = now; freshRemainder = true; }
        if (!freshThousands || !freshRemainder) return false;
        // Unsigned differences also work across the millis() rollover.
        if (now - thousandsAt > 15000U || now - remainderAt > 15000U) return false;
        freshThousands = freshRemainder = false;
        uint32_t total = thousands * 1000U + remainder;
        if (haveLast && total < last) return false; // Never fabricate a reset from a torn pair.
        if (haveLast && total > last + 1U) {
            if (!haveCandidate || candidate != total) {
                candidate = total; haveCandidate = true;
                return false; // Confirm a large change with a second complete pair.
            }
        }
        haveCandidate = false;
        haveLast = true;
        last = out = total;
        return true;
    }
};

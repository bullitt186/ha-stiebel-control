#pragma once
// WPM display integration; depends only on lower modules.
#include "can_io.h"
#include "calculated.h"
#include "energy.h"


bool handleWpmDisplayCounter(const CanMember& cm, const ElsterIndex* ei, const std::string& value) {
    if (strncmp(ei->Name, "EL_AUFNAHMELEISTUNG_", sizeof("EL_AUFNAHMELEISTUNG_") - 1) == 0 ||
        strncmp(ei->Name, "WAERMEERTRAG_", sizeof("WAERMEERTRAG_") - 1) == 0) {
        if (cm.CanId != CanMembers[cm_heizmodul_wpl].CanId) return false;
        if (!wpl9ValidEnergyValue(ei->Name, value.c_str())) {
            const std::string name(ei->Name);
            auto it = wpl9EnergyPairs.find(name.substr(0, name.rfind('_')));
            if (it != wpl9EnergyPairs.end()) it->second.freshHigh = it->second.freshLow = false;
            return false;
        }
    }
    if (ei->Index == WpmDisplayRegisters::heatRuntime || ei->Index == WpmDisplayRegisters::dhwRuntime || ei->Index == WpmDisplayRegisters::defrostRuntime || ei->Index == WpmDisplayRegisters::coolingRuntime) {
        if (cm.CanId != CanMembers[cm_heizmodul_wpl].CanId || value == "-255") return false;
    }
    if ((ei->Index == WpmDisplayRegisters::startsThousands || ei->Index == WpmDisplayRegisters::startsRemainder) && cm.CanId == CanMembers[cm_heizmodul_wpl].CanId) {
        uint32_t starts;
        if (wpl9StartCounter.update(ei->Index == WpmDisplayRegisters::startsThousands, value.c_str(), millis(), starts)) {
            publishCalculatedSensorDiscovery(wpl9LifetimeStartsSensor, false);
            char buf[24];
            snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(starts));
            publishCalculatedState(wpl9LifetimeStartsSensor.stateTopic, buf);
        }
        // Independent periodic requests have random phases and may never form a
        // fresh pair. Read the remainder as a follow-up to a valid kilo reply.
        if (ei->Index == WpmDisplayRegisters::startsThousands && value != "-255") {
            readSignal(&cm, "VERDICHTER_STARTS_4EF1");
        }
        return false; // The two partial counters are not independent HA totals.
    }
    return true;
}

void storeWpmDisplayEnergy(const CanMember& cm, const char* signalName, const std::string& value) {

    const std::string name(signalName);
    const auto split = name.rfind('_');
    const std::string pairKey = name.substr(0, split);
    const bool high = name.substr(split + 1) != (pairKey.find("_TAG") != std::string::npos ? "WH" : "KWH");
    if (high) {
        const std::string lowName = pairKey + (pairKey.find("_TAG") != std::string::npos ? "_WH" : "_KWH");
        readSignal(&cm, lowName.c_str());
    }
    float hi, lo;
    auto& pair = wpl9EnergyPairs[pairKey];
    if (!pair.update(high, std::strtof(value.c_str(), nullptr), millis(), hi, lo, pairKey.find("_TAG") != std::string::npos, lastTag)) {
        // Confirm SUM promptly after boot instead of waiting the full polling
        // period. Bound extra requests even if the unit answers torn pairs.
        if (!high && !pair.haveLast && pair.bootCandidate && pair.bootSamples < 3)
            readSignal(&cm, (pairKey + "_MWH").c_str());
        return;
    }
    const std::string highKey = pairKey + (pairKey.find("_TAG") != std::string::npos ? "_KWH" : "_MWH");
    const std::string lowKey = pairKey + (pairKey.find("_TAG") != std::string::npos ? "_WH" : "_KWH");
    copEnergyValues[highKey] = hi; copEnergyValues[lowKey] = lo;
    copEnergyByMember[std::string(cm.Name) + "|" + highKey] = hi;
    copEnergyByMember[std::string(cm.Name) + "|" + lowKey] = lo;
    updateCOPCalculations();
    return;

}

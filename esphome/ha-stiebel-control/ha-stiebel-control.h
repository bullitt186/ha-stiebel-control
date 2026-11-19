/*
 *
 *  Copyright (C) 2023 Bastian Stahmer (bastian@stahmer.net)
 *  This program is part of the ESPHome / Home Assistant Program "ha-stiebel-control"
 *  and is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(ha_stiebel_control_H)
#define ha_stiebel_control_H

// ============================================================================
// MODULES (#109)
// ============================================================================
// This header holds only the CAN entry points: processAndUpdate() for every received
// frame and updateSensor(), which publishes a decoded value and hands it to the module
// that derives something from it. Everything else lives in its own header; see
// docs/ARCHITECTURE.md for the include order.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "ElsterTable.h"
#include "KElsterTable.h"
#include "config.h"
#include "model_profile.h"
#include "language_select.h"
#include "can_members.h"   // CanMember, CanMembers[], CanMemberType, SignalRequest
#include "signal_route.h"
#include "mqtt_publish.h"
#include "can_io.h"
#include "paced_writes.h"
#include "discovery.h"
#include "bus_scan.h"
#include "request_manager.h"
#include "calculated.h"
#include "energy.h"
#include "writables.h"
#include "sg_ready_io.h"
#include "time_programs.h"
#include "set_commands.h"
#include "fault_list.h"
#include "republish.h"

// ============================================================================
// COMPILE-TIME STRING HASHING FOR FAST SIGNAL DISPATCH
// ============================================================================

// Compile-time DJB2 hash function (constexpr for C++11 compatibility). The runtime version is
// elsterNameHash() in signal_route.h; a test checks that both agree.
constexpr uint32_t hash_impl(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : hash_impl(str + 1, ((hash << 5) + hash) + static_cast<uint32_t>(*str));
}

constexpr uint32_t hash(const char* str) {
    return hash_impl(str);
}

// Pre-computed compile-time hashes for all monitored signals
constexpr uint32_t HASH_JAHR = hash("JAHR");
constexpr uint32_t HASH_MONAT = hash("MONAT");
constexpr uint32_t HASH_TAG = hash("TAG");
constexpr uint32_t HASH_STUNDE = hash("STUNDE");
constexpr uint32_t HASH_MINUTE = hash("MINUTE");
constexpr uint32_t HASH_SEKUNDE = hash("SEKUNDE");
constexpr uint32_t HASH_WPVORLAUFIST = hash("WPVORLAUFIST");
constexpr uint32_t HASH_RUECKLAUFISTTEMP = hash("RUECKLAUFISTTEMP");
constexpr uint32_t HASH_VERDICHTER = hash("VERDICHTER");
constexpr uint32_t HASH_PUMPENSTATUS = hash("PUMPENSTATUS");
constexpr uint32_t HASH_EINSTELL_SPEICHERSOLLTEMP = hash("EINSTELL_SPEICHERSOLLTEMP");
constexpr uint32_t HASH_RAUMSOLLTEMP_I = hash("RAUMSOLLTEMP_I");
constexpr uint32_t HASH_ZEITDAUER_LETZTE_ABTAUUNG = hash("ZEITDAUER_LETZTE_ABTAUUNG");
constexpr uint32_t HASH_FEHLERMELDUNG = hash("FEHLERMELDUNG");
constexpr uint32_t HASH_EVU_SPERRE_AKTIV = hash("EVU_SPERRE_AKTIV");
constexpr uint32_t HASH_FERNBEDIENUNGSZUORDNUNG = hash("FERNBEDIENUNGSZUORDNUNG");
constexpr uint32_t HASH_SOMMERBETRIEB = hash("SOMMERBETRIEB");
constexpr uint32_t HASH_SW_AUSSENTEMP = hash("SW_AUSSENTEMP");
constexpr uint32_t HASH_GEBAEUDEART = hash("GEBAEUDEART");
constexpr uint32_t HASH_AUSSENTEMP = hash("AUSSENTEMP");
constexpr uint32_t HASH_KESSELSOLLTEMP = hash("KESSELSOLLTEMP");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ = hash("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW = hash("EL_AUFNAHMELEISTUNG_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_2WE_WW = hash("WAERMEERTRAG_2WE_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_2WE_HEIZ = hash("WAERMEERTRAG_2WE_HEIZ_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW = hash("WAERMEERTRAG_WW_SUM_MWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ = hash("WAERMEERTRAG_HEIZ_SUM_MWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ_KWH = hash("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW_KWH = hash("EL_AUFNAHMELEISTUNG_WW_SUM_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ_KWH = hash("WAERMEERTRAG_HEIZ_SUM_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW_KWH = hash("WAERMEERTRAG_WW_SUM_KWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH = hash("EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW_TAG_WH = hash("EL_AUFNAHMELEISTUNG_WW_TAG_WH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH = hash("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH");
constexpr uint32_t HASH_EL_AUFNAHMELEISTUNG_WW_TAG_KWH = hash("EL_AUFNAHMELEISTUNG_WW_TAG_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ_TAG_WH = hash("WAERMEERTRAG_HEIZ_TAG_WH");
constexpr uint32_t HASH_WAERMEERTRAG_HEIZ_TAG_KWH = hash("WAERMEERTRAG_HEIZ_TAG_KWH");
constexpr uint32_t HASH_WAERMEERTRAG_WW_TAG_WH = hash("WAERMEERTRAG_WW_TAG_WH");
constexpr uint32_t HASH_WAERMEERTRAG_WW_TAG_KWH = hash("WAERMEERTRAG_WW_TAG_KWH");

// WPL13E: the WPMW II polls the IWS runtime block itself, so these answers appear
// on the bus although we don't request them. Single-compressor unit without
// cooling: compressor 1 and cooling counters are only 0 / -255 (HA history).
// (The former "MANAGER x = 0.0" copies were read requests, filtered in processAndUpdate.)
static bool isIgnoredOnWpl13e(const char* name) {
    if (!kModel.ignoreUnusedIwsRuntime) return false;
    return strncmp(name, "LZ_VERD_", 8) == 0 && (strstr(name, "VERD_1") || strstr(name, "KUEHL"));
}

void updateSensor(const CanMember &cm, const ElsterIndex *ei, const std::string &value)
{
    // Skip empty values
    if (value.empty()) {
        return;
    }
    pacedWriteNoteValue(cm, ei, value);
    if (isWpmw2Model() && timeProgramUpdate(cm, ei->Name, value)) return;   // shown as one text per day
    if (isIgnoredOnWpl13e(ei->Name)) return;
    if (isWpmw2Model() && strcmp(cm.Name, "HEIZMODUL") == 0) lastHeizmodulFrameMs = millis();
    // Bool fields carrying other bits decode to "?"; HA rejects that payload for a
    // binary sensor (log warning per poll) and keeps the last state anyway. Seen on
    // WPL13E: ABTAUUNGAKTIV = 0x0200 while the compressor runs (no defrost).
    if (value == "?" && (ei->Type == et_bool || ei->Type == et_little_bool)) return;

    // Per frame: no std::string temporaries. The name hash drives the member/signal
    // key (discovery, back-off) and the dispatch switch below.
    const char* signalName = ei->Name;
    const uint32_t signalHash = elsterNameHash(signalName);
    const uint64_t key = signalKey(cm, signalHash);

    // -255 = the device does not provide this value: count it for the poll
    // back-off and create no HA entity until a real value has been seen.
    if (value == "-255") {
        uint8_t& streak = invalidStreak[key];
        if (streak < 255) streak++;
        if (streak >= 3) {
            ESP_LOGD("BACKOFF", "%s_%s streak=%u", cm.Name, signalName, streak);
        }
        if (discoveredSignals.find(key) == discoveredSignals.end()) return;
        // A counter must not drop to -255: HA reads a drop of a total_increasing sensor as a
        // meter reset and adds the next real value to its sum again. Keep the last state.
        if (strcmp(signalStateClass(ei), "total_increasing") == 0) return;
    } else if (!invalidStreak.empty()) {
        invalidStreak.erase(key);
    }

    // Invert boolean logic for EVU_SPERRE_AKTIV signal
    // CAN: 1 = lock inactive (off), 0 = lock active (on)
    // MQTT: "off" = lock inactive, "on" = lock active
    const char* publishValue = value.c_str();
    if (signalHash == HASH_EVU_SPERRE_AKTIV && strcmp(signalName, "EVU_SPERRE_AKTIV") == 0) {
        if (value == "on") {
            publishValue = "off";
        } else if (value == "off") {
            publishValue = "on";
        }
    }
    
    // Check if discovery is needed: mark as discovered and publish discovery immediately
    if (discoveredSignals.find(key) == discoveredSignals.end()) {
        discoveredSignals.emplace(key, DiscoveredSignal{&cm, ei});
        publishMqttDiscovery(cm, ei);
    }
    
    // Publish state (on change / heartbeat)
    publishMqttState(cm, ei, publishValue);
    
    // FE 7 assignment changed: the room temperature moves to the other circuit's climate
    if (signalHash == HASH_FERNBEDIENUNGSZUORDNUNG && isWpmw2Model() && strcmp(cm.Name, "MANAGER") == 0) {
        const long circuit = strtol(value.c_str(), nullptr, 10);
        const uint8_t next = (circuit == 1 || circuit == 2) ? static_cast<uint8_t>(circuit) : 0;
        if (next != remoteControlCircuit) {
            remoteControlCircuit = next;
            publishClimateDiscoveries();
        }
    }

    // Fast signal dispatch using compile-time hash (O(1) switch/jump table)
    
    switch (signalHash) {
        case HASH_JAHR: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastJahr = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated JAHR: %d", lastJahr);
            }
            break;
        }
        
        case HASH_MONAT: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastMonat = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated MONAT: %d", lastMonat);
            }
            break;
        }
        
        case HASH_TAG: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastTag = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated TAG: %d", lastTag);
                publishDate();
            }
            break;
        }
        
        case HASH_STUNDE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastStunde = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated STUNDE: %d", lastStunde);
            }
            break;
        }
        
        case HASH_MINUTE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastMinute = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated MINUTE: %d", lastMinute);
                publishTime();
            }
            break;
        }
        
        case HASH_SEKUNDE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastSekunde = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated SEKUNDE: %d", lastSekunde);
            }
            break;
        }
        
        case HASH_WPVORLAUFIST: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                // Delta T: only the heat pump's own sensors (IWS); other members are not mixed in
                if (strcmp(cm.Name, "HEIZMODUL") == 0) lastWpVorlaufIst = floatValue;
                else if (strcmp(cm.Name, "KESSEL") == 0) lastWpVorlaufKessel = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse WPVORLAUFIST value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_RUECKLAUFISTTEMP: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                if (strcmp(cm.Name, "HEIZMODUL") == 0) lastRuecklaufIstTemp = floatValue;
                if (strcmp(cm.Name, "KESSEL") == 0) lastKesselRuecklaufIst = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse RUECKLAUFISTTEMP value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_KESSELSOLLTEMP: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (strcmp(cm.Name, "KESSEL") == 0 && endPtr != value.c_str() && *endPtr == '\0') {
                lastKesselSollTemp = floatValue;
            }
            break;
        }

        case HASH_SOMMERBETRIEB:
            summer.automatic = (value == "on") ? 1 : (value == "off") ? 0 : -1;
            publishSummerState();
            break;

        case HASH_SW_AUSSENTEMP: {   // threshold, display parameter 10 AUSSENTEMP
            float v = strtof(value.c_str(), nullptr);
            if (v >= 5.0f && v <= 35.0f) { summer.threshold = v; publishSummerState(); }
            break;
        }

        case HASH_GEBAEUDEART: {
            int v = atoi(value.c_str());
            if (v >= 1 && v <= 3) summer.buildingType = v;
            break;
        }

        case HASH_AUSSENTEMP:
            if (strcmp(cm.Name, "KESSEL") == 0 && value != "-255")
                updateSummerOutsideTemp(strtof(value.c_str(), nullptr), millis());
            break;

        case HASH_FEHLERMELDUNG: {
            char topic[96], buf[192];
            snprintf(topic, sizeof(topic), HA_TOPIC_ROOT "/%s/FEHLERMELDUNG/attributes", cm.Name);
            snprintf(buf, sizeof(buf), "{\"abhilfe\":\"%s\"}", faultRemedy(value));
            mqttPublish(topic, buf, strlen(buf));
            break;
        }

        case HASH_ZEITDAUER_LETZTE_ABTAUUNG:
            // The IWS resets the last defrost duration to 0 when a defrost starts and
            // writes the new duration when it ends, so 0 marks a running defrost (#48).
            // ABTAUUNGAKTIV stayed off through three defrosts on a WPL13E.
            if (strcmp(cm.Name, "HEIZMODUL") == 0 && value != "-255") {
                publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_ABTAUUNG_LAEUFT], false);
                publishCalculatedState(calculatedSensors[CALC_IDX_ABTAUUNG_LAEUFT].stateTopic,
                                       atoi(value.c_str()) == 0 ? "on" : "off");
            }
            break;

        case HASH_EINSTELL_SPEICHERSOLLTEMP:
        case HASH_RAUMSOLLTEMP_I: {
            SignalRoute route = {cm_manager, cm_manager, cm_manager};
            if (kModel.canBDhwRoutes)
                route = resolveSignalRoute(ei->Index, cm_manager);
            if (CanMembers[route.readMember].CanId != cm.CanId) break;
            char* endPtr;
            const float v = strtof(value.c_str(), &endPtr);
            if (endPtr == value.c_str() || *endPtr != '\0' || v < -50.0f) break;
            if (strcmp(signalName, "EINSTELL_SPEICHERSOLLTEMP") == 0) lastSgDhwSetpoint = v;
            else if (strcmp(signalName, "RAUMSOLLTEMP_I") == 0) lastSgRoomSetpoint = v;
            break;
        }

        case HASH_PUMPENSTATUS:
            if (strcmp(cm.Name, "KESSEL") == 0 && value != "-255") lastPumpenstatus = atoi(value.c_str());
            break;

        case HASH_VERDICHTER: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastVerdichterValue = floatValue;
                // Compressor state will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse VERDICHTER value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ:
        case HASH_EL_AUFNAHMELEISTUNG_WW:
        case HASH_WAERMEERTRAG_2WE_WW:
        case HASH_WAERMEERTRAG_2WE_HEIZ:
        case HASH_WAERMEERTRAG_WW:
        case HASH_WAERMEERTRAG_HEIZ:
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ_KWH:
        case HASH_EL_AUFNAHMELEISTUNG_WW_KWH:
        case HASH_WAERMEERTRAG_HEIZ_KWH:
        case HASH_WAERMEERTRAG_WW_KWH:
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH:
        case HASH_EL_AUFNAHMELEISTUNG_WW_TAG_WH:
        case HASH_EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH:
        case HASH_EL_AUFNAHMELEISTUNG_WW_TAG_KWH:
        case HASH_WAERMEERTRAG_HEIZ_TAG_WH:
        case HASH_WAERMEERTRAG_HEIZ_TAG_KWH:
        case HASH_WAERMEERTRAG_WW_TAG_WH:
        case HASH_WAERMEERTRAG_WW_TAG_KWH:
            storeCOPEnergyValue(signalName, value);
            storeCOPEnergyValueMember(cm.Name, signalName, value);
            updateCOPCalculations();
            break;
        
        default:
            // Signal not monitored for calculated sensors - no action needed
            break;
    }
}

void processAndUpdate(uint32_t can_id, const std::vector<uint8_t> &msg)
{
    // processCanMessage() leaves the member unset for short frames
    if (msg.size() < 7) return;
    // Elster telegram type = low nibble of byte 0 (0 write, 1 read request, 2 response).
    // A read request carries no value: decoding it attributed a fake 0 to the
    // requesting node (e.g. the WPMW II polling the IWS -> "MANAGER VERDICHTER 0.0").
    if ((msg[0] & 0x0F) == 1) return;
    // Limit answers (switch byte 0xF8/0xF9) carry no current value and use the long layout,
    // so they must not go through the short-form decoding below (#79)
    if (handleLimitAnswer(can_id, msg)) return;

    std::string value;
    const CanMember *cm = nullptr;
    const ElsterIndex *ei = processCanMessage(msg, can_id, value, &cm);

    // Skip nodes that are not mapped in CanMembers and only broadcast
    // placeholder/unknown values (e.g. WPM3 display FES 0x100, mixer module 2
    // 0x601, redundant DHW duplicates). They fall back to the OTHER member.
    if (cm->CanId == 0x000)
    {
        return;
    }

    if (handleScanFrame(*cm, ei, msg, value)) return;
    if (handleFaultListFrame(*cm, msg)) return;

    // Skip permanently blacklisted signals (flag of the decoded row, no name lookup)
    if (ei->isBlacklisted)
    {
        return; // Reject before lookup, parsing, formatting, logging
    }

    updateSensor(*cm, ei, value);
    return;
}

#endif // ha_stiebel_control_H


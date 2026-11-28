/*
 * energy.h - COP and the combined energy values (totals, daily values, electric power,
 * derived operating state) from the heat pump's energy counters (#109).
 *
 * Uses the ESPHome API (id(), millis(), ESP_LOGx) from main.cpp, or tests/esphome_stubs.h.
 * Keeps the base of the sliding totals in NVS.
 */

#ifndef ENERGY_H
#define ENERGY_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include "calculated.h"
#include "wpl9_observed_counters.h"

// COP discovery configs sent (reset by a discovery republish)
static bool copDiscoveryPublished = false;

// Diagnostics removed for simplification

// Track which COP values we have valid data for
static std::unordered_map<std::string, float> copEnergyValues;
static std::unordered_map<std::string, Wpl9EnergyPair> wpl9EnergyPairs;

// Publish MQTT discovery for COP sensors
void publishCOPDiscovery() {
    if (copDiscoveryPublished) return;
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_WW]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_HEIZ]);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_GESAMT]);
    copDiscoveryPublished = true;
    ESP_LOGD("MQTT", "Discovery published for COP sensors");
}

// Store energy value when received for COP calculation
void storeCOPEnergyValue(const char* signalName, const std::string &value) {
    // Validate the string contains a valid number (no exceptions available)
    if (value.empty()) {
        ESP_LOGW("COP", "Empty value for %s", signalName);
        return;
    }
    
    // Check for valid numeric characters
    bool hasDigit = false;
    bool isValid = true;
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c >= '0' && c <= '9') {
            hasDigit = true;
        } else if (c != '.' && c != '-' && c != '+' && c != ' ') {
            isValid = false;
            break;
        }
    }
    
    if (!hasDigit || !isValid) {
        ESP_LOGW("COP", "Invalid numeric value for %s: %s", signalName, value.c_str());
        return;
    }
    
    // Parse the value (std::stof may still fail but won't throw in this context)
    float fval = std::atof(value.c_str());
    copEnergyValues[signalName] = fval;
    ESP_LOGD("COP", "Stored %s = %.3f", signalName, fval);
}

// Vorwaertsdeklaration: publiziert kombinierte Energie-Gesamtwerte (MWh*1000 + kWh)
void publishCombinedEnergySensors();
inline bool wplBaseTotal(int, const char*, const char*, const char*, const char*, float&);

void publishWpmDisplayCOP() {
    // WPM documents electrical VD energy separately from NHZ heat. For the
    // electric booster, electrical energy is approximated by its heat yield.
    // Include NHZ on BOTH sides; these are cumulative energy ratios, not live
    // COP or a complete system boundary (external auxiliary loads are absent).
    float totals[4] = {};
    const char* bases[] = {"EL_AUFNAHMELEISTUNG_HEIZ", "EL_AUFNAHMELEISTUNG_WW",
                           "WAERMEERTRAG_HEIZ", "WAERMEERTRAG_WW"};
    bool have[4];
    for (int i = 0; i < 4; ++i) {
        const std::string b(bases[i]);
        // Require current-boot SUM and TAG parts, even when an NVS base exists.
        have[i] = copEnergyValues.count(b+"_SUM_MWH") && copEnergyValues.count(b+"_SUM_KWH") &&
                  copEnergyValues.count(b+"_TAG_KWH") && copEnergyValues.count(b+"_TAG_WH") &&
                  wplBaseTotal(i, (b+"_SUM_MWH").c_str(), (b+"_SUM_KWH").c_str(),
                               (b+"_TAG_KWH").c_str(), (b+"_TAG_WH").c_str(), totals[i]);
    }
    auto publish = [](int idx, bool ready, float heat, float electricity) {
        char value[24];
        if (ready && electricity > 0.001f && std::isfinite(heat) && std::isfinite(electricity))
            snprintf(value, sizeof(value), "%.2f", heat/electricity);
        else snprintf(value, sizeof(value), "unknown");
        publishCalculatedState(calculatedSensors[idx].stateTopic, value);
    };
    float nhz[2] = {};
    bool haveNhz[2] = {};
    const char* nhzBases[] = {"WAERMEERTRAG_2WE_HEIZ", "WAERMEERTRAG_2WE_WW"};
    for (int i = 0; i < 2; ++i) {
        const std::string b(nhzBases[i]);
        auto hi = copEnergyValues.find(b+"_SUM_MWH");
        auto lo = copEnergyValues.find(b+"_SUM_KWH");
        // Missing NHZ inputs must not silently become zero after a reboot.
        haveNhz[i] = hi != copEnergyValues.end() && lo != copEnergyValues.end();
        if (haveNhz[i]) {
            nhz[i] = hi->second*1000.0f + lo->second;
            haveNhz[i] = std::isfinite(nhz[i]) && nhz[i] >= 0.0f;
        }
    }
    publish(CALC_IDX_COP_HEIZ, have[0] && have[2] && haveNhz[0],
            totals[2]+nhz[0], totals[0]+nhz[0]);
    publish(CALC_IDX_COP_WW, have[1] && have[3] && haveNhz[1],
            totals[3]+nhz[1], totals[1]+nhz[1]);
    publish(CALC_IDX_COP_GESAMT, have[0] && have[1] && have[2] && have[3] && haveNhz[0] && haveNhz[1],
            totals[2]+totals[3]+nhz[0]+nhz[1], totals[0]+totals[1]+nhz[0]+nhz[1]);
}

void publishWpmDisplayNhz() {
    // NHZ counters have no fractional-day requirement: expose the complete
    // SUM (including its kWh remainder) separately from the compressor totals.
    const char* nhzBases[] = {"WAERMEERTRAG_2WE_HEIZ", "WAERMEERTRAG_2WE_WW"};
    const int nhzIndices[] = {CALC_IDX_WPL9_NHZ_HEIZ, CALC_IDX_WPL9_NHZ_WW};
    for (int i = 0; i < 2; ++i) {
        const std::string b(nhzBases[i]);
        auto hi = copEnergyValues.find(b+"_SUM_MWH");
        auto lo = copEnergyValues.find(b+"_SUM_KWH");
        if (hi == copEnergyValues.end() || lo == copEnergyValues.end()) continue;
        const auto& cfg = calculatedSensors[nhzIndices[i]];
        char value[24];
        snprintf(value, sizeof(value), "%.3f", hi->second*1000.0f+lo->second);
        publishCalculatedSensorDiscovery(cfg, false);
        publishCalculatedState(cfg.stateTopic, value);
    }
}



// Energy in MWh incl. the kWh remainder register when the model provides it
// (e.g. WPL13E: SUM_MWH is whole MWh only, SUM_KWH holds the 0..999 kWh part).
static float copMWh(const std::string& base) {
    float v = copEnergyValues[base + "_SUM_MWH"];
    auto kwh = copEnergyValues.find(base + "_SUM_KWH");
    // 0..999 only: invalid markers (e.g. 0x8000) must not skew the COP
    if (kwh != copEnergyValues.end() && kwh->second >= 0.0f && kwh->second < 1000.0f) v += kwh->second / 1000.0f;
    return v;
}

// Models with kWh remainder registers: wait until all four are known, otherwise
// the COP jumps around after boot while only some remainders have arrived.
static bool copKwhIncomplete() {
    static const char* keys[] = {"WAERMEERTRAG_HEIZ_SUM_KWH", "WAERMEERTRAG_WW_SUM_KWH",
                                 "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", "EL_AUFNAHMELEISTUNG_WW_SUM_KWH"};
    int have = 0;
    for (const char* k : keys) have += copEnergyValues.count(k);
    return have > 0 && have < 4;
}

// Calculate and publish COP values if all required data is available
void updateCOPCalculations() {
    publishCOPDiscovery();
    if (kModel.wpmDisplayCounters) {
        publishWpmDisplayCOP();
        publishCombinedEnergySensors();
        return;
    }
    if (copKwhIncomplete()) {
        publishCombinedEnergySensors();
        return;
    }
    
    // COP WW: (WAERMEERTRAG_WW_SUM + WAERMEERTRAG_2WE_WW_SUM) / EL_AUFNAHMELEISTUNG_WW_SUM
    if (copEnergyValues.find("WAERMEERTRAG_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_SUM_MWH") != copEnergyValues.end()) {
        
        float el_ww = copMWh("EL_AUFNAHMELEISTUNG_WW");
        if (el_ww > 0.001f) { // Avoid division by zero
            float waerme_ww = copMWh("WAERMEERTRAG_WW") + copMWh("WAERMEERTRAG_2WE_WW");
            float cop_ww = waerme_ww / el_ww;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_ww);
            const char* stateTopic = calculatedSensors[CALC_IDX_COP_WW].stateTopic;
            publishCalculatedState(stateTopic, valueStr);
            ESP_LOGD("COP", "COP WW: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_ww, waerme_ww, el_ww);
        }
    }
    
    // COP Heizung: (WAERMEERTRAG_HEIZ_SUM + WAERMEERTRAG_2WE_HEIZ_SUM) / EL_AUFNAHMELEISTUNG_HEIZ_SUM
    if (copEnergyValues.find("WAERMEERTRAG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH") != copEnergyValues.end()) {
        
        float el_heiz = copMWh("EL_AUFNAHMELEISTUNG_HEIZ");
        if (el_heiz > 0.001f) { // Avoid division by zero
            float waerme_heiz = copMWh("WAERMEERTRAG_HEIZ") + copMWh("WAERMEERTRAG_2WE_HEIZ");
            float cop_heiz = waerme_heiz / el_heiz;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_heiz);
            const char* stateTopic = calculatedSensors[CALC_IDX_COP_HEIZ].stateTopic;
            publishCalculatedState(stateTopic, valueStr);
            ESP_LOGD("COP", "COP Heizung: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_heiz, waerme_heiz, el_heiz);
        }
    }
    
    // COP Gesamt: (all WAERMEERTRAG) / (all EL_AUFNAHMELEISTUNG)
    if (copEnergyValues.find("WAERMEERTRAG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("WAERMEERTRAG_2WE_WW_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_SUM_MWH") != copEnergyValues.end()) {
        
        float el_total = copMWh("EL_AUFNAHMELEISTUNG_HEIZ") + copMWh("EL_AUFNAHMELEISTUNG_WW");
        if (el_total > 0.001f) { // Avoid division by zero
            float waerme_total = copMWh("WAERMEERTRAG_HEIZ") + copMWh("WAERMEERTRAG_2WE_HEIZ") +
                               copMWh("WAERMEERTRAG_WW") + copMWh("WAERMEERTRAG_2WE_WW");
            float cop_gesamt = waerme_total / el_total;
            
            char valueStr[16];
            snprintf(valueStr, sizeof(valueStr), "%.2f", cop_gesamt);
            const char* stateTopic = calculatedSensors[CALC_IDX_COP_GESAMT].stateTopic;
            publishCalculatedState(stateTopic, valueStr);
            ESP_LOGD("COP", "COP Gesamt: %.2f (Wärme: %.3f MWh, El: %.3f MWh)", cop_gesamt, waerme_total, el_total);
        }
    }

    // === KOMBINIERTE ENERGIE-GESAMTWERTE (MWh*1000 + kWh) ===
    // Zusaetzliche Sensoren; die Einzelwerte (MWh/kWh) bleiben fuer Kompatibilitaet.
    publishCombinedEnergySensors();
}

// ============================================================================
// DEGLITCH FUER KOMBINIERTE ENERGIE-GESAMTWERTE
// ============================================================================
// Die Gesamtwerte werden als MWh*1000 + kWh berechnet. Die Einzelregister liegen
// in einem nur nach Signalnamen geschluesselten Cache, daher koennen MWh und kWh
// kurzzeitig von unterschiedlichen CAN-Membern stammen (oder einen ungueltigen
// Marker wie 0x8000 -> 32768000 tragen). Das erzeugt transiente Spitzen von ca.
// +/-1000 kWh (nachts, wenn das Register aktualisiert/umlaeuft) sowie kleinere.
// Echte Energiezaehler wachsen nur, deshalb:
//   - kleine, nicht sinkende Schritte werden sofort akzeptiert,
//   - jede Verkleinerung oder ein Sprung > MAX_SMALL_STEP gilt als verdaechtig;
//     der letzte gute Wert wird weiter gesendet, bis der neue Wert CONFIRM_MS
//     lang stabil bleibt (ein Glitch verschwindet lange vorher).
static std::unordered_map<std::string, float> stableValue;    // zuletzt akzeptierter Wert
static std::unordered_map<std::string, float> pendingValue;   // verdaechtiger Kandidat
static std::unordered_map<std::string, unsigned long> pendingSince;

static bool sanitizeEnergyTotal(const char* key, float raw, float& out) {

    // Toleranz "gleicher Wert": SUM fuehrt nur ganze kWh, beim Tageswechsel faellt
    // der Wh-Anteil weg. Solche Ruecksprunge bleiben unsichtbar (der Wert bleibt stehen).
    const float TOL_EPS = 1.0f;
    const float MAX_SMALL_STEP = 20.0f;         // normales Wachstum (kWh), sofort ok
    const float TOL = 25.0f;                    // Naehe eines Kandidaten zu seiner Region
    const unsigned long CONFIRM_MS = 900000UL;  // 15 min stabile Bestaetigung

    // Klar ungueltige Marker (0x8000 -> 32768000, -255, ...) verwerfen.
    // Rueckgabe = "ein publizierbarer Wert liegt in out vor".
    if (raw <= -1000.0f || raw >= 1000000.0f) {
        auto s = stableValue.find(key);
        if (s != stableValue.end()) { out = s->second; return true; }
        return false;   // noch kein stabiler Wert -> nicht publizieren
    }

    auto it = stableValue.find(key);
    if (it == stableValue.end()) {              // ersten Wert uebernehmen
        stableValue[key] = raw;
        out = raw;
        return true;
    }
    const float last = it->second;

    // Normales, nicht sinkendes Wachstum -> sofort akzeptieren
    if (raw >= last - TOL_EPS && raw <= last + MAX_SMALL_STEP) {
        const float kept = (raw > last) ? raw : last;
        stableValue[key] = kept;
        pendingValue.erase(key);
        pendingSince.erase(key);
        out = kept;
        return true;
    }

    // Verdaechtig: Verkleinerung oder grosser Sprung -> erst nach Stablezeit uebernehmen
    auto p = pendingValue.find(key);
    float diff = (p != pendingValue.end()) ? (p->second - raw) : 0.0f;
    if (diff < 0.0f) diff = -diff;
    if (p == pendingValue.end() || diff > TOL) {
        pendingValue[key] = raw;
        pendingSince[key] = millis();
    } else if ((millis() - pendingSince[key]) >= CONFIRM_MS) {
        stableValue[key] = raw;
        pendingValue.erase(key);
        pendingSince.erase(key);
        out = raw;
        return true;
    }

    out = last;   // letzten guten Wert weiter senden
    return true;
}

// ============================================================================
// GLEITENDER ENERGIE-GESAMTWERT (NVS-Sockel + Tagesanteil, ohne Rollover-Doppelzaehlung)
// ============================================================================
// Der Geraete-Gesamtzahler (SUM_MWH/SUM_KWH) aktualisiert sich nur einmal pro
// Nacht; die Tageszaehler (TAG_KWH/TAG_WH) laufen den Tag ueber. Der gleitende
// Gesamtwert = SUM_MWH*1000 + SUM_KWH + TAG_KWH + TAG_WH/1000 ist daher
// fortlaufend. Damit sich SUM und TAG nicht mit den gleichnamigen Registern
// anderer Member vermischen, werden die Energiewerte zusaetzlich member-scharf
// gespeichert und fuer den WPL17 ausschliesslich aus HEIZMODUL_WPL (0x514)
// gelesen.
static std::unordered_map<std::string, float> copEnergyByMember;   // "MEMBER|SIGNAL"

inline void storeCOPEnergyValueMember(const char* memberName, const char* signalName, const std::string &value) {
    if (value.empty()) return;
    bool hasDigit = false, isValid = true;
    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c >= '0' && c <= '9') hasDigit = true;
        else if (c != '.' && c != '-' && c != '+' && c != ' ') { isValid = false; break; }
    }
    if (!hasDigit || !isValid) return;
    copEnergyByMember[std::string(memberName) + "|" + signalName] = std::atof(value.c_str());
}

inline bool getEnergyForMember(const char* memberName, const char* signalName, float& out) {
    auto it = copEnergyByMember.find(std::string(memberName) + "|" + signalName);
    if (it == copEnergyByMember.end()) return false;
    out = it->second;
    return true;
}

inline bool getSlidingTotalKwh(const char* memberName, const char* sumMwh, const char* sumKwh,
                               const char* tagKwh, const char* tagWh, float& out) {
    float sm, sk, tk, tw;
    if (getEnergyForMember(memberName, sumMwh, sm) && getEnergyForMember(memberName, sumKwh, sk) &&
        getEnergyForMember(memberName, tagKwh, tk) && getEnergyForMember(memberName, tagWh, tw)) {
        out = sm * 1000.0f + sk + tk + tw / 1000.0f;
        return true;
    }
    return false;
}

// NVS-persistierter SUM-Sockel: total = Sockel + Tagesanteil. Die WPL17 liefert
// die SUM-Register nur einmal nachts bzw. nicht auf Anforderung; der Sockel
// uebersteht daher Reboots und der Gesamtwert bleibt fortlaufend.
struct EnergyBaseStorage { uint32_t magic; float v[4]; };
static ESPPreferenceObject energyBasePref;
static EnergyBaseStorage energyBase = {0x454E4B42u, {0, 0, 0, 0}};
static bool energyBaseReady = false;

inline void ensureEnergyBase() {
    if (energyBaseReady) return;
    energyBaseReady = true;
    energyBasePref = global_preferences->make_preference<EnergyBaseStorage>(
        kModel.wpmDisplayCounters ? 0x574C3945u : 0x454E4B42u,
        true);
    EnergyBaseStorage tmp;
    if (energyBasePref.load(&tmp) && tmp.magic == 0x454E4B42u) energyBase = tmp;
}

// Zustand fuer den naechtlichen Rollover. SUM und TAG aktualisieren sich zeitlich
// versetzt: SUM springt ~23:49 auf den neuen Wert (der den abgelaufenen Tag bereits
// enthaelt), TAG wird erst ~23:51 auf 0 gesetzt. In diesem Fenster wuerde SUM + TAG
// den Vortag doppelt zaehlen ("Mitternachts-Peak"). Deshalb wird der beim SUM-Wechsel
// aktuelle Tagesanteil als "gefaltet" gemerkt und nicht erneut addiert; beim
// Tagesreset (TAG faellt auf ~0) ist er abgegolten.
static float energyTagLast[4]   = {0.0f, 0.0f, 0.0f, 0.0f};   // letzter Tagesanteil (kWh)
static float energyTagFolded[4] = {0.0f, 0.0f, 0.0f, 0.0f};   // in SUM bereits enthaltener Anteil
static bool  energyTagValid[4]  = {false, false, false, false};

// One atomic NVS record for the four accepted combined totals AND the associated
// SUM/TAG folding state. Restoring just the floor would still double-count TAG
// when rebooting between the midnight SUM step and the later TAG reset.
struct WpmEnergyCheckpoint {
    uint32_t magic = 0x57454331u; // WEC1, independent of the existing base record
    uint32_t totalMask = 0, tagMask = 0;
    float total[4] = {}, base[4] = {}, tag[4] = {}, folded[4] = {};
};
static const char* const wpmEnergyKeys[] = {
    "combined_energie_heiz", "combined_energie_ww", "combined_waerme_heiz", "combined_waerme_ww"
};
static WpmEnergyCheckpoint wpmEnergyCheckpoint;
static ESPPreferenceObject wpmEnergyCheckpointPref;
static bool wpmEnergyCheckpointLoaded = false;

inline bool validWpmEnergyCheckpoint(const WpmEnergyCheckpoint& c) {
    if (c.magic != 0x57454331u || (c.totalMask & ~15u) || (c.tagMask & ~15u)) return false;
    for (int i = 0; i < 4; ++i) {
        for (float v : {c.total[i], c.base[i], c.tag[i], c.folded[i]})
            if (!std::isfinite(v) || v < 0 || v >= 1000000.0f) return false;
    }
    return true;
}

inline void ensureWpmEnergyCheckpoint() {
    if (!kModel.wpmDisplayCounters || wpmEnergyCheckpointLoaded) return;
    wpmEnergyCheckpointLoaded = true;
    wpmEnergyCheckpointPref = global_preferences->make_preference<WpmEnergyCheckpoint>(0x57454331u, true);
    WpmEnergyCheckpoint saved;
    if (!wpmEnergyCheckpointPref.load(&saved) || !validWpmEnergyCheckpoint(saved)) return;
    wpmEnergyCheckpoint = saved;
    for (int i = 0; i < 4; ++i) {
        if (saved.totalMask & (1u << i)) stableValue[wpmEnergyKeys[i]] = saved.total[i];
        energyBase.v[i] = saved.base[i];
        energyTagLast[i] = saved.tag[i];
        energyTagFolded[i] = saved.folded[i];
        energyTagValid[i] = (saved.tagMask & (1u << i)) != 0;
    }
}

inline bool checkpointWpmEnergy(int slot, float total) {
    if (!kModel.wpmDisplayCounters) return true;
    ensureWpmEnergyCheckpoint();
    WpmEnergyCheckpoint next = wpmEnergyCheckpoint;
    next.totalMask |= 1u << slot;
    next.total[slot] = total;
    next.tagMask = 0;
    bool changed = next.totalMask != wpmEnergyCheckpoint.totalMask ||
                   total != wpmEnergyCheckpoint.total[slot];
    for (int i = 0; i < 4; ++i) {
        next.base[i] = energyBase.v[i]; next.tag[i] = energyTagLast[i]; next.folded[i] = energyTagFolded[i];
        if (energyTagValid[i]) next.tagMask |= 1u << i;
        changed = changed || next.base[i] != wpmEnergyCheckpoint.base[i] ||
                  next.tag[i] != wpmEnergyCheckpoint.tag[i] || next.folded[i] != wpmEnergyCheckpoint.folded[i];
    }
    changed = changed || next.tagMask != wpmEnergyCheckpoint.tagMask;
    if (!changed) return true;
    // Only changed counter/folding data writes flash (normally at ten-minute
    // counter polls). Commit BEFORE publishing so even abrupt power loss cannot
    // discard a value that Home Assistant has already received.
    if (!wpmEnergyCheckpointPref.save(&next) || !global_preferences->sync()) {
        ESP_LOGW("ENERGIE", "WPM checkpoint not durable; delaying combined total publication");
        return false;
    }
    wpmEnergyCheckpoint = next;
    return true;
}

// Member mit den Energiezaehlern: WPL17 0x514, WPMW II 0x500.
inline const char* energyMemberName() { return isWpmw2Model() ? "HEIZMODUL" : "HEIZMODUL_WPL"; }

// slot: 0=EL Heizung, 1=EL Warmwasser, 2=Waerme Heizung, 3=Waerme Warmwasser
inline bool wplBaseTotal(int slot, const char* sumMwh, const char* sumKwh,
                         const char* tagKwh, const char* tagWh, float& out) {
    ensureEnergyBase();
    ensureWpmEnergyCheckpoint();
    float sm, sk, tk, tw;

    // Tagesanteil (TAG_KWH + TAG_WH/1000) lesen. Er ist innerhalb eines Tages
    // monoton und faellt nur beim Tageswechsel (~23:51) auf 0 zurueck.
    const char* member = energyMemberName();
    if (kModel.wpmDisplayCounters &&
        (!getEnergyForMember(member, sumMwh, sm) || !getEnergyForMember(member, sumKwh, sk))) return false;
    bool haveTag = getEnergyForMember(member, tagKwh, tk) &&
                   getEnergyForMember(member, tagWh, tw);
    float tag = haveTag ? (tk + tw / 1000.0f) : 0.0f;

    // Tageswechsel erkannt -> der gefaltete Anteil ist abgegolten.
    // Wichtig: TAG_KWH und TAG_WH setzen NICHT exakt gleichzeitig zurueck. Im
    // Zwischenzyklus (ein Zaehler schon neu, der andere noch alt) faellt der
    // Tagesanteil nur um <1 kWh - das ist KEIN Tageswechsel. Ein echter Reset
    // faellt dagegen auf ~0 bzw. um den ganzen Tag. Sonst wuerde der noch alte
    // Zaehleranteil erneut addiert und erzeugt weiter einen Peak.
    const float TAG_RESET_NEAR = 0.5f;   // Tagesanteil praktisch bei 0
    const float TAG_RESET_DROP = 1.0f;   // oder Rueckgang um mehr als 1 kWh
    if (haveTag && energyTagValid[slot] &&
        (tag < TAG_RESET_NEAR || tag < energyTagLast[slot] - TAG_RESET_DROP)) {
        energyTagFolded[slot] = 0.0f;
    }
    if (haveTag) { energyTagLast[slot] = tag; energyTagValid[slot] = true; }

    // SUM-Sockel (MWh*1000 + kWh) lesen. Bei Aenderung (naechtlicher Rollover)
    // ist der aktuelle Tagesanteil in SUM bereits enthalten -> merken, damit er
    // nicht doppelt addiert wird.
    if (getEnergyForMember(member, sumMwh, sm) && getEnergyForMember(member, sumKwh, sk)) {
        float base = sm * 1000.0f + sk;
        if ((base > 0.0f || (kModel.wpmDisplayCounters && base == 0.0f)) && base != energyBase.v[slot]) {   // nur bei Aenderung schreiben (NVS-Schonung)
            if (energyBase.v[slot] > 0.0f && haveTag) energyTagFolded[slot] = tag;
            energyBase.v[slot] = base;
            energyBasePref.save(&energyBase);
        }
    }
    if (energyBase.v[slot] <= 0.0f && !kModel.wpmDisplayCounters) return false;
    // Nach einem Neustart kommt der NVS-Sockel vor dem Tagesanteil: ohne ihn waere der
    // Wert um den heutigen Verbrauch zu klein (Ruecksprung). Dann lieber nichts senden.
    if (!haveTag) return false;
    out = energyBase.v[slot];
    float add = tag - energyTagFolded[slot];
    out += (add > 0.0f) ? add : 0.0f;   // nie negativ -> kein Ruecksprung
    return true;
}



// Publiziert kombinierte Gesamt-kWh-Werte aus den einzelnen MWh- und kWh-Registern.
// Formel: Gesamt_kWh = MWh_Reg * 1000 + kWh_Reg
// Die Einzelregister (MWh, kWh) werden weiterhin separat veroeffentlicht.
void publishCombinedEnergySensors() {
    if (kModel.wpmDisplayCounters) publishWpmDisplayNhz();
    // Gesamtwert = SUM (MWh*1000 + kWh) + heutiger Tagesanteil (kWh + Wh/1000),
    // gelesen vom Member HEIZMODUL_WPL; Fallback auf den Namen-gekeyten SUM
    // anderer Modelle (ohne WPL-Member).
    // Fuer WPL17 ausschliesslich den member-scharfen gleitenden Wert nutzen
    // (kein Legacy-Fallback), sonst wuerde der Deglitch den niedrigeren,
    // korrekten Startwert als "Verkleinerung" verwerfen.
    // Fuer WPL17 ausschliesslich den member-scharfen gleitenden Wert
    // (SUM_MWH*1000 + SUM_KWH + TAG_KWH + TAG_WH/1000) aus HEIZMODUL_WPL nutzen.
    // Damit ist der totale Zaehler fortlaufend (kein Sprung beim naechtlichen
    // Rollover). Die SUM-Register werden dazu mit FREQ_1MIN gelesen.
    auto publishTotal = [](const CalculatedSensorConfig& cfg, const char* topic, const char* key, int slot,
                           const char* sumMwh, const char* sumKwh, const char* tagKwh, const char* tagWh) {
        float rawTotal; bool have = false;
        if (wplBaseTotal(slot, sumMwh, sumKwh, tagKwh, tagWh, rawTotal)) {
            have = true;   // WPL-Basis (NVS-Sockel + Tagesanteil), sobald SUM bekannt
        } else if (!kModel.wpmDisplayCounters && energyBase.v[slot] <= 0.0f &&   // Geraet ohne Tageszaehler-Sockel
                   copEnergyValues.find(sumMwh) != copEnergyValues.end() &&
                   copEnergyValues.find(sumKwh) != copEnergyValues.end()) {
            rawTotal = copEnergyValues[sumMwh] * 1000.0f + copEnergyValues[sumKwh];
            have = true;   // Fallback: Geraete-SUM (naechtlich aktualisiert)
        }
        if (!have) return;
        float total;
        if (!sanitizeEnergyTotal(key, rawTotal, total)) return;   // kein Wert -> nicht senden
        if (!checkpointWpmEnergy(slot, total)) return;
        char valueStr[32];
        snprintf(valueStr, sizeof(valueStr), "%.3f", total);
        publishCalculatedSensorDiscovery(cfg, false);
        publishCalculatedState(topic, valueStr);
        ESP_LOGD("ENERGIE", "Gesamt (%s): %.3f kWh", key, total);
    };
    publishTotal(calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH], ENERGY_CALC_TOPIC("energie_heiz_total_kwh"), "combined_energie_heiz", 0,
                 "EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH",
                 "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH");
    publishTotal(calculatedSensors[CALC_IDX_ENERGIE_WW_TOTAL_KWH], ENERGY_CALC_TOPIC("energie_ww_total_kwh"), "combined_energie_ww", 1,
                 "EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "EL_AUFNAHMELEISTUNG_WW_SUM_KWH",
                 "EL_AUFNAHMELEISTUNG_WW_TAG_KWH", "EL_AUFNAHMELEISTUNG_WW_TAG_WH");
    publishTotal(calculatedSensors[CALC_IDX_WAERME_HEIZ_TOTAL_KWH], ENERGY_CALC_TOPIC("waerme_heiz_total_kwh"), "combined_waerme_heiz", 2,
                 "WAERMEERTRAG_HEIZ_SUM_MWH", "WAERMEERTRAG_HEIZ_SUM_KWH",
                 "WAERMEERTRAG_HEIZ_TAG_KWH", "WAERMEERTRAG_HEIZ_TAG_WH");
    publishTotal(calculatedSensors[CALC_IDX_WAERME_WW_TOTAL_KWH], ENERGY_CALC_TOPIC("waerme_ww_total_kwh"), "combined_waerme_ww", 3,
                 "WAERMEERTRAG_WW_SUM_MWH", "WAERMEERTRAG_WW_SUM_KWH",
                 "WAERMEERTRAG_WW_TAG_KWH", "WAERMEERTRAG_WW_TAG_WH");

    // === AKTUELLE ELEKTRISCHE LEISTUNG (W) + ABGELEITETER BETRIEBSSTATUS ===
    // Beides aus den feinen Tageszaehlern EL_*_TAG_WH (1 Wh Aufloesung):
    //  - Leistung = (Wh_neu - Wh_alt) / (t_neu - t_alt) * 3600 -> W
    //  - Betriebsstatus: welcher der beiden getrennten Verbrauchszaehler steigt,
    //    verraet den Betrieb (so trennt die WP selbst Heizen und Warmwasser).
    //    Das WP_STATUS-Bitfeld des Geraets ist auf der WPL17 unbrauchbar (immer 0).
    if (copEnergyValues.find("EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH") != copEnergyValues.end() &&
        copEnergyValues.find("EL_AUFNAHMELEISTUNG_WW_TAG_WH") != copEnergyValues.end()) {

        static bool init = false;
        static float lastHeizWh = 0.0f, lastWwWh = 0.0f;
        static float accHeizWh = 0.0f, accWwWh = 0.0f;
        static uint32_t windowStartMs = 0;
        static uint32_t lastStatusMs = 0;
        static const char* opState = "Aus";

        float heizWh = copEnergyValues["EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH"];
        float wwWh   = copEnergyValues["EL_AUFNAHMELEISTUNG_WW_TAG_WH"];
        uint32_t nowMs = millis();

        if (init) {
            float dH = heizWh - lastHeizWh; if (dH > 0.0f) accHeizWh += dH;
            float dW = wwWh   - lastWwWh;   if (dW > 0.0f) accWwWh   += dW;
        } else {
            windowStartMs = nowMs; lastStatusMs = nowMs;
        }
        lastHeizWh = heizWh; lastWwWh = wwWh; init = true;

        // Verdichter: 0.8 = steht, 2.4 = laeuft (Heizmodul)
        bool running = (lastVerdichterValue > 1.5f);

        // Betriebsstatus live (alle 30 s): laeuft/steht aus dem Verdichter,
        // Heizen/Warmwasser aus den Energie-Deltas der letzten Minuten.
        if (nowMs - lastStatusMs >= 30000UL) {
            const char* ns;
            float sum = accHeizWh + accWwWh;
            if (!running) {
                ns = "Aus";
            } else if (copEnergyValues.find("KUEHLZUSTAND") != copEnergyValues.end() &&
                       copEnergyValues["KUEHLZUSTAND"] > 0.5f) {
                ns = "Kuehlen";
            } else if (isWpmw2Model() && lastPumpenstatus >= 0) {
                // WPMW II: die TAG_WH-Zaehler springen nur stuendlich, die Ladepumpe zeigt
                // den Warmwasserbetrieb sofort an.
                ns = isHotWaterCharging(lastPumpenstatus) ? "Warmwasser" : "Heizen";
            } else if (sum > 0.0f && accWwWh > accHeizWh) {
                ns = "Warmwasser";
            } else if (sum > 0.0f) {
                ns = "Heizen";
            } else {
                ns = (strcmp(opState, "Warmwasser") == 0) ? "Warmwasser" : "Heizen";
            }
            opState = ns;
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_BETRIEBSSTATUS], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/betriebsstatus/state", opState);
            lastStatusMs = nowMs;
        }

        // Leistung als 5-Minuten-Mittel (die TAG-Zaehler springen grob). Auf dem WPMW II
        // stehen sie waehrend eines Laufs still, eine Live-Leistung waere dort falsch; das
        // Fenster laeuft dort trotzdem, damit die Summen fuer den Betriebszustand (bis
        // PUMPENSTATUS bekannt ist) nur die letzten Minuten zaehlen.
        if (nowMs - windowStartMs >= 300000UL && !isWpmw2Model()) {
            float dtSec = (nowMs - windowStartMs) / 1000.0f;
            float pHeiz = running ? accHeizWh / dtSec * 3600.0f : 0.0f;
            float pWw   = running ? accWwWh   / dtSec * 3600.0f : 0.0f;
            char v[16], ph[16], pw[16];
            snprintf(v, sizeof(v), "%.0f", pHeiz + pWw);
            snprintf(ph, sizeof(ph), "%.0f", pHeiz);
            snprintf(pw, sizeof(pw), "%.0f", pWw);
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_W], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/el_leistung_w/state", v);
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_HEIZ_W], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/el_leistung_heiz_w/state", ph);
            publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_WW_W], false);
            publishCalculatedState(HA_TOPIC_ROOT "/calculated/el_leistung_ww_w/state", pw);
            ESP_LOGD("ENERGIE", "Leistung: %.0f W (Heiz %.0f, WW %.0f) in %.0fs", pHeiz + pWw, pHeiz, pWw, dtSec);
        }
        if (nowMs - windowStartMs >= 300000UL) {   // new window on every model
            accHeizWh = 0.0f; accWwWh = 0.0f; windowStartMs = nowMs;
        }
    }

    // === TAGES-ENERGIEWERTE: kWh-Anteil + Wh/1000 (Wh-Aufloesung) ===
    // Aktualisieren laufend und fallen ~Mitternacht auf 0 zurueck. HA behandelt
    // das bei state_class total_increasing korrekt (Reset = Zaehlerneustart) und
    // nutzt diese Werte als Basis fuer utility_meter (Tag/Monat/Jahr).
    {
        auto pubDaily = [](const char* kwhKey, const char* whKey, const CalculatedSensorConfig& cfg, const char* topic) {
            auto a = copEnergyValues.find(kwhKey);
            auto b = copEnergyValues.find(whKey);
            if (a == copEnergyValues.end() || b == copEnergyValues.end()) return;
            float daily = a->second + b->second / 1000.0f;
            char s[24];
            snprintf(s, sizeof(s), "%.3f", daily);
            publishCalculatedSensorDiscovery(cfg, false);
            publishCalculatedState(topic, s);
        };
        pubDaily("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH", calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TAG_KWH], ENERGY_CALC_TOPIC("energie_heiz_tag_kwh"));
        pubDaily("EL_AUFNAHMELEISTUNG_WW_TAG_KWH",   "EL_AUFNAHMELEISTUNG_WW_TAG_WH",   calculatedSensors[CALC_IDX_ENERGIE_WW_TAG_KWH], ENERGY_CALC_TOPIC("energie_ww_tag_kwh"));
        pubDaily("WAERMEERTRAG_HEIZ_TAG_KWH",        "WAERMEERTRAG_HEIZ_TAG_WH",        calculatedSensors[CALC_IDX_WAERME_HEIZ_TAG_KWH], ENERGY_CALC_TOPIC("waerme_heiz_tag_kwh"));
        pubDaily("WAERMEERTRAG_WW_TAG_KWH",          "WAERMEERTRAG_WW_TAG_WH",          calculatedSensors[CALC_IDX_WAERME_WW_TAG_KWH], ENERGY_CALC_TOPIC("waerme_ww_tag_kwh"));
    }
}

// Validation removed for simplification - all values are published as-is
// Invalid values like -255, -32768, etc. will be visible in Home Assistant
// Users can filter these in HA automations/dashboards if needed

// Discovery republish: the COP configs go out again with the next COP update
inline void copDiscoveryRepublish() { copDiscoveryPublished = false; }

#endif  // ENERGY_H

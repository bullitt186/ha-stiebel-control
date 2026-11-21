#ifdef HA_HAS_WPM_DISPLAY_COUNTERS
// Simulate the independently repeated SUM confirmation read, not just one torn pair.
static void wpmTestUpdate(const CanMember& cm, const ElsterIndex* ei, const std::string& value) {
    updateSensor(cm, ei, value);
    const std::string n(ei->Name);
    if (n.size() > 8 && n.substr(n.size()-8) == "_SUM_KWH") {
        const std::string key=n.substr(0,n.size()-4);
        auto it=wpl9EnergyPairs.find(key);
        if (it!=wpl9EnergyPairs.end() && !it->second.haveLast && it->second.bootCandidate) {
            updateSensor(cm,GetElsterIndex((key+"_MWH").c_str()),std::to_string((unsigned)it->second.high));
            updateSensor(cm,ei,value);
        }
    }
}
TEST_CASE("WPL9 display counters combine fresh parts without rollover spikes", "[wpl9-counters]") {
    Wpl9StartCounter c;
    uint32_t n = 123;
    CHECK_FALSE(c.update(true, "8", 1000, n));
    REQUIRE(c.update(false, "941", 2000, n));
    CHECK(n == 8941);
    // Reordered or delayed partial replies cannot form a new pair.
    CHECK_FALSE(c.update(false, "942", 3000, n));
    REQUIRE(c.update(true, "8", 4000, n));
    CHECK(n == 8942);
    CHECK_FALSE(c.update(true, "8", 5000, n));
    CHECK_FALSE(c.update(false, "-255", 6000, n));
    CHECK_FALSE(c.update(false, "943", 7000, n));
    CHECK_FALSE(c.update(true, "8", 30000, n));
    REQUIRE(c.update(false, "943", 31000, n));
    CHECK(n == 8943);
    // A larger legitimate gap needs a second complete pair.
    CHECK_FALSE(c.update(true, "8", 32000, n));
    CHECK_FALSE(c.update(false, "999", 33000, n));
    CHECK_FALSE(c.update(true, "8", 34000, n));
    REQUIRE(c.update(false, "999", 35000, n));
    CHECK(n == 8999);
    // Kilo increments before the remainder: do not emit 9999.
    CHECK_FALSE(c.update(true, "9", 36000, n));
    CHECK_FALSE(c.update(false, "999", 37000, n));
    CHECK_FALSE(c.update(true, "9", 38000, n));
    REQUIRE(c.update(false, "0", 39000, n));
    CHECK(n == 9000);
    CHECK_FALSE(c.update(true, "8", 40000, n));
    CHECK_FALSE(c.update(false, "0", 41000, n));
    CHECK(n == 9000);
}

TEST_CASE("WPL9 lifetime counter rejects invalid parts and handles clock wrap", "[wpl9-counters]") {
    for (const char* invalid : {"-255", "32768", "65536", "nan", "8.1", "8x", ""}) {
        Wpl9StartCounter c;
        uint32_t n = 0;
        CHECK_FALSE(c.update(true, invalid, 0, n));
        CHECK_FALSE(c.update(false, "941", 1000, n));
    }
    Wpl9StartCounter c;
    uint32_t n = 0;
    CHECK_FALSE(c.update(false, "1000", 0, n));
    CHECK_FALSE(c.update(true, "8", 0xfffffff0U, n));
    REQUIRE(c.update(false, "941", 15, n));
    CHECK(n == 8941);
}

TEST_CASE("WPL9 counter CAN replies announce a hardware total and republish it", "[wpl9-counters][can]") {
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    wpmEnergyCheckpoint = {}; wpmEnergyCheckpointLoaded = true;
    resetDiscoveryState();
    wpl9StartCounter = {};
    fake_millis() = 1000;
    fake_can().sent.clear();
    // Observed long-index reply: 8 thousands and 941 remainder.
    processAndUpdate(0x514, {0xd2,0x00,0xfa,0x4e,0xf0,0x00,0x08});
    REQUIRE(fake_can().sent.size() == 1);
    CHECK(fake_can().sent[0] == std::vector<uint8_t>{0xa1,0x14,0xfa,0x4e,0xf1,0,0});
    processAndUpdate(0x514, {0xd2,0x00,0xfa,0x4e,0xf1,0x03,0xad});
    bool found = false;
    for (auto& m : mqtt_client_instance().messages) {
        if (m.topic == wpl9LifetimeStartsSensor.stateTopic) { CHECK(m.payload == "8941"); found = true; }
    }
    REQUIRE(found);
    mqtt_client_instance().clear();
    publishAllCalculatedSensorDiscoveries(true);
    CHECK(mqttFindPayload("homeassistant/sensor/heatingpump/stiebel_calculated_wpl9_verdichter_starts_gesamt/config").find("total_increasing") != std::string::npos);
    CHECK(wpl9StartCounter.last == 8941);
#ifdef HA_LANGUAGE_EN
    CHECK(std::string(wpl9LifetimeStartsSensor.name) == "Compressor lifetime starts (WPM)");
#else
    CHECK(std::string(wpl9LifetimeStartsSensor.name) == "Verdichterstarts gesamt (WPM)");

#endif
}

TEST_CASE("WPL9 energy calculation uses 0x514 electrical counters only", "[wpl9-counters][calc]") {
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    wpmEnergyCheckpoint = {}; wpmEnergyCheckpointLoaded = true;
    resetDiscoveryState();
    copEnergyValues.clear(); copEnergyByMember.clear();
    wpl9EnergyPairs.clear();
    energyBase = {0x454e4b42u, {0,0,0,0}};
    energyBaseReady = true;
    for (int i = 0; i < 4; ++i) { energyTagValid[i] = false; energyTagFolded[i] = 0; }
    const char* names[] = {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH",
                          "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH"};
    const char* oldValues[] = {"2.000", "55", "3.000", "181"};
    const char* newValues[] = {"3.000", "99", "2.000", "896"};
    for (int i = 0; i < 4; ++i) updateSensor(CanMembers[cm_heizmodul], GetElsterIndex(names[i]), oldValues[i]);
    CHECK(copEnergyValues.empty());
    for (int i = 0; i < 4; ++i) wpmTestUpdate(CanMembers[cm_heizmodul_wpl], GetElsterIndex(names[i]), newValues[i]);
    CHECK(copEnergyValues.at(names[1]) == 99);
    bool found = false;
    for (auto& m : mqtt_client_instance().messages) {
        if (m.topic == calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH].stateTopic) {
            CHECK(std::stof(m.payload) == Approx(3101.896f)); found = true;
        }
        CHECK(m.topic != "heatingpump/calculated/energie_heiz_total_kwh/state");
        if (m.topic == "heatingpump/calculated/cop_heiz/state") CHECK(m.payload == "unknown");
    }
    REQUIRE(found);
    CHECK(std::string(calculatedSensors[CALC_IDX_ENERGIE_HEIZ_TOTAL_KWH].uniqueId) == "stiebel_calculated_wpl9_energie_heiz_total_kwh");
    // An old member's unsolicited reply cannot overwrite a corrected value.
    updateSensor(CanMembers[cm_heizmodul], GetElsterIndex(names[1]), "999");
    CHECK(copEnergyValues.at(names[1]) == 99);
    for (const char* invalid : {"-255", "-1", "nan", "1000", "99.5"}) {
        wpmTestUpdate(CanMembers[cm_heizmodul_wpl], GetElsterIndex(names[1]), invalid);
        CHECK(copEnergyValues.at(names[1]) == 99);
    }
    // Old thermal source cannot populate the calculation cache.
    updateSensor(CanMembers[cm_heizmodul], GetElsterIndex("WAERMEERTRAG_HEIZ_SUM_MWH"), "12");
    updateSensor(CanMembers[cm_heizmodul], GetElsterIndex("WAERMEERTRAG_HEIZ_SUM_KWH"), "695");
    CHECK(copEnergyValues.count("WAERMEERTRAG_HEIZ_SUM_MWH") == 0);
    wpmTestUpdate(CanMembers[cm_heizmodul_wpl], GetElsterIndex("WAERMEERTRAG_HEIZ_SUM_MWH"), "19");
    wpmTestUpdate(CanMembers[cm_heizmodul_wpl], GetElsterIndex("WAERMEERTRAG_HEIZ_SUM_KWH"), "535");
    CHECK(copEnergyValues.at("WAERMEERTRAG_HEIZ_SUM_MWH") == 19);
    updateSensor(CanMembers[cm_heizmodul], GetElsterIndex("WAERMEERTRAG_HEIZ_SUM_MWH"), "12");
    CHECK(copEnergyValues.at("WAERMEERTRAG_HEIZ_SUM_MWH") == 19);
    CHECK(energyBase.v[2] == 19535);
}

TEST_CASE("WPL9 fresh energy pairs prevent the recorded carry peak", "[wpl9-counters]") {
    Wpl9EnergyPair p;
    float hi = 0, lo = 0;
    CHECK_FALSE(p.update(true, 1, 0, hi, lo));
    REQUIRE(p.update(false, 996, 1000, hi, lo));
    CHECK(hi + lo/1000 == Approx(1.996));
    CHECK_FALSE(p.update(true, 2, 2000, hi, lo));
    CHECK(hi + lo/1000 == Approx(1.996));
    REQUIRE(p.update(false, 30, 3000, hi, lo));
    CHECK(hi + lo/1000 == Approx(2.030));
    // Even a freshly read low part can predate the device's carry update.
    CHECK_FALSE(p.update(false, 30, 4000, hi, lo));
    CHECK_FALSE(p.update(true, 3, 5000, hi, lo));
    CHECK(hi + lo/1000 == Approx(2.030));
    CHECK_FALSE(p.update(true, 3, 6000, hi, lo));
    REQUIRE(p.update(false, 10, 7000, hi, lo));
    CHECK(hi + lo/1000 == Approx(3.010));
    CHECK_FALSE(p.update(true, 0, 8000, hi, lo));
    CHECK_FALSE(p.update(false, 0, 9000, hi, lo));
    CHECK_FALSE(p.update(true, 0, 9100, hi, lo));
    REQUIRE(p.update(false, 0, 9200, hi, lo));
    CHECK(hi == 0); CHECK(lo == 0); // daily reset
    CHECK_FALSE(p.update(true, 1, 10000, hi, lo));
    CHECK_FALSE(p.update(false, -255, 11000, hi, lo));
    CHECK_FALSE(p.update(false, 42, 12000, hi, lo));
    REQUIRE(p.update(true, 0, 13000, hi, lo));
    CHECK(hi + lo/1000 == Approx(0.042));
    CHECK_FALSE(p.update(true, 0, 14000, hi, lo));
    CHECK_FALSE(p.update(false, 43, 915000, hi, lo)); // stale high part
    // Opposite ordering: the low part rolls first. Avoid a false daily reset.
    Wpl9EnergyPair reverse;
    CHECK_FALSE(reverse.update(true, 1, 0, hi, lo));
    REQUIRE(reverse.update(false, 996, 1, hi, lo));
    CHECK_FALSE(reverse.update(true, 1, 2, hi, lo));
    CHECK_FALSE(reverse.update(false, 30, 3, hi, lo));
    CHECK(hi + lo/1000 == Approx(1.996));
    CHECK_FALSE(reverse.update(false, 30, 4, hi, lo));
    REQUIRE(reverse.update(true, 2, 5, hi, lo));
    CHECK(hi + lo/1000 == Approx(2.030));
}

TEST_CASE("WPL9 photo regression includes NHZ on both COP sides", "[wpl9-counters][calc]") {
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    wpmEnergyCheckpoint = {}; wpmEnergyCheckpointLoaded = true;
    resetDiscoveryState();
    copDiscoveryPublished = false;
    copEnergyValues.clear(); copEnergyByMember.clear(); wpl9EnergyPairs.clear();
    energyBase = {0x454e4b42u, {0,0,0,0}}; energyBaseReady = true;
    for (int i = 0; i < 4; ++i) { energyTagValid[i] = false; energyTagFolded[i] = 0; }
    auto send = [](const char* name, const char* value) {
        wpmTestUpdate(CanMembers[cm_heizmodul_wpl], GetElsterIndex(name), value);
    };
    // Candidate SUM bases plus photographed TAG values, consistent with the
    // display's whole-kWh totals. Exact current SUM bases still need live readback.
    struct Sample { const char* base; const char* mwh; const char* kwh; const char* day; const char* wh; };
    for (auto s : {Sample{"EL_AUFNAHMELEISTUNG_HEIZ","3","105","2","803"},
                   Sample{"EL_AUFNAHMELEISTUNG_WW","2","356","0","309"},
                   Sample{"WAERMEERTRAG_HEIZ","19","535","19","473"},
                   Sample{"WAERMEERTRAG_WW","8","705","0","730"}}) {
        const std::string b(s.base);
        send((b+"_SUM_MWH").c_str(), s.mwh); send((b+"_SUM_KWH").c_str(), s.kwh);
        send((b+"_TAG_KWH").c_str(), s.day); send((b+"_TAG_WH").c_str(), s.wh);
    }
    CHECK(mqttFindPayload("calculated/wpl9_waerme_heiz_tag_kwh/state") == "19.473");
    CHECK(mqttFindPayload("calculated/wpl9_waerme_ww_tag_kwh/state") == "0.730");
    CHECK(std::stof(mqttFindPayload("calculated/wpl9_waerme_heiz_total_kwh/state")) == Approx(19554.473f));
    auto lastPayload = [](const std::string& suffix) {
        for (auto it = mqtt_client_instance().messages.rbegin(); it != mqtt_client_instance().messages.rend(); ++it)
            if (it->topic.find(suffix) != std::string::npos) return it->payload;
        return std::string();
    };
    CHECK(lastPayload("calculated/cop_heiz/state") == "unknown");
    CHECK(lastPayload("calculated/cop_ww/state") == "unknown");
    CHECK(lastPayload("calculated/cop_gesamt/state") == "unknown");
    send("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "0");
    CHECK(lastPayload("calculated/cop_heiz/state") == "unknown");
    send("WAERMEERTRAG_2WE_HEIZ_SUM_KWH", "464");
    CHECK(lastPayload("calculated/cop_heiz/state") == "5.60");
    CHECK(lastPayload("calculated/cop_gesamt/state") == "unknown");
    send("WAERMEERTRAG_2WE_WW_SUM_MWH", "0"); send("WAERMEERTRAG_2WE_WW_SUM_KWH", "765");
    CHECK(mqttFindPayload("HEIZMODUL_WPL/WAERMEERTRAG_2WE_HEIZ_SUM_KWH/state") == "464");
    CHECK(mqttFindPayload("calculated/wpl9_nhz_heiz_total_kwh/state") == "464.000");
    CHECK(mqttFindPayload("calculated/wpl9_nhz_ww_total_kwh/state") == "765.000");
    CHECK(lastPayload("calculated/cop_heiz/state") == "5.60");
    CHECK(lastPayload("calculated/cop_ww/state") == "3.03");
    CHECK(lastPayload("calculated/cop_gesamt/state") == "4.41");
    CHECK(mqttFindPayload("homeassistant/sensor/heatingpump/cop_ww/config").find("float(none)") != std::string::npos);
    CHECK(std::string(calculatedSensors[CALC_IDX_WAERME_HEIZ_TOTAL_KWH].uniqueId) == "stiebel_calculated_wpl9_waerme_heiz_total_kwh");
    mqtt_client_instance().clear();
    send("WAERMEERTRAG_WW_TAG_WH", "-137");
    CHECK(mqtt_client_instance().messages.empty());
    // Reboot with an NVS base must not publish a partial total or numeric COP.
    copEnergyValues.clear(); copEnergyByMember.clear(); wpl9EnergyPairs.clear();
    updateCOPCalculations();
    CHECK(mqttFindPayload("calculated/cop_heiz/state") == "unknown");
    CHECK_FALSE(mqttTopicPublished("calculated/wpl9_waerme_heiz_total_kwh/state"));
    // Fresh zero counters must override a stale nonzero NVS base, not divide by it.
    for (const char* b : {"EL_AUFNAHMELEISTUNG_HEIZ", "EL_AUFNAHMELEISTUNG_WW",
                          "WAERMEERTRAG_HEIZ", "WAERMEERTRAG_WW"}) {
        for (const char* suffix : {"_SUM_MWH", "_SUM_KWH", "_TAG_KWH", "_TAG_WH"})
            send((std::string(b)+suffix).c_str(), "0");
    }
    CHECK(lastPayload("calculated/cop_heiz/state") == "unknown");
    CHECK(energyBase.v[0] == 0);
    send("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "0");
    send("WAERMEERTRAG_2WE_HEIZ_SUM_KWH", "0");
    CHECK(lastPayload("calculated/cop_heiz/state") == "unknown"); // zero denominator
    // NHZ is resistance heating: with no VD electricity/heat, its ratio is 1.
    send("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "0");
    send("WAERMEERTRAG_2WE_HEIZ_SUM_KWH", "464");
    CHECK(lastPayload("calculated/cop_heiz/state") == "1.00");
}

TEST_CASE("WPL9 runtime frames match the display with whole-hour metadata", "[wpl9-counters][can]") {
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    wpmEnergyCheckpoint = {}; wpmEnergyCheckpointLoaded = true;
    resetDiscoveryState();
    struct Sample { uint16_t index, value; const char* signal; };
    for (auto s : {Sample{0x4efb,10599,"LAUFZEIT_VD_HEIZEN"},
                   Sample{0x4efd,3011,"LAUFZEIT_VD_WW"},
                   Sample{0x4f06,78,"LAUFZEIT_VD_ABTAUEN"},
                   Sample{0x07ff,4614,"LZ_VERD_1_KUEHLBETRIEB"}}) {
        mqtt_client_instance().clear();
        processAndUpdate(0x514, {0xd2,0,0xfa,static_cast<uint8_t>(s.index>>8),static_cast<uint8_t>(s.index),static_cast<uint8_t>(s.value>>8),static_cast<uint8_t>(s.value)});
        std::string expected = "heatingpump/HEIZMODUL_WPL/" + std::string(s.signal) + "/state";
        bool found = false;
        for (auto& m : mqtt_client_instance().messages) if (m.topic == expected) {
            CHECK(m.payload == std::to_string(s.value)); found = true;
        }
        CHECK(found);
        const auto* meta = GetElsterMeta(GetElsterIndex(s.signal));
        // Existing cooling metadata uses type defaults; the three new rows are explicit.
        if (s.index != 0x07ff) {
            REQUIRE(meta != nullptr);
            CHECK(std::string(meta->unit) == "h");
        }
    }
}
#endif

TEST_CASE("WPM thermal counters advance above one kWh per polling cycle", "[wpl9-counters]") {
    for (int delta : {1200, 1500, 2000, 3000}) {
        Wpl9EnergyPair p;
        float hi = 0, lo = 0;
        int accepted = 0;
        for (int cycle = 0; cycle < 40; ++cycle) {
            const int wh = 1100 + cycle * delta;
            const uint32_t at = cycle * 600000U;
            CHECK_FALSE(p.update(true, wh / 1000, at, hi, lo));
            if (p.update(false, wh % 1000, at + 100, hi, lo)) ++accepted;
        }
        CHECK(accepted == 40);
        CHECK(hi * 1000 + lo == 1100 + 39 * delta);
    }
}

TEST_CASE("WPM repeated same-high drops cannot fabricate a daily reset", "[wpl9-counters]") {
    Wpl9EnergyPair p;
    float hi = 0, lo = 0;
    auto pair = [&](int h, int l, uint32_t at, int day = 18, bool daily = true) {
        p.update(true, h, at, hi, lo, daily, day);
        return p.update(false, l, at + 100, hi, lo, daily, day);
    };
    REQUIRE(pair(0, 600, 0));
    CHECK_FALSE(pair(0, 50, 600000));
    CHECK_FALSE(pair(0, 100, 1200000));
    CHECK_FALSE(pair(0, 100, 1800000));
    CHECK(hi * 1000 + lo == 600);
    // A real date transition permits a reset, confirmed by two complete pairs.
    CHECK_FALSE(pair(0, 50, 2400000, 19));
    REQUIRE(pair(0, 100, 3000000, 19));
    CHECK(hi * 1000 + lo == 100);
    // SUM is a lifetime counter: even a day change never permits a decrease.
    CHECK_FALSE(pair(0, 0, 3600000, 20, false));
    CHECK_FALSE(pair(0, 0, 4200000, 20, false));
    CHECK(hi * 1000 + lo == 100);
}

TEST_CASE("WPM torn midnight pair is held before reset confirmation", "[wpl9-counters]") {
    Wpl9EnergyPair p;
    float hi = 0, lo = 0;
    auto pair = [&](int h, int l, uint32_t at, int day) {
        p.update(true, h, at, hi, lo, true, day);
        return p.update(false, l, at + 100, hi, lo, true, day);
    };
    REQUIRE(pair(19, 600, 0, 31));
    CHECK_FALSE(pair(0, 600, 600000, 31));
    CHECK_FALSE(pair(0, 50, 1200000, 1));
    CHECK(hi * 1000 + lo == 19600);
    REQUIRE(pair(0, 100, 1800000, 1));
    CHECK(hi * 1000 + lo == 100);
}

#ifdef HA_HAS_WPM_DISPLAY_COUNTERS
TEST_CASE("WPM high-load yield reaches published daily total and COP", "[wpl9-counters][calc]") {
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    wpmEnergyCheckpoint = {}; wpmEnergyCheckpointLoaded = true;
    resetDiscoveryState(); resetStateCache();
    copEnergyValues.clear(); copEnergyByMember.clear(); wpl9EnergyPairs.clear();
    energyBase = {0x454e4b42u, {0,0,0,0}}; energyBaseReady = true;
    for (int i = 0; i < 4; ++i) { energyTagValid[i] = false; energyTagFolded[i] = 0; }
    auto send = [](const std::string& name, int value) {
        wpmTestUpdate(CanMembers[cm_heizmodul_wpl], GetElsterIndex(name.c_str()), std::to_string(value));
    };
    fake_millis() = 0;
    for (const char* base : {"EL_AUFNAHMELEISTUNG_HEIZ", "EL_AUFNAHMELEISTUNG_WW", "WAERMEERTRAG_WW"}) {
        const std::string b(base);
        send(b+"_SUM_MWH", 1); send(b+"_SUM_KWH", 0);
        send(b+"_TAG_KWH", 0); send(b+"_TAG_WH", 0);
    }
    for (const char* base : {"WAERMEERTRAG_2WE_HEIZ", "WAERMEERTRAG_2WE_WW"}) {
        send(std::string(base)+"_SUM_MWH", 0); send(std::string(base)+"_SUM_KWH", 0);
    }
    send("WAERMEERTRAG_HEIZ_SUM_MWH", 5); send("WAERMEERTRAG_HEIZ_SUM_KWH", 0);
    auto latest = [](const char* suffix) {
        for (auto it = mqtt_client_instance().messages.rbegin(); it != mqtt_client_instance().messages.rend(); ++it)
            if (it->topic.find(suffix) != std::string::npos) return it->payload;
        return std::string();
    };
    for (int cycle = 0; cycle < 40; ++cycle) {
        fake_millis() = cycle * 600000U;
        const int wh = 1100 + cycle * 1500;
        send("WAERMEERTRAG_HEIZ_TAG_KWH", wh / 1000);
        send("WAERMEERTRAG_HEIZ_TAG_WH", wh % 1000);
        CHECK(std::stof(latest("calculated/wpl9_waerme_heiz_tag_kwh/state")) == Approx(wh/1000.0));
        // The existing total sanitizer retains values from earlier test cases
        // and confirms this different fixture baseline for 15 minutes.
        if (cycle >= 3)
            CHECK(std::stof(latest("calculated/wpl9_waerme_heiz_total_kwh/state")) == Approx(5000+wh/1000.0));
    }
    CHECK(latest("calculated/cop_heiz/state") == "5.06");
}

#endif

TEST_CASE("WPM midnight permission survives an outage and cannot reset twice", "[wpl9-counters]") {
    Wpl9EnergyPair p;
    float hi = 0, lo = 0;
    auto pair = [&](int h, int l, uint32_t at, int day) {
        p.update(true, h, at, hi, lo, true, day);
        return p.update(false, l, at+100, hi, lo, true, day);
    };
    REQUIRE(pair(0, 600, 0, 18));
    REQUIRE(pair(0, 700, 600000, 19));
    CHECK_FALSE(pair(0, 50, 3600000, 19));
    REQUIRE(pair(0, 100, 4200000, 19));
    CHECK(lo == 100);
    Wpl9EnergyPair q;
    auto resetPair = [&](int h, int l, uint32_t at, int day) {
        q.update(true,h,at,hi,lo,true,day);
        return q.update(false,l,at+100,hi,lo,true,day);
    };
    REQUIRE(resetPair(19,600,0,18));
    CHECK_FALSE(resetPair(0,50,600000,18));
    REQUIRE(resetPair(0,100,1200000,18));
    REQUIRE(resetPair(0,600,1800000,19));
    CHECK_FALSE(resetPair(0,50,2400000,19));
    CHECK_FALSE(resetPair(0,100,3000000,19));
    CHECK(lo == 600);
}

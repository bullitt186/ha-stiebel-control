TEST_CASE("WPM SUM boot confirmation rejects a first torn MWh carry", "[wpl9-counters][boot]") {
    Wpl9EnergyPair p;
    float hi=-1,lo=-1;
    auto pair=[&](int h,int l,uint32_t t) {
        p.update(true,h,t,hi,lo,false,21);
        return p.update(false,l,t+100,hi,lo,false,21);
    };
    CHECK_FALSE(pair(4,998,0)); // actual unit advanced from 3/998 to 4/001
    CHECK_FALSE(pair(4,1,1000));
    CHECK(hi == -1);
    REQUIRE(pair(4,1,2000));
    CHECK(hi*1000+lo == 4001);
    REQUIRE(pair(4,2,600000));
    CHECK(hi*1000+lo == 4002); // no frozen extra MWh
}

TEST_CASE("WPM daily zero high word resets after a missed midnight window", "[wpl9-counters][midnight]") {
    Wpl9EnergyPair p;
    float hi=0,lo=0;
    auto pair=[&](int l,uint32_t t,int day) {
        p.update(true,0,t,hi,lo,true,day);
        return p.update(false,l,t+100,hi,lo,true,day);
    };
    REQUIRE(pair(800,0,20));
    // The new date arrives before a CAN outage longer than 30 minutes.
    CHECK_FALSE(p.update(true,0,600000,hi,lo,true,21));
    CHECK_FALSE(pair(50,4200000,21));
    REQUIRE(pair(100,4800000,21));
    CHECK(lo == 100);
    // No repeated reset within the same date.
    CHECK_FALSE(pair(20,5400000,21));
    CHECK_FALSE(pair(30,6000000,21));
    CHECK(lo == 100);
}

TEST_CASE("WPM pre-midnight device reset waits for the next observed date", "[wpl9-counters][midnight]") {
    Wpl9EnergyPair p;
    float hi=0,lo=0;
    auto pair=[&](int h,int l,uint32_t t,int day) {
        p.update(true,h,t,hi,lo,true,day);
        return p.update(false,l,t+100,hi,lo,true,day);
    };
    REQUIRE(pair(12,300,0,20));
    CHECK_FALSE(pair(0,999,600000,20));
    CHECK_FALSE(pair(1,900,1200000,20));
    CHECK_FALSE(pair(2,800,1800000,20));
    CHECK(hi*1000+lo == 12300);
    REQUIRE(pair(3,700,2400000,21));
    CHECK(hi*1000+lo == 3700);
}

TEST_CASE("WPM invalid counters retain poll back-off", "[wpl9-counters][backoff]") {
    if (!kModel.wpmDisplayCounters) return;
    resetDiscoveryState();
    const auto& cm=CanMembers[cm_heizmodul_wpl];
    const auto* ei=GetElsterIndex("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH");
    for (int i=0;i<3;++i) updateSensor(cm,ei,"-255");
    CHECK(invalidStreak.at(signalKey(cm,elsterNameHash(ei->Name))) == 3);
    CHECK_FALSE(mqttTopicPublished("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH/state"));
}

TEST_CASE("WPM boot carry confirmation reaches the real published total", "[wpl9-counters][boot][calc]") {
    if (!kModel.wpmDisplayCounters) return;
    resetDiscoveryState(); resetStateCache();
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    wpmEnergyCheckpoint = {}; wpmEnergyCheckpointLoaded = true;
    copEnergyValues.clear(); copEnergyByMember.clear(); wpl9EnergyPairs.clear();
    energyBase={0x454e4b42u,{0,0,0,0}};energyBaseReady=true;
    for(int i=0;i<4;++i) {energyTagValid[i]=false;energyTagFolded[i]=0;}
    fake_millis()=1000;fake_can().sent.clear();
    const auto& cm=CanMembers[cm_heizmodul_wpl];
    auto send=[&](const char* suffix,const char* value) {
        updateSensor(cm,GetElsterIndex((std::string("EL_AUFNAHMELEISTUNG_HEIZ_")+suffix).c_str()),value);
    };
    send("SUM_MWH","4");send("SUM_KWH","998");
    CHECK(copEnergyValues.count("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH")==0);
    CHECK(fake_can().sent.size()==2); // low follow-up, then one fresh high confirmation
    send("SUM_MWH","4");send("SUM_KWH","1");
    CHECK(copEnergyValues.count("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH")==0);
    send("SUM_MWH","4");send("SUM_KWH","1");
    CHECK(copEnergyValues.at("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH")==1);
    send("TAG_KWH","0");send("TAG_WH","250");
    CHECK(mmPayload(ENERGY_CALC_TOPIC("energie_heiz_total_kwh"))=="4001.250");
    CHECK(mmPayload(HA_TOPIC_ROOT "/calculated/cop_heiz/state")=="unknown");
    fake_millis()=0;
}

// Emulate RAM loss while retaining only committed NVS bytes. No pre-existing
// checkpoint values are injected into the second boot by these fixtures.
static void resetEnergyRamForReboot() {
    stableValue.clear(); pendingValue.clear(); pendingSince.clear();
    copEnergyValues.clear(); copEnergyByMember.clear(); wpl9EnergyPairs.clear();
    energyBase={0x454e4b42u,{0,0,0,0}};energyBaseReady=false;
    for (int i=0;i<4;++i) {energyTagLast[i]=energyTagFolded[i]=0;energyTagValid[i]=false;}
    wpmEnergyCheckpoint={};wpmEnergyCheckpointLoaded=false;
    fake_preferences().pending.clear();
    resetDiscoveryState();
}
struct DurableEnergyFixture {
    DurableEnergyFixture() {fake_preferences()={};fake_preferences().enabled=true;resetEnergyRamForReboot();}
    ~DurableEnergyFixture() {fake_preferences()={};resetEnergyRamForReboot();}
};
static void feedWpmWarmwaterTotal(int baseKwh, int tagWh) {
    const auto& cm=CanMembers[cm_heizmodul_wpl];
    auto send=[&](const char* suffix,int v) {
        updateSensor(cm,GetElsterIndex((std::string("WAERMEERTRAG_WW_")+suffix).c_str()),std::to_string(v));
    };
    send("SUM_MWH",8);send("SUM_KWH",baseKwh);
    send("SUM_MWH",8);send("SUM_KWH",baseKwh); // current-boot pairing confirmation
    send("TAG_KWH",0);send("TAG_WH",tagWh);
}

TEST_CASE("WPM observed 8720.240 total survives reboot before the day fraction catches up", "[postflash][nvs][energy]") {
    if (!kModel.wpmDisplayCounters) return;
    DurableEnergyFixture f;
    feedWpmWarmwaterTotal(720,240);
    REQUIRE(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.240");
    REQUIRE(fake_preferences().committed.count(0x57454331u)==1);
    const auto commits=fake_preferences().syncs;
    for (int i=0;i<30;++i) publishCombinedEnergySensors();
    CHECK(fake_preferences().syncs==commits); // no repeated flash writes for unchanged values
    resetEnergyRamForReboot();
    feedWpmWarmwaterTotal(720,0);
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.240");
    feedWpmWarmwaterTotal(720,300);
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.300");
    resetEnergyRamForReboot();
    // Stored state alone must never publish before current-boot input pairs.
    updateCOPCalculations();
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="<none>");
    feedWpmWarmwaterTotal(720,0);
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.300");
}

TEST_CASE("WPM durable checkpoint includes midnight folding across a reboot", "[postflash][nvs][midnight]") {
    if (!kModel.wpmDisplayCounters) return;
    DurableEnergyFixture f;
    auto raw=[&](int base,int day,int wh) {
        const char* m=energyMemberName();
        storeCOPEnergyValueMember(m,"WAERMEERTRAG_WW_SUM_MWH","8");
        storeCOPEnergyValueMember(m,"WAERMEERTRAG_WW_SUM_KWH",std::to_string(base));
        storeCOPEnergyValueMember(m,"WAERMEERTRAG_WW_TAG_KWH",std::to_string(day));
        storeCOPEnergyValueMember(m,"WAERMEERTRAG_WW_TAG_WH",std::to_string(wh));
        publishCombinedEnergySensors();
        return mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"));
    };
    CHECK(raw(720,3,500)=="8723.500");
    CHECK(raw(724,3,500)=="8724.000"); // SUM has folded the old day; TAG still old
    resetEnergyRamForReboot();
    CHECK(raw(724,3,500)=="8724.000"); // must not jump to 8727.500
    CHECK(raw(724,0,0)=="8724.000");
    CHECK(raw(724,0,250)=="8724.250");
    resetEnergyRamForReboot();
    CHECK(raw(724,0,0)=="8724.250");
    CHECK(raw(724,0,300)=="8724.300");
}

TEST_CASE("WPM suppresses a total until its checkpoint is durably committed", "[postflash][nvs][failure]") {
    if (!kModel.wpmDisplayCounters) return;
    DurableEnergyFixture f;
    const bool failSave=GENERATE(false,true);
    fake_preferences().failSync=!failSave;fake_preferences().failSave=failSave;
    feedWpmWarmwaterTotal(720,240);
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="<none>");
    CHECK(fake_preferences().committed.count(0x57454331u)==0);
    fake_preferences().failSync=false;fake_preferences().failSave=false;
    publishCombinedEnergySensors();
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.240");
    resetEnergyRamForReboot();
    feedWpmWarmwaterTotal(720,0);
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.240");
}

TEST_CASE("WPM rejects a corrupt continuity record", "[postflash][nvs][failure]") {
    if (!kModel.wpmDisplayCounters) return;
    DurableEnergyFixture f;
    WpmEnergyCheckpoint bad;bad.totalMask=15;bad.total[3]=NAN;
    auto p=global_preferences->make_preference<WpmEnergyCheckpoint>(0x57454331u,true);
    REQUIRE(p.save(&bad));REQUIRE(global_preferences->sync());
    feedWpmWarmwaterTotal(720,0);
    CHECK(mmPayload(ENERGY_CALC_TOPIC("waerme_ww_total_kwh"))=="8720.000");
}

TEST_CASE("WPM checkpoint restores all four independent total floors", "[postflash][nvs][energy]") {
    if (!kModel.wpmDisplayCounters) return;
    DurableEnergyFixture f;
    const char* names[]={"EL_AUFNAHMELEISTUNG_HEIZ","EL_AUFNAHMELEISTUNG_WW","WAERMEERTRAG_HEIZ","WAERMEERTRAG_WW"};
    auto feed=[&](int wh) {
        for (int i=0;i<4;++i) {
            const std::string b(names[i]);const char* m=energyMemberName();
            storeCOPEnergyValueMember(m,(b+"_SUM_MWH").c_str(),std::to_string(i+1));
            storeCOPEnergyValueMember(m,(b+"_SUM_KWH").c_str(),"20");
            storeCOPEnergyValueMember(m,(b+"_TAG_KWH").c_str(),"0");
            storeCOPEnergyValueMember(m,(b+"_TAG_WH").c_str(),std::to_string(wh));
        }
        publishCombinedEnergySensors();
    };
    feed(240);REQUIRE(wpmEnergyCheckpoint.totalMask==15);
    resetEnergyRamForReboot();feed(0);
    for (int i=0;i<4;++i) {
        CHECK(stableValue.at(wpmEnergyKeys[i])==Approx((i+1)*1000+20.240f));
        CHECK(wpmEnergyCheckpoint.total[i]==Approx((i+1)*1000+20.240f));
    }
}

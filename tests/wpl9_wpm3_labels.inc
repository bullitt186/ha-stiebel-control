#ifdef HA_HAS_WPM_PROGRAM_LABELS
TEST_CASE("WPL9 WPM3: display modes retain their CAN values in both directions", "[wpl9][modes]") {
    struct Mode { unsigned short word; const char* name; };
    // Owner transcribed all six display labels on 2026-11-21, ECO clarified explicitly.
    const Mode modes[] = {{0x0000, "Notbetrieb"}, {0x0100, "Bereitschaftsbetrieb"},
                          {0x0200, "Programmbetrieb"}, {0x0300, "Komfortbetrieb"},
                          {0x0400, "ECO-Betrieb"}, {0x0500, "Warmwasserbetrieb"}};
    for (const auto& mode : modes) {
        CAPTURE(mode.name);
        char decoded[64];
        SetValueType(decoded, et_betriebsart, mode.word);
        CHECK(std::string(decoded) == mode.name);
        const char* text = mode.name;
        CHECK(TranslateString(text, et_betriebsart) == mode.word);
        fake_can().sent.clear();
        writeSignal(&CanMembers[cm_manager], "PROGRAMMSCHALTER", mode.name);
        REQUIRE(fake_can().sent.size() == 1);
        const auto& frame = fake_can().sent.back();
        REQUIRE(frame.size() == 7);
        CHECK(frame[2] == 0xfa);
        CHECK(frame[3] == 0x01);
        CHECK(frame[4] == 0x12);
        CHECK(frame[5] == (mode.word >> 8));
        CHECK(frame[6] == 0);
    }
}

TEST_CASE("WPL9 WPM3: select and climate agree with incoming mode labels", "[wpl9][mqtt][modes]") {
    resetDiscoveryState();
    resetStateCache();
    publishClimateDiscoveries();
    publishWritableSelectDiscovery(writableSelects[0]);
    const std::string climate = mqttFindPayload("climate/heatingpump/stiebel_climate_heizung/config");
    const std::string select = mqttFindPayload("select/heatingpump/stiebel_manager_programmschalter/config");
    for (const char* label : {"Programmbetrieb", "Komfortbetrieb", "ECO-Betrieb", "Bereitschaftsbetrieb", "Warmwasserbetrieb"}) {
        CHECK(climate.find(label) != std::string::npos);
        CHECK(select.find(label) != std::string::npos);
    }
    CHECK(climate.find("Tagbetrieb") == std::string::npos);
    CHECK(climate.find("Absenkbetrieb") == std::string::npos);
    updateSensor(CanMembers[cm_manager], GetElsterIndex("PROGRAMMSCHALTER"), "Komfortbetrieb");
    CHECK(mqttFindPayload("MANAGER/PROGRAMMSCHALTER/state") == "Komfortbetrieb");
    CHECK(climate.find("heatingpump/MANAGER/PROGRAMMSCHALTER/state") != std::string::npos);
}

TEST_CASE("WPL9 WPM3: SG Ready normal modes use the same protocol vocabulary", "[wpl9][sgready][modes]") {
    CHECK(std::string(SG_READY_NORMAL_MODE) == "Komfortbetrieb");
    CHECK(std::string(SG_READY_AUTO_MODE) == "Programmbetrieb");
    SgReadyController controller(sgReadyIO);
    CHECK(std::string(controller.normalMode()) == "Komfortbetrieb");
    controller.setNormalAutomatik(true, false);
    CHECK(std::string(controller.normalMode()) == "Programmbetrieb");
}
TEST_CASE("WPM mode command aliases keep their original CAN words", "[wpl9][modes]") {
    const char* aliases[] = {"Automatik", "Tagbetrieb", "Absenkbetrieb", "Bereitschaft", "Warmwasser"};
    const int words[] = {2,3,4,1,5};
    for (int i = 0; i < 5; ++i) {
        fake_can().sent.clear();
        REQUIRE(handleSelectCommand(writableSelects[0], aliases[i]));
        REQUIRE(fake_can().sent.size() == 2); // write, then read-back
        CHECK(fake_can().sent[0][5] == words[i]);
        CHECK(fake_can().sent[0][6] == 0);
    }
}
#endif

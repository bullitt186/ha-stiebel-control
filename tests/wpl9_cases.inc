// Included by test_can_logic.cpp after the firmware and test helpers.
TEST_CASE("model routes preserve defaults and WPL17 ownership", "[routes]") {
    for (const char* model : {"wpl10ac", "wpl13e", "wpf10", "unknown"}) {
        for (uint16_t index : {0x0013, 0x0a06, 0x010e}) {
            auto r = resolveSignalRoute(model, index, cm_manager);
            CHECK(r.writeMember == cm_manager);
            CHECK(r.readMember == cm_manager);
            CHECK(r.stateMember == cm_manager);
        }
    }
    for (uint16_t index : {0x0013, 0x0a06, 0x010e}) {
        auto r = resolveSignalRoute("wpl17", index, cm_manager);
        CHECK(r.writeMember == cm_kessel_canb);
        CHECK(r.readMember == cm_kessel_canb);
        CHECK(r.stateMember == cm_kessel_canb);
    }
    auto room = resolveSignalRoute("wpl9", 0x0005, cm_manager);
    CHECK(room.writeMember == cm_manager);
    CHECK(room.readMember == cm_manager);
}

TEST_CASE("WPL9 DHW writes and feedback use CAN-B", "[routes]") {
    auto primary = resolveSignalRoute("wpl9", 0x0013, cm_manager);
    auto secondary = resolveSignalRoute("wpl9", 0x0a06, cm_manager);
    CHECK(primary.writeMember == cm_kessel_canb);
    CHECK(primary.readMember == cm_kessel_canb);
    CHECK(primary.stateMember == cm_kessel_canb);
    CHECK(secondary.writeMember == cm_kessel_canb);
    CHECK(secondary.readMember == cm_kessel_canb);
    CHECK(secondary.stateMember == cm_kessel_canb);
}

TEST_CASE("configured DHW commands send short and extended register frames", "[routes][can]") {
    const bool wpl9 = std::strcmp(HA_DEVICE_MODEL_STR, "wpl9") == 0;
    const bool wpl17 = std::strcmp(HA_DEVICE_MODEL_STR, "wpl17") == 0;
    const uint8_t writeFirst = (wpl9 || wpl17) ? 0x40 : 0x90;
    const uint8_t writeSecond = (wpl9 || wpl17) ? 0x01 : 0x00;
    fake_can().sent.clear();
    writeConfiguredSignal(cm_manager, "EINSTELL_SPEICHERSOLLTEMP", "49");
    REQUIRE(fake_can().sent.size() == 2);
    CHECK(fake_can().sent[0] == std::vector<uint8_t>{writeFirst, writeSecond, 0x13, 0x01, 0xea, 0, 0});
    CHECK(fake_can().sent[1] == std::vector<uint8_t>{static_cast<uint8_t>(writeFirst + 1), writeSecond, 0x13, 0, 0, 0, 0});
    fake_can().sent.clear();
    writeConfiguredSignal(cm_manager, "EINSTELL_SPEICHERSOLLTEMP2", "49");
    REQUIRE(fake_can().sent.size() == 2);
    CHECK(fake_can().sent[0] == std::vector<uint8_t>{writeFirst, writeSecond, 0xfa, 0x0a, 0x06, 0x01, 0xea});
    const uint8_t readFirst = static_cast<uint8_t>(writeFirst + 1);
    const uint8_t readSecond = writeSecond;
    CHECK(fake_can().sent[1] == std::vector<uint8_t>{readFirst, readSecond, 0xfa, 0x0a, 0x06, 0, 0});
}

TEST_CASE("WPL9 discovery uses subscribed CAN-B command and state topics", "[routes][mqtt]") {
    if (std::strcmp(HA_DEVICE_MODEL_STR, "wpl9") != 0) return;
    for (size_t i : {0U, 1U}) {
        resetDiscoveryState();
        publishWritableNumberDiscovery(writableNumbers[i], true);
        std::string payload = mqttFindPayload("homeassistant/number/heatingpump/");
        const std::string base = std::string("heatingpump/KESSEL_CANB/") + writableNumbers[i].signalName;
        CHECK(payload.find("\"command_topic\":\"" + base + "/set\"") != std::string::npos);
        CHECK(payload.find("\"state_topic\":\"" + base + "/state\"") != std::string::npos);
    }
}

TEST_CASE("WPL9 SG Ready adapter shares DHW destinations and keeps room writes", "[routes][can]") {
    if (std::strcmp(HA_DEVICE_MODEL_STR, "wpl9") != 0) return;
    EspHomeSgReadyIO adapter;
    fake_can().sent.clear();
    adapter.writeCanSignal("EINSTELL_SPEICHERSOLLTEMP", "49");
    REQUIRE(fake_can().sent.size() == 2);
    CHECK(fake_can().sent[0] == std::vector<uint8_t>{0x40, 1, 0x13, 1, 0xea, 0, 0});
    CHECK(fake_can().sent[1] == std::vector<uint8_t>{0x41, 1, 0x13, 0, 0, 0, 0});
    fake_can().sent.clear();
    adapter.writeCanSignal("RAUMSOLLTEMP_I", "20");
    REQUIRE(fake_can().sent.size() == 2);
    CHECK(fake_can().sent[0][0] == 0x90);
}

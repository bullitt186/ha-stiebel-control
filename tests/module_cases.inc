// ============================================================================
// Included by test_can_logic.cpp after the firmware and test helpers (#123).
//
// Tests for time_programs.h, energy.h and discovery.h. Expected values come from the
// documented formats, not from running the code:
//   - time programs: docs/CONFIGURATION.md "DHW Time Program", dhw_program.h (format)
//   - COP: docs/ARCHITECTURE.md "COP and booster energy" (heat + 2WE) / electrical
//   - combined energy: the formulas in the energy.h comments (MWh*1000 + kWh, day counter
//     kWh + Wh/1000, power = delta Wh / delta t * 3600, 5 minute mean, 15 min confirmation)
//   - discovery: ElsterMetaTable rows, the calculatedSensors[] rows, the HA MQTT discovery
//     schema and the default entity IDs of #96 (sensor.<root>_<member>_<signal>)
// ============================================================================

// Last payload sent on exactly this topic, "<none>" if none
static std::string mmPayload(const std::string& topic) {
    const auto& msgs = mqtt_client_instance().messages;
    for (auto it = msgs.rbegin(); it != msgs.rend(); ++it)
        if (it->topic == topic) return it->payload;
    return "<none>";
}
static size_t mmCount(const std::string& topic) {
    size_t n = 0;
    for (const auto& m : mqtt_client_instance().messages) n += (m.topic == topic);
    return n;
}
// Value of the first "key": in a flat JSON payload (string without quotes), "<absent>" if missing
static std::string mmField(const std::string& json, const std::string& key) {
    const std::string k = "\"" + key + "\":";
    size_t p = json.find(k);
    if (p == std::string::npos) return "<absent>";
    p += k.size();
    if (p < json.size() && json[p] == '"') {
        const size_t e = json.find('"', p + 1);
        return json.substr(p + 1, e - p - 1);
    }
    const size_t e = json.find_first_of(",}]", p);
    return json.substr(p, e - p);
}
// The "device":{...} block of a discovery payload
static std::string mmDevice(const std::string& json) {
    const size_t p = json.find("\"device\":{");
    return p == std::string::npos ? std::string() : json.substr(p + 9);
}
// <root> as it appears in an entity ID (#96: only a-z, 0-9, "_")
static std::string mmEntityRoot() {
    std::string r = HA_TOPIC_ROOT;
    std::replace(r.begin(), r.end(), '-', '_');
    return r;
}
static const std::string kRoot = HA_TOPIC_ROOT;

// ============================================================================
// time_programs.h (WPMW II time programs, #82)
// ============================================================================

TEST_CASE("time program: a day is published once all three periods are known", "[dhwprog][programs]") {
    resetDiscoveryState();
    fake_millis() = 1000000;
    const CanMember& mgr = CanMembers[cm_manager];
    CHECK(timeProgramUpdate(mgr, "W_WASSERPROG_1_DI_SCHALT_2", "17:00-22:00"));
    CHECK(timeProgramUpdate(mgr, "W_WASSERPROG_1_DI", "06:30-09:00"));
    CHECK(mqtt_client_instance().messages.empty());   // _SCHALT_3 still unknown
    CHECK(timeProgramUpdate(mgr, "W_WASSERPROG_1_DI_SCHALT_3", "not used time domain"));
    // CONFIGURATION.md: periods in order, separated by commas
    CHECK(mmPayload(kRoot + "/dhw_program/di/state") == "06:30-09:00,17:00-22:00");

    // Discovery: text entity where writable, else a read-only sensor (WPL18E, generic)
    const bool writable = dhwProgramWritable();
    const std::string comp = writable ? "text" : "sensor";
    const std::string cfg = mmPayload("homeassistant/" + comp + "/" + kRoot + "/stiebel_ww_programm_di/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "unique_id") == "stiebel_ww_programm_di");
    CHECK(mmField(cfg, "default_entity_id") == comp + "." + mmEntityRoot() + "_ww_programm_di");
    CHECK(mmField(cfg, "state_topic") == kRoot + "/dhw_program/di/state");
    CHECK(mmField(cfg, "icon") == "mdi:calendar-clock");
    if (writable) {
        CHECK(mmField(cfg, "command_topic") == kRoot + "/dhw_program/di/set");
        // three periods "HH:MM-HH:MM" and two commas = 35 characters must fit
        CHECK(std::atoi(mmField(cfg, "max").c_str()) >= 35);
    } else {
        CHECK(mmField(cfg, "command_topic") == "<absent>");
        CHECK(mmField(cfg, "max") == "<absent>");
    }
    CHECK(mmField(mmDevice(cfg), "identifiers") == "[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"");
    CHECK(mmCount("homeassistant/sensor/" + kRoot + "/stiebel_ww_programm_mi/config") == 0);   // other days not yet
    resetDiscoveryState();
}

TEST_CASE("time program: unchanged text is repeated only after the 30 minute heartbeat", "[dhwprog][programs]") {
    resetDiscoveryState();
    const unsigned long t0 = 5000000;
    fake_millis() = t0;
    const CanMember& mgr = CanMembers[cm_manager];
    const std::string topic = kRoot + "/dhw_program/fr/state";
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR", "06:00-22:00");
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR_SCHALT_2", "not used time domain");
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR_SCHALT_3", "not used time domain");
    REQUIRE(mmPayload(topic) == "06:00-22:00");
    mqtt_client_instance().clear();

    fake_millis() = t0 + 60000;   // next poll, same value
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR", "06:00-22:00");
    CHECK(mmCount(topic) == 0);
    fake_millis() = t0 + 30UL * 60UL * 1000UL - 1;
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR", "06:00-22:00");
    CHECK(mmCount(topic) == 0);
    fake_millis() = t0 + 30UL * 60UL * 1000UL;   // heartbeat due
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR", "06:00-22:00");
    CHECK(mmCount(topic) == 1);

    // a changed period goes out at once
    mqtt_client_instance().clear();
    fake_millis() = t0 + 30UL * 60UL * 1000UL + 1000;
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR_SCHALT_3", "23:00-24:00");
    CHECK(mmPayload(topic) == "06:00-22:00,23:00-24:00");
    // every period unused: "-"
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR", "not used time domain");
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR_SCHALT_3", "not used time domain");
    CHECK(mmPayload(topic) == "-");

    // after a discovery republish, config and text go out again with the next poll
    mqtt_client_instance().clear();
    dhwProgramRepublish();
    timeProgramUpdate(mgr, "W_WASSERPROG_1_FR", "not used time domain");
    CHECK(mmPayload(topic) == "-");
    const std::string comp = dhwProgramWritable() ? "text" : "sensor";
    CHECK(mmCount("homeassistant/" + comp + "/" + kRoot + "/stiebel_ww_programm_fr/config") == 1);
    resetDiscoveryState();
}

TEST_CASE("time program: member and name select the program, -255 is ignored", "[dhwprog][programs]") {
    resetDiscoveryState();
    fake_millis() = 1000;
    const CanMember& mgr = CanMembers[cm_manager];
    const CanMember& hk2 = CanMembers[cm_mischermodul_3];
    // not a program period: block setters and other members are raw signals
    CHECK_FALSE(timeProgramUpdate(mgr, "W_WASSERPROG_1_MO_FR", "06:00-22:00"));
    CHECK_FALSE(timeProgramUpdate(CanMembers[cm_kessel], "W_WASSERPROG_1_MO", "06:00-22:00"));
    CHECK_FALSE(timeProgramUpdate(mgr, "AUSSENTEMP", "4.5"));
    // -255: the manager does not provide it; consumed, the day stays unknown
    CHECK(timeProgramUpdate(mgr, "W_WASSERPROG_1_SA", "-255"));
    CHECK(timeProgram[TP_DHW][5].known == 0);

    // CONFIGURATION.md: circuit 2 is HEIZPROG_1 on MISCHERMODUL_3 (0x602), circuit 1 on the manager
    for (const char* s : {"HEIZPROG_1_SO", "HEIZPROG_1_SO_SCHALT_2", "HEIZPROG_1_SO_SCHALT_3"})
        CHECK(timeProgramUpdate(hk2, s, std::strcmp(s, "HEIZPROG_1_SO") == 0 ? "05:00-21:00" : "not used time domain"));
    CHECK(mmPayload(kRoot + "/heating_program_2/so/state") == "05:00-21:00");
    CHECK(mmCount(kRoot + "/heating_program/so/state") == 0);
    for (const char* s : {"HEIZPROG_1_SO", "HEIZPROG_1_SO_SCHALT_2", "HEIZPROG_1_SO_SCHALT_3"})
        CHECK(timeProgramUpdate(mgr, s, std::strcmp(s, "HEIZPROG_1_SO") == 0 ? "00:00-24:00" : "not used time domain"));
    CHECK(mmPayload(kRoot + "/heating_program/so/state") == "00:00-24:00");
    CHECK(mmPayload(kRoot + "/heating_program_2/so/state") == "05:00-21:00");
    const std::string comp = dhwProgramWritable() ? "text" : "sensor";
    CHECK(mmCount("homeassistant/" + comp + "/" + kRoot + "/stiebel_heiz_programm_hk2_so/config") == 1);
    CHECK(mmCount("homeassistant/" + comp + "/" + kRoot + "/stiebel_heiz_programm_so/config") == 1);
    resetDiscoveryState();
}

TEST_CASE("time program command: range check, only changed periods are written", "[dhwprog][programs]") {
    resetDiscoveryState();
    pacedWriteCount = 0;
    // program and day out of range: nothing written
    CHECK_FALSE(handleTimeProgramCommand(-1, 0, "06:00-08:00"));
    CHECK_FALSE(handleTimeProgramCommand(TP_COUNT, 0, "06:00-08:00"));
    CHECK_FALSE(handleTimeProgramCommand(TP_DHW, -1, "06:00-08:00"));
    CHECK_FALSE(handleTimeProgramCommand(TP_DHW, dhwprog::DAYS, "06:00-08:00"));
    CHECK(pacedWriteCount == 0);
    if (!dhwProgramWritable()) {   // CONFIGURATION.md: read-only on WPL18E and other models
        CHECK_FALSE(handleTimeProgramCommand(TP_DHW, 0, "06:00-08:00"));
        CHECK(pacedWriteCount == 0);
        resetDiscoveryState();
        return;
    }
    // last program and last day are valid
    CHECK(handleTimeProgramCommand(TP_HEATING2, dhwprog::DAYS - 1, "06:00-08:00"));
    CHECK(pacedWriteCount == 3);   // nothing read yet: every period is written
    REQUIRE(pacedWriteCount >= 1);
    CHECK(std::string(pacedWrites[0].ei->Name) == "HEIZPROG_1_SO");
    CHECK(pacedWrites[0].writeCm == &CanMembers[cm_mischermodul_3]);
    CHECK(std::string(pacedWrites[0].value) == "06:00-08:00");
    pacedWriteCount = 0;

    const CanMember& mgr = CanMembers[cm_manager];
    timeProgramUpdate(mgr, "W_WASSERPROG_1_DI", "06:30-09:00");
    timeProgramUpdate(mgr, "W_WASSERPROG_1_DI_SCHALT_2", "17:00-22:00");
    timeProgramUpdate(mgr, "W_WASSERPROG_1_DI_SCHALT_3", "not used time domain");
    // "-" clears the two used periods; the unused third one is not written
    CHECK(handleTimeProgramCommand(TP_DHW, 1, "-"));
    REQUIRE(pacedWriteCount == 2);
    CHECK(std::string(pacedWrites[0].ei->Name) == "W_WASSERPROG_1_DI");
    CHECK(std::string(pacedWrites[0].value).empty());
    CHECK(std::string(pacedWrites[1].ei->Name) == "W_WASSERPROG_1_DI_SCHALT_2");
    CHECK(std::string(pacedWrites[1].value).empty());
    pacedWriteCount = 0;
    // a third period: only _SCHALT_3 changes
    CHECK(handleTimeProgramCommand(TP_DHW, 1, "06:30-09:00,17:00-22:00,22:00-23:00"));
    REQUIRE(pacedWriteCount == 1);
    CHECK(std::string(pacedWrites[0].ei->Name) == "W_WASSERPROG_1_DI_SCHALT_3");
    CHECK(std::string(pacedWrites[0].value) == "22:00-23:00");
    pacedWriteCount = 0;
    // the same program again: nothing to write
    CHECK(handleTimeProgramCommand(TP_DHW, 1, "06:30-09:00, 17:00-22:00"));
    CHECK(pacedWriteCount == 0);
    pendingReadbackCount = 0;
    resetDiscoveryState();
}

// ============================================================================
// energy.h: COP, combined totals, day values, power and operating state (#109)
// ============================================================================

static void mmResetEnergy() {
    resetDiscoveryState();
    copEnergyValues.clear();
    copEnergyByMember.clear();
    copDiscoveryPublished = false;
    energyBase = EnergyBaseStorage{0x454E4B42u, {0.0f, 0.0f, 0.0f, 0.0f}};
    for (int i = 0; i < 4; i++) {
        energyTagLast[i] = 0.0f;
        energyTagFolded[i] = 0.0f;
        energyTagValid[i] = false;
    }
    lastVerdichterValue = NAN;
    lastPumpenstatus = -1;
}

TEST_CASE("storeCOPEnergyValue: only numbers are stored", "[calc][energy]") {
    mmResetEnergy();
    storeCOPEnergyValue("MM_A", "12.5");
    storeCOPEnergyValue("MM_B", "-255");
    storeCOPEnergyValue("MM_C", "+3");
    storeCOPEnergyValue("MM_D", " 7");
    storeCOPEnergyValue("MM_E", "90");
    CHECK(copEnergyValues["MM_E"] == 90.0f);
    CHECK(copEnergyValues["MM_A"] == 12.5f);
    CHECK(copEnergyValues["MM_B"] == -255.0f);
    CHECK(copEnergyValues["MM_C"] == 3.0f);
    CHECK(copEnergyValues["MM_D"] == 7.0f);
    for (const char* bad : {"", "abc", "12a", "1e3", ".", "-"}) {
        INFO(bad);
        storeCOPEnergyValue("MM_BAD", bad);
        CHECK(copEnergyValues.count("MM_BAD") == 0);
    }
    // the per-member store follows the same rule
    float v = 0.0f;
    storeCOPEnergyValueMember("HEIZMODUL", "MM_A", "24.5");
    REQUIRE(getEnergyForMember("HEIZMODUL", "MM_A", v));
    CHECK(v == 24.5f);
    CHECK_FALSE(getEnergyForMember("MANAGER", "MM_A", v));   // other member
    for (const char* ok : {"-1.0", "+2", " 3", "9", "0"}) {
        storeCOPEnergyValueMember("HEIZMODUL", "MM_OK", ok);
        CHECK(getEnergyForMember("HEIZMODUL", "MM_OK", v));
        copEnergyByMember.clear();
    }
    for (const char* bad : {"", "abc", "12a", "."}) {
        INFO(bad);
        storeCOPEnergyValueMember("HEIZMODUL", "MM_BAD", bad);
        CHECK_FALSE(getEnergyForMember("HEIZMODUL", "MM_BAD", v));
    }
    mmResetEnergy();
}

TEST_CASE("COP: (heat + booster heat) / electrical energy for DHW, heating and total", "[calc][energy]") {
    mmResetEnergy();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH", "3");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH", "1");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "2");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_MWH", "9");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "1");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "4");
    updateCOPCalculations();
    CHECK(mmPayload(kRoot + "/calculated/cop_ww/state") == "2.00");       // (3+1)/2
    CHECK(mmPayload(kRoot + "/calculated/cop_heiz/state") == "2.50");     // (9+1)/4
    CHECK(mmPayload(kRoot + "/calculated/cop_gesamt/state") == "2.33");   // (3+1+9+1)/(2+4)
    // configs on their kept discovery topics (calculated.h objectId), once
    for (const char* id : {"cop_ww", "cop_heiz", "cop_gesamt"})
        CHECK(mmCount("homeassistant/sensor/" + kRoot + "/" + id + "/config") == 1);
    updateCOPCalculations();
    CHECK(mmCount("homeassistant/sensor/" + kRoot + "/cop_ww/config") == 1);
    // a discovery republish sends them again
    copDiscoveryRepublish();
    discoveredCalculatedSensors.clear();
    updateCOPCalculations();
    CHECK(mmCount("homeassistant/sensor/" + kRoot + "/cop_ww/config") == 2);
    mmResetEnergy();
}

TEST_CASE("COP: no value without electrical energy or a missing register", "[calc][energy]") {
    mmResetEnergy();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH", "3");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH", "1");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "0");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_MWH", "9");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "1");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "4");
    updateCOPCalculations();
    CHECK(mmCount(kRoot + "/calculated/cop_ww/state") == 0);              // division by 0
    CHECK(mmPayload(kRoot + "/calculated/cop_heiz/state") == "2.50");
    CHECK(mmPayload(kRoot + "/calculated/cop_gesamt/state") == "3.50");   // 14 / (0+4)

    mmResetEnergy();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH", "3");                  // 2WE_WW missing
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "2");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_MWH", "9");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "1");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "0");
    updateCOPCalculations();
    CHECK(mmCount(kRoot + "/calculated/cop_ww/state") == 0);
    CHECK(mmCount(kRoot + "/calculated/cop_heiz/state") == 0);
    CHECK(mmCount(kRoot + "/calculated/cop_gesamt/state") == 0);
    mmResetEnergy();
}

TEST_CASE("COP: the kWh remainder (0..999) is added to the whole MWh", "[calc][energy]") {
    // ARCHITECTURE.md: the MWh registers carry whole MWh, *_SUM_KWH the remainder
    mmResetEnergy();
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_MWH", "42");
    storeCOPEnergyValue("WAERMEERTRAG_WW_SUM_KWH", "629");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_WW_SUM_MWH", "0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_MWH", "14");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_SUM_KWH", "75");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_MWH", "20");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_SUM_KWH", "500");
    storeCOPEnergyValue("WAERMEERTRAG_2WE_HEIZ_SUM_MWH", "0");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "5");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", "125");
    updateCOPCalculations();
    CHECK(mmPayload(kRoot + "/calculated/cop_ww/state") == "3.03");     // 42.629 / 14.075
    CHECK(mmPayload(kRoot + "/calculated/cop_heiz/state") == "4.00");   // 20.5 / 5.125
    // out-of-range remainders (0x8000 marker, 1000) do not count
    for (const char* bad : {"32768", "1000", "-255"}) {
        INFO(bad);
        storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", bad);
        updateCOPCalculations();
        CHECK(mmPayload(kRoot + "/calculated/cop_heiz/state") == "4.10");   // 20.5 / 5
    }
    mmResetEnergy();
}

TEST_CASE("sanitizeEnergyTotal: tolerance, small steps and the 15 minute confirmation", "[calc][energy]") {
    const unsigned long t0 = 100000000;
    float out = 0.0f;
    fake_millis() = t0;
    // "Toleranz gleicher Wert" 1 kWh: such a drop never becomes a candidate, the value stands
    REQUIRE(sanitizeEnergyTotal("mm_small", 100.0f, out));
    CHECK(out == 100.0f);
    REQUIRE(sanitizeEnergyTotal("mm_small", 99.0f, out));
    CHECK(out == 100.0f);
    fake_millis() = t0 + 900000;
    REQUIRE(sanitizeEnergyTotal("mm_small", 99.0f, out));
    CHECK(out == 100.0f);
    // growth up to 20 kWh is taken at once, more is held back
    REQUIRE(sanitizeEnergyTotal("mm_small", 120.0f, out));
    CHECK(out == 120.0f);
    REQUIRE(sanitizeEnergyTotal("mm_small", 141.0f, out));
    CHECK(out == 120.0f);
    REQUIRE(sanitizeEnergyTotal("mm_small", 120.5f, out));
    CHECK(out == 120.5f);

    // a jump is taken once the candidate (within 25 kWh) is stable for 15 min
    fake_millis() = t0;
    REQUIRE(sanitizeEnergyTotal("mm_jump", 1000.0f, out));
    REQUIRE(sanitizeEnergyTotal("mm_jump", 2000.0f, out));
    CHECK(out == 1000.0f);
    fake_millis() = t0 + 60000;
    REQUIRE(sanitizeEnergyTotal("mm_jump", 2000.0f, out));
    CHECK(out == 1000.0f);
    fake_millis() = t0 + 899999;
    REQUIRE(sanitizeEnergyTotal("mm_jump", 2000.0f, out));
    CHECK(out == 1000.0f);
    fake_millis() = t0 + 900000;
    REQUIRE(sanitizeEnergyTotal("mm_jump", 2010.0f, out));
    CHECK(out == 2010.0f);

    // exactly 25 kWh from the candidate still confirms it
    fake_millis() = t0;
    REQUIRE(sanitizeEnergyTotal("mm_tol", 1000.0f, out));
    REQUIRE(sanitizeEnergyTotal("mm_tol", 2000.0f, out));
    fake_millis() = t0 + 900000;
    REQUIRE(sanitizeEnergyTotal("mm_tol", 2025.0f, out));
    CHECK(out == 2025.0f);

    // a candidate that moves more than 25 kWh starts its 15 minutes again
    fake_millis() = t0;
    REQUIRE(sanitizeEnergyTotal("mm_move", 1000.0f, out));
    REQUIRE(sanitizeEnergyTotal("mm_move", 2000.0f, out));
    fake_millis() = t0 + 900000;
    REQUIRE(sanitizeEnergyTotal("mm_move", 2030.0f, out));
    CHECK(out == 1000.0f);
    fake_millis() = t0 + 1800000;
    REQUIRE(sanitizeEnergyTotal("mm_move", 2030.0f, out));
    CHECK(out == 2030.0f);

    // a lasting drop (counter replaced) is taken after 15 min as well
    fake_millis() = t0;
    REQUIRE(sanitizeEnergyTotal("mm_drop", 1000.0f, out));
    REQUIRE(sanitizeEnergyTotal("mm_drop", 500.0f, out));
    CHECK(out == 1000.0f);
    fake_millis() = t0 + 900000;
    REQUIRE(sanitizeEnergyTotal("mm_drop", 500.0f, out));
    CHECK(out == 500.0f);

    // invalid markers (0x8000 -> 32768000): nothing before a stable value, then the stable one
    fake_millis() = t0;
    CHECK_FALSE(sanitizeEnergyTotal("mm_marker", 32768000.0f, out));
    REQUIRE(sanitizeEnergyTotal("mm_marker", 500.0f, out));
    out = 0.0f;
    REQUIRE(sanitizeEnergyTotal("mm_marker", 32768000.0f, out));
    CHECK(out == 500.0f);
    fake_millis() = 0;
}

// Brings a total key to a known stable value whatever earlier tests left behind:
// either taken at once, or confirmed by the same value 15 minutes later.
static void mmPrimeTotal(const char* key, float value, unsigned long t) {
    float out = 0.0f;
    fake_millis() = t;
    sanitizeEnergyTotal(key, value, out);
    fake_millis() = t + 900000;
    sanitizeEnergyTotal(key, value, out);
    REQUIRE(out == value);
}

TEST_CASE("combined total: MWh*1000 + kWh on models without day counters", "[calc][energy]") {
    mmResetEnergy();
    const unsigned long t = 400000000;
    mmPrimeTotal("combined_energie_heiz", 24730.0f, t);
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", "24");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", "738");
    publishCombinedEnergySensors();
    CHECK(mmPayload(kRoot + "/calculated/energie_heiz_total_kwh/state") == "24738.000");
    const std::string cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_calculated_energie_heiz_total_kwh/config");
    CHECK(mmField(cfg, "device_class") == "energy");
    CHECK(mmField(cfg, "unit_of_measurement") == "kWh");
    CHECK(mmField(cfg, "state_class") == "total_increasing");
    CHECK(mmCount(kRoot + "/calculated/energie_ww_total_kwh/state") == 0);   // WW registers unknown

    // once a day counter base exists, the device SUM alone is not used
    mqtt_client_instance().clear();
    resetStateCache();
    energyBase.v[0] = 24000.0f;
    publishCombinedEnergySensors();
    CHECK(mmCount(kRoot + "/calculated/energie_heiz_total_kwh/state") == 0);
    mmResetEnergy();
}

TEST_CASE("combined total: SUM base plus today's day counter from the energy member", "[calc][energy]") {
    mmResetEnergy();
    const unsigned long t = 500000000;
    mmPrimeTotal("combined_waerme_heiz", 24735.0f, t);
    const char* m = energyMemberName();
    storeCOPEnergyValueMember(m, "WAERMEERTRAG_HEIZ_SUM_MWH", "24");
    storeCOPEnergyValueMember(m, "WAERMEERTRAG_HEIZ_SUM_KWH", "738");
    storeCOPEnergyValueMember(m, "WAERMEERTRAG_HEIZ_TAG_KWH", "3");
    storeCOPEnergyValueMember(m, "WAERMEERTRAG_HEIZ_TAG_WH", "500");
    publishCombinedEnergySensors();
    // SUM_MWH*1000 + SUM_KWH + TAG_KWH + TAG_WH/1000
    CHECK(mmPayload(kRoot + "/calculated/waerme_heiz_total_kwh/state") == "24741.500");
    // the same registers of another member are ignored
    storeCOPEnergyValueMember("MANAGER", "WAERMEERTRAG_HEIZ_TAG_KWH", "9");
    publishCombinedEnergySensors();
    CHECK(mmPayload(kRoot + "/calculated/waerme_heiz_total_kwh/state") == "24741.500");
    mmResetEnergy();
}

TEST_CASE("wplBaseTotal: no value without a SUM base", "[calc][energy]") {
    mmResetEnergy();
    const char* m = energyMemberName();
    storeCOPEnergyValueMember(m, "MM_NOBASE_TAG_KWH", "3");
    storeCOPEnergyValueMember(m, "MM_NOBASE_TAG_WH", "500");
    float out = -1.0f;
    CHECK_FALSE(wplBaseTotal(0, "MM_NOBASE_SUM_MWH", "MM_NOBASE_SUM_KWH", "MM_NOBASE_TAG_KWH",
                             "MM_NOBASE_TAG_WH", out));
    mmResetEnergy();
}

TEST_CASE("wplBaseTotal: the two day counters reset one after the other", "[calc][energy]") {
    // energy.h: SUM steps first and already holds the day; TAG_WH and TAG_KWH then reset in
    // different cycles. A drop below 1 kWh is not the day change, near 0 is.
    mmResetEnergy();
    const char* m = energyMemberName();
    auto set = [m](const char* sm, const char* sk, const char* tk, const char* tw) {
        storeCOPEnergyValueMember(m, "MM_ROLL_SUM_MWH", sm);
        storeCOPEnergyValueMember(m, "MM_ROLL_SUM_KWH", sk);
        storeCOPEnergyValueMember(m, "MM_ROLL_TAG_KWH", tk);
        storeCOPEnergyValueMember(m, "MM_ROLL_TAG_WH", tw);
        float out = -1.0f;
        REQUIRE(wplBaseTotal(0, "MM_ROLL_SUM_MWH", "MM_ROLL_SUM_KWH", "MM_ROLL_TAG_KWH", "MM_ROLL_TAG_WH", out));
        return out;
    };
    const auto kWh = [](float v) { return Approx(v).margin(0.01); };
    CHECK(set("24", "738", "3", "500") == kWh(24741.5f));
    CHECK(set("24", "742", "3", "500") == kWh(24742.0f));   // SUM holds the 3.5 kWh of the day
    CHECK(set("24", "742", "3", "0") == kWh(24742.0f));     // TAG_WH reset first: 3.0, no day change
    CHECK(set("24", "742", "0", "0") == kWh(24742.0f));     // TAG_KWH reset: day change
    CHECK(set("24", "742", "1", "0") == kWh(24743.0f));
    mmResetEnergy();
}

TEST_CASE("wplBaseTotal: a day below 1 kWh resets at 'near 0'", "[calc][energy]") {
    // energy.h: a real reset falls to about 0 or by the whole day; a day of 0.8 kWh drops
    // by less than 1 kWh, so only "near 0" recognizes it
    mmResetEnergy();
    const char* m = energyMemberName();
    auto set = [m](const char* sk, const char* tw) {
        storeCOPEnergyValueMember(m, "MM_SMALL_SUM_MWH", "24");
        storeCOPEnergyValueMember(m, "MM_SMALL_SUM_KWH", sk);
        storeCOPEnergyValueMember(m, "MM_SMALL_TAG_KWH", "0");
        storeCOPEnergyValueMember(m, "MM_SMALL_TAG_WH", tw);
        float out = -1.0f;
        REQUIRE(wplBaseTotal(1, "MM_SMALL_SUM_MWH", "MM_SMALL_SUM_KWH", "MM_SMALL_TAG_KWH", "MM_SMALL_TAG_WH", out));
        return out;
    };
    const auto kWh = [](float v) { return Approx(v).margin(0.01); };
    CHECK(set("742", "800") == kWh(24742.8f));
    CHECK(set("743", "800") == kWh(24743.0f));   // SUM steps and holds the 0.8 kWh
    CHECK(set("743", "0") == kWh(24743.0f));     // day change
    CHECK(set("743", "300") == kWh(24743.3f));   // the new day counts again
    mmResetEnergy();
}

TEST_CASE("day values: kWh part + Wh/1000, only with both registers", "[calc][energy]") {
    mmResetEnergy();
    fake_millis() = 600000000;
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", "12");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH", "345");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_TAG_KWH", "1");
    storeCOPEnergyValue("EL_AUFNAHMELEISTUNG_WW_TAG_WH", "5");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_TAG_KWH", "30");
    storeCOPEnergyValue("WAERMEERTRAG_HEIZ_TAG_WH", "999");
    storeCOPEnergyValue("WAERMEERTRAG_WW_TAG_KWH", "4");    // TAG_WH missing
    publishCombinedEnergySensors();
    CHECK(mmPayload(kRoot + "/calculated/energie_heiz_tag_kwh/state") == "12.345");
    CHECK(mmPayload(kRoot + "/calculated/energie_ww_tag_kwh/state") == "1.005");
    CHECK(mmPayload(kRoot + "/calculated/waerme_heiz_tag_kwh/state") == "30.999");
    CHECK(mmCount(kRoot + "/calculated/waerme_ww_tag_kwh/state") == 0);
    const std::string cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_calculated_energie_heiz_tag_kwh/config");
    CHECK(mmField(cfg, "state_class") == "total_increasing");
    mmResetEnergy();
    fake_millis() = 0;
}

TEST_CASE("power and operating state from the EL_*_TAG_WH counters", "[calc][energy]") {
    // energy.h: power = (Wh_new - Wh_old) / (t_new - t_old) * 3600 as a 5 minute mean (not on
    // the WPMW II); state every 30 s: compressor (0.8 stopped, 2.4 running), cooling, then
    // which of the two consumption counters rises. Falling counters (day change) add nothing.
    mmResetEnergy();
    lastVerdichterValue = 2.4f;
    const bool wpmw2 = isWpmw2Model();
    const std::string st = kRoot + "/calculated/betriebsstatus/state";
    const std::string pw = kRoot + "/calculated/el_leistung_w/state";
    const std::string ph = kRoot + "/calculated/el_leistung_heiz_w/state";
    const std::string pww = kRoot + "/calculated/el_leistung_ww_w/state";
    const unsigned long t0 = 2000000000UL;
    auto step = [](unsigned long t, float heizWh, float wwWh) {
        fake_millis() = t;
        copEnergyValues["EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH"] = heizWh;
        copEnergyValues["EL_AUFNAHMELEISTUNG_WW_TAG_WH"] = wwWh;
        publishCombinedEnergySensors();
    };
    step(t0, 100, 50);   // start of the window (or end of one left by an earlier test)
    mqtt_client_instance().clear();
    resetStateCache();
    // WPMW II: the counter sums decide only until the pump status is known; the
    // counter-based state on the WPMW II is covered by the 5 minute window test below.

    step(t0 + 29999, 100, 50);
    CHECK(mmCount(st) == 0);   // not yet 30 s
    step(t0 + 30000, 140, 50);
    if (!wpmw2) CHECK(mmPayload(st) == "Heizen");
    CHECK(mmCount(pw) == 0);   // not yet 5 min
    step(t0 + 300000, 200, 75);
    if (!wpmw2) CHECK(mmPayload(st) == "Heizen");
    if (!wpmw2) {
        CHECK(mmPayload(ph) == "1200");    // 100 Wh in 300 s
        CHECK(mmPayload(pww) == "300");    // 25 Wh in 300 s
        CHECK(mmPayload(pw) == "1500");
    } else {
        CHECK(mmCount(pw) == 0);
    }
    step(t0 + 330000, 200, 250);
    if (!wpmw2) CHECK(mmPayload(st) == "Warmwasser");

    if (!wpmw2) {
        step(t0 + 600000, 200, 250);
        CHECK(mmPayload(ph) == "0");
        CHECK(mmPayload(pww) == "2100");   // 175 Wh in 300 s
        CHECK(mmPayload(pw) == "2100");
        step(t0 + 630000, 200, 250);       // nothing consumed: the last mode stays
        CHECK(mmPayload(st) == "Warmwasser");
        step(t0 + 660000, 10, 5);          // day change: the counters fall, nothing is added
        CHECK(mmPayload(st) == "Warmwasser");
        step(t0 + 900000, 70, 5);
        CHECK(mmPayload(st) == "Heizen");
        CHECK(mmPayload(ph) == "720");     // 60 Wh in 300 s
        CHECK(mmPayload(pww) == "0");
        CHECK(mmPayload(pw) == "720");
    }
    lastVerdichterValue = 0.8f;            // compressor stopped
    step(t0 + 930000, 100, 5);
    CHECK(mmPayload(st) == "Aus");
    if (!wpmw2) {
        step(t0 + 1200000, 130, 5);
        CHECK(mmPayload(pw) == "0");       // no power while the compressor stands
        CHECK(mmPayload(ph) == "0");
    }
    lastVerdichterValue = 2.4f;
    copEnergyValues["KUEHLZUSTAND"] = 1.0f;
    step(t0 + 1230000, 160, 5);
    CHECK(mmPayload(st) == "Kuehlen");
    copEnergyValues["KUEHLZUSTAND"] = 0.0f;
    step(t0 + 1260000, 190, 5);
    if (!wpmw2) CHECK(mmPayload(st) == "Heizen");
    if (wpmw2) {   // WPMW II: the charge pump shows DHW at once (PUMPENSTATUS seen on a WPL13E)
        lastPumpenstatus = 28930;   // 0x7102 DHW charge
        step(t0 + 1290000, 220, 5);
        CHECK(mmPayload(st) == "Warmwasser");
        lastPumpenstatus = 12546;   // 0x3102 heating
        step(t0 + 1320000, 220, 5);
        CHECK(mmPayload(st) == "Heizen");
    }
    mmResetEnergy();
    fake_millis() = 0;
}

TEST_CASE("operating state from the counters follows the last 5 minute window on every model", "[calc][energy]") {
    // energy.h: "Heizen/Warmwasser aus den Energie-Deltas der letzten Minuten". Until the pump
    // status is known this also applies on the WPMW II, so its sums must restart with every
    // 5 minute window there too (no power is published on the WPMW II).
    mmResetEnergy();
    lastVerdichterValue = 2.4f;   // running
    lastPumpenstatus = -1;        // pump status not seen yet
    const std::string st = kRoot + "/calculated/betriebsstatus/state";
    const unsigned long t0 = 2100000000UL;
    auto step = [](unsigned long t, float heizWh, float wwWh) {
        fake_millis() = t;
        copEnergyValues["EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH"] = heizWh;
        copEnergyValues["EL_AUFNAHMELEISTUNG_WW_TAG_WH"] = wwWh;
        publishCombinedEnergySensors();
    };
    step(t0, 100, 50);              // closes any window an earlier test left open
    mqtt_client_instance().clear();
    resetStateCache();
    step(t0 + 30000, 100, 550);     // window 1: 500 Wh hot water
    CHECK(mmPayload(st) == "Warmwasser");
    step(t0 + 300000, 100, 550);    // window 1 ends
    step(t0 + 330000, 200, 550);    // window 2: 100 Wh heating, no hot water
    CHECK(mmPayload(st) == "Heizen");
    mmResetEnergy();
    fake_millis() = 0;
}

// ============================================================================
// discovery.h: discovery payloads of raw signals and calculated sensors (#95, #96)
// ============================================================================

TEST_CASE("discovery: raw signal payload from its ElsterMetaTable row", "[mqtt][discovery]") {
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("AUSSENTEMP");
    REQUIRE(ei != nullptr);
    const ElsterMeta* meta = GetElsterMeta(ei);
    REQUIRE(meta != nullptr);
    publishMqttDiscovery(CanMembers[cm_manager], ei);
    const std::string cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_manager_aussentemp/config");
    REQUIRE(cfg != "<none>");
    // row: { 0x000c, "AUSSENTEMP", ..., "sensor", "temperature", "°C", "measurement", "mdi:thermometer" }
    CHECK(mmField(cfg, "name") == meta->friendlyName);
    CHECK(mmField(cfg, "unique_id") == "stiebel_manager_aussentemp");
    CHECK(mmField(cfg, "default_entity_id") == "sensor." + mmEntityRoot() + "_manager_aussentemp");
    CHECK(mmField(cfg, "state_topic") == kRoot + "/MANAGER/AUSSENTEMP/state");
    CHECK(mmField(cfg, "availability_topic") == kRoot + "/status");
    CHECK(mmField(cfg, "device_class") == "temperature");
    CHECK(mmField(cfg, "unit_of_measurement") == "°C");
    CHECK(mmField(cfg, "state_class") == "measurement");
    CHECK(mmField(cfg, "icon") == "mdi:thermometer");
    CHECK(mmField(cfg, "payload_on") == "<absent>");
    CHECK(mmField(cfg, "json_attributes_topic") == "<absent>");
    const std::string dev = mmDevice(cfg);
    CHECK(mmField(dev, "identifiers") == "[\"stiebel_MANAGER\"");
    CHECK(mmField(dev, "name") == "Manager");
    CHECK(mmField(dev, "via_device") == "stiebel_eltron_" HA_DEVICE_MODEL_STR);
    CHECK(mmField(dev, "manufacturer") == "Stiebel Eltron");
}

TEST_CASE("discovery: empty metadata fields are left out", "[mqtt][discovery]") {
    // HEIZKURVE: { "sensor", NULL, NULL, "measurement", "mdi:chart-line" }, et_cent_val
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_manager], GetElsterIndex("HEIZKURVE"));
    const std::string cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_manager_heizkurve/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "device_class") == "<absent>");
    CHECK(mmField(cfg, "unit_of_measurement") == "<absent>");
    CHECK(mmField(cfg, "state_class") == "measurement");
    CHECK(mmField(cfg, "icon") == "mdi:chart-line");
}

TEST_CASE("discovery: state_class only for numeric value types", "[mqtt][discovery]") {
    // HA rejects a state_class on a sensor whose state is not a number. Numeric decoders:
    // et_dec_val, et_cent_val, et_mil_val, et_hours, et_byte, et_double_val, et_triple_val,
    // et_little_endian (KElsterTable.cpp).
    struct Row { const char* signal; CanMemberType member; const char* sc; };
    const Row numeric[] = {
        {"HEIZKURVE", cm_manager, "measurement"},                      // et_cent_val
        {"TN", cm_manager, "measurement"},                             // et_little_endian
        {"SOLAR_GESAMTERTRAG_MWH", cm_manager, "total_increasing"},    // et_triple_val
        {"WAERMEERTRAG_WW_SUM_MWH", cm_heizmodul, "total_increasing"}, // et_double_val
        {"LZ_VERD_1_HEIZBETRIEB", cm_heizmodul, "total_increasing"},   // et_hours, type defaults
    };
    for (const Row& r : numeric) {
        INFO(r.signal);
        resetDiscoveryState();
        const ElsterIndex* ei = GetElsterIndex(r.signal);
        REQUIRE(ei != nullptr);
        publishMqttDiscovery(CanMembers[r.member], ei);
        REQUIRE(mqtt_client_instance().messages.size() == 1);
        CHECK(mmField(mqtt_client_instance().messages[0].payload, "state_class") == r.sc);
    }
    // the same metadata on a text-valued type: no state_class
    for (ElsterType t : {et_betriebsart, et_err_nr, et_zeit, et_datum, et_dev_id}) {
        INFO(static_cast<int>(t));
        resetDiscoveryState();
        ElsterIndex textual = *GetElsterIndex("AUSSENTEMP");
        textual.Type = t;
        publishMqttDiscovery(CanMembers[cm_manager], &textual);
        REQUIRE(mqtt_client_instance().messages.size() == 1);
        CHECK(mmField(mqtt_client_instance().messages[0].payload, "state_class") == "<absent>");
    }
}

TEST_CASE("discovery: binary signal without metadata gets on/off payloads", "[mqtt][discovery]") {
    // ANTILEG_AKTIV: et_bool, no ElsterMetaTable row -> type defaults, decoded as on/off
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("ANTILEG_AKTIV");
    REQUIRE(ei != nullptr);
    REQUIRE(GetElsterMeta(ei) == nullptr);
    publishMqttDiscovery(CanMembers[cm_kessel], ei);
    const std::string cfg = mmPayload("homeassistant/binary_sensor/" + kRoot + "/stiebel_kessel_antileg_aktiv/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "name") == "ANTILEG_AKTIV");   // no friendly name: the table name
    CHECK(mmField(cfg, "default_entity_id") == "binary_sensor." + mmEntityRoot() + "_kessel_antileg_aktiv");
    CHECK(mmField(cfg, "payload_on") == "on");
    CHECK(mmField(cfg, "payload_off") == "off");
    CHECK(mmField(cfg, "state_class") == "<absent>");
    CHECK(mmField(cfg, "unit_of_measurement") == "<absent>");
    CHECK(mmField(mmDevice(cfg), "name") == "Kessel");
}

TEST_CASE("discovery: fault message carries its attributes topic", "[mqtt][discovery]") {
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_manager], GetElsterIndex("FEHLERMELDUNG"));
    const std::string cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_manager_fehlermeldung/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "json_attributes_topic") == kRoot + "/MANAGER/FEHLERMELDUNG/attributes");
    CHECK(mmField(cfg, "icon") == "mdi:alert-circle-outline");
    CHECK(mmField(cfg, "state_class") == "<absent>");
    // a member without a label keeps its bus name as device name
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_mischermodul_3], GetElsterIndex("RAUMSOLLTEMP_NACHT"));
    REQUIRE(mqtt_client_instance().messages.size() == 1);
    const std::string dev = mmDevice(mqtt_client_instance().messages[0].payload);
    CHECK(mmField(dev, "identifiers") == "[\"stiebel_MISCHERMODUL_3\"");
    CHECK(mmField(dev, "name") == "MISCHERMODUL_3");
}

TEST_CASE("discovery: default entity ID is lower case <root>_<id> without stiebel_ (#96)", "[mqtt][discovery]") {
    const std::string r = mmEntityRoot();
    CHECK(defaultEntityIdJson("sensor", "stiebel_MANAGER_AUSSENTEMP") ==
          "\"default_entity_id\":\"sensor." + r + "_manager_aussentemp\",");
    CHECK(defaultEntityIdJson("text", "other_id") == "\"default_entity_id\":\"text." + r + "_other_id\",");
    CHECK(defaultEntityIdJson("select", "stiebel_a-b") == "\"default_entity_id\":\"select." + r + "_a_b\",");
    // "stiebel_" only as a prefix
    CHECK(defaultEntityIdJson("sensor", "x_stiebel_y") == "\"default_entity_id\":\"sensor." + r + "_x_stiebel_y\",");
}

TEST_CASE("discovery: calculated sensor payload from its calculatedSensors row", "[mqtt][discovery]") {
    resetDiscoveryState();
    // { "stiebel_cop_ww", ..., "sensor", "", "", "measurement", "mdi:water-boiler", "", "", "", true, true, "cop_ww" }
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_WW]);
    std::string cfg = mmPayload("homeassistant/sensor/" + kRoot + "/cop_ww/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "name") == calculatedSensors[CALC_IDX_COP_WW].name);
    CHECK(mmField(cfg, "unique_id") == "stiebel_cop_ww");
    CHECK(mmField(cfg, "default_entity_id") == "sensor." + mmEntityRoot() + "_cop_ww");
    CHECK(mmField(cfg, "state_topic") == kRoot + "/calculated/cop_ww/state");
    CHECK(mmField(cfg, "device_class") == "<absent>");
    CHECK(mmField(cfg, "unit_of_measurement") == "<absent>");
    CHECK(mmField(cfg, "state_class") == "measurement");
    CHECK(mmField(cfg, "icon") == "mdi:water-boiler");
    CHECK(mmField(cfg, "payload_on") == "<absent>");
    CHECK(mmField(cfg, "entity_category") == "<absent>");
    CHECK(mmField(cfg, "enabled_by_default") == "<absent>");
    CHECK(mmField(mmDevice(cfg), "identifiers") == "[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"");
    CHECK(mmField(mmDevice(cfg), "manufacturer") == "Stiebel Eltron");

    // once only, unless forced
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_WW]);
    CHECK(mmCount("homeassistant/sensor/" + kRoot + "/cop_ww/config") == 1);
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COP_WW], true);
    CHECK(mmCount("homeassistant/sensor/" + kRoot + "/cop_ww/config") == 2);

    // { "stiebel_calculated_el_leistung_w", ..., "sensor", "power", "W", "measurement", "mdi:lightning-bolt" }
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_EL_LEISTUNG_W]);
    cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_calculated_el_leistung_w/config");
    CHECK(mmField(cfg, "device_class") == "power");
    CHECK(mmField(cfg, "unit_of_measurement") == "W");
    CHECK(mmField(cfg, "state_class") == "measurement");

    // { "stiebel_calculated_compressor_active", ..., "binary_sensor", "running", "", "", "mdi:engine", "on", "off" }
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_COMPRESSOR_ACTIVE]);
    cfg = mmPayload("homeassistant/binary_sensor/" + kRoot + "/stiebel_calculated_compressor_active/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "device_class") == "running");
    CHECK(mmField(cfg, "payload_on") == "on");
    CHECK(mmField(cfg, "payload_off") == "off");
    CHECK(mmField(cfg, "state_class") == "<absent>");
    CHECK(mmField(cfg, "default_entity_id") == "binary_sensor." + mmEntityRoot() + "_calculated_compressor_active");

    // { "stiebel_calculated_can_tec", ..., "", "", "measurement", ..., "diagnostic", false }
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_CAN_TEC]);
    cfg = mmPayload("homeassistant/sensor/" + kRoot + "/stiebel_calculated_can_tec/config");
    REQUIRE(cfg != "<none>");
    CHECK(mmField(cfg, "entity_category") == "diagnostic");
    CHECK(mmField(cfg, "enabled_by_default") == "false");
    resetDiscoveryState();
}

TEST_CASE("discovery: et_default values keep the state_class of their metadata row", "[mqtt][discovery]") {
    // et_default decodes to a plain integer (%d, KElsterTable.cpp), so it is numeric (#127).
    // { 0x0084, "SOLAR_AKT_LEISTUNG_W", ..., "power", "W", "measurement" }
    resetDiscoveryState();
    const ElsterIndex* ei = GetElsterIndex("SOLAR_AKT_LEISTUNG_W");
    REQUIRE(ei != nullptr);
    REQUIRE(ei->Type == et_default);
    publishMqttDiscovery(CanMembers[cm_manager], ei);
    REQUIRE(mqtt_client_instance().messages.size() == 1);
    CHECK(mmField(mqtt_client_instance().messages[0].payload, "state_class") == "measurement");
}

TEST_CASE("discovery: remainder registers of split counters have no state_class", "[mqtt][discovery]") {
    // Split counters: total = MWh*1000 + kWh, day = kWh + Wh/1000 (energy.h). The fine register is
    // a remainder (0..999) that falls at every carry; as total_increasing HA would read each carry
    // as a meter reset and lose the remainder (seen 19./20.11.: 4.05 of 7.74 kWh counted).
    for (const char* sig : {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH", "WAERMEERTRAG_HEIZ_SUM_KWH"}) {
        INFO(sig);
        resetDiscoveryState();
        const ElsterIndex* ei = GetElsterIndex(sig);
        REQUIRE(ei != nullptr);
        publishMqttDiscovery(CanMembers[cm_heizmodul], ei);
        REQUIRE(mqtt_client_instance().messages.size() == 1);
        const std::string& cfg = mqtt_client_instance().messages[0].payload;
        CHECK(mmField(cfg, "state_class") == "<absent>");
        CHECK(mmField(cfg, "device_class") == "energy");   // still an energy value with a unit
    }
    // The coarse register is the counter and keeps total_increasing
    resetDiscoveryState();
    publishMqttDiscovery(CanMembers[cm_heizmodul], GetElsterIndex("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH"));
    REQUIRE(mqtt_client_instance().messages.size() == 1);
    CHECK(mmField(mqtt_client_instance().messages[0].payload, "state_class") == "total_increasing");
}

TEST_CASE("ElsterMetaTable: no total_increasing row has a coarser register of the same counter", "[elster][discovery]") {
    // Rule from the split-counter layout: X_WH is the remainder of X_KWH, X_KWH of X_MWH.
    auto exists = [](const std::string& name) {
        const ElsterIndex* ei = GetElsterIndex(name.c_str());
        return ei != nullptr && ei != &ElsterTable[0] && name == ei->Name;
    };
    for (unsigned j = 0; j < ElsterMetaCount; j++) {
        const ElsterMeta& m = ElsterMetaTable[j];
        if (!m.stateClass || std::strcmp(m.stateClass, "total_increasing") != 0) continue;
        const std::string n = m.Name;
        INFO(n);
        auto endsWith = [&](const char* suf) {
            const size_t l = std::strlen(suf);
            return n.size() > l && n.compare(n.size() - l, l, suf) == 0;
        };
        if (endsWith("_WH")) CHECK_FALSE(exists(n.substr(0, n.size() - 3) + "_KWH"));
        if (endsWith("_KWH")) CHECK_FALSE(exists(n.substr(0, n.size() - 4) + "_MWH"));
    }
}

TEST_CASE("updateSensor: a total_increasing counter keeps its last state on -255", "[mqtt][discovery]") {
    // HA reads a drop of a total_increasing sensor as a meter reset and adds the next real
    // value to the sum again, so "not available" (0x8000 -> -255) must not reach the state.
    // { 0x091f, "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", ..., "energy", "kWh", "total_increasing" }
    resetDiscoveryState();
    resetStateCache();
    const ElsterIndex* ei = GetElsterIndex("EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH");
    REQUIRE(ei != nullptr);
    const std::string topic = kRoot + "/HEIZMODUL/EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH/state";
    updateSensor(CanMembers[cm_heizmodul], ei, "12");
    REQUIRE(mmPayload(topic) == "12");
    updateSensor(CanMembers[cm_heizmodul], ei, "-255");
    CHECK(mmPayload(topic) == "12");
    CHECK(mmCount(topic) == 1);
    updateSensor(CanMembers[cm_heizmodul], ei, "13");   // a real value goes through again
    CHECK(mmPayload(topic) == "13");
    resetDiscoveryState();
    resetStateCache();
}

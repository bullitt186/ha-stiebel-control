#if !defined(heatingpump_H)
#define heatingpump_H
#include "ElsterTable.h"
#include "KElsterTable.h"
#include <sstream>
#include <iomanip>

typedef struct
{
    const char *Name;
    uint32_t CanId;
    uint8_t ReadId[2];
    uint8_t WriteId[2];
    uint8_t ConfirmationId[2];
} CanMember;

static const CanMember CanMembers[] =
    {
        // TODO: Read/Write-IDs gerade ziehen
        //  Name  CanId     ReadId          WriteId         ConfirmationID
        {"KESSEL", 0x180, {0x31, 0x00}, {0x30, 0x00}, {0x00, 0x00}},
        {"ATEZ", 0x280, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"BEDIENMODUL_1", 0x300, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"BEDIENMODUL_2", 0x301, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"BEDIENMODUL_3", 0x302, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"BEDIENMODUL_4", 0x303, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"RAUMFERNFUEHLER", 0x400, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"MANAGER", 0x480, {0x91, 0x00}, {0x90, 0x00}, {0x00, 0x00}},
        {"HEIZMODUL", 0x500, {0xA1, 0x00}, {0xA0, 0x00}, {0x00, 0x00}},
        {"BUSKOPPLER", 0x580, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"MISCHERMODUL_1", 0x600, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"MISCHERMODUL_2", 0x601, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"MISCHERMODUL_3", 0x602, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"MISCHERMODUL_4", 0x603, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"PC", 0x680, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"FREMDGERAET", 0x700, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"DCF_MODUL", 0x780, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}},
        {"OTHER", 0x000, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}}};

typedef enum
{
    // Die Reihenfolge muss mit CanMembers übereinstimmen!
    cm_kessel = 0,
    cm_atez,
    cm_bedienmodul_1,
    cm_bedienmodul_2,
    cm_bedienmodul_3,
    cm_bedienmodul_4,
    cm_raumfernfuehler,
    cm_manager,
    cm_heizmodul,
    cm_buskoppler,
    cm_mischermodul_1,
    cm_mischermodul_2,
    cm_mischermodul_3,
    cm_mischermodul_4,
    cm_pc,
    cm_fremdgeraet,
    cm_dcf_modul,
    cm_other
} CanMemberType;

const CanMember &lookupCanMember(uint32_t canId)
{
    for (const CanMember &member : CanMembers)
    {
        if (member.CanId == canId)
        {
            return member;
        }
    }
    // Return the last element if member is not found
    return CanMembers[sizeof(CanMembers) / sizeof(CanMembers[0]) - 1];
}

CanMemberType lookupCanMemberType(uint32_t canId)
{
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i)
    {
        if (CanMembers[i].CanId == canId)
        {
            return static_cast<CanMemberType>(i);
        }
    }
    return cm_other; // Return cm_other if member is not found
}

const ElsterIndex *processCanMessage(const std::vector<uint8_t> &msg, uint32_t can_id, std::string &signalValue)
{
    // Return if the message is too small
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const CanMember &cm = lookupCanMember(can_id);

    const ElsterIndex *ei;
    unsigned char byte1;
    unsigned char byte2;
    char charValue[16];

    if (msg[2] == 0xfa)
    {
        byte1 = msg[5];
        byte2 = msg[6];
        ei = GetElsterIndex(msg[4] + (msg[3] << 8));
    }
    else
    {
        byte1 = msg[3];
        byte2 = msg[4];
        ei = GetElsterIndex(msg[2]);
    }

    switch (ei->Type)
    {
    case et_double_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    case et_triple_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    default:
        SetValueType(charValue, ei->Type, static_cast<int>(byte2 + (byte1 << 8)));
        break;
    }

    ESP_LOGI("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(%s)", cm.Name, cm.CanId, ei->Name, charValue, ElsterTypeStr[ei->Type]);

    signalValue = charValue;
    return ei;
}

void readSignal(const CanMember *member, const ElsterIndex *ei)
{
    constexpr bool use_extended_id = false; // No use of extended ID
    const uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    const uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;

    if (IndexByte1 == 0x00)
    {
        data = {member->ReadId[0],
                member->ReadId[1],
                IndexByte2,
                0x00,
                0x00,
                0x00,
                0x00};
    }
    else
    {
        data = {member->ReadId[0],
                member->ReadId[1],
                0xFA,
                IndexByte1,
                IndexByte2,
                0x00,
                0x00};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, member->Name, member->CanId, member->ReadId[0], member->ReadId[1], data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("readSignal()", "%s", logmsg);

    id(my_mcp2515).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void writeSignal(const CanMember *member, const ElsterIndex *ei, const char *&str)
{
    bool use_extended_id = false;
    int writeValue = TranslateString(str, ei->Type);
    uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;

    if (IndexByte1 == 0x00)
    {
        data = {member->WriteId[0],
                member->WriteId[1],
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF),
                0x00,
                0x00};
    }
    else
    {
        data = {member->WriteId[0],
                member->WriteId[1],
                0xfa,
                IndexByte1,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF)};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             ei->Name, ei->Index, writeValue, member->Name, member->CanId, member->WriteId[0], member->WriteId[1],
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("writeSignal()", "%s", logmsg);

    id(my_mcp2515).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
}

void publishDate()
{
    int ijahr = static_cast<int>(id(JAHR).state);
    std::string jahr;
    if (ijahr >= 0 && ijahr < 99)
    {
        jahr = (ijahr < 10) ? "0" + std::to_string(ijahr) : std::to_string(ijahr);
    }
    else
    {
        jahr = "00";
    }

    int imonat = static_cast<int>(id(MONAT).state);
    std::string monat;
    if (imonat > 0 && imonat <= 12)
    {
        monat = (imonat < 10) ? "0" + std::to_string(imonat) : std::to_string(imonat);
    }
    else
    {
        monat = "00";
    }

    int itag = static_cast<int>(id(TAG).state);
    std::string tag;
    if (itag > 0 && itag <= 31)
    {
        tag = (itag < 10) ? "0" + std::to_string(itag) : std::to_string(itag);
    }
    else
    {
        tag = "00";
    }

    std::string datum = "20" + jahr + "-" + monat + "-" + tag;
    id(DATUM).publish_state(datum);
}

std::string formatNumber(int number, int width)
{
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << number;
    return oss.str();
}

void publishTime()
{
    int istunde = (int)id(STUNDE).state;
    std::string stunde = formatNumber(istunde, 2);

    int iminute = (int)id(MINUTE).state;
    std::string minute = formatNumber(iminute, 2);

    int isekunde = (int)id(SEKUNDE).state;
    std::string sekunde = formatNumber(isekunde, 2);

    id(ZEIT).publish_state(stunde + ":" + minute + ":" + sekunde);
}

void update_COP_WW()
{
    float cop_ww = (id(WAERMEERTRAG_WW_SUM).state + id(WAERMEERTRAG_2WE_WW_SUM).state) / id(EL_AUFNAHMELEISTUNG_WW_SUM).state;
    id(COP_WW).publish_state(cop_ww);
}

void update_COP_HEIZ()
{
    float cop_heiz = (id(WAERMEERTRAG_HEIZ_SUM).state + id(WAERMEERTRAG_2WE_HEIZ_SUM).state) / id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).state;
    id(COP_HEIZ).publish_state(cop_heiz);
}

void update_COP_GESAMT()
{
    float cop_gesamt = (id(WAERMEERTRAG_HEIZ_SUM).state + id(WAERMEERTRAG_2WE_HEIZ_SUM).state + id(WAERMEERTRAG_WW_SUM).state + id(WAERMEERTRAG_2WE_WW_SUM).state) / (id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).state + id(EL_AUFNAHMELEISTUNG_WW_SUM).state);
    id(COP_GESAMT).publish_state(cop_gesamt);
}


void updateSensor(uint32_t can_id, const ElsterIndex *ei, std::string value)
{
    CanMemberType cmt = lookupCanMemberType(can_id);

    if (ei->Name == "AUSSENTEMP")
    {
        id(AUSSENTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "EINSTELL_SPEICHERSOLLTEMP")
    {
        id(EINSTELL_SPEICHERSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "EINSTELL_SPEICHERSOLLTEMP2")
    {
        id(EINSTELL_SPEICHERSOLLTEMP2).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "HILFSKESSELSOLL")
    {
        id(HILFSKESSELSOLL).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "KESSELSOLLTEMP")
    {
        id(KESSELSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RUECKLAUFISTTEMP")
    {
        id(RUECKLAUFISTTEMP_KESSEL).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SPEICHERISTTEMP")
    {
        id(SPEICHERISTTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SPEICHERSOLLTEMP")
    {
        id(SPEICHERSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "VORLAUFSOLLTEMP")
    {
        id(VORLAUFSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "WPVORLAUFIST")
    {
        id(WPVORLAUFIST_KESSEL).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "GERAETE_ID")
    {
        switch (cmt)
        {
        case cm_kessel:
            id(GERAETE_ID_KESSEL).publish_state(value);
            break;
        case cm_manager:
            id(GERAETE_ID_MANAGER).publish_state(value);
            break;
        case cm_heizmodul:
            id(GERAETE_ID_HEIZMODUL).publish_state(value);
            break;
        }
        return;
    }
    if (ei->Name == "JAHR")
    {
        id(JAHR).publish_state(std::stoi(value));
        publishDate();
        return;
    }
    if (ei->Name == "LAUFZEIT_VERD_BEI_SPEICHERBEDARF")
    {
        id(LAUFZEIT_VERD_BEI_SPEICHERBEDARF).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "LUEFT_PASSIVKUEHLUNG_UEBER_FORTLUEFTER")
    {
        id(LUEFT_PASSIVKUEHLUNG_UEBER_FORTLUEFTER).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "MINUTE")
    {
        id(MINUTE).publish_state(std::stoi(value));
        publishTime();
        return;
    }
    if (ei->Name == "MONAT")
    {
        id(MONAT).publish_state(std::stoi(value));
        publishDate();
        return;
    }
    if (ei->Name == "PROGRAMMSCHALTER")
    {
        id(PROGRAMMSCHALTER).publish_state(value);
        return;
    }
    if (ei->Name == "RAUMISTTEMP")
    {
        id(RAUMISTTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_I")
    {
        id(RAUMSOLLTEMP_I).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_II")
    {
        id(RAUMSOLLTEMP_II).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_III")
    {
        id(RAUMSOLLTEMP_III).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RAUMSOLLTEMP_NACHT")
    {
        id(RAUMSOLLTEMP_NACHT).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RUECKLAUFISTTEMP")
    {
        id(RUECKLAUFISTTEMP_MANAGER).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "SEKUNDE")
    {
        id(SEKUNDE).publish_state(std::stoi(value));
        publishTime();
        return;
    }
    if (ei->Name == "SOFTWARE_VERSION")
    {
        id(SOFTWARE_VERSION).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "SOMMERBETRIEB")
    {
        id(SOMMERBETRIEB).publish_state(value);
        return;
    }
    if (ei->Name == "STUNDE")
    {
        id(STUNDE).publish_state(std::stoi(value));
        publishTime();
        return;
    }
    if (ei->Name == "TAG")
    {
        id(TAG).publish_state(std::stoi(value));
        publishDate();
        return;
    }
    if (ei->Name == "TEILVORRANG_WW")
    {
        id(TEILVORRANG_WW).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "TEMPORALE_LUEFTUNGSSTUFE_DAUER")
    {
        id(TEMPORALE_LUEFTUNGSSTUFE_DAUER).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "VERDICHTER")
    {
        id(VERDICHTER_MANAGER).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "VERSTELLTE_RAUMSOLLTEMP")
    {
        id(VERSTELLTE_RAUMSOLLTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "WP_STATUS")
    {
        id(WP_STATUS).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "WPVORLAUFIST")
    {
        id(WPVORLAUFIST_MANAGER).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "WW_ECO")
    {
        id(WW_ECO).publish_state(value);
        return;
    }
    if (ei->Name == "ZWISCHENEINSPRITZUNG_ISTTEMP")
    {
        id(ZWISCHENEINSPRITZUNG_ISTTEMP).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "EVU_SPERRE_AKTIV")
    {
        id(EVU_SPERRE_AKTIV_MANAGER).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "VERDAMPFERTEMP")
    {
        id(VERDAMPFERTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "RUECKLAUFISTTEMP")
    {
        id(RUECKLAUFISTTEMP).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "VERDICHTER")
    {
        id(VERDICHTER_HEIZMODUL).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "WPVORLAUFIST")
    {
        id(WPVORLAUFIST).publish_state(std::stof(value));
        return;
    }
    if (ei->Name == "ABTAUUNGAKTIV")
    {
        id(ABTAUUNGAKTIV).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "BETRIEBSART_WP")
    {
        id(ABTAUUNGAKTIV).publish_state(std::stoi(value));
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_HEIZ_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_SUM_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_SUM_KWH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_WW_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_WW_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_TAG_WH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).publish_state(std::stoi(value));
        id(EL_AUFNAHMELEISTUNG_WW_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_WW_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);

        return;
    }
    if (ei->Name == "LZ_VERD_1_2_WW_BETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_2_WW_BETRIEB_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_2_WW_BETRIEB_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_WW_BETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_WW_BETRIEB_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_WW_BETRIEB_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_2_WW_BETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_2_WW_BETRIEB_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_2_WW_BETRIEB_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "SOFTWARE_NUMMER")
    {
        switch (cmt)
        {
        case cm_manager:
            id(SOFTWARE_NUMMER_MANAGER).publish_state(std::stoi(value));
            break;
        case cm_heizmodul:
            id(SOFTWARE_NUMMER_HEIZMODUL).publish_state(std::stoi(value));
            break;
        }
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_HEIZ_SUM_KWH")
    {
        id(WAERMEERTRAG_2WE_HEIZ_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_2WE_HEIZ_SUM).publish_state(id(WAERMEERTRAG_2WE_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_2WE_HEIZ_SUM_KWH).state / 1000);
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_WW_SUM_KWH")
    {
        id(WAERMEERTRAG_2WE_WW_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_2WE_WW_SUM).publish_state(id(WAERMEERTRAG_2WE_WW_SUM_MWH).state + id(WAERMEERTRAG_2WE_WW_SUM_KWH).state / 1000);
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_SUM_KWH")
    {
        id(WAERMEERTRAG_HEIZ_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_HEIZ_SUM).publish_state(id(WAERMEERTRAG_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_TAG_WH")
    {
        id(WAERMEERTRAG_HEIZ_TAG_WH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_HEIZ_TAG).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);
        id(WAERMEERTRAG_HEIZ_INCREASING).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);

        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_SUM_KWH")
    {
        id(WAERMEERTRAG_WW_SUM_KWH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_WW_SUM).publish_state(id(WAERMEERTRAG_WW_SUM_MWH).state + id(WAERMEERTRAG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_TAG_WH")
    {
        id(WAERMEERTRAG_WW_TAG_WH).publish_state(std::stoi(value));
        id(WAERMEERTRAG_WW_TAG).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);
        id(WAERMEERTRAG_WW_INCREASING).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);

        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_HEIZ_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_HEIZ_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH).state / 1000);
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_SUM_MWH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_SUM_MWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_WW_SUM).publish_state(id(EL_AUFNAHMELEISTUNG_WW_SUM_MWH).state + id(EL_AUFNAHMELEISTUNG_WW_SUM_KWH).state / 1000);
        update_COP_WW();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "EL_AUFNAHMELEISTUNG_WW_TAG_KWH")
    {
        id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).publish_state(std::stod(value));
        id(EL_AUFNAHMELEISTUNG_WW_TAG).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        id(EL_AUFNAHMELEISTUNG_WW_INCREASING).publish_state(id(EL_AUFNAHMELEISTUNG_WW_TAG_KWH).state + id(EL_AUFNAHMELEISTUNG_WW_TAG_WH).state / 1000);
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_HEIZ_SUM_MWH")
    {
        id(WAERMEERTRAG_2WE_HEIZ_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_2WE_HEIZ_SUM).publish_state(id(WAERMEERTRAG_2WE_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_2WE_HEIZ_SUM_KWH).state / 1000);
        return;
    }
    if (ei->Name == "WAERMEERTRAG_2WE_WW_SUM_MWH")
    {
        id(WAERMEERTRAG_2WE_WW_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_2WE_WW_SUM).publish_state(id(WAERMEERTRAG_2WE_WW_SUM_MWH).state + id(WAERMEERTRAG_2WE_WW_SUM_KWH).state / 1000);
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_SUM_MWH")
    {
        id(WAERMEERTRAG_HEIZ_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_HEIZ_SUM).publish_state(id(WAERMEERTRAG_HEIZ_SUM_MWH).state + id(WAERMEERTRAG_HEIZ_SUM_KWH).state / 1000);
        update_COP_HEIZ();
        update_COP_GESAMT();
        return;
    }
    if (ei->Name == "WAERMEERTRAG_HEIZ_TAG_KWH")
    {
        id(WAERMEERTRAG_HEIZ_TAG_KWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_HEIZ_TAG).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);
        id(WAERMEERTRAG_HEIZ_INCREASING).publish_state(id(WAERMEERTRAG_HEIZ_TAG_KWH).state + id(WAERMEERTRAG_HEIZ_TAG_WH).state / 1000);

        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_SUM_MWH")
    {
        id(WAERMEERTRAG_WW_SUM_MWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_WW_SUM).publish_state(id(WAERMEERTRAG_WW_SUM_MWH).state + id(WAERMEERTRAG_WW_SUM_KWH).state / 1000);
        return;
    }
    if (ei->Name == "WAERMEERTRAG_WW_TAG_KWH")
    {
        id(WAERMEERTRAG_WW_TAG_KWH).publish_state(std::stod(value));
        id(WAERMEERTRAG_WW_TAG).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);
        id(WAERMEERTRAG_WW_INCREASING).publish_state(id(WAERMEERTRAG_WW_TAG_KWH).state + id(WAERMEERTRAG_WW_TAG_WH).state / 1000);

        return;
    }
    if (ei->Name == "LZ_VERD_1_2_HEIZBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_2_HEIZBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_2_HEIZBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_2_KUEHLBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_2_KUEHLBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_2_KUEHLBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_HEIZBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_HEIZBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_HEIZBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_1_KUEHLBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_1_KUEHLBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_1_KUEHLBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_2_HEIZBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_2_HEIZBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_2_HEIZBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
    if (ei->Name == "LZ_VERD_2_KUEHLBETRIEB")
    {
        switch (cmt)
        {
        case cm_manager:
            id(LZ_VERD_2_KUEHLBETRIEB_MANAGER).publish_state(std::stod(value));
            break;
        case cm_heizmodul:
            id(LZ_VERD_2_KUEHLBETRIEB_HEIZMODUL).publish_state(std::stod(value));
            break;
        }
        return;
    }
}

void identifyCanMembers()
{
    ESP_LOGI("identifyCanMembers()", "Identifying CAN Members...");
    readSignal(&CanMembers[cm_heizmodul], GetElsterIndex("GERAETE_ID"));

    ESP_LOGI("identifyCanMembers()", "Identified CAN Members!");
    return;
}

void processAndUpdate(uint32_t can_id, std::vector<uint8_t> msg)
{
    std::string value;
    const ElsterIndex *ei = processCanMessage(msg, can_id, value);
    updateSensor(can_id, ei, value);
    return;
}

#endif

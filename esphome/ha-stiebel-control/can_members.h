/*
 * can_members.h — bus members and the request row type, without ESPHome dependencies (#95).
 *
 * Shared by the firmware modules, the model request tables and the native test stubs, so the
 * tests use the real SignalRequest layout instead of a copy.
 */

#ifndef CAN_MEMBERS_H
#define CAN_MEMBERS_H

#include <cstdint>

// ============================================================================
// CAN BUS MEMBER DEFINITIONS
// ============================================================================

typedef struct
{
    const char *Name;
    uint32_t CanId;
} CanMember;

static const CanMember CanMembers[] =
    {
        //  Name  CanId
        {"KESSEL", 0x180},
        {"ATEZ", 0x280},
        {"BEDIENMODUL_1", 0x300},
        {"BEDIENMODUL_2", 0x301},
        {"BEDIENMODUL_3", 0x302},
        {"BEDIENMODUL_4", 0x303},
        {"RAUMFERNFUEHLER", 0x400},
        {"RAUMFERNFUEHLER_2", 0x401},
        {"MANAGER", 0x480},
        {"HEIZMODUL", 0x500},
        {"BUSKOPPLER", 0x580},
        {"MISCHERMODUL_1", 0x600},
        {"MISCHERMODUL_3", 0x602},
        {"MISCHERMODUL_4", 0x603},
        {"PC", 0x680},
        {"FREMDGERAET", 0x700},
        {"DCF_MODUL", 0x780},
        // WPL17 (CAN-B, X1.2, 50 kbps) model-specific members. The WPL17 has two
        // buses (CAN-A 20 kbps / CAN-B 50 kbps); these addresses live on CAN-B.
        {"HEIZMODUL_WPL", 0x514},  // WPL17 Heizmodul (fine daily energy counters)
        {"KESSEL_CANB", 0x201},    // WPL17 DHW / room / heating-curve registers
        {"MISCHERMODUL_2", 0x601}, // WPL17 exposes the heating program here
        {"OTHER", 0x000}};

typedef enum
{
    // Same order as CanMembers[] (checked by the static_assert below)
    cm_kessel = 0,
    cm_atez,
    cm_bedienmodul_1,
    cm_bedienmodul_2,
    cm_bedienmodul_3,
    cm_bedienmodul_4,
    cm_raumfernfuehler,
    cm_raumfernfuehler_2,
    cm_manager,
    cm_heizmodul,
    cm_buskoppler,
    cm_mischermodul_1,
    cm_mischermodul_3,
    cm_mischermodul_4,
    cm_pc,
    cm_fremdgeraet,
    cm_dcf_modul,
    cm_heizmodul_wpl,
    cm_kessel_canb,
    cm_mischermodul_2,
    cm_other
} CanMemberType;

static_assert(sizeof(CanMembers) / sizeof(CanMembers[0]) == cm_other + 1,
              "CanMembers[] and CanMemberType must list the same members in the same order");

// Signal request configuration structure
typedef struct {
    const char* signalName;
    unsigned long frequency;     // Request frequency in seconds
    CanMemberType member;        // Use cm_other for "all members"
} SignalRequest;

#endif // CAN_MEMBERS_H

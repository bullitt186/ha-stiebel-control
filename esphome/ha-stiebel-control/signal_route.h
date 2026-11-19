/*
 * signal_route.h - how a signal is addressed: its member/name key and its model route (#109).
 *
 * Pure C++, no ESPHome dependencies. SignalRoute maps a register to the member that is
 * written, read back and owns the state on the current model (kModel).
 */

#ifndef SIGNAL_ROUTE_H
#define SIGNAL_ROUTE_H

#include <cstdint>
#include <cstring>
#include "model_profile.h"
#include "can_members.h"

struct SignalRoute {
    CanMemberType writeMember;
    CanMemberType readMember;
    CanMemberType stateMember;
};

inline SignalRoute resolveSignalRoute(const ModelProfile& model, uint16_t index,
                                     CanMemberType fallback) {
    // WPL9 test candidate: both DHW registers use CAN-B for writes and feedback.
    // Valid live replies and actual WPM write acceptance still need capture.
    if (model.canBDhwRoutes && (index == 0x0013 || index == 0x0a06))
        return {cm_kessel_canb, cm_kessel_canb, cm_kessel_canb};
    // WPL17: DHW setpoints and the heating curve live on KESSEL_CANB.
    if (model.canBRoutes && (index == 0x0013 || index == 0x0a06 || index == 0x010e))
        return {cm_kessel_canb, cm_kessel_canb, cm_kessel_canb};
    return {fallback, fallback, fallback};
}
inline SignalRoute resolveSignalRoute(uint16_t index, CanMemberType fallback) {
    return resolveSignalRoute(kModel, index, fallback);
}

// Models with a WPMW II manager. Writes stay limited per model in writableNumberApplies().
static constexpr bool isWpmw2Model() { return kModel.wpmw2; }

// DJB2 hash of a signal name: h = 5381, then h = h * 33 + c per character (uint32_t wrap).
// The one runtime implementation; hash() in ha-stiebel-control.h is its compile-time twin.
inline uint32_t elsterNameHash(const char* str) {
    uint32_t h = 5381;
    while (*str) h = ((h << 5) + h) + static_cast<uint32_t>(*str++);
    return h;
}

// Key of a (CAN member, signal name) pair without building a string: member CAN ID
// in the upper 32 bits, name hash below. Same identity as the former "MEMBER_SIGNAL"
// string keys and the stiebel_<member>_<signal> uid (ElsterTable names have no
// hash collisions, see test "signalKey: ElsterTable names have distinct hashes").
inline uint64_t signalKey(const CanMember& cm, uint32_t nameHash) {
    return (static_cast<uint64_t>(cm.CanId) << 32) | nameHash;
}
inline uint64_t signalKey(const CanMember& cm, const char* signalName) {
    return signalKey(cm, elsterNameHash(signalName));
}

#endif  // SIGNAL_ROUTE_H

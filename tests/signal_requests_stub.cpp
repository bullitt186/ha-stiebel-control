/*
 * Stub definitions for signalRequests[] and SIGNAL_REQUEST_COUNT_VALUE.
 *
 * Provides the two extern symbols declared in ha-stiebel-control.h without pulling in
 * ha-stiebel-control.h itself (which would cause duplicate statics). The SignalRequest
 * type comes from can_members.h, the same header the firmware uses.
 */
#include <cstddef>
#include "can_members.h"

// Small table for the request manager tests (test_can_logic.cpp).
extern const SignalRequest signalRequests[] = {
    {"AUSSENTEMP",         30, cm_kessel},   // known, one member
    {"SPEICHERISTTEMP",    60, cm_other},    // known, all members (KESSEL, MANAGER, HEIZMODUL)
    {"DOES_NOT_EXIST_XYZ", 30, cm_manager},  // unknown name: never requested
    {"GERAETE_ID",         30, cm_manager},  // blacklisted: never requested
};
extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(signalRequests[0]);

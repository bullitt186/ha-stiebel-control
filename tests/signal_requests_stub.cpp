/*
 * Stub definitions for signalRequests[] and SIGNAL_REQUEST_COUNT_VALUE.
 *
 * Provides the two extern symbols declared in ha-stiebel-control.h without
 * pulling in ha-stiebel-control.h itself (which would cause duplicate statics).
 * The SignalRequest struct is redeclared minimally here — it must match the
 * layout in ha-stiebel-control.h exactly.
 */
#include <cstddef>

struct SignalRequest {
    const char* signalName;
    unsigned long frequency;
    int member;
};

// Small table for the request manager tests (test_mqtt_state.cpp). Member values
// are CanMemberType: 0 = cm_kessel, 8 = cm_manager, 20 = cm_other (checked there).
extern const SignalRequest signalRequests[] = {
    {"AUSSENTEMP",         30, 0},    // known, one member
    {"SPEICHERISTTEMP",    60, 20},   // known, all members (KESSEL, MANAGER, HEIZMODUL)
    {"DOES_NOT_EXIST_XYZ", 30, 8},    // unknown name: never requested
    {"GERAETE_ID",         30, 8},    // blacklisted: never requested
};
extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(signalRequests[0]);

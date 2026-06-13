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

extern const SignalRequest signalRequests[] = {};
extern const size_t SIGNAL_REQUEST_COUNT_VALUE = 0;

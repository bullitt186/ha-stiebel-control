// Load the standard-library locale declarations before the ESPHome id(x)
// stub macro; otherwise GCC expands std::locale::id constructors as macros.
#include <sstream>
#include "esphome_stubs.h"
#include "../esphome/ha-stiebel-control/ha-stiebel-control.h"
#include "../esphome/ha-stiebel-control/signal_requests_wpl9.h"
#include <cassert>
int main() {
    int electrical = 0, heat = 0, runtime = 0, parts = 0;
    for (size_t i = 0; i < SIGNAL_REQUEST_COUNT_VALUE; ++i) {
        const auto& r = signalRequests[i];
        if (strncmp(r.signalName, "EL_AUFNAHMELEISTUNG_", sizeof("EL_AUFNAHMELEISTUNG_")-1) == 0) {
            assert(r.member == cm_heizmodul_wpl); ++electrical;
        }
        if (strncmp(r.signalName, "WAERMEERTRAG_", sizeof("WAERMEERTRAG_")-1) == 0) {
            assert(r.member == cm_heizmodul_wpl); ++heat;
        }
        if (strncmp(r.signalName, "LAUFZEIT_VD_", sizeof("LAUFZEIT_VD_")-1) == 0 || strcmp(r.signalName, "LZ_VERD_1_KUEHLBETRIEB") == 0) {
            assert(r.member == cm_heizmodul_wpl); ++runtime;
        }
        if (strncmp(r.signalName, "VERDICHTER_STARTS_", sizeof("VERDICHTER_STARTS_")-1) == 0) {
            assert(r.member == cm_heizmodul_wpl); ++parts;
        }
        assert(strcmp(r.signalName, "LZ_VERD_1_HEIZBETRIEB") != 0);
    }
    assert(electrical == 8 && heat == 12 && runtime == 4 && parts == 1);
}

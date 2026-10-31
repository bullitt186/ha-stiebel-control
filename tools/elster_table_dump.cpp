// Prints every ElsterTable row as elsterRowText() (one line per row).
// Built by tools/elster_table_split.py against the old (13-field) and the new
// (split metadata) header to prove both tables carry the same data.
//   old layout: -DELSTER_TABLE_DEFINITIONS -DELSTER_HEADER="\"/path/ElsterTable.h\""
//   new layout: -DELSTER_DUMP_LINKED_TABLE, linked with KElsterTable.cpp
#include <cstdio>
#include <cstring>
#include "elster_row_text.h"
#include ELSTER_HEADER

#ifdef ELSTER_TABLE_SPLIT_META
#include "KElsterTable.h"
#endif

int main() {
    for (unsigned i = 0; i < ElsterTableCount; i++) {
        const ElsterIndex& e = ElsterTable[i];
        const char* meta[8] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
#ifdef ELSTER_TABLE_SPLIT_META
        const ElsterMeta* m = GetElsterMeta(&e);
        if (e.hasMetadata && !m) {
            fprintf(stderr, "row %u (%s): hasMetadata but no ElsterMetaTable entry\n", i, e.Name);
            return 1;
        }
        if (m) {
            const char* v[8] = {m->friendlyName, m->haComponent, m->haDeviceClass, m->unit,
                                m->stateClass, m->icon, m->payloadOn, m->payloadOff};
            memcpy(meta, v, sizeof(meta));
        }
#else
        if (e.hasMetadata) {
            const char* v[8] = {e.friendlyName, e.haComponent, e.haDeviceClass, e.unit,
                                e.stateClass, e.icon, e.payloadOn, e.payloadOff};
            memcpy(meta, v, sizeof(meta));
        }
#endif
        printf("%s\n", elsterRowText(e.Name, e.Index, e.Type, e.isBlacklisted, e.hasMetadata, meta).c_str());
    }
    return 0;
}

/*
 * #57: ElsterTable rows were split into a slim main table and the ElsterMetaTable
 * side table. The one-off conversion was verified row by row against the old
 * 13-field table (tools/elster_table_split.py --verify); these checks keep the two
 * tables consistent when signals are added or changed.
 */
#include "catch2/catch_amalgamated.hpp"
#include <cstring>
#include <string>
#include "../esphome/ha-stiebel-control/elster/ElsterTable.h"
#include "../esphome/ha-stiebel-control/elster/KElsterTable.h"

TEST_CASE("ElsterTable split: metadata rows and side table match one to one", "[kelster][elster57]") {
    unsigned metaRows = 0;
    for (unsigned i = 0; i < ElsterTableCount; i++) {
        const ElsterIndex& e = ElsterTable[i];
        if (e.hasMetadata) {
            metaRows++;
            const ElsterMeta* m = GetElsterMeta(&e);
            REQUIRE(m != nullptr);
            CHECK(m->Index == e.Index);
            CHECK(std::string(m->Name) == e.Name);
        } else {
            CHECK(GetElsterMeta(&e) == nullptr);
        }
    }
    CHECK(metaRows == ElsterMetaCount);
    // every side table entry belongs to a metadata row
    for (unsigned j = 0; j < ElsterMetaCount; j++) {
        bool found = false;
        for (unsigned i = 0; i < ElsterTableCount && !found; i++)
            found = ElsterTable[i].hasMetadata && ElsterTable[i].Index == ElsterMetaTable[j].Index &&
                    strcmp(ElsterTable[i].Name, ElsterMetaTable[j].Name) == 0;
        CHECK(found);
    }
}

TEST_CASE("ElsterTable split: main row is 8 bytes on 32-bit targets", "[kelster][elster57]") {
    // Name pointer + Index (2) + Type (1) + one flag byte; padded to 16 on a 64-bit host
    CHECK(sizeof(ElsterIndex) == (sizeof(const char*) == 4 ? 8u : 16u));
}

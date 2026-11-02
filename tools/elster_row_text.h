// One text line per ElsterTable row with every field the firmware reads:
// name, index, type, flags and, for rows with hasMetadata, the eight HA metadata
// strings (NULL and "" are kept apart). Shared by tools/elster_table_dump.cpp
// (old vs new layout) and tests/test_elster_split.cpp (reference digests, #57).
#pragma once
#include <cstdint>
#include <string>

inline std::string elsterRowText(const char* name, unsigned index, unsigned type, bool blacklisted,
                                 bool hasMetadata, const char* const meta[8]) {
    std::string s(name ? name : "<null>");
    char buf[48];
    snprintf(buf, sizeof(buf), "|%04x|%u|%d|%d", index, type, blacklisted ? 1 : 0, hasMetadata ? 1 : 0);
    s += buf;
    if (hasMetadata) {
        for (int i = 0; i < 8; i++) {
            s += '|';
            s += meta[i] ? std::string("\"") + meta[i] + "\"" : std::string("NULL");
        }
    }
    return s;
}

inline uint32_t elsterFnv1a(const std::string& s) {
    uint32_t h = 0x811C9DC5u;
    for (unsigned char c : s) { h ^= c; h *= 0x01000193u; }
    return h;
}

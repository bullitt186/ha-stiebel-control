/*
 * Time programs of the WPMW II (DHW #82, heating circuit 1): parsing and validation,
 * no ESPHome dependencies.
 *
 * The manager holds three switching periods per day in W_WASSERPROG_1_<day>,
 * W_WASSERPROG_1_<day>_SCHALT_2 and _SCHALT_3 (MANAGER, et_time_domain). A period is
 * stored as two quarter-hour counts (start, end, 0..96); 0x8080 means "not used".
 * Home Assistant shows one text per day: "11:00-17:00" or "06:30-09:00,11:00-17:00",
 * "-" for a day without a period.
 */

#ifndef DHW_PROGRAM_H
#define DHW_PROGRAM_H

#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace dhwprog {

static constexpr int DAYS = 7;
static constexpr int PERIODS = 3;
static constexpr const char* NONE = "-";   // a day without a period
static const char* const DAY_CODES[DAYS] = {"MO", "DI", "MI", "DO", "FR", "SA", "SO"};

struct Period { int start; int end; };   // quarter hours, 0..96

inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// "HH:MM" (or "H:MM") on the 15-minute grid, 00:00..24:00 → quarter hours
inline bool parseTime(const std::string& t, int& q) {
    int h = 0, m = 0;
    char extra = 0;
    if (std::sscanf(t.c_str(), "%d:%d%c", &h, &m, &extra) != 2) return false;
    if (t.find(':') == std::string::npos || t.size() < 4 || t.size() > 5) return false;
    if (h < 0 || h > 24 || m < 0 || m > 59 || m % 15 != 0) return false;
    if (h == 24 && m != 0) return false;
    q = h * 4 + m / 15;
    return true;
}

inline std::string formatTime(int q) {
    char b[8];
    std::snprintf(b, sizeof(b), "%02d:%02d", q / 4, 15 * (q % 4));
    return b;
}

inline std::string formatPeriod(const Period& p) { return formatTime(p.start) + "-" + formatTime(p.end); }

// "06:30-09:00, 11:00-17:00" → sorted periods. "", "-" = no period.
// Rejects more than three periods, times off the 15-minute grid, start >= end and overlaps
// (one period may end where the next starts). err says why.
inline bool parsePeriods(const std::string& input, std::vector<Period>& out, std::string& err) {
    out.clear();
    const std::string in = trim(input);
    if (in.empty() || in == NONE) return true;
    size_t pos = 0;
    while (pos <= in.size()) {
        size_t comma = in.find(',', pos);
        std::string item = trim(in.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
        size_t dash = item.find('-');
        Period p{};
        if (dash == std::string::npos || !parseTime(trim(item.substr(0, dash)), p.start) ||
            !parseTime(trim(item.substr(dash + 1)), p.end)) {
            err = "expected HH:MM-HH:MM on a 15 minute step: \"" + item + "\"";
            return false;
        }
        if (p.start >= p.end) { err = "start must be before end: \"" + item + "\""; return false; }
        out.push_back(p);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    if (out.size() > static_cast<size_t>(PERIODS)) { err = "at most 3 periods per day"; return false; }
    std::sort(out.begin(), out.end(), [](const Period& a, const Period& b) { return a.start < b.start; });
    for (size_t i = 1; i < out.size(); i++)
        if (out[i].start < out[i - 1].end) { err = "periods overlap"; return false; }
    return true;
}

// Decoded CAN value ("06:30-23:00" or "not used time domain") → period text, "" if unused.
inline std::string normalizeDecoded(const std::string& v) {
    return v.find(':') == std::string::npos ? std::string() : v;
}

// Text for a day from its three decoded periods.
inline std::string dayText(const std::string periods[PERIODS]) {
    std::string out;
    for (int i = 0; i < PERIODS; i++) {
        if (periods[i].empty()) continue;
        if (!out.empty()) out += ",";
        out += periods[i];
    }
    return out.empty() ? std::string(NONE) : out;
}

// Programs shown in Home Assistant: DHW and heating circuits 1 and 2 (circuit 2: HEIZPROG_1 on 0x602).
static constexpr const char* PREFIX_DHW = "W_WASSERPROG_1_";
static constexpr const char* PREFIX_HEATING = "HEIZPROG_1_";

// "W_WASSERPROG_1_MO" → day 0 slot 0, "..._MO_SCHALT_3" → day 0 slot 2. Block setters
// (MO_FR, SA_SO, MO_SO) and other names → false.
inline bool parseSignalName(const char* name, int& day, int& slot, const char* prefix = PREFIX_DHW) {
    const size_t pl = std::strlen(prefix);
    if (std::strncmp(name, prefix, pl) != 0) return false;
    const char* d = name + pl;
    for (int i = 0; i < DAYS; i++) {
        if (std::strncmp(d, DAY_CODES[i], 2) != 0) continue;
        const char* rest = d + 2;
        if (*rest == '\0') { day = i; slot = 0; return true; }
        if (std::strcmp(rest, "_SCHALT_2") == 0) { day = i; slot = 1; return true; }
        if (std::strcmp(rest, "_SCHALT_3") == 0) { day = i; slot = 2; return true; }
        return false;
    }
    return false;
}

inline std::string signalName(int day, int slot, const char* prefix = PREFIX_DHW) {
    std::string n = std::string(prefix) + DAY_CODES[day];
    if (slot == 1) n += "_SCHALT_2";
    if (slot == 2) n += "_SCHALT_3";
    return n;
}

} // namespace dhwprog

#endif // DHW_PROGRAM_H

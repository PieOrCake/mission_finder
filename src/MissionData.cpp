#include "MissionData.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstring>
#include <algorithm>

namespace MissionFinder {

static std::string ReadFileToString(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/** Skip whitespace; return pointer to first non-ws character. */
static const char* SkipWs(const char* p) {
    while (*p && (unsigned char)*p <= 32)
        p++;
    return p;
}

/** Parse a JSON string (starts after opening "). Returns pointer after closing ", or nullptr on error. */
static const char* ParseJsonString(const char* p, std::string& out) {
    out.clear();
    if (*p != '"')
        return nullptr;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (*p == '"') out += '"';
            else if (*p == '\\') out += '\\';
            else if (*p == '/') out += '/';
            else if (*p == 'b') out += '\b';
            else if (*p == 'f') out += '\f';
            else if (*p == 'n') out += '\n';
            else if (*p == 'r') out += '\r';
            else if (*p == 't') out += '\t';
            else if (*p == 'u') { /* skip \uXXXX */ p += 4; }
            else out += *p;
            p++;
        } else
            out += *p++;
    }
    if (*p != '"')
        return nullptr;
    return p + 1;
}

/** Parse array of 3 strings ["name","zone","waypoint"]. p points after the '['. */
static const char* ParseRow(const char* p, MissionRow& row) {
    p = SkipWs(p);
    if (*p != '"') return nullptr;
    p = ParseJsonString(p, row.name);
    if (!p) return nullptr;
    p = SkipWs(p);
    if (*p != ',') return nullptr;
    p = SkipWs(p + 1);
    p = ParseJsonString(p, row.zone);
    if (!p) return nullptr;
    p = SkipWs(p);
    if (*p != ',') return nullptr;
    p = SkipWs(p + 1);
    p = ParseJsonString(p, row.waypoint);
    if (!p) return nullptr;
    return SkipWs(p);
}

/** Parse array of rows: [[...],[...],...]. p points to '[' of the outer array. */
static const char* ParseRowArray(const char* p, std::vector<MissionRow>& rows) {
    rows.clear();
    if (*p != '[') return nullptr;
    p = SkipWs(p + 1);
    while (*p && *p != ']') {
        p = SkipWs(p);
        if (*p != '[') return nullptr;
        MissionRow row;
        p = ParseRow(p + 1, row);
        if (!p) return nullptr;
        if (*p != ']') return nullptr;
        rows.push_back(std::move(row));
        p = SkipWs(p + 1);
        if (*p == ',') p = SkipWs(p + 1);
    }
    if (*p != ']') return nullptr;
    return p + 1;
}

/** Find key "key" in JSON object and set *start to the value start (after ':'). */
static bool FindKey(const char* json, const char* key, const char*& valueStart) {
    std::string search = "\"";
    search += key;
    search += "\"";
    const char* p = strstr(json, search.c_str());
    if (!p) return false;
    p += search.size();
    p = SkipWs(p);
    if (*p != ':') return false;
    p = SkipWs(p + 1);
    valueStart = p;
    return true;
}

bool MissionData::LoadFromString(const char* json) {
    if (!json || !*json)
        return false;

    const char* p = nullptr;
    if (!FindKey(json, "trek", p)) return false;
    if (!ParseRowArray(p, trek)) return false;

    if (!FindKey(json, "race", p)) return false;
    if (!ParseRowArray(p, race)) return false;

    if (!FindKey(json, "bounty", p)) return false;
    if (!ParseRowArray(p, bounty)) return false;

    if (!FindKey(json, "puzzle", p)) return false;
    if (!ParseRowArray(p, puzzle)) return false;

    if (!FindKey(json, "challenge", p)) return false;
    if (!ParseRowArray(p, challenge)) return false;

    return true;
}

bool MissionData::LoadFromFile(const char* path) {
    std::string json = ReadFileToString(path);
    return LoadFromString(json.c_str());
}

void MissionData::BuildAll(std::vector<std::pair<const char*, const MissionRow*>>& out) const {
    out.clear();
    for (const auto& r : trek)  out.push_back({"Trek",     &r});
    for (const auto& r : race) out.push_back({"Race",     &r});
    for (const auto& r : bounty) out.push_back({"Bounty",  &r});
    for (const auto& r : puzzle) out.push_back({"Puzzle",  &r});
    for (const auto& r : challenge) out.push_back({"Challenge", &r});
}

} // namespace MissionFinder

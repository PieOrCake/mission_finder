#ifndef MISSION_FINDER_MISSION_DATA_H
#define MISSION_FINDER_MISSION_DATA_H

#include <string>
#include <vector>

namespace MissionFinder {

struct MissionRow {
    std::string name;
    std::string zone;
    std::string waypoint;
};

struct MissionData {
    std::vector<MissionRow> trek;
    std::vector<MissionRow> race;
    std::vector<MissionRow> bounty;
    std::vector<MissionRow> puzzle;
    std::vector<MissionRow> challenge;

    /** Load from JSON string in memory. Returns true on success. */
    bool LoadFromString(const char* json);

    /** Load from JSON file (mission_finder_cache.json format). Returns true on success. */
    bool LoadFromFile(const char* path);

    /** Build combined list of (type_name, row) for "All" tab. */
    void BuildAll(std::vector<std::pair<const char*, const MissionRow*>>& out) const;
};

} // namespace MissionFinder

#endif

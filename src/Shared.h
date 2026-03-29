#ifndef MISSION_FINDER_SHARED_H
#define MISSION_FINDER_SHARED_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "MissionData.h"
#include <vector>
#include <utility>

extern const char* ADDON_NAME;
extern AddonAPI_t* APIDefs;
extern AddonDefinition_t AddonDef;
extern bool g_windowVisible;
extern bool g_dataLoaded;
extern MissionFinder::MissionData g_data;
extern std::vector<std::pair<const char*, const MissionFinder::MissionRow*>> g_allRows;
extern HWND g_gameHandle;
extern Mumble::Data* g_mumbleLink;

#endif

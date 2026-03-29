#include "Shared.h"

const char* ADDON_NAME = "MissionFinder";
AddonAPI_t* APIDefs = nullptr;
bool g_windowVisible = false;
bool g_dataLoaded = false;
MissionFinder::MissionData g_data;
std::vector<std::pair<const char*, const MissionFinder::MissionRow*>> g_allRows;
HWND g_gameHandle = nullptr;
Mumble::Data* g_mumbleLink = nullptr;

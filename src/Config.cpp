#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "Config.h"
#include "Shared.h"
#include <string>
#include <fstream>

int g_clickAction = ClickAction_CopyToClipboard;
int g_clickActionLastSaved = -1;

static std::string GetConfigPath() {
    std::string path;
    if (APIDefs) {
        const char* dir = APIDefs->Paths_GetAddonDirectory(ADDON_NAME);
        if (dir && dir[0]) {
            path = dir;
            path += "\\mission_finder.ini";
        }
    }
    return path;
}

void LoadClickActionConfig() {
    std::string path = GetConfigPath();
    if (path.empty()) return;
    std::ifstream f(path);
    if (!f) return;
    int v = 0;
    if (f >> v && (v == 0 || v == 1)) {
        g_clickAction = v;
        g_clickActionLastSaved = v;
    }
}

void SaveClickActionConfig() {
    std::string path = GetConfigPath();
    if (path.empty()) return;
    // Ensure addon directory exists
    const char* dir = APIDefs->Paths_GetAddonDirectory(ADDON_NAME);
    if (dir && dir[0])
        CreateDirectoryA(dir, NULL);
    std::ofstream f(path);
    if (f) {
        f << g_clickAction;
        g_clickActionLastSaved = g_clickAction;
    }
}

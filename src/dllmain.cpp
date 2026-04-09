#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "Shared.h"
#include "Config.h"
#include "UI.h"
#include "imgui.h"
#include "IconData.inc"
#include "EmbeddedMissionData.inc"
#include <cstring>

// Version constants
#define V_MAJOR 0
#define V_MINOR 9
#define V_BUILD 1
#define V_REVISION 2

static const char* KB_TOGGLE = "KB_MISSION_FINDER_TOGGLE";
static const char* QA_ID = "MissionFinder_qa";
static const char* TEX_ICON = "TEX_MISSION_FINDER_ICON";

static HMODULE hSelf;

UINT WndProcCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (!g_gameHandle)
        g_gameHandle = hWnd;
    return uMsg;
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;
    if (strcmp(aIdentifier, KB_TOGGLE) == 0) {
        g_windowVisible = !g_windowVisible;
    }
}

void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree);

    // Get Mumble Link for chat state detection
    g_mumbleLink = (Mumble::Data*)APIDefs->DataLink_Get(DL_MUMBLE_LINK);

    // Register WndProc to capture game HWND
    APIDefs->WndProc_Register(WndProcCallback);

    LoadClickActionConfig();

    // Load embedded mission data
    if (g_data.LoadFromString(EMBEDDED_MISSION_JSON)) {
        g_data.BuildAll(g_allRows);
        g_dataLoaded = true;
        APIDefs->Log(LOGL_INFO, ADDON_NAME, "Loaded embedded mission data.");
    } else {
        APIDefs->Log(LOGL_WARNING, ADDON_NAME, "Failed to parse embedded mission data.");
    }

    // Register render callbacks
    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    // Register keybind
    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE, ProcessKeybind, "CTRL+SHIFT+G");

    // Load icon texture and register Quick Access shortcut
    APIDefs->Textures_LoadFromMemory(TEX_ICON, (void*)bounty_target_icon_png,
        (size_t)bounty_target_icon_png_len, nullptr);
    APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON, KB_TOGGLE, "Mission Finder");

    APIDefs->Log(LOGL_INFO, ADDON_NAME, "Addon loaded successfully.");
}

void AddonUnload() {
    SaveClickActionConfig();

    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->InputBinds_Deregister(KB_TOGGLE);
    APIDefs->WndProc_Deregister(WndProcCallback);

    g_mumbleLink = nullptr;
    g_gameHandle = nullptr;
    APIDefs = nullptr;
}

// Export function
AddonDefinition_t AddonDef{};

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = 0xb308ba49;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Mission Finder";
    AddonDef.Version.Major = V_MAJOR;
    AddonDef.Version.Minor = V_MINOR;
    AddonDef.Version.Build = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author = "PieOrCake.7635";
    AddonDef.Description = "Browse and search guild mission locations. Click a mission to copy its waypoint info or post directly to chat.";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/PieOrCake/mission_finder";
    return &AddonDef;
}

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}
#endif

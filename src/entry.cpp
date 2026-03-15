#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "Shared.h"
#include "MissionData.h"
// Version constants
#define V_MAJOR 0
#define V_MINOR 9
#define V_BUILD 0
#define V_REVISION 0
#include "imgui.h"
#include "IconData.inc"
#include "EmbeddedMissionData.inc"
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <fstream>
#ifdef _WIN32
#include <thread>
#endif

static const char* ADDON_NAME = "MissionFinder";
static const char* KB_TOGGLE = "KB_MISSION_FINDER_TOGGLE";
static const char* WINDOW_NAME = "Pie's Glorious Mission Finder";
static const char* QA_ID = "MissionFinder_qa";
static const char* TEX_ICON = "TEX_MISSION_FINDER_ICON";

static HMODULE hSelf;
static MissionFinder::MissionData g_data;
static bool g_windowVisible = false;
static char g_searchBuf[256] = {};
static std::string g_statusMessage;
static double g_statusTime = 0.0;
static std::vector<std::pair<const char*, const MissionFinder::MissionRow*>> g_allRows;
static bool g_dataLoaded = false;

enum ClickAction { ClickAction_CopyToClipboard = 0, ClickAction_SendChatMessage = 1 };
static int g_clickAction = ClickAction_CopyToClipboard;
static int g_clickActionLastSaved = -1;

struct TabDef {
    const char* label;
    const char* typeName;
    const std::vector<MissionFinder::MissionRow>* rows;
};

static ImVec4 GetTypeColor(const char* typeName) {
    if (!typeName) return ImVec4(1.f, 1.f, 1.f, 1.f);
    if (std::strcmp(typeName, "Trek") == 0)      return ImVec4(0.4f, 0.8f, 0.4f, 1.f);  // green
    if (std::strcmp(typeName, "Race") == 0)      return ImVec4(0.3f, 0.7f, 1.0f, 1.f);  // blue
    if (std::strcmp(typeName, "Bounty") == 0)    return ImVec4(1.0f, 0.45f, 0.35f, 1.f); // red
    if (std::strcmp(typeName, "Puzzle") == 0)    return ImVec4(0.9f, 0.75f, 0.3f, 1.f);  // gold
    if (std::strcmp(typeName, "Challenge") == 0) return ImVec4(0.8f, 0.5f, 0.9f, 1.f);  // purple
    return ImVec4(1.f, 1.f, 1.f, 1.f);
}

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

static void LoadClickActionConfig() {
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

static void SaveClickActionConfig() {
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

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
void AddonRender();
void AddonOptions();

static bool RowMatchesSearch(const MissionFinder::MissionRow* row, const char* query) {
    if (!query || !*query) return true;
    std::string q(query);
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    std::string name = row->name;
    std::string zone = row->zone;
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    std::transform(zone.begin(), zone.end(), zone.begin(), ::tolower);
    return name.find(q) != std::string::npos || zone.find(q) != std::string::npos;
}

#ifdef _WIN32
#define WM_PASTE 0x0302

static HWND FindGameWindow() {
    HWND h = NULL;
    while ((h = FindWindowExW(NULL, h, NULL, NULL)) != NULL) {
        wchar_t title[256] = {};
        if (GetWindowTextW(h, title, 255) > 0) {
            std::wstring t(title);
            if (t.find(L"Guild Wars 2") != std::wstring::npos)
                return h;
        }
    }
    return GetForegroundWindow();
}

static void TypeMessageAndSendFromThread(std::string messageUtf8) {
    HWND game = FindGameWindow();
    if (!game) return;
    SetForegroundWindow(game);
    Sleep(400);

    int n = MultiByteToWideChar(CP_UTF8, 0, messageUtf8.c_str(), (int)messageUtf8.size(), NULL, 0);
    if (n <= 0) return;
    std::wstring wide(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, messageUtf8.c_str(), (int)messageUtf8.size(), &wide[0], n);

    for (wchar_t wc : wide) {
        APIDefs->WndProc_SendToGameOnly(game, WM_CHAR, (WPARAM)wc, 0);
        Sleep(12);
    }

    Sleep(150);
    DWORD sc = (DWORD)MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC);
    LPARAM lDown = 1 | (sc << 16);
    LPARAM lUp = (1u << 31) | (1u << 30) | 1 | (sc << 16);
    APIDefs->WndProc_SendToGameOnly(game, WM_KEYDOWN, VK_RETURN, lDown);
    APIDefs->WndProc_SendToGameOnly(game, WM_KEYUP, VK_RETURN, lUp);
}
#endif

static void OnMissionRowClicked(const char* typeName, const MissionFinder::MissionRow* row) {
    std::string line = std::string(typeName) + " - " + row->name + " - " + row->waypoint;
    ImGui::SetClipboardText(line.c_str());
    if (g_clickAction == ClickAction_SendChatMessage) {
#ifdef _WIN32
        APIDefs->GameBinds_InvokeAsync(GB_UiChatFocus, 150);
        std::thread([line]() {
            Sleep(900);
            TypeMessageAndSendFromThread(line);
        }).detach();
        g_statusMessage = "Sending to chat...";
        g_statusTime = ImGui::GetTime();
#else
        g_statusMessage = "Copied row to clipboard.";
        g_statusTime = ImGui::GetTime();
#endif
    } else {
        g_statusMessage = "Copied row to clipboard.";
        g_statusTime = ImGui::GetTime();
    }
}

static int s_missionSortCol = 1;
static bool s_missionSortAsc = true;
static char s_scrollToLetter = 0;

static void DrawMissionTable(const std::vector<std::pair<const char*, const MissionFinder::MissionRow*>>& filtered) {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY;

    const float bodyH = ImGui::GetContentRegionAvail().y;
    if (bodyH <= 0.f)
        return;

    // Style: stronger alternating rows and hover highlight
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,    ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(1.00f, 1.00f, 1.00f, 0.04f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,  ImVec4(0.30f, 0.55f, 0.80f, 0.40f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,   ImVec4(0.30f, 0.55f, 0.80f, 0.55f));

    if (ImGui::BeginTable("missions", 3, flags, ImVec2(-1, bodyH)))
    {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 75.f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch
            | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortAscending, 1.f);
        ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        if (sort_specs && sort_specs->SpecsCount > 0) {
            s_missionSortCol = sort_specs->Specs[0].ColumnIndex;
            s_missionSortAsc = (sort_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
        }

        auto rows = filtered;
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            int cmp = 0;
            if (s_missionSortCol == 0)
                cmp = std::strcmp(a.first, b.first);
            else if (s_missionSortCol == 1)
                cmp = a.second->name.compare(b.second->name);
            else
                cmp = a.second->zone.compare(b.second->zone);
            return s_missionSortAsc ? (cmp < 0) : (cmp > 0);
        });

        bool scrollHandled = false;
        for (size_t i = 0; i < rows.size(); i++) {
            const char* typeName = rows[i].first;
            const MissionFinder::MissionRow* row = rows[i].second;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleColor(ImGuiCol_Text, GetTypeColor(typeName));
            bool clicked = ImGui::Selectable(typeName, false,
                ImGuiSelectableFlags_SpanAllColumns);
            ImGui::PopStyleColor();
            ImGui::PopID();
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (clicked)
                OnMissionRowClicked(typeName, row);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(row->name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row->zone.c_str());

            if (s_scrollToLetter && !scrollHandled) {
                const char* matchStr = nullptr;
                if (s_missionSortCol == 0) matchStr = typeName;
                else if (s_missionSortCol == 1) matchStr = row->name.c_str();
                else matchStr = row->zone.c_str();
                if (matchStr && toupper((unsigned char)matchStr[0]) == s_scrollToLetter) {
                    ImGui::SetScrollHereY(0.f);
                    scrollHandled = true;
                }
            }
        }
        if (s_scrollToLetter) s_scrollToLetter = 0;
        ImGui::EndTable();
    }

    ImGui::PopStyleColor(4);
}

static void DrawAlphabetIndex(float height) {
    const float alphaW = 16.f;
    ImGui::BeginChild("##alphaIdx", ImVec2(alphaW, height), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    float avail = ImGui::GetContentRegionAvail().y;
    float letterH = avail / 26.f;
    float fontSize = ImGui::GetFontSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int c = 0; c < 26; c++) {
        char letter[2] = { (char)('A' + c), 0 };
        float yStart = letterH * c;
        ImGui::SetCursorPosY(yStart);
        ImGui::PushID(c);
        if (ImGui::InvisibleButton("##l", ImVec2(alphaW, letterH))) {
            s_scrollToLetter = 'A' + c;
        }
        ImVec2 rMin = ImGui::GetItemRectMin();
        ImVec2 rMax = ImGui::GetItemRectMax();
        ImVec2 ts = ImGui::CalcTextSize(letter);
        bool hovered = ImGui::IsItemHovered();
        ImU32 col = hovered ? IM_COL32(255, 200, 100, 255) : IM_COL32(160, 160, 160, 200);
        dl->AddText(ImVec2(rMin.x + (alphaW - ts.x) * 0.5f,
                           rMin.y + (letterH - ts.y) * 0.5f), col, letter);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

static void DrawFilteredTab(const char* childId, float statusH,
    const char* typeName, const std::vector<MissionFinder::MissionRow>* rows)
{
    ImGui::BeginChild(childId, ImVec2(0, -statusH), true, ImGuiWindowFlags_NoScrollbar);
    std::vector<std::pair<const char*, const MissionFinder::MissionRow*>> filtered;
    if (rows) {
        for (const auto& r : *rows) {
            if (RowMatchesSearch(&r, g_searchBuf))
                filtered.push_back({typeName, &r});
        }
    } else {
        for (const auto& p : g_allRows) {
            if (RowMatchesSearch(p.second, g_searchBuf))
                filtered.push_back(p);
        }
    }
    float h = ImGui::GetContentRegionAvail().y;
    DrawAlphabetIndex(h);
    ImGui::SameLine();
    DrawMissionTable(filtered);
    ImGui::EndChild();
}

void AddonRender() {
    if (!g_windowVisible)
        return;

    ImGui::SetNextWindowSize(ImVec2(520, 580), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));

    if (!ImGui::Begin(WINDOW_NAME, &g_windowVisible, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    if (!g_dataLoaded) {
        ImGui::Text("Mission data failed to load.");
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-50.f);
    ImGui::InputTextWithHint("##search", "Search missions...", g_searchBuf, sizeof(g_searchBuf));
    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(-1.f, 0.f)))
        g_searchBuf[0] = '\0';
    ImGui::Spacing();

    float statusH = 0.f;
    if (!g_statusMessage.empty())
        statusH = ImGui::GetTextLineHeightWithSpacing() * 2.f;

    const TabDef tabs[] = {
        { "All",       nullptr,     nullptr          },
        { "Trek",      "Trek",      &g_data.trek     },
        { "Race",      "Race",      &g_data.race     },
        { "Bounty",    "Bounty",    &g_data.bounty   },
        { "Puzzle",    "Puzzle",    &g_data.puzzle   },
        { "Challenge", "Challenge", &g_data.challenge },
    };

    if (ImGui::BeginTabBar("MissionTabs")) {
        for (const auto& tab : tabs) {
            if (ImGui::BeginTabItem(tab.label)) {
                std::string childId = std::string("MissionTableScroll_") + tab.label;
                DrawFilteredTab(childId.c_str(), statusH, tab.typeName, tab.rows);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    if (!g_statusMessage.empty()) {
        const double elapsed = ImGui::GetTime() - g_statusTime;
        const double fadeStart = 2.0;
        const double fadeDuration = 1.0;
        float alpha = 1.f;
        if (elapsed > fadeStart)
            alpha = 1.f - (float)((elapsed - fadeStart) / fadeDuration);
        if (alpha <= 0.f) {
            g_statusMessage.clear();
        } else {
            ImGui::Separator();
            ImVec4 statusColor;
            if (g_statusMessage.find("Copied") != std::string::npos)
                statusColor = ImVec4(0.4f, 0.9f, 0.4f, alpha);
            else if (g_statusMessage.find("Sending") != std::string::npos)
                statusColor = ImVec4(1.0f, 0.85f, 0.3f, alpha);
            else
                statusColor = ImVec4(1.f, 1.f, 1.f, alpha);
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
            ImGui::TextUnformatted(g_statusMessage.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}

void AddonOptions() {
    ImGui::Text("When clicking a mission");
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2.f);
    ImGui::RadioButton("Copy to clipboard", &g_clickAction, ClickAction_CopyToClipboard);
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2.f);
    ImGui::RadioButton("Send a chat message", &g_clickAction, ClickAction_SendChatMessage);
    if (g_clickAction != g_clickActionLastSaved)
        SaveClickActionConfig();
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

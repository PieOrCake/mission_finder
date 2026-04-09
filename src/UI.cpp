#include "UI.h"
#include "Shared.h"
#include "MissionData.h"
#include "Config.h"
#include "ChatMessage.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#ifdef _WIN32
#include <shellapi.h>
#endif

static const char* WINDOW_NAME = "Pie's Glorious Mission Finder";

static char g_searchBuf[256] = {};
static std::string g_statusMessage;
static double g_statusTime = 0.0;

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

static void OnMissionRowClicked(const char* typeName, const MissionFinder::MissionRow* row) {
    std::string line = std::string(typeName) + " - " + row->name + " - " + row->waypoint;
    ImGui::SetClipboardText(line.c_str());
    if (g_clickAction == ClickAction_SendChatMessage) {
        SendToChatAsync(line);
        g_statusMessage = "Sending to chat...";
        g_statusTime = ImGui::GetTime();
    } else {
        g_statusMessage = "Copied row to clipboard.";
        g_statusTime = ImGui::GetTime();
    }
}

static int s_missionSortCol = 1;
static bool s_missionSortAsc = true;
static char s_scrollToLetter = 0;

// Cached display list — only rebuilt when inputs change
static std::vector<std::pair<const char*, const MissionFinder::MissionRow*>> s_displayRows;
static std::string s_prevSearch;
static int s_prevSortCol = 1;
static bool s_prevSortAsc = true;
static const void* s_prevTabRows = nullptr;
static bool s_displayDirty = true;

static void SortDisplayRows() {
    std::sort(s_displayRows.begin(), s_displayRows.end(), [](const auto& a, const auto& b) {
        int cmp = 0;
        if (s_missionSortCol == 0)
            cmp = std::strcmp(a.first, b.first);
        else if (s_missionSortCol == 1)
            cmp = a.second->name.compare(b.second->name);
        else
            cmp = a.second->zone.compare(b.second->zone);
        return s_missionSortAsc ? (cmp < 0) : (cmp > 0);
    });
}

static void RebuildDisplayRows(const char* typeName,
    const std::vector<MissionFinder::MissionRow>* rows)
{
    s_displayRows.clear();
    if (rows) {
        for (const auto& r : *rows) {
            if (RowMatchesSearch(&r, g_searchBuf))
                s_displayRows.push_back({typeName, &r});
        }
    } else {
        for (const auto& p : g_allRows) {
            if (RowMatchesSearch(p.second, g_searchBuf))
                s_displayRows.push_back(p);
        }
    }
    SortDisplayRows();
}

static void DrawMissionTable() {
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

        // Detect sort changes — only resort when column or direction changes
        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        if (sort_specs && sort_specs->SpecsCount > 0) {
            int col = sort_specs->Specs[0].ColumnIndex;
            bool asc = (sort_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
            if (col != s_prevSortCol || asc != s_prevSortAsc) {
                s_missionSortCol = col;
                s_missionSortAsc = asc;
                s_prevSortCol = col;
                s_prevSortAsc = asc;
                SortDisplayRows();
            }
        }

        bool scrollHandled = false;
        for (size_t i = 0; i < s_displayRows.size(); i++) {
            const char* typeName = s_displayRows[i].first;
            const MissionFinder::MissionRow* row = s_displayRows[i].second;
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

    // Detect search or tab change — rebuild filtered+sorted cache
    std::string curSearch(g_searchBuf);
    const void* tabKey = (const void*)rows;
    if (curSearch != s_prevSearch || tabKey != s_prevTabRows || s_displayDirty) {
        s_prevSearch = curSearch;
        s_prevTabRows = tabKey;
        s_displayDirty = false;
        RebuildDisplayRows(typeName, rows);
    }

    float h = ImGui::GetContentRegionAvail().y;
    DrawAlphabetIndex(h);
    ImGui::SameLine();
    DrawMissionTable();
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
    if (ImGui::SmallButton("Homepage")) {
        ShellExecuteA(NULL, "open", "https://pie.rocks.cc/", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Buy me a coffee!")) {
        ShellExecuteA(NULL, "open", "https://ko-fi.com/pieorcake", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::Separator();
    ImGui::Text("When clicking a mission");
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2.f);
    ImGui::RadioButton("Copy to clipboard", &g_clickAction, ClickAction_CopyToClipboard);
    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2.f);
    ImGui::RadioButton("Send a chat message", &g_clickAction, ClickAction_SendChatMessage);
    if (g_clickAction != g_clickActionLastSaved)
        SaveClickActionConfig();
}

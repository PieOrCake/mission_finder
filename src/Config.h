#ifndef MISSION_FINDER_CONFIG_H
#define MISSION_FINDER_CONFIG_H

enum ClickAction { ClickAction_CopyToClipboard = 0, ClickAction_SendChatMessage = 1 };

extern int g_clickAction;
extern int g_clickActionLastSaved;

void LoadClickActionConfig();
void SaveClickActionConfig();

#endif

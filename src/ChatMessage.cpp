#ifdef _WIN32
#include <thread>
#include <chrono>
#endif
#include "ChatMessage.h"
#include "Shared.h"

#ifdef _WIN32
static const int MSG_DELAY_MS = 50;

static LPARAM GetLParam(uint32_t vk, bool down) {
    int64_t lp = !down;        // transition state
    lp = lp << 1;
    lp += !down;               // previous key state
    lp = lp << 1;
    lp += 0;                   // context code
    lp = lp << 1;
    lp = lp << 4;
    lp = lp << 1;
    lp = lp << 8;
    lp += MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    lp = lp << 16;
    lp += 1;
    return (LPARAM)lp;
}

static bool CopyToOsClipboard(HWND hwnd, const std::string& utf8) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (wlen <= 0) return false;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR) * (wlen + 1));
    if (!hMem) return false;

    WCHAR* wBuf = (WCHAR*)GlobalLock(hMem);
    if (!wBuf) { GlobalFree(hMem); return false; }
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wBuf, wlen);
    wBuf[wlen] = L'\0';
    GlobalUnlock(hMem);

    if (!OpenClipboard(hwnd)) { GlobalFree(hMem); return false; }
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}

static void PasteAndSendFromThread(std::string message) {
    using namespace std::chrono_literals;
    HWND game = g_gameHandle;
    if (!game) return;

    // Set the OS clipboard
    if (!CopyToOsClipboard(game, message)) return;

    // Open chat if not already focused
    if (!g_mumbleLink || !g_mumbleLink->Context.IsTextboxFocused) {
        SendMessage(game, WM_KEYDOWN, VK_RETURN, GetLParam(VK_RETURN, true));
        SendMessage(game, WM_KEYUP, VK_RETURN, GetLParam(VK_RETURN, false));
        std::this_thread::sleep_for(std::chrono::milliseconds(MSG_DELAY_MS));
    }

    // Ctrl down (via SendInput — GW2 needs modifier at hardware level)
    INPUT ctrlDown[1] = {};
    ctrlDown[0].type = INPUT_KEYBOARD;
    ctrlDown[0].ki.wVk = VK_CONTROL;
    ctrlDown[0].ki.wScan = (WORD)MapVirtualKeyA(VK_CONTROL, MAPVK_VK_TO_VSC);
    SendInput(1, ctrlDown, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_DELAY_MS));

    // V key (via SendMessage)
    SendMessage(game, WM_KEYDOWN, 'V', GetLParam('V', true));
    SendMessage(game, WM_KEYUP, 'V', GetLParam('V', false));
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_DELAY_MS));

    // Ctrl up (via SendInput)
    INPUT ctrlUp[1] = {};
    ctrlUp[0].type = INPUT_KEYBOARD;
    ctrlUp[0].ki.wVk = VK_CONTROL;
    ctrlUp[0].ki.wScan = (WORD)MapVirtualKeyA(VK_CONTROL, MAPVK_VK_TO_VSC);
    ctrlUp[0].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, ctrlUp, sizeof(INPUT));
    std::this_thread::sleep_for(std::chrono::milliseconds(MSG_DELAY_MS));

    // Enter to send
    SendMessage(game, WM_KEYDOWN, VK_RETURN, GetLParam(VK_RETURN, true));
    SendMessage(game, WM_KEYUP, VK_RETURN, GetLParam(VK_RETURN, false));
}
#endif

void SendToChatAsync(const std::string& message) {
#ifdef _WIN32
    std::thread([message]() {
        PasteAndSendFromThread(message);
    }).detach();
#else
    (void)message;
#endif
}

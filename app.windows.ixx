module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <magnification.h>
#include <windows.h>

export module app.windows;

import app.types;
import app.layout;

export namespace app {

void UpdateWindowRegion(HWND hwnd) {
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return;
    }
    HRGN region = CreateRectRgn(0, 0, width, height);
    if (!region) {
        return;
    }
    if (SetWindowRgn(hwnd, region, TRUE) == 0) {
        DeleteObject(region);
    }
}

void EnsureMagnifierFilter() {
    if (!g_state.magnifier) {
        return;
    }
    HWND excluded[2] = {g_state.frame, g_state.capture};
    MagSetWindowFilterList(g_state.magnifier, MW_FILTERMODE_EXCLUDE, 2, excluded);
}

RECT GetFrameContentRectScreen() {
    RECT client{};
    GetClientRect(g_state.frame, &client);

    RECT content = MakeRect(0, 0, client.right, client.bottom);

    if (content.right <= content.left || content.bottom <= content.top) {
        return RECT{};
    }

    POINT topLeft{content.left, content.top};
    POINT bottomRight{content.right, content.bottom};
    ClientToScreen(g_state.frame, &topLeft);
    ClientToScreen(g_state.frame, &bottomRight);

    return MakeRect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
}

void UpdateCaptureFromFrame() {
    if (!g_state.capture || !g_state.magnifier || !g_state.frame) {
        return;
    }

    RECT source = GetFrameContentRectScreen();
    int width = source.right - source.left;
    int height = source.bottom - source.top;

    if (width <= 0 || height <= 0) {
        return;
    }

    SetWindowPos(g_state.capture, nullptr, source.left, source.top, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(g_state.magnifier, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

    if (g_state.streamPaused) {
        InvalidateRect(g_state.capture, nullptr, FALSE);
        return;
    }

    MagSetWindowSource(g_state.magnifier, source);
    InvalidateRect(g_state.magnifier, nullptr, FALSE);
}

void ApplyPresetSize(HWND hwnd, int contentWidth, int contentHeight) {
    int width = contentWidth + g_state.borderThickness * 2;
    int height = contentHeight + g_state.borderThickness * 2;

    SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

RECT ClampToMonitor(const RECT& rect) {
    RECT result = rect;
    HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        return result;
    }

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int workWidth = info.rcWork.right - info.rcWork.left;
    int workHeight = info.rcWork.bottom - info.rcWork.top;

    width = std::min(width, workWidth);
    height = std::min(height, workHeight);

    int maxX = info.rcWork.right - width;
    int maxY = info.rcWork.bottom - height;

    result.left = std::clamp<LONG>(rect.left, info.rcWork.left, static_cast<LONG>(maxX));
    result.top = std::clamp<LONG>(rect.top, info.rcWork.top, static_cast<LONG>(maxY));
    result.right = result.left + width;
    result.bottom = result.top + height;
    return result;
}

void ShowPresetMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(PresetId::Size1920x1080), L"1920 x 1080");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(PresetId::Size1600x900), L"1600 x 900");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(PresetId::Size1280x720), L"1280 x 720");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(PresetId::Size1024x768), L"1024 x 768");
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(PresetId::Size800x600), L"800 x 600");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    UINT command = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                  pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    if (command != 0) {
        g_state.suppressPresetClickUntil = 0;
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        return;
    }

    POINT cursor{};
    if (!g_state.locked && GetCursorPos(&cursor)) {
        ScreenToClient(hwnd, &cursor);
        UpdateButtonRects(hwnd);
        if (PointInRect(g_state.presetRect, cursor)) {
            g_state.suppressPresetClickUntil = GetTickCount64() + 250;
            return;
        }
    }
    g_state.suppressPresetClickUntil = 0;
}

void SetFrameTopMost(HWND hwnd, bool topMost) {
    SetWindowPos(hwnd, topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void SetFrameMouseThrough(HWND hwnd, bool enabled) {
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool isEnabled = (exStyle & WS_EX_TRANSPARENT) != 0;
    if (enabled == isEnabled) {
        return;
    }
    if (enabled) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool IsCursorOverLockButton(HWND hwnd) {
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return false;
    }
    ScreenToClient(hwnd, &pt);
    UpdateButtonRects(hwnd);
    return PointInRect(g_state.lockRect, pt);
}

void UpdateLockedMouseThrough(HWND hwnd) {
    bool shouldBeTransparent = g_state.locked && !IsCursorOverLockButton(hwnd);
    if (shouldBeTransparent != g_state.mouseThrough) {
        g_state.mouseThrough = shouldBeTransparent;
        SetFrameMouseThrough(hwnd, shouldBeTransparent);
    }
}

void SetLocked(HWND hwnd, bool locked) {
    g_state.locked = locked;
    g_state.hoverPreset = false;
    g_state.hoverPause = false;
    g_state.hoverClose = false;
    g_state.suppressPresetClickUntil = 0;
    UpdateMetrics(g_state.dpi);
    UpdateCaptureFromFrame();
    UpdateLockedMouseThrough(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void SetStreamPaused(HWND hwnd, bool paused) {
    if (g_state.streamPaused == paused) {
        return;
    }

    g_state.streamPaused = paused;

    if (g_state.magnifier) {
        ShowWindow(g_state.magnifier, paused ? SW_HIDE : SW_SHOW);
    }

    UpdateCaptureFromFrame();
    if (!paused && g_state.magnifier) {
        InvalidateRect(g_state.magnifier, nullptr, FALSE);
    }
    if (g_state.capture) {
        InvalidateRect(g_state.capture, nullptr, TRUE);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

} // namespace app

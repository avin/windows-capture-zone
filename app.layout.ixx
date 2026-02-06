module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

export module app.layout;

import app.types;

export namespace app {

int ScaleForDpi(int value, UINT dpi) {
    return static_cast<int>(MulDiv(value, static_cast<int>(dpi), 96));
}

void UpdateMetrics(UINT dpi) {
    g_state.dpi = dpi;
    g_state.topBarHeight = ScaleForDpi(kBaseTopBarHeight, dpi);
    g_state.borderThickness = ScaleForDpi(kBaseBorderThickness, dpi);
    g_state.dragHandleWidth = ScaleForDpi(kBaseDragHandleWidth, dpi);
    g_state.buttonSize = ScaleForDpi(kBaseButtonSize, dpi);
    g_state.buttonPadding = ScaleForDpi(kBaseButtonPadding, dpi);
    g_state.presetButtonWidth = g_state.buttonSize;
}

RECT MakeRect(int left, int top, int right, int bottom) {
    RECT rect{};
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    return rect;
}

bool PointInRect(const RECT& rect, POINT pt) {
    return pt.x >= rect.left && pt.x < rect.right && pt.y >= rect.top && pt.y < rect.bottom;
}

void UpdateButtonRects(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);

    int top = g_state.borderThickness;

    int unlockedWidth = g_state.buttonPadding * 2 + g_state.dragHandleWidth + g_state.buttonPadding +
                        g_state.buttonSize + g_state.buttonPadding + g_state.presetButtonWidth + g_state.buttonPadding +
                        g_state.buttonSize;

    int barLeft = g_state.borderThickness + ScaleForDpi(kControlLeftOffset, g_state.dpi);

    g_state.topBarRect = MakeRect(barLeft, top, barLeft + unlockedWidth, top + g_state.topBarHeight);

    int contentTop = top + (g_state.topBarHeight - g_state.buttonSize) / 2;
    int cursor = barLeft + g_state.buttonPadding;

    g_state.lockRect = MakeRect(cursor, contentTop, cursor + g_state.buttonSize, contentTop + g_state.buttonSize);
    cursor = g_state.lockRect.right + g_state.buttonPadding;

    g_state.dragRect = MakeRect(cursor, top, cursor + g_state.dragHandleWidth, top + g_state.topBarHeight);
    cursor = g_state.dragRect.right + g_state.buttonPadding;

    g_state.presetRect =
        MakeRect(cursor, contentTop, cursor + g_state.presetButtonWidth, contentTop + g_state.buttonSize);
    cursor = g_state.presetRect.right + g_state.buttonPadding;

    g_state.closeRect = MakeRect(cursor, contentTop, cursor + g_state.buttonSize, contentTop + g_state.buttonSize);
}

int GetUnlockedControlWidth() {
    return g_state.buttonPadding * 2 + g_state.buttonSize + g_state.buttonPadding + g_state.dragHandleWidth +
           g_state.buttonPadding + g_state.presetButtonWidth + g_state.buttonPadding + g_state.buttonSize;
}

int GetControlLeftOffset() {
    return ScaleForDpi(kControlLeftOffset, g_state.dpi);
}

} // namespace app

module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "resource.h"

#include <algorithm>
#include <cstdlib>
#include <windows.h>

export module app.graphics;

import app.types;
import app.layout;

namespace {

struct ScopedBrush {
    HBRUSH brush = nullptr;
    explicit ScopedBrush(COLORREF color) : brush(CreateSolidBrush(color)) {}
    ~ScopedBrush() {
        if (brush) {
            DeleteObject(brush);
        }
    }
    HBRUSH get() const { return brush; }
};

struct ScopedPen {
    HPEN pen = nullptr;
    ScopedPen(int style, int width, COLORREF color) : pen(CreatePen(style, width, color)) {}
    ~ScopedPen() {
        if (pen) {
            DeleteObject(pen);
        }
    }
    HPEN get() const { return pen; }
};

struct ScopedSelect {
    HDC hdc = nullptr;
    HGDIOBJ old = nullptr;
    ScopedSelect(HDC hdc, HGDIOBJ obj) : hdc(hdc), old(SelectObject(hdc, obj)) {}
    ~ScopedSelect() {
        if (hdc && old) {
            SelectObject(hdc, old);
        }
    }
};

int SnapIconSize(int desired) {
    static const int kSizes[] = {16, 20, 24, 28, 32, 40, 48, 64};
    int best = kSizes[0];
    int bestDelta = std::abs(desired - best);
    for (int size : kSizes) {
        int delta = std::abs(desired - size);
        if (delta < bestDelta) {
            best = size;
            bestDelta = delta;
        }
    }
    return best;
}

HICON LoadIconResource(int resourceId, int size) {
    return static_cast<HICON>(
        LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(resourceId), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
}

} // namespace

export namespace app {

void DrawBorder(HDC hdc, const RECT& client) {
    ScopedBrush brush(kBorderColor);
    RECT top = MakeRect(0, 0, client.right, g_state.borderThickness);
    RECT bottom = MakeRect(0, client.bottom - g_state.borderThickness, client.right, client.bottom);
    RECT left = MakeRect(0, 0, g_state.borderThickness, client.bottom);
    RECT right = MakeRect(client.right - g_state.borderThickness, 0, client.right, client.bottom);

    FillRect(hdc, &top, brush.get());
    FillRect(hdc, &bottom, brush.get());
    FillRect(hdc, &left, brush.get());
    FillRect(hdc, &right, brush.get());
}

void DrawDragHandle(HDC hdc, const RECT& dragRect) {
    if (g_state.locked) {
        return;
    }

    int dotsAreaWidth = std::min(g_state.dragHandleWidth, static_cast<int>(dragRect.right - dragRect.left));
    int dotsLeft = dragRect.left + g_state.buttonPadding / 2;
    int dotsRight = dotsLeft + dotsAreaWidth;
    int dotsTop = dragRect.top + (dragRect.bottom - dragRect.top) / 2 - ScaleForDpi(6, g_state.dpi);
    int dotsBottom = dotsTop + ScaleForDpi(12, g_state.dpi);

    int dotSize = std::max(2, ScaleForDpi(2, g_state.dpi));
    int gap = std::max(3, ScaleForDpi(4, g_state.dpi));

    ScopedBrush brush(kDragDotsColor);
    ScopedPen pen(PS_SOLID, 1, kDragDotsColor);
    ScopedSelect brushSelect(hdc, brush.get());
    ScopedSelect penSelect(hdc, pen.get());

    for (int y = dotsTop; y + dotSize <= dotsBottom; y += gap) {
        for (int x = dotsLeft; x + dotSize <= dotsRight; x += gap) {
            Ellipse(hdc, x, y, x + dotSize, y + dotSize);
        }
    }
}

void DrawButtonBase(HDC hdc, const RECT& rect, bool active, bool hover) {
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    COLORREF fill = kButtonColor;
    if (active) {
        fill = kButtonActiveColor;
    } else if (hover) {
        fill = kButtonHoverColor;
    }

    ScopedBrush brush(fill);
    FillRect(hdc, &rect, brush.get());
}

void DrawIcon(HDC hdc, const RECT& rect, HICON icon) {
    if (!icon) {
        return;
    }
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int size = std::min(g_state.iconSize, std::min(width, height));
    int x = rect.left + (width - size) / 2;
    int y = rect.top + (height - size) / 2;
    DrawIconEx(hdc, x, y, icon, size, size, 0, nullptr, DI_NORMAL);
}

void DestroyIcons() {
    if (g_state.lockIcon) {
        DestroyIcon(g_state.lockIcon);
        g_state.lockIcon = nullptr;
    }
    if (g_state.unlockIcon) {
        DestroyIcon(g_state.unlockIcon);
        g_state.unlockIcon = nullptr;
    }
    if (g_state.sizeIcon) {
        DestroyIcon(g_state.sizeIcon);
        g_state.sizeIcon = nullptr;
    }
    if (g_state.closeIcon) {
        DestroyIcon(g_state.closeIcon);
        g_state.closeIcon = nullptr;
    }
}

void DestroyAppIcons() {
    if (g_state.appIconBig) {
        DestroyIcon(g_state.appIconBig);
        g_state.appIconBig = nullptr;
    }
    if (g_state.appIconSmall) {
        DestroyIcon(g_state.appIconSmall);
        g_state.appIconSmall = nullptr;
    }
}

void ReloadIcons() {
    DestroyIcons();
    int desired = std::max(12, g_state.buttonSize - ScaleForDpi(4, g_state.dpi));
    g_state.iconSize = SnapIconSize(desired);
    g_state.lockIcon = LoadIconResource(IDI_LOCK, g_state.iconSize);
    g_state.unlockIcon = LoadIconResource(IDI_UNLOCK, g_state.iconSize);
    g_state.sizeIcon = LoadIconResource(IDI_SIZE, g_state.iconSize);
    g_state.closeIcon = LoadIconResource(IDI_CLOSE, g_state.iconSize);
}

void LoadAppIcons(UINT dpi) {
    DestroyAppIcons();
    int bigSize = GetSystemMetricsForDpi(SM_CXICON, dpi);
    int smallSize = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    g_state.appIconBig = LoadIconResource(IDI_APP, bigSize);
    g_state.appIconSmall = LoadIconResource(IDI_APP, smallSize);
}

void ApplyAppIcons(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    if (g_state.appIconBig) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_state.appIconBig));
    }
    if (g_state.appIconSmall) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_state.appIconSmall));
    }
}

} // namespace app

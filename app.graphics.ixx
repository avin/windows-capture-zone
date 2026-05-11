module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <algorithm>
#include <cstdlib>
#include <windows.h>
#include <shellapi.h>

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

HFONT CreateIconFont(int pixelSize) {
    int height = -std::max(8, pixelSize);
    HFONT font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             L"Segoe MDL2 Assets");
    if (font) {
        return font;
    }
    return CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
}

HICON LoadStockIcon(SHSTOCKICONID iconId, int size) {
    SHSTOCKICONINFO info{};
    info.cbSize = sizeof(info);

    const int smallIconSize = GetSystemMetrics(SM_CXSMICON);
    const UINT flags = SHGSI_ICON | (size <= smallIconSize ? SHGSI_SMALLICON : SHGSI_LARGEICON);
    if (FAILED(SHGetStockIconInfo(iconId, flags, &info))) {
        return nullptr;
    }
    return info.hIcon;
}

} // namespace

export namespace app {

void DrawBorder(HDC hdc, const RECT& client) {
    ScopedBrush brush(g_state.streamPaused ? kBorderPausedColor : kBorderColor);
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

void DrawIcon(HDC hdc, const RECT& rect, wchar_t glyph) {
    if (glyph == L'\0' || !g_state.iconFont) {
        return;
    }

    ScopedSelect fontSelect(hdc, g_state.iconFont);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldTextColor = SetTextColor(hdc, RGB(230, 230, 230));

    wchar_t text[2] = {glyph, L'\0'};
    RECT textRect = rect;
    DrawTextW(hdc, text, 1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
}

void DrawSlashedIcon(HDC hdc, const RECT& rect, wchar_t glyph) {
    DrawIcon(hdc, rect, glyph);

    int inset = std::max(4, ScaleForDpi(4, g_state.dpi));
    int width = std::max(2, ScaleForDpi(2, g_state.dpi));
    ScopedPen pen(PS_SOLID, width, RGB(230, 230, 230));
    ScopedSelect penSelect(hdc, pen.get());

    MoveToEx(hdc, rect.left + inset, rect.bottom - inset, nullptr);
    LineTo(hdc, rect.right - inset, rect.top + inset);
}

void DestroyIcons() {
    if (g_state.iconFont) {
        DeleteObject(g_state.iconFont);
        g_state.iconFont = nullptr;
    }
    g_state.lockGlyph = L'\0';
    g_state.unlockGlyph = L'\0';
    g_state.sizeGlyph = L'\0';
    g_state.closeGlyph = L'\0';
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
    g_state.iconFont = CreateIconFont(g_state.iconSize);
    g_state.lockGlyph = L'\uE72E';
    g_state.unlockGlyph = L'\uE785';
    g_state.sizeGlyph = L'\uE713';
    g_state.viewGlyph = L'\uE890';
    g_state.closeGlyph = L'\uE711';
}

void DrawPausedCapturePlaceholder(HDC hdc, const RECT& client) {
    if (client.right <= client.left || client.bottom <= client.top) {
        return;
    }

    ScopedBrush brush(RGB(86, 35, 130));
    FillRect(hdc, &client, brush.get());
}

void LoadAppIcons(UINT dpi) {
    DestroyAppIcons();
    int bigSize = GetSystemMetricsForDpi(SM_CXICON, dpi);
    int smallSize = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    g_state.appIconBig = LoadStockIcon(SIID_DESKTOPPC, bigSize);
    g_state.appIconSmall = LoadStockIcon(SIID_DESKTOPPC, smallSize);
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

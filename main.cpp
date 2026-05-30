#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <dwmapi.h>
#include <magnification.h>
#include <windows.h>
#include <windowsx.h>

import app.types;
import app.layout;
import app.graphics;
import app.settings;
import app.windows;

using namespace app;

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            g_state.frame = hwnd;
            UpdateMetrics(GetDpiForWindow(hwnd));
            ReloadIcons();
            ApplyAppIcons(hwnd);
            {
                DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
                DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
            }
            {
                const DWORD cornerPreference = DWMWCP_DONOTROUND;
                DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference,
                                      sizeof(cornerPreference));
            }
            UpdateWindowRegion(hwnd);
            UpdateCaptureFromFrame();
            SetTimer(hwnd, kUpdateTimerId, kUpdateIntervalMs, nullptr);
            UpdateLockedMouseThrough(hwnd);
            return 0;
        }
        case WM_DESTROY:
            g_state.shuttingDown = true;
            KillTimer(hwnd, kUpdateTimerId);
            SaveSettings(hwnd);
            DestroyIcons();
            DestroyAppIcons();
            if (g_state.capture) {
                DestroyWindow(g_state.capture);
                g_state.capture = nullptr;
            }
            MagUninitialize();
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client{};
            GetClientRect(hwnd, &client);
            auto paintFrame = [&](HDC target) {
                HBRUSH transparentBrush = CreateSolidBrush(kTransparentKey);
                FillRect(target, &client, transparentBrush);
                DeleteObject(transparentBrush);

                UpdateButtonRects(hwnd);
                if (!g_state.locked) {
                    HBRUSH topBrush = CreateSolidBrush(kTopBarColor);
                    FillRect(target, &g_state.topBarRect, topBrush);
                    DeleteObject(topBrush);
                    DrawDragHandle(target, g_state.dragRect);
                }

                DrawButtonBase(target, g_state.lockRect, g_state.locked, g_state.hoverLock);
                DrawIcon(target, g_state.lockRect, g_state.locked ? g_state.lockGlyph : g_state.unlockGlyph);

                if (!g_state.locked) {
                    DrawButtonBase(target, g_state.presetRect, false, g_state.hoverPreset);
                    DrawIcon(target, g_state.presetRect, g_state.sizeGlyph);

                    DrawButtonBase(target, g_state.pauseRect, false, g_state.hoverPause);
                    if (g_state.streamPaused) {
                        DrawSlashedIcon(target, g_state.pauseRect, g_state.viewGlyph);
                    } else {
                        DrawIcon(target, g_state.pauseRect, g_state.viewGlyph);
                    }

                    DrawButtonBase(target, g_state.closeRect, false, g_state.hoverClose);
                    DrawIcon(target, g_state.closeRect, g_state.closeGlyph);
                }

                DrawBorder(target, client);
            };

            int width = client.right - client.left;
            int height = client.bottom - client.top;
            if (width > 0 && height > 0) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBitmap = memDC ? CreateCompatibleBitmap(hdc, width, height) : nullptr;
                if (memDC && memBitmap) {
                    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);
                    paintFrame(memDC);
                    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
                    SelectObject(memDC, oldBitmap);
                    DeleteObject(memBitmap);
                    DeleteDC(memDC);
                } else {
                    if (memBitmap) {
                        DeleteObject(memBitmap);
                    }
                    if (memDC) {
                        DeleteDC(memDC);
                    }
                    paintFrame(hdc);
                }
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DPICHANGED: {
            UINT newDpi = HIWORD(wparam);
            UpdateMetrics(newDpi);
            ReloadIcons();
            LoadAppIcons(newDpi);
            ApplyAppIcons(g_state.frame);
            ApplyAppIcons(g_state.capture);

            RECT* suggested = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);

            UpdateWindowRegion(hwnd);
            UpdateCaptureFromFrame();
            UpdateLockedMouseThrough(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        case WM_ACTIVATE:
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_NCACTIVATE:
            return TRUE;
        case WM_NCPAINT:
            return 0;
        case WM_SIZE:
        case WM_MOVE:
            UpdateWindowRegion(hwnd);
            UpdateCaptureFromFrame();
            UpdateLockedMouseThrough(hwnd);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_TIMER:
            if (wparam == kUpdateTimerId) {
                UpdateCaptureFromFrame();
                UpdateLockedMouseThrough(hwnd);
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            g_state.dragInProgress = true;
            SetFrameTopMost(hwnd, false);
            return 0;
        case WM_EXITSIZEMOVE:
            g_state.dragInProgress = false;
            SetFrameTopMost(hwnd, true);
            return 0;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            UpdateCaptureFromFrame();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            int minWidth = std::max(kMinContentWidth, GetControlLeftOffset() + GetUnlockedControlWidth()) +
                           g_state.borderThickness * 2;
            int minHeight = kMinContentHeight + g_state.borderThickness * 2;
            info->ptMinTrackSize.x = minWidth;
            info->ptMinTrackSize.y = minHeight;
            return 0;
        }
        case WM_NCCALCSIZE:
            return 0;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &pt);

            UpdateButtonRects(hwnd);

            if (PointInRect(g_state.lockRect, pt)) {
                return HTCLIENT;
            }
            if (g_state.locked) {
                return HTTRANSPARENT;
            }
            if (!g_state.locked && PointInRect(g_state.presetRect, pt)) {
                return HTCLIENT;
            }
            if (!g_state.locked && PointInRect(g_state.pauseRect, pt)) {
                return HTCLIENT;
            }
            if (!g_state.locked && PointInRect(g_state.closeRect, pt)) {
                return HTCLIENT;
            }
            if (!g_state.locked && PointInRect(g_state.dragRect, pt)) {
                return HTCAPTION;
            }
            RECT client{};
            GetClientRect(hwnd, &client);

            if (!g_state.locked) {
                int resizeThickness = g_state.borderThickness;
                bool left = pt.x < resizeThickness;
                bool right = pt.x >= client.right - resizeThickness;
                bool top = pt.y < resizeThickness;
                bool bottom = pt.y >= client.bottom - resizeThickness;

                if (top && left)
                    return HTTOPLEFT;
                if (top && right)
                    return HTTOPRIGHT;
                if (bottom && left)
                    return HTBOTTOMLEFT;
                if (bottom && right)
                    return HTBOTTOMRIGHT;
                if (left)
                    return HTLEFT;
                if (right)
                    return HTRIGHT;
                if (top)
                    return HTTOP;
                if (bottom)
                    return HTBOTTOM;
            }

            return HTTRANSPARENT;
        }
        case WM_MOUSEMOVE: {
            POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            UpdateButtonRects(hwnd);
            bool hoverLock = PointInRect(g_state.lockRect, pt);
            bool hoverPreset = !g_state.locked && PointInRect(g_state.presetRect, pt);
            bool hoverPause = !g_state.locked && PointInRect(g_state.pauseRect, pt);
            bool hoverClose = !g_state.locked && PointInRect(g_state.closeRect, pt);

            if (hoverLock != g_state.hoverLock || hoverPreset != g_state.hoverPreset ||
                hoverPause != g_state.hoverPause || hoverClose != g_state.hoverClose) {
                g_state.hoverLock = hoverLock;
                g_state.hoverPreset = hoverPreset;
                g_state.hoverPause = hoverPause;
                g_state.hoverClose = hoverClose;
                InvalidateRect(hwnd, nullptr, FALSE);
            }

            if (!g_state.trackingMouse) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                g_state.trackingMouse = true;
            }

            UpdateLockedMouseThrough(hwnd);
            return 0;
        }
        case WM_MOUSELEAVE:
            g_state.trackingMouse = false;
            if (g_state.hoverLock || g_state.hoverPreset || g_state.hoverPause || g_state.hoverClose) {
                g_state.hoverLock = false;
                g_state.hoverPreset = false;
                g_state.hoverPause = false;
                g_state.hoverClose = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            POINT pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            UpdateButtonRects(hwnd);

            if (PointInRect(g_state.lockRect, pt)) {
                SetLocked(hwnd, !g_state.locked);
                return 0;
            }
            if (!g_state.locked && PointInRect(g_state.presetRect, pt)) {
                if (g_state.suppressPresetClickUntil != 0 && GetTickCount64() <= g_state.suppressPresetClickUntil) {
                    g_state.suppressPresetClickUntil = 0;
                    return 0;
                }
                g_state.suppressPresetClickUntil = 0;
                ShowPresetMenu(hwnd);
                return 0;
            }
            if (!g_state.locked && PointInRect(g_state.pauseRect, pt)) {
                SetStreamPaused(hwnd, !g_state.streamPaused);
                return 0;
            }
            if (!g_state.locked && PointInRect(g_state.closeRect, pt)) {
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wparam)) {
                case static_cast<UINT>(PresetId::ToggleCursorCapture):
                    SetCaptureCursor(hwnd, !g_state.captureCursor);
                    return 0;
                case static_cast<UINT>(PresetId::Size1920x1080):
                    ApplyPresetSize(hwnd, 1920, 1080);
                    return 0;
                case static_cast<UINT>(PresetId::Size1600x900):
                    ApplyPresetSize(hwnd, 1600, 900);
                    return 0;
                case static_cast<UINT>(PresetId::Size1280x720):
                    ApplyPresetSize(hwnd, 1280, 720);
                    return 0;
                case static_cast<UINT>(PresetId::Size1024x768):
                    ApplyPresetSize(hwnd, 1024, 768);
                    return 0;
                case static_cast<UINT>(PresetId::Size800x600):
                    ApplyPresetSize(hwnd, 800, 600);
                    return 0;
                default:
                    return 0;
            }
        }
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

LRESULT CALLBACK CaptureProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            g_state.capture = hwnd;

            DWORD magnifierStyle = WS_CHILD | WS_VISIBLE;
            if (g_state.captureCursor) {
                magnifierStyle |= MS_SHOWMAGNIFIEDCURSOR;
            }
            g_state.magnifier =
                CreateWindowExW(0, WC_MAGNIFIER, nullptr, magnifierStyle, 0, 0, 1, 1, hwnd, nullptr,
                                reinterpret_cast<LPCREATESTRUCT>(lparam)->hInstance, nullptr);
            if (!g_state.magnifier) {
                return -1;
            }

            MAGTRANSFORM transform{};
            transform.v[0][0] = 1.0f;
            transform.v[1][1] = 1.0f;
            transform.v[2][2] = 1.0f;
            MagSetWindowTransform(g_state.magnifier, &transform);

            EnsureMagnifierFilter();
            ApplyCaptureCursorStyle();
            UpdateCaptureFromFrame();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            if (g_state.streamPaused) {
                RECT client{};
                GetClientRect(hwnd, &client);
                DrawPausedCapturePlaceholder(hdc, client);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_state.capture = nullptr;
            if (!g_state.shuttingDown) {
                g_state.shuttingDown = true;
                if (g_state.frame && IsWindow(g_state.frame)) {
                    DestroyWindow(g_state.frame);
                } else {
                    PostQuitMessage(0);
                }
            }
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int cmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!MagInitialize()) {
        MessageBoxW(nullptr, L"Failed to initialize the Magnification API.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    WindowSettings settings = LoadSettings();

    g_state.locked = settings.locked;
    g_state.captureCursor = settings.captureCursor;

    UINT initialDpi = GetDpiForSystem();
    UpdateMetrics(initialDpi);
    LoadAppIcons(initialDpi);

    int width = kDefaultContentWidth + g_state.borderThickness * 2;
    int height = kDefaultContentHeight + g_state.borderThickness * 2;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    if (settings.hasPlacement && settings.width > 0 && settings.height > 0) {
        width = settings.width;
        height = settings.height;
        x = settings.x;
        y = settings.y;
    }

    RECT initialRect = MakeRect(x, y, x + width, y + height);
    initialRect = ClampToMonitor(initialRect);

    WNDCLASSEXW frameClass{};
    frameClass.cbSize = sizeof(frameClass);
    frameClass.style = CS_HREDRAW | CS_VREDRAW;
    frameClass.lpfnWndProc = FrameProc;
    frameClass.hInstance = instance;
    frameClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    frameClass.hbrBackground = nullptr;
    frameClass.hIcon = g_state.appIconBig;
    frameClass.hIconSm = g_state.appIconSmall;
    frameClass.lpszClassName = kFrameClassName;
    RegisterClassExW(&frameClass);

    WNDCLASSEXW captureClass{};
    captureClass.cbSize = sizeof(captureClass);
    captureClass.style = CS_HREDRAW | CS_VREDRAW;
    captureClass.lpfnWndProc = CaptureProc;
    captureClass.hInstance = instance;
    captureClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    captureClass.hbrBackground = nullptr;
    captureClass.hIcon = g_state.appIconBig;
    captureClass.hIconSm = g_state.appIconSmall;
    captureClass.lpszClassName = kCaptureClassName;
    RegisterClassExW(&captureClass);

    DWORD frameStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    DWORD frameExStyle = WS_EX_TOPMOST | WS_EX_LAYERED;

    HWND frame = CreateWindowExW(frameExStyle, kFrameClassName, kFrameTitle, frameStyle, initialRect.left,
                                 initialRect.top, initialRect.right - initialRect.left,
                                 initialRect.bottom - initialRect.top, nullptr, nullptr, instance, nullptr);

    if (!frame) {
        MagUninitialize();
        return 1;
    }

    g_state.frame = frame;
    ApplyAppIcons(frame);
    SetLayeredWindowAttributes(frame, kTransparentKey, 0, LWA_COLORKEY);
    SetWindowDisplayAffinity(frame, WDA_EXCLUDEFROMCAPTURE);
    SetWindowPos(frame, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    DWORD captureStyle = WS_POPUP;
    DWORD captureExStyle = WS_EX_APPWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT;

    HWND capture = CreateWindowExW(captureExStyle, kCaptureClassName, kCaptureTitle, captureStyle, initialRect.left,
                                   initialRect.top, 1, 1, nullptr, nullptr, instance, nullptr);

    if (!capture) {
        DestroyWindow(frame);
        MagUninitialize();
        return 1;
    }

    g_state.capture = capture;
    ApplyAppIcons(capture);
    SetLayeredWindowAttributes(capture, 0, kCaptureAlpha, LWA_ALPHA);

    ShowWindow(capture, SW_SHOWNORMAL);
    ShowWindow(frame, cmdShow == 0 ? SW_SHOW : cmdShow);
    UpdateWindow(frame);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

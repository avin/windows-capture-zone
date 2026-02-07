module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

export module app.types;

export namespace app {

constexpr wchar_t kFrameClassName[] = L"WindowsCaptureZoneFrameWindow";
constexpr wchar_t kCaptureClassName[] = L"WindowsCaptureZoneCaptureWindow";
constexpr wchar_t kAppTitle[] = L"Windows Capture Zone";
constexpr wchar_t kFrameTitle[] = L"";
constexpr wchar_t kCaptureTitle[] = L"<<<CAPTURE ME>>>";

constexpr UINT_PTR kUpdateTimerId = 1;
constexpr UINT kUpdateIntervalMs = 16;

constexpr int kBaseTopBarHeight = 28;
constexpr int kBaseBorderThickness = 4;
constexpr int kBaseDragHandleWidth = 60;
constexpr int kBaseButtonSize = 22;
constexpr int kBaseButtonPadding = 6;
constexpr int kDefaultContentWidth = 1280;
constexpr int kDefaultContentHeight = 720;

constexpr int kMinContentWidth = 320;
constexpr int kMinContentHeight = 200;

constexpr COLORREF kBorderColor = RGB(0, 200, 255);
constexpr COLORREF kTopBarColor = RGB(26, 26, 26);
constexpr COLORREF kButtonColor = RGB(40, 40, 40);
constexpr COLORREF kButtonHoverColor = RGB(60, 60, 60);
constexpr COLORREF kButtonActiveColor = RGB(0, 140, 200);
constexpr COLORREF kDragDotsColor = RGB(180, 180, 180);
constexpr COLORREF kTransparentKey = RGB(255, 0, 255);
constexpr BYTE kCaptureAlpha = 1;
constexpr int kControlLeftOffset = 100;

constexpr LPCWSTR kRegistryPath = L"Software\\windows-capture-zone";

enum class PresetId : UINT {
    Size1920x1080 = 1001,
    Size1600x900 = 1002,
    Size1280x720 = 1003,
    Size1024x768 = 1004,
    Size800x600 = 1005
};

struct WindowSettings {
    bool hasPlacement = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool locked = false;
};

struct AppState {
    HWND frame = nullptr;
    HWND capture = nullptr;
    HWND magnifier = nullptr;
    UINT dpi = 96;
    bool locked = false;
    bool mouseThrough = false;
    bool shuttingDown = false;
    bool trackingMouse = false;
    bool hoverLock = false;
    bool hoverPreset = false;
    bool hoverClose = false;
    bool dragInProgress = false;
    int topBarHeight = kBaseTopBarHeight;
    int borderThickness = kBaseBorderThickness;
    int dragHandleWidth = kBaseDragHandleWidth;
    int buttonSize = kBaseButtonSize;
    int buttonPadding = kBaseButtonPadding;
    int presetButtonWidth = kBaseButtonSize;
    int iconSize = 16;
    RECT topBarRect{};
    RECT dragRect{};
    RECT lockRect{};
    RECT presetRect{};
    RECT closeRect{};
    HFONT iconFont = nullptr;
    wchar_t lockGlyph = L'\0';
    wchar_t unlockGlyph = L'\0';
    wchar_t sizeGlyph = L'\0';
    wchar_t closeGlyph = L'\0';
    HICON appIconBig = nullptr;
    HICON appIconSmall = nullptr;
};

inline AppState g_state{};

} // namespace app

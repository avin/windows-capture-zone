module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

export module app.settings;

import app.types;

export namespace app {

WindowSettings LoadSettings() {
    WindowSettings settings{};

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return settings;
    }

    bool hasX = false;
    bool hasY = false;
    bool hasW = false;
    bool hasH = false;

    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(key, nullptr, L"X", RRF_RT_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        settings.x = static_cast<int>(value);
        hasX = true;
    }
    size = sizeof(value);
    if (RegGetValueW(key, nullptr, L"Y", RRF_RT_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        settings.y = static_cast<int>(value);
        hasY = true;
    }
    size = sizeof(value);
    if (RegGetValueW(key, nullptr, L"W", RRF_RT_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        settings.width = static_cast<int>(value);
        hasW = true;
    }
    size = sizeof(value);
    if (RegGetValueW(key, nullptr, L"H", RRF_RT_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        settings.height = static_cast<int>(value);
        hasH = true;
    }
    size = sizeof(value);
    if (RegGetValueW(key, nullptr, L"Locked", RRF_RT_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
        settings.locked = value != 0;
    }

    settings.hasPlacement = hasX && hasY && hasW && hasH;

    RegCloseKey(key);
    return settings;
}

void SaveSettings(HWND hwnd) {
    RECT rect{};
    GetWindowRect(hwnd, &rect);

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }

    DWORD value = static_cast<DWORD>(rect.left);
    RegSetValueExW(key, L"X", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    value = static_cast<DWORD>(rect.top);
    RegSetValueExW(key, L"Y", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    value = static_cast<DWORD>(rect.right - rect.left);
    RegSetValueExW(key, L"W", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    value = static_cast<DWORD>(rect.bottom - rect.top);
    RegSetValueExW(key, L"H", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    value = static_cast<DWORD>(g_state.locked ? 1 : 0);
    RegSetValueExW(key, L"Locked", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));

    RegCloseKey(key);
}

} // namespace app

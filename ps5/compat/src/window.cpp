// Super Mario Galaxy for the PlayStation 5 - windows, the cursor, the keyboard,
// monitors and the console window, as a machine with one 3840x2160 display and
// no desktop: no window exists, none has focus, no key is down. Nebula's
// runtime then takes its paths for a missing window, and input comes from the
// pad through XInput (xinput.cpp).
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <cstdio>
#include <cwchar>

namespace {
constexpr LONG kScreenWidth = 3840;
constexpr LONG kScreenHeight = 2160;
HMONITOR const kMonitor = reinterpret_cast<HMONITOR>(0x1000);
}  // namespace

int GetSystemMetrics(int index) noexcept {
    switch (index) {
    case SM_CXSCREEN:
    case SM_CXVIRTUALSCREEN: return kScreenWidth;
    case SM_CYSCREEN:
    case SM_CYVIRTUALSCREEN: return kScreenHeight;
    case SM_CMONITORS: return 1;
    case SM_CXDOUBLECLK:
    case SM_CYDOUBLECLK: return 4;
    case SM_CXCURSOR:
    case SM_CYCURSOR: return 32;
    default: return 0;
    }
}

BOOL PeekMessageW(LPMSG, HWND, UINT, UINT, UINT) noexcept { return FALSE; }
BOOL GetMessageW(LPMSG, HWND, UINT, UINT) noexcept { return FALSE; }
BOOL TranslateMessage(const MSG*) noexcept { return FALSE; }
LRESULT DispatchMessageW(const MSG*) noexcept { return 0; }
BOOL PostMessageW(HWND, UINT, WPARAM, LPARAM) noexcept { return FALSE; }
LRESULT SendMessageW(HWND, UINT, WPARAM, LPARAM) noexcept { return 0; }
BOOL PostThreadMessageW(DWORD, UINT, WPARAM, LPARAM) noexcept { return FALSE; }
void PostQuitMessage(int) noexcept {  }
LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM) noexcept { return 0; }
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds, DWORD) noexcept { if (count == 0) { Sleep(milliseconds == INFINITE ? 1 : milliseconds); return WAIT_TIMEOUT; } return WaitForMultipleObjects(count, handles, wait_all, milliseconds); }
DWORD MsgWaitForMultipleObjectsEx(DWORD count, const HANDLE* handles, DWORD milliseconds, DWORD wake_mask, DWORD) noexcept { return MsgWaitForMultipleObjects(count, handles, FALSE, milliseconds, wake_mask); }
ATOM RegisterClassExW(const WNDCLASSEXW*) noexcept { return 1; }
BOOL UnregisterClassW(LPCWSTR, HINSTANCE) noexcept { return TRUE; }
HWND CreateWindowExW(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) noexcept { return nullptr; }
BOOL DestroyWindow(HWND) noexcept { return FALSE; }
BOOL ShowWindow(HWND, int) noexcept { return FALSE; }
BOOL UpdateWindow(HWND) noexcept { return FALSE; }
BOOL IsWindow(HWND) noexcept { return FALSE; }
BOOL IsWindowVisible(HWND) noexcept { return FALSE; }
BOOL IsIconic(HWND) noexcept { return FALSE; }
BOOL IsZoomed(HWND) noexcept { return FALSE; }
HWND GetForegroundWindow() noexcept { return nullptr; }
BOOL SetForegroundWindow(HWND) noexcept { return FALSE; }
HWND GetActiveWindow() noexcept { return nullptr; }
HWND SetActiveWindow(HWND) noexcept { return nullptr; }
HWND GetFocus() noexcept { return nullptr; }
HWND SetFocus(HWND) noexcept { return nullptr; }
HWND GetDesktopWindow() noexcept { return nullptr; }
HWND GetConsoleWindow() noexcept { return nullptr; }
HWND GetAncestor(HWND, UINT) noexcept { return nullptr; }
HWND GetParent(HWND) noexcept { return nullptr; }
DWORD GetWindowThreadProcessId(HWND, LPDWORD process_id) noexcept { if (process_id != nullptr) *process_id = 0; return 0; }
BOOL GetClientRect(HWND, LPRECT) noexcept { return FALSE; }
BOOL GetWindowRect(HWND, LPRECT) noexcept { return FALSE; }
BOOL ClientToScreen(HWND, LPPOINT) noexcept { return FALSE; }
BOOL ScreenToClient(HWND, LPPOINT) noexcept { return FALSE; }
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) noexcept { return FALSE; }
BOOL MoveWindow(HWND, int, int, int, int, BOOL) noexcept { return FALSE; }
BOOL AdjustWindowRect(LPRECT, DWORD, BOOL) noexcept { return FALSE; }
BOOL AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD) noexcept { return FALSE; }
BOOL AdjustWindowRectExForDpi(LPRECT, DWORD, BOOL, DWORD, UINT) noexcept { return FALSE; }
LONG_PTR SetWindowLongPtrW(HWND, int, LONG_PTR) noexcept { return 0; }
LONG_PTR GetWindowLongPtrW(HWND, int) noexcept { return 0; }
LONG SetWindowLongW(HWND, int, LONG) noexcept { return 0; }
LONG GetWindowLongW(HWND, int) noexcept { return 0; }
BOOL SetWindowTextW(HWND, LPCWSTR) noexcept { return FALSE; }
int GetWindowTextW(HWND, LPWSTR, int) noexcept { return 0; }
int GetClassNameW(HWND, LPWSTR, int) noexcept { return 0; }
BOOL EnumWindows(WNDENUMPROC, LPARAM) noexcept { return FALSE; }
BOOL EnumThreadWindows(DWORD, WNDENUMPROC, LPARAM) noexcept { return FALSE; }
BOOL GetWindowPlacement(HWND, WINDOWPLACEMENT*) noexcept { return FALSE; }
BOOL SetWindowPlacement(HWND, const WINDOWPLACEMENT*) noexcept { return FALSE; }
BOOL InvalidateRect(HWND, const RECT*, BOOL) noexcept { return FALSE; }
BOOL ValidateRect(HWND, const RECT*) noexcept { return FALSE; }
BOOL FlashWindow(HWND, BOOL) noexcept { return FALSE; }
BOOL BringWindowToTop(HWND) noexcept { return FALSE; }
BOOL SetPropW(HWND, LPCWSTR, HANDLE) noexcept { return FALSE; }
HANDLE GetPropW(HWND, LPCWSTR) noexcept { return nullptr; }
HANDLE RemovePropW(HWND, LPCWSTR) noexcept { return nullptr; }
HWND FindWindowW(LPCWSTR, LPCWSTR) noexcept { return nullptr; }
HWND WindowFromPoint(POINT) noexcept { return nullptr; }
BOOL SetConsoleTitleW(LPCWSTR) noexcept { return TRUE; }
BOOL SetConsoleOutputCP(UINT) noexcept { return TRUE; }
BOOL SetConsoleCtrlHandler(BOOL (*)(DWORD), BOOL) noexcept { return TRUE; }
BOOL AllocConsole() noexcept { return FALSE; }
BOOL FreeConsole() noexcept { return FALSE; }
BOOL AttachConsole(DWORD) noexcept { return FALSE; }
HANDLE GetStdHandle(DWORD) noexcept { return nullptr; }
BOOL GetConsoleMode(HANDLE, LPDWORD) noexcept { return FALSE; }
BOOL SetConsoleMode(HANDLE, DWORD) noexcept { return FALSE; }
int MessageBoxW(HWND, LPCWSTR text, LPCWSTR caption, UINT) noexcept {
    std::fprintf(stderr, "[message] %s: %s\n", compat::utf8(caption).c_str(), compat::utf8(text).c_str());
    return IDOK;
}
int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT) noexcept {
    std::fprintf(stderr, "[message] %s: %s\n", caption != nullptr ? caption : "", text != nullptr ? text : "");
    return IDOK;
}
BOOL GetCursorPos(LPPOINT) noexcept { return FALSE; }
BOOL SetCursorPos(int, int) noexcept { return FALSE; }
HCURSOR SetCursor(HCURSOR) noexcept { return nullptr; }
HCURSOR GetCursor() noexcept { return nullptr; }
int ShowCursor(BOOL show) noexcept { return show ? 0 : -1; }
BOOL ClipCursor(const RECT*) noexcept { return FALSE; }
BOOL GetClipCursor(LPRECT) noexcept { return FALSE; }
HCURSOR LoadCursorW(HINSTANCE, LPCWSTR) noexcept { return nullptr; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR) noexcept { return nullptr; }
HICON LoadIconW(HINSTANCE, LPCWSTR) noexcept { return nullptr; }
HANDLE LoadImageW(HINSTANCE, LPCWSTR, UINT, int, int, UINT) noexcept { return nullptr; }
BOOL DestroyIcon(HICON) noexcept { return FALSE; }
HWND GetCapture() noexcept { return nullptr; }
HWND SetCapture(HWND) noexcept { return nullptr; }
BOOL ReleaseCapture() noexcept { return FALSE; }
SHORT GetAsyncKeyState(int) noexcept { return 0; }
SHORT GetKeyState(int) noexcept { return 0; }
BOOL GetKeyboardState(PBYTE) noexcept { return FALSE; }
UINT MapVirtualKeyA(UINT, UINT) noexcept { return 0; }
UINT MapVirtualKeyW(UINT, UINT) noexcept { return 0; }
int GetKeyNameTextW(LONG, LPWSTR, int) noexcept { return 0; }
BOOL TrackMouseEvent(LPTRACKMOUSEEVENT) noexcept { return TRUE; }
BOOL RegisterRawInputDevices(PCRAWINPUTDEVICE, UINT, UINT) noexcept { return TRUE; }
UINT GetRawInputData(HRAWINPUT, UINT, LPVOID, PUINT size, UINT) noexcept { if (size != nullptr) *size = 0; return 0; }
HMONITOR MonitorFromWindow(HWND, DWORD) noexcept { return kMonitor; }
HMONITOR MonitorFromPoint(POINT, DWORD) noexcept { return kMonitor; }
HMONITOR MonitorFromRect(LPCRECT, DWORD) noexcept { return kMonitor; }
BOOL GetMonitorInfoW(HMONITOR, LPMONITORINFO info) noexcept {
    if (info == nullptr) return FALSE;
    info->rcMonitor = RECT{0, 0, kScreenWidth, kScreenHeight};
    info->rcWork = info->rcMonitor;
    info->dwFlags = MONITORINFOF_PRIMARY;
    if (info->cbSize >= sizeof(MONITORINFOEXW)) {
        auto* extended = static_cast<MONITORINFOEXW*>(info);
        std::wcscpy(extended->szDevice, L"\\\\.\\DISPLAY1");
    }
    return TRUE;
}
BOOL EnumDisplayMonitors(HDC dc, LPCRECT, MONITORENUMPROC callback, LPARAM parameter) noexcept {
    RECT screen{0, 0, kScreenWidth, kScreenHeight};
    if (callback != nullptr) callback(kMonitor, dc, &screen, parameter);
    return TRUE;
}
BOOL EnumDisplaySettingsW(LPCWSTR, DWORD mode, DEVMODEW* devmode) noexcept {
    if (devmode == nullptr || (mode != ENUM_CURRENT_SETTINGS && mode != ENUM_REGISTRY_SETTINGS && mode != 0)) return FALSE;
    devmode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    devmode->dmPelsWidth = kScreenWidth;
    devmode->dmPelsHeight = kScreenHeight;
    devmode->dmDisplayFrequency = 60;
    devmode->dmBitsPerPel = 32;
    return TRUE;
}
BOOL EnumDisplaySettingsExW(LPCWSTR device, DWORD mode, DEVMODEW* devmode, DWORD) noexcept { return EnumDisplaySettingsW(device, mode, devmode); }
BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT) noexcept { return TRUE; }
DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext() noexcept { return DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2; }
DPI_AWARENESS_CONTEXT SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT) noexcept { return DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2; }
BOOL AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT a, DPI_AWARENESS_CONTEXT b) noexcept { return a == b; }
BOOL SetProcessDPIAware() noexcept { return TRUE; }
UINT GetDpiForSystem() noexcept { return USER_DEFAULT_SCREEN_DPI; }
UINT GetDpiForWindow(HWND) noexcept { return USER_DEFAULT_SCREEN_DPI; }
int GetSystemMetricsForDpi(int index, UINT) noexcept { return GetSystemMetrics(index); }
HDC GetDC(HWND) noexcept { return nullptr; }
int ReleaseDC(HWND, HDC) noexcept { return 0; }
int GetDeviceCaps(HDC, int index) noexcept { return index == VREFRESH ? 60 : index == LOGPIXELSX || index == LOGPIXELSY ? USER_DEFAULT_SCREEN_DPI : 0; }
HGDIOBJ GetStockObject(int) noexcept { return nullptr; }
BOOL DeleteObject(HGDIOBJ) noexcept { return FALSE; }
HBRUSH CreateSolidBrush(COLORREF) noexcept { return nullptr; }
HDC BeginPaint(HWND, LPPAINTSTRUCT) noexcept { return nullptr; }
BOOL EndPaint(HWND, const PAINTSTRUCT*) noexcept { return FALSE; }
int FillRect(HDC, const RECT*, HBRUSH) noexcept { return 0; }
HDEVNOTIFY RegisterDeviceNotificationW(HANDLE, LPVOID, DWORD) noexcept { return reinterpret_cast<HDEVNOTIFY>(1); }
BOOL UnregisterDeviceNotification(HDEVNOTIFY) noexcept { return TRUE; }
HPOWERNOTIFY RegisterPowerSettingNotification(HANDLE, const GUID*, DWORD) noexcept { return reinterpret_cast<HPOWERNOTIFY>(1); }
BOOL UnregisterPowerSettingNotification(HPOWERNOTIFY) noexcept { return TRUE; }
HPOWERNOTIFY RegisterSuspendResumeNotification(HANDLE, DWORD) noexcept { return reinterpret_cast<HPOWERNOTIFY>(1); }
BOOL UnregisterSuspendResumeNotification(HPOWERNOTIFY) noexcept { return TRUE; }

// Super Mario Galaxy for the PlayStation 5 - windows, the cursor, the keyboard,
// monitors, the registry and the console window: the desktop the console does
// not have. Each call answers as Windows does when there is nothing there
// (no window, no focus, no key down, one 3840x2160 display), so Nebula's
// runtime takes its paths for a missing window and reads the pad through XInput.
// Included by windows.h.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// ---- messages -------------------------------------------------------------------
typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *PMSG, *LPMSG;
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL(CALLBACK* WNDENUMPROC)(HWND, LPARAM);
typedef BOOL(CALLBACK* MONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);

#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_ENABLE 0x000A
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SHOWWINDOW 0x0018
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_MOUSEACTIVATE 0x0021
#define WM_GETMINMAXINFO 0x0024
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_DISPLAYCHANGE 0x007E
#define WM_GETICON 0x007F
#define WM_SETICON 0x0080
#define WM_NCCREATE 0x0081
#define WM_NCDESTROY 0x0082
#define WM_NCCALCSIZE 0x0083
#define WM_NCHITTEST 0x0084
#define WM_NCACTIVATE 0x0086
#define WM_NCMOUSEMOVE 0x00A0
#define WM_INPUT 0x00FF
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_MOUSEHWHEEL 0x020E
#define WM_ENTERSIZEMOVE 0x0231
#define WM_EXITSIZEMOVE 0x0232
#define WM_DEVICECHANGE 0x0219
#define WM_CAPTURECHANGED 0x0215
#define WM_POWERBROADCAST 0x0218
#define WM_DPICHANGED 0x02E0
#define WM_MOUSELEAVE 0x02A3
#define WM_INPUT_DEVICE_CHANGE 0x00FE
#define WM_SETTINGCHANGE 0x001A
#define WM_USER 0x0400
#define WM_APP 0x8000
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002
#define SC_KEYMENU 0xF100
#define SC_SCREENSAVE 0xF140
#define SC_MONITORPOWER 0xF170
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2
#define SIZE_RESTORED 0
#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_MBUTTON 0x0010
#define HTCLIENT 1
#define MA_ACTIVATE 1
#define WHEEL_DELTA 120
#define GET_WHEEL_DELTA_WPARAM(w) ((short)HIWORD(w))
#define GET_KEYSTATE_WPARAM(w) (LOWORD(w))
#define PBT_APMSUSPEND 0x0004
#define PBT_APMRESUMEAUTOMATIC 0x0012
#define PBT_APMRESUMESUSPEND 0x0007
#define PBT_POWERSETTINGCHANGE 0x8013

BOOL PeekMessageW(LPMSG message, HWND window, UINT first, UINT last, UINT remove) noexcept;
BOOL GetMessageW(LPMSG message, HWND window, UINT first, UINT last) noexcept;
BOOL TranslateMessage(const MSG* message) noexcept;
LRESULT DispatchMessageW(const MSG* message) noexcept;
BOOL PostMessageW(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
LRESULT SendMessageW(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
BOOL PostThreadMessageW(DWORD thread, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
void PostQuitMessage(int code) noexcept;
LRESULT DefWindowProcW(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds,
                                DWORD wake_mask) noexcept;
DWORD MsgWaitForMultipleObjectsEx(DWORD count, const HANDLE* handles, DWORD milliseconds, DWORD wake_mask,
                                  DWORD flags) noexcept;
#define QS_ALLINPUT 0x04FF
#define QS_ALLEVENTS 0x04BF
#define MWMO_INPUTAVAILABLE 0x0004
#define MWMO_ALERTABLE 0x0002
#define MWMO_WAITALL 0x0001
#define PeekMessage PeekMessageW
#define DispatchMessage DispatchMessageW
#define PostMessage PostMessageW
#define DefWindowProc DefWindowProcW

// ---- windows --------------------------------------------------------------------
typedef struct tagWNDCLASSEXW {
    UINT cbSize;
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra;
    int cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCWSTR lpszMenuName;
    LPCWSTR lpszClassName;
    HICON hIconSm;
} WNDCLASSEXW, *PWNDCLASSEXW;
typedef WNDCLASSEXW WNDCLASSEX;
typedef struct tagCREATESTRUCTW {
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy;
    int cx;
    int y;
    int x;
    LONG style;
    LPCWSTR lpszName;
    LPCWSTR lpszClass;
    DWORD dwExStyle;
} CREATESTRUCTW, *LPCREATESTRUCTW;
typedef struct tagMINMAXINFO {
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;
typedef struct tagWINDOWPLACEMENT {
    UINT length;
    UINT flags;
    UINT showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT rcNormalPosition;
} WINDOWPLACEMENT;
typedef struct tagWINDOWPOS {
    HWND hwnd;
    HWND hwndInsertAfter;
    int x;
    int y;
    int cx;
    int cy;
    UINT flags;
} WINDOWPOS, *LPWINDOWPOS;
ATOM RegisterClassExW(const WNDCLASSEXW* window_class) noexcept;
BOOL UnregisterClassW(LPCWSTR name, HINSTANCE instance) noexcept;
HWND CreateWindowExW(DWORD ex_style, LPCWSTR class_name, LPCWSTR window_name, DWORD style, int x, int y, int width,
                     int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID parameter) noexcept;
BOOL DestroyWindow(HWND window) noexcept;
BOOL ShowWindow(HWND window, int command) noexcept;
BOOL UpdateWindow(HWND window) noexcept;
BOOL IsWindow(HWND window) noexcept;
BOOL IsWindowVisible(HWND window) noexcept;
BOOL IsIconic(HWND window) noexcept;
BOOL IsZoomed(HWND window) noexcept;
HWND GetForegroundWindow() noexcept;
BOOL SetForegroundWindow(HWND window) noexcept;
HWND GetActiveWindow() noexcept;
HWND SetActiveWindow(HWND window) noexcept;
HWND GetFocus() noexcept;
HWND SetFocus(HWND window) noexcept;
HWND GetDesktopWindow() noexcept;
HWND GetConsoleWindow() noexcept;
HWND GetAncestor(HWND window, UINT flags) noexcept;
HWND GetParent(HWND window) noexcept;
#define GA_PARENT 1
#define GA_ROOT 2
#define GA_ROOTOWNER 3
DWORD GetWindowThreadProcessId(HWND window, LPDWORD process_id) noexcept;
BOOL GetClientRect(HWND window, LPRECT rect) noexcept;
BOOL GetWindowRect(HWND window, LPRECT rect) noexcept;
BOOL ClientToScreen(HWND window, LPPOINT point) noexcept;
BOOL ScreenToClient(HWND window, LPPOINT point) noexcept;
BOOL SetWindowPos(HWND window, HWND after, int x, int y, int width, int height, UINT flags) noexcept;
BOOL MoveWindow(HWND window, int x, int y, int width, int height, BOOL repaint) noexcept;
BOOL AdjustWindowRect(LPRECT rect, DWORD style, BOOL menu) noexcept;
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD ex_style) noexcept;
BOOL AdjustWindowRectExForDpi(LPRECT rect, DWORD style, BOOL menu, DWORD ex_style, UINT dpi) noexcept;
LONG_PTR SetWindowLongPtrW(HWND window, int index, LONG_PTR value) noexcept;
LONG_PTR GetWindowLongPtrW(HWND window, int index) noexcept;
LONG SetWindowLongW(HWND window, int index, LONG value) noexcept;
LONG GetWindowLongW(HWND window, int index) noexcept;
BOOL SetWindowTextW(HWND window, LPCWSTR text) noexcept;
int GetWindowTextW(HWND window, LPWSTR text, int count) noexcept;
int GetClassNameW(HWND window, LPWSTR name, int count) noexcept;
BOOL EnumWindows(WNDENUMPROC callback, LPARAM parameter) noexcept;
BOOL EnumThreadWindows(DWORD thread, WNDENUMPROC callback, LPARAM parameter) noexcept;
BOOL GetWindowPlacement(HWND window, WINDOWPLACEMENT* placement) noexcept;
BOOL SetWindowPlacement(HWND window, const WINDOWPLACEMENT* placement) noexcept;
BOOL InvalidateRect(HWND window, const RECT* rect, BOOL erase) noexcept;
BOOL ValidateRect(HWND window, const RECT* rect) noexcept;
BOOL FlashWindow(HWND window, BOOL invert) noexcept;
BOOL BringWindowToTop(HWND window) noexcept;
BOOL SetPropW(HWND window, LPCWSTR name, HANDLE data) noexcept;
HANDLE GetPropW(HWND window, LPCWSTR name) noexcept;
HANDLE RemovePropW(HWND window, LPCWSTR name) noexcept;
HWND FindWindowW(LPCWSTR class_name, LPCWSTR window_name) noexcept;
HWND WindowFromPoint(POINT point) noexcept;
BOOL SetConsoleTitleW(LPCWSTR title) noexcept;
BOOL SetConsoleOutputCP(UINT code_page) noexcept;
BOOL SetConsoleCtrlHandler(BOOL(WINAPI* handler)(DWORD), BOOL add) noexcept;
BOOL AllocConsole() noexcept;
BOOL FreeConsole() noexcept;
BOOL AttachConsole(DWORD process) noexcept;
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
HANDLE GetStdHandle(DWORD handle) noexcept;
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
BOOL GetConsoleMode(HANDLE console, LPDWORD mode) noexcept;
BOOL SetConsoleMode(HANDLE console, DWORD mode) noexcept;
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define CTRL_C_EVENT 0
#define CTRL_BREAK_EVENT 1
#define CTRL_CLOSE_EVENT 2
int MessageBoxW(HWND window, LPCWSTR text, LPCWSTR caption, UINT type) noexcept;
int MessageBoxA(HWND window, LPCSTR text, LPCSTR caption, UINT type) noexcept;
#define MessageBox MessageBoxW
#define MB_OK 0x00000000L
#define MB_OKCANCEL 0x00000001L
#define MB_YESNO 0x00000004L
#define MB_ICONERROR 0x00000010L
#define MB_ICONWARNING 0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_TOPMOST 0x00040000L
#define MB_SETFOREGROUND 0x00010000L
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7
#define GWLP_WNDPROC (-4)
#define GWLP_HINSTANCE (-6)
#define GWLP_USERDATA (-21)
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_APPWINDOW 0x00040000L
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#define WS_EX_TOOLWINDOW 0x00000080L
#define WS_EX_LAYERED 0x00080000
#define WS_EX_TRANSPARENT 0x00000020L
#define WS_EX_NOACTIVATE 0x08000000L
#define CS_HREDRAW 0x0002
#define CS_VREDRAW 0x0001
#define CS_OWNDC 0x0020
#define CS_DBLCLKS 0x0008
#define CW_USEDEFAULT ((int)0x80000000)
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWNA 8
#define SW_RESTORE 9
#define SW_SHOWDEFAULT 10
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080
#define SWP_NOOWNERZORDER 0x0200
#define SWP_NOSENDCHANGING 0x0400
#define SWP_ASYNCWINDOWPOS 0x4000
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)-1)
#define HWND_NOTOPMOST ((HWND)-2)
#define HWND_MESSAGE ((HWND)-3)

// ---- the cursor, the mouse and the keyboard ------------------------------------------
BOOL GetCursorPos(LPPOINT point) noexcept;
BOOL SetCursorPos(int x, int y) noexcept;
HCURSOR SetCursor(HCURSOR cursor) noexcept;
HCURSOR GetCursor() noexcept;
int ShowCursor(BOOL show) noexcept;
BOOL ClipCursor(const RECT* rect) noexcept;
BOOL GetClipCursor(LPRECT rect) noexcept;
HCURSOR LoadCursorW(HINSTANCE instance, LPCWSTR name) noexcept;
HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name) noexcept;
HICON LoadIconW(HINSTANCE instance, LPCWSTR name) noexcept;
HANDLE LoadImageW(HINSTANCE instance, LPCWSTR name, UINT type, int width, int height, UINT flags) noexcept;
BOOL DestroyIcon(HICON icon) noexcept;
#define IDC_ARROW MAKEINTRESOURCEW(32512)
#define IDC_HAND MAKEINTRESOURCEW(32649)
#define IDI_APPLICATION MAKEINTRESOURCEW(32512)
#define IMAGE_ICON 1
#define IMAGE_CURSOR 2
#define LR_DEFAULTCOLOR 0x00000000
#define LR_DEFAULTSIZE 0x00000040
#define LR_SHARED 0x00008000
HWND GetCapture() noexcept;
HWND SetCapture(HWND window) noexcept;
BOOL ReleaseCapture() noexcept;
SHORT GetAsyncKeyState(int key) noexcept;
SHORT GetKeyState(int key) noexcept;
BOOL GetKeyboardState(PBYTE state) noexcept;
UINT MapVirtualKeyA(UINT code, UINT map_type) noexcept;
UINT MapVirtualKeyW(UINT code, UINT map_type) noexcept;
int GetKeyNameTextW(LONG lparam, LPWSTR text, int count) noexcept;
#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_CHAR 2
#define MAPVK_VSC_TO_VK_EX 3
#define MAPVK_VK_TO_VSC_EX 4
typedef struct tagTRACKMOUSEEVENT {
    DWORD cbSize;
    DWORD dwFlags;
    HWND hwndTrack;
    DWORD dwHoverTime;
} TRACKMOUSEEVENT, *LPTRACKMOUSEEVENT;
BOOL TrackMouseEvent(LPTRACKMOUSEEVENT event) noexcept;
#define TME_LEAVE 0x00000002
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_F17 0x80
#define VK_F18 0x81
#define VK_F19 0x82
#define VK_F20 0x83
#define VK_F21 0x84
#define VK_F22 0x85
#define VK_F23 0x86
#define VK_F24 0x87
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_102 0xE2
#define VK_GAMEPAD_A 0xC3

// Raw input
DECLARE_HANDLE(HRAWINPUT);
typedef struct tagRAWINPUTDEVICE {
    USHORT usUsagePage;
    USHORT usUsage;
    DWORD dwFlags;
    HWND hwndTarget;
} RAWINPUTDEVICE, *PRAWINPUTDEVICE;
typedef const RAWINPUTDEVICE* PCRAWINPUTDEVICE;
typedef struct tagRAWINPUTHEADER {
    DWORD dwType;
    DWORD dwSize;
    HANDLE hDevice;
    WPARAM wParam;
} RAWINPUTHEADER;
typedef struct tagRAWMOUSE {
    USHORT usFlags;
    union {
        ULONG ulButtons;
        struct {
            USHORT usButtonFlags;
            USHORT usButtonData;
        };
    };
    ULONG ulRawButtons;
    LONG lLastX;
    LONG lLastY;
    ULONG ulExtraInformation;
} RAWMOUSE;
typedef struct tagRAWKEYBOARD {
    USHORT MakeCode;
    USHORT Flags;
    USHORT Reserved;
    USHORT VKey;
    UINT Message;
    ULONG ExtraInformation;
} RAWKEYBOARD;
typedef struct tagRAWHID {
    DWORD dwSizeHid;
    DWORD dwCount;
    BYTE bRawData[1];
} RAWHID;
typedef struct tagRAWINPUT {
    RAWINPUTHEADER header;
    union {
        RAWMOUSE mouse;
        RAWKEYBOARD keyboard;
        RAWHID hid;
    } data;
} RAWINPUT, *PRAWINPUT;
BOOL RegisterRawInputDevices(PCRAWINPUTDEVICE devices, UINT count, UINT size) noexcept;
UINT GetRawInputData(HRAWINPUT input, UINT command, LPVOID data, PUINT size, UINT header_size) noexcept;
#define RID_INPUT 0x10000003
#define RIM_TYPEMOUSE 0
#define RIM_TYPEKEYBOARD 1
#define RIM_TYPEHID 2
#define RIDEV_INPUTSINK 0x00000100
#define RIDEV_REMOVE 0x00000001
#define RIDEV_DEVNOTIFY 0x00002000
#define MOUSE_MOVE_RELATIVE 0
#define MOUSE_MOVE_ABSOLUTE 1
#define RI_MOUSE_LEFT_BUTTON_DOWN 0x0001
#define RI_MOUSE_LEFT_BUTTON_UP 0x0002
#define RI_MOUSE_RIGHT_BUTTON_DOWN 0x0004
#define RI_MOUSE_RIGHT_BUTTON_UP 0x0008
#define RI_MOUSE_WHEEL 0x0400

// ---- monitors and DPI --------------------------------------------------------------
typedef struct tagMONITORINFO {
    DWORD cbSize;
    RECT rcMonitor;
    RECT rcWork;
    DWORD dwFlags;
} MONITORINFO, *LPMONITORINFO;
typedef struct tagMONITORINFOEXW : tagMONITORINFO {
    WCHAR szDevice[32];
} MONITORINFOEXW, *LPMONITORINFOEXW;
typedef MONITORINFOEXW MONITORINFOEX;
HMONITOR MonitorFromWindow(HWND window, DWORD flags) noexcept;
HMONITOR MonitorFromPoint(POINT point, DWORD flags) noexcept;
HMONITOR MonitorFromRect(LPCRECT rect, DWORD flags) noexcept;
BOOL GetMonitorInfoW(HMONITOR monitor, LPMONITORINFO info) noexcept;
#define GetMonitorInfo GetMonitorInfoW
BOOL EnumDisplayMonitors(HDC dc, LPCRECT clip, MONITORENUMPROC callback, LPARAM parameter) noexcept;
#define MONITOR_DEFAULTTONULL 0x00000000
#define MONITOR_DEFAULTTOPRIMARY 0x00000001
#define MONITOR_DEFAULTTONEAREST 0x00000002
#define MONITORINFOF_PRIMARY 0x00000001
typedef struct _devicemodeW {
    WCHAR dmDeviceName[32];
    WORD dmSpecVersion;
    WORD dmDriverVersion;
    WORD dmSize;
    WORD dmDriverExtra;
    DWORD dmFields;
    POINT dmPosition;
    DWORD dmDisplayOrientation;
    DWORD dmDisplayFixedOutput;
    short dmColor;
    short dmDuplex;
    short dmYResolution;
    short dmTTOption;
    short dmCollate;
    WCHAR dmFormName[32];
    WORD dmLogPixels;
    DWORD dmBitsPerPel;
    DWORD dmPelsWidth;
    DWORD dmPelsHeight;
    DWORD dmDisplayFlags;
    DWORD dmDisplayFrequency;
    DWORD dmICMMethod;
    DWORD dmICMIntent;
    DWORD dmMediaType;
    DWORD dmDitherType;
    DWORD dmReserved1;
    DWORD dmReserved2;
    DWORD dmPanningWidth;
    DWORD dmPanningHeight;
} DEVMODEW, *PDEVMODEW, *LPDEVMODEW;
typedef DEVMODEW DEVMODE;
BOOL EnumDisplaySettingsW(LPCWSTR device, DWORD mode, DEVMODEW* devmode) noexcept;
BOOL EnumDisplaySettingsExW(LPCWSTR device, DWORD mode, DEVMODEW* devmode, DWORD flags) noexcept;
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define ENUM_REGISTRY_SETTINGS ((DWORD)-2)
#define DM_PELSWIDTH 0x00080000L
#define DM_PELSHEIGHT 0x00100000L
#define DM_DISPLAYFREQUENCY 0x00400000L
#define DM_INTERLACED 0x00000002
BOOL SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT context) noexcept;
DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext() noexcept;
DPI_AWARENESS_CONTEXT SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context) noexcept;
BOOL AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT a, DPI_AWARENESS_CONTEXT b) noexcept;
BOOL SetProcessDPIAware() noexcept;
UINT GetDpiForSystem() noexcept;
UINT GetDpiForWindow(HWND window) noexcept;
int GetSystemMetricsForDpi(int index, UINT dpi) noexcept;
#define DPI_AWARENESS_CONTEXT_UNAWARE ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((DPI_AWARENESS_CONTEXT)-3)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#define USER_DEFAULT_SCREEN_DPI 96
HDC GetDC(HWND window) noexcept;
int ReleaseDC(HWND window, HDC dc) noexcept;
int GetDeviceCaps(HDC dc, int index) noexcept;
#define LOGPIXELSX 88
#define LOGPIXELSY 90
#define VREFRESH 116
HGDIOBJ GetStockObject(int object) noexcept;
#define BLACK_BRUSH 4
BOOL DeleteObject(HGDIOBJ object) noexcept;
HBRUSH CreateSolidBrush(COLORREF color) noexcept;
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
typedef struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *LPPAINTSTRUCT;
HDC BeginPaint(HWND window, LPPAINTSTRUCT paint) noexcept;
BOOL EndPaint(HWND window, const PAINTSTRUCT* paint) noexcept;
int FillRect(HDC dc, const RECT* rect, HBRUSH brush) noexcept;

// Device and power notifications
HDEVNOTIFY RegisterDeviceNotificationW(HANDLE recipient, LPVOID filter, DWORD flags) noexcept;
BOOL UnregisterDeviceNotification(HDEVNOTIFY handle) noexcept;
HPOWERNOTIFY RegisterPowerSettingNotification(HANDLE recipient, const GUID* setting, DWORD flags) noexcept;
BOOL UnregisterPowerSettingNotification(HPOWERNOTIFY handle) noexcept;
HPOWERNOTIFY RegisterSuspendResumeNotification(HANDLE recipient, DWORD flags) noexcept;
BOOL UnregisterSuspendResumeNotification(HPOWERNOTIFY handle) noexcept;
#define DEVICE_NOTIFY_WINDOW_HANDLE 0x00000000
#define DEVICE_NOTIFY_ALL_INTERFACE_CLASSES 0x00000004
#define DBT_DEVICEARRIVAL 0x8000
#define DBT_DEVICEREMOVECOMPLETE 0x8004
#define DBT_DEVNODES_CHANGED 0x0007
#define DBT_DEVTYP_DEVICEINTERFACE 0x00000005
typedef struct _DEV_BROADCAST_HDR {
    DWORD dbch_size;
    DWORD dbch_devicetype;
    DWORD dbch_reserved;
} DEV_BROADCAST_HDR, *PDEV_BROADCAST_HDR;
typedef struct _DEV_BROADCAST_DEVICEINTERFACE_W {
    DWORD dbcc_size;
    DWORD dbcc_devicetype;
    DWORD dbcc_reserved;
    GUID dbcc_classguid;
    wchar_t dbcc_name[1];
} DEV_BROADCAST_DEVICEINTERFACE_W, DEV_BROADCAST_DEVICEINTERFACE;

// ---- the registry ------------------------------------------------------------------
typedef ACCESS_MASK REGSAM;
typedef LONG LSTATUS;
#define HKEY_CLASSES_ROOT ((HKEY)(ULONG_PTR)((LONG)0x80000000))
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)((LONG)0x80000001))
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define KEY_QUERY_VALUE 0x0001
#define KEY_WOW64_64KEY 0x0100
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4
#define REG_MULTI_SZ 7
#define REG_QWORD 11
#define RRF_RT_REG_DWORD 0x00000010
#define RRF_RT_REG_SZ 0x00000002
#define RRF_RT_ANY 0x0000ffff
LSTATUS RegOpenKeyExW(HKEY key, LPCWSTR sub_key, DWORD options, REGSAM desired, HKEY* result) noexcept;
LSTATUS RegQueryValueExW(HKEY key, LPCWSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data,
                         LPDWORD size) noexcept;
LSTATUS RegGetValueW(HKEY key, LPCWSTR sub_key, LPCWSTR value, DWORD flags, LPDWORD type, PVOID data,
                     LPDWORD size) noexcept;
LSTATUS RegCloseKey(HKEY key) noexcept;
LSTATUS RegSetValueExW(HKEY key, LPCWSTR name, DWORD reserved, DWORD type, const BYTE* data, DWORD size) noexcept;
LSTATUS RegCreateKeyExW(HKEY key, LPCWSTR sub_key, DWORD reserved, LPWSTR class_name, DWORD options, REGSAM desired,
                        LPSECURITY_ATTRIBUTES attributes, HKEY* result, LPDWORD disposition) noexcept;

// ---- handles ------------------------------------------------------------------------
BOOL GetHandleInformation(HANDLE object, LPDWORD flags) noexcept;
BOOL SetHandleInformation(HANDLE object, DWORD mask, DWORD flags) noexcept;
#define HANDLE_FLAG_INHERIT 0x00000001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE 0x00000002
BOOL IsBadReadPtr(const void* pointer, UINT_PTR size) noexcept;
BOOL IsBadWritePtr(LPVOID pointer, UINT_PTR size) noexcept;

// ---- processor topology ---------------------------------------------------------------
typedef ULONG_PTR KAFFINITY;
typedef struct _GROUP_AFFINITY {
    KAFFINITY Mask;
    WORD Group;
    WORD Reserved[3];
} GROUP_AFFINITY, *PGROUP_AFFINITY;
BOOL GetThreadGroupAffinity(HANDLE thread, PGROUP_AFFINITY affinity) noexcept;
BOOL SetThreadGroupAffinity(HANDLE thread, const GROUP_AFFINITY* affinity, PGROUP_AFFINITY previous) noexcept;
WORD GetActiveProcessorGroupCount() noexcept;
DWORD GetActiveProcessorCount(WORD group) noexcept;
#define ALL_PROCESSOR_GROUPS 0xffff
typedef enum _LOGICAL_PROCESSOR_RELATIONSHIP {
    RelationProcessorCore,
    RelationNumaNode,
    RelationCache,
    RelationProcessorPackage,
    RelationGroup,
    RelationProcessorDie,
    RelationNumaNodeEx,
    RelationProcessorModule,
    RelationAll = 0xffff
} LOGICAL_PROCESSOR_RELATIONSHIP;
typedef enum _PROCESSOR_CACHE_TYPE { CacheUnified, CacheInstruction, CacheData, CacheTrace } PROCESSOR_CACHE_TYPE;
typedef struct _PROCESSOR_RELATIONSHIP {
    BYTE Flags;
    BYTE EfficiencyClass;
    BYTE Reserved[20];
    WORD GroupCount;
    GROUP_AFFINITY GroupMask[1];
} PROCESSOR_RELATIONSHIP;
typedef struct _CACHE_RELATIONSHIP {
    BYTE Level;
    BYTE Associativity;
    WORD LineSize;
    DWORD CacheSize;
    PROCESSOR_CACHE_TYPE Type;
    BYTE Reserved[18];
    WORD GroupCount;
    union {
        GROUP_AFFINITY GroupMask;
        GROUP_AFFINITY GroupMasks[1];
    };
} CACHE_RELATIONSHIP;
typedef struct _NUMA_NODE_RELATIONSHIP {
    DWORD NodeNumber;
    BYTE Reserved[18];
    WORD GroupCount;
    union {
        GROUP_AFFINITY GroupMask;
        GROUP_AFFINITY GroupMasks[1];
    };
} NUMA_NODE_RELATIONSHIP;
typedef struct _PROCESSOR_GROUP_INFO {
    BYTE MaximumProcessorCount;
    BYTE ActiveProcessorCount;
    BYTE Reserved[38];
    KAFFINITY ActiveProcessorMask;
} PROCESSOR_GROUP_INFO;
typedef struct _GROUP_RELATIONSHIP {
    WORD MaximumGroupCount;
    WORD ActiveGroupCount;
    BYTE Reserved[20];
    PROCESSOR_GROUP_INFO GroupInfo[1];
} GROUP_RELATIONSHIP;
typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX {
    LOGICAL_PROCESSOR_RELATIONSHIP Relationship;
    DWORD Size;
    union {
        PROCESSOR_RELATIONSHIP Processor;
        NUMA_NODE_RELATIONSHIP NumaNode;
        CACHE_RELATIONSHIP Cache;
        GROUP_RELATIONSHIP Group;
    };
} SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, *PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;
BOOL GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP relationship,
                                      PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer, PDWORD length) noexcept;

// ---- input sources, power requests and messages the renderer's window handles ------------
#define WM_CANCELMODE 0x001F
#define WM_ENTERMENULOOP 0x0211
#define WM_EXITMENULOOP 0x0212
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#define GET_XBUTTON_WPARAM(w) (HIWORD(w))
#define SMTO_NORMAL 0x0000
#define SMTO_BLOCK 0x0001
#define SMTO_ABORTIFHUNG 0x0002
LONG GetMessageTime() noexcept;
BOOL ShowWindowAsync(HWND window, int command) noexcept;
LRESULT SendMessageTimeoutW(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT flags, UINT timeout,
                            PDWORD_PTR result) noexcept;
HCURSOR LoadCursorA(HINSTANCE instance, LPCWSTR name) noexcept;
typedef enum tagINPUT_MESSAGE_DEVICE_TYPE {
    IMDT_UNAVAILABLE = 0, IMDT_KEYBOARD = 1, IMDT_MOUSE = 2, IMDT_TOUCH = 4, IMDT_PEN = 8, IMDT_TOUCHPAD = 16
} INPUT_MESSAGE_DEVICE_TYPE;
typedef enum tagINPUT_MESSAGE_ORIGIN_ID {
    IMO_UNAVAILABLE = 0, IMO_HARDWARE = 1, IMO_INJECTED = 2, IMO_SYSTEM = 4
} INPUT_MESSAGE_ORIGIN_ID;
typedef struct tagINPUT_MESSAGE_SOURCE {
    INPUT_MESSAGE_DEVICE_TYPE deviceType;
    INPUT_MESSAGE_ORIGIN_ID originId;
} INPUT_MESSAGE_SOURCE;
BOOL GetCurrentInputMessageSource(INPUT_MESSAGE_SOURCE* source) noexcept;
#define POWER_REQUEST_CONTEXT_VERSION 0
#define POWER_REQUEST_CONTEXT_SIMPLE_STRING 0x1
typedef struct _REASON_CONTEXT {
    ULONG Version;
    DWORD Flags;
    union {
        struct {
            HMODULE LocalizedReasonModule;
            ULONG LocalizedReasonId;
            ULONG ReasonStringCount;
            LPWSTR* ReasonStrings;
        } Detailed;
        LPWSTR SimpleReasonString;
    } Reason;
} REASON_CONTEXT, *PREASON_CONTEXT;
typedef enum _POWER_REQUEST_TYPE {
    PowerRequestDisplayRequired, PowerRequestSystemRequired, PowerRequestAwayModeRequired, PowerRequestExecutionRequired
} POWER_REQUEST_TYPE;
HANDLE PowerCreateRequest(PREASON_CONTEXT context) noexcept;
BOOL PowerSetRequest(HANDLE request, POWER_REQUEST_TYPE type) noexcept;
BOOL PowerClearRequest(HANDLE request, POWER_REQUEST_TYPE type) noexcept;

// ---- alternate data streams ----------------------------------------------------------------
typedef enum _STREAM_INFO_LEVELS { FindStreamInfoStandard, FindStreamInfoMaxInfoLevel } STREAM_INFO_LEVELS;
typedef struct _WIN32_FIND_STREAM_DATA {
    LARGE_INTEGER StreamSize;
    WCHAR cStreamName[MAX_PATH + 36];
} WIN32_FIND_STREAM_DATA, *PWIN32_FIND_STREAM_DATA;
HANDLE FindFirstStreamW(LPCWSTR name, STREAM_INFO_LEVELS level, LPVOID data, DWORD flags) noexcept;
HANDLE FindFirstStreamW(const char* name, STREAM_INFO_LEVELS level, LPVOID data, DWORD flags) noexcept;
BOOL FindNextStreamW(HANDLE find, LPVOID data) noexcept;

// ---- the shell -----------------------------------------------------------------------
HINSTANCE ShellExecuteW(HWND window, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory,
                        INT show) noexcept;
HINSTANCE ShellExecuteW(HWND window, LPCWSTR operation, const char* file, LPCWSTR parameters, const char* directory,
                        INT show) noexcept;

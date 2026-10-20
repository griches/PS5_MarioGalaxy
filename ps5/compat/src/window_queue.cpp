// Super Mario Galaxy for the PlayStation 5 - windows and their message queues.
//
// Nebula's renderer owns a Win32 window on a thread of its own (window_owner.cpp)
// and talks to it with posted and sent messages. Here a window is an object
// with its class's procedure and the creating thread's queue; it always covers
// the console's 3840x2160 output, is visible, active and has the focus. Nothing
// draws into it: the swap chain presents to the display (ps5/gx/dxgi_ps5.cpp).
//
// GetMessageW/PeekMessageW run the messages other threads send before they
// return posted ones, as Windows does, so a sender waits for the window thread.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <condition_variable>
#include <cwchar>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr LONG kWidth = 3840;
constexpr LONG kHeight = 2160;

struct Sent {
    MSG message{};
    LRESULT result = 0;
    bool done = false;
};

struct Queue {
    std::mutex lock;
    std::condition_variable changed;
    std::deque<MSG> posted;
    std::deque<std::shared_ptr<Sent>> sent;
    bool quit = false;
    int quit_code = 0;
};

struct Window {
    WNDPROC procedure = nullptr;
    DWORD thread = 0;
    LONG_PTR user_data = 0;
    LONG_PTR style = 0;
    LONG_PTR ex_style = 0;
    bool shown = false;
    std::map<std::wstring, HANDLE> properties;
};

struct State {
    std::mutex lock;
    std::unordered_map<std::wstring, WNDPROC> classes;
    std::unordered_set<Window*> windows;
    std::unordered_map<DWORD, std::shared_ptr<Queue>> queues;
    HWND focus = nullptr;
};

State& state() noexcept {
    static State instance;
    return instance;
}

std::shared_ptr<Queue> queue_of(DWORD thread) {
    State& s = state();
    std::lock_guard lock(s.lock);
    std::shared_ptr<Queue>& queue = s.queues[thread];
    if (queue == nullptr) queue = std::make_shared<Queue>();
    return queue;
}

Window* window_of(HWND window) noexcept {
    State& s = state();
    std::lock_guard lock(s.lock);
    Window* const object = reinterpret_cast<Window*>(window);
    return s.windows.count(object) != 0 ? object : nullptr;
}

LRESULT call(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    Window* const object = window_of(window);
    if (object == nullptr || object->procedure == nullptr) return 0;
    return object->procedure(window, message, wparam, lparam);
}

// Runs every message sent to this thread. Called with the queue unlocked.
void run_sent(Queue& queue) {
    for (;;) {
        std::shared_ptr<Sent> sent;
        {
            std::lock_guard lock(queue.lock);
            if (queue.sent.empty()) return;
            sent = queue.sent.front();
            queue.sent.pop_front();
        }
        const LRESULT result = call(sent->message.hwnd, sent->message.message, sent->message.wParam,
                                    sent->message.lParam);
        std::lock_guard lock(queue.lock);
        sent->result = result;
        sent->done = true;
        queue.changed.notify_all();
    }
}

bool matches(const MSG& message, HWND window, UINT first, UINT last) noexcept {
    if (window != nullptr && message.hwnd != window) return false;
    if (first == 0 && last == 0) return true;
    return message.message >= first && message.message <= last;
}

// Takes the first posted message that matches, or WM_QUIT once posted messages run out.
bool take(Queue& queue, LPMSG out, HWND window, UINT first, UINT last, bool remove) {
    for (auto it = queue.posted.begin(); it != queue.posted.end(); ++it) {
        if (!matches(*it, window, first, last)) continue;
        if (out != nullptr) *out = *it;
        if (remove) queue.posted.erase(it);
        return true;
    }
    if (queue.quit) {
        if (out != nullptr) *out = MSG{nullptr, WM_QUIT, static_cast<WPARAM>(queue.quit_code), 0, 0, {}};
        if (remove) queue.quit = false;
        return true;
    }
    return false;
}

bool post(DWORD thread, HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    const std::shared_ptr<Queue> queue = queue_of(thread);
    std::lock_guard lock(queue->lock);
    queue->posted.push_back(MSG{window, message, wparam, lparam, static_cast<DWORD>(GetTickCount()), {}});
    queue->changed.notify_all();
    return true;
}
}  // namespace

ATOM RegisterClassExW(const WNDCLASSEXW* window_class) noexcept {
    if (window_class == nullptr || window_class->lpszClassName == nullptr) return 0;
    State& s = state();
    std::lock_guard lock(s.lock);
    s.classes[window_class->lpszClassName] = window_class->lpfnWndProc;
    return 1;
}

BOOL UnregisterClassW(LPCWSTR name, HINSTANCE) noexcept {
    if (name == nullptr) return FALSE;
    State& s = state();
    std::lock_guard lock(s.lock);
    return s.classes.erase(name) != 0;
}

HWND CreateWindowExW(DWORD ex_style, LPCWSTR class_name, LPCWSTR title, DWORD style, int x, int y, int width,
                     int height, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param) noexcept {
    auto* object = new Window;
    object->thread = GetCurrentThreadId();
    object->style = style;
    object->ex_style = ex_style;
    {
        State& s = state();
        std::lock_guard lock(s.lock);
        const auto found = class_name != nullptr ? s.classes.find(class_name) : s.classes.end();
        object->procedure = found != s.classes.end() ? found->second : nullptr;
        s.windows.insert(object);
        s.focus = reinterpret_cast<HWND>(object);
    }
    (void)queue_of(object->thread);
    HWND const window = reinterpret_cast<HWND>(object);
    CREATESTRUCTW create{param, instance, menu, parent, height, width, y, x, static_cast<LONG>(style), title,
                         class_name, ex_style};
    (void)call(window, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&create));
    if (call(window, WM_CREATE, 0, reinterpret_cast<LPARAM>(&create)) == -1) {
        DestroyWindow(window);
        return nullptr;
    }
    (void)call(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(kWidth, kHeight));
    return window;
}

BOOL DestroyWindow(HWND window) noexcept {
    if (window_of(window) == nullptr) return FALSE;
    (void)call(window, WM_DESTROY, 0, 0);
    (void)call(window, WM_NCDESTROY, 0, 0);
    State& s = state();
    std::lock_guard lock(s.lock);
    Window* const object = reinterpret_cast<Window*>(window);
    s.windows.erase(object);
    if (s.focus == window) s.focus = nullptr;
    delete object;
    return TRUE;
}

BOOL IsWindow(HWND window) noexcept { return window_of(window) != nullptr; }
BOOL IsWindowVisible(HWND window) noexcept { return window_of(window) != nullptr; }
// The first show activates the window and gives it the focus, as Windows does.
BOOL ShowWindow(HWND window, int) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr) return FALSE;
    if (object->shown) return TRUE;
    object->shown = true;
    (void)call(window, WM_SHOWWINDOW, TRUE, 0);
    (void)call(window, WM_ACTIVATEAPP, TRUE, 0);
    (void)call(window, WM_ACTIVATE, WA_ACTIVE, 0);
    (void)call(window, WM_SETFOCUS, 0, 0);
    return FALSE;
}

HWND GetForegroundWindow() noexcept {
    State& s = state();
    std::lock_guard lock(s.lock);
    return s.focus;
}
HWND GetActiveWindow() noexcept { return GetForegroundWindow(); }
HWND GetFocus() noexcept { return GetForegroundWindow(); }

DWORD GetWindowThreadProcessId(HWND window, LPDWORD process_id) noexcept {
    if (process_id != nullptr) *process_id = GetCurrentProcessId();
    Window* const object = window_of(window);
    return object != nullptr ? object->thread : 0;
}

BOOL GetClientRect(HWND window, LPRECT rect) noexcept {
    if (rect == nullptr || window_of(window) == nullptr) return FALSE;
    *rect = RECT{0, 0, kWidth, kHeight};
    return TRUE;
}
BOOL GetWindowRect(HWND window, LPRECT rect) noexcept { return GetClientRect(window, rect); }
BOOL ClientToScreen(HWND window, LPPOINT) noexcept { return window_of(window) != nullptr; }
BOOL ScreenToClient(HWND window, LPPOINT) noexcept { return window_of(window) != nullptr; }

LONG_PTR SetWindowLongPtrW(HWND window, int index, LONG_PTR value) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr) return 0;
    LONG_PTR* slot = index == GWLP_USERDATA ? &object->user_data
                     : index == GWL_STYLE   ? &object->style
                     : index == GWL_EXSTYLE ? &object->ex_style
                                            : nullptr;
    if (slot == nullptr) return 0;
    const LONG_PTR previous = *slot;
    *slot = value;
    return previous;
}

LONG_PTR GetWindowLongPtrW(HWND window, int index) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr) return 0;
    return index == GWLP_USERDATA ? object->user_data
           : index == GWL_STYLE   ? object->style
           : index == GWL_EXSTYLE ? object->ex_style
                                  : 0;
}

BOOL SetPropW(HWND window, LPCWSTR name, HANDLE value) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr || name == nullptr) return FALSE;
    object->properties[name] = value;
    return TRUE;
}
HANDLE GetPropW(HWND window, LPCWSTR name) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr || name == nullptr) return nullptr;
    const auto found = object->properties.find(name);
    return found != object->properties.end() ? found->second : nullptr;
}
HANDLE RemovePropW(HWND window, LPCWSTR name) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr || name == nullptr) return nullptr;
    const auto found = object->properties.find(name);
    if (found == object->properties.end()) return nullptr;
    HANDLE const value = found->second;
    object->properties.erase(found);
    return value;
}

LRESULT DefWindowProcW(HWND window, UINT message, WPARAM, LPARAM) noexcept {
    if (message == WM_CLOSE) DestroyWindow(window);
    return message == WM_NCCREATE ? TRUE : 0;
}

BOOL PostMessageW(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr) return FALSE;
    return post(object->thread, window, message, wparam, lparam);
}

BOOL PostThreadMessageW(DWORD thread, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    return post(thread, nullptr, message, wparam, lparam);
}

void PostQuitMessage(int code) noexcept {
    const std::shared_ptr<Queue> queue = queue_of(GetCurrentThreadId());
    std::lock_guard lock(queue->lock);
    queue->quit = true;
    queue->quit_code = code;
    queue->changed.notify_all();
}

LRESULT SendMessageW(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
    Window* const object = window_of(window);
    if (object == nullptr) return 0;
    if (object->thread == GetCurrentThreadId()) return call(window, message, wparam, lparam);
    auto sent = std::make_shared<Sent>();
    sent->message = MSG{window, message, wparam, lparam, 0, {}};
    const std::shared_ptr<Queue> queue = queue_of(object->thread);
    std::unique_lock lock(queue->lock);
    queue->sent.push_back(sent);
    queue->changed.notify_all();
    queue->changed.wait(lock, [&] { return sent->done; });
    return sent->result;
}

BOOL PeekMessageW(LPMSG message, HWND window, UINT first, UINT last, UINT flags) noexcept {
    const std::shared_ptr<Queue> queue = queue_of(GetCurrentThreadId());
    run_sent(*queue);
    std::lock_guard lock(queue->lock);
    return take(*queue, message, window, first, last, (flags & PM_REMOVE) != 0);
}

BOOL GetMessageW(LPMSG message, HWND window, UINT first, UINT last) noexcept {
    const std::shared_ptr<Queue> queue = queue_of(GetCurrentThreadId());
    for (;;) {
        run_sent(*queue);
        std::unique_lock lock(queue->lock);
        if (take(*queue, message, window, first, last, true)) {
            return message != nullptr && message->message == WM_QUIT ? FALSE : TRUE;
        }
        queue->changed.wait(lock, [&] { return !queue->sent.empty() || !queue->posted.empty() || queue->quit; });
    }
}

BOOL TranslateMessage(const MSG*) noexcept { return FALSE; }

LRESULT DispatchMessageW(const MSG* message) noexcept {
    if (message == nullptr || message->hwnd == nullptr) return 0;
    return call(message->hwnd, message->message, message->wParam, message->lParam);
}

// Waits for the handles or for a message to this thread (polled in short steps:
// the queue has no kernel object to wait on alongside the handles).
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds,
                                DWORD) noexcept {
    const std::shared_ptr<Queue> queue = queue_of(GetCurrentThreadId());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        run_sent(*queue);
        {
            std::lock_guard lock(queue->lock);
            if (!queue->posted.empty() || queue->quit) return WAIT_OBJECT_0 + count;
        }
        if (count != 0) {
            const DWORD result = WaitForMultipleObjects(count, handles, wait_all, 0);
            if (result != WAIT_TIMEOUT) return result;
        }
        if (milliseconds != INFINITE && std::chrono::steady_clock::now() >= deadline) return WAIT_TIMEOUT;
        std::unique_lock lock(queue->lock);
        queue->changed.wait_for(lock, std::chrono::milliseconds(1));
    }
}

DWORD MsgWaitForMultipleObjectsEx(DWORD count, const HANDLE* handles, DWORD milliseconds, DWORD wake_mask,
                                  DWORD flags) noexcept {
    return MsgWaitForMultipleObjects(count, handles, (flags & MWMO_WAITALL) != 0, milliseconds, wake_mask);
}

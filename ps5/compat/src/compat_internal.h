// Super Mario Galaxy for the PlayStation 5 - what the Windows compatibility layer's
// own files share: the kernel object every HANDLE points at, the wait lock, and
// the string and path conversions.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <ps5_compat.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

namespace compat {

enum class Kind { Event, Mutex, Semaphore, Timer, Thread, File, Mapping, Find, Snapshot, Other };

// A HANDLE is a pointer to one of these. ready() and acquire() run under wait_lock().
struct Object {
    explicit Object(Kind object_kind) noexcept : kind(object_kind) {}
    virtual ~Object() = default;
    virtual bool ready() const noexcept { return true; }
    virtual void acquire() noexcept {}
    // When a waiter must look again even if nobody signals (a timer's due time).
    virtual std::chrono::steady_clock::time_point wake_time() const noexcept {
        return std::chrono::steady_clock::time_point::max();
    }
    virtual void close() noexcept { delete this; }
    Kind kind;
    bool closed = false;
};

std::mutex& wait_lock() noexcept;
void notify_waiters() noexcept;
DWORD wait_objects(Object* const* objects, DWORD count, bool wait_all, DWORD milliseconds) noexcept;
DWORD current_thread_id() noexcept;
void set_thread_name(const std::string& name) noexcept;
std::string thread_name() noexcept;

// UTF-32 to UTF-8 and back.
std::string utf8(const wchar_t* text);
std::string utf8(std::wstring_view text);
std::wstring wide(std::string_view text);
// A Windows path as the console's: UTF-8, '/' for '\', a drive letter dropped.
std::string host_path(const wchar_t* path);
std::string host_path(const char* path);
// errno to the Windows error it means.
DWORD error_from_errno(int error) noexcept;
// Copies a string into a W buffer as Windows' GetXxxW does: the length without
// the terminator on success, the size needed with it when the buffer is short.
DWORD copy_out(std::wstring_view text, LPWSTR buffer, DWORD size) noexcept;
// A malloc'd copy (the console's libc has no wcsdup).
wchar_t* wide_duplicate(std::wstring_view text) noexcept;

}  // namespace compat

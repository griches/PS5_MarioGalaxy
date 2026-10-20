// Super Mario Galaxy for the PlayStation 5 - kernel objects: events, mutexes,
// semaphores, waitable timers and threads, as Windows HANDLEs, and the waits on
// them. Every waitable object signals through one process-wide condition
// variable, so WaitForMultipleObjects waits on any mix of them exactly.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace compat {

namespace {
thread_local DWORD t_last_error = 0;

// All waiting goes through this one lock and condition variable: an object's
// state changes under it and notify_all wakes every waiter to look again.
std::mutex g_wait_lock;
std::condition_variable g_wait_changed;

std::chrono::steady_clock::time_point deadline_after(DWORD milliseconds) {
    return std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
}
}  // namespace

std::mutex& wait_lock() noexcept { return g_wait_lock; }
void notify_waiters() noexcept { g_wait_changed.notify_all(); }

struct Event final : Object {
    Event(bool manual, bool initial) : Object(Kind::Event), manual_reset(manual), signaled(initial) {}
    bool manual_reset;
    bool signaled;
    bool ready() const noexcept override { return signaled; }
    void acquire() noexcept override {
        if (!manual_reset) signaled = false;
    }
};

struct Mutex final : Object {
    Mutex() : Object(Kind::Mutex) {}
    DWORD owner = 0;
    unsigned count = 0;
    std::string name;
    bool ready() const noexcept override { return count == 0 || owner == GetCurrentThreadId(); }
    void acquire() noexcept override {
        owner = GetCurrentThreadId();
        ++count;
    }
};

struct Semaphore final : Object {
    Semaphore(LONG initial, LONG maximum) : Object(Kind::Semaphore), count(initial), maximum(maximum) {}
    LONG count;
    LONG maximum;
    bool ready() const noexcept override { return count > 0; }
    void acquire() noexcept override { --count; }
};

// A waitable timer is signaled once its due time passes; the waiters'
// timeouts are clipped to that time, so no thread has to fire it.
struct Timer final : Object {
    explicit Timer(bool manual) : Object(Kind::Timer), manual_reset(manual) {}
    bool manual_reset;
    bool armed = false;
    bool signaled = false;
    std::chrono::steady_clock::time_point due{};
    std::chrono::nanoseconds period{};
    bool ready() const noexcept override {
        return signaled || (armed && std::chrono::steady_clock::now() >= due);
    }
    void acquire() noexcept override {
        if (armed && std::chrono::steady_clock::now() >= due) {
            signaled = true;
            if (period.count() > 0) {
                due += period;
            } else {
                armed = false;
            }
        }
        if (!manual_reset) signaled = false;
    }
    std::chrono::steady_clock::time_point wake_time() const noexcept override {
        return armed ? due : std::chrono::steady_clock::time_point::max();
    }
};

struct Thread final : Object {
    Thread() : Object(Kind::Thread) {}
    pthread_t thread{};
    DWORD id = 0;
    bool finished = false;
    DWORD exit_code = STILL_ACTIVE;
    bool joined_or_detached = false;
    bool ready() const noexcept override { return finished; }
    void acquire() noexcept override {}
};

namespace {
// Pseudo handles, as Windows has them
HANDLE const kCurrentProcess = reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1));
HANDLE const kCurrentThread = reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-2));

std::atomic<DWORD> g_next_thread_id{1000};
thread_local DWORD t_thread_id = 0;

std::mutex g_named_lock;
std::unordered_map<std::string, Mutex*> g_named_mutexes;

Object* object_of(HANDLE handle) noexcept {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE || handle == kCurrentThread) return nullptr;
    return static_cast<Object*>(handle);
}
}  // namespace

DWORD current_thread_id() noexcept {
    if (t_thread_id == 0) t_thread_id = g_next_thread_id.fetch_add(1, std::memory_order_relaxed);
    return t_thread_id;
}

// Waits until the objects are ready (all of them, or any one), acquiring them.
// Returns the index of the one acquired (0 for all), or WAIT_TIMEOUT.
DWORD wait_objects(Object* const* objects, DWORD count, bool wait_all, DWORD milliseconds) noexcept {
    std::unique_lock lock(g_wait_lock);
    const bool forever = milliseconds == INFINITE;
    const auto deadline = forever ? std::chrono::steady_clock::time_point::max() : deadline_after(milliseconds);
    for (;;) {
        if (wait_all) {
            bool all = true;
            for (DWORD i = 0; i < count; ++i) all = all && objects[i]->ready();
            if (all) {
                for (DWORD i = 0; i < count; ++i) objects[i]->acquire();
                return WAIT_OBJECT_0;
            }
        } else {
            for (DWORD i = 0; i < count; ++i) {
                if (objects[i]->ready()) {
                    objects[i]->acquire();
                    return WAIT_OBJECT_0 + i;
                }
            }
        }
        auto wake = deadline;
        for (DWORD i = 0; i < count; ++i) wake = std::min(wake, objects[i]->wake_time());
        if (!forever && std::chrono::steady_clock::now() >= deadline) return WAIT_TIMEOUT;
        if (wake == std::chrono::steady_clock::time_point::max()) {
            g_wait_changed.wait(lock);
        } else {
            g_wait_changed.wait_until(lock, wake);
        }
    }
}

}  // namespace compat

using namespace compat;

DWORD GetLastError() noexcept { return t_last_error; }
void SetLastError(DWORD error) noexcept { t_last_error = error; }

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr) {
        SetLastError(ERROR_INVALID_HANDLE);
        return WAIT_FAILED;
    }
    if (object->kind == Kind::File) return WAIT_OBJECT_0;
    return wait_objects(&object, 1, false, milliseconds);
}

DWORD WaitForSingleObjectEx(HANDLE handle, DWORD milliseconds, BOOL) noexcept {
    return WaitForSingleObject(handle, milliseconds);
}

DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds) noexcept {
    if (count == 0 || count > MAXIMUM_WAIT_OBJECTS) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return WAIT_FAILED;
    }
    Object* objects[MAXIMUM_WAIT_OBJECTS];
    for (DWORD i = 0; i < count; ++i) {
        objects[i] = object_of(handles[i]);
        if (objects[i] == nullptr) {
            SetLastError(ERROR_INVALID_HANDLE);
            return WAIT_FAILED;
        }
    }
    return wait_objects(objects, count, wait_all != FALSE, milliseconds);
}

DWORD WaitForMultipleObjectsEx(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds,
                               BOOL) noexcept {
    return WaitForMultipleObjects(count, handles, wait_all, milliseconds);
}

BOOL CloseHandle(HANDLE handle) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr) {
        if (handle == kCurrentThread || handle == kCurrentProcess) return TRUE;
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (object->kind == Kind::Thread) {
        auto* thread = static_cast<Thread*>(object);
        std::unique_lock lock(wait_lock());
        if (!thread->joined_or_detached) {
            thread->joined_or_detached = true;
            if (thread->finished) {
                lock.unlock();
                pthread_join(thread->thread, nullptr);
                delete thread;
                return TRUE;
            }
            pthread_detach(thread->thread);
        }
        // The thread deletes itself when it ends (thread_main).
        thread->closed = true;
        return TRUE;
    }
    if (object->kind == Kind::Mutex) {
        auto* mutex = static_cast<Mutex*>(object);
        if (!mutex->name.empty()) {
            std::lock_guard named(g_named_lock);
            g_named_mutexes.erase(mutex->name);
        }
    }
    object->close();
    return TRUE;
}

BOOL DuplicateHandle(HANDLE, HANDLE source, HANDLE, LPHANDLE target, DWORD, BOOL, DWORD) noexcept {
    // Only the current thread's pseudo handle is duplicated (to name it from
    // another thread): the real thread object for it.
    if (target == nullptr) return FALSE;
    if (source == kCurrentThread) {
        *target = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
        return *target != nullptr;
    }
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

// ---- events ---------------------------------------------------------------------
HANDLE CreateEventW(LPSECURITY_ATTRIBUTES, BOOL manual_reset, BOOL initial_state, LPCWSTR) noexcept {
    return new Event(manual_reset != FALSE, initial_state != FALSE);
}
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES attributes, BOOL manual_reset, BOOL initial_state, LPCSTR) noexcept {
    return CreateEventW(attributes, manual_reset, initial_state, nullptr);
}
HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES attributes, LPCWSTR, DWORD flags, DWORD) noexcept {
    return CreateEventW(attributes, (flags & CREATE_EVENT_MANUAL_RESET) != 0, (flags & CREATE_EVENT_INITIAL_SET) != 0,
                        nullptr);
}
BOOL SetEvent(HANDLE handle) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Event) return FALSE;
    {
        std::lock_guard lock(wait_lock());
        static_cast<Event*>(object)->signaled = true;
    }
    notify_waiters();
    return TRUE;
}
BOOL ResetEvent(HANDLE handle) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Event) return FALSE;
    std::lock_guard lock(wait_lock());
    static_cast<Event*>(object)->signaled = false;
    return TRUE;
}

// ---- mutexes ----------------------------------------------------------------------
HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES, BOOL initial_owner, LPCWSTR name) noexcept {
    std::string key = name != nullptr ? utf8(name) : std::string{};
    if (!key.empty()) {
        std::lock_guard named(g_named_lock);
        if (const auto it = g_named_mutexes.find(key); it != g_named_mutexes.end()) {
            SetLastError(ERROR_ALREADY_EXISTS);
            return it->second;
        }
    }
    auto* mutex = new Mutex();
    mutex->name = key;
    if (initial_owner) {
        std::lock_guard lock(wait_lock());
        mutex->acquire();
    }
    if (!key.empty()) {
        std::lock_guard named(g_named_lock);
        g_named_mutexes.emplace(key, mutex);
    }
    SetLastError(ERROR_SUCCESS);
    return mutex;
}
HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES attributes, LPCWSTR name, DWORD flags, DWORD) noexcept {
    return CreateMutexW(attributes, (flags & CREATE_MUTEX_INITIAL_OWNER) != 0, name);
}
BOOL ReleaseMutex(HANDLE handle) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Mutex) return FALSE;
    auto* mutex = static_cast<Mutex*>(object);
    {
        std::lock_guard lock(wait_lock());
        if (mutex->count == 0 || mutex->owner != GetCurrentThreadId()) return FALSE;
        if (--mutex->count == 0) mutex->owner = 0;
    }
    notify_waiters();
    return TRUE;
}

// ---- semaphores -------------------------------------------------------------------
HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES, LONG initial, LONG maximum, LPCWSTR) noexcept {
    return new Semaphore(initial, maximum);
}
HANDLE CreateSemaphoreExW(LPSECURITY_ATTRIBUTES attributes, LONG initial, LONG maximum, LPCWSTR name, DWORD,
                          DWORD) noexcept {
    return CreateSemaphoreW(attributes, initial, maximum, name);
}
BOOL ReleaseSemaphore(HANDLE handle, LONG count, LPLONG previous) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Semaphore) return FALSE;
    auto* semaphore = static_cast<Semaphore*>(object);
    {
        std::lock_guard lock(wait_lock());
        if (previous != nullptr) *previous = semaphore->count;
        if (semaphore->count + count > semaphore->maximum) return FALSE;
        semaphore->count += count;
    }
    notify_waiters();
    return TRUE;
}

// ---- waitable timers ----------------------------------------------------------------
HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES, BOOL manual_reset, LPCWSTR) noexcept {
    return new Timer(manual_reset != FALSE);
}
HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD flags, DWORD) noexcept {
    return new Timer((flags & CREATE_WAITABLE_TIMER_MANUAL_RESET) != 0);
}
BOOL SetWaitableTimer(HANDLE handle, const LARGE_INTEGER* due, LONG period, PTIMERAPCROUTINE, LPVOID,
                      BOOL) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Timer || due == nullptr) return FALSE;
    auto* timer = static_cast<Timer*>(object);
    const auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point when;
    if (due->QuadPart < 0) {
        // Relative, in 100 ns units
        when = now + std::chrono::nanoseconds(-due->QuadPart * 100);
    } else {
        // Absolute FILETIME: from the system clock to the steady one
        const std::int64_t unix_100ns = due->QuadPart - 116444736000000000LL;
        const auto system_due = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(unix_100ns * 100)));
        when = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(system_due -
                                                                                     std::chrono::system_clock::now());
    }
    {
        std::lock_guard lock(wait_lock());
        timer->armed = true;
        timer->signaled = false;
        timer->due = when;
        timer->period = std::chrono::milliseconds(period);
    }
    notify_waiters();
    return TRUE;
}
BOOL SetWaitableTimerEx(HANDLE timer, const LARGE_INTEGER* due, LONG period, PTIMERAPCROUTINE routine,
                        LPVOID argument, void*, ULONG) noexcept {
    return SetWaitableTimer(timer, due, period, routine, argument, FALSE);
}
BOOL CancelWaitableTimer(HANDLE handle) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Timer) return FALSE;
    std::lock_guard lock(wait_lock());
    static_cast<Timer*>(object)->armed = false;
    return TRUE;
}

// ---- threads ------------------------------------------------------------------------
namespace {
struct ThreadStart {
    Thread* thread;
    LPTHREAD_START_ROUTINE start;
    LPVOID parameter;
    DWORD id;
};

void* thread_main(void* argument) {
    auto* start = static_cast<ThreadStart*>(argument);
    t_thread_id = start->id;
    Thread* const thread = start->thread;
    const DWORD code = start->start(start->parameter);
    delete start;
    bool free_now = false;
    {
        std::lock_guard lock(wait_lock());
        thread->finished = true;
        thread->exit_code = code;
        free_now = thread->closed;
    }
    notify_waiters();
    if (free_now) delete thread;
    return nullptr;
}
}  // namespace

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stack_size, LPTHREAD_START_ROUTINE start, LPVOID parameter,
                    DWORD, LPDWORD thread_id) noexcept {
    auto* thread = new Thread();
    thread->id = g_next_thread_id.fetch_add(1, std::memory_order_relaxed);
    auto* info = new ThreadStart{thread, start, parameter, thread->id};
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, std::max<SIZE_T>(stack_size, 1u << 20));
    const int result = pthread_create(&thread->thread, &attributes, thread_main, info);
    pthread_attr_destroy(&attributes);
    if (result != 0) {
        delete info;
        delete thread;
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    if (thread_id != nullptr) *thread_id = thread->id;
    return thread;
}

DWORD ResumeThread(HANDLE) noexcept { return 0; }
DWORD SuspendThread(HANDLE) noexcept {
    SetLastError(ERROR_NOT_SUPPORTED);
    return static_cast<DWORD>(-1);
}
BOOL GetExitCodeThread(HANDLE handle, LPDWORD code) noexcept {
    Object* object = object_of(handle);
    if (object == nullptr || object->kind != Kind::Thread || code == nullptr) return FALSE;
    std::lock_guard lock(wait_lock());
    *code = static_cast<Thread*>(object)->exit_code;
    return TRUE;
}
HANDLE GetCurrentThread() noexcept { return kCurrentThread; }
DWORD GetCurrentThreadId() noexcept { return current_thread_id(); }
DWORD GetThreadId(HANDLE handle) noexcept {
    if (handle == kCurrentThread) return GetCurrentThreadId();
    Object* object = object_of(handle);
    return object != nullptr && object->kind == Kind::Thread ? static_cast<Thread*>(object)->id : 0;
}
// A handle that names a thread by id, for the diagnostics that query one; it
// never finishes (nothing waits on it).
HANDLE OpenThread(DWORD, BOOL, DWORD thread_id) noexcept {
    auto* thread = new Thread();
    thread->id = thread_id;
    thread->joined_or_detached = true;
    thread->closed = false;
    return thread;
}
HANDLE GetCurrentProcess() noexcept { return kCurrentProcess; }
DWORD GetCurrentProcessId() noexcept { return static_cast<DWORD>(getpid()); }
DWORD GetProcessId(HANDLE) noexcept { return GetCurrentProcessId(); }
// A title may not end itself with exit(2) (the kernel answers SIGSYS): it asks
// the shell to close it and waits (ps5/src/platform.c).
extern "C" [[noreturn]] void catchReturnFromMain(int status);
void ExitProcess(UINT code) noexcept {
    std::fflush(nullptr);
    catchReturnFromMain(static_cast<int>(code));
}
BOOL TerminateProcess(HANDLE process, UINT code) noexcept {
    if (process == kCurrentProcess) ExitProcess(code);
    return FALSE;
}
void Sleep(DWORD milliseconds) noexcept {
    if (milliseconds == 0) {
        sched_yield();
        return;
    }
    if (milliseconds == INFINITE) {
        for (;;) pause();
    }
    struct timespec duration = {static_cast<time_t>(milliseconds / 1000), static_cast<long>(milliseconds % 1000) * 1000000L};
    while (nanosleep(&duration, &duration) != 0 && errno == EINTR) {
    }
}
DWORD SleepEx(DWORD milliseconds, BOOL) noexcept {
    Sleep(milliseconds);
    return 0;
}
BOOL SwitchToThread() noexcept {
    sched_yield();
    return TRUE;
}
HRESULT SetThreadDescription(HANDLE thread, PCWSTR description) noexcept {
    if (description != nullptr && (thread == kCurrentThread || thread == nullptr)) {
        std::string name = utf8(description);
        if (name.size() > 31) name.resize(31);
        set_thread_name(name);
    }
    return S_OK;
}
HRESULT GetThreadDescription(HANDLE, PWSTR* description) noexcept {
    if (description == nullptr) return E_POINTER;
    const std::wstring name = wide(thread_name());
    auto* copy = static_cast<wchar_t*>(std::calloc(name.size() + 1, sizeof(wchar_t)));
    if (copy == nullptr) return E_OUTOFMEMORY;
    std::wmemcpy(copy, name.c_str(), name.size());
    *description = copy;
    return S_OK;
}
// Thread priorities are the console scheduler's own; the runtime's requests are
// remembered (it reads them back to check), not applied.
namespace {
thread_local int t_priority = THREAD_PRIORITY_NORMAL;
std::mutex g_priority_lock;
std::unordered_map<DWORD, int> g_priorities;  // by thread id, for handles of other threads
}  // namespace
BOOL SetThreadPriority(HANDLE thread, int priority) noexcept {
    if (thread == kCurrentThread || thread == nullptr) {
        t_priority = priority;
        std::lock_guard lock(g_priority_lock);
        g_priorities[GetCurrentThreadId()] = priority;
        return TRUE;
    }
    const DWORD id = GetThreadId(thread);
    std::lock_guard lock(g_priority_lock);
    g_priorities[id] = priority;
    if (id == GetCurrentThreadId()) t_priority = priority;
    return TRUE;
}
int GetThreadPriority(HANDLE thread) noexcept {
    if (thread == kCurrentThread || thread == nullptr) return t_priority;
    const DWORD id = GetThreadId(thread);
    if (id == GetCurrentThreadId()) return t_priority;
    std::lock_guard lock(g_priority_lock);
    const auto it = g_priorities.find(id);
    return it != g_priorities.end() ? it->second : THREAD_PRIORITY_NORMAL;
}
BOOL SetThreadPriorityBoost(HANDLE, BOOL) noexcept { return TRUE; }
DWORD_PTR SetThreadAffinityMask(HANDLE, DWORD_PTR) noexcept { return 0xFF; }
BOOL GetProcessAffinityMask(HANDLE, PDWORD_PTR process_mask, PDWORD_PTR system_mask) noexcept {
    const DWORD_PTR all = (DWORD_PTR{1} << std::max(1u, std::thread::hardware_concurrency())) - 1;
    if (process_mask != nullptr) *process_mask = all;
    if (system_mask != nullptr) *system_mask = all;
    return TRUE;
}
DWORD SetThreadIdealProcessor(HANDLE, DWORD) noexcept { return 0; }
BOOL SetPriorityClass(HANDLE, DWORD) noexcept { return TRUE; }
DWORD GetPriorityClass(HANDLE) noexcept { return NORMAL_PRIORITY_CLASS; }
BOOL SetProcessPriorityBoost(HANDLE, BOOL) noexcept { return TRUE; }
BOOL SetProcessWorkingSetSize(HANDLE, SIZE_T, SIZE_T) noexcept { return TRUE; }
BOOL SetProcessInformation(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD) noexcept { return TRUE; }
BOOL SetThreadInformation(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD) noexcept { return TRUE; }
UINT SetErrorMode(UINT) noexcept { return 0; }

// CPU time: the calling thread's own clock (any thread handle reads the caller's).
BOOL QueryThreadCycleTime(HANDLE, PULONG64 cycles) noexcept {
    if (cycles == nullptr) return FALSE;
    struct timespec now;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    // In TSC-like units: nanoseconds times the nominal 3.5 GHz clock
    *cycles = (static_cast<ULONG64>(now.tv_sec) * 1000000000ull + static_cast<ULONG64>(now.tv_nsec)) * 7 / 2;
    return TRUE;
}
BOOL QueryProcessCycleTime(HANDLE, PULONG64 cycles) noexcept {
    if (cycles == nullptr) return FALSE;
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    *cycles = (static_cast<ULONG64>(now.tv_sec) * 1000000000ull + static_cast<ULONG64>(now.tv_nsec)) * 7 / 2;
    return TRUE;
}
namespace {
void filetime_from_ns(LPFILETIME time, std::uint64_t ns) noexcept {
    if (time == nullptr) return;
    const std::uint64_t units = ns / 100u;
    time->dwLowDateTime = static_cast<DWORD>(units);
    time->dwHighDateTime = static_cast<DWORD>(units >> 32);
}
}  // namespace
BOOL GetThreadTimes(HANDLE, LPFILETIME creation, LPFILETIME exit, LPFILETIME kernel, LPFILETIME user) noexcept {
    struct timespec now;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    filetime_from_ns(creation, 0);
    filetime_from_ns(exit, 0);
    filetime_from_ns(kernel, 0);
    filetime_from_ns(user, static_cast<std::uint64_t>(now.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(now.tv_nsec));
    return TRUE;
}
BOOL GetProcessTimes(HANDLE, LPFILETIME creation, LPFILETIME exit, LPFILETIME kernel, LPFILETIME user) noexcept {
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    filetime_from_ns(creation, 0);
    filetime_from_ns(exit, 0);
    filetime_from_ns(kernel, 0);
    filetime_from_ns(user, static_cast<std::uint64_t>(now.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(now.tv_nsec));
    return TRUE;
}

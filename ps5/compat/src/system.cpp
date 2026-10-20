// Super Mario Galaxy for the PlayStation 5 - time, locks, modules, memory and the
// system's description, and the desktop the console does not have (windows,
// cursor, keyboard, monitors, registry, devices, COM, the shell, symbols): each
// of those answers as Windows does when nothing is there.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <crtdbg.h>
#include <DbgHelp.h>
#include <dwmapi.h>
#include <Psapi.h>
#include <SetupAPI.h>
#include <Shlobj.h>
#include <TlHelp32.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <timeapi.h>
#include <winusb.h>

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace compat {
namespace {
thread_local std::string t_thread_name;
}
void set_thread_name(const std::string& name) noexcept { t_thread_name = name; }
std::string thread_name() noexcept { return t_thread_name; }
}  // namespace compat

using namespace compat;

// ---- time ---------------------------------------------------------------------------
namespace {
std::uint64_t monotonic_ns() noexcept {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(now.tv_nsec);
}
std::uint64_t filetime_now() noexcept {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return static_cast<std::uint64_t>(now.tv_sec) * 10000000ull + static_cast<std::uint64_t>(now.tv_nsec) / 100u +
           116444736000000000ull;
}
void systemtime_from_tm(const struct tm& parts, unsigned milliseconds, LPSYSTEMTIME time) noexcept {
    time->wYear = static_cast<WORD>(parts.tm_year + 1900);
    time->wMonth = static_cast<WORD>(parts.tm_mon + 1);
    time->wDayOfWeek = static_cast<WORD>(parts.tm_wday);
    time->wDay = static_cast<WORD>(parts.tm_mday);
    time->wHour = static_cast<WORD>(parts.tm_hour);
    time->wMinute = static_cast<WORD>(parts.tm_min);
    time->wSecond = static_cast<WORD>(parts.tm_sec);
    time->wMilliseconds = static_cast<WORD>(milliseconds);
}
}  // namespace

BOOL QueryPerformanceCounter(LARGE_INTEGER* count) noexcept {
    count->QuadPart = static_cast<LONGLONG>(monotonic_ns());
    return TRUE;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency) noexcept {
    frequency->QuadPart = 1000000000LL;
    return TRUE;
}
DWORD GetTickCount() noexcept { return static_cast<DWORD>(monotonic_ns() / 1000000u); }
ULONGLONG GetTickCount64() noexcept { return monotonic_ns() / 1000000u; }
void GetSystemTimeAsFileTime(LPFILETIME time) noexcept {
    const std::uint64_t now = filetime_now();
    time->dwLowDateTime = static_cast<DWORD>(now);
    time->dwHighDateTime = static_cast<DWORD>(now >> 32);
}
void GetSystemTimePreciseAsFileTime(LPFILETIME time) noexcept { GetSystemTimeAsFileTime(time); }
void GetLocalTime(LPSYSTEMTIME time) noexcept {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    struct tm parts{};
    const time_t seconds = now.tv_sec;
    localtime_r(&seconds, &parts);
    systemtime_from_tm(parts, static_cast<unsigned>(now.tv_nsec / 1000000), time);
}
void GetSystemTime(LPSYSTEMTIME time) noexcept {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    struct tm parts{};
    const time_t seconds = now.tv_sec;
    gmtime_r(&seconds, &parts);
    systemtime_from_tm(parts, static_cast<unsigned>(now.tv_nsec / 1000000), time);
}
BOOL FileTimeToSystemTime(const FILETIME* file_time, LPSYSTEMTIME system_time) noexcept {
    const std::uint64_t units = (static_cast<std::uint64_t>(file_time->dwHighDateTime) << 32) | file_time->dwLowDateTime;
    const time_t seconds = static_cast<time_t>((units - 116444736000000000ull) / 10000000ull);
    struct tm parts{};
    gmtime_r(&seconds, &parts);
    systemtime_from_tm(parts, static_cast<unsigned>((units / 10000u) % 1000u), system_time);
    return TRUE;
}
BOOL SystemTimeToFileTime(const SYSTEMTIME* system_time, LPFILETIME file_time) noexcept {
    struct tm parts{};
    parts.tm_year = system_time->wYear - 1900;
    parts.tm_mon = system_time->wMonth - 1;
    parts.tm_mday = system_time->wDay;
    parts.tm_hour = system_time->wHour;
    parts.tm_min = system_time->wMinute;
    parts.tm_sec = system_time->wSecond;
    // Days from the civil date (the console's libc has no timegm)
    const int year = parts.tm_year + 1900 - (parts.tm_mon < 2 ? 1 : 0);
    const int era = (year >= 0 ? year : year - 399) / 400;
    const int year_of_era = year - era * 400;
    const int month = parts.tm_mon + 1;
    const int day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + parts.tm_mday - 1;
    const int day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    const long long days = static_cast<long long>(era) * 146097 + day_of_era - 719468;
    const time_t seconds = static_cast<time_t>(days * 86400 + parts.tm_hour * 3600 + parts.tm_min * 60 + parts.tm_sec);
    const std::uint64_t units = static_cast<std::uint64_t>(seconds) * 10000000ull +
                                static_cast<std::uint64_t>(system_time->wMilliseconds) * 10000ull + 116444736000000000ull;
    file_time->dwLowDateTime = static_cast<DWORD>(units);
    file_time->dwHighDateTime = static_cast<DWORD>(units >> 32);
    return TRUE;
}
BOOL FileTimeToLocalFileTime(const FILETIME* file_time, LPFILETIME local) noexcept {
    *local = *file_time;
    return TRUE;
}
BOOL QueryUnbiasedInterruptTime(PULONGLONG time) noexcept {
    *time = monotonic_ns() / 100u;
    return TRUE;
}
void QueryInterruptTime(PULONGLONG time) noexcept { *time = monotonic_ns() / 100u; }
void QueryInterruptTimePrecise(PULONGLONG time) noexcept { *time = monotonic_ns() / 100u; }
MMRESULT timeBeginPeriod(UINT) noexcept { return TIMERR_NOERROR; }
MMRESULT timeEndPeriod(UINT) noexcept { return TIMERR_NOERROR; }
MMRESULT timeGetDevCaps(LPTIMECAPS caps, UINT) noexcept {
    caps->wPeriodMin = 1;
    caps->wPeriodMax = 1000000;
    return TIMERR_NOERROR;
}
DWORD timeGetTime() noexcept { return GetTickCount(); }

// ---- slim reader/writer locks --------------------------------------------------------
// A zero SRWLOCK is unlocked. The word: bit 0 = a writer holds it, the rest =
// readers << 1. Contention backs off with a pause, then yields; the runtime's
// locks guard short sections.
namespace {
std::atomic<std::uintptr_t>& lock_word(PSRWLOCK lock) noexcept {
    static_assert(sizeof(std::atomic<std::uintptr_t>) == sizeof(PVOID));
    return *reinterpret_cast<std::atomic<std::uintptr_t>*>(&lock->Ptr);
}
void backoff(unsigned& spins) noexcept {
    if (++spins < 64) {
        _mm_pause();
    } else {
        sched_yield();
    }
}
}  // namespace
void InitializeSRWLock(PSRWLOCK lock) noexcept { lock->Ptr = nullptr; }
BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK lock) noexcept {
    std::uintptr_t expected = 0;
    return lock_word(lock).compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
}
void AcquireSRWLockExclusive(PSRWLOCK lock) noexcept {
    unsigned spins = 0;
    while (!TryAcquireSRWLockExclusive(lock)) backoff(spins);
}
void ReleaseSRWLockExclusive(PSRWLOCK lock) noexcept { lock_word(lock).store(0, std::memory_order_release); }
BOOLEAN TryAcquireSRWLockShared(PSRWLOCK lock) noexcept {
    auto& word = lock_word(lock);
    std::uintptr_t value = word.load(std::memory_order_relaxed);
    while ((value & 1) == 0) {
        if (word.compare_exchange_weak(value, value + 2, std::memory_order_acquire, std::memory_order_relaxed)) return TRUE;
    }
    return FALSE;
}
void AcquireSRWLockShared(PSRWLOCK lock) noexcept {
    unsigned spins = 0;
    while (!TryAcquireSRWLockShared(lock)) backoff(spins);
}
void ReleaseSRWLockShared(PSRWLOCK lock) noexcept { lock_word(lock).fetch_sub(2, std::memory_order_release); }

// ---- condition variables and address waits --------------------------------------------
// One process-wide generation counter and condition variable serve every
// CONDITION_VARIABLE and WaitOnAddress: a wake wakes all waiters, who check
// again (spurious wakes are allowed by both APIs).
namespace {
std::mutex g_cv_lock;
std::condition_variable g_cv;
std::uint64_t g_cv_generation = 0;

bool wait_generation(std::uint64_t seen, DWORD milliseconds) {
    std::unique_lock guard(g_cv_lock);
    if (milliseconds == INFINITE) {
        g_cv.wait(guard, [&] { return g_cv_generation != seen; });
        return true;
    }
    return g_cv.wait_for(guard, std::chrono::milliseconds(milliseconds), [&] { return g_cv_generation != seen; });
}
std::uint64_t current_generation() {
    std::lock_guard guard(g_cv_lock);
    return g_cv_generation;
}
void wake_all_generation() {
    {
        std::lock_guard guard(g_cv_lock);
        ++g_cv_generation;
    }
    g_cv.notify_all();
}
}  // namespace
void InitializeConditionVariable(PCONDITION_VARIABLE variable) noexcept { variable->Ptr = nullptr; }
BOOL SleepConditionVariableSRW(PCONDITION_VARIABLE, PSRWLOCK lock, DWORD milliseconds, ULONG flags) noexcept {
    const bool shared = (flags & CONDITION_VARIABLE_LOCKMODE_SHARED) != 0;
    const std::uint64_t seen = current_generation();
    shared ? ReleaseSRWLockShared(lock) : ReleaseSRWLockExclusive(lock);
    const bool woken = wait_generation(seen, milliseconds);
    shared ? AcquireSRWLockShared(lock) : AcquireSRWLockExclusive(lock);
    if (!woken) {
        SetLastError(ERROR_TIMEOUT);
        return FALSE;
    }
    return TRUE;
}
void WakeConditionVariable(PCONDITION_VARIABLE) noexcept { wake_all_generation(); }
void WakeAllConditionVariable(PCONDITION_VARIABLE) noexcept { wake_all_generation(); }
BOOL WaitOnAddress(volatile void* address, PVOID compare, SIZE_T size, DWORD milliseconds) noexcept {
    const std::uint64_t seen = current_generation();
    if (std::memcmp(const_cast<void*>(address), compare, size) != 0) return TRUE;
    if (!wait_generation(seen, milliseconds)) {
        SetLastError(ERROR_TIMEOUT);
        return FALSE;
    }
    return TRUE;
}
void WakeByAddressSingle(PVOID) noexcept { wake_all_generation(); }
void WakeByAddressAll(PVOID) noexcept { wake_all_generation(); }

// ---- critical sections, one-time initialisation, TLS ---------------------------------------
void InitializeCriticalSection(LPCRITICAL_SECTION section) noexcept { section->impl = new std::recursive_mutex(); }
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION section, DWORD) noexcept {
    InitializeCriticalSection(section);
    return TRUE;
}
void DeleteCriticalSection(LPCRITICAL_SECTION section) noexcept {
    delete static_cast<std::recursive_mutex*>(section->impl);
    section->impl = nullptr;
}
void EnterCriticalSection(LPCRITICAL_SECTION section) noexcept { static_cast<std::recursive_mutex*>(section->impl)->lock(); }
void LeaveCriticalSection(LPCRITICAL_SECTION section) noexcept {
    static_cast<std::recursive_mutex*>(section->impl)->unlock();
}
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION section) noexcept {
    return static_cast<std::recursive_mutex*>(section->impl)->try_lock();
}
BOOL InitOnceExecuteOnce(PINIT_ONCE once, PINIT_ONCE_FN function, PVOID parameter, LPVOID* context) noexcept {
    static std::mutex lock;
    std::lock_guard guard(lock);
    if (once->Ptr == nullptr) {
        if (!function(once, parameter, context)) return FALSE;
        once->Ptr = reinterpret_cast<PVOID>(1);
    }
    return TRUE;
}
namespace {
constexpr DWORD kTlsSlots = 128;
std::atomic<bool> g_tls_used[kTlsSlots];
thread_local LPVOID t_tls[kTlsSlots];
}  // namespace
DWORD TlsAlloc() noexcept {
    for (DWORD i = 0; i < kTlsSlots; ++i) {
        bool expected = false;
        if (g_tls_used[i].compare_exchange_strong(expected, true)) return i;
    }
    return TLS_OUT_OF_INDEXES;
}
BOOL TlsFree(DWORD index) noexcept {
    if (index >= kTlsSlots) return FALSE;
    g_tls_used[index] = false;
    return TRUE;
}
LPVOID TlsGetValue(DWORD index) noexcept { return index < kTlsSlots ? t_tls[index] : nullptr; }
BOOL TlsSetValue(DWORD index, LPVOID value) noexcept {
    if (index >= kTlsSlots) return FALSE;
    t_tls[index] = value;
    return TRUE;
}

// ---- the system ---------------------------------------------------------------------
void GetSystemInfo(LPSYSTEM_INFO info) noexcept {
    std::memset(info, 0, sizeof(*info));
    info->wProcessorArchitecture = 9;  // PROCESSOR_ARCHITECTURE_AMD64
    info->dwPageSize = 16384;
    info->dwAllocationGranularity = 65536;
    info->lpMinimumApplicationAddress = reinterpret_cast<LPVOID>(0x10000);
    info->lpMaximumApplicationAddress = reinterpret_cast<LPVOID>(0x7FFFFFFEFFFFull);
    info->dwNumberOfProcessors = std::max(1u, std::thread::hardware_concurrency());
    info->dwActiveProcessorMask = (DWORD_PTR{1} << info->dwNumberOfProcessors) - 1;
    info->dwProcessorType = 8664;
    info->wProcessorLevel = 23;
}
void GetNativeSystemInfo(LPSYSTEM_INFO info) noexcept { GetSystemInfo(info); }
BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX status) noexcept {
    status->dwMemoryLoad = 30;
    status->ullTotalPhys = 12ull << 30;
    status->ullAvailPhys = 8ull << 30;
    status->ullTotalPageFile = status->ullTotalPhys;
    status->ullAvailPageFile = status->ullAvailPhys;
    status->ullTotalVirtual = 1ull << 46;
    status->ullAvailVirtual = 1ull << 45;
    status->ullAvailExtendedVirtual = 0;
    return TRUE;
}
BOOL IsProcessorFeaturePresent(DWORD feature) noexcept {
    // Zen 2: SSE3 through AVX2 and FMA
    return feature == PF_AVX2_INSTRUCTIONS_AVAILABLE || feature == PF_SSE3_INSTRUCTIONS_AVAILABLE || feature == 6 ||
           feature == 10 || feature == 17 || feature == 36 || feature == 37 || feature == 38 || feature == 39;
}
void GetCurrentProcessorNumberEx(PPROCESSOR_NUMBER number) noexcept {
    number->Group = 0;
    number->Number = 0;
    number->Reserved = 0;
}
DWORD GetCurrentProcessorNumber() noexcept { return 0; }
WORD GetActiveProcessorGroupCount() noexcept { return 1; }
DWORD GetActiveProcessorCount(WORD) noexcept { return std::max(1u, std::thread::hardware_concurrency()); }
BOOL GetThreadGroupAffinity(HANDLE, PGROUP_AFFINITY affinity) noexcept {
    std::memset(affinity, 0, sizeof(*affinity));
    affinity->Mask = (KAFFINITY{1} << GetActiveProcessorCount(0)) - 1;
    return TRUE;
}
BOOL SetThreadGroupAffinity(HANDLE thread, const GROUP_AFFINITY*, PGROUP_AFFINITY previous) noexcept {
    if (previous != nullptr) GetThreadGroupAffinity(thread, previous);
    return TRUE;
}
BOOL GetLogicalProcessorInformationEx(LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX,
                                      PDWORD length) noexcept {
    // No topology to report: the callers then treat every core alike.
    if (length != nullptr) *length = 0;
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
EXECUTION_STATE SetThreadExecutionState(EXECUTION_STATE state) noexcept { return state; }

// ---- memory -----------------------------------------------------------------------------
LPVOID VirtualAlloc(LPVOID, SIZE_T size, DWORD type, DWORD) noexcept {
    // Reservations and commits alike come from the title heap (direct memory), zeroed.
    if ((type & (MEM_COMMIT | MEM_RESERVE)) == 0) return nullptr;
    if (size >= (64u << 20)) std::fprintf(stderr, "[compat] VirtualAlloc %zu MiB\n", static_cast<std::size_t>(size >> 20));
    void* memory = nullptr;
    if (posix_memalign(&memory, 65536, (size + 0xFFFF) & ~SIZE_T{0xFFFF}) != 0) return nullptr;
    std::memset(memory, 0, size);
    return memory;
}
BOOL VirtualFree(LPVOID address, SIZE_T, DWORD type) noexcept {
    if ((type & MEM_RELEASE) != 0) std::free(address);
    return TRUE;
}
BOOL VirtualProtect(LPVOID, SIZE_T, DWORD protect, PDWORD old_protect) noexcept {
    if (old_protect != nullptr) *old_protect = protect;
    return TRUE;
}
BOOL VirtualLock(LPVOID, SIZE_T) noexcept { return TRUE; }
BOOL VirtualUnlock(LPVOID, SIZE_T) noexcept { return TRUE; }
SIZE_T VirtualQuery(LPCVOID address, PMEMORY_BASIC_INFORMATION buffer, SIZE_T length) noexcept {
    if (buffer == nullptr || length < sizeof(*buffer)) return 0;
    std::memset(buffer, 0, sizeof(*buffer));
    buffer->BaseAddress = const_cast<LPVOID>(address);
    buffer->AllocationBase = const_cast<LPVOID>(address);
    buffer->RegionSize = 4096;
    buffer->State = MEM_COMMIT;
    buffer->Protect = PAGE_READWRITE;
    buffer->Type = MEM_PRIVATE;
    return sizeof(*buffer);
}
BOOL PrefetchVirtualMemory(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG) noexcept { return TRUE; }
HANDLE GetProcessHeap() noexcept { return reinterpret_cast<HANDLE>(0x1); }
LPVOID HeapAlloc(HANDLE, DWORD flags, SIZE_T bytes) noexcept {
    return (flags & HEAP_ZERO_MEMORY) != 0 ? std::calloc(1, bytes) : std::malloc(bytes);
}
BOOL HeapFree(HANDLE, DWORD, LPVOID memory) noexcept {
    std::free(memory);
    return TRUE;
}
LPVOID HeapReAlloc(HANDLE, DWORD, LPVOID memory, SIZE_T bytes) noexcept { return std::realloc(memory, bytes); }
HLOCAL LocalFree(HLOCAL memory) noexcept {
    std::free(memory);
    return nullptr;
}
HLOCAL LocalAlloc(UINT flags, SIZE_T bytes) noexcept {
    return (flags & LMEM_ZEROINIT) != 0 ? std::calloc(1, bytes) : std::malloc(bytes);
}
BOOL IsBadReadPtr(const void* pointer, UINT_PTR) noexcept { return pointer == nullptr; }
BOOL IsBadWritePtr(LPVOID pointer, UINT_PTR) noexcept { return pointer == nullptr; }
BOOL GetHandleInformation(HANDLE object, LPDWORD flags) noexcept {
    if (object == nullptr || object == INVALID_HANDLE_VALUE) return FALSE;
    if (flags != nullptr) *flags = 0;
    return TRUE;
}
BOOL SetHandleInformation(HANDLE, DWORD, DWORD) noexcept { return TRUE; }

// ---- modules ---------------------------------------------------------------------------
// The modules linked into the eboot register their exports here
// (ps5/src/modules.cpp); LoadLibrary matches a name by the part it contains.
namespace {
struct Module {
    const char* name_part;
    const compat::ModuleExport* exports;
    std::size_t count;
};
std::vector<Module>& modules() {
    static std::vector<Module> list;
    return list;
}
int g_main_module_marker;
HMODULE main_module() noexcept { return reinterpret_cast<HMODULE>(&g_main_module_marker); }

HMODULE find_module(const std::string& name) noexcept {
    for (const Module& module : modules()) {
        if (name.find(module.name_part) != std::string::npos) return reinterpret_cast<HMODULE>(const_cast<Module*>(&module));
    }
    SetLastError(ERROR_MOD_NOT_FOUND);
    return nullptr;
}
}  // namespace

void compat::register_module(const char* name_part, const ModuleExport* exports, std::size_t count) noexcept {
    modules().push_back(Module{name_part, exports, count});
}

HMODULE LoadLibraryW(LPCWSTR name) noexcept { return find_module(utf8(name)); }
HMODULE LoadLibraryW(const char* name) noexcept { return find_module(name != nullptr ? name : ""); }
HMODULE LoadLibraryA(LPCSTR name) noexcept { return find_module(name != nullptr ? name : ""); }
HMODULE LoadLibraryExW(LPCWSTR name, HANDLE, DWORD) noexcept { return find_module(utf8(name)); }
HMODULE LoadLibraryExW(const char* name, HANDLE, DWORD) noexcept { return find_module(name != nullptr ? name : ""); }
HMODULE LoadLibraryExA(LPCSTR name, HANDLE, DWORD) noexcept { return find_module(name != nullptr ? name : ""); }
FARPROC GetProcAddress(HMODULE handle, LPCSTR name) noexcept {
    if (handle == nullptr || handle == main_module() || name == nullptr) return nullptr;
    for (const Module& module : modules()) {
        if (reinterpret_cast<HMODULE>(const_cast<Module*>(&module)) != handle) continue;
        for (std::size_t i = 0; i < module.count; ++i) {
            if (std::strcmp(module.exports[i].name, name) == 0) return reinterpret_cast<FARPROC>(module.exports[i].address);
        }
    }
    SetLastError(ERROR_PROC_NOT_FOUND);
    return nullptr;
}
BOOL FreeLibrary(HMODULE) noexcept { return TRUE; }
HMODULE GetModuleHandleW(LPCWSTR name) noexcept {
    // The running program; system DLLs (ntdll, kernelbase, avrt...) are absent.
    return name == nullptr ? main_module() : nullptr;
}
HMODULE GetModuleHandleA(LPCSTR name) noexcept { return name == nullptr ? main_module() : nullptr; }
BOOL GetModuleHandleExW(DWORD, LPCWSTR, HMODULE* module) noexcept {
    if (module != nullptr) *module = main_module();
    return TRUE;
}
DWORD GetModuleFileNameW(HMODULE, LPWSTR name, DWORD size) noexcept {
    const DWORD length = copy_out(L"/app0/eboot.bin", name, size);
    return length < size ? length : size;
}
DWORD GetModuleFileNameA(HMODULE, LPSTR name, DWORD size) noexcept {
    const char path[] = "/app0/eboot.bin";
    if (size == 0) return 0;
    std::snprintf(name, size, "%s", path);
    return static_cast<DWORD>(std::min<std::size_t>(sizeof(path) - 1, size));
}
BOOL SetDllDirectoryW(LPCWSTR) noexcept { return TRUE; }
BOOL SetDefaultDllDirectories(DWORD) noexcept { return TRUE; }
void* AddDllDirectory(PCWSTR) noexcept { return reinterpret_cast<void*>(1); }

// ---- process diagnostics ---------------------------------------------------------------
BOOL GetProcessMemoryInfo(HANDLE, PPROCESS_MEMORY_COUNTERS counters, DWORD size) noexcept {
    std::memset(counters, 0, size);
    counters->cb = size;
    return TRUE;
}
namespace {
struct Snapshot final : Object {
    Snapshot() : Object(Kind::Snapshot) {}
    bool listed = false;
};
}  // namespace
HANDLE CreateToolhelp32Snapshot(DWORD, DWORD) noexcept { return new Snapshot(); }
BOOL Thread32First(HANDLE snapshot, LPTHREADENTRY32 entry) noexcept {
    if (snapshot == nullptr || entry == nullptr) return FALSE;
    entry->th32ThreadID = GetCurrentThreadId();
    entry->th32OwnerProcessID = GetCurrentProcessId();
    static_cast<Snapshot*>(snapshot)->listed = true;
    return TRUE;
}
BOOL Thread32Next(HANDLE, LPTHREADENTRY32) noexcept {
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}
DWORD SymSetOptions(DWORD options) noexcept { return options; }
DWORD SymGetOptions() noexcept { return 0; }
BOOL SymInitialize(HANDLE, PCSTR, BOOL) noexcept { return FALSE; }
BOOL SymInitializeW(HANDLE, PCWSTR, BOOL) noexcept { return FALSE; }
BOOL SymCleanup(HANDLE) noexcept { return TRUE; }
BOOL SymFromAddr(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO) noexcept { return FALSE; }
BOOL SymGetLineFromAddr64(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64) noexcept { return FALSE; }
DWORD64 SymGetModuleBase64(HANDLE, DWORD64) noexcept { return 0; }
PVOID SymFunctionTableAccess64(HANDLE, DWORD64) noexcept { return nullptr; }
BOOL MiniDumpWriteDump(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PVOID, PVOID) noexcept {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
int _CrtSetReportMode(int, int) noexcept { return 0; }
HANDLE _CrtSetReportFile(int, HANDLE) noexcept { return nullptr; }
_CRT_REPORT_HOOK _CrtSetReportHook(_CRT_REPORT_HOOK) noexcept { return nullptr; }
int _CrtSetReportHook2(int, _CRT_REPORT_HOOK) noexcept { return 0; }
unsigned int _set_abort_behavior(unsigned int, unsigned int) noexcept { return 0; }
int _set_error_mode(int) noexcept { return 0; }
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler) noexcept { return nullptr; }
_invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler) noexcept {
    return nullptr;
}
_purecall_handler _set_purecall_handler(_purecall_handler) noexcept { return nullptr; }

// ---- known folders ------------------------------------------------------------------------
HRESULT SHGetFolderPathW(HWND, int, HANDLE, DWORD, LPWSTR path) noexcept {
    if (path == nullptr) return E_POINTER;
    copy_out(L"/app0/userdata", path, MAX_PATH);
    return S_OK;
}
HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* path) noexcept {
    if (path == nullptr) return E_POINTER;
    *path = compat::wide_duplicate(L"/app0/userdata");
    return *path != nullptr ? S_OK : E_OUTOFMEMORY;
}
HINSTANCE ShellExecuteW(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT) noexcept {
    return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(ERROR_FILE_NOT_FOUND));
}
HINSTANCE ShellExecuteW(HWND, LPCWSTR, const char*, LPCWSTR, const char*, INT) noexcept {
    return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(ERROR_FILE_NOT_FOUND));
}

// ---- COM ------------------------------------------------------------------------------
HRESULT CoInitializeEx(LPVOID, DWORD) noexcept { return S_OK; }
HRESULT CoInitialize(LPVOID) noexcept { return S_OK; }
void CoUninitialize() noexcept {}
HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID* object) noexcept {
    if (object != nullptr) *object = nullptr;
    return E_NOINTERFACE;
}
void CoTaskMemFree(LPVOID memory) noexcept { std::free(memory); }
LPVOID CoTaskMemAlloc(SIZE_T bytes) noexcept { return std::malloc(bytes); }
HRESULT CLSIDFromString(LPCWSTR, CLSID* clsid) noexcept {
    if (clsid != nullptr) *clsid = {};
    return E_INVALIDARG;
}
HRESULT IIDFromString(LPCWSTR text, IID* iid) noexcept { return CLSIDFromString(text, iid); }

// ---- devices: none ----------------------------------------------------------------------
HDEVINFO SetupDiGetClassDevsW(const GUID*, PCWSTR, HWND, DWORD) noexcept { return INVALID_HANDLE_VALUE; }
HDEVINFO SetupDiCreateDeviceInfoList(const GUID*, HWND) noexcept { return INVALID_HANDLE_VALUE; }
BOOL SetupDiDestroyDeviceInfoList(HDEVINFO) noexcept { return TRUE; }
BOOL SetupDiEnumDeviceInterfaces(HDEVINFO, PSP_DEVINFO_DATA, const GUID*, DWORD, PSP_DEVICE_INTERFACE_DATA) noexcept {
    SetLastError(ERROR_NO_MORE_ITEMS);
    return FALSE;
}
BOOL SetupDiEnumDeviceInfo(HDEVINFO, DWORD, PSP_DEVINFO_DATA) noexcept {
    SetLastError(ERROR_NO_MORE_ITEMS);
    return FALSE;
}
BOOL SetupDiGetDeviceInterfaceDetailW(HDEVINFO, PSP_DEVICE_INTERFACE_DATA, PSP_DEVICE_INTERFACE_DETAIL_DATA_W, DWORD,
                                      PDWORD, PSP_DEVINFO_DATA) noexcept {
    SetLastError(ERROR_NO_MORE_ITEMS);
    return FALSE;
}
BOOL SetupDiGetDeviceInfoListDetailW(HDEVINFO, PSP_DEVINFO_LIST_DETAIL_DATA_W) noexcept { return FALSE; }
HKEY SetupDiOpenDevRegKey(HDEVINFO, PSP_DEVINFO_DATA, DWORD, DWORD, DWORD, REGSAM) noexcept {
    return reinterpret_cast<HKEY>(INVALID_HANDLE_VALUE);
}
BOOL SetupDiGetDeviceRegistryPropertyW(HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD, PDWORD) noexcept {
    return FALSE;
}
BOOL SetupDiGetDeviceInstanceIdW(HDEVINFO, PSP_DEVINFO_DATA, PWSTR, DWORD, PDWORD) noexcept { return FALSE; }
void HidD_GetHidGuid(GUID* guid) noexcept { *guid = {0x4D1E55B2, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}}; }
BOOLEAN HidD_GetAttributes(HANDLE, PHIDD_ATTRIBUTES) noexcept { return FALSE; }
BOOLEAN HidD_GetPreparsedData(HANDLE, PHIDP_PREPARSED_DATA*) noexcept { return FALSE; }
BOOLEAN HidD_FreePreparsedData(PHIDP_PREPARSED_DATA) noexcept { return TRUE; }
BOOLEAN HidD_SetOutputReport(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_GetInputReport(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_SetFeature(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_GetFeature(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_SetNumInputBuffers(HANDLE, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_GetProductString(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_GetSerialNumberString(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
BOOLEAN HidD_GetManufacturerString(HANDLE, PVOID, ULONG) noexcept { return FALSE; }
NTSTATUS HidP_GetCaps(PHIDP_PREPARSED_DATA, PHIDP_CAPS) noexcept { return static_cast<NTSTATUS>(0xC0110001); }
CONFIGRET CM_Register_Notification(PCM_NOTIFY_FILTER, PVOID, PCM_NOTIFY_CALLBACK, PHCMNOTIFICATION notification) noexcept {
    if (notification != nullptr) *notification = reinterpret_cast<HCMNOTIFICATION>(1);
    return CR_SUCCESS;
}
CONFIGRET CM_Unregister_Notification(HCMNOTIFICATION) noexcept { return CR_SUCCESS; }
CONFIGRET CM_Get_Device_Interface_List_SizeW(PULONG length, const GUID*, PWSTR, ULONG) noexcept {
    if (length != nullptr) *length = 1;
    return CR_SUCCESS;
}
CONFIGRET CM_Get_Device_Interface_ListW(const GUID*, PWSTR, PWSTR buffer, ULONG length, ULONG) noexcept {
    if (buffer != nullptr && length > 0) buffer[0] = L'\0';
    return CR_SUCCESS;
}
CONFIGRET CM_Get_Device_IDW(DEVINST, PWSTR buffer, ULONG length, ULONG) noexcept {
    if (buffer != nullptr && length > 0) buffer[0] = L'\0';
    return CR_NO_SUCH_VALUE;
}
CONFIGRET CM_Locate_DevNodeW(DEVINST*, PWSTR, ULONG) noexcept { return CR_NO_SUCH_VALUE; }
BOOL WinUsb_Initialize(HANDLE, PWINUSB_INTERFACE_HANDLE) noexcept { return FALSE; }
BOOL WinUsb_Free(WINUSB_INTERFACE_HANDLE) noexcept { return TRUE; }
BOOL WinUsb_QueryInterfaceSettings(WINUSB_INTERFACE_HANDLE, UCHAR, PUSB_INTERFACE_DESCRIPTOR) noexcept { return FALSE; }
BOOL WinUsb_QueryPipe(WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, PWINUSB_PIPE_INFORMATION) noexcept { return FALSE; }
BOOL WinUsb_ReadPipe(WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED) noexcept { return FALSE; }
BOOL WinUsb_WritePipe(WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED) noexcept { return FALSE; }
BOOL WinUsb_SetPipePolicy(WINUSB_INTERFACE_HANDLE, UCHAR, ULONG, ULONG, PVOID) noexcept { return FALSE; }
BOOL WinUsb_ControlTransfer(WINUSB_INTERFACE_HANDLE, WINUSB_SETUP_PACKET, PUCHAR, ULONG, PULONG, LPOVERLAPPED) noexcept {
    return FALSE;
}
BOOL WinUsb_AbortPipe(WINUSB_INTERFACE_HANDLE, UCHAR) noexcept { return FALSE; }
BOOL WinUsb_FlushPipe(WINUSB_INTERFACE_HANDLE, UCHAR) noexcept { return FALSE; }

// ---- the registry: empty ----------------------------------------------------------------
LSTATUS RegOpenKeyExW(HKEY, LPCWSTR, DWORD, REGSAM, HKEY* result) noexcept {
    if (result != nullptr) *result = nullptr;
    return ERROR_FILE_NOT_FOUND;
}
LSTATUS RegQueryValueExW(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) noexcept { return ERROR_FILE_NOT_FOUND; }
LSTATUS RegGetValueW(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) noexcept { return ERROR_FILE_NOT_FOUND; }
LSTATUS RegCloseKey(HKEY) noexcept { return ERROR_SUCCESS; }
LSTATUS RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD) noexcept { return ERROR_ACCESS_DENIED; }
LSTATUS RegCreateKeyExW(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, HKEY* result,
                        LPDWORD) noexcept {
    if (result != nullptr) *result = nullptr;
    return ERROR_ACCESS_DENIED;
}

// ---- the compositor: none --------------------------------------------------------------
HRESULT DwmFlush() noexcept { return E_NOTIMPL; }
HRESULT DwmGetCompositionTimingInfo(HWND, DWM_TIMING_INFO*) noexcept { return E_NOTIMPL; }
HRESULT DwmSetWindowAttribute(HWND, DWORD, LPCVOID, DWORD) noexcept { return S_OK; }
HRESULT DwmIsCompositionEnabled(BOOL* enabled) noexcept {
    if (enabled != nullptr) *enabled = FALSE;
    return S_OK;
}

// Super Mario Galaxy for the PlayStation 5 - the part of the Windows API Nebula's
// runtime calls, on the console's FreeBSD libc and the PS5 platform layer.
//
// Nebula is written for Windows only. Rather than rewrite 70,000 lines of
// runtime around another API, the console build gives it this header: the
// types, constants and functions it uses, implemented in ps5/compat/src. Files,
// events, timers, fibers, environment and time behave as Windows documents
// them; windows, HID devices and the shell answer "none" (the console has one
// screen and its own pad, which input goes through XInput: ps5/compat/src/xinput.cpp).
//
// wchar_t is 32-bit here (UTF-32), not 16-bit: the W functions convert to UTF-8.
// The Mac's file system ignores case, so "Windows.h" finds this file too.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <intrin.h>
#include "msvc_crt.h"

// ---- base types ----------------------------------------------------------------
#define WINAPI
#define APIENTRY
#define CALLBACK
#define WINAPIV
#define STDMETHODCALLTYPE
#define STDAPI extern "C" HRESULT
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _In_reads_(x)
#define _In_reads_bytes_(x)
#define _Out_writes_(x)
#define _Out_writes_bytes_(x)
#define _Out_writes_opt_(x)
#define _Outptr_
#define _Outptr_opt_
#define _COM_Outptr_
#define _Reserved_
#define _Use_decl_annotations_
#define DECLSPEC_UUID(x)
#define DECLSPEC_NOVTABLE
#define MIDL_INTERFACE(x) struct
#define FORCEINLINE inline __attribute__((always_inline))
#define UNREFERENCED_PARAMETER(x) (void)(x)
#define CONST const
#define VOID void

typedef int BOOL;
typedef unsigned char BOOLEAN;
typedef std::uint8_t BYTE;
typedef std::uint8_t UCHAR;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef std::uint16_t WORD;
typedef std::uint16_t USHORT;
typedef std::int16_t SHORT;
typedef std::uint32_t DWORD;
typedef std::uint32_t UINT;
typedef std::uint32_t UINT32;
typedef std::int32_t INT32;
typedef std::uint64_t UINT64;
typedef std::int64_t INT64;
typedef std::uint8_t UINT8;
typedef std::uint16_t UINT16;
typedef std::int8_t INT8;
typedef std::int16_t INT16;
typedef std::uint32_t ULONG;
typedef std::int32_t LONG;
typedef std::int32_t INT;
typedef float FLOAT;
typedef std::int64_t LONGLONG;
typedef std::uint64_t ULONGLONG;
typedef std::uint64_t DWORDLONG;
typedef std::uint64_t DWORD64;
typedef std::uint64_t ULONG64;
typedef std::int64_t LONG64;
typedef std::intptr_t INT_PTR;
typedef std::uintptr_t UINT_PTR;
typedef std::intptr_t LONG_PTR;
typedef std::uintptr_t ULONG_PTR;
typedef std::uintptr_t DWORD_PTR;
typedef std::size_t SIZE_T;
typedef std::intptr_t SSIZE_T;
typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef BOOL* PBOOL;
typedef BOOL* LPBOOL;
typedef BYTE* PBYTE;
typedef BYTE* LPBYTE;
typedef UCHAR* PUCHAR;
typedef char* PCHAR;
typedef WORD* PWORD;
typedef WORD* LPWORD;
typedef USHORT* PUSHORT;
typedef DWORD* PDWORD;
typedef DWORD* LPDWORD;
typedef LONG* PLONG;
typedef LONG* LPLONG;
typedef ULONG* PULONG;
typedef UINT* PUINT;
typedef SIZE_T* PSIZE_T;
typedef ULONG_PTR* PULONG_PTR;
typedef DWORD_PTR* PDWORD_PTR;
typedef DWORD64* PDWORD64;
typedef ULONG64* PULONG64;
typedef ULONGLONG* PULONGLONG;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef char* PSTR;
typedef wchar_t* LPWSTR;
typedef wchar_t* PWSTR;
typedef wchar_t* LPWCH;
typedef const wchar_t* LPCWSTR;
typedef const wchar_t* PCWSTR;
typedef const wchar_t* LPCTSTR;
typedef std::int32_t HRESULT;
typedef DWORD ACCESS_MASK;
typedef WORD ATOM;
typedef DWORD COLORREF;
typedef LONG NTSTATUS;

typedef void* HANDLE;
typedef HANDLE* PHANDLE;
typedef HANDLE* LPHANDLE;
#define DECLARE_HANDLE(name) \
    struct name##__ {        \
        int unused;          \
    };                       \
    typedef struct name##__* name
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;
typedef void* HGDIOBJ;
typedef void* HDEVNOTIFY;
typedef void* HPOWERNOTIFY;
typedef void* HLOCAL;
typedef void* HGLOBAL;
typedef int (*FARPROC)();
typedef int (*PROC)();
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1)))
#define INVALID_FILE_SIZE 0xFFFFFFFFu
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#define INVALID_SET_FILE_POINTER 0xFFFFFFFFu

#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEW

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    struct {
        DWORD LowPart;
        DWORD HighPart;
    } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _GUID {
    std::uint32_t Data1;
    std::uint16_t Data2;
    std::uint16_t Data3;
    std::uint8_t Data4[8];
} GUID, IID, CLSID, UUID;
typedef const GUID& REFGUID;
typedef const IID& REFIID;
typedef const CLSID& REFCLSID;
inline bool operator==(const GUID& a, const GUID& b) noexcept { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID& a, const GUID& b) noexcept { return !(a == b); }
#define IsEqualGUID(a, b) ((a) == (b))
#define IsEqualIID(a, b) ((a) == (b))
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

typedef struct _LUID {
    DWORD LowPart;
    LONG HighPart;
} LUID, *PLUID;

typedef struct _POINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;
typedef struct _RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT* LPCRECT;
typedef struct _SIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        };
        PVOID Pointer;
    };
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

// Windows names the FILE_DISPOSITION_INFO member through this macro too
#define DeleteFile DeleteFileW

// ---- HRESULT --------------------------------------------------------------------
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define E_POINTER ((HRESULT)0x80004003L)
#define E_ABORT ((HRESULT)0x80004004L)
#define E_UNEXPECTED ((HRESULT)0x8000FFFFL)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_ACCESSDENIED ((HRESULT)0x80070005L)
#define E_HANDLE ((HRESULT)0x80070006L)
#define RPC_E_CHANGED_MODE ((HRESULT)0x80010106L)
#define HRESULT_FROM_WIN32(x) \
    ((HRESULT)(x) <= 0 ? ((HRESULT)(x)) : ((HRESULT)(((x)&0x0000FFFF) | (7 << 16) | 0x80000000)))
#define MAKE_HRESULT(sev, fac, code) \
    ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) | ((unsigned long)(code))))
#define HRESULT_CODE(hr) ((hr)&0xFFFF)

// ---- errors ---------------------------------------------------------------------
#define ERROR_SUCCESS 0L
#define NO_ERROR 0L
#define ERROR_INVALID_FUNCTION 1L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_TOO_MANY_OPEN_FILES 4L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_INVALID_DATA 13L
#define ERROR_OUTOFMEMORY 14L
#define ERROR_NO_MORE_FILES 18L
#define ERROR_NOT_READY 21L
#define ERROR_WRITE_FAULT 29L
#define ERROR_READ_FAULT 30L
#define ERROR_GEN_FAILURE 31L
#define ERROR_SHARING_VIOLATION 32L
#define ERROR_LOCK_VIOLATION 33L
#define ERROR_HANDLE_EOF 38L
#define ERROR_NOT_SUPPORTED 50L
#define ERROR_FILE_EXISTS 80L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_BROKEN_PIPE 109L
#define ERROR_DISK_FULL 112L
#define ERROR_CALL_NOT_IMPLEMENTED 120L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_INVALID_NAME 123L
#define ERROR_MOD_NOT_FOUND 126L
#define ERROR_PROC_NOT_FOUND 127L
#define ERROR_DIR_NOT_EMPTY 145L
#define ERROR_BUSY 170L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_ENVVAR_NOT_FOUND 203L
#define ERROR_MORE_DATA 234L
#define WAIT_TIMEOUT 258L
#define ERROR_NO_MORE_ITEMS 259L
#define ERROR_OPERATION_ABORTED 995L
#define ERROR_IO_INCOMPLETE 996L
#define ERROR_IO_PENDING 997L
#define ERROR_NOACCESS 998L
#define ERROR_DEVICE_NOT_CONNECTED 1167L
#define ERROR_NOT_FOUND 1168L
#define ERROR_CANCELLED 1223L
#define ERROR_TIMEOUT 1460L
#define ERROR_CRC 23L
#define ERROR_HANDLE_DISK_FULL 39L
#define ERROR_BAD_PATHNAME 161L
#define ERROR_FILE_INVALID 1006L
#define ERROR_PRIVILEGE_NOT_HELD 1314L
#define ERROR_INVALID_STATE 5023L
#define ERROR_DEVICE_REMOVED 1617L

DWORD GetLastError() noexcept;
void SetLastError(DWORD error) noexcept;

// ---- waiting --------------------------------------------------------------------
#define WAIT_OBJECT_0 0x00000000L
#define WAIT_ABANDONED 0x00000080L
#define WAIT_ABANDONED_0 WAIT_ABANDONED
#define WAIT_IO_COMPLETION 0x000000C0L
#define WAIT_FAILED 0xFFFFFFFFu
#define MAXIMUM_WAIT_OBJECTS 64
#define STATUS_WAIT_0 ((DWORD)0x00000000L)
#define STATUS_TIMEOUT ((DWORD)0x00000102L)

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds) noexcept;
DWORD WaitForSingleObjectEx(HANDLE handle, DWORD milliseconds, BOOL alertable) noexcept;
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds) noexcept;
DWORD WaitForMultipleObjectsEx(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD milliseconds,
                               BOOL alertable) noexcept;
BOOL CloseHandle(HANDLE handle) noexcept;
BOOL DuplicateHandle(HANDLE source_process, HANDLE source, HANDLE target_process, LPHANDLE target,
                     DWORD access, BOOL inherit, DWORD options) noexcept;
#define DUPLICATE_SAME_ACCESS 0x00000002
#define DUPLICATE_CLOSE_SOURCE 0x00000001

// Events
HANDLE CreateEventW(LPSECURITY_ATTRIBUTES attributes, BOOL manual_reset, BOOL initial_state, LPCWSTR name) noexcept;
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES attributes, BOOL manual_reset, BOOL initial_state, LPCSTR name) noexcept;
HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES attributes, LPCWSTR name, DWORD flags, DWORD access) noexcept;
#define CreateEvent CreateEventW
#define CREATE_EVENT_MANUAL_RESET 0x00000001
#define CREATE_EVENT_INITIAL_SET 0x00000002
#define EVENT_ALL_ACCESS 0x1F0003
#define EVENT_MODIFY_STATE 0x0002
#define SYNCHRONIZE 0x00100000L
BOOL SetEvent(HANDLE event) noexcept;
BOOL ResetEvent(HANDLE event) noexcept;

// Mutexes (named: the instance lease)
HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES attributes, BOOL initial_owner, LPCWSTR name) noexcept;
HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES attributes, LPCWSTR name, DWORD flags, DWORD access) noexcept;
BOOL ReleaseMutex(HANDLE mutex) noexcept;
#define CREATE_MUTEX_INITIAL_OWNER 0x00000001
#define MUTEX_ALL_ACCESS 0x1F0001

// Semaphores
HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES attributes, LONG initial, LONG maximum, LPCWSTR name) noexcept;
HANDLE CreateSemaphoreExW(LPSECURITY_ATTRIBUTES attributes, LONG initial, LONG maximum, LPCWSTR name, DWORD flags,
                          DWORD access) noexcept;
BOOL ReleaseSemaphore(HANDLE semaphore, LONG count, LPLONG previous) noexcept;
#define SEMAPHORE_ALL_ACCESS 0x1F0003

// Waitable timers (high resolution)
HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES attributes, BOOL manual_reset, LPCWSTR name) noexcept;
HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES attributes, LPCWSTR name, DWORD flags, DWORD access) noexcept;
typedef void(CALLBACK* PTIMERAPCROUTINE)(LPVOID, DWORD, DWORD);
BOOL SetWaitableTimer(HANDLE timer, const LARGE_INTEGER* due, LONG period, PTIMERAPCROUTINE routine, LPVOID argument,
                      BOOL resume) noexcept;
BOOL SetWaitableTimerEx(HANDLE timer, const LARGE_INTEGER* due, LONG period, PTIMERAPCROUTINE routine,
                        LPVOID argument, void* wake_context, ULONG tolerable_delay) noexcept;
BOOL CancelWaitableTimer(HANDLE timer) noexcept;
#define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x00000001
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#define TIMER_ALL_ACCESS 0x1F0003
#define TIMER_MODIFY_STATE 0x0002

// Slim reader/writer locks and condition variables
typedef struct _RTL_SRWLOCK {
    PVOID Ptr;
} SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT {0}
#define RTL_SRWLOCK_INIT {0}
void InitializeSRWLock(PSRWLOCK lock) noexcept;
void AcquireSRWLockExclusive(PSRWLOCK lock) noexcept;
void ReleaseSRWLockExclusive(PSRWLOCK lock) noexcept;
void AcquireSRWLockShared(PSRWLOCK lock) noexcept;
void ReleaseSRWLockShared(PSRWLOCK lock) noexcept;
BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK lock) noexcept;
BOOLEAN TryAcquireSRWLockShared(PSRWLOCK lock) noexcept;
typedef struct _RTL_CONDITION_VARIABLE {
    PVOID Ptr;
} CONDITION_VARIABLE, *PCONDITION_VARIABLE;
#define CONDITION_VARIABLE_INIT {0}
#define CONDITION_VARIABLE_LOCKMODE_SHARED 0x1
void InitializeConditionVariable(PCONDITION_VARIABLE variable) noexcept;
BOOL SleepConditionVariableSRW(PCONDITION_VARIABLE variable, PSRWLOCK lock, DWORD milliseconds, ULONG flags) noexcept;
void WakeConditionVariable(PCONDITION_VARIABLE variable) noexcept;
void WakeAllConditionVariable(PCONDITION_VARIABLE variable) noexcept;

typedef struct _RTL_CRITICAL_SECTION {
    PVOID impl;
} CRITICAL_SECTION, *LPCRITICAL_SECTION, *PCRITICAL_SECTION;
void InitializeCriticalSection(LPCRITICAL_SECTION section) noexcept;
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION section, DWORD spin) noexcept;
void DeleteCriticalSection(LPCRITICAL_SECTION section) noexcept;
void EnterCriticalSection(LPCRITICAL_SECTION section) noexcept;
void LeaveCriticalSection(LPCRITICAL_SECTION section) noexcept;
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION section) noexcept;

// WaitOnAddress / WakeByAddress
BOOL WaitOnAddress(volatile void* address, PVOID compare, SIZE_T size, DWORD milliseconds) noexcept;
void WakeByAddressSingle(PVOID address) noexcept;
void WakeByAddressAll(PVOID address) noexcept;

// One-time initialisation
typedef union _RTL_RUN_ONCE {
    PVOID Ptr;
} INIT_ONCE, *PINIT_ONCE, *LPINIT_ONCE;
#define INIT_ONCE_STATIC_INIT {0}
typedef BOOL(WINAPI* PINIT_ONCE_FN)(PINIT_ONCE, PVOID, PVOID*);
BOOL InitOnceExecuteOnce(PINIT_ONCE once, PINIT_ONCE_FN function, PVOID parameter, LPVOID* context) noexcept;

// Interlocked
inline LONG InterlockedIncrement(volatile LONG* value) noexcept { return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST); }
inline LONG InterlockedDecrement(volatile LONG* value) noexcept { return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST); }
inline LONG InterlockedExchange(volatile LONG* target, LONG value) noexcept {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}
inline LONG InterlockedCompareExchange(volatile LONG* target, LONG exchange, LONG comparand) noexcept {
    __atomic_compare_exchange_n(target, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
inline LONG InterlockedExchangeAdd(volatile LONG* target, LONG value) noexcept {
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}
inline LONG64 InterlockedIncrement64(volatile LONG64* value) noexcept {
    return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
}
inline LONG64 InterlockedExchange64(volatile LONG64* target, LONG64 value) noexcept {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}
inline LONG64 InterlockedCompareExchange64(volatile LONG64* target, LONG64 exchange, LONG64 comparand) noexcept {
    __atomic_compare_exchange_n(target, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
inline PVOID InterlockedCompareExchangePointer(PVOID volatile* target, PVOID exchange, PVOID comparand) noexcept {
    __atomic_compare_exchange_n(target, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
inline PVOID InterlockedExchangePointer(PVOID volatile* target, PVOID value) noexcept {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}
#define MemoryBarrier() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define YieldProcessor() _mm_pause()

// ---- threads and processes ------------------------------------------------------
typedef DWORD(WINAPI* LPTHREAD_START_ROUTINE)(LPVOID);
typedef LPTHREAD_START_ROUTINE PTHREAD_START_ROUTINE;
HANDLE CreateThread(LPSECURITY_ATTRIBUTES attributes, SIZE_T stack_size, LPTHREAD_START_ROUTINE start,
                    LPVOID parameter, DWORD flags, LPDWORD thread_id) noexcept;
#define CREATE_SUSPENDED 0x00000004
#define STACK_SIZE_PARAM_IS_A_RESERVATION 0x00010000
DWORD ResumeThread(HANDLE thread) noexcept;
DWORD SuspendThread(HANDLE thread) noexcept;
BOOL GetExitCodeThread(HANDLE thread, LPDWORD code) noexcept;
#define STILL_ACTIVE 259
HANDLE GetCurrentThread() noexcept;
DWORD GetCurrentThreadId() noexcept;
DWORD GetThreadId(HANDLE thread) noexcept;
HANDLE OpenThread(DWORD access, BOOL inherit, DWORD thread_id) noexcept;
#define THREAD_ALL_ACCESS 0x1FFFFF
#define THREAD_QUERY_INFORMATION 0x0040
#define THREAD_QUERY_LIMITED_INFORMATION 0x0800
#define THREAD_SUSPEND_RESUME 0x0002
#define THREAD_GET_CONTEXT 0x0008
#define THREAD_SET_INFORMATION 0x0020
#define THREAD_SET_LIMITED_INFORMATION 0x0400
HANDLE GetCurrentProcess() noexcept;
DWORD GetCurrentProcessId() noexcept;
DWORD GetProcessId(HANDLE process) noexcept;
[[noreturn]] void ExitProcess(UINT code) noexcept;
BOOL TerminateProcess(HANDLE process, UINT code) noexcept;
void Sleep(DWORD milliseconds) noexcept;
DWORD SleepEx(DWORD milliseconds, BOOL alertable) noexcept;
BOOL SwitchToThread() noexcept;
HRESULT SetThreadDescription(HANDLE thread, PCWSTR description) noexcept;
HRESULT GetThreadDescription(HANDLE thread, PWSTR* description) noexcept;
BOOL SetThreadPriority(HANDLE thread, int priority) noexcept;
int GetThreadPriority(HANDLE thread) noexcept;
BOOL SetThreadPriorityBoost(HANDLE thread, BOOL disable) noexcept;
#define THREAD_PRIORITY_IDLE -15
#define THREAD_PRIORITY_LOWEST -2
#define THREAD_PRIORITY_BELOW_NORMAL -1
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define THREAD_PRIORITY_ERROR_RETURN 0x7FFFFFFF
#define THREAD_MODE_BACKGROUND_BEGIN 0x00010000
#define THREAD_MODE_BACKGROUND_END 0x00020000
DWORD_PTR SetThreadAffinityMask(HANDLE thread, DWORD_PTR mask) noexcept;
BOOL GetProcessAffinityMask(HANDLE process, PDWORD_PTR process_mask, PDWORD_PTR system_mask) noexcept;
DWORD SetThreadIdealProcessor(HANDLE thread, DWORD processor) noexcept;
BOOL SetPriorityClass(HANDLE process, DWORD priority_class) noexcept;
DWORD GetPriorityClass(HANDLE process) noexcept;
#define NORMAL_PRIORITY_CLASS 0x00000020
#define ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
#define HIGH_PRIORITY_CLASS 0x00000080
#define REALTIME_PRIORITY_CLASS 0x00000100
#define BELOW_NORMAL_PRIORITY_CLASS 0x00004000
#define IDLE_PRIORITY_CLASS 0x00000040
BOOL QueryThreadCycleTime(HANDLE thread, PULONG64 cycles) noexcept;
BOOL QueryProcessCycleTime(HANDLE process, PULONG64 cycles) noexcept;
BOOL GetThreadTimes(HANDLE thread, LPFILETIME creation, LPFILETIME exit, LPFILETIME kernel, LPFILETIME user) noexcept;
BOOL GetProcessTimes(HANDLE process, LPFILETIME creation, LPFILETIME exit, LPFILETIME kernel, LPFILETIME user) noexcept;
BOOL IsProcessorFeaturePresent(DWORD feature) noexcept;
#define PF_AVX2_INSTRUCTIONS_AVAILABLE 40
#define PF_SSE3_INSTRUCTIONS_AVAILABLE 13
BOOL IsDebuggerPresent() noexcept;
void OutputDebugStringA(LPCSTR text) noexcept;
void OutputDebugStringW(LPCWSTR text) noexcept;
#define OutputDebugString OutputDebugStringW
void DebugBreak() noexcept;

typedef struct _PROCESSOR_NUMBER {
    WORD Group;
    BYTE Number;
    BYTE Reserved;
} PROCESSOR_NUMBER, *PPROCESSOR_NUMBER;
void GetCurrentProcessorNumberEx(PPROCESSOR_NUMBER number) noexcept;
DWORD GetCurrentProcessorNumber() noexcept;

typedef struct _SYSTEM_INFO {
    union {
        DWORD dwOemId;
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        };
    };
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;
void GetSystemInfo(LPSYSTEM_INFO info) noexcept;
void GetNativeSystemInfo(LPSYSTEM_INFO info) noexcept;

typedef struct _MEMORYSTATUSEX {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
    DWORDLONG ullTotalPageFile;
    DWORDLONG ullAvailPageFile;
    DWORDLONG ullTotalVirtual;
    DWORDLONG ullAvailVirtual;
    DWORDLONG ullAvailExtendedVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;
BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX status) noexcept;

// Process information classes (power throttling)
typedef enum _PROCESS_INFORMATION_CLASS {
    ProcessMemoryPriority,
    ProcessMemoryExhaustionInfo,
    ProcessAppMemoryInfo,
    ProcessInPrivateInfo,
    ProcessPowerThrottling,
    ProcessReservedValue1,
    ProcessTelemetryCoverageInfo,
    ProcessProtectionLevelInfo,
    ProcessLeapSecondInfo,
    ProcessMachineTypeInfo,
    ProcessInformationClassMax
} PROCESS_INFORMATION_CLASS;
typedef enum _THREAD_INFORMATION_CLASS {
    ThreadMemoryPriority,
    ThreadAbsoluteCpuPriority,
    ThreadDynamicCodePolicy,
    ThreadPowerThrottling,
    ThreadInformationClassMax
} THREAD_INFORMATION_CLASS;
typedef struct _PROCESS_POWER_THROTTLING_STATE {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
} PROCESS_POWER_THROTTLING_STATE, *PPROCESS_POWER_THROTTLING_STATE;
typedef struct _THREAD_POWER_THROTTLING_STATE {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
} THREAD_POWER_THROTTLING_STATE;
#define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#define THREAD_POWER_THROTTLING_CURRENT_VERSION 1
#define THREAD_POWER_THROTTLING_EXECUTION_SPEED 0x1
typedef struct _MEMORY_PRIORITY_INFORMATION {
    ULONG MemoryPriority;
} MEMORY_PRIORITY_INFORMATION;
#define MEMORY_PRIORITY_NORMAL 5
BOOL SetProcessInformation(HANDLE process, PROCESS_INFORMATION_CLASS info_class, LPVOID info, DWORD size) noexcept;
BOOL SetThreadInformation(HANDLE thread, THREAD_INFORMATION_CLASS info_class, LPVOID info, DWORD size) noexcept;
UINT SetErrorMode(UINT mode) noexcept;
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOGPFAULTERRORBOX 0x0002
#define SEM_NOOPENFILEERRORBOX 0x8000
BOOL SetProcessPriorityBoost(HANDLE process, BOOL disable) noexcept;
BOOL SetProcessWorkingSetSize(HANDLE process, SIZE_T minimum, SIZE_T maximum) noexcept;

// Fibers (the cooperative scheduler: one per guest thread)
typedef void(WINAPI* PFIBER_START_ROUTINE)(LPVOID);
typedef PFIBER_START_ROUTINE LPFIBER_START_ROUTINE;
#define FIBER_FLAG_FLOAT_SWITCH 0x1
LPVOID ConvertThreadToFiber(LPVOID parameter) noexcept;
LPVOID ConvertThreadToFiberEx(LPVOID parameter, DWORD flags) noexcept;
BOOL ConvertFiberToThread() noexcept;
LPVOID CreateFiber(SIZE_T stack_size, LPFIBER_START_ROUTINE start, LPVOID parameter) noexcept;
LPVOID CreateFiberEx(SIZE_T commit, SIZE_T reserve, DWORD flags, LPFIBER_START_ROUTINE start,
                     LPVOID parameter) noexcept;
void DeleteFiber(LPVOID fiber) noexcept;
void SwitchToFiber(LPVOID fiber) noexcept;
PVOID GetCurrentFiber() noexcept;
PVOID GetFiberData() noexcept;
BOOL IsThreadAFiber() noexcept;

// Thread-local storage
DWORD TlsAlloc() noexcept;
BOOL TlsFree(DWORD index) noexcept;
LPVOID TlsGetValue(DWORD index) noexcept;
BOOL TlsSetValue(DWORD index, LPVOID value) noexcept;
#define TLS_OUT_OF_INDEXES 0xFFFFFFFFu

// ---- time -----------------------------------------------------------------------
BOOL QueryPerformanceCounter(LARGE_INTEGER* count) noexcept;
BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency) noexcept;
DWORD GetTickCount() noexcept;
ULONGLONG GetTickCount64() noexcept;
void GetSystemTimeAsFileTime(LPFILETIME time) noexcept;
void GetSystemTimePreciseAsFileTime(LPFILETIME time) noexcept;
void GetLocalTime(LPSYSTEMTIME time) noexcept;
void GetSystemTime(LPSYSTEMTIME time) noexcept;
BOOL FileTimeToSystemTime(const FILETIME* file_time, LPSYSTEMTIME system_time) noexcept;
BOOL SystemTimeToFileTime(const SYSTEMTIME* system_time, LPFILETIME file_time) noexcept;
BOOL FileTimeToLocalFileTime(const FILETIME* file_time, LPFILETIME local) noexcept;
BOOL QueryUnbiasedInterruptTime(PULONGLONG time) noexcept;
void QueryInterruptTime(PULONGLONG time) noexcept;
void QueryInterruptTimePrecise(PULONGLONG time) noexcept;

// ---- files ----------------------------------------------------------------------
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define GENERIC_EXECUTE 0x20000000u
#define GENERIC_ALL 0x10000000u
#define DELETE 0x00010000L
#define READ_CONTROL 0x00020000L
#define FILE_READ_DATA 0x0001
#define FILE_WRITE_DATA 0x0002
#define FILE_APPEND_DATA 0x0004
#define FILE_READ_ATTRIBUTES 0x0080
#define FILE_WRITE_ATTRIBUTES 0x0100
#define FILE_GENERIC_READ 0x00120089
#define FILE_GENERIC_WRITE 0x00120116
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_HIDDEN 0x00000002
#define FILE_ATTRIBUTE_SYSTEM 0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE 0x00000020
#define FILE_ATTRIBUTE_DEVICE 0x00000040
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#define FILE_ATTRIBUTE_OFFLINE 0x00001000
#define FILE_FLAG_WRITE_THROUGH 0x80000000u
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define FILE_FLAG_RANDOM_ACCESS 0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE 0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_FLAG_OPEN_REPARSE_POINT 0x00200000
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED 0x00000002
#define MOVEFILE_WRITE_THROUGH 0x00000008
#define REPLACEFILE_WRITE_THROUGH 0x00000001
#define FILE_TYPE_UNKNOWN 0x0000
#define FILE_TYPE_DISK 0x0001
#define FILE_TYPE_CHAR 0x0002
#define FILE_TYPE_PIPE 0x0003

HANDLE CreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition,
                   DWORD flags, HANDLE template_file) noexcept;
HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition,
                   DWORD flags, HANDLE template_file) noexcept;
#define CreateFile CreateFileW
BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD bytes, LPDWORD read, LPOVERLAPPED overlapped) noexcept;
BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD bytes, LPDWORD written, LPOVERLAPPED overlapped) noexcept;
BOOL GetOverlappedResult(HANDLE file, LPOVERLAPPED overlapped, LPDWORD transferred, BOOL wait) noexcept;
BOOL GetOverlappedResultEx(HANDLE file, LPOVERLAPPED overlapped, LPDWORD transferred, DWORD milliseconds,
                           BOOL alertable) noexcept;
BOOL CancelIo(HANDLE file) noexcept;
BOOL CancelIoEx(HANDLE file, LPOVERLAPPED overlapped) noexcept;
BOOL FlushFileBuffers(HANDLE file) noexcept;
BOOL GetFileSizeEx(HANDLE file, PLARGE_INTEGER size) noexcept;
DWORD GetFileSize(HANDLE file, LPDWORD high) noexcept;
BOOL SetFilePointerEx(HANDLE file, LARGE_INTEGER distance, PLARGE_INTEGER position, DWORD method) noexcept;
DWORD SetFilePointer(HANDLE file, LONG distance, PLONG high, DWORD method) noexcept;
BOOL SetEndOfFile(HANDLE file) noexcept;
DWORD GetFileType(HANDLE file) noexcept;
BOOL SetFileTime(HANDLE file, const FILETIME* creation, const FILETIME* access, const FILETIME* write) noexcept;
BOOL GetFileTime(HANDLE file, LPFILETIME creation, LPFILETIME access, LPFILETIME write) noexcept;
DWORD GetFileAttributesW(LPCWSTR name) noexcept;
DWORD GetFileAttributesA(LPCSTR name) noexcept;
#define GetFileAttributes GetFileAttributesW
BOOL SetFileAttributesW(LPCWSTR name, DWORD attributes) noexcept;
typedef enum _GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard, GetFileExMaxInfoLevel } GET_FILEEX_INFO_LEVELS;
typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;
BOOL GetFileAttributesExW(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID information) noexcept;
typedef struct _BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;
BOOL GetFileInformationByHandle(HANDLE file, LPBY_HANDLE_FILE_INFORMATION information) noexcept;
typedef enum _FILE_INFO_BY_HANDLE_CLASS {
    FileBasicInfo,
    FileStandardInfo,
    FileNameInfo,
    FileRenameInfo,
    FileDispositionInfo,
    FileAllocationInfo,
    FileEndOfFileInfo,
    FileStreamInfo,
    FileCompressionInfo,
    FileAttributeTagInfo,
    FileIdBothDirectoryInfo,
    FileIdBothDirectoryRestartInfo,
    FileIoPriorityHintInfo,
    FileRemoteProtocolInfo,
    FileFullDirectoryInfo,
    FileFullDirectoryRestartInfo,
    FileStorageInfo,
    FileAlignmentInfo,
    FileIdInfo,
    FileIdExtdDirectoryInfo,
    FileIdExtdDirectoryRestartInfo,
    FileDispositionInfoEx,
    FileRenameInfoEx,
    FileCaseSensitiveInfo,
    FileNormalizedNameInfo,
    MaximumFileInfoByHandleClass
} FILE_INFO_BY_HANDLE_CLASS;
typedef struct _FILE_STANDARD_INFO {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    DWORD NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN Directory;
} FILE_STANDARD_INFO;
typedef struct _FILE_BASIC_INFO {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    DWORD FileAttributes;
} FILE_BASIC_INFO;
typedef struct _FILE_DISPOSITION_INFO {
    BOOLEAN DeleteFile;
} FILE_DISPOSITION_INFO;
typedef struct _FILE_DISPOSITION_INFO_EX {
    DWORD Flags;
} FILE_DISPOSITION_INFO_EX;
#define FILE_DISPOSITION_FLAG_DELETE 0x00000001
#define FILE_DISPOSITION_FLAG_POSIX_SEMANTICS 0x00000002
typedef struct _FILE_CASE_SENSITIVE_INFO {
    ULONG Flags;
} FILE_CASE_SENSITIVE_INFO;
#define FILE_CS_FLAG_CASE_SENSITIVE_DIR 0x00000001
#define FILE_NAMED_STREAMS 0x00040000
#define FILE_CASE_SENSITIVE_SEARCH 0x00000001
#define FILE_CASE_PRESERVED_NAMES 0x00000002
#define FILE_UNICODE_ON_DISK 0x00000004
#define FILE_SUPPORTS_HARD_LINKS 0x00400000
typedef struct _FILE_END_OF_FILE_INFO {
    LARGE_INTEGER EndOfFile;
} FILE_END_OF_FILE_INFO;
typedef struct _FILE_ID_128 {
    BYTE Identifier[16];
} FILE_ID_128;
typedef struct _FILE_ID_INFO {
    ULONGLONG VolumeSerialNumber;
    FILE_ID_128 FileId;
} FILE_ID_INFO;
typedef struct _FILE_RENAME_INFO {
    union {
        BOOLEAN ReplaceIfExists;
        DWORD Flags;
    };
    HANDLE RootDirectory;
    DWORD FileNameLength;
    WCHAR FileName[1];
} FILE_RENAME_INFO;
BOOL GetFileInformationByHandleEx(HANDLE file, FILE_INFO_BY_HANDLE_CLASS info_class, LPVOID information,
                                  DWORD size) noexcept;
BOOL SetFileInformationByHandle(HANDLE file, FILE_INFO_BY_HANDLE_CLASS info_class, LPVOID information,
                                DWORD size) noexcept;
DWORD GetFinalPathNameByHandleW(HANDLE file, LPWSTR path, DWORD count, DWORD flags) noexcept;
#define FILE_NAME_NORMALIZED 0x0
#define VOLUME_NAME_DOS 0x0
BOOL DeleteFileW(LPCWSTR name) noexcept;
BOOL DeleteFileA(LPCSTR name) noexcept;
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags) noexcept;
BOOL MoveFileW(LPCWSTR from, LPCWSTR to) noexcept;
BOOL ReplaceFileW(LPCWSTR replaced, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude,
                  LPVOID reserved) noexcept;
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL fail_if_exists) noexcept;
BOOL CreateHardLinkW(LPCWSTR link, LPCWSTR existing, LPSECURITY_ATTRIBUTES attributes) noexcept;
BOOL CreateDirectoryW(LPCWSTR name, LPSECURITY_ATTRIBUTES attributes) noexcept;
BOOL CreateDirectoryA(LPCSTR name, LPSECURITY_ATTRIBUTES attributes) noexcept;
#define CreateDirectory CreateDirectoryW
BOOL RemoveDirectoryW(LPCWSTR name) noexcept;
DWORD GetFullPathNameW(LPCWSTR name, DWORD count, LPWSTR buffer, LPWSTR* file_part) noexcept;
DWORD GetTempPathW(DWORD count, LPWSTR buffer) noexcept;
DWORD GetCurrentDirectoryW(DWORD count, LPWSTR buffer) noexcept;
BOOL SetCurrentDirectoryW(LPCWSTR path) noexcept;
BOOL GetVolumePathNameW(LPCWSTR name, LPWSTR volume, DWORD count) noexcept;
BOOL GetVolumeInformationW(LPCWSTR root, LPWSTR name, DWORD name_size, LPDWORD serial, LPDWORD max_component,
                           LPDWORD flags, LPWSTR file_system, DWORD file_system_size) noexcept;
UINT GetDriveTypeW(LPCWSTR root) noexcept;
#define DRIVE_FIXED 3
BOOL GetDiskFreeSpaceExW(LPCWSTR directory, PULARGE_INTEGER available, PULARGE_INTEGER total,
                         PULARGE_INTEGER free) noexcept;

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[MAX_PATH];
    WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data) noexcept;
BOOL FindNextFileW(HANDLE find, LPWIN32_FIND_DATAW data) noexcept;
BOOL FindClose(HANDLE find) noexcept;
#define FindFirstFile FindFirstFileW

// std::filesystem::path::c_str() is char (UTF-8) here, wchar_t on Windows:
// every W function that takes a path also takes a UTF-8 one.
HANDLE CreateFileW(const char* name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attributes, DWORD disposition,
                   DWORD flags, HANDLE template_file) noexcept;
DWORD GetFileAttributesW(const char* name) noexcept;
BOOL GetFileAttributesExW(const char* name, GET_FILEEX_INFO_LEVELS level, LPVOID information) noexcept;
BOOL SetFileAttributesW(const char* name, DWORD attributes) noexcept;
BOOL DeleteFileW(const char* name) noexcept;
BOOL MoveFileExW(const char* from, const char* to, DWORD flags) noexcept;
BOOL MoveFileW(const char* from, const char* to) noexcept;
BOOL ReplaceFileW(const char* replaced, const char* replacement, const char* backup, DWORD flags, LPVOID exclude,
                  LPVOID reserved) noexcept;
BOOL CopyFileW(const char* from, const char* to, BOOL fail_if_exists) noexcept;
BOOL CreateHardLinkW(const char* link, const char* existing, LPSECURITY_ATTRIBUTES attributes) noexcept;
BOOL CreateDirectoryW(const char* name, LPSECURITY_ATTRIBUTES attributes) noexcept;
BOOL RemoveDirectoryW(const char* name) noexcept;
HANDLE FindFirstFileW(const char* pattern, LPWIN32_FIND_DATAW data) noexcept;
BOOL GetVolumePathNameW(const char* name, LPWSTR volume, DWORD count) noexcept;
BOOL GetDiskFreeSpaceExW(const char* directory, PULARGE_INTEGER available, PULARGE_INTEGER total,
                         PULARGE_INTEGER free) noexcept;
#define FindNextFile FindNextFileW

// File mappings
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_GUARD 0x100
#define SEC_COMMIT 0x08000000
#define SEC_RESERVE 0x04000000
#define FILE_MAP_COPY 0x0001
#define FILE_MAP_WRITE 0x0002
#define FILE_MAP_READ 0x0004
#define FILE_MAP_ALL_ACCESS 0x000F001F
#define FILE_MAP_EXECUTE 0x0020
HANDLE CreateFileMappingW(HANDLE file, LPSECURITY_ATTRIBUTES attributes, DWORD protect, DWORD size_high,
                          DWORD size_low, LPCWSTR name) noexcept;
HANDLE CreateFileMappingA(HANDLE file, LPSECURITY_ATTRIBUTES attributes, DWORD protect, DWORD size_high,
                          DWORD size_low, LPCSTR name) noexcept;
LPVOID MapViewOfFile(HANDLE mapping, DWORD access, DWORD offset_high, DWORD offset_low, SIZE_T bytes) noexcept;
BOOL UnmapViewOfFile(LPCVOID view) noexcept;
BOOL FlushViewOfFile(LPCVOID view, SIZE_T bytes) noexcept;

// Virtual memory
#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define MEM_FREE 0x00010000
#define MEM_PRIVATE 0x00020000
#define MEM_MAPPED 0x00040000
#define MEM_RESET 0x00080000
#define MEM_TOP_DOWN 0x00100000
#define MEM_LARGE_PAGES 0x20000000
#define MEM_IMAGE 0x01000000
LPVOID VirtualAlloc(LPVOID address, SIZE_T size, DWORD type, DWORD protect) noexcept;
BOOL VirtualFree(LPVOID address, SIZE_T size, DWORD type) noexcept;
BOOL VirtualProtect(LPVOID address, SIZE_T size, DWORD protect, PDWORD old_protect) noexcept;
BOOL VirtualLock(LPVOID address, SIZE_T size) noexcept;
BOOL VirtualUnlock(LPVOID address, SIZE_T size) noexcept;
typedef struct _MEMORY_BASIC_INFORMATION {
    PVOID BaseAddress;
    PVOID AllocationBase;
    DWORD AllocationProtect;
    WORD PartitionId;
    SIZE_T RegionSize;
    DWORD State;
    DWORD Protect;
    DWORD Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;
SIZE_T VirtualQuery(LPCVOID address, PMEMORY_BASIC_INFORMATION buffer, SIZE_T length) noexcept;
typedef struct _WIN32_MEMORY_RANGE_ENTRY {
    PVOID VirtualAddress;
    SIZE_T NumberOfBytes;
} WIN32_MEMORY_RANGE_ENTRY, *PWIN32_MEMORY_RANGE_ENTRY;
BOOL PrefetchVirtualMemory(HANDLE process, ULONG_PTR count, PWIN32_MEMORY_RANGE_ENTRY entries, ULONG flags) noexcept;
HANDLE GetProcessHeap() noexcept;
LPVOID HeapAlloc(HANDLE heap, DWORD flags, SIZE_T bytes) noexcept;
BOOL HeapFree(HANDLE heap, DWORD flags, LPVOID memory) noexcept;
LPVOID HeapReAlloc(HANDLE heap, DWORD flags, LPVOID memory, SIZE_T bytes) noexcept;
#define HEAP_ZERO_MEMORY 0x00000008
HLOCAL LocalFree(HLOCAL memory) noexcept;
HLOCAL LocalAlloc(UINT flags, SIZE_T bytes) noexcept;
#define LMEM_FIXED 0x0000
#define LMEM_ZEROINIT 0x0040
#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)
#define ZeroMemory(destination, length) std::memset((destination), 0, (length))
#define SecureZeroMemory(destination, length) std::memset((destination), 0, (length))
#define CopyMemory(destination, source, length) std::memcpy((destination), (source), (length))
#define FillMemory(destination, length, fill) std::memset((destination), (fill), (length))
#define RtlZeroMemory ZeroMemory
#define RtlCopyMemory CopyMemory

// ---- modules ----------------------------------------------------------------------
// The console has no DLLs: the game, Home Menu and DSP modules are linked into
// the eboot, and LoadLibrary finds them by name in a table (ps5/compat/src/modules.cpp).
HMODULE LoadLibraryW(LPCWSTR name) noexcept;
HMODULE LoadLibraryA(LPCSTR name) noexcept;
HMODULE LoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags) noexcept;
HMODULE LoadLibraryExA(LPCSTR name, HANDLE file, DWORD flags) noexcept;
HMODULE LoadLibraryExW(const char* name, HANDLE file, DWORD flags) noexcept;
HMODULE LoadLibraryW(const char* name) noexcept;
#define LoadLibrary LoadLibraryW
#define LoadLibraryEx LoadLibraryExW
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x00000100
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x00000008
#define DONT_RESOLVE_DLL_REFERENCES 0x00000001
#define LOAD_LIBRARY_AS_DATAFILE 0x00000002
FARPROC GetProcAddress(HMODULE module, LPCSTR name) noexcept;
BOOL FreeLibrary(HMODULE module) noexcept;
HMODULE GetModuleHandleW(LPCWSTR name) noexcept;
HMODULE GetModuleHandleA(LPCSTR name) noexcept;
#define GetModuleHandle GetModuleHandleW
BOOL GetModuleHandleExW(DWORD flags, LPCWSTR name, HMODULE* module) noexcept;
#define GET_MODULE_HANDLE_EX_FLAG_PIN 0x00000001
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x00000002
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x00000004
DWORD GetModuleFileNameW(HMODULE module, LPWSTR name, DWORD size) noexcept;
DWORD GetModuleFileNameA(HMODULE module, LPSTR name, DWORD size) noexcept;
#define GetModuleFileName GetModuleFileNameW
BOOL SetDllDirectoryW(LPCWSTR path) noexcept;
BOOL SetDefaultDllDirectories(DWORD flags) noexcept;
void* AddDllDirectory(PCWSTR path) noexcept;

// ---- environment and command line ---------------------------------------------------
DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD size) noexcept;
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size) noexcept;
#define GetEnvironmentVariable GetEnvironmentVariableW
BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value) noexcept;
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) noexcept;
#define SetEnvironmentVariable SetEnvironmentVariableW
DWORD ExpandEnvironmentStringsW(LPCWSTR source, LPWSTR destination, DWORD size) noexcept;
LPWSTR GetCommandLineW() noexcept;
LPWSTR* CommandLineToArgvW(LPCWSTR command_line, int* count) noexcept;
LPWCH GetEnvironmentStringsW() noexcept;
BOOL FreeEnvironmentStringsW(LPWCH block) noexcept;

// ---- strings ---------------------------------------------------------------------
#define CP_ACP 0
#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 0x00000008
#define WC_ERR_INVALID_CHARS 0x00000080
int MultiByteToWideChar(UINT code_page, DWORD flags, LPCSTR source, int source_count, LPWSTR destination,
                        int destination_count) noexcept;
int WideCharToMultiByte(UINT code_page, DWORD flags, LPCWSTR source, int source_count, LPSTR destination,
                        int destination_count, LPCSTR default_char, LPBOOL used_default) noexcept;
int lstrlenW(LPCWSTR text) noexcept;
int lstrcmpiW(LPCWSTR a, LPCWSTR b) noexcept;
int lstrcmpW(LPCWSTR a, LPCWSTR b) noexcept;
int _wcsicmp(const wchar_t* a, const wchar_t* b) noexcept;
int _wcsnicmp(const wchar_t* a, const wchar_t* b, std::size_t count) noexcept;
int _stricmp(const char* a, const char* b) noexcept;
int _strnicmp(const char* a, const char* b, std::size_t count) noexcept;
#define _strdup strdup
#define _wcsdup wcsdup
errno_t wcscpy_s(wchar_t* destination, std::size_t size, const wchar_t* source) noexcept;
errno_t wcsncpy_s(wchar_t* destination, std::size_t size, const wchar_t* source, std::size_t count) noexcept;
errno_t wcscat_s(wchar_t* destination, std::size_t size, const wchar_t* source) noexcept;
errno_t strcpy_s(char* destination, std::size_t size, const char* source) noexcept;
errno_t strncpy_s(char* destination, std::size_t size, const char* source, std::size_t count) noexcept;
errno_t strcat_s(char* destination, std::size_t size, const char* source) noexcept;
template <std::size_t N>
inline errno_t wcscpy_s(wchar_t (&destination)[N], const wchar_t* source) noexcept {
    return wcscpy_s(destination, N, source);
}
template <std::size_t N>
inline errno_t strcpy_s(char (&destination)[N], const char* source) noexcept {
    return strcpy_s(destination, N, source);
}
template <std::size_t N>
inline errno_t strcat_s(char (&destination)[N], const char* source) noexcept {
    return strcat_s(destination, N, source);
}
template <std::size_t N>
inline errno_t wcsncpy_s(wchar_t (&destination)[N], const wchar_t* source, std::size_t count) noexcept {
    return wcsncpy_s(destination, N, source, count);
}
template <std::size_t N>
inline errno_t wcsncpy_s(wchar_t (&destination)[N], const char* source, std::size_t count) noexcept {
    wchar_t* const end = destination + N - 1;
    wchar_t* out = destination;
    for (; *source != '\0' && out < end && count != 0; ++source, --count) *out++ = static_cast<unsigned char>(*source);
    *out = L'\0';
    return 0;
}
template <std::size_t N>
inline errno_t strncpy_s(char (&destination)[N], const char* source, std::size_t count) noexcept {
    return strncpy_s(destination, N, source, count);
}
template <std::size_t N>
inline errno_t wcscat_s(wchar_t (&destination)[N], const wchar_t* source) noexcept {
    return wcscat_s(destination, N, source);
}
errno_t memcpy_s(void* destination, std::size_t size, const void* source, std::size_t count) noexcept;
errno_t memmove_s(void* destination, std::size_t size, const void* source, std::size_t count) noexcept;
#define _TRUNCATE ((std::size_t)-1)
// The secure printf family, in both its forms: (buffer, size, format, ...) and,
// for an array, (buffer, format, ...).
template <typename... Arguments>
inline int sprintf_s(char* buffer, std::size_t size, const char* format, Arguments... arguments) noexcept {
    return std::snprintf(buffer, size, format, arguments...);
}
template <std::size_t N, typename... Arguments>
inline int sprintf_s(char (&buffer)[N], const char* format, Arguments... arguments) noexcept {
    return std::snprintf(buffer, N, format, arguments...);
}
template <typename... Arguments>
inline int swprintf_s(wchar_t* buffer, std::size_t size, const wchar_t* format, Arguments... arguments) noexcept {
    return std::swprintf(buffer, size, format, arguments...);
}
template <std::size_t N, typename... Arguments>
inline int swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, Arguments... arguments) noexcept {
    return std::swprintf(buffer, N, format, arguments...);
}
#define _snprintf snprintf
#define _snprintf_s(buffer, size, count, ...) snprintf((buffer), (size), __VA_ARGS__)
#define _snwprintf swprintf
#define _snwprintf_s(buffer, size, count, ...) swprintf((buffer), (size), __VA_ARGS__)
#define vsprintf_s vsnprintf
#define _vsnprintf vsnprintf
#define sscanf_s sscanf
#define swscanf_s swscanf
#define _wtoi(s) ((int)wcstol((s), nullptr, 10))
#define _wtoi64(s) ((long long)wcstoll((s), nullptr, 10))
#define _atoi64(s) atoll(s)
#define _strtoui64 strtoull
#define _strtoi64 strtoll
#define _wcstoui64 wcstoull
#define _wcstoi64 wcstoll
#define _O_BINARY 0x8000
#define _O_TEXT 0x4000
#define _O_U8TEXT 0x40000
#define _O_U16TEXT 0x20000
#define _SH_DENYNO 0x40
#define _SH_DENYWR 0x20
#define _SH_DENYRW 0x10
#define _fseeki64 fseeko
#define _ftelli64 ftello

// ---- crash handling -----------------------------------------------------------------
#define EXCEPTION_MAXIMUM_PARAMETERS 15
#define EXCEPTION_ACCESS_VIOLATION ((DWORD)0xC0000005L)
#define EXCEPTION_DATATYPE_MISALIGNMENT ((DWORD)0x80000002L)
#define EXCEPTION_BREAKPOINT ((DWORD)0x80000003L)
#define EXCEPTION_SINGLE_STEP ((DWORD)0x80000004L)
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED ((DWORD)0xC000008CL)
#define EXCEPTION_FLT_DENORMAL_OPERAND ((DWORD)0xC000008DL)
#define EXCEPTION_FLT_DIVIDE_BY_ZERO ((DWORD)0xC000008EL)
#define EXCEPTION_FLT_INEXACT_RESULT ((DWORD)0xC000008FL)
#define EXCEPTION_FLT_INVALID_OPERATION ((DWORD)0xC0000090L)
#define EXCEPTION_FLT_OVERFLOW ((DWORD)0xC0000091L)
#define EXCEPTION_FLT_STACK_CHECK ((DWORD)0xC0000092L)
#define EXCEPTION_FLT_UNDERFLOW ((DWORD)0xC0000093L)
#define EXCEPTION_INT_DIVIDE_BY_ZERO ((DWORD)0xC0000094L)
#define EXCEPTION_INT_OVERFLOW ((DWORD)0xC0000095L)
#define EXCEPTION_PRIV_INSTRUCTION ((DWORD)0xC0000096L)
#define EXCEPTION_IN_PAGE_ERROR ((DWORD)0xC0000006L)
#define EXCEPTION_ILLEGAL_INSTRUCTION ((DWORD)0xC000001DL)
#define EXCEPTION_NONCONTINUABLE_EXCEPTION ((DWORD)0xC0000025L)
#define EXCEPTION_STACK_OVERFLOW ((DWORD)0xC00000FDL)
#define EXCEPTION_INVALID_DISPOSITION ((DWORD)0xC0000026L)
#define EXCEPTION_GUARD_PAGE ((DWORD)0x80000001L)
#define EXCEPTION_INVALID_HANDLE ((DWORD)0xC0000008L)
#define STATUS_STACK_BUFFER_OVERRUN ((DWORD)0xC0000409L)
#define STATUS_HEAP_CORRUPTION ((DWORD)0xC0000374L)
#define STATUS_FAIL_FAST_EXCEPTION ((DWORD)0xC0000602L)
#define STATUS_ACCESS_VIOLATION EXCEPTION_ACCESS_VIOLATION
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_NONCONTINUABLE 0x1

typedef struct alignas(16) _M128A {
    ULONGLONG Low;
    LONGLONG High;
} M128A, *PM128A;

// The registers a handler reads and writes, in the order Windows has them.
// ps5/compat/src/exceptions.cpp fills it from the signal's ucontext and puts
// the changes back.
typedef struct alignas(16) _CONTEXT {
    DWORD64 P1Home, P2Home, P3Home, P4Home, P5Home, P6Home;
    DWORD ContextFlags;
    DWORD MxCsr;
    WORD SegCs, SegDs, SegEs, SegFs, SegGs, SegSs;
    DWORD EFlags;
    DWORD64 Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    DWORD64 Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi;
    DWORD64 R8, R9, R10, R11, R12, R13, R14, R15;
    DWORD64 Rip;
    union {
        struct {
            M128A Header[2];
            M128A Legacy[8];
            M128A Xmm0, Xmm1, Xmm2, Xmm3, Xmm4, Xmm5, Xmm6, Xmm7;
            M128A Xmm8, Xmm9, Xmm10, Xmm11, Xmm12, Xmm13, Xmm14, Xmm15;
        };
        BYTE FltSave[512];
    };
    M128A VectorRegister[26];
    DWORD64 VectorControl;
    DWORD64 DebugControl;
    DWORD64 LastBranchToRip;
    DWORD64 LastBranchFromRip;
    DWORD64 LastExceptionToRip;
    DWORD64 LastExceptionFromRip;
} CONTEXT, *PCONTEXT, *LPCONTEXT;
#define CONTEXT_AMD64 0x00100000L
#define CONTEXT_CONTROL (CONTEXT_AMD64 | 0x00000001L)
#define CONTEXT_INTEGER (CONTEXT_AMD64 | 0x00000002L)
#define CONTEXT_SEGMENTS (CONTEXT_AMD64 | 0x00000004L)
#define CONTEXT_FLOATING_POINT (CONTEXT_AMD64 | 0x00000008L)
#define CONTEXT_DEBUG_REGISTERS (CONTEXT_AMD64 | 0x00000010L)
#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT)
#define CONTEXT_ALL (CONTEXT_FULL | CONTEXT_SEGMENTS | CONTEXT_DEBUG_REGISTERS)

typedef struct _EXCEPTION_RECORD {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    struct _EXCEPTION_RECORD* ExceptionRecord;
    PVOID ExceptionAddress;
    DWORD NumberParameters;
    ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;
typedef struct _EXCEPTION_POINTERS {
    PEXCEPTION_RECORD ExceptionRecord;
    PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;
typedef LONG(WINAPI* PVECTORED_EXCEPTION_HANDLER)(PEXCEPTION_POINTERS);
typedef LONG(WINAPI* PTOP_LEVEL_EXCEPTION_FILTER)(PEXCEPTION_POINTERS);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;
PVOID AddVectoredExceptionHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler) noexcept;
ULONG RemoveVectoredExceptionHandler(PVOID handle) noexcept;
PVOID AddVectoredContinueHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler) noexcept;
ULONG RemoveVectoredContinueHandler(PVOID handle) noexcept;
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter) noexcept;
LONG UnhandledExceptionFilter(PEXCEPTION_POINTERS pointers) noexcept;
void RaiseException(DWORD code, DWORD flags, DWORD count, const ULONG_PTR* arguments) noexcept;
void RaiseFailFastException(PEXCEPTION_RECORD record, PCONTEXT context, DWORD flags) noexcept;
void RtlCaptureContext(PCONTEXT context) noexcept;
WORD RtlCaptureStackBackTrace(DWORD skip, DWORD count, PVOID* trace, PDWORD hash) noexcept;
#define CaptureStackBackTrace RtlCaptureStackBackTrace
typedef struct _RUNTIME_FUNCTION {
    DWORD BeginAddress;
    DWORD EndAddress;
    DWORD UnwindData;
} RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;
typedef struct _UNWIND_HISTORY_TABLE {
    DWORD Count;
} UNWIND_HISTORY_TABLE, *PUNWIND_HISTORY_TABLE;
typedef struct _KNONVOLATILE_CONTEXT_POINTERS {
    PDWORD64 IntegerContext[16];
} KNONVOLATILE_CONTEXT_POINTERS, *PKNONVOLATILE_CONTEXT_POINTERS;
typedef enum _EXCEPTION_DISPOSITION {
    ExceptionContinueExecution,
    ExceptionContinueSearch,
    ExceptionNestedException,
    ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;
typedef EXCEPTION_DISPOSITION (*PEXCEPTION_ROUTINE)(PEXCEPTION_RECORD, PVOID, PCONTEXT, PVOID);
#define UNW_FLAG_NHANDLER 0x0
#define UNW_FLAG_EHANDLER 0x1
#define UNW_FLAG_UHANDLER 0x2
PRUNTIME_FUNCTION RtlLookupFunctionEntry(DWORD64 pc, PDWORD64 image_base, PUNWIND_HISTORY_TABLE history) noexcept;
PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD handler_type, DWORD64 image_base, DWORD64 pc, PRUNTIME_FUNCTION entry,
                                    PCONTEXT context, PVOID* handler_data, PDWORD64 frame,
                                    PKNONVOLATILE_CONTEXT_POINTERS pointers) noexcept;
BOOL GetThreadContext(HANDLE thread, LPCONTEXT context) noexcept;
BOOL SetThreadContext(HANDLE thread, const CONTEXT* context) noexcept;
BOOL FlushInstructionCache(HANDLE process, LPCVOID address, SIZE_T size) noexcept;

// ---- power and the display ------------------------------------------------------------
typedef DWORD EXECUTION_STATE;
#define ES_SYSTEM_REQUIRED 0x00000001
#define ES_DISPLAY_REQUIRED 0x00000002
#define ES_AWAYMODE_REQUIRED 0x00000040
#define ES_CONTINUOUS 0x80000000u
EXECUTION_STATE SetThreadExecutionState(EXECUTION_STATE state) noexcept;
int GetSystemMetrics(int index) noexcept;
#define SM_CXSCREEN 0
#define SM_CXICON 11
#define SM_CYICON 12
#define SM_CXSMICON 49
#define SM_CYSMICON 50
#define SM_CYSCREEN 1
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79
#define SM_XVIRTUALSCREEN 76
#define SM_YVIRTUALSCREEN 77
#define SM_CMONITORS 80
#define SM_REMOTESESSION 0x1000
#define SM_CXDOUBLECLK 36
#define SM_CYDOUBLECLK 37
#define SM_SWAPBUTTON 23
#define SM_MOUSEPRESENT 19
#define SM_DIGITIZER 94
#define SM_MAXIMUMTOUCHES 95
#define SM_CXCURSOR 13
#define SM_CYCURSOR 14

#include "win32_window.h"

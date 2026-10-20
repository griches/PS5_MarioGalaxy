// Super Mario Galaxy for the PlayStation 5 - symbols and minidumps: the console has
// neither, and each call fails (the crash report is klog's text).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#define SYMOPT_UNDNAME 0x00000002
#define SYMOPT_DEFERRED_LOADS 0x00000004
#define SYMOPT_NO_PROMPTS 0x00080000
#define SYMOPT_LOAD_LINES 0x00000010
#define MAX_SYM_NAME 2000
typedef struct _SYMBOL_INFO {
    ULONG SizeOfStruct;
    ULONG TypeIndex;
    ULONG64 Reserved[2];
    ULONG Index;
    ULONG Size;
    ULONG64 ModBase;
    ULONG Flags;
    ULONG64 Value;
    ULONG64 Address;
    ULONG Register;
    ULONG Scope;
    ULONG Tag;
    ULONG NameLen;
    ULONG MaxNameLen;
    CHAR Name[1];
} SYMBOL_INFO, *PSYMBOL_INFO;
typedef struct _IMAGEHLP_LINE64 {
    DWORD SizeOfStruct;
    PVOID Key;
    DWORD LineNumber;
    PCHAR FileName;
    DWORD64 Address;
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;
DWORD SymSetOptions(DWORD options) noexcept;
DWORD SymGetOptions() noexcept;
BOOL SymInitialize(HANDLE process, PCSTR search_path, BOOL invade) noexcept;
BOOL SymInitializeW(HANDLE process, PCWSTR search_path, BOOL invade) noexcept;
BOOL SymCleanup(HANDLE process) noexcept;
BOOL SymFromAddr(HANDLE process, DWORD64 address, PDWORD64 displacement, PSYMBOL_INFO symbol) noexcept;
BOOL SymGetLineFromAddr64(HANDLE process, DWORD64 address, PDWORD displacement, PIMAGEHLP_LINE64 line) noexcept;
DWORD64 SymGetModuleBase64(HANDLE process, DWORD64 address) noexcept;
PVOID SymFunctionTableAccess64(HANDLE process, DWORD64 address) noexcept;
typedef enum _MINIDUMP_TYPE {
    MiniDumpNormal = 0x00000000,
    MiniDumpWithDataSegs = 0x00000001,
    MiniDumpWithFullMemory = 0x00000002,
    MiniDumpWithHandleData = 0x00000004,
    MiniDumpWithUnloadedModules = 0x00000020,
    MiniDumpWithIndirectlyReferencedMemory = 0x00000040,
    MiniDumpWithProcessThreadData = 0x00000100,
    MiniDumpWithFullMemoryInfo = 0x00000800,
    MiniDumpWithThreadInfo = 0x00001000
} MINIDUMP_TYPE;
typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
    DWORD ThreadId;
    PEXCEPTION_POINTERS ExceptionPointers;
    BOOL ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;
BOOL MiniDumpWriteDump(HANDLE process, DWORD process_id, HANDLE file, MINIDUMP_TYPE type,
                       PMINIDUMP_EXCEPTION_INFORMATION exception, PVOID user_stream, PVOID callback) noexcept;

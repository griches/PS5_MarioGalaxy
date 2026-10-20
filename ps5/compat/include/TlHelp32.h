// Super Mario Galaxy for the PlayStation 5 - thread snapshots (the calling thread only).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#define TH32CS_SNAPTHREAD 0x00000004
#define TH32CS_SNAPPROCESS 0x00000002
typedef struct tagTHREADENTRY32 {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ThreadID;
    DWORD th32OwnerProcessID;
    LONG tpBasePri;
    LONG tpDeltaPri;
    DWORD dwFlags;
} THREADENTRY32, *PTHREADENTRY32, *LPTHREADENTRY32;
HANDLE CreateToolhelp32Snapshot(DWORD flags, DWORD process) noexcept;
BOOL Thread32First(HANDLE snapshot, LPTHREADENTRY32 entry) noexcept;
BOOL Thread32Next(HANDLE snapshot, LPTHREADENTRY32 entry) noexcept;

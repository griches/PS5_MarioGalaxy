// Super Mario Galaxy for the PlayStation 5 - process memory counters.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD cb;
    DWORD PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS, *PPROCESS_MEMORY_COUNTERS;
typedef struct _PROCESS_MEMORY_COUNTERS_EX : _PROCESS_MEMORY_COUNTERS {
    SIZE_T PrivateUsage;
} PROCESS_MEMORY_COUNTERS_EX, *PPROCESS_MEMORY_COUNTERS_EX;
BOOL GetProcessMemoryInfo(HANDLE process, PPROCESS_MEMORY_COUNTERS counters, DWORD size) noexcept;

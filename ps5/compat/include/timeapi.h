// Super Mario Galaxy for the PlayStation 5 - the multimedia timer: the console's
// sleeps are already fine-grained, so the period calls do nothing.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef UINT MMRESULT;
#define TIMERR_NOERROR 0
#define TIMERR_NOCANDO 97
typedef struct timecaps_tag {
    UINT wPeriodMin;
    UINT wPeriodMax;
} TIMECAPS, *LPTIMECAPS;
MMRESULT timeBeginPeriod(UINT period) noexcept;
MMRESULT timeEndPeriod(UINT period) noexcept;
MMRESULT timeGetDevCaps(LPTIMECAPS caps, UINT size) noexcept;
DWORD timeGetTime() noexcept;

// Super Mario Galaxy for the PlayStation 5 - the desktop compositor (none).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef ULONGLONG QPC_TIME;
typedef struct _UNSIGNED_RATIO {
    UINT32 uiNumerator;
    UINT32 uiDenominator;
} UNSIGNED_RATIO;
typedef struct _DWM_TIMING_INFO {
    UINT32 cbSize;
    UNSIGNED_RATIO rateRefresh;
    QPC_TIME qpcRefreshPeriod;
    UNSIGNED_RATIO rateCompose;
    QPC_TIME qpcVBlank;
    ULONGLONG cRefresh;
    UINT cDXRefresh;
    QPC_TIME qpcCompose;
    ULONGLONG cFrame;
    UINT cDXPresent;
    ULONGLONG cRefreshFrame;
    ULONGLONG cFrameSubmitted;
    UINT cDXPresentSubmitted;
    ULONGLONG cFrameConfirmed;
    UINT cDXPresentConfirmed;
    ULONGLONG cRefreshConfirmed;
    UINT cDXRefreshConfirmed;
    ULONGLONG cFramesLate;
    UINT cFramesOutstanding;
    ULONGLONG cFrameDisplayed;
    QPC_TIME qpcFrameDisplayed;
    ULONGLONG cRefreshFrameDisplayed;
    ULONGLONG cFrameComplete;
    QPC_TIME qpcFrameComplete;
    ULONGLONG cFramePending;
    QPC_TIME qpcFramePending;
    ULONGLONG cFramesDisplayed;
    ULONGLONG cFramesComplete;
    ULONGLONG cFramesPending;
    ULONGLONG cFramesAvailable;
    ULONGLONG cFramesDropped;
    ULONGLONG cFramesMissed;
    ULONGLONG cRefreshNextDisplayed;
    ULONGLONG cRefreshNextPresented;
    ULONGLONG cRefreshesDisplayed;
    ULONGLONG cRefreshesPresented;
    ULONGLONG cRefreshStarted;
    ULONGLONG cPixelsReceived;
    ULONGLONG cPixelsDrawn;
    ULONGLONG cBuffersEmpty;
} DWM_TIMING_INFO;
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
HRESULT DwmFlush() noexcept;
HRESULT DwmGetCompositionTimingInfo(HWND window, DWM_TIMING_INFO* info) noexcept;
HRESULT DwmSetWindowAttribute(HWND window, DWORD attribute, LPCVOID value, DWORD size) noexcept;
HRESULT DwmIsCompositionEnabled(BOOL* enabled) noexcept;

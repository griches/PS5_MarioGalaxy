// Super Mario Galaxy for the PlayStation 5 - HID report parsing (no devices).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef struct _HIDP_PREPARSED_DATA* PHIDP_PREPARSED_DATA;
typedef USHORT USAGE;
typedef struct _HIDP_CAPS {
    USAGE Usage;
    USAGE UsagePage;
    USHORT InputReportByteLength;
    USHORT OutputReportByteLength;
    USHORT FeatureReportByteLength;
    USHORT Reserved[17];
    USHORT NumberLinkCollectionNodes;
    USHORT NumberInputButtonCaps;
    USHORT NumberInputValueCaps;
    USHORT NumberInputDataIndices;
    USHORT NumberOutputButtonCaps;
    USHORT NumberOutputValueCaps;
    USHORT NumberOutputDataIndices;
    USHORT NumberFeatureButtonCaps;
    USHORT NumberFeatureValueCaps;
    USHORT NumberFeatureDataIndices;
} HIDP_CAPS, *PHIDP_CAPS;
#define HIDP_STATUS_SUCCESS ((NTSTATUS)0x00110000)
NTSTATUS HidP_GetCaps(PHIDP_PREPARSED_DATA data, PHIDP_CAPS caps) noexcept;

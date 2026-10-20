// Super Mario Galaxy for the PlayStation 5 - HID devices (none on the console).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "hidpi.h"
typedef struct _HIDD_ATTRIBUTES {
    ULONG Size;
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;
void HidD_GetHidGuid(GUID* guid) noexcept;
BOOLEAN HidD_GetAttributes(HANDLE device, PHIDD_ATTRIBUTES attributes) noexcept;
BOOLEAN HidD_GetPreparsedData(HANDLE device, PHIDP_PREPARSED_DATA* data) noexcept;
BOOLEAN HidD_FreePreparsedData(PHIDP_PREPARSED_DATA data) noexcept;
BOOLEAN HidD_SetOutputReport(HANDLE device, PVOID buffer, ULONG length) noexcept;
BOOLEAN HidD_GetInputReport(HANDLE device, PVOID buffer, ULONG length) noexcept;
BOOLEAN HidD_SetFeature(HANDLE device, PVOID buffer, ULONG length) noexcept;
BOOLEAN HidD_GetFeature(HANDLE device, PVOID buffer, ULONG length) noexcept;
BOOLEAN HidD_SetNumInputBuffers(HANDLE device, ULONG count) noexcept;
BOOLEAN HidD_GetProductString(HANDLE device, PVOID buffer, ULONG length) noexcept;
BOOLEAN HidD_GetSerialNumberString(HANDLE device, PVOID buffer, ULONG length) noexcept;
BOOLEAN HidD_GetManufacturerString(HANDLE device, PVOID buffer, ULONG length) noexcept;

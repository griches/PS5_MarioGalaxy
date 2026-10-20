// Super Mario Galaxy for the PlayStation 5 - device enumeration, which finds no
// devices: the console's pad is read through XInput, not as a HID device.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef PVOID HDEVINFO;
typedef struct _SP_DEVINFO_DATA {
    DWORD cbSize;
    GUID ClassGuid;
    DWORD DevInst;
    ULONG_PTR Reserved;
} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;
typedef struct _SP_DEVICE_INTERFACE_DATA {
    DWORD cbSize;
    GUID InterfaceClassGuid;
    DWORD Flags;
    ULONG_PTR Reserved;
} SP_DEVICE_INTERFACE_DATA, *PSP_DEVICE_INTERFACE_DATA;
typedef struct _SP_DEVICE_INTERFACE_DETAIL_DATA_W {
    DWORD cbSize;
    WCHAR DevicePath[1];
} SP_DEVICE_INTERFACE_DETAIL_DATA_W, *PSP_DEVICE_INTERFACE_DETAIL_DATA_W;
typedef struct _SP_DEVINFO_LIST_DETAIL_DATA_W {
    DWORD cbSize;
    GUID ClassGuid;
    HANDLE RemoteMachineHandle;
    WCHAR RemoteMachineName[263];
} SP_DEVINFO_LIST_DETAIL_DATA_W, *PSP_DEVINFO_LIST_DETAIL_DATA_W;
#define DIGCF_DEFAULT 0x00000001
#define DIGCF_PRESENT 0x00000002
#define DIGCF_ALLCLASSES 0x00000004
#define DIGCF_PROFILE 0x00000008
#define DIGCF_DEVICEINTERFACE 0x00000010
#define DICS_FLAG_GLOBAL 0x00000001
#define DIREG_DEV 0x00000001
#define SPDRP_HARDWAREID 0x00000001
HDEVINFO SetupDiGetClassDevsW(const GUID* class_guid, PCWSTR enumerator, HWND parent, DWORD flags) noexcept;
HDEVINFO SetupDiCreateDeviceInfoList(const GUID* class_guid, HWND parent) noexcept;
BOOL SetupDiDestroyDeviceInfoList(HDEVINFO set) noexcept;
BOOL SetupDiEnumDeviceInterfaces(HDEVINFO set, PSP_DEVINFO_DATA info, const GUID* guid, DWORD index,
                                 PSP_DEVICE_INTERFACE_DATA data) noexcept;
BOOL SetupDiEnumDeviceInfo(HDEVINFO set, DWORD index, PSP_DEVINFO_DATA info) noexcept;
BOOL SetupDiGetDeviceInterfaceDetailW(HDEVINFO set, PSP_DEVICE_INTERFACE_DATA data,
                                      PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail, DWORD size, PDWORD required,
                                      PSP_DEVINFO_DATA info) noexcept;
BOOL SetupDiGetDeviceInfoListDetailW(HDEVINFO set, PSP_DEVINFO_LIST_DETAIL_DATA_W detail) noexcept;
HKEY SetupDiOpenDevRegKey(HDEVINFO set, PSP_DEVINFO_DATA info, DWORD scope, DWORD profile, DWORD key_type,
                          REGSAM desired) noexcept;
BOOL SetupDiGetDeviceRegistryPropertyW(HDEVINFO set, PSP_DEVINFO_DATA info, DWORD property, PDWORD type,
                                       PBYTE buffer, DWORD size, PDWORD required) noexcept;
BOOL SetupDiGetDeviceInstanceIdW(HDEVINFO set, PSP_DEVINFO_DATA info, PWSTR id, DWORD size, PDWORD required) noexcept;

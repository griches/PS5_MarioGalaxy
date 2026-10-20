// Super Mario Galaxy for the PlayStation 5 - device arrival notifications (none).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef DWORD CONFIGRET;
typedef DWORD DEVINST;
#define CR_SUCCESS 0x00000000
#define MAX_DEVICE_ID_LEN 200
#define CR_FAILURE 0x00000013
#define CR_NO_SUCH_VALUE 0x00000025
#define CR_BUFFER_SMALL 0x0000001A
#define CM_GET_DEVICE_INTERFACE_LIST_PRESENT 0x00000000
DECLARE_HANDLE(HCMNOTIFICATION);
typedef HCMNOTIFICATION* PHCMNOTIFICATION;
typedef enum _CM_NOTIFY_FILTER_TYPE {
    CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE = 0,
    CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE,
    CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE,
    CM_NOTIFY_FILTER_TYPE_MAX
} CM_NOTIFY_FILTER_TYPE;
typedef struct _CM_NOTIFY_FILTER {
    DWORD cbSize;
    DWORD Flags;
    CM_NOTIFY_FILTER_TYPE FilterType;
    DWORD Reserved;
    union {
        struct {
            GUID ClassGuid;
        } DeviceInterface;
        struct {
            HANDLE hTarget;
        } DeviceHandle;
        struct {
            WCHAR InstanceId[200];
        } DeviceInstance;
    } u;
} CM_NOTIFY_FILTER, *PCM_NOTIFY_FILTER;
typedef enum _CM_NOTIFY_ACTION {
    CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL = 0,
    CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL,
    CM_NOTIFY_ACTION_DEVICEQUERYREMOVE,
    CM_NOTIFY_ACTION_DEVICEQUERYREMOVEFAILED,
    CM_NOTIFY_ACTION_DEVICEREMOVEPENDING,
    CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE,
    CM_NOTIFY_ACTION_DEVICECUSTOMEVENT,
    CM_NOTIFY_ACTION_DEVICEINSTANCEENUMERATED,
    CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED,
    CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED,
    CM_NOTIFY_ACTION_MAX
} CM_NOTIFY_ACTION;
typedef struct _CM_NOTIFY_EVENT_DATA {
    CM_NOTIFY_FILTER_TYPE FilterType;
    DWORD Reserved;
    union {
        struct {
            GUID ClassGuid;
            WCHAR SymbolicLink[1];
        } DeviceInterface;
    } u;
} CM_NOTIFY_EVENT_DATA, *PCM_NOTIFY_EVENT_DATA;
typedef DWORD(CALLBACK* PCM_NOTIFY_CALLBACK)(HCMNOTIFICATION, PVOID, CM_NOTIFY_ACTION, PCM_NOTIFY_EVENT_DATA, DWORD);
CONFIGRET CM_Register_Notification(PCM_NOTIFY_FILTER filter, PVOID context, PCM_NOTIFY_CALLBACK callback,
                                   PHCMNOTIFICATION notification) noexcept;
CONFIGRET CM_Unregister_Notification(HCMNOTIFICATION notification) noexcept;
CONFIGRET CM_Get_Device_Interface_List_SizeW(PULONG length, const GUID* guid, PWSTR device, ULONG flags) noexcept;
CONFIGRET CM_Get_Device_Interface_ListW(const GUID* guid, PWSTR device, PWSTR buffer, ULONG length,
                                        ULONG flags) noexcept;
CONFIGRET CM_Get_Device_IDW(DEVINST instance, PWSTR buffer, ULONG length, ULONG flags) noexcept;
CONFIGRET CM_Locate_DevNodeW(DEVINST* instance, PWSTR id, ULONG flags) noexcept;

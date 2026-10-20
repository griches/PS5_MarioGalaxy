// Super Mario Galaxy for the PlayStation 5 - WinUSB (no devices).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef PVOID WINUSB_INTERFACE_HANDLE, *PWINUSB_INTERFACE_HANDLE;
typedef enum _USBD_PIPE_TYPE { UsbdPipeTypeControl, UsbdPipeTypeIsochronous, UsbdPipeTypeBulk, UsbdPipeTypeInterrupt } USBD_PIPE_TYPE;
typedef struct _WINUSB_PIPE_INFORMATION {
    USBD_PIPE_TYPE PipeType;
    UCHAR PipeId;
    USHORT MaximumPacketSize;
    UCHAR Interval;
} WINUSB_PIPE_INFORMATION, *PWINUSB_PIPE_INFORMATION;
typedef struct _USB_INTERFACE_DESCRIPTOR {
    UCHAR bLength;
    UCHAR bDescriptorType;
    UCHAR bInterfaceNumber;
    UCHAR bAlternateSetting;
    UCHAR bNumEndpoints;
    UCHAR bInterfaceClass;
    UCHAR bInterfaceSubClass;
    UCHAR bInterfaceProtocol;
    UCHAR iInterface;
} USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;
typedef struct _WINUSB_SETUP_PACKET {
    UCHAR RequestType;
    UCHAR Request;
    USHORT Value;
    USHORT Index;
    USHORT Length;
} WINUSB_SETUP_PACKET, *PWINUSB_SETUP_PACKET;
#define PIPE_TRANSFER_TIMEOUT 0x03
#define USB_ENDPOINT_DIRECTION_MASK 0x80
#define USB_ENDPOINT_DIRECTION_IN(x) ((x)&USB_ENDPOINT_DIRECTION_MASK)
#define USB_ENDPOINT_DIRECTION_OUT(x) (!((x)&USB_ENDPOINT_DIRECTION_MASK))
BOOL WinUsb_Initialize(HANDLE device, PWINUSB_INTERFACE_HANDLE handle) noexcept;
BOOL WinUsb_Free(WINUSB_INTERFACE_HANDLE handle) noexcept;
BOOL WinUsb_QueryInterfaceSettings(WINUSB_INTERFACE_HANDLE handle, UCHAR index, PUSB_INTERFACE_DESCRIPTOR descriptor) noexcept;
BOOL WinUsb_QueryPipe(WINUSB_INTERFACE_HANDLE handle, UCHAR setting, UCHAR index, PWINUSB_PIPE_INFORMATION pipe) noexcept;
BOOL WinUsb_ReadPipe(WINUSB_INTERFACE_HANDLE handle, UCHAR pipe, PUCHAR buffer, ULONG length, PULONG transferred, LPOVERLAPPED overlapped) noexcept;
BOOL WinUsb_WritePipe(WINUSB_INTERFACE_HANDLE handle, UCHAR pipe, PUCHAR buffer, ULONG length, PULONG transferred, LPOVERLAPPED overlapped) noexcept;
BOOL WinUsb_SetPipePolicy(WINUSB_INTERFACE_HANDLE handle, UCHAR pipe, ULONG type, ULONG length, PVOID value) noexcept;
BOOL WinUsb_ControlTransfer(WINUSB_INTERFACE_HANDLE handle, WINUSB_SETUP_PACKET setup, PUCHAR buffer, ULONG length, PULONG transferred, LPOVERLAPPED overlapped) noexcept;
BOOL WinUsb_AbortPipe(WINUSB_INTERFACE_HANDLE handle, UCHAR pipe) noexcept;
BOOL WinUsb_FlushPipe(WINUSB_INTERFACE_HANDLE handle, UCHAR pipe) noexcept;


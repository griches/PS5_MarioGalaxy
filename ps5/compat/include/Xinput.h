// Super Mario Galaxy for the PlayStation 5 - XInput: the console's DualSense as
// controller 0, with Xbox naming (ps5/compat/src/xinput.cpp).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#define XUSER_MAX_COUNT 4
#define XUSER_INDEX_ANY 0x000000FF
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD 30
#define XINPUT_FLAG_GAMEPAD 0x00000001
#define XINPUT_DEVTYPE_GAMEPAD 0x01
#define XINPUT_DEVSUBTYPE_GAMEPAD 0x01
#define BATTERY_DEVTYPE_GAMEPAD 0x00
#define BATTERY_TYPE_WIRED 0x01
#define BATTERY_LEVEL_FULL 0x03
typedef struct _XINPUT_GAMEPAD {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;
typedef struct _XINPUT_STATE {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;
typedef struct _XINPUT_VIBRATION {
    WORD wLeftMotorSpeed;
    WORD wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;
typedef struct _XINPUT_CAPABILITIES {
    BYTE Type;
    BYTE SubType;
    WORD Flags;
    XINPUT_GAMEPAD Gamepad;
    XINPUT_VIBRATION Vibration;
} XINPUT_CAPABILITIES, *PXINPUT_CAPABILITIES;
typedef struct _XINPUT_BATTERY_INFORMATION {
    BYTE BatteryType;
    BYTE BatteryLevel;
} XINPUT_BATTERY_INFORMATION;
DWORD WINAPI XInputGetState(DWORD user, XINPUT_STATE* state) noexcept;
DWORD WINAPI XInputSetState(DWORD user, XINPUT_VIBRATION* vibration) noexcept;
DWORD WINAPI XInputGetCapabilities(DWORD user, DWORD flags, XINPUT_CAPABILITIES* capabilities) noexcept;
DWORD WINAPI XInputGetBatteryInformation(DWORD user, BYTE type, XINPUT_BATTERY_INFORMATION* information) noexcept;
void WINAPI XInputEnable(BOOL enable) noexcept;

// Super Mario Galaxy for the PlayStation 5 - XInput controller 0 is the console's
// DualSense (ps5/src/platform.c reads it), named as an Xbox pad: Cross A,
// Circle B, Square X, Triangle Y, Options Start, the touchpad button Back.
// Nebula maps that pad to the virtual Wii Remote and Nunchuk with its own dead
// zones, so the sticks go through raw.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <Xinput.h>

#include <mutex>

#include "platform.h"

namespace {
std::mutex g_pad_lock;
bool g_pad_opened = false;
DWORD g_packet = 0;
XINPUT_STATE g_last{};
ULONGLONG g_last_poll_ms = 0;

SHORT axis(std::uint8_t value, bool invert) {
    int centred = (static_cast<int>(value) - 128) * 256;
    if (value == 255) centred = 32767;
    if (invert) centred = -centred - 1;
    if (centred > 32767) centred = 32767;
    if (centred < -32768) centred = -32768;
    return static_cast<SHORT>(centred);
}
}  // namespace

DWORD WINAPI XInputGetState(DWORD user, XINPUT_STATE* state) noexcept {
    if (user != 0 || state == nullptr) return ERROR_DEVICE_NOT_CONNECTED;
    std::lock_guard lock(g_pad_lock);
    if (!g_pad_opened) {
        g_pad_opened = true;
        pad_open();
    }
    // The input thread may ask more often than the pad reports: one read a millisecond.
    const ULONGLONG now = GetTickCount64();
    if (now != g_last_poll_ms) {
        g_last_poll_ms = now;
        struct pad pad{};
        pad_poll(&pad);
        const struct pad_reading* readings = nullptr;
        const int count = pad_readings(&readings);
        XINPUT_STATE next{};
        if ((pad_players() & 1u) == 0) {
            return ERROR_DEVICE_NOT_CONNECTED;
        }
        if (count > 0 && (readings[count - 1].buttons & PAD_INTERCEPTED) == 0) {
            const struct pad_reading& reading = readings[count - 1];
            const std::uint32_t held = reading.buttons;
            WORD buttons = 0;
            if (held & PAD_UP) buttons |= XINPUT_GAMEPAD_DPAD_UP;
            if (held & PAD_DOWN) buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
            if (held & PAD_LEFT) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
            if (held & PAD_RIGHT) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
            if (held & PAD_OPTIONS) buttons |= XINPUT_GAMEPAD_START;
            if (held & PAD_TOUCH_PAD) buttons |= XINPUT_GAMEPAD_BACK;
            if (held & PAD_L3) buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
            if (held & PAD_R3) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;
            if (held & PAD_L1) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
            if (held & PAD_R1) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
            if (held & PAD_CROSS) buttons |= XINPUT_GAMEPAD_A;
            if (held & PAD_CIRCLE) buttons |= XINPUT_GAMEPAD_B;
            if (held & PAD_SQUARE) buttons |= XINPUT_GAMEPAD_X;
            if (held & PAD_TRIANGLE) buttons |= XINPUT_GAMEPAD_Y;
            next.Gamepad.wButtons = buttons;
            next.Gamepad.bLeftTrigger = reading.l2;
            next.Gamepad.bRightTrigger = reading.r2;
            next.Gamepad.sThumbLX = axis(reading.left_x, false);
            next.Gamepad.sThumbLY = axis(reading.left_y, true);
            next.Gamepad.sThumbRX = axis(reading.right_x, false);
            next.Gamepad.sThumbRY = axis(reading.right_y, true);
        } else if (count == 0) {
            next.Gamepad = g_last.Gamepad;
        }
        if (std::memcmp(&next.Gamepad, &g_last.Gamepad, sizeof(next.Gamepad)) != 0) ++g_packet;
        g_last = next;
        g_last.dwPacketNumber = g_packet;
    }
    *state = g_last;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputSetState(DWORD user, XINPUT_VIBRATION* vibration) noexcept {
    if (user != 0 || vibration == nullptr) return ERROR_DEVICE_NOT_CONNECTED;
    pad_vibrate(vibration->wLeftMotorSpeed / 65535.0f, vibration->wRightMotorSpeed / 65535.0f);
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputGetCapabilities(DWORD user, DWORD, XINPUT_CAPABILITIES* capabilities) noexcept {
    if (user != 0 || capabilities == nullptr) return ERROR_DEVICE_NOT_CONNECTED;
    *capabilities = {};
    capabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
    capabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    capabilities->Gamepad.wButtons = 0xF3FF;
    capabilities->Gamepad.bLeftTrigger = 255;
    capabilities->Gamepad.bRightTrigger = 255;
    capabilities->Vibration.wLeftMotorSpeed = 65535;
    capabilities->Vibration.wRightMotorSpeed = 65535;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputGetBatteryInformation(DWORD user, BYTE, XINPUT_BATTERY_INFORMATION* information) noexcept {
    if (user != 0 || information == nullptr) return ERROR_DEVICE_NOT_CONNECTED;
    information->BatteryType = BATTERY_TYPE_WIRED;
    information->BatteryLevel = BATTERY_LEVEL_FULL;
    return ERROR_SUCCESS;
}

void WINAPI XInputEnable(BOOL) noexcept {}

// Super Mario Galaxy for the PlayStation 5 - what the console build tells the
// Windows compatibility layer: the modules linked into the eboot, by the part
// of their DLL name LoadLibrary is asked for (ps5/src/modules.cpp); and what
// the layer gives back beyond Windows: the DualSense's motion readings.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>

namespace compat {
struct ModuleExport {
    const char* name;
    void* address;
};
void register_module(const char* name_part, const ModuleExport* exports, std::size_t count) noexcept;

// One motion reading of XInput user 0's pad, the DualSense (ps5/compat/src/xinput.cpp), in the
// SDL sensor frame: +X right, +Y out of the face, +Z toward the player.
struct PadMotion {
    float accel[3];  // g
    float gyro[3];   // rad/s, counter-clockwise positive
    std::uint64_t timestamp_us;
};
// The readings XInputGetState has taken since the last call, oldest first (the oldest are
// dropped past `capacity`). Returns their number.
std::size_t take_pad_motion(PadMotion* out, std::size_t capacity) noexcept;
}  // namespace compat

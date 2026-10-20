// Super Mario Galaxy for the PlayStation 5 - what the console build tells the
// Windows compatibility layer: the modules linked into the eboot, by the part
// of their DLL name LoadLibrary is asked for (ps5/src/modules.cpp).
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>

namespace compat {
struct ModuleExport {
    const char* name;
    void* address;
};
void register_module(const char* name_part, const ModuleExport* exports, std::size_t count) noexcept;
}  // namespace compat

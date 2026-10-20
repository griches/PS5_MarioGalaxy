// Super Mario Galaxy for the PlayStation 5 - the game's modules, linked into the
// eboot. On Windows Nebula loads RMGE01_game.dll and RMGE01_home_button.dll and
// asks each for its exports by name; here the compatibility layer's LoadLibrary
// finds them in this table (the audio DSP program is compiled into the runtime,
// GALAXY_HAVE_NATIVE_DSP).
//
// SPDX-License-Identifier: GPL-3.0-only
#include <ps5_compat.h>

// Only their addresses are taken here: the types are the modules' own.
extern "C" {
void galaxy_module_manifest();
void galaxy_module_init();
void galaxy_module_entry();
void galaxy_lookup_function();
void galaxy_module_float_flags();
void galaxy_module_transfer_flags();
void galaxy_module_function_records();
void galaxy_yield_rfi_return_protocol();
void galaxy_module_checkpoint_gate();
void galaxy_home_button_rso_manifest();
void galaxy_home_button_rso_try_call();
void galaxy_home_button_rso_resume();
}

namespace {
#define EXPORT(name) compat::ModuleExport{#name, reinterpret_cast<void*>(&name)}
const compat::ModuleExport kGameExports[] = {
    EXPORT(galaxy_module_manifest),        EXPORT(galaxy_module_init),
    EXPORT(galaxy_module_entry),           EXPORT(galaxy_lookup_function),
    EXPORT(galaxy_module_float_flags),     EXPORT(galaxy_module_transfer_flags),
    EXPORT(galaxy_module_function_records), EXPORT(galaxy_yield_rfi_return_protocol),
    EXPORT(galaxy_module_checkpoint_gate),
};
const compat::ModuleExport kHomeButtonExports[] = {
    EXPORT(galaxy_home_button_rso_manifest),
    EXPORT(galaxy_home_button_rso_try_call),
    EXPORT(galaxy_home_button_rso_resume),
};
#undef EXPORT
}  // namespace

void ps5_register_modules() {
    // "RMGE01_home_button" is looked for first: "RMGE01_game" is not part of it,
    // but the order keeps the more specific name ahead.
    compat::register_module("RMGE01_home_button", kHomeButtonExports, sizeof(kHomeButtonExports) / sizeof(kHomeButtonExports[0]));
    compat::register_module("RMGE01_game", kGameExports, sizeof(kGameExports) / sizeof(kGameExports[0]));
}

# Super Mario Galaxy for the PlayStation 5 - the title, built in the Lima VM with
# the payload SDK's toolchain (ps5/tools/build.sh): Nebula's runtime on the
# Windows compatibility layer (ps5/compat), the game's modules generated from
# the player's disc (gamedata/generated), and the console's platform layer,
# linked into one eboot by PS5_Vulkan's recipe (ps5/tools/link-title.sh).
#
# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(SuperMarioGalaxy LANGUAGES C CXX)

foreach(required PS5_PORT_DIR PS5_VULKAN_DIR PS5_GENERATED_DIR)
    if(NOT ${required})
        message(FATAL_ERROR "${required} is not set (use ps5/tools/build.sh)")
    endif()
endforeach()
set(ps5 ${PS5_PORT_DIR}/ps5)
set(nebula ${PS5_PORT_DIR}/vendor/nebula)
set(runtime ${nebula}/runtime)
set(generated ${PS5_GENERATED_DIR})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# An init array in source order (ps5/tools/cc-init-array.sh has the reason)
set(CMAKE_C_COMPILER_LAUNCHER sh ${ps5}/tools/cc-init-array.sh)
set(CMAKE_CXX_COMPILER_LAUNCHER sh ${ps5}/tools/cc-init-array.sh)

# The console's CPU (Zen 2: AVX2, FMA, MOVBE); frame pointers for its crash walks
add_compile_options(-O2 -march=znver2 -ffunction-sections -fdata-sections -fno-omit-frame-pointer)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>)
add_compile_definitions(GALAXY_PS5=1 SOFTFLOAT_FAST_INT64=1 GALAXY_HAVE_NATIVE_DSP=1)
# SoftFloat's state is thread-local in its C (runtime/softfloat/platform.h); the
# C++ that reads it must say so too (the console's TLS is emulated, by name)
add_compile_definitions($<$<COMPILE_LANGUAGE:CXX>:THREAD_LOCAL=thread_local>)
set(windows_flags
    -include ${ps5}/compat/include/msvc_crt.h -fdeclspec
    -Wno-unknown-attributes -Wno-ignored-attributes -Wno-unused-command-line-argument)
set(windows_includes ${ps5}/compat/include ${ps5}/compat/generated ${runtime}/include ${ps5}/src)

# ---- Berkeley SoftFloat 3 (the guest's exact float paths) -------------------------
set(softfloat_common
    s_eq128
    s_le128
    s_lt128
    s_shortShiftLeft128
    s_shortShiftRight128
    s_shortShiftRightJam64
    s_shortShiftRightJam64Extra
    s_shortShiftRightJam128
    s_shiftRightJam128
    s_shortShiftRightJam128Extra
    s_shiftRightJam32
    s_shiftRightJam64
    s_shiftRightJam64Extra
    s_countLeadingZeros8
    s_countLeadingZeros16
    s_countLeadingZeros32
    s_countLeadingZeros64
    s_add128
    s_sub128
    s_mul64ByShifted32To128
    s_mul64To128
    s_approxRecip_1Ks
    s_approxRecip32_1
    s_approxRecipSqrt_1Ks
    s_approxRecipSqrt32_1
    s_roundToUI32
    s_roundToUI64
    s_roundToI32
    s_roundToI64
    s_normSubnormalF32Sig
    s_roundPackToF32
    s_normRoundPackToF32
    s_addMagsF32
    s_subMagsF32
    s_mulAddF32
    s_normSubnormalF64Sig
    s_roundPackToF64
    s_normRoundPackToF64
    s_addMagsF64
    s_subMagsF64
    s_mulAddF64
    softfloat_state
    i32_to_f32
    i32_to_f64
    f32_to_i32
    f32_to_i32_r_minMag
    f32_to_f64
    f64_to_f32
    f64_to_i32
    f64_to_i32_r_minMag
    f32_roundToInt
    f32_add
    f32_sub
    f32_mul
    f32_mulAdd
    f32_div
    f32_rem
    f32_sqrt
    f32_eq
    f32_le
    f32_lt
    f32_eq_signaling
    f32_le_quiet
    f32_lt_quiet
    f32_isSignalingNaN
    f64_roundToInt
    f64_add
    f64_sub
    f64_mul
    f64_mulAdd
    f64_div
    f64_rem
    f64_sqrt
    f64_eq
    f64_le
    f64_lt
    f64_eq_signaling
    f64_le_quiet
    f64_lt_quiet
    f64_isSignalingNaN
    f64_to_f128
    f128_to_f32
    f128_mulAdd
    s_mulAddF128
    s_normSubnormalF128Sig
    s_roundPackToF128
    s_normRoundPackToF128
    s_mul128To256M
    s_add256M
    s_sub256M
    s_shiftRightJam256M
    s_shiftRightJam128Extra)
set(softfloat_specialize
    softfloat_raiseFlags
    s_f32UIToCommonNaN
    s_commonNaNToF32UI
    s_propagateNaNF32UI
    s_f64UIToCommonNaN
    s_commonNaNToF64UI
    s_propagateNaNF64UI
    s_commonNaNToF128UI
    s_f128UIToCommonNaN
    s_propagateNaNF128UI)
list(TRANSFORM softfloat_common PREPEND "${nebula}/third_party/berkeley-softfloat-3/source/")
list(TRANSFORM softfloat_common APPEND ".c")
list(TRANSFORM softfloat_specialize PREPEND "${nebula}/third_party/berkeley-softfloat-3/source/8086-SSE/")
list(TRANSFORM softfloat_specialize APPEND ".c")
add_library(softfloat STATIC ${softfloat_common} ${softfloat_specialize})
target_include_directories(softfloat PUBLIC ${nebula}/third_party/berkeley-softfloat-3/source/include
    PRIVATE ${runtime}/softfloat ${nebula}/third_party/berkeley-softfloat-3/source/8086-SSE)
target_compile_definitions(softfloat PRIVATE SOFTFLOAT_FAST_DIV32TO16=1 SOFTFLOAT_FAST_DIV64TO32=1 INLINE_LEVEL=5)
target_compile_options(softfloat PRIVATE -w)

# ---- the Windows compatibility layer ---------------------------------------------------
add_library(compat STATIC
    ${ps5}/compat/src/handles.cpp
    ${ps5}/compat/src/fibers.cpp
    ${ps5}/compat/src/files.cpp
    ${ps5}/compat/src/exceptions.cpp
    ${ps5}/compat/src/system.cpp
    ${ps5}/compat/src/window.cpp
    ${ps5}/compat/src/window_queue.cpp
    ${ps5}/compat/src/bcrypt.cpp
    ${ps5}/compat/src/xinput.cpp
    ${ps5}/compat/src/xaudio2.cpp)
target_include_directories(compat PRIVATE ${windows_includes} ${ps5}/compat/src)
target_compile_options(compat PRIVATE ${windows_flags} -g1)

# ---- vkd3d: Wine's Direct3D 12 on Vulkan (ps5/tools/setup-vkd3d.sh) ---------------------
# Nebula's renderer is Direct3D 12; vkd3d runs it on the console's RADV. Its
# parser sources and version header come from the VM's own build of the same
# tree (VKD3D_HOST_DIR); the console's configuration is ps5/vkd3d/config.h.
foreach(required VKD3D_HOST_DIR PS5_VULKAN_HEADERS PS5_SPIRV_HEADERS)
    if(NOT ${required})
        message(FATAL_ERROR "${required} is not set (use ps5/tools/build.sh)")
    endif()
endforeach()
set(vkd3d ${PS5_PORT_DIR}/.deps/vkd3d)
file(GLOB vkd3d_common_sources ${vkd3d}/libs/vkd3d-common/*.c)
file(GLOB vkd3d_shader_sources ${vkd3d}/libs/vkd3d-shader/*.c)
file(GLOB vkd3d_device_sources ${vkd3d}/libs/vkd3d/*.c)
file(GLOB vkd3d_utils_sources ${vkd3d}/libs/vkd3d-utils/*.c)
list(APPEND vkd3d_shader_sources
    ${VKD3D_HOST_DIR}/libs/vkd3d-shader/hlsl.tab.c ${VKD3D_HOST_DIR}/libs/vkd3d-shader/hlsl.yy.c
    ${VKD3D_HOST_DIR}/libs/vkd3d-shader/preproc.tab.c ${VKD3D_HOST_DIR}/libs/vkd3d-shader/preproc.yy.c)
add_library(vkd3d STATIC ${vkd3d_common_sources} ${vkd3d_shader_sources} ${vkd3d_device_sources} ${vkd3d_utils_sources})
target_include_directories(vkd3d PRIVATE ${ps5}/vkd3d ${vkd3d}/include ${vkd3d}/include/private
    ${VKD3D_HOST_DIR}/include/private ${vkd3d}/libs/vkd3d-shader ${VKD3D_HOST_DIR}/libs/vkd3d-shader
    ${PS5_VULKAN_HEADERS} ${PS5_SPIRV_HEADERS})
# System V calls for the COM methods (as the compatibility layer makes them), and
# no D3D12CreateDevice/D3DCompile of vkd3d-utils' own (ps5/gx/d3d12_ps5.cpp has them)
target_compile_definitions(vkd3d PRIVATE VKD3D_SYSV_ABI VKD3D_UTILS_PS5 _GNU_SOURCE)
target_compile_options(vkd3d PRIVATE -std=gnu11 -w -g1)
set_source_files_properties(${vkd3d_shader_sources} PROPERTIES
    COMPILE_DEFINITIONS "LIBVKD3D_SHADER_SOURCE;VKD3D_DEBUG_ENV=VKD3D_SHADER_DEBUG")
set_source_files_properties(${vkd3d_device_sources} PROPERTIES COMPILE_DEFINITIONS LIBVKD3D_SOURCE)
set_source_files_properties(${vkd3d_utils_sources} PROPERTIES COMPILE_DEFINITIONS LIBVKD3D_UTILS_SOURCE)

# ---- Nebula's shaders ------------------------------------------------------------------
# The uber shaders' HLSL is compiled at run time (vkd3d-shader): embedded as text,
# as Nebula's own CMake does. The presentation passes are compiled at build time
# to bytecode headers by the VM's vkd3d-compiler (Nebula uses fxc).
set(uber_embed_cpp ${CMAKE_BINARY_DIR}/generated/uber_shader_embed.cpp)
set(uber_embed_body "")
foreach(stage vs ps)
    set(uber_hlsl ${runtime}/src/gx/uber_${stage}.hlsl)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${uber_hlsl})
    file(READ ${uber_hlsl} uber_hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," uber_hex "${uber_hex}")
    string(APPEND uber_embed_body "extern const char galaxy_uber_${stage}_hlsl[] = {${uber_hex}0x00};\n")
endforeach()
file(CONFIGURE OUTPUT ${uber_embed_cpp} CONTENT "namespace galaxy::gx {\n${uber_embed_body}}\n" @ONLY)
set(post_shader_dir ${CMAKE_BINARY_DIR}/generated/post_shaders)
set(post_shader_names post_fullscreen_vs post_resample_ps post_fxaa_ps post_fsr_easu_ps post_fsr_rcas_ps
    post_smaa_edges_vs post_smaa_blend_vs post_smaa_blend_ps)
foreach(preset low medium high ultra)
    list(APPEND post_shader_names post_smaa_edges_ps_${preset} post_smaa_weights_vs_${preset}
        post_smaa_weights_ps_${preset})
endforeach()
list(TRANSFORM post_shader_names PREPEND ${post_shader_dir}/ OUTPUT_VARIABLE post_shader_headers)
list(TRANSFORM post_shader_headers APPEND .h)
add_custom_command(OUTPUT ${post_shader_headers}
    COMMAND bash ${ps5}/tools/post-shaders.sh ${nebula} ${VKD3D_HOST_DIR}/vkd3d-compiler ${post_shader_dir}
    DEPENDS ${ps5}/tools/post-shaders.sh ${runtime}/src/gx/post_process.hlsl ${nebula}/third_party/smaa/SMAA.hlsl
        ${nebula}/third_party/fidelityfx-fsr1/ffx_a.h ${nebula}/third_party/fidelityfx-fsr1/ffx_fsr1.h
    VERBATIM)

# ---- Nebula's runtime ---------------------------------------------------------------
set(runtime_sources
    native_runtime.cpp native_thp_video.cpp coop_scheduler.cpp site_hooks.cpp
    native_host.cpp controller_hid.cpp flat_guest_memory.cpp
    native_input.cpp pointer_control.cpp rumble.cpp switch_controller.cpp tilt.cpp
    native_input_causality.cpp ai_pcm_identity.cpp jaudio_host_mixer.cpp
    runtime_settings.cpp input_bindings.cpp atomic_file.cpp runtime_instance.cpp
    runtime_timeline.cpp decrementer_deadline.cpp frame_cadence_diagnostics.cpp
    checkpoint_gate.cpp host_pointer_events.cpp frame_telemetry.cpp
    ppc_float.cpp dsp_alu.cpp native_dsp.cpp
    gx/fifo_parser.cpp gx/gx_state.cpp gx/vertex_loader.cpp gx/frame_completion.cpp
    gx/render_config.cpp gx/gx_shim.cpp gx/gx_backend.cpp gx/renderer_d3d12.cpp gx/window_owner.cpp
    gx/texture_cache.cpp gx/placed_texture_allocator.cpp gx/shader_keys.cpp gx/efb_copy.cpp
    gx/line_point_raster.cpp gx/pipeline_cache.cpp gx/shader_gen.cpp gx/uber_shaders.cpp
    gx/post_process.cpp gx/temporal_upscaler.cpp gx/transform_history.cpp gx/display_modes.cpp
    gx/game_asset_extract.cpp)
list(TRANSFORM runtime_sources PREPEND "${runtime}/src/")
add_library(nebula_runtime STATIC ${runtime_sources}
    ${generated}/dsp/rmge01_dsp.cpp
    ${uber_embed_cpp} ${post_shader_headers}
    ${ps5}/gx/d3d12_ps5.cpp ${ps5}/gx/dxgi_ps5.cpp)
target_include_directories(nebula_runtime PRIVATE ${windows_includes} ${runtime}/softfloat
    ${vkd3d}/include ${PS5_VULKAN_HEADERS} ${post_shader_dir} ${nebula}/third_party/smaa)
target_compile_options(nebula_runtime PRIVATE ${windows_flags} -g1 -fwrapv -fno-strict-aliasing)
target_link_libraries(nebula_runtime PUBLIC softfloat)

# ---- the game's modules, generated from the disc -----------------------------------------
# As Nebula's own module build: no float contraction, wrapping arithmetic, no
# type-based aliasing. No debug information (it would be gigabytes).
set(module_flags ${windows_flags} -g0 -w -ffp-contract=off -fwrapv -fno-strict-aliasing)
file(GLOB game_sources CONFIGURE_DEPENDS ${generated}/game/*.cpp)
add_library(game_module STATIC ${game_sources})
target_include_directories(game_module PRIVATE ${windows_includes} ${generated}/game)
target_compile_definitions(game_module PRIVATE GALAXY_BUILDING_GAME_MODULE=1)
target_compile_options(game_module PRIVATE ${module_flags})
target_precompile_headers(game_module PRIVATE ${generated}/game/functions.h)
add_library(home_module STATIC ${generated}/home/home_button_native.cpp)
target_include_directories(home_module PRIVATE ${windows_includes})
target_compile_definitions(home_module PRIVATE GALAXY_BUILDING_GAME_MODULE=1)
# The Home Menu's code is one function of 148,000 lines: clang's optimiser
# takes far longer than the build allows on it, and it only runs while the
# Wii's HOME menu is open, so it is built unoptimised (11 s, not 10+ minutes)
target_compile_options(home_module PRIVATE ${module_flags} -O0)
# The generated sources are large (the Home Menu's alone takes ~3.4 GB to compile)
set_property(GLOBAL PROPERTY JOB_POOLS module_compile=9)
set_property(TARGET game_module home_module PROPERTY JOB_POOL_COMPILE module_compile)

# ---- the console ---------------------------------------------------------------------
add_library(ps5_platform STATIC ${ps5}/src/platform.c ${ps5}/src/shims.c ${ps5}/src/startup.c ${ps5}/src/paths.c)
target_include_directories(ps5_platform PUBLIC ${ps5}/src)
file(READ ${ps5}/sce_sys/param.json param_json)
string(JSON PS5_TITLE_NAME GET "${param_json}" localizedParameters en-US titleName)
target_compile_definitions(ps5_platform PRIVATE "PS5_TITLE_NAME=\"${PS5_TITLE_NAME}\"")
set_target_properties(ps5_platform PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(SuperMarioGalaxy ${ps5}/src/main.cpp ${ps5}/src/modules.cpp)
target_include_directories(SuperMarioGalaxy PRIVATE ${windows_includes})
target_compile_options(SuperMarioGalaxy PRIVATE ${windows_flags} -g1)
target_link_libraries(SuperMarioGalaxy PRIVATE nebula_runtime compat game_module home_module softfloat vkd3d)
# The platform archive is linked whole (its start-up constructor has no caller),
# and the link is PS5_Vulkan's recipe, with RADV (tools/link-title.sh)
set(CMAKE_CXX_LINK_EXECUTABLE
    "bash ${ps5}/tools/link-title.sh ${CMAKE_BINARY_DIR} ${PS5_VULKAN_DIR} ${PS5_PAYLOAD_SDK} <OBJECTS> --whole-archive ${CMAKE_BINARY_DIR}/libps5_platform.a --no-whole-archive <LINK_LIBRARIES>")
add_dependencies(SuperMarioGalaxy ps5_platform)

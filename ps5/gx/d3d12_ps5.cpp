// Super Mario Galaxy for the PlayStation 5 - Direct3D 12 devices and the HLSL
// compiler, over vkd3d (Wine's Direct3D 12 on Vulkan) and the console's RADV,
// which is linked into the title (ps5/tools/link-title.sh).
//
// One vkd3d instance serves every device. It asks Vulkan for VK_KHR_display,
// and each device for VK_KHR_swapchain, which the swap chain presents with
// (dxgi_ps5.cpp). Fences signal Win32 events through the compatibility layer.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "d3d12_ps5.h"

#include <d3dcompiler.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string_view>

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char* name);

namespace {

HRESULT signal_event(HANDLE event) { return SetEvent(event) ? S_OK : E_FAIL; }

std::mutex g_lock;
struct vkd3d_instance* g_instance = nullptr;

// True when a register(...) binding names a space: Shader Model 5.1 is needed.
bool uses_register_spaces(std::string_view source) noexcept {
    for (std::size_t at = source.find("register"); at != std::string_view::npos;
         at = source.find("register", at + 8)) {
        const std::size_t open = source.find('(', at);
        const std::size_t close = source.find(')', at);
        if (open == std::string_view::npos || close == std::string_view::npos || close < open) continue;
        if (source.substr(open, close - open).find("space") != std::string_view::npos) return true;
    }
    return false;
}

}  // namespace

namespace ps5gx {

PFN_vkGetInstanceProcAddr vulkan_loader() noexcept { return vk_icdGetInstanceProcAddr; }

struct vkd3d_instance* instance() noexcept {
    std::lock_guard lock(g_lock);
    if (g_instance != nullptr) return g_instance;
    static const char* const extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME};
    vkd3d_instance_create_info info{};
    info.type = VKD3D_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pfn_signal_event = signal_event;
    info.wchar_size = sizeof(WCHAR);
    info.pfn_vkGetInstanceProcAddr = vk_icdGetInstanceProcAddr;
    info.instance_extensions = extensions;
    info.instance_extension_count = 2;
    const HRESULT result = vkd3d_create_instance(&info, &g_instance);
    if (FAILED(result)) {
        std::fprintf(stderr, "[d3d12-ps5] vkd3d_create_instance failed: 0x%08x\n", static_cast<unsigned>(result));
        g_instance = nullptr;
    }
    return g_instance;
}

}  // namespace ps5gx

extern "C" HRESULT WINAPI D3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL minimum_feature_level, REFIID iid,
                                            void** device) {
    struct vkd3d_instance* const instance = ps5gx::instance();
    if (instance == nullptr) return E_FAIL;
    static const char* const extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    vkd3d_device_create_info info{};
    info.type = VKD3D_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.minimum_feature_level = minimum_feature_level;
    info.instance = instance;
    info.device_extensions = extensions;
    info.device_extension_count = 1;
    info.parent = adapter;
    info.adapter_luid = LUID{0x5053, 0};
    if (device == nullptr) {
        // A support query: create one and let it go.
        ID3D12Device* probe = nullptr;
        const HRESULT result = vkd3d_create_device(&info, __uuidof(ID3D12Device), reinterpret_cast<void**>(&probe));
        if (probe != nullptr) probe->Release();
        return SUCCEEDED(result) ? S_FALSE : result;
    }
    const HRESULT result = vkd3d_create_device(&info, iid, device);
    if (FAILED(result)) {
        std::fprintf(stderr, "[d3d12-ps5] vkd3d_create_device failed: 0x%08x\n", static_cast<unsigned>(result));
    }
    return result;
}

// vkd3d's Shader Model 5.1 output has no resource arrays (Texture2D tex[8]);
// 5.0 compiles them as one register each, which is the same binding when the
// source names no register space. Sources with spaces stay 5.1.
extern "C" HRESULT WINAPI D3DCompile(const void* data, SIZE_T data_size, const char* filename,
                                     const D3D_SHADER_MACRO* macros, ID3DInclude* include, const char* entry_point,
                                     const char* profile, UINT flags, UINT effect_flags, ID3DBlob** shader,
                                     ID3DBlob** messages) {
    char lowered[8]{};
    const char* used = profile;
    if (profile != nullptr && std::strlen(profile) == 6 && std::strcmp(profile + 2, "_5_1") == 0 &&
        !uses_register_spaces(std::string_view(static_cast<const char*>(data), data_size))) {
        std::memcpy(lowered, profile, 6);
        lowered[5] = '0';
        used = lowered;
    }
    const HRESULT result = D3DCompile2(data, data_size, filename, macros, include, entry_point, used, flags,
                                       effect_flags, 0, nullptr, 0, shader, messages);
    if (FAILED(result)) {
        std::fprintf(stderr, "[d3d12-ps5] D3DCompile(%s, %s) failed: 0x%08x\n", used != nullptr ? used : "?",
                     entry_point != nullptr ? entry_point : "?", static_cast<unsigned>(result));
        if (messages != nullptr && *messages != nullptr) {
            std::fprintf(stderr, "%.*s\n", static_cast<int>((*messages)->GetBufferSize()),
                         static_cast<const char*>((*messages)->GetBufferPointer()));
        }
    }
    return result;
}

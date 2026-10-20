// Super Mario Galaxy for the PlayStation 5 - what the Direct3D 12 device
// (d3d12_ps5.cpp) and the swap chain (dxgi_ps5.cpp) share: the vkd3d instance
// and the Vulkan entry point of the RADV linked into the title.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <d3d12.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vkd3d.h>

namespace ps5gx {
PFN_vkGetInstanceProcAddr vulkan_loader() noexcept;
struct vkd3d_instance* instance() noexcept;
}  // namespace ps5gx

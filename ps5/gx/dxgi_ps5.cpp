// Super Mario Galaxy for the PlayStation 5 - DXGI: the console's GPU as the one
// adapter, the TV as the one output, and a flip swap chain that presents with
// Vulkan's VK_KHR_display.
//
// The swap chain's buffers are Vulkan images handed to vkd3d as Direct3D 12
// resources (vkd3d_create_image_resource), so Nebula renders into them as on
// Windows. Their PRESENT state is the copy-source layout; Present() copies the
// current buffer onto the display's next image (scaled when the sizes differ)
// on vkd3d's own queue, after the frame's Direct3D 12 work, and queues it.
// The display's FIFO paces presentation; frame-latency objects are always
// signalled. The mode is 119.88 Hz where the TV runs it as VRR, else 59.94 Hz;
// Nebula reads the refresh from the display
// settings (compat::set_display_refresh_hz).
//
// SPDX-License-Identifier: GPL-3.0-only
#include "d3d12_ps5.h"

#include <dxgi1_6.h>
#include <ps5_compat.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

namespace {

void say(const char* format, auto... arguments) {
    std::fprintf(stderr, "[dxgi-ps5] ");
    std::fprintf(stderr, format, arguments...);
    std::fputc('\n', stderr);
}

constexpr UINT kOutputWidth = 3840;
constexpr UINT kOutputHeight = 2160;
// The display mode's refresh, once a swap chain has picked it (59.94 Hz until then).
std::atomic<std::uint32_t> g_refresh_millihertz{59940};

// ---- COM plumbing -----------------------------------------------------------------------

template <typename Interface>
class Object : public Interface {
public:
    virtual ~Object() = default;
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --references_;
        if (left == 0) delete this;
        return left;
    }

protected:
    // Answers for each interface in `Chain` (the object's interface and its bases).
    template <typename... Chain>
    HRESULT query(REFIID iid, void** object) {
        if (object == nullptr) return E_POINTER;
        *object = nullptr;
        const bool known = iid == __uuidof(IUnknown) || ((iid == __uuidof(Chain)) || ...);
        if (!known) return E_NOINTERFACE;
        *object = static_cast<Interface*>(this);
        AddRef();
        return S_OK;
    }

private:
    std::atomic<ULONG> references_{1};
};

// ---- Vulkan ------------------------------------------------------------------------------

struct Vulkan {
#define PS5_VK_FUNCTIONS(X)                       \
    X(vkGetPhysicalDeviceDisplayPropertiesKHR)    \
    X(vkGetPhysicalDeviceDisplayPlanePropertiesKHR) \
    X(vkGetDisplayModePropertiesKHR)              \
    X(vkGetDisplayPlaneSupportedDisplaysKHR)      \
    X(vkGetDisplayPlaneCapabilitiesKHR)           \
    X(vkCreateDisplayPlaneSurfaceKHR)             \
    X(vkDestroySurfaceKHR)                        \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)  \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR)       \
    X(vkGetPhysicalDeviceSurfaceSupportKHR)       \
    X(vkGetPhysicalDeviceMemoryProperties)        \
    X(vkGetDeviceProcAddr)
#define PS5_VK_DEVICE_FUNCTIONS(X) \
    X(vkCreateSwapchainKHR)        \
    X(vkDestroySwapchainKHR)       \
    X(vkGetSwapchainImagesKHR)     \
    X(vkAcquireNextImageKHR)       \
    X(vkQueuePresentKHR)           \
    X(vkCreateImage)               \
    X(vkDestroyImage)              \
    X(vkGetImageMemoryRequirements) \
    X(vkAllocateMemory)            \
    X(vkFreeMemory)                \
    X(vkBindImageMemory)           \
    X(vkCreateCommandPool)         \
    X(vkDestroyCommandPool)        \
    X(vkAllocateCommandBuffers)    \
    X(vkBeginCommandBuffer)        \
    X(vkEndCommandBuffer)          \
    X(vkResetCommandBuffer)        \
    X(vkCmdPipelineBarrier)        \
    X(vkCmdBlitImage)              \
    X(vkCmdClearColorImage)        \
    X(vkQueueSubmit)               \
    X(vkCreateFence)               \
    X(vkDestroyFence)              \
    X(vkWaitForFences)             \
    X(vkResetFences)               \
    X(vkCreateSemaphore)           \
    X(vkDestroySemaphore)          \
    X(vkDeviceWaitIdle)
#define PS5_VK_MEMBER(name) PFN_##name name = nullptr;
    PS5_VK_FUNCTIONS(PS5_VK_MEMBER)
    PS5_VK_DEVICE_FUNCTIONS(PS5_VK_MEMBER)
#undef PS5_VK_MEMBER

    bool load(VkInstance instance, VkDevice device) {
        const PFN_vkGetInstanceProcAddr loader = ps5gx::vulkan_loader();
        bool complete = true;
#define PS5_VK_LOAD(name)                                                            \
    name = reinterpret_cast<PFN_##name>(loader(instance, #name));                    \
    if (name == nullptr) {                                                           \
        say("no %s", #name);                                                         \
        complete = false;                                                            \
    }
        PS5_VK_FUNCTIONS(PS5_VK_LOAD)
#undef PS5_VK_LOAD
        if (vkGetDeviceProcAddr == nullptr) return false;
#define PS5_VK_LOAD_DEVICE(name)                                                     \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));         \
    if (name == nullptr) {                                                           \
        say("no %s", #name);                                                         \
        complete = false;                                                            \
    }
        PS5_VK_DEVICE_FUNCTIONS(PS5_VK_LOAD_DEVICE)
#undef PS5_VK_LOAD_DEVICE
        return complete;
    }
};

// ---- the output and the adapter -------------------------------------------------------------

class Output final : public Object<IDXGIOutput> {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        return query<IDXGIObject, IDXGIOutput>(iid, object);
    }
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC* desc) override {
        if (desc == nullptr) return E_INVALIDARG;
        *desc = {};
        std::wcsncpy(desc->DeviceName, L"\\\\.\\DISPLAY1", 31);
        desc->DesktopCoordinates = RECT{0, 0, static_cast<LONG>(kOutputWidth), static_cast<LONG>(kOutputHeight)};
        desc->AttachedToDesktop = TRUE;
        desc->Rotation = DXGI_MODE_ROTATION_IDENTITY;
        desc->Monitor = MonitorFromWindow(nullptr, 0);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC* mode, DXGI_MODE_DESC* closest,
                                                      IUnknown*) override {
        if (closest == nullptr) return E_INVALIDARG;
        *closest = {kOutputWidth, kOutputHeight, {g_refresh_millihertz.load(std::memory_order_relaxed), 1000},
                    mode != nullptr && mode->Format != DXGI_FORMAT_UNKNOWN ? mode->Format : DXGI_FORMAT_R8G8B8A8_UNORM,
                    DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE, DXGI_MODE_SCALING_UNSPECIFIED};
        return S_OK;
    }
};

class Adapter final : public Object<IDXGIAdapter3> {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        return query<IDXGIObject, IDXGIAdapter, IDXGIAdapter1, IDXGIAdapter2, IDXGIAdapter3>(iid, object);
    }
    HRESULT STDMETHODCALLTYPE EnumOutputs(UINT index, IDXGIOutput** output) override {
        if (output == nullptr) return E_INVALIDARG;
        *output = nullptr;
        if (index != 0) return DXGI_ERROR_NOT_FOUND;
        *output = new Output;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1* desc) override {
        if (desc == nullptr) return E_INVALIDARG;
        *desc = {};
        std::wcsncpy(desc->Description, L"AMD Radeon (PlayStation 5, RADV via vkd3d)", 127);
        desc->VendorId = 0x1002;
        desc->DeviceId = 0x13fe;
        // One pool of memory the GPU and CPU share; the title's direct memory.
        desc->DedicatedVideoMemory = SIZE_T{4} << 30;
        desc->SharedSystemMemory = SIZE_T{4} << 30;
        desc->AdapterLuid = LUID{0x5053, 0};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(UINT, DXGI_MEMORY_SEGMENT_GROUP,
                                                   DXGI_QUERY_VIDEO_MEMORY_INFO* info) override {
        if (info == nullptr) return E_INVALIDARG;
        *info = {UINT64{4} << 30, 0, UINT64{4} << 30, 0};
        return S_OK;
    }
};

// ---- the swap chain -----------------------------------------------------------------------

class SwapChain final : public Object<IDXGISwapChain3> {
public:
    ~SwapChain() override { destroy(); }

    HRESULT initialize(ID3D12CommandQueue* queue, const DXGI_SWAP_CHAIN_DESC1& desc) {
        queue_ = queue;
        queue_->AddRef();
        if (FAILED(queue_->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device_)))) {
            say("the queue has no device");
            return E_FAIL;
        }
        vk_device_ = vkd3d_get_vk_device(device_);
        vk_physical_ = vkd3d_get_vk_physical_device(device_);
        vk_instance_ = vkd3d_instance_get_vk_instance(vkd3d_instance_from_device(device_));
        queue_family_ = vkd3d_get_vk_queue_family_index(queue_);
        if (!vk_.load(vk_instance_, vk_device_)) return E_FAIL;
        desc_ = desc;
        if (desc_.Width == 0) desc_.Width = kOutputWidth;
        if (desc_.Height == 0) desc_.Height = kOutputHeight;
        if (desc_.BufferCount < 2) desc_.BufferCount = 2;
        if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                        reinterpret_cast<void**>(&fence_)))) {
            return E_FAIL;
        }
        fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!create_surface() || !create_display_swapchain() || !create_frames() || !create_buffers()) return E_FAIL;
        say("presenting %ux%u buffers (%u) to a %ux%u display, format %d", desc_.Width, desc_.Height,
            desc_.BufferCount, extent_.width, extent_.height, static_cast<int>(display_format_));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        return query<IDXGIObject, IDXGIDeviceSubObject, IDXGISwapChain, IDXGISwapChain1, IDXGISwapChain2,
                     IDXGISwapChain3>(iid, object);
    }

    HRESULT STDMETHODCALLTYPE Present(UINT, UINT flags) override {
        if ((flags & DXGI_PRESENT_TEST) != 0) return S_OK;
        if (!wait_for_submission()) return DXGI_ERROR_DEVICE_REMOVED;
        Frame& frame = frames_[frame_index_];
        vk_.vkWaitForFences(vk_device_, 1, &frame.done, VK_TRUE, UINT64_MAX);
        std::uint32_t image_index = 0;
        VkResult result = vk_.vkAcquireNextImageKHR(vk_device_, display_swapchain_, UINT64_MAX, frame.acquired,
                                                    VK_NULL_HANDLE, &image_index);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            say("vkAcquireNextImageKHR: %d", static_cast<int>(result));
            return DXGI_ERROR_DEVICE_REMOVED;
        }
        vk_.vkResetFences(vk_device_, 1, &frame.done);
        record_copy(frame.commands, buffers_[current_].image, display_images_[image_index]);

        const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &frame.acquired;
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &frame.commands;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &copied_[image_index];
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &copied_[image_index];
        present.swapchainCount = 1;
        present.pSwapchains = &display_swapchain_;
        present.pImageIndices = &image_index;
        VkQueue const queue = vkd3d_acquire_vk_queue(queue_);
        result = vk_.vkQueueSubmit(queue, 1, &submit, frame.done);
        const VkResult presented = result == VK_SUCCESS ? vk_.vkQueuePresentKHR(queue, &present) : result;
        vkd3d_release_vk_queue(queue_);
        if (result != VK_SUCCESS || (presented != VK_SUCCESS && presented != VK_SUBOPTIMAL_KHR)) {
            say("present failed: submit %d, present %d", static_cast<int>(result), static_cast<int>(presented));
            return DXGI_ERROR_DEVICE_REMOVED;
        }
        frame_index_ = (frame_index_ + 1) % frames_.size();
        current_ = (current_ + 1) % desc_.BufferCount;
        ++present_count_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetBuffer(UINT index, REFIID iid, void** surface) override {
        if (surface == nullptr) return E_POINTER;
        *surface = nullptr;
        if (index >= buffers_.size() || buffers_[index].resource == nullptr) return DXGI_ERROR_INVALID_CALL;
        return buffers_[index].resource->QueryInterface(iid, surface);
    }

    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL fullscreen, IDXGIOutput*) override {
        fullscreen_ = fullscreen;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* fullscreen, IDXGIOutput** target) override {
        if (fullscreen != nullptr) *fullscreen = fullscreen_;
        if (target != nullptr) *target = fullscreen_ ? new Output : nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count, UINT width, UINT height, DXGI_FORMAT format,
                                            UINT flags) override {
        DXGI_SWAP_CHAIN_DESC1 wanted = desc_;
        if (count != 0) wanted.BufferCount = std::max(count, 2u);
        wanted.Width = width != 0 ? width : kOutputWidth;
        wanted.Height = height != 0 ? height : kOutputHeight;
        if (format != DXGI_FORMAT_UNKNOWN) wanted.Format = format;
        wanted.Flags = flags;
        if (wanted.BufferCount == desc_.BufferCount && wanted.Width == desc_.Width &&
            wanted.Height == desc_.Height && wanted.Format == desc_.Format) {
            desc_.Flags = flags;
            current_ = 0;
            return S_OK;
        }
        if (!wait_for_submission()) return DXGI_ERROR_DEVICE_REMOVED;
        VkQueue const queue = vkd3d_acquire_vk_queue(queue_);
        (void)queue;
        vk_.vkDeviceWaitIdle(vk_device_);
        vkd3d_release_vk_queue(queue_);
        destroy_buffers();
        desc_ = wanted;
        current_ = 0;
        return create_buffers() ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** output) override {
        if (output == nullptr) return E_INVALIDARG;
        *output = new Output;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* statistics) override {
        if (statistics == nullptr) return E_INVALIDARG;
        *statistics = {};
        statistics->PresentCount = present_count_;
        statistics->PresentRefreshCount = present_count_;
        statistics->SyncRefreshCount = present_count_;
        QueryPerformanceCounter(&statistics->SyncQPCTime);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* desc) override {
        if (desc == nullptr) return E_INVALIDARG;
        *desc = desc_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT) override { return S_OK; }
    // The caller owns (and closes) the handle, as with DXGI's.
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override { return CreateEventW(nullptr, TRUE, TRUE, nullptr); }
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override { return current_; }

private:
    struct Buffer {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        ID3D12Resource* resource = nullptr;
    };
    struct Frame {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence done = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
    };

    // Every Direct3D 12 submission so far has reached the Vulkan queue.
    bool wait_for_submission() {
        const UINT64 value = ++fence_value_;
        if (FAILED(vkd3d_queue_signal_on_cpu(queue_, fence_, value))) return false;
        if (fence_->GetCompletedValue() >= value) return true;
        if (FAILED(fence_->SetEventOnCompletion(value, fence_event_))) return false;
        return WaitForSingleObject(fence_event_, 10000) == WAIT_OBJECT_0;
    }

    bool create_surface() {
        std::uint32_t count = 0;
        vk_.vkGetPhysicalDeviceDisplayPropertiesKHR(vk_physical_, &count, nullptr);
        std::vector<VkDisplayPropertiesKHR> displays(count);
        vk_.vkGetPhysicalDeviceDisplayPropertiesKHR(vk_physical_, &count, displays.data());
        if (displays.empty()) {
            say("no display");
            return false;
        }
        VkDisplayKHR const display = displays[0].display;
        vk_.vkGetDisplayModePropertiesKHR(vk_physical_, display, &count, nullptr);
        std::vector<VkDisplayModePropertiesKHR> modes(count);
        vk_.vkGetDisplayModePropertiesKHR(vk_physical_, display, &count, modes.data());
        if (modes.empty()) {
            say("the display has no mode");
            return false;
        }
        // The largest mode at its fastest refresh: 119.88 Hz where the display
        // offers it (the WSI keeps it only on a TV that runs it as VRR, which then
        // shows 60 frames/s as 60 Hz). The FRAME RATE setting can then go above 60
        // (Nebula's frames in between) without a restart.
        const VkDisplayModePropertiesKHR* best = &modes[0];
        auto score = [](const VkDisplayModePropertiesKHR& mode) {
            const auto& p = mode.parameters;
            const long long area = static_cast<long long>(p.visibleRegion.width) * p.visibleRegion.height;
            return area * 1000000 + static_cast<long long>(p.refreshRate);
        };
        for (const auto& mode : modes) {
            say("display offers %ux%u at %u mHz", mode.parameters.visibleRegion.width,
                mode.parameters.visibleRegion.height, mode.parameters.refreshRate);
            if (score(mode) > score(*best)) best = &mode;
        }
        vk_.vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk_physical_, &count, nullptr);
        std::vector<VkDisplayPlanePropertiesKHR> planes(count);
        vk_.vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk_physical_, &count, planes.data());
        std::uint32_t plane_index = UINT32_MAX;
        for (std::uint32_t plane = 0; plane < planes.size() && plane_index == UINT32_MAX; ++plane) {
            if (planes[plane].currentDisplay != VK_NULL_HANDLE && planes[plane].currentDisplay != display) continue;
            std::uint32_t supported_count = 0;
            vk_.vkGetDisplayPlaneSupportedDisplaysKHR(vk_physical_, plane, &supported_count, nullptr);
            std::vector<VkDisplayKHR> supported(supported_count);
            vk_.vkGetDisplayPlaneSupportedDisplaysKHR(vk_physical_, plane, &supported_count, supported.data());
            if (std::find(supported.begin(), supported.end(), display) != supported.end()) plane_index = plane;
        }
        if (plane_index == UINT32_MAX) plane_index = 0;
        g_refresh_millihertz.store(best->parameters.refreshRate, std::memory_order_relaxed);
        // Windows reports whole hertz rounded down at high rates (119 for 119.88).
        compat::set_display_refresh_hz(best->parameters.refreshRate >= 100000 ? best->parameters.refreshRate / 1000 : 60);
        say("display mode %ux%u at %u mHz", best->parameters.visibleRegion.width,
            best->parameters.visibleRegion.height, best->parameters.refreshRate);
        VkDisplayPlaneCapabilitiesKHR capabilities{};
        vk_.vkGetDisplayPlaneCapabilitiesKHR(vk_physical_, best->displayMode, plane_index, &capabilities);
        VkDisplayPlaneAlphaFlagBitsKHR alpha = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
        for (VkDisplayPlaneAlphaFlagBitsKHR candidate :
             {VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR, VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR,
              VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR, VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR}) {
            if ((capabilities.supportedAlpha & candidate) != 0) {
                alpha = candidate;
                break;
            }
        }
        VkDisplaySurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR};
        info.displayMode = best->displayMode;
        info.planeIndex = plane_index;
        info.planeStackIndex = planes.empty() ? 0 : planes[plane_index].currentStackIndex;
        info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.globalAlpha = 1.0f;
        info.alphaMode = alpha;
        info.imageExtent = best->parameters.visibleRegion;
        const VkResult result = vk_.vkCreateDisplayPlaneSurfaceKHR(vk_instance_, &info, nullptr, &surface_);
        if (result != VK_SUCCESS) {
            say("vkCreateDisplayPlaneSurfaceKHR: %d", static_cast<int>(result));
            return false;
        }
        VkBool32 supported = VK_FALSE;
        vk_.vkGetPhysicalDeviceSurfaceSupportKHR(vk_physical_, queue_family_, surface_, &supported);
        if (!supported) say("queue family %u cannot present to the display (trying anyway)", queue_family_);
        return true;
    }

    bool create_display_swapchain() {
        VkSurfaceCapabilitiesKHR capabilities{};
        vk_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_, surface_, &capabilities);
        std::uint32_t count = 0;
        vk_.vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_, surface_, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(count);
        vk_.vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_, surface_, &count, formats.data());
        if (formats.empty()) {
            say("the display has no format");
            return false;
        }
        VkSurfaceFormatKHR chosen = formats[0];
        for (const VkSurfaceFormatKHR& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) {
                chosen = format;
                break;
            }
        }
        display_format_ = chosen.format;
        extent_ = capabilities.currentExtent;
        if (extent_.width == UINT32_MAX || extent_.width == 0) extent_ = {kOutputWidth, kOutputHeight};
        std::uint32_t images = std::max(3u, capabilities.minImageCount);
        if (capabilities.maxImageCount != 0) images = std::min(images, capabilities.maxImageCount);
        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface_;
        info.minImageCount = images;
        info.imageFormat = chosen.format;
        info.imageColorSpace = chosen.colorSpace;
        info.imageExtent = extent_;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        const VkResult result = vk_.vkCreateSwapchainKHR(vk_device_, &info, nullptr, &display_swapchain_);
        if (result != VK_SUCCESS) {
            say("vkCreateSwapchainKHR: %d", static_cast<int>(result));
            return false;
        }
        vk_.vkGetSwapchainImagesKHR(vk_device_, display_swapchain_, &count, nullptr);
        display_images_.resize(count);
        vk_.vkGetSwapchainImagesKHR(vk_device_, display_swapchain_, &count, display_images_.data());
        copied_.resize(count);
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (VkSemaphore& copied : copied_) vk_.vkCreateSemaphore(vk_device_, &semaphore, nullptr, &copied);
        return true;
    }

    bool create_frames() {
        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = queue_family_;
        if (vk_.vkCreateCommandPool(vk_device_, &pool, nullptr, &command_pool_) != VK_SUCCESS) return false;
        for (Frame& frame : frames_) {
            VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocate.commandPool = command_pool_;
            allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate.commandBufferCount = 1;
            if (vk_.vkAllocateCommandBuffers(vk_device_, &allocate, &frame.commands) != VK_SUCCESS) return false;
            VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vk_.vkCreateFence(vk_device_, &fence, nullptr, &frame.done);
            VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vk_.vkCreateSemaphore(vk_device_, &semaphore, nullptr, &frame.acquired);
        }
        return true;
    }

    bool create_buffers() {
        VkPhysicalDeviceMemoryProperties memory{};
        vk_.vkGetPhysicalDeviceMemoryProperties(vk_physical_, &memory);
        buffers_.assign(desc_.BufferCount, Buffer{});
        for (Buffer& buffer : buffers_) {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = vkd3d_get_vk_format(desc_.Format);
            info.extent = {desc_.Width, desc_.Height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (vk_.vkCreateImage(vk_device_, &info, nullptr, &buffer.image) != VK_SUCCESS) {
                say("vkCreateImage failed for a %ux%u buffer", desc_.Width, desc_.Height);
                return false;
            }
            VkMemoryRequirements requirements{};
            vk_.vkGetImageMemoryRequirements(vk_device_, buffer.image, &requirements);
            std::uint32_t type = UINT32_MAX;
            for (std::uint32_t i = 0; i < memory.memoryTypeCount && type == UINT32_MAX; ++i) {
                if ((requirements.memoryTypeBits & (1u << i)) != 0 &&
                    (memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
                    type = i;
                }
            }
            VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocate.allocationSize = requirements.size;
            allocate.memoryTypeIndex = type;
            if (type == UINT32_MAX ||
                vk_.vkAllocateMemory(vk_device_, &allocate, nullptr, &buffer.memory) != VK_SUCCESS ||
                vk_.vkBindImageMemory(vk_device_, buffer.image, buffer.memory, 0) != VK_SUCCESS) {
                say("no memory for a %ux%u buffer", desc_.Width, desc_.Height);
                return false;
            }
            vkd3d_image_resource_create_info resource{};
            resource.type = VKD3D_STRUCTURE_TYPE_IMAGE_RESOURCE_CREATE_INFO;
            resource.vk_image = buffer.image;
            resource.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resource.desc.Width = desc_.Width;
            resource.desc.Height = desc_.Height;
            resource.desc.DepthOrArraySize = 1;
            resource.desc.MipLevels = 1;
            resource.desc.Format = desc_.Format;
            resource.desc.SampleDesc.Count = 1;
            resource.desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resource.desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            resource.flags = VKD3D_RESOURCE_INITIAL_STATE_TRANSITION | VKD3D_RESOURCE_PRESENT_STATE_TRANSITION;
            resource.present_state = D3D12_RESOURCE_STATE_COPY_SOURCE;
            const HRESULT result = vkd3d_create_image_resource(device_, &resource, &buffer.resource);
            if (FAILED(result)) {
                say("vkd3d_create_image_resource: 0x%08x", static_cast<unsigned>(result));
                return false;
            }
        }
        return true;
    }

    void record_copy(VkCommandBuffer commands, VkImage source, VkImage target) {
        vk_.vkResetCommandBuffer(commands, 0);
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vk_.vkBeginCommandBuffer(commands, &begin);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageMemoryBarrier barriers[2]{};
        // The buffer: already in the copy-source layout (its PRESENT state); make
        // the frame's writes visible to the copy.
        barriers[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barriers[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = source;
        barriers[0].subresourceRange = range;
        barriers[1] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barriers[1].srcAccessMask = 0;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].image = target;
        barriers[1].subresourceRange = range;
        vk_.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                 nullptr, 0, nullptr, 2, barriers);
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[1] = {static_cast<int32_t>(desc_.Width), static_cast<int32_t>(desc_.Height), 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[1] = {static_cast<int32_t>(extent_.width), static_cast<int32_t>(extent_.height), 1};
        vk_.vkCmdBlitImage(commands, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
        VkImageMemoryBarrier to_present{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_present.dstAccessMask = 0;
        to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.image = target;
        to_present.subresourceRange = range;
        vk_.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &to_present);
        vk_.vkEndCommandBuffer(commands);
    }

    void destroy_buffers() {
        for (Buffer& buffer : buffers_) {
            if (buffer.resource != nullptr) buffer.resource->Release();
            if (buffer.image != VK_NULL_HANDLE) vk_.vkDestroyImage(vk_device_, buffer.image, nullptr);
            if (buffer.memory != VK_NULL_HANDLE) vk_.vkFreeMemory(vk_device_, buffer.memory, nullptr);
        }
        buffers_.clear();
    }

    void destroy() {
        if (vk_device_ != VK_NULL_HANDLE && vk_.vkDeviceWaitIdle != nullptr && queue_ != nullptr) {
            (void)wait_for_submission();
            VkQueue const queue = vkd3d_acquire_vk_queue(queue_);
            (void)queue;
            vk_.vkDeviceWaitIdle(vk_device_);
            vkd3d_release_vk_queue(queue_);
            destroy_buffers();
            for (Frame& frame : frames_) {
                if (frame.done != VK_NULL_HANDLE) vk_.vkDestroyFence(vk_device_, frame.done, nullptr);
                if (frame.acquired != VK_NULL_HANDLE) vk_.vkDestroySemaphore(vk_device_, frame.acquired, nullptr);
            }
            for (VkSemaphore copied : copied_) vk_.vkDestroySemaphore(vk_device_, copied, nullptr);
            if (command_pool_ != VK_NULL_HANDLE) vk_.vkDestroyCommandPool(vk_device_, command_pool_, nullptr);
            if (display_swapchain_ != VK_NULL_HANDLE) {
                vk_.vkDestroySwapchainKHR(vk_device_, display_swapchain_, nullptr);
            }
            if (surface_ != VK_NULL_HANDLE) vk_.vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
        }
        if (fence_ != nullptr) fence_->Release();
        if (fence_event_ != nullptr) CloseHandle(fence_event_);
        if (device_ != nullptr) device_->Release();
        if (queue_ != nullptr) queue_->Release();
        fence_ = nullptr;
        device_ = nullptr;
        queue_ = nullptr;
    }

    Vulkan vk_;
    ID3D12CommandQueue* queue_ = nullptr;
    ID3D12Device* device_ = nullptr;
    ID3D12Fence* fence_ = nullptr;
    UINT64 fence_value_ = 0;
    HANDLE fence_event_ = nullptr;
    VkInstance vk_instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice vk_physical_ = VK_NULL_HANDLE;
    VkDevice vk_device_ = VK_NULL_HANDLE;
    std::uint32_t queue_family_ = 0;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR display_swapchain_ = VK_NULL_HANDLE;
    VkFormat display_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> display_images_;
    std::vector<VkSemaphore> copied_;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::array<Frame, 3> frames_{};
    std::size_t frame_index_ = 0;
    std::vector<Buffer> buffers_;
    DXGI_SWAP_CHAIN_DESC1 desc_{};
    UINT current_ = 0;
    UINT present_count_ = 0;
    BOOL fullscreen_ = FALSE;
};

// ---- the factory ----------------------------------------------------------------------------

class Factory final : public Object<IDXGIFactory6> {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        return query<IDXGIObject, IDXGIFactory, IDXGIFactory1, IDXGIFactory2, IDXGIFactory3, IDXGIFactory4,
                     IDXGIFactory5, IDXGIFactory6>(iid, object);
    }
    HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT index, IDXGIAdapter1** adapter) override {
        if (adapter == nullptr) return E_INVALIDARG;
        *adapter = nullptr;
        if (index != 0) return DXGI_ERROR_NOT_FOUND;
        *adapter = new Adapter;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT index, DXGI_GPU_PREFERENCE, REFIID iid,
                                                         void** adapter) override {
        if (adapter == nullptr) return E_INVALIDARG;
        *adapter = nullptr;
        if (index != 0) return DXGI_ERROR_NOT_FOUND;
        Adapter* const object = new Adapter;
        const HRESULT result = object->QueryInterface(iid, adapter);
        object->Release();
        return result;
    }
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE feature, void* data, UINT size) override {
        if (feature != DXGI_FEATURE_PRESENT_ALLOW_TEARING || data == nullptr || size != sizeof(BOOL)) {
            return E_INVALIDARG;
        }
        *static_cast<BOOL*>(data) = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown* device, HWND, const DXGI_SWAP_CHAIN_DESC1* desc,
                                                     const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
                                                     IDXGISwapChain1** swap_chain) override {
        if (device == nullptr || desc == nullptr || swap_chain == nullptr) return E_INVALIDARG;
        *swap_chain = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        if (FAILED(device->QueryInterface(__uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue)))) {
            say("CreateSwapChainForHwnd needs a command queue");
            return E_INVALIDARG;
        }
        SwapChain* const chain = new SwapChain;
        const HRESULT result = chain->initialize(queue, *desc);
        queue->Release();
        if (FAILED(result)) {
            chain->Release();
            return result;
        }
        *swap_chain = chain;
        return S_OK;
    }
};

}  // namespace

extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT, REFIID iid, void** factory) {
    if (factory == nullptr) return E_POINTER;
    Factory* const object = new Factory;
    const HRESULT result = object->QueryInterface(iid, factory);
    object->Release();
    return result;
}

extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID iid, void** factory) { return CreateDXGIFactory2(0, iid, factory); }

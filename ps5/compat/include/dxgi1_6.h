// Super Mario Galaxy for the PlayStation 5 - DXGI, as far as Nebula's renderer
// uses it: one adapter (the console's GPU), one output (the TV) and a flip swap
// chain, all implemented by ps5/gx/dxgi_ps5.cpp over Vulkan's VK_KHR_display.
//
// The interfaces declare only the methods Nebula calls; nothing outside this
// build implements or calls them, so their tables need not match Windows'.
// The formats and DXGI_SAMPLE_DESC are vkd3d's (d3d12.h).
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "d3d12.h"

#define DXGI_ERROR_INVALID_CALL ((HRESULT)0x887A0001)
#define DXGI_ERROR_NOT_FOUND ((HRESULT)0x887A0002)
#define DXGI_ERROR_MORE_DATA ((HRESULT)0x887A0003)
#define DXGI_ERROR_UNSUPPORTED ((HRESULT)0x887A0004)
#define DXGI_ERROR_DEVICE_REMOVED ((HRESULT)0x887A0005)
#define DXGI_ERROR_DEVICE_HUNG ((HRESULT)0x887A0006)
#define DXGI_ERROR_DEVICE_RESET ((HRESULT)0x887A0007)
#define DXGI_ERROR_WAS_STILL_DRAWING ((HRESULT)0x887A000A)
#define DXGI_ERROR_DRIVER_INTERNAL_ERROR ((HRESULT)0x887A0020)
#define DXGI_ERROR_NOT_CURRENTLY_AVAILABLE ((HRESULT)0x887A0022)
#define DXGI_STATUS_OCCLUDED ((HRESULT)0x087A0001)

#define DXGI_USAGE_SHADER_INPUT 0x00000010u
#define DXGI_USAGE_RENDER_TARGET_OUTPUT 0x00000020u
#define DXGI_CREATE_FACTORY_DEBUG 0x1u
#define DXGI_MWA_NO_WINDOW_CHANGES 0x1u
#define DXGI_MWA_NO_ALT_ENTER 0x2u
#define DXGI_MWA_NO_PRINT_SCREEN 0x4u
#define DXGI_PRESENT_TEST 0x00000001u
#define DXGI_PRESENT_DO_NOT_WAIT 0x00000008u
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200u

typedef enum DXGI_ADAPTER_FLAG {
    DXGI_ADAPTER_FLAG_NONE = 0,
    DXGI_ADAPTER_FLAG_REMOTE = 1,
    DXGI_ADAPTER_FLAG_SOFTWARE = 2,
} DXGI_ADAPTER_FLAG;

typedef enum DXGI_SWAP_EFFECT {
    DXGI_SWAP_EFFECT_DISCARD = 0,
    DXGI_SWAP_EFFECT_SEQUENTIAL = 1,
    DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL = 3,
    DXGI_SWAP_EFFECT_FLIP_DISCARD = 4,
} DXGI_SWAP_EFFECT;

typedef enum DXGI_SWAP_CHAIN_FLAG {
    DXGI_SWAP_CHAIN_FLAG_NONPREROTATED = 1,
    DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH = 2,
    DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE = 4,
    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT = 64,
    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING = 2048,
} DXGI_SWAP_CHAIN_FLAG;

typedef enum DXGI_SCALING {
    DXGI_SCALING_STRETCH = 0,
    DXGI_SCALING_NONE = 1,
    DXGI_SCALING_ASPECT_RATIO_STRETCH = 2,
} DXGI_SCALING;

typedef enum DXGI_ALPHA_MODE {
    DXGI_ALPHA_MODE_UNSPECIFIED = 0,
    DXGI_ALPHA_MODE_PREMULTIPLIED = 1,
    DXGI_ALPHA_MODE_STRAIGHT = 2,
    DXGI_ALPHA_MODE_IGNORE = 3,
} DXGI_ALPHA_MODE;

typedef enum DXGI_MODE_SCANLINE_ORDER {
    DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED = 0,
    DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE = 1,
    DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST = 2,
    DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST = 3,
} DXGI_MODE_SCANLINE_ORDER;

typedef enum DXGI_MODE_SCALING {
    DXGI_MODE_SCALING_UNSPECIFIED = 0,
    DXGI_MODE_SCALING_CENTERED = 1,
    DXGI_MODE_SCALING_STRETCHED = 2,
} DXGI_MODE_SCALING;

typedef enum DXGI_MODE_ROTATION {
    DXGI_MODE_ROTATION_UNSPECIFIED = 0,
    DXGI_MODE_ROTATION_IDENTITY = 1,
    DXGI_MODE_ROTATION_ROTATE90 = 2,
    DXGI_MODE_ROTATION_ROTATE180 = 3,
    DXGI_MODE_ROTATION_ROTATE270 = 4,
} DXGI_MODE_ROTATION;

typedef enum DXGI_MEMORY_SEGMENT_GROUP {
    DXGI_MEMORY_SEGMENT_GROUP_LOCAL = 0,
    DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL = 1,
} DXGI_MEMORY_SEGMENT_GROUP;

typedef enum DXGI_GPU_PREFERENCE {
    DXGI_GPU_PREFERENCE_UNSPECIFIED = 0,
    DXGI_GPU_PREFERENCE_MINIMUM_POWER = 1,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE = 2,
} DXGI_GPU_PREFERENCE;

typedef enum DXGI_FEATURE {
    DXGI_FEATURE_PRESENT_ALLOW_TEARING = 0,
} DXGI_FEATURE;

typedef struct DXGI_RATIONAL {
    UINT Numerator;
    UINT Denominator;
} DXGI_RATIONAL;

typedef struct DXGI_MODE_DESC {
    UINT Width;
    UINT Height;
    DXGI_RATIONAL RefreshRate;
    DXGI_FORMAT Format;
    DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
    DXGI_MODE_SCALING Scaling;
} DXGI_MODE_DESC;

typedef struct DXGI_SWAP_CHAIN_DESC1 {
    UINT Width;
    UINT Height;
    DXGI_FORMAT Format;
    BOOL Stereo;
    DXGI_SAMPLE_DESC SampleDesc;
    UINT BufferUsage;
    UINT BufferCount;
    DXGI_SCALING Scaling;
    DXGI_SWAP_EFFECT SwapEffect;
    DXGI_ALPHA_MODE AlphaMode;
    UINT Flags;
} DXGI_SWAP_CHAIN_DESC1;

typedef struct DXGI_SWAP_CHAIN_FULLSCREEN_DESC {
    DXGI_RATIONAL RefreshRate;
    DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
    DXGI_MODE_SCALING Scaling;
    BOOL Windowed;
} DXGI_SWAP_CHAIN_FULLSCREEN_DESC;

typedef struct DXGI_OUTPUT_DESC {
    WCHAR DeviceName[32];
    RECT DesktopCoordinates;
    BOOL AttachedToDesktop;
    DXGI_MODE_ROTATION Rotation;
    HMONITOR Monitor;
} DXGI_OUTPUT_DESC;

typedef struct DXGI_FRAME_STATISTICS {
    UINT PresentCount;
    UINT PresentRefreshCount;
    UINT SyncRefreshCount;
    LARGE_INTEGER SyncQPCTime;
    LARGE_INTEGER SyncGPUTime;
} DXGI_FRAME_STATISTICS;

typedef struct DXGI_ADAPTER_DESC1 {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
    UINT Flags;
} DXGI_ADAPTER_DESC1;

typedef struct DXGI_QUERY_VIDEO_MEMORY_INFO {
    UINT64 Budget;
    UINT64 CurrentUsage;
    UINT64 AvailableForReservation;
    UINT64 CurrentReservation;
} DXGI_QUERY_VIDEO_MEMORY_INFO;

struct IDXGIObject : IUnknown {};

struct IDXGIOutput : IDXGIObject {
    virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC* desc) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC* mode, DXGI_MODE_DESC* closest,
                                                              IUnknown* device) = 0;
};

struct IDXGIAdapter : IDXGIObject {
    virtual HRESULT STDMETHODCALLTYPE EnumOutputs(UINT index, IDXGIOutput** output) = 0;
};
struct IDXGIAdapter1 : IDXGIAdapter {
    virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1* desc) = 0;
};
struct IDXGIAdapter2 : IDXGIAdapter1 {};
struct IDXGIAdapter3 : IDXGIAdapter2 {
    virtual HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(UINT node, DXGI_MEMORY_SEGMENT_GROUP group,
                                                           DXGI_QUERY_VIDEO_MEMORY_INFO* info) = 0;
};

struct IDXGIDeviceSubObject : IDXGIObject {};
struct IDXGISwapChain : IDXGIDeviceSubObject {
    virtual HRESULT STDMETHODCALLTYPE Present(UINT sync_interval, UINT flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT index, REFIID iid, void** surface) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL fullscreen, IDXGIOutput* target) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* fullscreen, IDXGIOutput** target) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count, UINT width, UINT height, DXGI_FORMAT format,
                                                    UINT flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* mode) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** output) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* statistics) = 0;
};
struct IDXGISwapChain1 : IDXGISwapChain {
    virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* desc) = 0;
};
struct IDXGISwapChain2 : IDXGISwapChain1 {
    virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT latency) = 0;
    virtual HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() = 0;
};
struct IDXGISwapChain3 : IDXGISwapChain2 {
    virtual UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() = 0;
};

struct IDXGIFactory : IDXGIObject {
    virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND window, UINT flags) = 0;
};
struct IDXGIFactory1 : IDXGIFactory {
    virtual HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT index, IDXGIAdapter1** adapter) = 0;
};
struct IDXGIFactory2 : IDXGIFactory1 {
    virtual HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown* device, HWND window,
                                                             const DXGI_SWAP_CHAIN_DESC1* desc,
                                                             const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
                                                             IDXGIOutput* restrict_to_output,
                                                             IDXGISwapChain1** swap_chain) = 0;
};
struct IDXGIFactory3 : IDXGIFactory2 {};
struct IDXGIFactory4 : IDXGIFactory3 {};
struct IDXGIFactory5 : IDXGIFactory4 {
    virtual HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE feature, void* data, UINT size) = 0;
};
struct IDXGIFactory6 : IDXGIFactory5 {
    virtual HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT index, DXGI_GPU_PREFERENCE preference,
                                                                 REFIID iid, void** adapter) = 0;
};

extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID iid, void** factory);
extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID iid, void** factory);

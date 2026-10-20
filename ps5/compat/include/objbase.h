// Super Mario Galaxy for the PlayStation 5 - COM: IUnknown, CoInitializeEx and the
// task allocator. CoCreateInstance finds no class: the only one Nebula asks for
// is the audio endpoint enumerator, and the audio sink then uses the default output.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#include <type_traits>

// IUnknown as COM lays it out (three methods, no destructor in the table), so
// vkd3d's Direct3D 12 objects, written in C, can be called through it.
#define __IUnknown_FWD_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
};
typedef IUnknown* LPUNKNOWN;

// __uuidof(Type or expression): the interface's real GUID where a header
// declares one (__CRT_UUID_DECL, as vkd3d's Direct3D 12 headers do), otherwise
// a distinct GUID for each type (nothing compares those with Windows' values).
template <typename T>
struct ComUuid {
    static const GUID& value() noexcept {
        static const GUID id = {static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&id)), 0, 0, {}};
        return id;
    }
};
#define __CRT_UUID_DECL(type, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern "C++" { \
    template <> \
    struct ComUuid<type> { \
        static const GUID& value() noexcept { \
            static const GUID id = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}; \
            return id; \
        } \
    }; \
    }
template <typename T>
using ComUuidType = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;
#define __uuidof(type) (ComUuid<ComUuidType<__typeof__(type)>>::value())
#define IID_PPV_ARGS(pp) \
    ComUuid<ComUuidType<decltype(**(pp))>>::value(), reinterpret_cast<void**>(pp)
__CRT_UUID_DECL(IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)
static const GUID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

typedef enum tagCOINIT {
    COINIT_APARTMENTTHREADED = 0x2,
    COINIT_MULTITHREADED = 0x0,
    COINIT_DISABLE_OLE1DDE = 0x4,
    COINIT_SPEED_OVER_MEMORY = 0x8
} COINIT;
#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_ALL 0x17
HRESULT CoInitializeEx(LPVOID reserved, DWORD flags) noexcept;
HRESULT CoInitialize(LPVOID reserved) noexcept;
void CoUninitialize() noexcept;
HRESULT CoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD context, REFIID iid, LPVOID* object) noexcept;
void CoTaskMemFree(LPVOID memory) noexcept;
HRESULT CLSIDFromString(LPCWSTR text, CLSID* clsid) noexcept;
HRESULT IIDFromString(LPCWSTR text, IID* iid) noexcept;
#define NOERROR 0
LPVOID CoTaskMemAlloc(SIZE_T bytes) noexcept;

// Super Mario Galaxy for the PlayStation 5 - COM: IUnknown, CoInitializeEx and the
// task allocator. CoCreateInstance finds no class: the only one Nebula asks for
// is the audio endpoint enumerator, and the audio sink then uses the default output.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#include <type_traits>

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
    virtual ~IUnknown() = default;
};
typedef IUnknown* LPUNKNOWN;

// __uuidof(Type): a distinct GUID for each interface type (the console has no
// registry of real ones, and nothing compares them with Windows' values).
template <typename T>
struct ComUuid {
    static const GUID& value() noexcept {
        static const GUID id = {static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&id)), 0, 0, {}};
        return id;
    }
};
#define __uuidof(type) (ComUuid<type>::value())
#define IID_PPV_ARGS(pp) \
    ComUuid<std::remove_cv_t<std::remove_reference_t<decltype(**(pp))>>>::value(), reinterpret_cast<void**>(pp)

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

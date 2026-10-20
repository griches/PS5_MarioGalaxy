// Super Mario Galaxy for the PlayStation 5 - the audio endpoint interfaces, which
// the console never hands out (CoCreateInstance fails, objbase.h).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "objbase.h"
typedef enum { eRender, eCapture, eAll } EDataFlow;
typedef enum { eConsole, eMultimedia, eCommunications } ERole;
#define DEVICE_STATE_ACTIVE 0x00000001
struct IPropertyStore;
struct IMMDevice : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Activate(REFIID iid, DWORD context, void* parameters, void** interface_) = 0;
    virtual HRESULT STDMETHODCALLTYPE OpenPropertyStore(DWORD access, IPropertyStore** store) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetId(LPWSTR* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetState(DWORD* state) = 0;
};
struct IMMDeviceCollection : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE Item(UINT index, IMMDevice** device) = 0;
};
struct IMMNotificationClient {
    virtual HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD state) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR id, const void* key) = 0;
};
struct IMMDeviceEnumerator : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow flow, DWORD mask, IMMDeviceCollection** devices) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow flow, ERole role, IMMDevice** endpoint) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR id, IMMDevice** device) = 0;
    virtual HRESULT STDMETHODCALLTYPE RegisterEndpointNotificationCallback(IMMNotificationClient* client) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnregisterEndpointNotificationCallback(IMMNotificationClient* client) = 0;
};
struct MMDeviceEnumerator;

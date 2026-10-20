// Super Mario Galaxy for the PlayStation 5 - Direct3D 12: vkd3d's declarations
// (vkd3d_com.h explains the arrangement) and the entry points ps5/gx/d3d12_ps5.cpp
// provides over vkd3d.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "vkd3d_com.h"
#include <vkd3d_d3d12.h>

extern "C" {
HRESULT WINAPI D3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL minimum_feature_level, REFIID iid,
                                 void** device);
HRESULT WINAPI D3D12GetDebugInterface(REFIID iid, void** debug);
HRESULT WINAPI D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc, D3D_ROOT_SIGNATURE_VERSION version,
                                           ID3DBlob** blob, ID3DBlob** error_blob);
HRESULT WINAPI D3D12SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc, ID3DBlob** blob,
                                                    ID3DBlob** error_blob);
}

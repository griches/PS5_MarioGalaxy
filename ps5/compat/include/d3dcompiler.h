// Super Mario Galaxy for the PlayStation 5 - the HLSL compiler: vkd3d-shader's
// (vkd3d-utils' D3DCompile), wrapped by ps5/gx/d3d12_ps5.cpp so Shader Model 5.1
// sources without register spaces compile as 5.0 (vkd3d's 5.1 output lacks
// resource arrays, which 5.0 splits into one register each).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "vkd3d_com.h"
#include <vkd3d_d3dcommon.h>
extern "C" {
#include <vkd3d_d3dcompiler.h>
}

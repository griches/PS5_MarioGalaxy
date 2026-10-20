// Super Mario Galaxy for the PlayStation 5 - what vkd3d's widl-generated COM
// headers (vkd3d_d3d12.h and friends) expect from windows.h.
//
// Nebula's renderer is Direct3D 12; on the console it runs on vkd3d, Wine's
// Direct3D 12 over Vulkan. vkd3d's headers declare the same C++ interfaces as
// the Windows SDK's, laid out like vkd3d's C objects: the console build of
// vkd3d uses the System V calling convention (VKD3D_SYSV_ABI, ps5/patches/
// vkd3d.patch), as this layer does, and methods that return a structure take
// a hidden result pointer as MSVC's do (WIDL_EXPLICIT_AGGREGATE_RETURNS).
// vkd3d_windows.h steps aside when a windows.h came first (_INC_WINDOWS).
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#include "objbase.h"

#ifndef _INC_WINDOWS
#define _INC_WINDOWS
#endif
#define COM_NO_WINDOWS_H
#define VKD3D_HOST_WINDOWS_TYPES
#define WIDL_EXPLICIT_AGGREGATE_RETURNS
#ifndef interface
#define interface struct
#endif
#define BEGIN_INTERFACE
#define END_INTERFACE
#define CONST_VTBL const
#ifndef EXTERN_C
#define EXTERN_C extern "C"
#endif
#ifndef DECLSPEC_HIDDEN
#define DECLSPEC_HIDDEN
#endif
#define __C89_NAMELESS
#define __C89_NAMELESSUNIONNAME
#ifndef DEFINE_ENUM_FLAG_OPERATORS
#define DEFINE_ENUM_FLAG_OPERATORS(type) \
    extern "C++" { \
    inline type operator&(type x, type y) { return (type)((int)x & (int)y); } \
    inline type operator&=(type& x, type y) { return (type&)((int&)x &= (int)y); } \
    inline type operator~(type x) { return (type)~(int)x; } \
    inline type operator|(type x, type y) { return (type)((int)x | (int)y); } \
    inline type operator|=(type& x, type y) { return (type&)((int&)x |= (int)y); } \
    inline type operator^(type x, type y) { return (type)((int)x ^ (int)y); } \
    inline type operator^=(type& x, type y) { return (type&)((int&)x ^= (int)y); } \
    }
#endif

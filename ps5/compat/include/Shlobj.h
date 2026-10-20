// Super Mario Galaxy for the PlayStation 5 - known folders: every per-user folder
// is the title's own /app0/userdata.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "objbase.h"
#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define CSIDL_PERSONAL 0x0005
#define CSIDL_PROFILE 0x0028
#define CSIDL_FLAG_CREATE 0x8000
#define SHGFP_TYPE_CURRENT 0
#define KF_FLAG_DEFAULT 0x00000000
#define KF_FLAG_CREATE 0x00008000
typedef GUID KNOWNFOLDERID;
typedef const KNOWNFOLDERID& REFKNOWNFOLDERID;
DEFINE_GUID(FOLDERID_LocalAppData, 0xF1B32785, 0x6FBA, 0x4FCF, 0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91);
DEFINE_GUID(FOLDERID_RoamingAppData, 0x3EB685DB, 0x65F9, 0x4CF6, 0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D);
DEFINE_GUID(FOLDERID_Documents, 0xFDD39AD0, 0x238F, 0x46AF, 0xAD, 0xB4, 0x6C, 0x85, 0x48, 0x03, 0x69, 0xC7);
DEFINE_GUID(FOLDERID_SavedGames, 0x4C5C32FF, 0xBB9D, 0x43b0, 0xB5, 0xB4, 0x2D, 0x72, 0xE5, 0x4E, 0xAA, 0xA4);
DEFINE_GUID(FOLDERID_Pictures, 0x33E28130, 0x4E1E, 0x4676, 0x83, 0x5A, 0x98, 0x39, 0x5C, 0x3B, 0xC3, 0xBB);
HRESULT SHGetFolderPathW(HWND window, int folder, HANDLE token, DWORD flags, LPWSTR path) noexcept;
HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID id, DWORD flags, HANDLE token, PWSTR* path) noexcept;
int SHCreateDirectoryExW(HWND window, LPCWSTR path, const SECURITY_ATTRIBUTES* attributes) noexcept;

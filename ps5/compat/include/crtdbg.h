// Super Mario Galaxy for the PlayStation 5 - the MSVC debug CRT's report hooks:
// the console's C library shows no dialogs, so each setting is accepted and kept.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
#define _CRT_WARN 0
#define _CRT_ERROR 1
#define _CRT_ASSERT 2
#define _CRTDBG_MODE_FILE 0x1
#define _CRTDBG_MODE_DEBUG 0x2
#define _CRTDBG_MODE_WNDW 0x4
#define _CRTDBG_REPORT_MODE -1
#define _CRTDBG_FILE_STDERR ((HANDLE)(intptr_t)-5)
#define _CRTDBG_FILE_STDOUT ((HANDLE)(intptr_t)-4)
#define _OUT_TO_DEFAULT 0
#define _OUT_TO_STDERR 1
#define _OUT_TO_MSGBOX 2
#define _WRITE_ABORT_MSG 0x1
#define _CALL_REPORTFAULT 0x2
int _CrtSetReportMode(int type, int mode) noexcept;
HANDLE _CrtSetReportFile(int type, HANDLE file) noexcept;
typedef int(__cdecl* _CRT_REPORT_HOOK)(int, char*, int*);
_CRT_REPORT_HOOK _CrtSetReportHook(_CRT_REPORT_HOOK hook) noexcept;
int _CrtSetReportHook2(int mode, _CRT_REPORT_HOOK hook) noexcept;
#define _CRT_RPTHOOK_INSTALL 0
#define _CRT_RPTHOOK_REMOVE 1
unsigned int _set_abort_behavior(unsigned int flags, unsigned int mask) noexcept;
int _set_error_mode(int mode) noexcept;
typedef void(__cdecl* _invalid_parameter_handler)(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t);
_invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler handler) noexcept;
_invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler handler) noexcept;
typedef void(__cdecl* _purecall_handler)();
_purecall_handler _set_purecall_handler(_purecall_handler handler) noexcept;

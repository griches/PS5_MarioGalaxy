// Super Mario Galaxy for the PlayStation 5 - the MSVC C runtime calls and keywords
// Nebula uses without including windows.h (native_api.h's getenv_s reaches the
// generated game code): the build includes this file in every unit (-include).
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdio>
#include <ctime>

#include "cxx_polyfill.h"

#define __stdcall
#define __cdecl
#define __thiscall
#define __vectorcall

typedef int errno_t;
errno_t getenv_s(std::size_t* required, char* buffer, std::size_t size, const char* name) noexcept;
errno_t _dupenv_s(char** buffer, std::size_t* count, const char* name) noexcept;
errno_t _wdupenv_s(wchar_t** buffer, std::size_t* count, const wchar_t* name) noexcept;
wchar_t* _wgetenv(const wchar_t* name) noexcept;
int _putenv_s(const char* name, const char* value) noexcept;
int _wputenv_s(const wchar_t* name, const wchar_t* value) noexcept;
int _wputenv(const wchar_t* assignment) noexcept;
errno_t fopen_s(FILE** file, const char* name, const char* mode) noexcept;
errno_t _wfopen_s(FILE** file, const wchar_t* name, const wchar_t* mode) noexcept;
errno_t _wfopen_s(FILE** file, const char* name, const wchar_t* mode) noexcept;
FILE* _wfopen(const wchar_t* name, const wchar_t* mode) noexcept;
FILE* _wfopen(const char* name, const wchar_t* mode) noexcept;
FILE* _wfsopen(const wchar_t* name, const wchar_t* mode, int share) noexcept;
FILE* _wfsopen(const char* name, const wchar_t* mode, int share) noexcept;
FILE* _fsopen(const char* name, const char* mode, int share) noexcept;
errno_t localtime_s(struct tm* result, const time_t* time) noexcept;
errno_t gmtime_s(struct tm* result, const time_t* time) noexcept;
int _fileno(FILE* file) noexcept;
int _isatty(int fd) noexcept;
int _setmode(int fd, int mode) noexcept;
std::size_t strnlen_s(const char* text, std::size_t size) noexcept;
std::size_t wcsnlen_s(const wchar_t* text, std::size_t size) noexcept;

// Locale-explicit number conversion: the console's C library is always in the
// "C" locale (a '.' decimal point), so the locale argument changes nothing.
typedef struct _locale_compat* _locale_t;
_locale_t _create_locale(int category, const char* locale) noexcept;
void _free_locale(_locale_t locale) noexcept;
float _strtof_l(const char* text, char** end, _locale_t locale) noexcept;
double _strtod_l(const char* text, char** end, _locale_t locale) noexcept;
int _snprintf_s_l(char* buffer, std::size_t size, std::size_t count, const char* format, _locale_t locale, ...) noexcept;
int _sprintf_l(char* buffer, const char* format, _locale_t locale, ...) noexcept;
#include <cstdarg>
int _vsnprintf_s(char* buffer, std::size_t size, std::size_t count, const char* format, va_list arguments) noexcept;
int _vsnwprintf_s(wchar_t* buffer, std::size_t size, std::size_t count, const wchar_t* format, va_list arguments) noexcept;
#endif

// Super Mario Galaxy for the PlayStation 5 - BCrypt's SHA-1 and SHA-256 hashes,
// which the runtime uses to identify game files and its own state
// (ps5/compat/src/bcrypt.cpp).
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows.h"
typedef PVOID BCRYPT_HANDLE;
typedef PVOID BCRYPT_ALG_HANDLE;
typedef PVOID BCRYPT_HASH_HANDLE;
#define BCRYPT_SHA1_ALGORITHM L"SHA1"
#define BCRYPT_SHA256_ALGORITHM L"SHA256"
#define BCRYPT_OBJECT_LENGTH L"ObjectLength"
#define BCRYPT_HASH_LENGTH L"HashDigestLength"
#define BCRYPT_ALG_HANDLE_HMAC_FLAG 0x00000008
#define BCRYPT_HASH_REUSABLE_FLAG 0x00000020
#define BCRYPT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBL)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
NTSTATUS BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE* algorithm, LPCWSTR id, LPCWSTR implementation,
                                     ULONG flags) noexcept;
NTSTATUS BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE algorithm, ULONG flags) noexcept;
NTSTATUS BCryptGetProperty(BCRYPT_HANDLE object, LPCWSTR property, PUCHAR output, ULONG size, ULONG* result,
                           ULONG flags) noexcept;
NTSTATUS BCryptCreateHash(BCRYPT_ALG_HANDLE algorithm, BCRYPT_HASH_HANDLE* hash, PUCHAR object, ULONG object_size,
                          PUCHAR secret, ULONG secret_size, ULONG flags) noexcept;
NTSTATUS BCryptHashData(BCRYPT_HASH_HANDLE hash, PUCHAR input, ULONG size, ULONG flags) noexcept;
NTSTATUS BCryptFinishHash(BCRYPT_HASH_HANDLE hash, PUCHAR output, ULONG size, ULONG flags) noexcept;
NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE hash) noexcept;
NTSTATUS BCryptGenRandom(BCRYPT_ALG_HANDLE algorithm, PUCHAR buffer, ULONG size, ULONG flags) noexcept;
#define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002

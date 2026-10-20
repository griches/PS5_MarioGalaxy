// Super Mario Galaxy for the PlayStation 5 - MSVC's compiler intrinsics as clang
// spells them for x86-64. The console's CPU is a Zen 2 (the build targets
// znver2): everything here is one instruction or a builtin.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <immintrin.h>
#include <x86intrin.h>

#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#define __noop(...) ((void)0)
#define __assume(condition) __builtin_assume(condition)
#define _ReadWriteBarrier() __asm__ __volatile__("" ::: "memory")
#define _WriteBarrier() __asm__ __volatile__("" ::: "memory")
#define _ReadBarrier() __asm__ __volatile__("" ::: "memory")
#define _ReturnAddress() __builtin_return_address(0)
#define _AddressOfReturnAddress() (reinterpret_cast<void*>(static_cast<char*>(__builtin_frame_address(0)) + sizeof(void*)))
#define __debugbreak() __builtin_debugtrap()
#define __fastfail(code) __builtin_trap()

inline void __cpuidex(int registers[4], int leaf, int subleaf) noexcept {
    __asm__ __volatile__("cpuid"
                         : "=a"(registers[0]), "=b"(registers[1]), "=c"(registers[2]), "=d"(registers[3])
                         : "a"(leaf), "c"(subleaf));
}
inline void __cpuid(int registers[4], int leaf) noexcept { __cpuidex(registers, leaf, 0); }

inline void __faststorefence() noexcept { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

inline unsigned char _BitScanForward(unsigned long* index, unsigned long mask) noexcept {
    if (mask == 0) return 0;
    *index = static_cast<unsigned long>(__builtin_ctzl(mask));
    return 1;
}
inline unsigned char _BitScanReverse(unsigned long* index, unsigned long mask) noexcept {
    if (mask == 0) return 0;
    *index = static_cast<unsigned long>(63 - __builtin_clzl(mask));
    return 1;
}
inline unsigned char _BitScanForward64(unsigned long* index, unsigned long long mask) noexcept {
    if (mask == 0) return 0;
    *index = static_cast<unsigned long>(__builtin_ctzll(mask));
    return 1;
}
inline unsigned char _BitScanReverse64(unsigned long* index, unsigned long long mask) noexcept {
    if (mask == 0) return 0;
    *index = static_cast<unsigned long>(63 - __builtin_clzll(mask));
    return 1;
}
inline unsigned short _byteswap_ushort(unsigned short value) noexcept { return __builtin_bswap16(value); }
inline unsigned int _byteswap_ulong(unsigned int value) noexcept { return __builtin_bswap32(value); }
inline unsigned long long _byteswap_uint64(unsigned long long value) noexcept { return __builtin_bswap64(value); }
inline unsigned int __popcnt(unsigned int value) noexcept { return static_cast<unsigned>(__builtin_popcount(value)); }
inline unsigned long long __popcnt64(unsigned long long value) noexcept {
    return static_cast<unsigned long long>(__builtin_popcountll(value));
}
inline unsigned int __lzcnt(unsigned int value) noexcept { return value == 0 ? 32u : static_cast<unsigned>(__builtin_clz(value)); }
inline unsigned long long __umulh(unsigned long long a, unsigned long long b) noexcept {
    return static_cast<unsigned long long>((static_cast<unsigned __int128>(a) * b) >> 64);
}
inline long long __mulh(long long a, long long b) noexcept {
    return static_cast<long long>((static_cast<__int128>(a) * b) >> 64);
}
// std::uint64_t is unsigned long here, unsigned long long on Windows: both spellings.
inline std::uint64_t _umul128(std::uint64_t a, std::uint64_t b, std::uint64_t* high) noexcept {
    const unsigned __int128 product = static_cast<unsigned __int128>(a) * b;
    *high = static_cast<std::uint64_t>(product >> 64);
    return static_cast<std::uint64_t>(product);
}
inline unsigned long long _umul128(unsigned long long a, unsigned long long b, unsigned long long* high) noexcept {
    const unsigned __int128 product = static_cast<unsigned __int128>(a) * b;
    *high = static_cast<unsigned long long>(product >> 64);
    return static_cast<unsigned long long>(product);
}
// The quotient of a 128-bit dividend; as the instruction, the caller keeps it under 2^64.
inline std::uint64_t _udiv128(std::uint64_t high, std::uint64_t low, std::uint64_t divisor,
                              std::uint64_t* remainder) noexcept {
    const unsigned __int128 dividend = (static_cast<unsigned __int128>(high) << 64) | low;
    *remainder = static_cast<std::uint64_t>(dividend % divisor);
    return static_cast<std::uint64_t>(dividend / divisor);
}
inline unsigned long long _udiv128(unsigned long long high, unsigned long long low, unsigned long long divisor,
                                   unsigned long long* remainder) noexcept {
    const unsigned __int128 dividend = (static_cast<unsigned __int128>(high) << 64) | low;
    *remainder = static_cast<unsigned long long>(dividend % divisor);
    return static_cast<unsigned long long>(dividend / divisor);
}
inline long long _mul128(long long a, long long b, long long* high) noexcept {
    const __int128 product = static_cast<__int128>(a) * b;
    *high = static_cast<long long>(product >> 64);
    return static_cast<long long>(product);
}
inline unsigned long long __shiftleft128(unsigned long long low, unsigned long long high, unsigned char shift) noexcept {
    const unsigned __int128 value = (static_cast<unsigned __int128>(high) << 64) | low;
    return static_cast<unsigned long long>((value << (shift & 63)) >> 64);
}
inline unsigned long long __shiftright128(unsigned long long low, unsigned long long high, unsigned char shift) noexcept {
    const unsigned __int128 value = (static_cast<unsigned __int128>(high) << 64) | low;
    return static_cast<unsigned long long>(value >> (shift & 63));
}

inline long _InterlockedIncrement(volatile long* value) noexcept { return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST); }
inline long _InterlockedDecrement(volatile long* value) noexcept { return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST); }
inline long _InterlockedExchange(volatile long* target, long value) noexcept {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}
inline long _InterlockedExchangeAdd(volatile long* target, long value) noexcept {
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}
inline long _InterlockedCompareExchange(volatile long* target, long exchange, long comparand) noexcept {
    __atomic_compare_exchange_n(target, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
inline long _InterlockedOr(volatile long* target, long value) noexcept {
    return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}
inline long _InterlockedAnd(volatile long* target, long value) noexcept {
    return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedOr64(volatile long long* target, long long value) noexcept {
    return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedAnd64(volatile long long* target, long long value) noexcept {
    return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedExchange64(volatile long long* target, long long value) noexcept {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedExchangeAdd64(volatile long long* target, long long value) noexcept {
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedIncrement64(volatile long long* value) noexcept {
    return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedDecrement64(volatile long long* value) noexcept {
    return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST);
}
inline long long _InterlockedCompareExchange64(volatile long long* target, long long exchange,
                                               long long comparand) noexcept {
    __atomic_compare_exchange_n(target, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
inline void* _InterlockedCompareExchangePointer(void* volatile* target, void* exchange, void* comparand) noexcept {
    __atomic_compare_exchange_n(target, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
inline void* _InterlockedExchangePointer(void* volatile* target, void* value) noexcept {
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

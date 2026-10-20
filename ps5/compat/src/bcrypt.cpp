// Super Mario Galaxy for the PlayStation 5 - BCrypt's SHA-1 and SHA-256 (FIPS 180-4),
// which Nebula uses to identify the game's files and its own state, and its
// random numbers.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <bcrypt.h>

#include <cstring>
#include <random>

namespace {

enum class Algorithm { Sha1, Sha256 };

struct Provider {
    Algorithm algorithm;
};

struct Hash {
    Algorithm algorithm;
    std::uint32_t state[8];
    std::uint8_t block[64];
    std::size_t used;
    std::uint64_t length;
};

std::uint32_t rotl(std::uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
std::uint32_t load_be(const std::uint8_t* p) {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | p[3];
}

void sha1_block(std::uint32_t* h, const std::uint8_t* block) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = load_be(block + 4 * i);
    for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        const std::uint32_t t = rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

constexpr std::uint32_t kSha256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

void sha256_block(std::uint32_t* h, const std::uint8_t* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = load_be(block + 4 * i);
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = k + s1 + ch + kSha256[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        k = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += k;
}

void hash_reset(Hash& hash) {
    hash.used = 0;
    hash.length = 0;
    if (hash.algorithm == Algorithm::Sha1) {
        const std::uint32_t initial[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
        std::memcpy(hash.state, initial, sizeof(initial));
    } else {
        const std::uint32_t initial[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        std::memcpy(hash.state, initial, sizeof(initial));
    }
}

void hash_block(Hash& hash, const std::uint8_t* block) {
    hash.algorithm == Algorithm::Sha1 ? sha1_block(hash.state, block) : sha256_block(hash.state, block);
}

ULONG digest_size(Algorithm algorithm) { return algorithm == Algorithm::Sha1 ? 20u : 32u; }

}  // namespace

NTSTATUS BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE* algorithm, LPCWSTR id, LPCWSTR, ULONG) noexcept {
    if (algorithm == nullptr || id == nullptr) return STATUS_INVALID_PARAMETER;
    if (std::wcscmp(id, BCRYPT_SHA1_ALGORITHM) == 0) {
        *algorithm = new Provider{Algorithm::Sha1};
    } else if (std::wcscmp(id, BCRYPT_SHA256_ALGORITHM) == 0) {
        *algorithm = new Provider{Algorithm::Sha256};
    } else {
        return STATUS_NOT_SUPPORTED;
    }
    return STATUS_SUCCESS;
}
NTSTATUS BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE algorithm, ULONG) noexcept {
    delete static_cast<Provider*>(algorithm);
    return STATUS_SUCCESS;
}
NTSTATUS BCryptGetProperty(BCRYPT_HANDLE object, LPCWSTR property, PUCHAR output, ULONG size, ULONG* result,
                           ULONG) noexcept {
    if (object == nullptr || property == nullptr || output == nullptr || size < sizeof(ULONG)) return STATUS_INVALID_PARAMETER;
    ULONG value = 0;
    if (std::wcscmp(property, BCRYPT_OBJECT_LENGTH) == 0) {
        value = sizeof(Hash);
    } else if (std::wcscmp(property, BCRYPT_HASH_LENGTH) == 0) {
        value = digest_size(static_cast<Provider*>(object)->algorithm);
    } else {
        return STATUS_NOT_SUPPORTED;
    }
    std::memcpy(output, &value, sizeof(value));
    if (result != nullptr) *result = sizeof(value);
    return STATUS_SUCCESS;
}
NTSTATUS BCryptCreateHash(BCRYPT_ALG_HANDLE algorithm, BCRYPT_HASH_HANDLE* hash, PUCHAR object, ULONG object_size,
                          PUCHAR, ULONG, ULONG) noexcept {
    if (algorithm == nullptr || hash == nullptr) return STATUS_INVALID_PARAMETER;
    Hash* state = object != nullptr && object_size >= sizeof(Hash) ? new (object) Hash{} : new Hash{};
    state->algorithm = static_cast<Provider*>(algorithm)->algorithm;
    hash_reset(*state);
    *hash = state;
    return STATUS_SUCCESS;
}
NTSTATUS BCryptHashData(BCRYPT_HASH_HANDLE handle, PUCHAR input, ULONG size, ULONG) noexcept {
    auto* hash = static_cast<Hash*>(handle);
    if (hash == nullptr) return STATUS_INVALID_PARAMETER;
    hash->length += size;
    while (size > 0) {
        const std::size_t take = std::min<std::size_t>(64 - hash->used, size);
        std::memcpy(hash->block + hash->used, input, take);
        hash->used += take;
        input += take;
        size -= static_cast<ULONG>(take);
        if (hash->used == 64) {
            hash_block(*hash, hash->block);
            hash->used = 0;
        }
    }
    return STATUS_SUCCESS;
}
NTSTATUS BCryptFinishHash(BCRYPT_HASH_HANDLE handle, PUCHAR output, ULONG size, ULONG) noexcept {
    auto* hash = static_cast<Hash*>(handle);
    if (hash == nullptr || output == nullptr || size < digest_size(hash->algorithm)) return STATUS_INVALID_PARAMETER;
    const std::uint64_t bits = hash->length * 8;
    std::uint8_t pad = 0x80;
    BCryptHashData(handle, &pad, 1, 0);
    pad = 0;
    while (hash->used != 56) BCryptHashData(handle, &pad, 1, 0);
    std::uint8_t length[8];
    for (int i = 0; i < 8; ++i) length[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    BCryptHashData(handle, length, 8, 0);
    const ULONG words = digest_size(hash->algorithm) / 4;
    for (ULONG i = 0; i < words; ++i) {
        output[4 * i] = static_cast<UCHAR>(hash->state[i] >> 24);
        output[4 * i + 1] = static_cast<UCHAR>(hash->state[i] >> 16);
        output[4 * i + 2] = static_cast<UCHAR>(hash->state[i] >> 8);
        output[4 * i + 3] = static_cast<UCHAR>(hash->state[i]);
    }
    hash_reset(*hash);
    return STATUS_SUCCESS;
}
NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE) noexcept {
    // The hash object lives in the caller's buffer (BCryptCreateHash's pbHashObject).
    return STATUS_SUCCESS;
}
NTSTATUS BCryptGenRandom(BCRYPT_ALG_HANDLE, PUCHAR buffer, ULONG size, ULONG) noexcept {
    static std::mt19937_64 generator{std::random_device{}()};
    for (ULONG i = 0; i < size; ++i) buffer[i] = static_cast<UCHAR>(generator());
    return STATUS_SUCCESS;
}

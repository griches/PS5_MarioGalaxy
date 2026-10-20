// Super Mario Galaxy for the PlayStation 5 - replacement textures (texture_pack.h).
//
// The index maps (width, height, format, texture hash) to a file; the TLUT hash
// of paletted names is not checked (a Super Mario Galaxy pack has none), so the
// first file of a key wins. Loads run on one worker thread; a loaded image waits
// for take() and is then the cache's (it keeps the GPU copy).
//
// SPDX-License-Identifier: GPL-3.0-only
#include "texture_pack.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace galaxy::gx::pack {
namespace {

constexpr const char* kPackRoot = "/app0/textures";

// ---- XXH64 (Yann Collet's xxHash, BSD-2-Clause; the reference algorithm) --------------
constexpr std::uint64_t kPrime1 = 0x9E3779B185EBCA87ull;
constexpr std::uint64_t kPrime2 = 0xC2B2AE3D27D4EB4Full;
constexpr std::uint64_t kPrime3 = 0x165667B19E3779F9ull;
constexpr std::uint64_t kPrime4 = 0x85EBCA77C2B2AE63ull;
constexpr std::uint64_t kPrime5 = 0x27D4EB2F165667C5ull;

inline std::uint64_t rotl(std::uint64_t value, int bits) noexcept { return (value << bits) | (value >> (64 - bits)); }
inline std::uint64_t read64(const std::uint8_t* p) noexcept {
    std::uint64_t value;
    std::memcpy(&value, p, 8);
    return value;
}
inline std::uint32_t read32(const std::uint8_t* p) noexcept {
    std::uint32_t value;
    std::memcpy(&value, p, 4);
    return value;
}
inline std::uint64_t xxh_round(std::uint64_t acc, std::uint64_t input) noexcept {
    acc += input * kPrime2;
    acc = rotl(acc, 31);
    return acc * kPrime1;
}
inline std::uint64_t xxh_merge(std::uint64_t acc, std::uint64_t value) noexcept {
    acc ^= xxh_round(0, value);
    return acc * kPrime1 + kPrime4;
}

// ---- the index ------------------------------------------------------------------------
struct Key {
    std::uint64_t hash;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t format;
    bool operator==(const Key&) const = default;
};
struct KeyHash {
    std::size_t operator()(const Key& key) const noexcept {
        return static_cast<std::size_t>(key.hash ^ (static_cast<std::uint64_t>(key.width) << 40) ^
                                        (static_cast<std::uint64_t>(key.height) << 20) ^ key.format);
    }
};

enum class State { Unloaded, Queued, Ready, Failed };
struct Entry {
    std::string path;
    State state = State::Unloaded;
    std::shared_ptr<const Image> image;
};

std::optional<std::uint64_t> parse_hex(std::string_view text) {
    if (text.empty() || text.size() > 16) return std::nullopt;
    std::uint64_t value = 0;
    for (char c : text) {
        value <<= 4;
        if (c >= '0' && c <= '9') value |= static_cast<std::uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<std::uint64_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<std::uint64_t>(c - 'A' + 10);
        else return std::nullopt;
    }
    return value;
}
std::optional<std::uint32_t> parse_u32(std::string_view text) {
    if (text.empty() || text.size() > 9) return std::nullopt;
    std::uint32_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return value;
}

// tex1_<w>x<h>[_m]_<hash>[_<tlut>]_<format>[_arb]
std::optional<Key> parse_name(std::string_view name) {
    if (name.size() < 5 || name.substr(name.size() - 4) != ".dds") return std::nullopt;
    name.remove_suffix(4);
    std::vector<std::string_view> parts;
    for (std::size_t start = 0;;) {
        const std::size_t end = name.find('_', start);
        parts.push_back(name.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    if (parts.size() < 4 || parts[0] != "tex1") return std::nullopt;
    if (parts.back() == "arb") parts.pop_back();
    const std::size_t x = parts[1].find('x');
    if (x == std::string_view::npos) return std::nullopt;
    const auto width = parse_u32(parts[1].substr(0, x));
    const auto height = parse_u32(parts[1].substr(x + 1));
    std::size_t index = 2;
    if (index < parts.size() && parts[index] == "m") ++index;
    const std::size_t remaining = parts.size() - index;
    if (remaining != 2 && remaining != 3) return std::nullopt;
    const auto hash = parse_hex(parts[index]);
    const auto format = parse_u32(parts.back());
    if (!width || !height || !hash || !format) return std::nullopt;
    return Key{*hash, *width, *height, *format};
}

// ---- DDS --------------------------------------------------------------------------------
std::uint32_t le32(const std::uint8_t* p) { return read32(p); }

std::shared_ptr<const Image> load_dds(const std::string& path, std::uint32_t id) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return nullptr;
    const std::streamsize size = file.tellg();
    if (size < 128) return nullptr;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) return nullptr;
    const std::uint8_t* h = bytes.data();
    if (std::memcmp(h, "DDS ", 4) != 0 || le32(h + 4) != 124) return nullptr;
    auto image = std::make_shared<Image>();
    image->id = id;
    image->height = le32(h + 12);
    image->width = le32(h + 16);
    image->mips = std::max<std::uint32_t>(1u, le32(h + 28));
    const std::uint32_t pf_flags = le32(h + 80);
    const std::uint32_t fourcc = le32(h + 84);
    std::size_t offset = 128;
    std::uint32_t block_bytes = 0;  // 0: uncompressed
    bool convert_rgb = false;
    std::uint32_t rmask = 0, gmask = 0, bmask = 0, amask = 0;
    if ((pf_flags & 0x4u) != 0u) {  // DDPF_FOURCC
        if (fourcc == 0x30315844u) {  // "DX10"
            if (bytes.size() < 148) return nullptr;
            const std::uint32_t dxgi = le32(h + 128);
            offset = 148;
            switch (dxgi) {
            case 98: case 99: image->format = DXGI_FORMAT_BC7_UNORM; block_bytes = 16; break;
            case 71: case 72: image->format = DXGI_FORMAT_BC1_UNORM; block_bytes = 8; break;
            case 74: case 75: image->format = DXGI_FORMAT_BC2_UNORM; block_bytes = 16; break;
            case 77: case 78: image->format = DXGI_FORMAT_BC3_UNORM; block_bytes = 16; break;
            case 28: case 29: image->format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
            case 87: case 91: image->format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
            default: return nullptr;
            }
        } else if (fourcc == 0x31545844u) {  // DXT1
            image->format = DXGI_FORMAT_BC1_UNORM;
            block_bytes = 8;
        } else if (fourcc == 0x33545844u) {  // DXT3
            image->format = DXGI_FORMAT_BC2_UNORM;
            block_bytes = 16;
        } else if (fourcc == 0x35545844u) {  // DXT5
            image->format = DXGI_FORMAT_BC3_UNORM;
            block_bytes = 16;
        } else {
            return nullptr;
        }
    } else if ((pf_flags & 0x40u) != 0u && le32(h + 88) == 32u) {  // DDPF_RGB, 32 bits
        rmask = le32(h + 92);
        gmask = le32(h + 96);
        bmask = le32(h + 100);
        amask = (pf_flags & 0x1u) != 0u ? le32(h + 104) : 0u;  // DDPF_ALPHAPIXELS
        image->format = DXGI_FORMAT_R8G8B8A8_UNORM;
        convert_rgb = true;
    } else {
        return nullptr;
    }
    // Level layout, tightly packed as DDS stores it.
    std::size_t total = 0;
    for (std::uint32_t level = 0; level < image->mips; ++level) {
        const std::uint32_t w = std::max<std::uint32_t>(1u, image->width >> level);
        const std::uint32_t hgt = std::max<std::uint32_t>(1u, image->height >> level);
        const std::uint32_t row_bytes = block_bytes != 0 ? ((w + 3) / 4) * block_bytes : w * 4u;
        const std::uint32_t rows = block_bytes != 0 ? (hgt + 3) / 4 : hgt;
        image->level_offsets.push_back(total);
        image->level_row_bytes.push_back(row_bytes);
        image->level_rows.push_back(rows);
        total += static_cast<std::size_t>(row_bytes) * rows;
    }
    if (offset + total > bytes.size()) {
        // A truncated chain: keep the levels that are whole.
        std::uint32_t whole = 0;
        while (whole < image->mips &&
               offset + image->level_offsets[whole] +
                       static_cast<std::size_t>(image->level_row_bytes[whole]) * image->level_rows[whole] <=
                   bytes.size()) {
            ++whole;
        }
        if (whole == 0) return nullptr;
        image->mips = whole;
        image->level_offsets.resize(whole);
        image->level_row_bytes.resize(whole);
        image->level_rows.resize(whole);
        total = image->level_offsets.back() +
                static_cast<std::size_t>(image->level_row_bytes.back()) * image->level_rows.back();
    }
    image->data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                       bytes.begin() + static_cast<std::ptrdiff_t>(offset + total));
    if (convert_rgb) {
        // Any 32-bit masked layout to RGBA8; no alpha mask means opaque.
        auto shift_of = [](std::uint32_t mask) {
            int shift = 0;
            while (mask != 0u && (mask & 1u) == 0u) {
                mask >>= 1;
                ++shift;
            }
            return shift;
        };
        const int rs = shift_of(rmask), gs = shift_of(gmask), bs = shift_of(bmask), as = shift_of(amask);
        for (std::size_t i = 0; i + 4 <= image->data.size(); i += 4) {
            const std::uint32_t pixel = read32(image->data.data() + i);
            image->data[i + 0] = static_cast<std::uint8_t>((pixel & rmask) >> rs);
            image->data[i + 1] = static_cast<std::uint8_t>((pixel & gmask) >> gs);
            image->data[i + 2] = static_cast<std::uint8_t>((pixel & bmask) >> bs);
            image->data[i + 3] = amask != 0u ? static_cast<std::uint8_t>((pixel & amask) >> as) : 0xFFu;
        }
    }
    return image;
}

// ---- state ----------------------------------------------------------------------------------
struct Pack {
    std::mutex lock;
    std::condition_variable wake;
    std::unordered_map<Key, std::uint32_t, KeyHash> index;
    std::vector<Entry> entries;
    std::deque<std::uint32_t> queue;
    std::atomic<bool> new_ready{false};
    std::uint64_t loaded = 0;
    std::uint64_t loaded_bytes = 0;
    std::thread worker;

    Pack() {
        std::error_code error;
        std::size_t scanned = 0;
        for (std::filesystem::recursive_directory_iterator it(kPackRoot, error), end; !error && it != end;
             it.increment(error)) {
            if (!it->is_regular_file(error)) continue;
            ++scanned;
            const std::string name = it->path().filename().string();
            const auto key = parse_name(name);
            if (!key) continue;
            if (index.emplace(*key, static_cast<std::uint32_t>(entries.size())).second) {
                entries.push_back(Entry{it->path().string()});
            }
        }
        std::fprintf(stderr, "[texpack] %zu replacement textures in %s (%zu files)\n", entries.size(), kPackRoot,
                     scanned);
        if (!entries.empty()) worker = std::thread([this] { run(); });
    }

    void run() {
        for (;;) {
            std::uint32_t id;
            std::string path;
            {
                std::unique_lock guard(lock);
                wake.wait(guard, [this] { return !queue.empty(); });
                id = queue.front();
                queue.pop_front();
                path = entries[id].path;
            }
            auto image = load_dds(path, id);
            std::lock_guard guard(lock);
            if (image == nullptr) {
                entries[id].state = State::Failed;
                std::fprintf(stderr, "[texpack] could not read %s\n", path.c_str());
                continue;
            }
            ++loaded;
            loaded_bytes += image->data.size();
            if (loaded <= 8 || loaded % 50 == 0) {
                std::fprintf(stderr, "[texpack] loaded %llu (%.1f MiB): %s %ux%u\n",
                             static_cast<unsigned long long>(loaded), static_cast<double>(loaded_bytes) / 1048576.0,
                             std::filesystem::path(path).filename().string().c_str(), image->width, image->height);
            }
            entries[id].image = std::move(image);
            entries[id].state = State::Ready;
            new_ready.store(true, std::memory_order_release);
        }
    }
};

Pack& pack() {
    static Pack* instance = new Pack;  // lives for the process (its worker never ends)
    return *instance;
}

}  // namespace

std::uint64_t xxh64(const void* data, std::size_t size, std::uint64_t seed) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    const std::uint8_t* const end = p + size;
    std::uint64_t h64;
    if (size >= 32) {
        const std::uint8_t* const limit = end - 32;
        std::uint64_t v1 = seed + kPrime1 + kPrime2;
        std::uint64_t v2 = seed + kPrime2;
        std::uint64_t v3 = seed;
        std::uint64_t v4 = seed - kPrime1;
        do {
            v1 = xxh_round(v1, read64(p));
            v2 = xxh_round(v2, read64(p + 8));
            v3 = xxh_round(v3, read64(p + 16));
            v4 = xxh_round(v4, read64(p + 24));
            p += 32;
        } while (p <= limit);
        h64 = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
        h64 = xxh_merge(h64, v1);
        h64 = xxh_merge(h64, v2);
        h64 = xxh_merge(h64, v3);
        h64 = xxh_merge(h64, v4);
    } else {
        h64 = seed + kPrime5;
    }
    h64 += static_cast<std::uint64_t>(size);
    while (p + 8 <= end) {
        h64 ^= xxh_round(0, read64(p));
        h64 = rotl(h64, 27) * kPrime1 + kPrime4;
        p += 8;
    }
    if (p + 4 <= end) {
        h64 ^= static_cast<std::uint64_t>(read32(p)) * kPrime1;
        h64 = rotl(h64, 23) * kPrime2 + kPrime3;
        p += 4;
    }
    while (p < end) {
        h64 ^= static_cast<std::uint64_t>(*p) * kPrime5;
        h64 = rotl(h64, 11) * kPrime1;
        ++p;
    }
    h64 ^= h64 >> 33;
    h64 *= kPrime2;
    h64 ^= h64 >> 29;
    h64 *= kPrime3;
    h64 ^= h64 >> 32;
    return h64;
}

bool active() { return !pack().entries.empty(); }

std::optional<std::uint32_t> lookup(std::uint32_t width, std::uint32_t height, std::uint8_t format,
                                    std::uint64_t texture_hash) {
    const Pack& p = pack();
    const auto found = p.index.find(Key{texture_hash, width, height, format});
    if (found == p.index.end()) return std::nullopt;
    return found->second;
}

std::shared_ptr<const Image> take(std::uint32_t id) {
    Pack& p = pack();
    if (id >= p.entries.size()) return nullptr;
    std::lock_guard guard(p.lock);
    Entry& entry = p.entries[id];
    if (entry.state == State::Ready) {
        entry.state = State::Unloaded;
        return std::move(entry.image);
    }
    if (entry.state == State::Unloaded) {
        entry.state = State::Queued;
        p.queue.push_back(id);
        p.wake.notify_one();
    }
    return nullptr;
}

bool settled(std::uint32_t id) {
    Pack& p = pack();
    if (id >= p.entries.size()) return true;
    std::lock_guard guard(p.lock);
    return p.entries[id].state == State::Ready || p.entries[id].state == State::Failed;
}

bool take_new_ready() noexcept {
    Pack& p = pack();
    return !p.entries.empty() && p.new_ready.exchange(false, std::memory_order_acq_rel);
}

}  // namespace galaxy::gx::pack

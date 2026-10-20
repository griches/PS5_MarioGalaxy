// Super Mario Galaxy for the PlayStation 5 - replacement textures in Dolphin's
// format: DDS files named tex1_<w>x<h>[_m]_<hash>[_<tlut hash>]_<format>[_arb].dds
// anywhere under /app0/textures (an HD pack's RMG/ folder, as Dolphin loads it).
//
// The hash is Dolphin's: XXH64 (seed 0) of the texture's base level as it sits in
// guest memory, block-rounded. TextureCache (texture_cache.cpp) looks a texture
// up on a first-use decode; a match is read and parsed on a worker thread while
// the original shows, and take_new_ready() tells the cache at a frame boundary
// that it should look again.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <d3d12.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace galaxy::gx::pack {

// A parsed DDS: every mip level, tightly packed in level order.
struct Image {
    std::uint32_t id = 0;  // the pack entry, for the cache's GPU copy
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mips = 1;
    std::vector<std::uint8_t> data;
    std::vector<std::size_t> level_offsets;
    std::vector<std::uint32_t> level_row_bytes;  // bytes in one row of blocks (or pixels)
    std::vector<std::uint32_t> level_rows;       // rows of blocks (or pixels)
};

[[nodiscard]] std::uint64_t xxh64(const void* data, std::size_t size, std::uint64_t seed = 0) noexcept;

// True when the pack folder held at least one usable file (scanned once).
[[nodiscard]] bool active();

// The pack entry for a texture (its GX size, format and Dolphin hash), when the
// pack has one.
[[nodiscard]] std::optional<std::uint32_t> lookup(std::uint32_t width, std::uint32_t height, std::uint8_t format,
                                                  std::uint64_t texture_hash);

// An entry's image once it has loaded, handed over: the pack keeps no copy (the
// cache uploads it once and keeps the GPU texture). Null while it loads; the
// first call starts the load, and a call after the image was handed over loads
// the file again.
[[nodiscard]] std::shared_ptr<const Image> take(std::uint32_t id);

// True when take() returns the image now, or the file could not be read (then
// take() never does).
[[nodiscard]] bool settled(std::uint32_t id);

// True once after one or more loads have finished since the last call.
[[nodiscard]] bool take_new_ready() noexcept;

}  // namespace galaxy::gx::pack

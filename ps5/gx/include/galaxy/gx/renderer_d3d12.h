// Super Mario Galaxy for the PlayStation 5 - what the GX front end (the vertex
// loader) takes from Nebula's Direct3D 12 renderer header, without Direct3D:
// the per-frame upload ring it writes vertices and indices into. Ahead of
// runtime/include on the console build's include path. While the console has
// no renderer (ps5/gx/gx_null.cpp) the ring is plain memory.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "galaxy/gx/gx_bitfields.h"
#include "galaxy/gx/immutable_upload.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace galaxy::gx {

class UploadRing {
public:
    struct Allocation {
        std::byte* cpu = nullptr;
        std::uint64_t gpu = 0;
        std::size_t offset = 0;
    };

    ~UploadRing() { std::free(memory_); }

    bool initialize(std::size_t bytes_per_frame) {
        memory_ = static_cast<std::byte*>(std::aligned_alloc(256, (bytes_per_frame + 255) & ~std::size_t{255}));
        size_ = bytes_per_frame;
        used_ = 0;
        return memory_ != nullptr;
    }
    void reset() noexcept { used_ = 0; }
    void begin_frame(unsigned) noexcept { used_ = 0; }

    Allocation allocate(std::size_t size, std::size_t alignment) {
        const std::size_t start = (used_ + alignment - 1) & ~(alignment - 1);
        if (start + size > size_) throw std::runtime_error("upload ring exhausted");
        used_ = start + size;
        return Allocation{memory_ + start, 0, start};
    }
    Allocation upload_immutable(const void* source, std::size_t size, std::size_t alignment, ImmutableUploadToken&) {
        const Allocation allocation = allocate(size, alignment);
        std::memcpy(allocation.cpu, source, size);
        return allocation;
    }

private:
    std::byte* memory_ = nullptr;
    std::size_t size_ = 0;
    std::size_t used_ = 0;
};

}  // namespace galaxy::gx

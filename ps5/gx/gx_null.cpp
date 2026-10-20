// Super Mario Galaxy for the PlayStation 5 - the GX API (galaxy/gx_d3d12.h) with
// no picture: the first milestone, which runs the game's code, memory, audio and
// input on the console before the Vulkan renderer exists.
//
// Each FIFO capture is parsed for its pixel-engine events (PE_FINISH, PE_TOKEN),
// which are raised through the native services in FIFO order once the capture
// is "rendered", as Nebula's synchronous render path raises them; its receipt
// completes at once. EFB peeks read a cleared EFB (black, far depth).
//
// SPDX-License-Identifier: GPL-3.0-only
#include "galaxy/gx_d3d12.h"

#include "galaxy/gx/fifo_parser.h"
#include "galaxy/gx/gx_state.h"
#include "galaxy/gx/vertex_loader.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <vector>

namespace galaxy::gx {
namespace {

struct PeEvent {
    bool finish;
    std::uint16_t token;
    bool interrupt;
};

class PeCollector final : public FifoSink {
public:
    explicit PeCollector(GxState& state) : state_(state) {}

    [[nodiscard]] std::size_t draw_payload_size(std::uint8_t vtxfmt, std::uint16_t vertex_count) const override {
        return VertexLoader::source_vertex_size(state_.vertex_desc(vtxfmt)) * static_cast<std::size_t>(vertex_count);
    }
    void on_draw(PrimitiveClass, std::uint8_t vtxfmt, FifoCursor& cursor) override {
        const std::uint16_t vertex_count = cursor.read_u16();
        (void)cursor.take(draw_payload_size(vtxfmt, vertex_count));
    }
    [[nodiscard]] bool on_cached_prepared_draw_run(PrimitiveClass, std::uint8_t, std::span<const std::byte>, std::size_t,
                                                   std::size_t, std::uint8_t, std::size_t) override {
        return true;
    }
    [[nodiscard]] bool on_cached_packet_draw_run(std::uint64_t, std::size_t, PrimitiveClass, std::uint8_t,
                                                 std::span<const std::byte>, std::size_t,
                                                 std::span<const CachedDrawPacket>, std::uint32_t, std::uint32_t,
                                                 std::span<const std::uint16_t>) override {
        return true;
    }
    void on_efb_copy(std::uint32_t) override { ++efb_copies; }
    void on_pe_finish() override { events.push_back(PeEvent{true, 0, false}); }
    void on_pe_token(std::uint16_t token, bool interrupt) override { events.push_back(PeEvent{false, token, interrupt}); }
    void on_invalidate_textures() override {}
    void on_tlut_load() override {}
    void on_invalidate_vertex_cache() override {}

    std::vector<PeEvent> events;
    std::uint64_t efb_copies = 0;

private:
    GxState& state_;
};

struct NullBackend {
    std::mutex lock;
    bool initialized = false;
    FifoParser parser;
    GxState state;
    std::vector<std::byte> pending;  // a command split across two captures
    FramePeCompletionContract completions;
    std::uint64_t captures = 0;
    std::uint64_t presents = 0;
    std::uint64_t xfb_copies = 0;
    std::uint64_t pe_events = 0;
    std::chrono::steady_clock::time_point last_report = std::chrono::steady_clock::now();
    std::uint64_t presents_at_report = 0;
    XfbPresentStats present_stats{};
};

NullBackend& backend() {
    static NullBackend instance;
    return instance;
}

void deliver(const NativeServicesV1* services, const std::vector<PeEvent>& events) {
    for (const PeEvent& event : events) {
        if (event.finish) {
            services->gx_pe_finish(services->user);
        } else {
            services->gx_pe_token(services->user, event.token, event.interrupt);
        }
    }
}

// A line a second in klog: proof of life while there is no picture.
void report(NullBackend& b) {
    const auto now = std::chrono::steady_clock::now();
    if (now - b.last_report < std::chrono::seconds(1)) return;
    const double seconds = std::chrono::duration<double>(now - b.last_report).count();
    std::fprintf(stderr, "[gx-null] presents/s=%.1f captures=%llu xfb-copies=%llu pe-events=%llu\n",
                 static_cast<double>(b.presents - b.presents_at_report) / seconds,
                 static_cast<unsigned long long>(b.captures), static_cast<unsigned long long>(b.xfb_copies),
                 static_cast<unsigned long long>(b.pe_events));
    b.last_report = now;
    b.presents_at_report = b.presents;
}

}  // namespace

bool initialize(int width, int height) {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    b.initialized = true;
    b.completions.reset();  // opens the first epoch, as GxBackend::initialize does
    std::fprintf(stderr, "[gx-null] no renderer yet: %dx%d requested, nothing drawn\n", width, height);
    return true;
}

void set_native_settings_content_root(const std::string&) {}

FramePeCompletionToken render_frame(const std::byte* fifo_data, std::size_t fifo_size, GuestMemoryV1* memory,
                                    const NativeServicesV1* services, bool present_swap_chain,
                                    std::uint32_t displayed_xfb_addr) {
    NullBackend& b = backend();
    if (fifo_data == nullptr || fifo_size == 0) {
        if (present_swap_chain) present_cached_xfb(displayed_xfb_addr, true);
        return {};
    }
    PeCollector sink(b.state);
    FramePeCompletionToken token;
    {
        std::lock_guard lock(b.lock);
        if (b.pending.empty()) {
            const std::span<const std::byte> fifo(fifo_data, fifo_size);
            const std::size_t consumed = b.parser.run_available(fifo, memory, sink, b.state);
            if (consumed < fifo.size()) b.pending.assign(fifo.begin() + static_cast<std::ptrdiff_t>(consumed), fifo.end());
        } else {
            b.pending.insert(b.pending.end(), fifo_data, fifo_data + fifo_size);
            const std::size_t consumed = b.parser.run_available(b.pending, memory, sink, b.state);
            b.pending.erase(b.pending.begin(), b.pending.begin() + static_cast<std::ptrdiff_t>(consumed));
        }
        ++b.captures;
        b.xfb_copies += sink.efb_copies;
        b.pe_events += sink.events.size();
        token = b.completions.issue();
        b.completions.resolve_on_submission(token);
    }
    deliver(services, sink.events);
    if (present_swap_chain) present_cached_xfb(displayed_xfb_addr, true);
    return token;
}

void wait_for_render_idle() {}
void wait_for_render_fifo_effects_idle() {}

void wait_for_frame_pe_completion(FramePeCompletionToken token) {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    b.completions.consume_wait(token);
}

bool wait_for_frame_pe_completion_for(FramePeCompletionToken token, std::uint32_t) {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    if (b.completions.probe_wait(token) != FramePeWaitState::Complete) return false;
    b.completions.consume_wait(token);
    return true;
}

bool render_queue_has_space() { return true; }

bool capture_pointer_depth(std::span<std::uint32_t> pixels) {
    std::fill(pixels.begin(), pixels.end(), 0x00FFFFFFu);
    return true;
}

bool enqueue_pointer_depth_capture(const std::shared_ptr<PointerDepthCapture>& capture) {
    if (capture == nullptr) return false;
    capture->rgba = nullptr;
    capture->pixels.fill(0x00FFFFFFu);
    capture->success = true;
    capture->publish_complete();
    return true;
}

void attach_pointer_depth_capture(const std::shared_ptr<PointerDepthCapture>& capture, std::size_t) {
    (void)enqueue_pointer_depth_capture(capture);
}
void attach_fifo_marker(FifoMarker, std::size_t) noexcept {}
void attach_scene_view(const float[12]) noexcept {}
void set_pointer_response(PointerResponseStamp) noexcept {}
void set_prerecorded_movie_content(bool) noexcept {}
void set_safety_scene_surround(float) noexcept {}

bool peek_efb(std::uint16_t, std::uint16_t, EfbPeekKind kind, std::uint32_t& value) {
    value = kind == EfbPeekKind::Depth ? 0x00FFFFFFu : 0u;
    return true;
}
bool peek_efb_published(std::uint16_t x, std::uint16_t y, EfbPeekKind kind, std::uint32_t& value) {
    return peek_efb(x, y, kind, value);
}

void present_cached_xfb(std::uint32_t displayed_xfb_addr, bool present_swap_chain) {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    if (!present_swap_chain) return;
    ++b.presents;
    ++b.present_stats.requests;
    ++b.present_stats.presented;
    b.present_stats.last_presented_xfb_addr = displayed_xfb_addr;
    report(b);
}

void scan_pe_events(const std::byte* fifo_data, std::size_t fifo_size, GuestMemoryV1* memory,
                    const NativeServicesV1* services) {
    // The simulation-thread scan (GALAXY_SCAN_PE_EVENTS_ON_SIM_THREAD) raises
    // the same events render_frame would: one or the other is used.
    if (fifo_data == nullptr || fifo_size == 0) return;
    NullBackend& b = backend();
    PeCollector sink(b.state);
    {
        std::lock_guard lock(b.lock);
        (void)b.parser.run_available(std::span<const std::byte>(fifo_data, fifo_size), memory, sink, b.state);
    }
    deliver(services, sink.events);
}

void install_guest_dirty_tracker(GuestMemoryV1*) noexcept {}

void notify_guest_memory_write(std::uint32_t address, std::uint32_t size) noexcept {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    (void)b.parser.invalidate_display_list_cache_range(address, size);
}

void pump_messages() {}
bool quit_requested() noexcept { return false; }

HostPointerState poll_host_pointer_state() noexcept { return {}; }
host::HostPointerTransitionPoll poll_host_pointer_transition(std::uint64_t) noexcept { return {}; }
HostClientExtent current_host_client_extent() noexcept { return {3840, 2160}; }
HostClientExtent aspect_latch_extent() noexcept { return {3840, 2160}; }

bool native_settings_overlay_visible() noexcept { return false; }
bool native_window_input_captured() noexcept { return false; }
NativeSettingsOverlaySnapshot native_settings_overlay_snapshot() noexcept { return {}; }
NativeSettingsLayout compute_native_settings_layout(unsigned, unsigned, unsigned) noexcept { return {}; }
bool native_settings_handle_window_key(std::uintptr_t, bool) { return false; }
void native_settings_reset_overlay_state_for_tests() noexcept {}
RenderConfig native_settings_draft_for_tests() { return {}; }
void native_settings_post_key(std::uintptr_t) noexcept {}

std::uint64_t xfb_copy_count() noexcept { return backend().xfb_copies; }
std::uint64_t xfb_causal_trace_frame_index() noexcept { return backend().presents; }
XfbPresentStats xfb_present_stats() noexcept {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    return b.present_stats;
}
XfbMeasurementCadenceStats xfb_measurement_cadence_stats() noexcept { return {}; }
void reset_xfb_measurement_window_stats() noexcept {}

void shutdown() {
    NullBackend& b = backend();
    std::lock_guard lock(b.lock);
    b.initialized = false;
}

}  // namespace galaxy::gx

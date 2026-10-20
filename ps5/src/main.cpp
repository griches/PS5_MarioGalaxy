// Super Mario Galaxy for the PlayStation 5 - the title's main: Nebula's runtime
// starts at wmain with the game's content folder and module path, on a thread
// with the 256 MiB stack its Windows build reserves (guest threads nest their
// native frames on the simulation thread's stack before they get fibers).
//
// The title folder (/app0) holds content/ (game.pak and the loose partition
// files nebula-recomp package-content writes), RMGE01_boot_image.bin and the
// two module names the runtime checks for (the code itself is in this eboot).
//
// SPDX-License-Identifier: GPL-3.0-only
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include <ps5platform/heap.h>
#include <ps5platform/memory.h>

#include "platform.h"

int wmain(int argc, wchar_t** argv);
void ps5_register_modules();

namespace {

constexpr const char* kTitle = "/app0";
constexpr std::size_t kSimulationStack = 256u << 20;

void ensure_marker(const char* path) {
    struct stat status{};
    if (stat(path, &status) == 0) return;
    if (FILE* file = std::fopen(path, "wb")) {
        std::fputs("Linked into eboot.bin; the runtime only checks that this file exists.\n", file);
        std::fclose(file);
    }
}

void set_default(const char* name, const char* value) {
    if (std::getenv(name) == nullptr) setenv(name, value, 1);
}

// The platform layer points stderr at klog by swapping the FILE (a title may
// not dup2), after libc++ bound std::cerr to the old one: the C++ streams,
// which Nebula logs through, are pointed at the current stderr here.
class KlogBuffer final : public std::streambuf {
protected:
    int overflow(int c) override {
        if (c != traits_type::eof()) std::fputc(c, stderr);
        return c;
    }
    std::streamsize xsputn(const char* text, std::streamsize count) override {
        return static_cast<std::streamsize>(std::fwrite(text, 1, static_cast<std::size_t>(count), stderr));
    }
    int sync() override {
        std::fflush(stderr);
        return 0;
    }
};

class WideKlogBuffer final : public std::wstreambuf {
protected:
    int_type overflow(int_type c) override {
        if (c == traits_type::eof()) return c;
        const auto code = static_cast<std::uint32_t>(c);
        char utf8[4];
        std::size_t length = 0;
        if (code < 0x80) {
            utf8[length++] = static_cast<char>(code);
        } else if (code < 0x800) {
            utf8[length++] = static_cast<char>(0xC0 | (code >> 6));
            utf8[length++] = static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            utf8[length++] = static_cast<char>(0xE0 | (code >> 12));
            utf8[length++] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            utf8[length++] = static_cast<char>(0x80 | (code & 0x3F));
        } else {
            utf8[length++] = static_cast<char>(0xF0 | (code >> 18));
            utf8[length++] = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            utf8[length++] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            utf8[length++] = static_cast<char>(0x80 | (code & 0x3F));
        }
        std::fwrite(utf8, 1, length, stderr);
        return c;
    }
    int sync() override {
        std::fflush(stderr);
        return 0;
    }
};

// Everything written to stderr also goes to /app0/userdata/stderr.log: the
// runtime prints long diagnostic bursts, and klog drops lines under them. A
// write to the title folder costs over 100 ms on the console, so the file is
// buffered (1 MiB) and flushed by the memory guard every second and at exit.
FILE* g_klog_stream = nullptr;
FILE* g_log_file = nullptr;

// stderr becomes a pipe (the console's libc has no funopen); this thread
// copies what arrives to klog and to the file.
void tee_reader(int read_end) {
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(read_end, buffer, sizeof(buffer));
        if (count <= 0) continue;
        std::fwrite(buffer, 1, static_cast<std::size_t>(count), g_klog_stream);
        std::fflush(g_klog_stream);
        std::fwrite(buffer, 1, static_cast<std::size_t>(count), g_log_file);
    }
}

void tee_stderr_to_file() {
    g_log_file = std::fopen("/app0/userdata/stderr.log", "w");
    if (g_log_file == nullptr) return;
    std::setvbuf(g_log_file, nullptr, _IOFBF, 1u << 20);
    int ends[2];
    if (pipe(ends) != 0) return;
    FILE* const tee = fdopen(ends[1], "w");
    if (tee == nullptr) return;
    std::setvbuf(tee, nullptr, _IOLBF, 0);
    g_klog_stream = stderr;
    std::thread(tee_reader, ends[0]).detach();
    stderr = tee;
}

KlogBuffer g_klog_buffer;
WideKlogBuffer g_wide_klog_buffer;

void route_streams_to_klog() {
    std::cout.rdbuf(&g_klog_buffer);
    std::cerr.rdbuf(&g_klog_buffer);
    std::clog.rdbuf(&g_klog_buffer);
    std::wcout.rdbuf(&g_wide_klog_buffer);
    std::wcerr.rdbuf(&g_wide_klog_buffer);
    std::wclog.rdbuf(&g_wide_klog_buffer);
}

// The first run took the console's memory with it. A watchdog: the title's
// memory every two seconds, and an exit before the console runs short
// (GALAXY_PS5_MEMORY_FLOOR_MB, default 1536 MiB of direct memory left).
void memory_guard() {
    const char* floor_text = std::getenv("GALAXY_PS5_MEMORY_FLOOR_MB");
    const std::uint64_t floor = (floor_text != nullptr ? std::strtoull(floor_text, nullptr, 10) : 1536u) << 20;
    std::uint64_t last_reported = 0;
    for (;;) {
        struct ps5_memory_stats memory{};
        struct ps5_heap_stats heap{};
        const bool known = ps5_memory_query(&memory);
        ps5_heap_stats(&heap);
        const std::uint64_t used = memory.total_bytes - memory.free_bytes;
        if (known && (last_reported == 0 || used > last_reported + (64u << 20) || used + (64u << 20) < last_reported)) {
            say("[memory] direct free %llu MiB of %llu, flexible %llu MiB, heap %zu MiB",
                static_cast<unsigned long long>(memory.free_bytes >> 20),
                static_cast<unsigned long long>(memory.total_bytes >> 20),
                static_cast<unsigned long long>(memory.flexible_bytes >> 20), heap.mapped_bytes >> 20);
            last_reported = used;
        }
        if (known && memory.flexible_bytes != 0 && memory.flexible_bytes < (96u << 20)) {
            say("[memory] guard: only %llu MiB of flexible memory left, ending the title",
                static_cast<unsigned long long>(memory.flexible_bytes >> 20));
            std::fflush(nullptr);
            _exit(71);
        }
        if (known && memory.free_bytes < floor) {
            say("[memory] guard: only %llu MiB of direct memory left, ending the title",
                static_cast<unsigned long long>(memory.free_bytes >> 20));
            std::fflush(nullptr);
            _exit(70);
        }
        static unsigned ticks = 0;
        if (++ticks % 4u == 0u && g_log_file != nullptr) {
            std::fflush(stderr);
            std::fflush(g_log_file);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

struct Launch {
    std::vector<std::wstring> arguments;
    int result = 0;
};

void* run_runtime(void* opaque) {
    auto* launch = static_cast<Launch*>(opaque);
    std::vector<wchar_t*> argv;
    for (auto& argument : launch->arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    launch->result = wmain(static_cast<int>(launch->arguments.size()), argv.data());
    return nullptr;
}

}  // namespace

int main(int, char**) {
    mkdir("/app0/userdata", 0777);
    mkdir("/app0/userdata/saves", 0777);
    tee_stderr_to_file();
    route_streams_to_klog();
    std::thread(memory_guard).detach();
    ps5_register_modules();
    ensure_marker("/app0/RMGE01_game.dll");
    ensure_marker("/app0/RMGE01_home_button.dll");
    // The launcher's settings app does not exist here; saves live in the title folder.
    set_default("GALAXY_SKIP_LAUNCHER", "1");
    set_default("GALAXY_NAND_ROOT", "/app0/userdata/saves");
    set_default("GALAXY_UNBUFFERED_STDOUT", "1");

    Launch launch;
    launch.arguments = {L"/app0/eboot.bin", L"/app0/content", L"/app0/RMGE01_game.dll"};
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, kSimulationStack);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, run_runtime, &launch) != 0) {
        say("could not start the simulation thread with a %zu MiB stack", kSimulationStack >> 20);
        return 1;
    }
    pthread_attr_destroy(&attributes);
    pthread_join(thread, nullptr);
    say("the runtime returned %d", launch.result);
    return launch.result;
}

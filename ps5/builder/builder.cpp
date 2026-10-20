// Super Mario Galaxy for the PlayStation 5 - the builder: makes the PS5 title from
// the player's own disc on their computer (macOS or Windows), with the kit beside
// it (ps5/builder/make-kit.sh). Nothing of the game ships: nebula-recomp extracts
// and translates the disc here, the bundled clang compiles the translation for
// the PS5, and the kit's prebuilt rest of the title is linked to it.
//
//   galaxy-builder --disc IMAGE --out FOLDER [--kit KIT] [--jobs N] [--keep-work]
//
// FOLDER gets PPSA99315/, ready to copy to /data/homebrew/ on the console. The
// apps read standard output, one line a message:
//   STEP <n> <count> <text>     a new step
//   PROGRESS <done> <total>     within the step
//   LOG <text>                  a line of a tool's output (shown on request)
//   ERROR <text>                why it stopped (exit status 1)
//   DONE <folder>               the title folder (exit status 0)
//
// SPDX-License-Identifier: GPL-3.0-only
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#endif
extern char** environ;
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* kTitleId = "PPSA99315";
constexpr int kSteps = 7;

std::mutex g_out;
void say(const std::string& line) {
    std::lock_guard guard(g_out);
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}
void step(int n, const std::string& text) { say("STEP " + std::to_string(n) + " " + std::to_string(kSteps) + " " + text); }
void progress(std::uint64_t done, std::uint64_t total) {
    say("PROGRESS " + std::to_string(done) + " " + std::to_string(total));
}

struct Failure {
    std::string message;
};
[[noreturn]] void fail(const std::string& message) { throw Failure{message}; }

std::string u8(const fs::path& path) {
    const auto text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}
fs::path from_u8(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); }

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) return text;
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
    return text;
}

std::vector<std::string> read_lines(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) fail("The builder's kit is incomplete: " + u8(path) + " is missing.");
    std::vector<std::string> lines;
    for (std::string line; std::getline(file, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

// ---- running tools ------------------------------------------------------------------------
// Runs argv[0] with the arguments, in `cwd`; each line of its output (standard
// output and error together) goes to `on_line`. Returns the exit status.
#if defined(_WIN32)
std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}
// The quoting CommandLineToArgvW and the MSVC runtime undo.
void append_quoted(std::wstring& line, const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += argument;
        return;
    }
    line += L'"';
    for (std::size_t i = 0;; ++i) {
        std::size_t backslashes = 0;
        while (i < argument.size() && argument[i] == L'\\') {
            ++i;
            ++backslashes;
        }
        if (i == argument.size()) {
            line.append(backslashes * 2, L'\\');
            break;
        }
        if (argument[i] == L'"') {
            line.append(backslashes * 2 + 1, L'\\');
        } else {
            line.append(backslashes, L'\\');
        }
        line += argument[i];
    }
    line += L'"';
}
HANDLE g_job = nullptr;
int run(const std::vector<std::string>& argv, const fs::path& cwd, const std::function<void(const std::string&)>& on_line) {
    std::wstring line;
    for (const std::string& argument : argv) {
        if (!line.empty()) line += L' ';
        append_quoted(line, wide(argument));
    }
    SECURITY_ATTRIBUTES inherit{sizeof inherit, nullptr, TRUE};
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &inherit, 0)) fail("Could not start " + argv[0] + ".");
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_end;
    startup.hStdError = write_end;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const std::wstring directory = cwd.wstring();
    const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                                        nullptr, directory.c_str(), &startup, &process);
    CloseHandle(write_end);
    if (!started) {
        CloseHandle(read_end);
        fail("Could not start " + argv[0] + " (error " + std::to_string(GetLastError()) + ").");
    }
    if (g_job != nullptr) AssignProcessToJobObject(g_job, process.hProcess);
    ResumeThread(process.hThread);
    std::string pending;
    char buffer[8192];
    DWORD got = 0;
    while (ReadFile(read_end, buffer, sizeof buffer, &got, nullptr) && got != 0) {
        pending.append(buffer, got);
        for (std::size_t end; (end = pending.find('\n')) != std::string::npos;) {
            std::string text = pending.substr(0, end);
            if (!text.empty() && text.back() == '\r') text.pop_back();
            on_line(text);
            pending.erase(0, end + 1);
        }
    }
    if (!pending.empty()) on_line(pending);
    CloseHandle(read_end);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD status = 1;
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return static_cast<int>(status);
}
#else
int run(const std::vector<std::string>& argv, const fs::path& cwd, const std::function<void(const std::string&)>& on_line) {
    int pipe_ends[2];
    if (pipe(pipe_ends) != 0) fail("Could not start " + argv[0] + ".");
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe_ends[1], 1);
    posix_spawn_file_actions_adddup2(&actions, pipe_ends[1], 2);
    posix_spawn_file_actions_addclose(&actions, pipe_ends[0]);
    // (_np: posix_spawn_file_actions_addchdir is macOS 26 and later only)
    posix_spawn_file_actions_addchdir_np(&actions, u8(cwd).c_str());
    std::vector<char*> args;
    for (const std::string& argument : argv) args.push_back(const_cast<char*>(argument.c_str()));
    args.push_back(nullptr);
    pid_t pid = 0;
    const int spawned = posix_spawn(&pid, argv[0].c_str(), &actions, nullptr, args.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_ends[1]);
    if (spawned != 0) {
        close(pipe_ends[0]);
        fail("Could not start " + argv[0] + " (" + std::strerror(spawned) + ").");
    }
    std::string pending;
    char buffer[8192];
    for (;;) {
        const ssize_t got = read(pipe_ends[0], buffer, sizeof buffer);
        if (got <= 0) break;
        pending.append(buffer, static_cast<std::size_t>(got));
        for (std::size_t end; (end = pending.find('\n')) != std::string::npos;) {
            on_line(pending.substr(0, end));
            pending.erase(0, end + 1);
        }
    }
    if (!pending.empty()) on_line(pending);
    close(pipe_ends[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}
#endif

// Runs a tool and fails with `what` and the end of its output if it fails.
void run_or_fail(const std::vector<std::string>& argv, const fs::path& cwd, const std::string& what,
                 const std::function<void(const std::string&)>& on_line = {}) {
    std::deque<std::string> tail;
    const int status = run(argv, cwd, [&](const std::string& line) {
        if (on_line) on_line(line);
        tail.push_back(line);
        if (tail.size() > 40) tail.pop_front();
    });
    if (status != 0) {
        for (const std::string& line : tail) say("LOG " + line);
        fail(what + " (" + fs::path(argv[0]).filename().string() + " exited with status " + std::to_string(status) + ").");
    }
}

// ---- the machine ------------------------------------------------------------------------------
fs::path executable_path() {
#if defined(_WIN32)
    std::wstring path(32768, L'\0');
    path.resize(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())));
    return fs::path(path);
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    _NSGetExecutablePath(path.data(), &size);
    return fs::canonical(path.c_str());
#else
    return fs::canonical("/proc/self/exe");
#endif
}

std::uint64_t memory_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{sizeof status};
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
#elif defined(__APPLE__)
    std::uint64_t bytes = 0;
    std::size_t size = sizeof bytes;
    return sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0 ? bytes : 0;
#else
    return static_cast<std::uint64_t>(sysconf(_SC_PHYS_PAGES)) * static_cast<std::uint64_t>(sysconf(_SC_PAGE_SIZE));
#endif
}

#if defined(_WIN32)
constexpr const char* kExe = ".exe";
#else
constexpr const char* kExe = "";
#endif

// ---- compiling ----------------------------------------------------------------------------------
// The kit's PS5 target (x86_64-sie-ps5) puts constructors in .ctors, last to first,
// and its driver refuses -fuse-init-array; the title's CRT walks .init_array forwards
// (ps5/tools/cc-init-array.sh). As there: the driver's compiler job (-###) without
// -fno-use-init-array, run directly. Each kind of compile asks once, with probe names
// that each file's names then replace.
class Compiler {
public:
    Compiler(fs::path clang, fs::path work) : clang_(std::move(clang)), work_(std::move(work)) {
        fs::create_directories(work_ / "probe");
        std::ofstream(work_ / "probe" / "galaxy_probe_input.cpp") << "\n";
    }

    // The compiler job for `flags` with the probe file: argument vector.
    std::vector<std::string> job_for(const std::vector<std::string>& flags) {
        std::vector<std::string> argv{u8(clang_)};
#if defined(_WIN32)
        argv.push_back("--driver-mode=g++");
#endif
        argv.insert(argv.end(), flags.begin(), flags.end());
        argv.insert(argv.end(), {"-c", probe_input(), "-o", probe_output(), "-###"});
        std::vector<std::string> jobs;
        run_or_fail(argv, work_ / "probe", "The compiler did not start", [&](const std::string& line) {
            if (line.size() > 2 && line[0] == ' ' && line.find("\"-cc1\"") != std::string::npos) jobs.push_back(line);
        });
        if (jobs.size() != 1) fail("The compiler gave " + std::to_string(jobs.size()) + " jobs for one file.");
        std::vector<std::string> job;
        const std::string& line = jobs.front();
        for (std::size_t i = 0; i < line.size();) {
            if (line[i] != '"') {
                ++i;
                continue;
            }
            std::string argument;
            for (++i; i < line.size() && line[i] != '"'; ++i) {
                if (line[i] == '\\' && i + 1 < line.size()) ++i;
                argument += line[i];
            }
            ++i;
            if (argument != "-fno-use-init-array") job.push_back(argument);
        }
        return job;
    }

    // One compile with a job from job_for.
    void compile(const std::vector<std::string>& job, const fs::path& input, const fs::path& output) {
        std::vector<std::string> argv;
        const std::string in = u8(input), out = u8(output), in_name = input.filename().string();
        for (const std::string& argument : job) {
            std::string a = replace_all(argument, probe_output(), out);
            a = replace_all(a, probe_input(), in);
            a = replace_all(a, "galaxy_probe_input.cpp", in_name);
            argv.push_back(a);
        }
        run_or_fail(argv, work_, "Compiling " + in_name + " failed");
    }

private:
    std::string probe_input() const { return u8(work_ / "probe" / "galaxy_probe_input.cpp"); }
    std::string probe_output() const { return u8(work_ / "probe" / "galaxy_probe_output.o"); }
    fs::path clang_;
    fs::path work_;
};

std::vector<std::string> flag_file(const fs::path& kit, const std::string& name, const fs::path& generated) {
    std::vector<std::string> flags;
    for (const std::string& line : read_lines(kit / "flags" / (name + ".txt"))) {
        flags.push_back(replace_all(replace_all(line, "{KIT}", u8(kit)), "{GEN}", u8(generated)));
    }
    return flags;
}

void write_response_file(const fs::path& path, const std::vector<std::string>& arguments) {
    std::ofstream file(path, std::ios::binary);
    for (const std::string& argument : arguments) {
        file << '"';
        for (char c : argument) {
            if (c == '"' || c == '\\') file << '\\';
            file << c;
        }
        file << "\"\n";
    }
}

// ---- the build -------------------------------------------------------------------------------------
struct Options {
    fs::path disc;
    fs::path out;
    fs::path kit;
    unsigned jobs = 0;
    bool keep_work = false;
};

void build_title(const Options& options, const fs::path& work);

// The work folder (about 8 GB at most) goes whether the build finishes or fails.
void build(const Options& options) {
    const fs::path work = options.out / (std::string(kTitleId) + "-build");
    try {
        build_title(options, work);
    } catch (...) {
        std::error_code error;
        if (!options.keep_work) fs::remove_all(work, error);
        throw;
    }
}

void build_title(const Options& options, const fs::path& work) {
    const fs::path& kit = options.kit;
    const fs::path bin = kit / "bin";
    const fs::path recomp = bin / (std::string("nebula-recomp") + kExe);
    const fs::path clang = bin / (std::string("clang") + kExe);
#if defined(_WIN32)
    const fs::path lld = bin / "ld.lld.exe";
#else
    const fs::path lld = bin / "ld.lld";
#endif
    const fs::path ar = bin / (std::string("llvm-ar") + kExe);
    const fs::path native_tool = bin / (std::string("ps5-native-tool") + kExe);
    for (const fs::path& tool : {recomp, clang, lld, ar, native_tool}) {
        if (!fs::exists(tool)) fail("The builder's kit is incomplete: " + u8(tool) + " is missing.");
    }
    if (!fs::is_regular_file(options.disc)) fail("The disc image " + u8(options.disc) + " cannot be found.");

    const fs::path title = options.out / kTitleId;
    std::error_code error;
    fs::remove_all(work, error);
    fs::remove_all(title, error);
    fs::create_directories(work);
    const auto space = fs::space(options.out, error);
    if (!error && space.available < (std::uint64_t{12} << 30)) {
        fail("The building needs about 12 GB free on the drive with " + u8(options.out) + " (" +
             std::to_string(space.available >> 30) + " GB free).");
    }

    // 1. The disc
    step(1, "Checking your disc");
    std::string verdict;
    const int identified = run({u8(recomp), "identify", u8(options.disc)}, work, [&](const std::string& line) {
        say("LOG " + line);
        if (!verdict.empty()) verdict += " ";
        verdict += line;
    });
    if (verdict.rfind("Error: ", 0) == 0) verdict.erase(0, 7);
    if (identified != 0) {
        fail("This file isn't a Wii disc image the builder can read (" + verdict + ").");
    }
    if (verdict.find("Supported:") == std::string::npos) {
        fail("This disc image cannot be used: " + verdict +
             " The builder needs Super Mario Galaxy for the USA (RMGE01), revision 0.");
    }

    // nebula-recomp's progress, as PROGRESS lines
    const auto recomp_progress = [](const std::string& line) {
        if (line.rfind("PROGRESS ", 0) == 0) {
            say(line);
        } else if (!line.empty()) {
            say("LOG " + line);
        }
    };

    // 2. Extract
    step(2, "Extracting the disc");
    const fs::path disc = work / "disc";
    run_or_fail({u8(recomp), "extract", u8(options.disc), "--output", u8(disc)}, work, "Extracting the disc failed",
                recomp_progress);

    // 3. Translate
    step(3, "Translating the game's code");
    const fs::path generated = work / "generated";
    run_or_fail({u8(recomp), "generate", u8(disc), "--output", u8(generated)}, work,
                "Translating the game's code failed", recomp_progress);

    // 4. The game's data, then the extracted disc is no longer needed
    step(4, "Making the game's data");
    fs::create_directories(title);
    run_or_fail({u8(recomp), "package-content", u8(disc), "--output", u8(title / "content")}, work,
                "Making the game's data failed", recomp_progress);
    fs::copy_file(generated / "RMGE01_boot_image.bin", title / "RMGE01_boot_image.bin",
                  fs::copy_options::overwrite_existing);
    if (!options.keep_work) fs::remove_all(disc, error);

    // 5. Compile: the precompiled header first, the HOME Menu (the largest single
    // file) and the DSP alongside it, then the game's files.
    step(5, "Compiling the game for the PS5");
    const fs::path objects = work / "objects";
    fs::create_directories(objects / "game");
    Compiler compiler(clang, work);
    const auto game_flags = flag_file(kit, "game", generated);
    const auto home_flags = flag_file(kit, "home", generated);
    const auto dsp_flags = flag_file(kit, "dsp", generated);
    std::ofstream(work / "game_pch.hxx") << "#include \"" << u8(generated / "game" / "functions.h") << "\"\n";
    const fs::path pch = work / "game_pch.hxx.pch";
    std::vector<std::string> pch_flags = game_flags;
    pch_flags.insert(pch_flags.end(), {"-fpch-instantiate-templates", "-x", "c++-header"});
    std::vector<std::string> object_flags = game_flags;
    object_flags.insert(object_flags.end(), {"-include-pch", u8(pch)});
    const auto pch_job = compiler.job_for(pch_flags);
    const auto game_job = compiler.job_for(object_flags);
    const auto home_job = compiler.job_for(home_flags);
    const auto dsp_job = compiler.job_for(dsp_flags);

    std::vector<fs::path> game_sources;
    for (const auto& entry : fs::directory_iterator(generated / "game")) {
        if (entry.path().extension() == ".cpp") game_sources.push_back(entry.path());
    }
    std::sort(game_sources.begin(), game_sources.end());
    const std::uint64_t total = game_sources.size() + 3;
    std::atomic<std::uint64_t> done{0};
    std::mutex failure_lock;
    std::optional<Failure> failure;
    const auto guarded = [&](const std::function<void()>& work_item) {
        try {
            work_item();
        } catch (const Failure& f) {
            std::lock_guard guard(failure_lock);
            if (!failure) failure = f;
        }
        progress(++done, total);
    };
    unsigned jobs = options.jobs;
    if (jobs == 0) {
        // ~1.5 GB a compile at most (the HOME Menu's ~3.4 GB runs alone at first)
        const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
        const unsigned by_memory = static_cast<unsigned>(std::max<std::uint64_t>(1, memory_bytes() / (std::uint64_t{3} << 29)));
        jobs = std::clamp(std::min(cores, by_memory), 1u, 32u);
    }
    say("LOG compiling " + std::to_string(game_sources.size()) + " files with " + std::to_string(jobs) + " jobs");
    progress(0, total);
    std::thread home([&] {
        guarded([&] { compiler.compile(home_job, generated / "home" / "home_button_native.cpp", objects / "home.o"); });
    });
    std::thread dsp([&] {
        guarded([&] { compiler.compile(dsp_job, generated / "dsp" / "rmge01_dsp.cpp", objects / "dsp.o"); });
    });
    guarded([&] { compiler.compile(pch_job, work / "game_pch.hxx", pch); });
    if (failure) {
        home.join();
        dsp.join();
        throw *failure;
    }
    std::atomic<std::size_t> next{0};
    std::vector<std::thread> workers;
    for (unsigned w = 0; w < jobs; ++w) {
        workers.emplace_back([&] {
            for (std::size_t i; (i = next++) < game_sources.size();) {
                {
                    std::lock_guard guard(failure_lock);
                    if (failure) return;
                }
                guarded([&] {
                    compiler.compile(game_job, game_sources[i],
                                     objects / "game" / (game_sources[i].stem().string() + ".o"));
                });
            }
        });
    }
    for (std::thread& worker : workers) worker.join();
    home.join();
    dsp.join();
    if (failure) throw *failure;

    // 6. Link: the kit's prebuilt title with the player's three archives
    step(6, "Linking");
    const fs::path archives = work / "archives";
    fs::create_directories(archives);
    std::vector<std::string> members;
    for (const fs::path& source : game_sources) members.push_back(u8(objects / "game" / (source.stem().string() + ".o")));
    write_response_file(work / "game_members.rsp", members);
    run_or_fail({u8(ar), "rcs", u8(archives / "libgame_module.a"), "@" + u8(work / "game_members.rsp")}, work,
                "Archiving the game's code failed");
    run_or_fail({u8(ar), "rcs", u8(archives / "libhome_module.a"), u8(objects / "home.o")}, work,
                "Archiving the HOME Menu failed");
    run_or_fail({u8(ar), "rcs", u8(archives / "libdsp_module.a"), u8(objects / "dsp.o")}, work,
                "Archiving the DSP failed");
    const auto substitute = [&](const std::string& line) {
        return replace_all(replace_all(replace_all(line, "{KIT}", u8(kit)), "{GAME}", u8(archives)), "{WORK}", u8(work));
    };
    std::vector<std::string> link_arguments;
    for (const std::string& line : read_lines(kit / "link" / "lld.args")) link_arguments.push_back(substitute(line));
    write_response_file(work / "link.rsp", link_arguments);
    run_or_fail({u8(lld), "--rsp-quoting=posix", "@" + u8(work / "link.rsp")}, work, "Linking failed");
    std::vector<std::string> native{u8(native_tool)};
    for (const std::string& line : read_lines(kit / "link" / "native-link.args")) native.push_back(substitute(line));
    run_or_fail(native, work, "Making the PS5 executable failed");

    // 7. The title folder
    step(7, "Putting the title together");
    for (const auto& entry : fs::directory_iterator(kit / "title")) {
        fs::copy(entry.path(), title / entry.path().filename(),
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    run_or_fail({u8(native_tool), "self", "--sign", "--in", u8(work / "eboot.elf.new"), "--out", u8(title / "eboot.bin"),
                 "--magic", "0x1D3D154F"},
                work, "Signing the PS5 executable failed");
    if (!options.keep_work) fs::remove_all(work, error);
    say("DONE " + u8(title));
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                say("ERROR " + argument + " needs a value.");
                std::exit(2);
            }
            return argv[++i];
        };
        if (argument == "--disc") {
            options.disc = from_u8(value());
        } else if (argument == "--out") {
            options.out = from_u8(value());
        } else if (argument == "--kit") {
            options.kit = from_u8(value());
        } else if (argument == "--jobs") {
            options.jobs = static_cast<unsigned>(std::strtoul(value().c_str(), nullptr, 10));
        } else if (argument == "--keep-work") {
            options.keep_work = true;
        } else {
            say("ERROR usage: galaxy-builder --disc IMAGE --out FOLDER [--kit KIT] [--jobs N] [--keep-work]");
            return 2;
        }
    }
    if (options.disc.empty() || options.out.empty()) {
        say("ERROR usage: galaxy-builder --disc IMAGE --out FOLDER [--kit KIT] [--jobs N] [--keep-work]");
        return 2;
    }
#if defined(_WIN32)
    // The tools die with the builder (the app's Cancel closes it).
    g_job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
#else
    signal(SIGPIPE, SIG_IGN);
    // A group of its own: the app's Cancel ends the builder and its tools together.
    setpgid(0, 0);
#endif
    try {
        if (options.kit.empty()) {
            const fs::path here = executable_path().parent_path();
            for (const fs::path& candidate : {here / "kit", here.parent_path() / "Resources" / "kit"}) {
                if (fs::exists(candidate / "flags")) options.kit = candidate;
            }
            if (options.kit.empty()) fail("The builder's kit is missing (it belongs beside the builder).");
        }
        options.kit = fs::absolute(options.kit);
        options.out = fs::absolute(options.out);
        options.disc = fs::absolute(options.disc);
        fs::create_directories(options.out);
        const auto start = std::chrono::steady_clock::now();
        build(options);
        const auto minutes = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() / 60.0;
        char text[64];
        std::snprintf(text, sizeof text, "LOG built in %.1f minutes", minutes);
        say(text);
        return 0;
    } catch (const Failure& f) {
        say("ERROR " + f.message);
    } catch (const std::exception& e) {
        say(std::string("ERROR ") + e.what());
    }
    return 1;
}

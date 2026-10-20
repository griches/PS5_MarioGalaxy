// Super Mario Galaxy for the PlayStation 5 - structured exceptions from signals.
//
// Nebula's guest memory works by faulting: an access outside guest RAM (a
// hardware register) traps, and a vectored handler decodes the instruction,
// does the access in software and resumes after it (flat_guest_memory.cpp).
// Its crash reporter is a handler too. Here SIGSEGV, SIGBUS, SIGFPE, SIGILL and
// SIGTRAP become the EXCEPTION_RECORD and CONTEXT a Windows handler expects:
// the general registers from the signal's machine context, the XMM registers
// from its FXSAVE area (Windows' CONTEXT keeps them in the same layout), and
// the handler's changes go back the same way when it continues execution.
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#if defined(__PROSPERO__)
#include <ps5platform/context.h>
#endif

namespace {

struct Handler {
    PVECTORED_EXCEPTION_HANDLER function;
};

// Handlers in call order. Added rarely (at start-up), read in signal handlers:
// a fixed array published by count.
constexpr int kMaxHandlers = 16;
PVECTORED_EXCEPTION_HANDLER g_handlers[kMaxHandlers];
std::atomic<int> g_handler_count{0};
std::mutex g_handlers_lock;
std::atomic<LPTOP_LEVEL_EXCEPTION_FILTER> g_unhandled_filter{nullptr};
std::once_flag g_install_once;
bool g_fp_format_warned = false;

void context_from(const mcontext_t& machine, CONTEXT& context) noexcept {
    std::memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_FULL;
    context.Rax = static_cast<DWORD64>(machine.mc_rax);
    context.Rcx = static_cast<DWORD64>(machine.mc_rcx);
    context.Rdx = static_cast<DWORD64>(machine.mc_rdx);
    context.Rbx = static_cast<DWORD64>(machine.mc_rbx);
    context.Rsp = static_cast<DWORD64>(machine.mc_rsp);
    context.Rbp = static_cast<DWORD64>(machine.mc_rbp);
    context.Rsi = static_cast<DWORD64>(machine.mc_rsi);
    context.Rdi = static_cast<DWORD64>(machine.mc_rdi);
    context.R8 = static_cast<DWORD64>(machine.mc_r8);
    context.R9 = static_cast<DWORD64>(machine.mc_r9);
    context.R10 = static_cast<DWORD64>(machine.mc_r10);
    context.R11 = static_cast<DWORD64>(machine.mc_r11);
    context.R12 = static_cast<DWORD64>(machine.mc_r12);
    context.R13 = static_cast<DWORD64>(machine.mc_r13);
    context.R14 = static_cast<DWORD64>(machine.mc_r14);
    context.R15 = static_cast<DWORD64>(machine.mc_r15);
    context.Rip = static_cast<DWORD64>(machine.mc_rip);
    context.EFlags = static_cast<DWORD>(machine.mc_rflags);
    context.SegCs = static_cast<WORD>(machine.mc_cs);
    context.SegSs = static_cast<WORD>(machine.mc_ss);
    if (machine.mc_fpformat == _MC_FPFMT_XMM) {
        static_assert(sizeof(machine.mc_fpstate) >= 512);
        std::memcpy(context.FltSave, machine.mc_fpstate, 512);
        std::memcpy(&context.MxCsr, context.FltSave + 24, sizeof(DWORD));
    } else if (!g_fp_format_warned) {
        g_fp_format_warned = true;
        static const char message[] = "[compat] signal context without FXSAVE state: XMM registers unavailable\n";
        std::fputs(message, stderr);
    }
}

void context_to(const CONTEXT& context, mcontext_t& machine) noexcept {
    machine.mc_rax = static_cast<__register_t>(context.Rax);
    machine.mc_rcx = static_cast<__register_t>(context.Rcx);
    machine.mc_rdx = static_cast<__register_t>(context.Rdx);
    machine.mc_rbx = static_cast<__register_t>(context.Rbx);
    machine.mc_rsp = static_cast<__register_t>(context.Rsp);
    machine.mc_rbp = static_cast<__register_t>(context.Rbp);
    machine.mc_rsi = static_cast<__register_t>(context.Rsi);
    machine.mc_rdi = static_cast<__register_t>(context.Rdi);
    machine.mc_r8 = static_cast<__register_t>(context.R8);
    machine.mc_r9 = static_cast<__register_t>(context.R9);
    machine.mc_r10 = static_cast<__register_t>(context.R10);
    machine.mc_r11 = static_cast<__register_t>(context.R11);
    machine.mc_r12 = static_cast<__register_t>(context.R12);
    machine.mc_r13 = static_cast<__register_t>(context.R13);
    machine.mc_r14 = static_cast<__register_t>(context.R14);
    machine.mc_r15 = static_cast<__register_t>(context.R15);
    machine.mc_rip = static_cast<__register_t>(context.Rip);
    // Only the arithmetic flags a handler computes (CF PF AF ZF SF OF)
    constexpr DWORD64 kArithmetic = 0x8D5;
    machine.mc_rflags = static_cast<__register_t>((static_cast<DWORD64>(machine.mc_rflags) & ~kArithmetic) |
                                                  (context.EFlags & kArithmetic));
    if (machine.mc_fpformat == _MC_FPFMT_XMM) {
        // The XMM registers only (offset 160, 16 of them): the control state stays the thread's
        std::memcpy(reinterpret_cast<std::uint8_t*>(machine.mc_fpstate) + 160, context.FltSave + 160, 256);
    }
}

DWORD code_for(int signal_number, const siginfo_t* info) noexcept {
    switch (signal_number) {
    case SIGSEGV:
    case SIGBUS: return EXCEPTION_ACCESS_VIOLATION;
    case SIGILL: return EXCEPTION_ILLEGAL_INSTRUCTION;
    case SIGTRAP: return EXCEPTION_BREAKPOINT;
    case SIGFPE:
        switch (info != nullptr ? info->si_code : 0) {
        case FPE_INTDIV: return EXCEPTION_INT_DIVIDE_BY_ZERO;
        case FPE_INTOVF: return EXCEPTION_INT_OVERFLOW;
        case FPE_FLTDIV: return EXCEPTION_FLT_DIVIDE_BY_ZERO;
        case FPE_FLTOVF: return EXCEPTION_FLT_OVERFLOW;
        case FPE_FLTUND: return EXCEPTION_FLT_UNDERFLOW;
        case FPE_FLTRES: return EXCEPTION_FLT_INEXACT_RESULT;
        default: return EXCEPTION_FLT_INVALID_OPERATION;
        }
    default: return EXCEPTION_NONCONTINUABLE_EXCEPTION;
    }
}

void say_fatal(int signal_number, const siginfo_t* info, const CONTEXT& context) noexcept {
    char line[256];
    const int length = std::snprintf(line, sizeof(line),
                                     "[compat] fatal signal %d at rip=0x%016llx address=%p rsp=0x%016llx thread=%u\n",
                                     signal_number, static_cast<unsigned long long>(context.Rip),
                                     info != nullptr ? info->si_addr : nullptr,
                                     static_cast<unsigned long long>(context.Rsp), GetCurrentThreadId());
    if (length > 0) std::fwrite(line, 1, static_cast<std::size_t>(length), stderr);
    // Frame-pointer walk (the console build keeps frame pointers)
    auto* frame = reinterpret_cast<std::uintptr_t*>(context.Rbp);
    for (int depth = 0; depth < 24 && frame != nullptr && (reinterpret_cast<std::uintptr_t>(frame) & 7) == 0; ++depth) {
        const std::uintptr_t return_address = frame[1];
        if (return_address == 0) break;
        const int n = std::snprintf(line, sizeof(line), "[compat]   #%d 0x%016llx\n", depth,
                                    static_cast<unsigned long long>(return_address));
        if (n > 0) std::fwrite(line, 1, static_cast<std::size_t>(n), stderr);
        auto* next = reinterpret_cast<std::uintptr_t*>(frame[0]);
        if (next <= frame) break;
        frame = next;
    }
}

void on_signal(int signal_number, siginfo_t* info, void* opaque) {
    auto* ucontext = static_cast<ucontext_t*>(opaque);
    const int saved_errno = errno;
    EXCEPTION_RECORD record{};
    record.ExceptionCode = code_for(signal_number, info);
    record.ExceptionAddress = reinterpret_cast<PVOID>(ucontext->uc_mcontext.mc_rip);
    if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        record.NumberParameters = 2;
        // Page-fault error code bit 1: the access was a write
        record.ExceptionInformation[0] = (ucontext->uc_mcontext.mc_err & 2) != 0 ? 1 : 0;
        record.ExceptionInformation[1] = reinterpret_cast<ULONG_PTR>(info != nullptr ? info->si_addr : nullptr);
    }
    alignas(16) CONTEXT context;
    context_from(ucontext->uc_mcontext, context);
    EXCEPTION_POINTERS pointers{&record, &context};

    const int count = g_handler_count.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (g_handlers[i](&pointers) == EXCEPTION_CONTINUE_EXECUTION) {
            context_to(context, ucontext->uc_mcontext);
            errno = saved_errno;
            return;
        }
    }
    if (const auto filter = g_unhandled_filter.load(std::memory_order_acquire)) {
        if (filter(&pointers) == EXCEPTION_CONTINUE_EXECUTION) {
            context_to(context, ucontext->uc_mcontext);
            errno = saved_errno;
            return;
        }
    }
    say_fatal(signal_number, info, context);
    std::fflush(nullptr);
    // The console's default action ends the title: return to the faulting
    // instruction with the default disposition back in place.
    struct sigaction action{};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigaction(signal_number, &action, nullptr);
    if (signal_number == SIGTRAP || signal_number == SIGFPE) raise(signal_number);
}

void install_signal_handlers() noexcept {
    struct sigaction action{};
    action.sa_sigaction = on_signal;
    action.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&action.sa_mask);
    for (const int signal_number : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGTRAP}) sigaction(signal_number, &action, nullptr);
}

}  // namespace

PVOID AddVectoredExceptionHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler) noexcept {
    std::call_once(g_install_once, install_signal_handlers);
    std::lock_guard lock(g_handlers_lock);
    const int count = g_handler_count.load(std::memory_order_relaxed);
    if (count >= kMaxHandlers || handler == nullptr) return nullptr;
    if (first != 0) {
        for (int i = count; i > 0; --i) g_handlers[i] = g_handlers[i - 1];
        g_handlers[0] = handler;
    } else {
        g_handlers[count] = handler;
    }
    g_handler_count.store(count + 1, std::memory_order_release);
    return reinterpret_cast<PVOID>(handler);
}

ULONG RemoveVectoredExceptionHandler(PVOID handle) noexcept {
    std::lock_guard lock(g_handlers_lock);
    const int count = g_handler_count.load(std::memory_order_relaxed);
    for (int i = 0; i < count; ++i) {
        if (reinterpret_cast<PVOID>(g_handlers[i]) == handle) {
            for (int k = i; k + 1 < count; ++k) g_handlers[k] = g_handlers[k + 1];
            g_handler_count.store(count - 1, std::memory_order_release);
            return 1;
        }
    }
    return 0;
}

PVOID AddVectoredContinueHandler(ULONG, PVECTORED_EXCEPTION_HANDLER handler) noexcept {
    return reinterpret_cast<PVOID>(handler);
}
ULONG RemoveVectoredContinueHandler(PVOID) noexcept { return 1; }

LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter) noexcept {
    std::call_once(g_install_once, install_signal_handlers);
    return g_unhandled_filter.exchange(filter, std::memory_order_acq_rel);
}

LONG UnhandledExceptionFilter(PEXCEPTION_POINTERS) noexcept { return EXCEPTION_EXECUTE_HANDLER; }

void RaiseException(DWORD code, DWORD, DWORD, const ULONG_PTR*) noexcept {
    std::fprintf(stderr, "[compat] RaiseException 0x%08X\n", code);
    std::fflush(stderr);
    std::abort();
}

void RaiseFailFastException(PEXCEPTION_RECORD record, PCONTEXT, DWORD) noexcept {
    std::fprintf(stderr, "[compat] fail-fast 0x%08X\n", record != nullptr ? record->ExceptionCode : 0u);
    std::fflush(stderr);
    std::_Exit(3);
}

void RtlCaptureContext(PCONTEXT context) noexcept {
    if (context == nullptr) return;
    std::memset(context, 0, sizeof(*context));
    context->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    context->Rip = reinterpret_cast<DWORD64>(__builtin_return_address(0));
    context->Rbp = reinterpret_cast<DWORD64>(__builtin_frame_address(1));
    context->Rsp = reinterpret_cast<DWORD64>(__builtin_frame_address(0)) + 16;
}

WORD RtlCaptureStackBackTrace(DWORD skip, DWORD count, PVOID* trace, PDWORD hash) noexcept {
    auto* frame = static_cast<std::uintptr_t*>(__builtin_frame_address(0));
    WORD captured = 0;
    DWORD seen = 0;
    while (frame != nullptr && captured < count && (reinterpret_cast<std::uintptr_t>(frame) & 7) == 0) {
        const std::uintptr_t return_address = frame[1];
        if (return_address == 0) break;
        if (seen++ >= skip) trace[captured++] = reinterpret_cast<PVOID>(return_address);
        auto* next = reinterpret_cast<std::uintptr_t*>(frame[0]);
        if (next <= frame) break;
        frame = next;
    }
    if (hash != nullptr) *hash = captured;
    return captured;
}

// No Windows unwind tables in an ELF: the callers' walks end at once.
PRUNTIME_FUNCTION RtlLookupFunctionEntry(DWORD64, PDWORD64 image_base, PUNWIND_HISTORY_TABLE) noexcept {
    if (image_base != nullptr) *image_base = 0;
    return nullptr;
}
PEXCEPTION_ROUTINE RtlVirtualUnwind(DWORD, DWORD64, DWORD64, PRUNTIME_FUNCTION, PCONTEXT context, PVOID* handler_data,
                                    PDWORD64 frame, PKNONVOLATILE_CONTEXT_POINTERS) noexcept {
    if (handler_data != nullptr) *handler_data = nullptr;
    if (frame != nullptr) *frame = 0;
    if (context != nullptr) context->Rip = 0;
    return nullptr;
}
BOOL GetThreadContext(HANDLE, LPCONTEXT) noexcept {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
BOOL SetThreadContext(HANDLE, const CONTEXT*) noexcept {
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
BOOL FlushInstructionCache(HANDLE, LPCVOID, SIZE_T) noexcept { return TRUE; }
BOOL IsDebuggerPresent() noexcept { return FALSE; }
void DebugBreak() noexcept { __builtin_debugtrap(); }
void OutputDebugStringA(LPCSTR text) noexcept {
    if (text != nullptr) std::fputs(text, stderr);
}
void OutputDebugStringW(LPCWSTR text) noexcept {
    if (text != nullptr) std::fputs(compat::utf8(text).c_str(), stderr);
}

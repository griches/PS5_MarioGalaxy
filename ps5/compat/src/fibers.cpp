// Super Mario Galaxy for the PlayStation 5 - fibers, for Nebula's cooperative
// scheduler: every guest thread runs on a fiber of the simulation thread, and a
// guest thread switch is one SwitchToFiber. A switch saves the System V
// callee-saved registers, MXCSR and the x87 control word on the old stack and
// loads them from the new one (FIBER_FLAG_FLOAT_SWITCH's behaviour, always).
//
// SPDX-License-Identifier: GPL-3.0-only
#include "compat_internal.h"

#include <cstdio>
#include <cstdlib>
#include <atomic>

namespace {

struct Fiber {
    void* stack_pointer = nullptr;  // saved by a switch away
    void* stack = nullptr;          // allocated stack (null for a converted thread)
    SIZE_T stack_size = 0;
    LPFIBER_START_ROUTINE start = nullptr;
    LPVOID parameter = nullptr;
};

thread_local Fiber* t_current = nullptr;

}  // namespace

// compat_fiber_switch(void** save, void* load): park this context's stack
// pointer in *save and continue on `load`.
extern "C" void compat_fiber_switch(void** save, void* load);
__asm__(R"(
    .text
    .p2align 4
    .globl compat_fiber_switch
    .type compat_fiber_switch, @function
compat_fiber_switch:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq $16, %rsp
    stmxcsr 8(%rsp)
    fnstcw (%rsp)
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    fldcw (%rsp)
    ldmxcsr 8(%rsp)
    addq $16, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
    .size compat_fiber_switch, .-compat_fiber_switch
)");

extern "C" [[noreturn]] void compat_fiber_entry() {
    Fiber* const fiber = t_current;
    fiber->start(fiber->parameter);
    // A fiber's routine returning ends its thread on Windows; Nebula's never return.
    std::fprintf(stderr, "[compat] a fiber's start routine returned\n");
    std::abort();
}

LPVOID ConvertThreadToFiberEx(LPVOID parameter, DWORD) noexcept {
    if (t_current != nullptr) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return nullptr;
    }
    auto* fiber = new Fiber();
    fiber->parameter = parameter;
    t_current = fiber;
    return fiber;
}

LPVOID ConvertThreadToFiber(LPVOID parameter) noexcept { return ConvertThreadToFiberEx(parameter, 0); }

BOOL ConvertFiberToThread() noexcept {
    if (t_current == nullptr || t_current->stack != nullptr) return FALSE;
    delete t_current;
    t_current = nullptr;
    return TRUE;
}

LPVOID CreateFiberEx(SIZE_T, SIZE_T reserve, DWORD, LPFIBER_START_ROUTINE start, LPVOID parameter) noexcept {
    // The whole reserve, at least 16 MiB: translated guest functions keep large
    // frames, and the title heap (direct memory) has room.
    SIZE_T size = reserve < (16u << 20) ? (16u << 20) : reserve;
    size = (size + 0xFFFF) & ~SIZE_T{0xFFFF};
    void* stack = nullptr;
    if (posix_memalign(&stack, 0x10000, size) != 0 || stack == nullptr) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    static std::atomic<unsigned> created{0};
    const unsigned count = created.fetch_add(1) + 1;
    if (count <= 64 || (count & (count - 1)) == 0) {
        std::fprintf(stderr, "[compat] fiber %u created (%zu MiB stack)\n", count, static_cast<std::size_t>(size >> 20));
    }
    auto* fiber = new Fiber();
    fiber->stack = stack;
    fiber->stack_size = size;
    fiber->start = start;
    fiber->parameter = parameter;

    // The first switch to it pops this frame (see compat_fiber_switch) and
    // "returns" into compat_fiber_entry with the stack as a call leaves it.
    auto* top = reinterpret_cast<std::uint64_t*>((reinterpret_cast<std::uintptr_t>(stack) + size) & ~std::uintptr_t{15});
    std::uint32_t mxcsr = 0;
    std::uint16_t fcw = 0;
    __asm__ volatile("stmxcsr %0" : "=m"(mxcsr));
    __asm__ volatile("fnstcw %0" : "=m"(fcw));
    top[-1] = 0;                                                     // compat_fiber_entry's return address
    top[-2] = reinterpret_cast<std::uint64_t>(&compat_fiber_entry);  // ret
    for (int i = 3; i <= 8; ++i) top[-i] = 0;                        // rbp rbx r12 r13 r14 r15
    top[-9] = mxcsr;                                                 // 8(%rsp)
    top[-10] = fcw;                                                  // (%rsp)
    fiber->stack_pointer = &top[-10];
    return fiber;
}

LPVOID CreateFiber(SIZE_T stack_size, LPFIBER_START_ROUTINE start, LPVOID parameter) noexcept {
    return CreateFiberEx(stack_size, stack_size, 0, start, parameter);
}

void DeleteFiber(LPVOID handle) noexcept {
    auto* fiber = static_cast<Fiber*>(handle);
    if (fiber == nullptr) return;
    if (fiber == t_current) {
        std::fprintf(stderr, "[compat] DeleteFiber on the running fiber\n");
        std::abort();
    }
    std::free(fiber->stack);
    delete fiber;
}

void SwitchToFiber(LPVOID handle) noexcept {
    auto* target = static_cast<Fiber*>(handle);
    Fiber* const self = t_current;
    if (target == self) return;
    t_current = target;
    compat_fiber_switch(&self->stack_pointer, target->stack_pointer);
}

PVOID GetCurrentFiber() noexcept { return t_current; }
PVOID GetFiberData() noexcept { return t_current != nullptr ? t_current->parameter : nullptr; }
BOOL IsThreadAFiber() noexcept { return t_current != nullptr; }

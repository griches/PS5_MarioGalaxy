// Super Mario Galaxy for the PlayStation 5 - what Nebula takes from C++20's
// library that the payload SDK's libc++ (LLVM 18) does not have yet:
// std::atomic_ref (libc++ 19). Included in every unit through msvc_crt.h.
//
// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <atomic>
#include <type_traits>

#if !defined(__cpp_lib_atomic_ref)
namespace std {
template <typename T>
class atomic_ref {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    using value_type = T;
    static constexpr bool is_always_lock_free = __atomic_always_lock_free(sizeof(T), 0);
    static constexpr std::size_t required_alignment = alignof(T) < sizeof(T) ? sizeof(T) : alignof(T);

    explicit atomic_ref(T& object) noexcept : pointer_(std::addressof(object)) {}
    atomic_ref(const atomic_ref&) noexcept = default;
    atomic_ref& operator=(const atomic_ref&) = delete;

    T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
        T value;
        __atomic_load(pointer_, &value, static_cast<int>(order));
        return value;
    }
    void store(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        __atomic_store(pointer_, &value, static_cast<int>(order));
    }
    operator T() const noexcept { return load(); }
    T operator=(T value) const noexcept {
        store(value);
        return value;
    }
    T exchange(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        T previous;
        __atomic_exchange(pointer_, &value, &previous, static_cast<int>(order));
        return previous;
    }
    bool compare_exchange_weak(T& expected, T desired, std::memory_order success,
                               std::memory_order failure) const noexcept {
        return __atomic_compare_exchange(pointer_, &expected, &desired, true, static_cast<int>(success),
                                         static_cast<int>(failure));
    }
    bool compare_exchange_strong(T& expected, T desired, std::memory_order success,
                                 std::memory_order failure) const noexcept {
        return __atomic_compare_exchange(pointer_, &expected, &desired, false, static_cast<int>(success),
                                         static_cast<int>(failure));
    }
    bool compare_exchange_weak(T& expected, T desired,
                               std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return compare_exchange_weak(expected, desired, order, failure_order(order));
    }
    bool compare_exchange_strong(T& expected, T desired,
                                 std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return compare_exchange_strong(expected, desired, order, failure_order(order));
    }
    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
    T fetch_add(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_add(pointer_, value, static_cast<int>(order));
    }
    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
    T fetch_sub(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_sub(pointer_, value, static_cast<int>(order));
    }
    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
    T fetch_and(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_and(pointer_, value, static_cast<int>(order));
    }
    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
    T fetch_or(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_or(pointer_, value, static_cast<int>(order));
    }
    template <typename U = T, typename = std::enable_if_t<std::is_integral_v<U>>>
    T fetch_xor(T value, std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return __atomic_fetch_xor(pointer_, value, static_cast<int>(order));
    }
    bool is_lock_free() const noexcept { return __atomic_is_lock_free(sizeof(T), pointer_); }

private:
    static constexpr std::memory_order failure_order(std::memory_order order) noexcept {
        return order == std::memory_order_acq_rel ? std::memory_order_acquire
               : order == std::memory_order_release ? std::memory_order_relaxed
                                                    : order;
    }
    T* pointer_;
};
}  // namespace std
#endif

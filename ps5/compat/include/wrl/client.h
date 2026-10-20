// Super Mario Galaxy for the PlayStation 5 - Microsoft::WRL::ComPtr over objbase.h's IUnknown.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../objbase.h"
#include <cstddef>
#include <utility>

namespace Microsoft::WRL {
template <typename T>
class ComPtr {
public:
    using InterfaceType = T;
    ComPtr() noexcept = default;
    ComPtr(std::nullptr_t) noexcept {}
    template <typename U>
    ComPtr(U* other) noexcept : ptr_(other) { add_ref(); }
    ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) { add_ref(); }
    template <typename U>
    ComPtr(const ComPtr<U>& other) noexcept : ptr_(other.Get()) { add_ref(); }
    ComPtr(ComPtr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}
    ~ComPtr() { release(); }
    ComPtr& operator=(std::nullptr_t) noexcept { release(); return *this; }
    ComPtr& operator=(T* other) noexcept {
        if (ptr_ != other) { ComPtr(other).Swap(*this); }
        return *this;
    }
    ComPtr& operator=(const ComPtr& other) noexcept {
        if (ptr_ != other.ptr_) { ComPtr(other).Swap(*this); }
        return *this;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        ComPtr(std::move(other)).Swap(*this);
        return *this;
    }
    void Swap(ComPtr& other) noexcept { std::swap(ptr_, other.ptr_); }
    T* Get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T** GetAddressOf() noexcept { return &ptr_; }
    T* const* GetAddressOf() const noexcept { return &ptr_; }
    T** ReleaseAndGetAddressOf() noexcept { release(); return &ptr_; }
    T** operator&() noexcept { return ReleaseAndGetAddressOf(); }
    T* Detach() noexcept { return std::exchange(ptr_, nullptr); }
    void Attach(T* other) noexcept { release(); ptr_ = other; }
    ULONG Reset() noexcept { return release(); }
    template <typename U>
    HRESULT As(ComPtr<U>* other) const noexcept {
        return ptr_ != nullptr ? ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(other->ReleaseAndGetAddressOf()))
                               : E_POINTER;
    }
    // `&other` is ComPtr's operator&, which yields the released pointer's address.
    template <typename U>
    HRESULT As(U** other) const noexcept {
        return ptr_ != nullptr ? ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(other)) : E_POINTER;
    }
    HRESULT CopyTo(T** other) const noexcept { add_ref(); *other = ptr_; return S_OK; }
    friend bool operator==(const ComPtr& a, std::nullptr_t) noexcept { return a.ptr_ == nullptr; }
    friend bool operator!=(const ComPtr& a, std::nullptr_t) noexcept { return a.ptr_ != nullptr; }
    friend bool operator==(const ComPtr& a, const ComPtr& b) noexcept { return a.ptr_ == b.ptr_; }

private:
    void add_ref() const noexcept { if (ptr_ != nullptr) ptr_->AddRef(); }
    ULONG release() noexcept {
        ULONG count = 0;
        if (T* const old = std::exchange(ptr_, nullptr)) count = old->Release();
        return count;
    }
    T* ptr_ = nullptr;
};
}  // namespace Microsoft::WRL

#pragma once

#include <Windows.h>
#include <winhttp.h>
#include <functional>
#include <utility> // For std::move, std::swap

namespace Infrastructure {

// RAII wrapper for Windows handles (HANDLE, HINTERNET, etc.)
// Takes the handle type and the closer function as template parameters.
template <typename HandleT, auto CloserFn, HandleT InvalidValue = nullptr>
class HandleGuard {
public:
    // Default constructor (empty handle)
    HandleGuard() noexcept : handle_(InvalidValue) {}

    // Constructor taking ownership of an existing handle
    explicit HandleGuard(HandleT handle) noexcept : handle_(handle) {}

    // Destructor - closes the handle if valid
    ~HandleGuard() {
        close();
    }

    // Disable copy constructor and copy assignment
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    // Move constructor - transfers ownership
    HandleGuard(HandleGuard&& other) noexcept : handle_(other.release()) {}

    // Move assignment operator - transfers ownership
    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            close(); // Close existing handle first
            handle_ = other.release();
        }
        return *this;
    }

    // Gets the underlying handle value
    HandleT get() const noexcept {
        return handle_;
    }

    // Checks if the handle is valid
    explicit operator bool() const noexcept {
        return handle_ != InvalidValue;
    }

    // Releases ownership of the handle
    HandleT release() noexcept {
        HandleT temp = handle_;
        handle_ = InvalidValue;
        return temp;
    }

    // Resets the guard, closing the current handle (if any)
    // and taking ownership of a new handle.
    void reset(HandleT new_handle = InvalidValue) noexcept {
        if (handle_ != new_handle) {
            close();
            handle_ = new_handle;
        }
    }

    // Swaps the managed handle with another HandleGuard
    void swap(HandleGuard& other) noexcept {
        std::swap(handle_, other.handle_);
    }

private:
    HandleT handle_;

    // Closes the handle if it's valid
    void close() noexcept {
        if (handle_ != InvalidValue) {
            CloserFn(handle_);
            handle_ = InvalidValue;
        }
    }
};

// Helper function for swapping HandleGuards
template <typename HandleT, auto CloserFn, HandleT InvalidValue>
void swap(HandleGuard<HandleT, CloserFn, InvalidValue>& a, HandleGuard<HandleT, CloserFn, InvalidValue>& b) noexcept {
    a.swap(b);
}

// Define common handle types using WinHttpCloseHandle
using WinHttpHandleGuard = HandleGuard<HINTERNET, WinHttpCloseHandle>;

// Define common handle types using CloseHandle
using GenericHandleGuard = HandleGuard<HANDLE, CloseHandle>;


} // namespace Infrastructure 
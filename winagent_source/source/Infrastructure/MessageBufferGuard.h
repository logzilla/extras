#pragma once

// #include "../Agent/Globals.h" // Removed dependency
#include "IBufferManager.h"     // Added dependency on the interface
#include "PoolResourceMonitor.h"
#include <string>
#include <utility>
#include <stdexcept> // For assert or exceptions if needed

/**
    * RAII wrapper for message buffers obtained from an IBufferManager.
    * Automatically releases the buffer via the manager when going out of scope.
    */
class MessageBufferGuard {
private:
    char* buffer_;
    IBufferManager& bufferManager_; // Reference to the buffer manager
    bool released_;
    std::string name_;

public:
    /**
        * @brief Acquires a buffer from the given manager.
        * @param name An identifier for the buffer.
        * @param manager The buffer manager to use for allocation/release.
        */
    explicit MessageBufferGuard(const char* name, IBufferManager& manager)
        : buffer_(nullptr), bufferManager_(manager), released_(false), name_(name ? name : "unnamed") {
        buffer_ = bufferManager_.allocateBuffer(name_.c_str());
        // Still notify the global resource monitor if it's part of Infrastructure
        PoolResourceMonitor::instance().recordAllocation(name_); 
        if (!buffer_) {
            // Handle allocation failure? Throw exception or require check?
            // For now, let's assume allocation succeeds or user checks get()
        }
    }

    ~MessageBufferGuard() {
        release();
    }

    // Prevent copying
    MessageBufferGuard(const MessageBufferGuard&) = delete;
    MessageBufferGuard& operator=(const MessageBufferGuard&) = delete;

    // Allow moving
    MessageBufferGuard(MessageBufferGuard&& other) noexcept
        : buffer_(other.buffer_),
            bufferManager_(other.bufferManager_), // Copy manager reference
            released_(other.released_),
            name_(std::move(other.name_)) {
        other.buffer_ = nullptr;
        other.released_ = true;
    }

    MessageBufferGuard& operator=(MessageBufferGuard&& other) noexcept {
        if (this != &other) {
            // Ensure managers are the same instance before moving.
            // Move assignment between guards from different managers is problematic.
            if (&bufferManager_ != &other.bufferManager_) {
                    // Or throw, or handle differently? Asserting is reasonable during dev.
                throw std::logic_error("Cannot move assign MessageBufferGuard with different managers");
                }

            release(); // Release the current buffer
            buffer_ = other.buffer_;
            // bufferManager_ reference does not change
            released_ = other.released_;
            name_ = std::move(other.name_);
            other.buffer_ = nullptr;
            other.released_ = true;
        }
        return *this;
    }

    void release() {
        if (!released_ && buffer_) {
            // Still notify the global resource monitor if it's part of Infrastructure
            PoolResourceMonitor::instance().recordRelease(name_); 
            bufferManager_.releaseBuffer(buffer_); // Use the member manager
            released_ = true;
            buffer_ = nullptr;
        }
    }

    char* get() const { return buffer_; }
    operator char*() const { return buffer_; }

    /**
        * @brief Detach the buffer from this RAII object without releasing it.
        * @return The raw buffer pointer. The caller is now responsible for releasing
        *         it using the SAME IBufferManager instance that allocated it.
        */
    char* detach() {
        if (!buffer_) return nullptr;
        released_ = true; // Mark as released so destructor/release() does nothing
        return std::exchange(buffer_, nullptr);
    }
};

#pragma once

#include "MessageBatcher.h" // Include the base class definition for ReleaseBatchBuffer
#include <utility>         // For std::move

namespace Syslog_agent {

/**
 * @brief RAII wrapper for batch buffers obtained from a MessageBatcher.
 *
 * Ensures that ReleaseBatchBuffer is called when the guard goes out of scope.
 */
class BatchBufferGuard {
private:
    char* buffer_ = nullptr;
    const MessageBatcher* batcher_ = nullptr; // Pointer to the batcher that owns the buffer

public:
    /**
     * @brief Construct a BatchBufferGuard.
     * @param buffer The raw buffer pointer obtained from GetBatchBuffer.
     * @param batcher The MessageBatcher instance that provided the buffer.
     */
    BatchBufferGuard(char* buffer, const MessageBatcher& batcher)
        : buffer_(buffer), batcher_(buffer ? &batcher : nullptr) {} // Only store batcher if buffer is valid

    /**
     * @brief Destructor. Releases the buffer using the associated batcher.
     */
    ~BatchBufferGuard() {
        if (buffer_ && batcher_) {
            batcher_->ReleaseBatchBuffer(buffer_);
        }
    }

    // Prevent copying
    BatchBufferGuard(const BatchBufferGuard&) = delete;
    BatchBufferGuard& operator=(const BatchBufferGuard&) = delete;

    // Allow moving
    BatchBufferGuard(BatchBufferGuard&& other) noexcept
        : buffer_(other.buffer_), batcher_(other.batcher_) {
        other.buffer_ = nullptr; // Null out the moved-from object
        other.batcher_ = nullptr;
    }

    BatchBufferGuard& operator=(BatchBufferGuard&& other) noexcept {
        if (this != &other) {
            // Release the current buffer if holding one
            if (buffer_ && batcher_) {
                batcher_->ReleaseBatchBuffer(buffer_);
            }
            // Transfer ownership
            buffer_ = other.buffer_;
            batcher_ = other.batcher_;
            // Null out the moved-from object
            other.buffer_ = nullptr;
            other.batcher_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Get the underlying raw buffer pointer.
     * @return char* The raw buffer pointer, or nullptr if empty or released.
     */
    char* get() const { return buffer_; }

    /**
     * @brief Check if the guard holds a valid buffer.
     * @return true if the buffer pointer is not null, false otherwise.
     */
    explicit operator bool() const { return buffer_ != nullptr; }

    /**
     * @brief Manually release the buffer before the guard goes out of scope.
     */
    void release() {
         if (buffer_ && batcher_) {
            batcher_->ReleaseBatchBuffer(buffer_);
        }
        buffer_ = nullptr;
        batcher_ = nullptr;
    }

    /**
     * @brief Detach the buffer from RAII management.
     *
     * The caller becomes responsible for calling ReleaseBatchBuffer manually.
     * Use with caution.
     * @return char* The raw buffer pointer.
     */
    char* detach() {
        char* temp = buffer_;
        buffer_ = nullptr;
        batcher_ = nullptr; // Prevent destructor from releasing
        return temp;
    }
};

} // namespace Syslog_agent 
#pragma once

#include "MessageQueue.h"
#include "PoolResourceMonitorWrapper.h"
#include <array>
#include <cstring>

namespace Syslog_agent {

// Maximum length for guard names - keep short to avoid stack bloat
#define MAX_GUARD_NAME_LENGTH 32

/**
 * RAII wrapper for MessageBuffer objects from a MessageQueue.
 * Automatically returns the buffer to the pool when going out of scope.
 * Uses stack-based fixed-size array for names to avoid heap allocations.
 */
class MessageBufferGuard {
private:
    MessageQueue::MessageBuffer* buffer_;
    BitmappedObjectPool<MessageQueue::MessageBuffer>& pool_;
    bool released_;
    char name_[MAX_GUARD_NAME_LENGTH];

public:
    explicit MessageBufferGuard(const char* name, BitmappedObjectPool<MessageQueue::MessageBuffer>& pool)
        : buffer_(nullptr), pool_(pool), released_(false) {
        // Copy name to fixed buffer with truncation if needed
        if (name) {
            strncpy_s(name_, MAX_GUARD_NAME_LENGTH, name, MAX_GUARD_NAME_LENGTH - 1);
            name_[MAX_GUARD_NAME_LENGTH - 1] = '\0';  // Ensure null termination
        } else {
            strcpy_s(name_, MAX_GUARD_NAME_LENGTH, "unnamed");
        }
        
        buffer_ = pool_.getAndMarkNextUnused();
        if (buffer_) {
            buffer_->next = nullptr;
            // Record allocation in the monitor if available
            PoolResourceMonitorWrapper::recordAllocation(name_);
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
        : buffer_(other.buffer_), pool_(other.pool_), released_(other.released_) {
        strcpy_s(name_, MAX_GUARD_NAME_LENGTH, other.name_);
        other.buffer_ = nullptr;
        other.released_ = true;
    }

    MessageBufferGuard& operator=(MessageBufferGuard&& other) noexcept {
        if (this != &other && &pool_ == &other.pool_) {
            release();
            buffer_ = other.buffer_;
            released_ = other.released_;
            strcpy_s(name_, MAX_GUARD_NAME_LENGTH, other.name_);
            other.buffer_ = nullptr;
            other.released_ = true;
        }
        return *this;
    }

    void release() {
        if (!released_ && buffer_) {
            pool_.markAsUnused(buffer_);
            PoolResourceMonitorWrapper::recordRelease(name_);
            released_ = true;
            buffer_ = nullptr;
        }
    }

    MessageQueue::MessageBuffer* get() const { return buffer_; }
    MessageQueue::MessageBuffer* operator->() const { return buffer_; }
    MessageQueue::MessageBuffer& operator*() const { return *buffer_; }
    operator bool() const { return buffer_ != nullptr; }

    // Detach the buffer without releasing it
    MessageQueue::MessageBuffer* detach() {
        released_ = true;
        auto temp = buffer_;
        buffer_ = nullptr;
        return temp;
    }
};

/**
 * RAII wrapper for Message objects from a MessageQueue.
 * Automatically returns the message to the pool when going out of scope.
 * Uses stack-based fixed-size array for names to avoid heap allocations.
 */
class MessageGuard {
private:
    MessageQueue::Message* msg_;
    BitmappedObjectPool<MessageQueue::Message>& pool_;
    bool released_;
    char name_[MAX_GUARD_NAME_LENGTH];

public:
    explicit MessageGuard(const char* name, BitmappedObjectPool<MessageQueue::Message>& pool)
        : msg_(nullptr), pool_(pool), released_(false) {
        // Copy name to fixed buffer with truncation if needed
        if (name) {
            strncpy_s(name_, MAX_GUARD_NAME_LENGTH, name, MAX_GUARD_NAME_LENGTH - 1);
            name_[MAX_GUARD_NAME_LENGTH - 1] = '\0';  // Ensure null termination
        } else {
            strcpy_s(name_, MAX_GUARD_NAME_LENGTH, "unnamed_msg");
        }
        
        msg_ = pool_.getAndMarkNextUnused();
        if (msg_) {
            // Initialize message to a clean state
            msg_->next = nullptr;
            msg_->timestamp = 0;
            msg_->data_length = 0;
            msg_->buffer_count = 0;
            msg_->message_buffers = nullptr;
            // Record allocation in the monitor if available
            PoolResourceMonitorWrapper::recordAllocation(name_);
        }
    }

    ~MessageGuard() {
        release();
    }

    // Prevent copying
    MessageGuard(const MessageGuard&) = delete;
    MessageGuard& operator=(const MessageGuard&) = delete;

    // Allow moving
    MessageGuard(MessageGuard&& other) noexcept
        : msg_(other.msg_), pool_(other.pool_), released_(other.released_) {
        strcpy_s(name_, MAX_GUARD_NAME_LENGTH, other.name_);
        other.msg_ = nullptr;
        other.released_ = true;
    }

    MessageGuard& operator=(MessageGuard&& other) noexcept {
        if (this != &other && &pool_ == &other.pool_) {
            release();
            msg_ = other.msg_;
            released_ = other.released_;
            strcpy_s(name_, MAX_GUARD_NAME_LENGTH, other.name_);
            other.msg_ = nullptr;
            other.released_ = true;
        }
        return *this;
    }

    void release() {
        if (!released_ && msg_) {
            pool_.markAsUnused(msg_);
            PoolResourceMonitorWrapper::recordRelease(name_);
            released_ = true;
            msg_ = nullptr;
        }
    }

    MessageQueue::Message* get() const { return msg_; }
    MessageQueue::Message* operator->() const { return msg_; }
    MessageQueue::Message& operator*() const { return *msg_; }
    operator bool() const { return msg_ != nullptr; }

    // Detach the message without releasing it
    MessageQueue::Message* detach() {
        released_ = true;
        auto temp = msg_;
        msg_ = nullptr;
        return temp;
    }
};

} // namespace Syslog_agent 
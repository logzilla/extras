#pragma once

#include "MessageBatcher.h"
#include <cstdint>
#include <memory>
#include <mutex>

namespace Syslog_agent {

class HTTPMessageBatcher : public MessageBatcher {
public:
                                                                        // DEBUGGING
    static constexpr std::uint32_t MAX_BATCH_SIZE_BYTES = /* 2 * 1024 * 1024 */ 512 * 1024;   // Maximum size of a message batch in bytes
    static constexpr std::uint32_t BATCH_BUFFER_CHUNK_SIZE = 16;            // Number of buffers to allocate at once
    static constexpr std::uint32_t BATCH_BUFFER_PERCENT_SLACK = 25;         // Percentage of extra buffers to maintain

private:
    // Store string literals as member variables to return char*
    static constexpr const char* HEADER = "{ \"events\": [ ";
    static constexpr const char* SEPARATOR = ", ";
    static constexpr const char* TRAILER = " ] }";

public:
    HTTPMessageBatcher(std::uint32_t max_batch_size, std::uint32_t max_batch_age) 
        : MessageBatcher(max_batch_size, max_batch_age) {}
    
    ~HTTPMessageBatcher() = default;

protected:
    std::uint32_t GetMaxBatchSizeBytes_() const override { return MAX_BATCH_SIZE_BYTES; }

    void GetMessageHeader_(char* dest, size_t max_size, size_t& size_out) const override {
        size_t len = strlen(HEADER);
        if (len < max_size) {
            memcpy(dest, HEADER, len);
            size_out = len;
        } else {
            size_out = 0;
        }
    }

    void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const override {
        size_t len = strlen(SEPARATOR);
        if (len < max_size) {
            memcpy(dest, SEPARATOR, len);
            size_out = len;
        } else {
            size_out = 0;
        }
    }

    void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const override {
        size_t len = strlen(TRAILER);
        if (len < max_size) {
            memcpy(dest, TRAILER, len);
            size_out = len;
        } else {
            size_out = 0;
        }
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        auto logger = LOG_THIS;
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (!batch_buffers_) {
            batch_buffers_ = std::make_unique<BitmappedObjectPool<char[MAX_BATCH_SIZE_BYTES]>>(
                BATCH_BUFFER_CHUNK_SIZE, BATCH_BUFFER_PERCENT_SLACK);
            if (!batch_buffers_) {
                logger->fatal("Failed to allocate message buffer pool\n");
                return nullptr;
            }
        }
        auto* buffer = batch_buffers_->getAndMarkNextUnused();
        if (!buffer) {
            if (debug_identifier) {
                logger->recoverable_error("Failed to allocate message buffer for %s\n", debug_identifier);
            }
            else {
                logger->recoverable_error("Failed to allocate message buffer\n");
            }
            return nullptr;
        }
        return static_cast<char*>(*buffer);
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        if (!buffer || !batch_buffers_) {
            return false;
        }
        return batch_buffers_->markAsUnused(reinterpret_cast<char(*)[MAX_BATCH_SIZE_BYTES]>(buffer));
    }

    std::uint32_t GetMaxBatchSizeBytes() const override { return MAX_BATCH_SIZE_BYTES; }

    // Add implementation for the missing pure virtual function
    void validateBatchBuffer(char* buffer, size_t buffer_size) const override {
        // No-op implementation for now, add validation if needed later.
        (void)buffer; // Mark as used to prevent unused parameter warnings
        (void)buffer_size;
    }

protected:
    mutable std::unique_ptr<BitmappedObjectPool<char[MAX_BATCH_SIZE_BYTES]>> batch_buffers_;
    mutable std::mutex buffer_mutex_;
};

} // namespace Syslog_agent
#pragma once

#include "MessageBatcher.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <cstring>

namespace Syslog_agent {

class JSONMessageBatcher : public MessageBatcher {
public:
    static constexpr std::uint32_t MAX_BATCH_SIZE_BYTES = 65536;         // Maximum size of a message batch in bytes
    static constexpr std::uint32_t BATCH_BUFFER_CHUNK_SIZE = 12;        // Number of buffers to allocate
    static constexpr std::uint32_t BATCH_BUFFER_PERCENT_SLACK = 25;     // Unused buffer percentage
    static constexpr std::uint32_t BATCH_SIZE_THRESHOLD = 100;       // Messages per batch

private:
    // Store string literals as member variables to return char*
    static constexpr const char* HEADER = "";
    static constexpr const char* SEPARATOR = "\n";
    static constexpr const char* TRAILER = "";
    
    char header_[sizeof(HEADER)];      // Includes null terminator
    char separator_[sizeof(SEPARATOR)]; // Includes null terminator
    char trailer_[sizeof(TRAILER)];    // Includes null terminator

public:
    JSONMessageBatcher(std::uint32_t max_batch_size, std::uint32_t max_batch_age) 
        : MessageBatcher(max_batch_size, max_batch_age) {
        std::strcpy(header_, HEADER);
        std::strcpy(separator_, SEPARATOR);
        std::strcpy(trailer_, TRAILER);
    }
    
    ~JSONMessageBatcher() = default;

protected:
    std::uint32_t GetMaxBatchSizeBytes_() const override { return MAX_BATCH_SIZE_BYTES; }
    
    void GetMessageHeader_(char* dest, size_t max_size, size_t& size_out) const override {
        size_t len = std::strlen(header_);
        if (len < max_size) {
            std::memcpy(dest, header_, len);
            size_out = len;
        }
        else {
            size_out = 0;
        }
    }
    
    void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const override {
        size_t len = std::strlen(separator_);
        if (len < max_size) {
            std::memcpy(dest, separator_, len);
            size_out = len;
        }
        else {
            size_out = 0;
        }
    }
    
    void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const override {
        size_t len = std::strlen(trailer_);
        if (len < max_size) {
            std::memcpy(dest, trailer_, len);
            size_out = len;
        }
        else {
            dest[0] = '\0';
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

    std::uint32_t GetMaxBatchSizeBytes() const override { return MAX_BATCH_SIZE_BYTES; }

    bool ReleaseBatchBuffer(char* buffer) const override {
        if (!buffer || !batch_buffers_) {
            return false;
        }
        return batch_buffers_->markAsUnused(reinterpret_cast<char(*)[MAX_BATCH_SIZE_BYTES]>(buffer));
    }

    void validateBatchBuffer(char* buffer, size_t buffer_size) const override {
        // Currently no specific validation needed for JSON batch buffer beyond standard checks.
        // This function is implemented to make the class concrete.
        (void)buffer; // Suppress unused parameter warning
        (void)buffer_size; // Suppress unused parameter warning
    }

protected:
    mutable std::unique_ptr<BitmappedObjectPool<char[MAX_BATCH_SIZE_BYTES]>> batch_buffers_;
    mutable std::mutex buffer_mutex_;
};

} // namespace Syslog_agent
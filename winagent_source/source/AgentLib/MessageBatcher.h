#pragma once

#include <memory>
#include "MessageQueue.h"
#include "framework.h"

using std::shared_ptr;

namespace Syslog_agent {

    class AGENTLIB_API MessageBatcher
    {
    public:
        struct AGENTLIB_API BatchResult {
            enum Status {
                Success = 0,
                BufferTooSmall = -1,
                NoMessages = -2,
                InvalidBuffer = -3,
                MessageTooLarge = -4
            };
            Status status;
            std::uint32_t messages_batched;  // Number of messages successfully added to batch
            size_t bytes_written;       // Number of bytes written to buffer

            BatchResult(Status s = Status::InvalidBuffer, std::uint32_t msgs = 0, size_t bytes = 0)
                : status(s), messages_batched(msgs), bytes_written(bytes) {
            }
        };

        MessageBatcher(std::uint32_t max_batch_size, std::uint32_t max_batch_age);
        ~MessageBatcher();

        // Returns both status and number of messages batched
        BatchResult BatchEvents(shared_ptr<MessageQueue> message_queue, char* batch_buffer, size_t buffer_size) const;

        virtual char* GetBatchBuffer(const char* debug_identifier = nullptr) const = 0;
        virtual bool ReleaseBatchBuffer(char* buffer) const = 0;
        virtual std::uint32_t GetMaxBatchSizeBytes() const = 0;
        virtual void validateBatchBuffer(char* buffer, size_t buffer_size) const = 0;
    
    protected:
        std::uint32_t max_batch_size_;
        std::uint32_t max_batch_age_;

        virtual std::uint32_t GetMaxBatchSizeBytes_() const = 0;

        // Modified virtual methods to include buffer safety
        virtual void GetMessageHeader_(char* dest, size_t max_size, size_t& size_out) const = 0;
        virtual void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const = 0;
        virtual void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const = 0;

    private:
        BatchResult BatchEventsInternal(shared_ptr<MessageQueue> message_queue, char* batch_buffer, size_t buffer_size) const;
    };
};

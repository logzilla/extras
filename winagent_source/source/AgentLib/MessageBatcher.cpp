#include "pch.h"
#include "MessageBatcher.h"
#include "MessageQueue.h"
#include "../Infrastructure/Logger.h"
#include <typeinfo>
#include <algorithm> // for std::min
#include <stdexcept>
#include "BatchBufferGuard.h" // Include the new guard

namespace Syslog_agent {

    MessageBatcher::MessageBatcher(std::uint32_t max_batch_size, std::uint32_t max_batch_age)
        : max_batch_size_(max_batch_size), max_batch_age_(max_batch_age) {
    }

    MessageBatcher::~MessageBatcher() {
    }

    MessageBatcher::BatchResult MessageBatcher::BatchEvents(
        shared_ptr<MessageQueue> msg_queue,
        char* batch_buffer,
        size_t buffer_size) const {
        if (!msg_queue || !batch_buffer || buffer_size == 0) {
            return BatchResult(BatchResult::Status::InvalidBuffer);
        }

        return BatchEventsInternal(msg_queue, batch_buffer, buffer_size);
    }

    MessageBatcher::BatchResult MessageBatcher::BatchEventsInternal(
        shared_ptr<MessageQueue> message_queue,
        char* batch_buffer,
        size_t buffer_size) const {
        auto logger = LOG_THIS;
        size_t queue_length = message_queue->length();
        logger->debug3("MessageBatcher::BatchEventsInternal() Initial queue length: %d\n", queue_length);

        if (queue_length == 0) {
            return BatchResult(BatchResult::Status::NoMessages);
        }

        try {
            // Get sizes for header, separator, and trailer
            size_t header_size = 0;
            size_t separator_size = 0;
            size_t trailer_size = 0;
            char temp_buffer[1024];  // Temporary buffer for size calculations

            // Get header
            GetMessageHeader_(batch_buffer, buffer_size, header_size);
            // Check for header failure - could be either:
            // 1. The header size is 0 when it's not supposed to be (failure in header generation)
            // 2. The header is too large for the buffer
            if (header_size > buffer_size) {
                logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Header too large\n");
                return BatchResult(BatchResult::Status::BufferTooSmall, 0, 0); // Ensure no messages are reported as batched
            }

            // Get separator and trailer sizes
            GetMessageSeparator_(temp_buffer, sizeof(temp_buffer), separator_size);
            GetMessageTrailer_(temp_buffer, sizeof(temp_buffer), trailer_size);

            logger->debug2("MessageBatcher::BatchEventsInternal()> Sizes - Header: %zu, Separator: %zu, Trailer: %zu\n",
                header_size, separator_size, trailer_size);

            // Check if buffer is large enough for minimal batch (header + smallest possible message + trailer)
            if (buffer_size < (header_size + trailer_size + 1)) {
                logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Buffer size %zu too small for minimal batch (need %zu)\n",
                    buffer_size, header_size + trailer_size + 1);
                return BatchResult(BatchResult::Status::BufferTooSmall, 0, 0);
            }

            std::uint32_t max_batch = (std::min)(max_batch_size_, static_cast<std::uint32_t>(queue_length));
            logger->debug3("MessageBatcher::BatchEventsInternal()> Will process max %d messages\n", max_batch);

            // Header is already written, start after it
            size_t current_pos = header_size;
            std::uint32_t messages_batched = 0;
            bool found_valid_message = false;  // Track if we found any valid messages to process

            // Use BatchBufferGuard for automatic release
            BatchBufferGuard peek_buffer_guard(this->GetBatchBuffer("peek_buffer"), *this);
            char* peek_buffer = peek_buffer_guard.get();
            if (!peek_buffer) {
                logger->fatal("MessageBatcher::BatchEventsInternal()> Failed to get peek buffer\n");
                return BatchResult(BatchResult::Status::InvalidBuffer);
            }

            // Process messages
            for (const auto& msg : message_queue->traverseQueue()) {
                if (!msg) continue;

                // Get message length
                int msg_len = message_queue->peek(msg, peek_buffer, GetMaxBatchSizeBytes());

                if (msg_len == 0) {
                    logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Message with zero length, discarding\n");
                    continue;
                }

                if (msg_len < 0 || static_cast<size_t>(msg_len) > GetMaxBatchSizeBytes_()) {
                    logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Message too large\n");
                    found_valid_message = true;  // We found a message, even though it was too large
                    continue;  // Skip this message but continue processing
                }

                // Calculate space needed for this message
                size_t space_needed = msg_len;
                if (messages_batched > 0) {
                    space_needed += separator_size;  // Need separator if not first message
                }
                space_needed += trailer_size + 16;  // MUST reserve space for trailer with safety margin

                if (messages_batched == 0 && space_needed + header_size > buffer_size) {
                    // First message won't fit even with just header and trailer
                    logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Buffer too small for even one message (needs %zu, have %zu)\n",
                        space_needed + header_size, buffer_size);
                    // peek_buffer_guard will release the buffer automatically
                    return BatchResult(BatchResult::Status::BufferTooSmall, 0, 0);
                }
                else if (current_pos + space_needed > buffer_size) {
                    // Not enough space for message + separator (if needed) + trailer
                    logger->debug2("MessageBatcher::BatchEventsInternal()> Not enough space for next message (needs %zu, have %zu), ending batch\n",
                        space_needed, buffer_size - current_pos);
                    break;
                }

                found_valid_message = true;  // We found at least one valid message

                // Add separator if this isn't the first message
                if (messages_batched > 0) {
                    GetMessageSeparator_(batch_buffer + current_pos, buffer_size - current_pos, separator_size);
                    if (separator_size == 0) {
                        logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Failed to add separator\n");
                        break;
                    }
                    current_pos += separator_size;
                }

                // Copy message content
                memcpy(batch_buffer + current_pos, peek_buffer, msg_len);
                current_pos += msg_len;
                messages_batched++;

                // Stop if we've reached max batch size
                if (messages_batched >= max_batch) {
                    logger->debug3("MessageBatcher::BatchEventsInternal()> Reached max batch size of %d messages\n", max_batch);
                    break;
                }
            }

            // No need for manual release, BatchBufferGuard handles it
            // this->ReleaseBatchBuffer(peek_buffer);

            // If we haven't batched any messages but found valid ones to process,
            // return Success with 0 messages (they were all too large)
            if (messages_batched == 0) {
                if (!found_valid_message) {
                    logger->debug("MessageBatcher::BatchEventsInternal()> No messages were found\n");
                    return BatchResult(BatchResult::Status::NoMessages);
                }
                logger->debug("MessageBatcher::BatchEventsInternal()> Found messages but none could be batched\n");
                return BatchResult(BatchResult::Status::Success, 0, 0);
            }

            // Add trailer if we batched any messages
            if (messages_batched > 0) {
                // Check if we have enough space left for the trailer
                if (buffer_size - current_pos < trailer_size) {
                    logger->warning("MessageBatcher::BatchEventsInternal()> Not enough space left for trailer, need %zu, have %zu\n", 
                        trailer_size, buffer_size - current_pos);
                    return BatchResult(BatchResult::Status::BufferTooSmall, 0, 0);
                }
                
                size_t actual_trailer_size = 0;
                GetMessageTrailer_(batch_buffer + current_pos, buffer_size - current_pos, actual_trailer_size);
                current_pos += actual_trailer_size;
            }

            // Null terminate the batch but don't include it in bytes_written
            if (current_pos < buffer_size) {
                batch_buffer[current_pos] = '\0';
            }

            logger->debug3("MessageBatcher::BatchEventsInternal()> Messages batched: %d\n", messages_batched);
            return BatchResult(BatchResult::Status::Success, messages_batched, current_pos);
        }
        catch (const std::exception& e) {
            logger->recoverable_error("MessageBatcher::BatchEventsInternal()> Exception: %s\n", e.what());
            return BatchResult(BatchResult::Status::InvalidBuffer);
        }
    }
}

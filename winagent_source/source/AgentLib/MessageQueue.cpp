#include "pch.h"
#include "MessageQueue.h"
#include "MessageBufferGuard.h"
#include "../Infrastructure/PooledObject.h"
#include <algorithm>
#include <chrono>
#include <cstring>

namespace Syslog_agent {

MessageQueue::MessageQueue(uint32_t message_queue_size, uint32_t message_buffers_chunk_size)
    : message_queue_chunk_size_(message_buffers_chunk_size),
      length_(0),
      items_sem_(0)
{
    messages_pool_ = std::make_unique<BitmappedObjectPool<Message>>(message_queue_size, MESSAGE_QUEUE_SLACK_PERCENT);
    message_buffers_pool_ = std::make_unique<BitmappedObjectPool<MessageBuffer>>(message_buffers_chunk_size, MESSAGE_QUEUE_SLACK_PERCENT);
}

MessageQueue::~MessageQueue() {
    // Clean up any remaining messages
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (first_message_) {
        removeFrontInternal();
    }
}

void MessageQueue::releaseMessageBuffers(Message& msg) {
    auto logger = LOG_THIS;
    MessageBuffer* current = msg.message_buffers;
    uint32_t released_count = 0;
    uint32_t expected_count = msg.buffer_count;
    
    // Detect potential corruption - buffer count should be consistent with chain length
    if (expected_count == 0 && current != nullptr) {
        logger->warning("MessageQueue::releaseMessageBuffers() : Buffer count is 0 but message has buffers\n");
    }
    
    while (current) {
        // Verify that this buffer belongs to our pool before releasing
        if (!message_buffers_pool_->belongs(current)) {
            logger->fatal("MessageQueue::releaseMessageBuffers() : Detected invalid buffer pointer not from our pool\n");
            // Break the chain to prevent further invalid access
            msg.message_buffers = nullptr;
            break;
        }
        
        MessageBuffer* next = current->next;
        current->next = nullptr;
        
        // Create a temporary RAII wrapper that will release the buffer when it goes out of scope
        PooledObject<MessageBuffer> bufferGuard(current, *message_buffers_pool_);
        // No need to call markAsUnused explicitly - the destructor will handle it
        
        released_count++;
        
        // Guard against circular references or corrupt linked lists
        if (released_count > MAX_BUFFERS_PER_MESSAGE) {
            logger->fatal("MessageQueue::releaseMessageBuffers() : Possible circular reference detected. " 
                          "Released %u buffers when expecting %u\n", released_count, expected_count);
            msg.message_buffers = nullptr;
            break;
        }
        
        current = next;
    }
    
    // If we released fewer buffers than expected, log it as potential leak
    if (expected_count > 0 && released_count < expected_count) {
        logger->warning("MessageQueue::releaseMessageBuffers() : Expected to release %u buffers, but only found %u\n", 
                       expected_count, released_count);
    }
    // If we released more buffers than expected, log it as potential memory corruption
    else if (released_count > expected_count && expected_count > 0) {
        logger->warning("MessageQueue::releaseMessageBuffers() : Released %u buffers when expecting only %u\n", 
                       released_count, expected_count);
    }
    
    // Zero out metrics even if we encountered problems
    msg.buffer_count = 0;
    msg.message_buffers = nullptr;
}

MessageQueue::Message* MessageQueue::createMessage(const char* message_content, const uint32_t message_len, uint64_t timestamp) {
    auto logger = LOG_THIS;
    
    // Validate input parameters
    if (!message_content || message_len == 0) {
        logger->recoverable_error("MessageQueue::createMessage() : invalid parameters\n");
        return nullptr;
    }
    
    // Use RAII wrapper for Message allocation
    MessageGuard msgGuard("createMessage", *messages_pool_);
    if (!msgGuard) {
        logger->recoverable_error("MessageQueue::createMessage() : failed to allocate message\n");
        return nullptr;
    }

    // Initialize message before allocating buffers to ensure clean state on error
    Message* msg = msgGuard.get();
    msg->next = nullptr;
    msg->timestamp = timestamp;
    msg->data_length = message_len;
    msg->buffer_count = 0;
    msg->message_buffers = nullptr;

    // Track progress for debugging/logging
    #ifdef _DEBUG
    uint32_t allocated_buffers = 0;
    #endif
    
    // Use safer approach: first allocate all needed buffers without detaching them,
    // then validate the entire chain, and only then commit the buffers to the message
    try {
        uint32_t remaining = message_len;
        const char* ptr = message_content;
        
        // Calculate how many buffers we'll need
        uint32_t buffers_needed = (message_len + MESSAGE_BUFFER_SIZE - 1) / MESSAGE_BUFFER_SIZE;
        if (buffers_needed > MAX_BUFFERS_PER_MESSAGE) {
            logger->recoverable_error("MessageQueue::createMessage() : message requires %u buffers which exceeds MAX_BUFFERS_PER_MESSAGE (%u)\n", 
                                   buffers_needed, MAX_BUFFERS_PER_MESSAGE);
            throw std::runtime_error("Too many buffers required");
        }
        
        // Avoid heap allocations - only process one buffer at a time, but validate in two phases:
        // Phase 1: Validate we can allocate all buffers
        // We'll use a stack-allocated array to keep track of allocated buffers
        // This is safe because MAX_BUFFERS_PER_MESSAGE provides an upper bound
        MessageBuffer* buffer_ptrs[MAX_BUFFERS_PER_MESSAGE];
        uint32_t allocated_count = 0;
        
        // Try to allocate all buffers (without copying data yet)
        for (uint32_t i = 0; i < buffers_needed; i++) {
            MessageBufferGuard bufferGuard("createMessage_buffer", *message_buffers_pool_);
            if (!bufferGuard) {
                logger->recoverable_error("MessageQueue::createMessage() : failed to allocate buffer %u of %u\n", 
                                       i + 1, buffers_needed);
                // Clean up any buffers we've already attached to the message
                releaseMessageBuffers(*msg);
                throw std::runtime_error("Buffer allocation failed");
            }
            
            // Get the raw buffer pointer and detach from the guard
            buffer_ptrs[allocated_count++] = bufferGuard.detach();
            
            #ifdef _DEBUG
            // Track for debug logging
            if (allocated_count % 10 == 0 && allocated_count < buffers_needed) {
                logger->debug2("MessageQueue::createMessage() : %u of %u buffers allocated for message of length %u\n", 
                              allocated_count, buffers_needed, message_len);
            }
            #endif
        }
        
        // Phase 2: All buffers are allocated, now fill them with data and link them
        MessageBuffer* lastBuffer = nullptr;
        remaining = message_len;
        ptr = message_content;
        
        for (uint32_t i = 0; i < allocated_count; i++) {
            MessageBuffer* buffer = buffer_ptrs[i];
            uint32_t toCopy = (std::min)(remaining, static_cast<uint32_t>(MESSAGE_BUFFER_SIZE));
            
            // Copy data into buffer
            memcpy(buffer->buffer, ptr, toCopy);
            buffer->next = nullptr;
            
            // Link buffers together
            if (!msg->message_buffers) {
                msg->message_buffers = buffer;
            } else if (lastBuffer) {
                lastBuffer->next = buffer;
            }
            lastBuffer = buffer;
            
            remaining -= toCopy;
            ptr += toCopy;
            msg->buffer_count++;
        }
        
        #ifdef _DEBUG
        // Log completion stats for large messages (only in debug builds)
        if (msg->buffer_count > 1) {
            logger->debug3("MessageQueue::createMessage() : Successfully created message with %u buffers for %u bytes\r\n", 
                        msg->buffer_count, message_len);
        }
        #endif
        
        // Detach the message so it's not automatically released when msgGuard goes out of scope
        return msgGuard.detach();
    }
    catch (const std::exception& e) {
        #ifdef _DEBUG
        logger->recoverable_error("MessageQueue::createMessage() : Exception during message creation: %s\n", e.what());
        #endif
        
        // Clean up on any error - already attached buffers will be cleaned up by releaseMessageBuffers
        releaseMessageBuffers(*msg);
        // No need to call markAsUnused for the message - the msgGuard will handle it when it goes out of scope
        return nullptr;
    }
    catch (...) {
        #ifdef _DEBUG
        logger->recoverable_error("MessageQueue::createMessage() : Unknown exception during message creation\n");
        #endif
        
        // Clean up on any error
        releaseMessageBuffers(*msg);
        // No need to call markAsUnused - the msgGuard will handle it when it goes out of scope
        return nullptr;
    }
}

bool MessageQueue::enqueue(const char* message_content, const uint32_t message_len) {
    auto logger = LOG_THIS;
    // Reject messages whose size is zero or exceeds the total capacity of the buffer chain.
    // Capacity  = MESSAGE_BUFFER_SIZE * MAX_BUFFERS_PER_MESSAGE; messages equal to or larger than
    // that value are disallowed to maintain a strict upper-bound and keep logic simple.
    if (!message_content || message_len == 0 || message_len >= MESSAGE_BUFFER_SIZE * MAX_BUFFERS_PER_MESSAGE) {
        logger->recoverable_error("MessageQueue::enqueue() : invalid parameters\n");
        return false;
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);

    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    Message* msg = createMessage(message_content, message_len, timestamp);
    if (!msg) {
        return false;
    }

    if (enqueue_hook_ && !enqueue_hook_(length_,msg, false)) {
            return false; // Handler cancelled the enqueue
    }

    if (!first_message_) {
        first_message_ = msg;
        last_message_ = msg;
    } else {
        last_message_->next = msg;
        last_message_ = msg;
    }
    length_++;
    
    // Post-enqueue handler
    if (enqueue_hook_) {
        enqueue_hook_(length_, msg, true);
    }

    items_sem_.release();
    items_cv_.notify_one();  // Only need to notify one waiter
    return true;
}

int MessageQueue::peek(Message* msg, char* message_content, const uint32_t max_len) const {
    auto logger = LOG_THIS;
    if (!message_content || max_len == 0) {
        logger->recoverable_error("MessageQueue::peek() : invalid parameters\n");
        return -1;
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (msg == nullptr) {
        msg = first_message_;
        if (!msg) {
            logger->debug("MessageQueue::peek() : queue is empty\n");
            return -1;
        }
    }

    // Validate the message belongs to this queue
    if (!messages_pool_->belongs(msg)) {
        logger->recoverable_error("MessageQueue::peek() : invalid message pointer\n");
        return -1;
    }

    if (msg->data_length > max_len) {
        logger->recoverable_error("MessageQueue::peek() : message length %u exceeds buffer size %u\n", msg->data_length, max_len);
        return -1;
    }

    int copied = 0;
    for (MessageBuffer* buffer = msg->message_buffers; buffer != nullptr; buffer = buffer->next) {
        uint32_t toCopy = (std::min)(msg->data_length - static_cast<uint32_t>(copied), static_cast<uint32_t>(MESSAGE_BUFFER_SIZE));
        memcpy(message_content + copied, buffer->buffer, toCopy);
        copied += toCopy;
    }
    // Null terminate the output string
    if (copied < max_len) {
        message_content[copied] = '\0';
    }

    // logger->debug3("MessageQueue::peek() Successfully peeked message with length %d\n", msg->data_length);
    return msg->data_length;
}

void MessageQueue::removeFrontInternal() {
    // If there is no message to remove, just return.
    if (!first_message_) {
        return;
    }

    Message* msg = first_message_;
    first_message_ = first_message_->next;
    if (!first_message_) {
        last_message_ = nullptr;
    }
    length_--;

    // Set next to nullptr before releasing to prevent dangling pointer access.
    msg->next = nullptr;

    // Release all associated buffers.
    releaseMessageBuffers(*msg);

    // Create a temporary RAII wrapper that will release the message when it goes out of scope
    PooledObject<Message> msgGuard(msg, *messages_pool_);
    
    // The msgGuard destructor will handle releasing the message
}

int MessageQueue::dequeue(char* message_content, const uint32_t max_len) {
    auto logger = LOG_THIS;
    if (!message_content || max_len == 0) {
        logger->recoverable_error("MessageQueue::dequeue() : invalid parameters\n");
        return -1;
    }

    std::unique_lock<std::mutex> lock(queue_mutex_);

    // Immediately fail if the queue is shutting down.
    if (is_shutting_down_.load()) {
        return -1;
    }

    if (!first_message_) {
        logger->debug("MessageQueue::dequeue() : queue is empty\n");
        
        // Occasionally check memory health when queue is empty
        static int empty_count = 0;
        if (++empty_count >= 100) { // Check approximately every 100 empty dequeues
            empty_count = 0;
            PoolResourceMonitorWrapper::checkMemoryHealth();
        }
        
        return -1;
    }

    if (first_message_->data_length > max_len) {
        logger->recoverable_error("MessageQueue::dequeue() : message length %u exceeds buffer size %u\n", 
            first_message_->data_length, max_len);
        return -1;
    }

    int copied = 0;
    for (MessageBuffer* buffer = first_message_->message_buffers; buffer != nullptr; buffer = buffer->next) {
        uint32_t toCopy = (std::min)(first_message_->data_length - static_cast<uint32_t>(copied),
                                     static_cast<uint32_t>(MESSAGE_BUFFER_SIZE));
        memcpy(message_content + copied, buffer->buffer, toCopy);
        copied += toCopy;
    }
    // Null terminate the output string if space allows.
    if (copied < static_cast<int>(max_len)) {
        message_content[copied] = '\0';
    }

    int length = first_message_->data_length;
    removeFrontInternal();

    logger->debug2("MessageQueue::dequeue() Successfully dequeued message with length %d\n", length);
    return length;
}

bool MessageQueue::removeFront() {
    auto logger = LOG_THIS;
    items_sem_.acquire();
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (!first_message_) {
        logger->debug("MessageQueue::removeFront() : queue is empty\n");
        return false;
    }

    removeFrontInternal();
    return true;
}

std::experimental::generator<MessageQueue::Message*> MessageQueue::traverseQueue(Message* first) const {
    std::vector<Message*> messages;
    {
        // Lock only long enough to copy the pointers
        std::lock_guard<std::mutex> lock(queue_mutex_);
        Message* current = first ? first : first_message_;
        while (current) {
            // (Optional: you can add a check here if needed)
            messages.push_back(current);
            current = current->next;
        }
    }
    // Now yield messages without holding the lock.
    for (auto msg : messages) {
        co_yield msg;
    }
}

void MessageQueue::beginShutdown() {
    auto logger = LOG_THIS;
    logger->debug("MessageQueue::beginShutdown() : checking memory health before shutdown");
    PoolResourceMonitorWrapper::checkMemoryHealth();
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    is_shutting_down_.store(true);
    // Flush all queued messages.
    while (first_message_) {
        removeFrontInternal();
    }
    // Notify any waiting threads.
    items_cv_.notify_all();
    
    logger->debug("MessageQueue::beginShutdown() : checking memory health after queue flush");
    // Lock is released here, so we can call this outside the lock
    PoolResourceMonitorWrapper::checkMemoryHealth();
}


}
#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <experimental/generator>
#include <functional>
#include "../Infrastructure/BitmappedObjectPool.h"
#include "../Infrastructure/Logger.h"
#include "framework.h"

// Ensure Logger is used from global namespace to avoid ambiguity
using ::Logger;

// MessageQueue implements a thread-safe queue for messages built from one or more fixed‐size buffers.
// Each message is stored in a linked list of MessageBuffer objects. Message objects and
// MessageBuffer objects are allocated from pools (BitmappedObjectPool).
//
// Thread Safety:
// - All public methods are thread-safe
// - Internal synchronization uses a mutex and semaphore
// - Semaphore ensures proper message availability signaling
// - Multiple readers and writers are supported
//
// Memory Management:
// - Fixed-size buffer pools prevent heap fragmentation
// - Object pooling ensures efficient reuse of memory
// - Pools grow dynamically as needed until system memory is exhausted
// - No dynamic allocations during normal operation
// - Pools can shrink when memory pressure is reduced (controlled by MESSAGE_QUEUE_SLACK_PERCENT)

namespace Syslog_agent {
class MessageBatcher;  // Forward declaration

class AGENTLIB_API MessageQueue
{
public:
    friend class MessageBatcher;  // Allow MessageBatcher to access private members

    static constexpr unsigned int MAX_BUFFERS_PER_MESSAGE = 32;  // Maximum ~64KB per message
    static constexpr int MESSAGE_BUFFER_SIZE = 2048;            // Fixed 2KB buffers
    static constexpr int MESSAGE_QUEUE_SLACK_PERCENT = 80;      // Keep up to 80% unused before shrinking

    // Structure for each message buffer.
    struct MessageBuffer {
        char buffer[MESSAGE_BUFFER_SIZE];
        MessageBuffer* next = nullptr;
    };

    // Structure for each message.
    struct Message {
        uint32_t buffer_count = 0;
        uint32_t data_length = 0;
        int64_t timestamp = 0;  // Timestamp when the message was enqueued.
        MessageBuffer* message_buffers = nullptr;
        Message* next = nullptr; // For linking in the queue.
    };

    // Construct a MessageQueue.
    // message_queue_size: initial size for Message objects pool (will grow as needed).
    // message_buffers_chunk_size: initial size for MessageBuffer objects pool (will grow as needed).
    // Both pools will grow dynamically until system memory is exhausted.
    MessageQueue(uint32_t message_queue_size, uint32_t message_buffers_chunk_size);
    ~MessageQueue();

    // Returns true if the queue is empty.
    bool isEmpty() const {
        if (is_shutting_down_) return true;  // During shutdown, treat as empty
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return (first_message_ == nullptr);
    }
    
    // Enqueue a new message given its content and length.
    // Thread-safe: Yes
    // Returns true if successful, false if message is invalid or queue is full.
    bool enqueue(const char* message_content, const uint32_t message_len);

    // Dequeue the oldest message.
    // Thread-safe: Yes
    // Blocks until a message is available.
    // Copies the message content (concatenating across buffers) into message_content.
    // max_len is the size of the provided buffer.
    // Returns the message length on success, or -1 on error.
    int dequeue(char* message_content, const uint32_t max_len);

    // Peek at a message without removing it.
    // Thread-safe: Yes
    // If msg is null, peeks at the oldest message.
    // If msg is provided, peeks at that specific message.
    // Copies the message content into message_content.
    // Returns the message length on success, or -1 on error.
    int peek(Message* msg, char* message_content, const uint32_t max_len) const;

    // Remove the front message (waiting for one to be available).
    // Thread-safe: Yes
    // Returns true if an item was removed.
    bool removeFront();

    // Return the number of queued messages.
    uint32_t length() const {
        if (is_shutting_down_) return 0;  // During shutdown, treat as empty
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return length_;
    }
    
    // Wait until at least one message is available or until timeout_ms milliseconds elapse.
    // Thread-safe: Yes
    // Returns true if a message is available.
    bool waitForMessages(uint32_t timeout_ms) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return items_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this]() { return first_message_ != nullptr; });
    }

    // Get the timestamp (in milliseconds since epoch) of the oldest message.
    // Thread-safe: Yes
    // Returns 0 if queue is empty.
    int64_t getOldestMessageTimestamp() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!first_message_) {
            return 0;
        }
        if (!messages_pool_->isValidObject(first_message_)) {
            auto logger = LOG_THIS;
            logger->recoverable_error("MessageQueue::getOldestMessageTimestamp() : first_message_ points to invalid object\n");
            return 0;
        }
        return first_message_->timestamp;
    }

    // Traverse the queue starting from the given message or from the front if null.
    // Thread-safe: Yes
    // Note: This is primarily intended for use by MessageBatcher for efficient batch processing.
    std::experimental::generator<Message*> traverseQueue(Message* first = nullptr) const;

    // Set a hook function to be called before/after each enqueue operation.
    // The hook function should return true to proceed with the enqueue, or false to cancel it.
    // The hook is called with the queue length, the message being enqueued, and a boolean indicating
    // whether the enqueue is pre- or post- enqueue.
    void setEnqueueHook(std::function<bool(size_t queue_length, Message* message, bool is_pre_enqueue)> hook) {
        enqueue_hook_ = std::move(hook);
    }

    void beginShutdown();

    bool isShuttingDown() const {
        return is_shutting_down_;
    }

private:
    // Private helper that removes the front message from the linked list.
    // Assumes that queue_mutex_ is already held.
    void removeFrontInternal();

    // Releases all MessageBuffer objects associated with the given message.
    void releaseMessageBuffers(Message& message);

    // Creates a new Message from the given content.
    // Returns a pointer to a Message allocated from the pool (or nullptr on failure).
    Message* createMessage(const char* message_content, const uint32_t message_len, uint64_t timestamp);
    
    const uint32_t message_queue_chunk_size_;  // Initial chunk size for pools
    uint32_t length_{ 0 };  // Protected by queue_mutex_
    
    mutable std::mutex queue_mutex_;
    std::condition_variable items_cv_;

    // Pools for Message and MessageBuffer objects.
    std::unique_ptr<BitmappedObjectPool<Message>> messages_pool_;
    std::unique_ptr<BitmappedObjectPool<MessageBuffer>> message_buffers_pool_;

    // Linked list of queued messages.
    Message* first_message_ = nullptr;  // Protected by queue_mutex_
    Message* last_message_ = nullptr;   // Protected by queue_mutex_

    // Semaphore used to signal that messages are available.
    // Used in conjunction with mutex for proper synchronization.
    std::counting_semaphore<> items_sem_{ 0 };

    // Handler gets: queue size, message to be queued, and whether this is pre/post enqueue
    // Returns true to continue with the enqueue, false to cancel it
    std::function<bool(size_t, Message*, bool)> enqueue_hook_{ nullptr };

    std::atomic<bool> is_shutting_down_{false};
};
};

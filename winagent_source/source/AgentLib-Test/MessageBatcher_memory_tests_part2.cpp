#include "pch.h"
#include "../AgentLib/MessageBatcher.h"
#include "../AgentLib/MessageQueue.h"
#include "../AgentLib/BatchBufferGuard.h"

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include <future>

using namespace Syslog_agent;
using namespace std;

// -----------------------------------------------------------------------------
// MessageQueueWithTracking: Extends MessageQueue to track memory usage
// -----------------------------------------------------------------------------
class MessageQueueWithTracking : public MessageQueue {
private:
    size_t total_memory_used_ = 0;
    size_t peak_memory_used_ = 0;
    std::vector<size_t> message_sizes_;

public:
    MessageQueueWithTracking(size_t initial_queue_size, size_t max_message_size)
        : MessageQueue(initial_queue_size, max_message_size) {
    }

    // Override enqueue to track memory usage
    bool enqueue(const char* message, size_t message_length) override {
        bool result = MessageQueue::enqueue(message, message_length);
        if (result) {
            total_memory_used_ += message_length;
            peak_memory_used_ = std::max(peak_memory_used_, total_memory_used_);
            message_sizes_.push_back(message_length);
        }
        return result;
    }

    // Override removeFront to track memory usage
    bool removeFront() override {
        if (message_sizes_.empty()) {
            return false;
        }
        
        bool result = MessageQueue::removeFront();
        if (result) {
            total_memory_used_ -= message_sizes_.front();
            message_sizes_.erase(message_sizes_.begin());
        }
        return result;
    }

    // Get memory tracking metrics
    size_t GetTotalMemoryUsed() const { return total_memory_used_; }
    size_t GetPeakMemoryUsed() const { return peak_memory_used_; }
    size_t GetMessageCount() const { return message_sizes_.size(); }
};

// -----------------------------------------------------------------------------
// BatcherWithStringHandling: To test string operations memory impact
// -----------------------------------------------------------------------------
class BatcherWithStringHandling : public MessageBatcher {
private:
    mutable std::vector<char*> buffers_; // Track allocated buffers for cleanup
    mutable std::vector<std::string> string_buffers_; // String operations for testing
    bool use_inefficient_string_handling_;
    size_t string_buffer_count_;
    size_t buffer_size_;

public:
    BatcherWithStringHandling(uint32_t max_batch_size, uint32_t max_batch_age, 
                              bool use_inefficient_string_handling = false)
        : MessageBatcher(max_batch_size, max_batch_age),
          use_inefficient_string_handling_(use_inefficient_string_handling),
          string_buffer_count_(0),
          buffer_size_(1024 * 1024) {
    }
    
    ~BatcherWithStringHandling() {
        // Clean up any remaining buffers
        for (auto ptr : buffers_) {
            delete[] ptr;
        }
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        char* buffer = new char[buffer_size_];
        buffers_.push_back(buffer);
        
        // Test inefficient string handling if enabled
        if (use_inefficient_string_handling_) {
            // Create a bunch of temporary strings to simulate string concatenation
            for (size_t i = 0; i < 1000; i++) {
                std::string temp = "Temporary string for testing " + std::to_string(i);
                string_buffers_.push_back(temp);
            }
            string_buffer_count_ += string_buffers_.size();
        }
        
        return buffer;
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        auto it = std::find(buffers_.begin(), buffers_.end(), buffer);
        if (it != buffers_.end()) {
            delete[] *it;
            buffers_.erase(it);
        }
        
        // Clear string buffers if testing inefficient handling
        if (use_inefficient_string_handling_) {
            string_buffers_.clear();
        }
        
        return true;
    }

    uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }
    
    // Metrics for testing
    size_t GetBufferCount() const {
        return buffers_.size();
    }
    
    size_t GetStringBufferCount() const {
        return string_buffer_count_;
    }

protected:
    uint32_t GetMaxBatchSizeBytes_() const override {
        return static_cast<uint32_t>(buffer_size_);
    }

    void GetMessageHeader_(char* dest, size_t max_size, size_t& size_out) const override {
        const char* header = "[BATCH_START]";
        size_t len = strlen(header);
        if (max_size >= len) {
            memcpy(dest, header, len);
            size_out = len;
        }
        else {
            size_out = 0;
        }
    }

    void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const override {
        const char* sep = "|";
        size_t len = strlen(sep);
        if (max_size >= len) {
            memcpy(dest, sep, len);
            size_out = len;
        }
        else {
            size_out = 0;
        }
    }

    void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const override {
        const char* trailer = "[BATCH_END]";
        size_t len = strlen(trailer);
        if (max_size >= len) {
            memcpy(dest, trailer, len);
            size_out = len;
        }
        else {
            size_out = 0;
        }
    }
    
    void validateBatchBuffer(char* buffer, size_t buffer_size) const override {
        // Simple implementation for tests - verify buffer has trailer
        const char* trailer = "[BATCH_END]";
        size_t trailer_len = strlen(trailer);
        
        if (buffer_size >= trailer_len) {
            // Check if buffer ends with trailer
            if (strcmp(buffer + buffer_size - trailer_len, trailer) != 0) {
                // If space allows, add the trailer
                if (buffer_size + trailer_len < GetMaxBatchSizeBytes()) {
                    memcpy(buffer + buffer_size, trailer, trailer_len);
                }
            }
        }
    }
};

// -----------------------------------------------------------------------------
// Fixture for memory tracking tests
// -----------------------------------------------------------------------------
class MessageBatcherMemoryTrackingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a tracking message queue for memory measurement
        tracking_queue = std::make_shared<MessageQueueWithTracking>(10, 2 * 1024 * 1024);
    }

    void TearDown() override {
        tracking_queue.reset();
    }

    // Helper: Enqueue each message from a vector of strings
    void AddTestMessages(const std::vector<std::string>& messages) {
        for (const auto& msg : messages) {
            tracking_queue->enqueue(msg.c_str(), msg.length());
        }
    }

    // Helper: Generate a random message of specified size
    std::string GenerateMessage(size_t size) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        std::string result;
        result.reserve(size);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
        
        for (size_t i = 0; i < size; ++i) {
            result += alphanum[dis(gen)];
        }
        
        return result;
    }

    std::shared_ptr<MessageQueueWithTracking> tracking_queue;
};

// -----------------------------------------------------------------------------
// Unbounded Queue Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTrackingTest, UnboundedQueueGrowth) {
    // Test scenario where messages are added faster than processed
    auto batcher = std::make_unique<LargeBufferMessageBatcher>(10, 1000);
    
    // Add a large number of messages to simulate queue growth
    const int MESSAGE_COUNT = 1000;
    const size_t MESSAGE_SIZE = 10 * 1024; // 10 KB
    
    // Track memory growth
    std::vector<size_t> memory_usage;
    
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        tracking_queue->enqueue(GenerateMessage(MESSAGE_SIZE).c_str(), MESSAGE_SIZE);
        
        if (i % 100 == 0) {
            memory_usage.push_back(tracking_queue->GetTotalMemoryUsed());
            
            // Process a batch (but fewer than we've added - simulating falling behind)
            if (i > 0) {
                char* batch_buffer = batcher->GetBatchBuffer();
                auto result = batcher->BatchEvents(tracking_queue, batch_buffer, batcher->GetMaxBatchSizeBytes());
                
                // Process only a few messages (to simulate falling behind)
                for (uint32_t j = 0; j < std::min(static_cast<uint32_t>(5), result.messages_batched); j++) {
                    tracking_queue->removeFront();
                }
                
                batcher->ReleaseBatchBuffer(batch_buffer);
            }
        }
    }
    
    // Verify memory growth pattern
    for (size_t i = 1; i < memory_usage.size(); i++) {
        EXPECT_GT(memory_usage[i], memory_usage[0]); // Memory should increase
    }
    
    // Clean up remaining messages
    while (tracking_queue->length() > 0) {
        tracking_queue->removeFront();
    }
}

// -----------------------------------------------------------------------------
// String Handling Inefficiency Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTrackingTest, StringHandlingInefficiency) {
    // Compare memory usage with efficient vs inefficient string handling
    auto efficient_batcher = std::make_unique<BatcherWithStringHandling>(10, 1000, false);
    auto inefficient_batcher = std::make_unique<BatcherWithStringHandling>(10, 1000, true);
    
    // Add test messages
    std::vector<std::string> messages;
    for (int i = 0; i < 100; i++) {
        messages.push_back(GenerateMessage(1024)); // 1 KB messages
    }
    AddTestMessages(messages);
    
    // Test with efficient string handling
    char* efficient_buffer = efficient_batcher->GetBatchBuffer();
    auto efficient_result = efficient_batcher->BatchEvents(tracking_queue, efficient_buffer, efficient_batcher->GetMaxBatchSizeBytes());
    efficient_batcher->ReleaseBatchBuffer(efficient_buffer);
    
    // Reset queue for next test
    while (tracking_queue->length() > 0) {
        tracking_queue->removeFront();
    }
    AddTestMessages(messages);
    
    // Test with inefficient string handling
    char* inefficient_buffer = inefficient_batcher->GetBatchBuffer();
    auto inefficient_result = inefficient_batcher->BatchEvents(tracking_queue, inefficient_buffer, inefficient_batcher->GetMaxBatchSizeBytes());
    inefficient_batcher->ReleaseBatchBuffer(inefficient_buffer);
    
    // Verify inefficient string handling created more temporary strings
    EXPECT_EQ(efficient_batcher->GetStringBufferCount(), 0);
    EXPECT_GT(inefficient_batcher->GetStringBufferCount(), 0);
}

// -----------------------------------------------------------------------------
// Concurrent Batch Processing Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTrackingTest, ConcurrentBatchProcessing) {
    // Test scenario with multiple threads accessing the batcher to check for thread safety
    auto batcher = std::make_unique<LargeBufferMessageBatcher>(10, 1000, 1024 * 1024);
    
    // Add a large number of messages
    const int MESSAGE_COUNT = 1000;
    const size_t MESSAGE_SIZE = 1024; // 1 KB
    
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        tracking_queue->enqueue(GenerateMessage(MESSAGE_SIZE).c_str(), MESSAGE_SIZE);
    }
    
    // Create multiple threads to process batches concurrently
    const int THREAD_COUNT = 4;
    std::vector<std::future<bool>> futures;
    
    for (int i = 0; i < THREAD_COUNT; i++) {
        futures.push_back(std::async(std::launch::async, [&batcher, this]() {
            // Process batches until queue is empty
            while (tracking_queue->length() > 0) {
                char* batch_buffer = batcher->GetBatchBuffer();
                auto result = batcher->BatchEvents(tracking_queue, batch_buffer, batcher->GetMaxBatchSizeBytes());
                
                if (result.status == MessageBatcher::BatchResult::Status::Success && 
                    result.messages_batched > 0) {
                    // Simulate processing the batch
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    
                    // Remove processed messages
                    for (uint32_t j = 0; j < result.messages_batched; j++) {
                        tracking_queue->removeFront();
                    }
                }
                
                batcher->ReleaseBatchBuffer(batch_buffer);
                
                if (result.status == MessageBatcher::BatchResult::Status::NoMessages) {
                    break;
                }
            }
            
            return true;
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }
    
    // Verify all messages were processed
    EXPECT_EQ(tracking_queue->length(), 0);
    
    // Verify all buffers were properly released
    EXPECT_EQ(batcher->GetBufferCount(), 0);
}

// -----------------------------------------------------------------------------
// Memory Fragmentation Simulation Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTrackingTest, MemoryFragmentationSimulation) {
    // Create a batcher with variable buffer sizes
    auto batcher = std::make_unique<LargeBufferMessageBatcher>(10, 1000, 1024 * 1024);
    
    // Create messages of various sizes to simulate fragmentation
    std::vector<size_t> message_sizes = {
        512,       // 512 bytes
        1024,      // 1 KB
        4 * 1024,  // 4 KB
        16 * 1024, // 16 KB
        64 * 1024, // 64 KB
        256 * 1024 // 256 KB
    };
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, message_sizes.size() - 1);
    
    // Generate 1000 messages of random sizes
    for (int i = 0; i < 1000; i++) {
        size_t size = message_sizes[dis(gen)];
        tracking_queue->enqueue(GenerateMessage(size).c_str(), size);
    }
    
    // Process messages in small batches to simulate fragmentation
    const int BATCH_COUNT = 50;
    for (int i = 0; i < BATCH_COUNT; i++) {
        // Process a batch
        char* batch_buffer = batcher->GetBatchBuffer();
        auto result = batcher->BatchEvents(tracking_queue, batch_buffer, batcher->GetMaxBatchSizeBytes());
        
        if (result.status == MessageBatcher::BatchResult::Status::Success) {
            // Remove processed messages
            for (uint32_t j = 0; j < result.messages_batched; j++) {
                tracking_queue->removeFront();
            }
        }
        
        batcher->ReleaseBatchBuffer(batch_buffer);
        
        // Add more messages to simulate continuous operation
        if (i % 5 == 0) {
            for (int j = 0; j < 20; j++) {
                size_t size = message_sizes[dis(gen)];
                tracking_queue->enqueue(GenerateMessage(size).c_str(), size);
            }
        }
    }
    
    // Verify all buffers were properly released
    EXPECT_EQ(batcher->GetBufferCount(), 0);
}

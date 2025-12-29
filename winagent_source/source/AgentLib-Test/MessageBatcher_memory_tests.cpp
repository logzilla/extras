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
// TestMessageBatcherWithMemLeak: test class that intentionally leaks memory for test purposes
// -----------------------------------------------------------------------------
class TestMessageBatcherWithMemLeak : public MessageBatcher {
private:
    mutable std::vector<char*> leaked_buffers_; // Track buffers that aren't properly released
    bool should_leak_buffer_;
    bool should_leak_on_batch_buffer_;
    size_t batch_buffer_size_;

public:
    TestMessageBatcherWithMemLeak(uint32_t max_batch_size, uint32_t max_batch_age, 
                                bool should_leak_buffer = false,
                                bool should_leak_on_batch_buffer = false)
        : MessageBatcher(max_batch_size, max_batch_age), 
          should_leak_buffer_(should_leak_buffer),
          should_leak_on_batch_buffer_(should_leak_on_batch_buffer),
          batch_buffer_size_(1024) {
    }
    
    ~TestMessageBatcherWithMemLeak() {
        // Clean up any remaining buffers to avoid actual memory leaks in tests
        for (auto ptr : leaked_buffers_) {
            delete[] ptr;
        }
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        char* buffer = new char[batch_buffer_size_];
        if (should_leak_on_batch_buffer_) {
            leaked_buffers_.push_back(buffer);
        }
        return buffer;
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        if (should_leak_buffer_) {
            leaked_buffers_.push_back(buffer);
            return true;
        }
        delete[] buffer;
        return true;
    }

    uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }
    
    // Expose the size of leaked buffers for testing
    size_t GetLeakedBufferCount() const {
        return leaked_buffers_.size();
    }
    
    // Set buffer size to test different allocation sizes
    void SetBatchBufferSize(size_t size) {
        batch_buffer_size_ = size;
    }

protected:
    uint32_t GetMaxBatchSizeBytes_() const override {
        return 1024; // For test purposes
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
// LargeBufferMessageBatcher: for testing very large buffer allocations
// -----------------------------------------------------------------------------
class LargeBufferMessageBatcher : public MessageBatcher {
private:
    size_t buffer_size_;
    mutable std::vector<char*> buffers_; // Track allocated buffers for cleanup

public:
    LargeBufferMessageBatcher(uint32_t max_batch_size, uint32_t max_batch_age, size_t buffer_size = 1024 * 1024)
        : MessageBatcher(max_batch_size, max_batch_age), buffer_size_(buffer_size) {
    }
    
    ~LargeBufferMessageBatcher() {
        // Clean up any remaining buffers
        for (auto ptr : buffers_) {
            delete[] ptr;
        }
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        char* buffer = new char[buffer_size_];
        buffers_.push_back(buffer);
        return buffer;
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        auto it = std::find(buffers_.begin(), buffers_.end(), buffer);
        if (it != buffers_.end()) {
            delete[] *it;
            buffers_.erase(it);
        }
        return true;
    }

    uint32_t GetMaxBatchSizeBytes() const override {
        return static_cast<uint32_t>(buffer_size_);
    }
    
    // Get current buffer count for testing
    size_t GetBufferCount() const {
        return buffers_.size();
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
// MessageBatcherMemoryTest fixture
// -----------------------------------------------------------------------------
class MessageBatcherMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a message queue with initial pool sizes
        message_queue = std::make_shared<MessageQueue>(10, 20);
    }

    void TearDown() override {
        message_queue.reset();
    }

    // Helper: Enqueue each message from a vector of strings
    void AddTestMessages(const std::vector<std::string>& messages) {
        for (const auto& msg : messages) {
            message_queue->enqueue(msg.c_str(), msg.length());
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

    std::shared_ptr<MessageQueue> message_queue;
};

// -----------------------------------------------------------------------------
// Buffer Allocation/Release Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTest, BatchBufferGuardCleanup) {
    // Test that BatchBufferGuard properly releases memory
    auto batcher = std::make_unique<TestMessageBatcherWithMemLeak>(5, 1000);
    
    {
        // Test normal case - buffer should be released when guard goes out of scope
        BatchBufferGuard guard(batcher->GetBatchBuffer(), *batcher);
        EXPECT_TRUE(guard.get() != nullptr);
    }
    
    // No leaks should be recorded
    EXPECT_EQ(batcher->GetLeakedBufferCount(), 0);
}

TEST_F(MessageBatcherMemoryTest, BatchBufferGuardWithException) {
    // Test that BatchBufferGuard properly releases memory even when an exception is thrown
    auto batcher = std::make_unique<TestMessageBatcherWithMemLeak>(5, 1000);
    
    try {
        BatchBufferGuard guard(batcher->GetBatchBuffer(), *batcher);
        EXPECT_TRUE(guard.get() != nullptr);
        throw std::runtime_error("Test exception");
    }
    catch (const std::exception&) {
        // Exception caught, but buffer should still be released
    }
    
    // No leaks should be recorded
    EXPECT_EQ(batcher->GetLeakedBufferCount(), 0);
}

TEST_F(MessageBatcherMemoryTest, BatchBufferLeakDetection) {
    // Test to ensure our leak detection in tests is working
    auto batcher = std::make_unique<TestMessageBatcherWithMemLeak>(5, 1000, true);
    
    // Get a buffer but without using the guard
    char* buffer = batcher->GetBatchBuffer();
    EXPECT_TRUE(buffer != nullptr);
    
    // Explicitly release it, but our test class should "leak" it
    batcher->ReleaseBatchBuffer(buffer);
    
    // Should have one leak recorded
    EXPECT_EQ(batcher->GetLeakedBufferCount(), 1);
}

// -----------------------------------------------------------------------------
// Memory Leak Detection Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTest, DetectLeaksInBatchEvents) {
    // Test to detect leaks in BatchEvents
    auto batcher = std::make_unique<TestMessageBatcherWithMemLeak>(5, 1000, false, true);
    
    // Add some test messages
    std::vector<std::string> messages = {"Test message 1", "Test message 2", "Test message 3"};
    AddTestMessages(messages);
    
    // Allocate a buffer for batching
    char* batch_buffer = new char[1024];
    
    // Call BatchEvents, which internally uses GetBatchBuffer
    auto result = batcher->BatchEvents(message_queue, batch_buffer, 1024);
    
    // Verify the batch was successful
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 3);
    
    // Check if BatchEvents leaked any buffers
    EXPECT_EQ(batcher->GetLeakedBufferCount(), 1); // Should have one leak from the peek buffer
    
    delete[] batch_buffer;
}

// -----------------------------------------------------------------------------
// Buffer Size/Capacity Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryTest, LargeBufferAllocation) {
    // Test handling of large buffer allocations
    const size_t LARGE_BUFFER_SIZE = 10 * 1024 * 1024; // 10 MB
    auto batcher = std::make_unique<LargeBufferMessageBatcher>(100, 1000, LARGE_BUFFER_SIZE);
    
    // Add some large test messages
    std::vector<std::string> messages;
    for (int i = 0; i < 10; i++) {
        messages.push_back(GenerateMessage(100 * 1024)); // 100 KB messages
    }
    AddTestMessages(messages);
    
    // Allocate a large buffer for batching
    std::unique_ptr<char[]> batch_buffer(new char[LARGE_BUFFER_SIZE]);
    
    // Call BatchEvents
    auto result = batcher->BatchEvents(message_queue, batch_buffer.get(), LARGE_BUFFER_SIZE);
    
    // Verify the batch was successful
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_GT(result.messages_batched, 0);
    
    // Check if all buffers were properly released
    EXPECT_EQ(batcher->GetBufferCount(), 0);
}

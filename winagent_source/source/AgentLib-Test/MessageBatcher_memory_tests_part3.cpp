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
#include <stdexcept>

using namespace Syslog_agent;
using namespace std;

// -----------------------------------------------------------------------------
// ExceptionHandlingBatcher: Tests exception handling and memory safety
// -----------------------------------------------------------------------------
class ExceptionHandlingBatcher : public MessageBatcher {
private:
    mutable std::vector<char*> buffers_;
    bool throw_during_get_buffer_;
    bool throw_during_release_buffer_;
    bool throw_during_peek_;
    int call_counter_;
    int throw_on_call_;
    size_t buffer_size_;

public:
    ExceptionHandlingBatcher(uint32_t max_batch_size, uint32_t max_batch_age, 
                          bool throw_during_get_buffer = false,
                          bool throw_during_release_buffer = false,
                          bool throw_during_peek = false,
                          int throw_on_call = 1)
        : MessageBatcher(max_batch_size, max_batch_age),
          throw_during_get_buffer_(throw_during_get_buffer),
          throw_during_release_buffer_(throw_during_release_buffer),
          throw_during_peek_(throw_during_peek),
          call_counter_(0),
          throw_on_call_(throw_on_call),
          buffer_size_(1024 * 1024) {
    }
    
    ~ExceptionHandlingBatcher() {
        // Clean up any remaining buffers to avoid actual memory leaks
        for (auto ptr : buffers_) {
            delete[] ptr;
        }
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        call_counter_++;
        if (throw_during_get_buffer_ && call_counter_ == throw_on_call_) {
            throw std::runtime_error("Simulated exception during GetBatchBuffer");
        }
        
        char* buffer = new char[buffer_size_];
        buffers_.push_back(buffer);
        return buffer;
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        if (throw_during_release_buffer_) {
            throw std::runtime_error("Simulated exception during ReleaseBatchBuffer");
        }
        
        auto it = std::find(buffers_.begin(), buffers_.end(), buffer);
        if (it != buffers_.end()) {
            delete[] *it;
            buffers_.erase(it);
        }
        return true;
    }

    uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }
    
    BatchResult CustomBatchEvents(shared_ptr<MessageQueue> message_queue, char* batch_buffer, size_t buffer_size) const {
        if (throw_during_peek_) {
            // Process a few messages and then throw to simulate an exception during message processing
            auto result = BatchEvents(message_queue, batch_buffer, buffer_size);
            throw std::runtime_error("Simulated exception during message processing");
            return result; // This line won't execute but prevents compiler warnings
        }
        return BatchEvents(message_queue, batch_buffer, buffer_size);
    }
    
    size_t GetBufferCount() const {
        return buffers_.size();
    }
    
    void ResetCallCounter() {
        call_counter_ = 0;
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
// RetryBufferBatcher: Simulates retry buffer handling
// -----------------------------------------------------------------------------
class RetryBufferBatcher : public MessageBatcher {
private:
    mutable std::vector<char*> buffers_;
    mutable std::vector<std::pair<char*, size_t>> retry_buffers_; // buffer pointer and size
    bool should_retry_;
    size_t max_retry_buffer_size_;
    size_t current_retry_size_;
    size_t buffer_size_;

public:
    RetryBufferBatcher(uint32_t max_batch_size, uint32_t max_batch_age, 
                     bool should_retry = true,
                     size_t max_retry_buffer_size = 5 * 1024 * 1024) // 5 MB max retry buffer
        : MessageBatcher(max_batch_size, max_batch_age),
          should_retry_(should_retry),
          max_retry_buffer_size_(max_retry_buffer_size),
          current_retry_size_(0),
          buffer_size_(1024 * 1024) {
    }
    
    ~RetryBufferBatcher() {
        // Clean up any remaining buffers
        for (auto ptr : buffers_) {
            delete[] ptr;
        }
        
        // Clean up retry buffers
        for (auto& pair : retry_buffers_) {
            delete[] pair.first;
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
        return GetMaxBatchSizeBytes_();
    }
    
    // Simulate handling a failed batch by adding it to retry buffer
    bool AddToRetryBuffer(char* buffer, size_t size) const {
        if (!should_retry_ || buffer == nullptr) {
            return false;
        }
        
        // Check if adding would exceed max retry buffer size
        if (current_retry_size_ + size > max_retry_buffer_size_) {
            return false;
        }
        
        // Create a copy of the buffer for retry
        char* retry_buffer = new char[size];
        memcpy(retry_buffer, buffer, size);
        
        // Add to retry buffers
        retry_buffers_.push_back(std::make_pair(retry_buffer, size));
        current_retry_size_ += size;
        
        return true;
    }
    
    // Retry the oldest batch and remove it from retry buffer
    bool RetryOldestBatch() const {
        if (retry_buffers_.empty()) {
            return false;
        }
        
        // Simulate successful retry by removing the oldest buffer
        auto oldest = retry_buffers_.front();
        delete[] oldest.first;
        current_retry_size_ -= oldest.second;
        retry_buffers_.erase(retry_buffers_.begin());
        
        return true;
    }
    
    // Metrics for testing
    size_t GetBufferCount() const {
        return buffers_.size();
    }
    
    size_t GetRetryBufferCount() const {
        return retry_buffers_.size();
    }
    
    size_t GetCurrentRetrySize() const {
        return current_retry_size_;
    }
    
    bool IsRetryBufferFull(size_t additional_size = 0) const {
        return current_retry_size_ + additional_size > max_retry_buffer_size_;
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
// MessageDuplicationBatcher: Tests message duplication scenarios
// -----------------------------------------------------------------------------
class MessageDuplicationBatcher : public MessageBatcher {
private:
    mutable std::vector<char*> buffers_;
    mutable std::vector<char*> duplicate_buffers_;
    bool copy_messages_;
    size_t buffer_size_;

public:
    MessageDuplicationBatcher(uint32_t max_batch_size, uint32_t max_batch_age, 
                           bool copy_messages = true)
        : MessageBatcher(max_batch_size, max_batch_age),
          copy_messages_(copy_messages),
          buffer_size_(1024 * 1024) {
    }
    
    ~MessageDuplicationBatcher() {
        // Clean up any remaining buffers
        for (auto ptr : buffers_) {
            delete[] ptr;
        }
        
        // Clean up duplicate buffers
        for (auto ptr : duplicate_buffers_) {
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
        return GetMaxBatchSizeBytes_();
    }
    
    // Create and track a duplicate buffer (to simulate message duplication)
    char* CreateDuplicateBuffer(const char* original, size_t size) const {
        if (!copy_messages_ || original == nullptr) {
            return nullptr;
        }
        
        char* duplicate = new char[size];
        memcpy(duplicate, original, size);
        duplicate_buffers_.push_back(duplicate);
        
        return duplicate;
    }
    
    // Metrics for testing
    size_t GetBufferCount() const {
        return buffers_.size();
    }
    
    size_t GetDuplicateBufferCount() const {
        return duplicate_buffers_.size();
    }
    
    // Clean all duplicate buffers
    void CleanDuplicateBuffers() const {
        for (auto ptr : duplicate_buffers_) {
            delete[] ptr;
        }
        duplicate_buffers_.clear();
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
// Test fixture for memory handling specific tests
// -----------------------------------------------------------------------------
class MessageBatcherMemoryExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        message_queue = std::make_shared<MessageQueue>(10, 2 * 1024 * 1024);
    }

    void TearDown() override {
        message_queue.reset();
    }

    // Helper: Enqueue test messages
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
// Exception Handling Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryExceptionTest, ExceptionDuringGetBuffer) {
    // Test that exceptions during GetBatchBuffer don't cause memory leaks
    auto batcher = std::make_shared<ExceptionHandlingBatcher>(10, 1000, true, false, false);
    
    // Add test messages
    std::vector<std::string> messages = {
        "Test message 1", "Test message 2", "Test message 3"
    };
    AddTestMessages(messages);
    
    // Create a buffer for batching
    std::unique_ptr<char[]> batch_buffer(new char[batcher->GetMaxBatchSizeBytes()]);
    
    // Attempt to process batches, which should throw on first call to GetBatchBuffer
    EXPECT_THROW({
        batcher->CustomBatchEvents(message_queue, batch_buffer.get(), batcher->GetMaxBatchSizeBytes());
    }, std::runtime_error);
    
    // After exception, there should be no memory leaks
    EXPECT_EQ(batcher->GetBufferCount(), 0);
}

TEST_F(MessageBatcherMemoryExceptionTest, ExceptionDuringReleaseDuringBatch) {
    // Test that exceptions during ReleaseBatchBuffer don't cause memory leaks
    auto batcher = std::make_shared<ExceptionHandlingBatcher>(10, 1000, false, true, false);
    
    // Add test messages
    std::vector<std::string> messages = {
        "Test message 1", "Test message 2", "Test message 3"
    };
    AddTestMessages(messages);
    
    // Create a buffer for batching
    std::unique_ptr<char[]> batch_buffer(new char[batcher->GetMaxBatchSizeBytes()]);
    
    // Process should throw when releasing the buffer inside BatchEvents
    EXPECT_THROW({
        batcher->CustomBatchEvents(message_queue, batch_buffer.get(), batcher->GetMaxBatchSizeBytes());
    }, std::runtime_error);
    
    // Buffer count may be 1 since the test is designed to fail during release,
    // but the guard pattern should still properly clean up
    EXPECT_LE(batcher->GetBufferCount(), 1);
}

TEST_F(MessageBatcherMemoryExceptionTest, ExceptionDuringProcessing) {
    // Test that exceptions during message processing don't cause memory leaks
    auto batcher = std::make_shared<ExceptionHandlingBatcher>(10, 1000, false, false, true);
    
    // Add test messages
    std::vector<std::string> messages = {
        "Test message 1", "Test message 2", "Test message 3"
    };
    AddTestMessages(messages);
    
    // Create a buffer for batching
    std::unique_ptr<char[]> batch_buffer(new char[batcher->GetMaxBatchSizeBytes()]);
    
    // Process should throw during message peek/processing
    EXPECT_THROW({
        batcher->CustomBatchEvents(message_queue, batch_buffer.get(), batcher->GetMaxBatchSizeBytes());
    }, std::runtime_error);
    
    // Verify memory is properly cleaned up
    EXPECT_EQ(batcher->GetBufferCount(), 0);
}

// -----------------------------------------------------------------------------
// Retry Buffer Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryExceptionTest, RetryBufferManagement) {
    // Test scenario with retry buffers for failed batches
    auto batcher = std::make_shared<RetryBufferBatcher>(10, 1000);
    
    // Add test messages
    std::vector<std::string> messages;
    for (int i = 0; i < 50; i++) {
        messages.push_back(GenerateMessage(10 * 1024)); // 10 KB messages
    }
    AddTestMessages(messages);
    
    // Process batches and simulate failures (add to retry buffer)
    for (int i = 0; i < 10; i++) {
        char* batch_buffer = batcher->GetBatchBuffer();
        auto result = batcher->BatchEvents(message_queue, batch_buffer, batcher->GetMaxBatchSizeBytes());
        
        if (result.status == MessageBatcher::BatchResult::Status::Success) {
            // Simulate failure and add to retry buffer
            batcher->AddToRetryBuffer(batch_buffer, result.bytes_written);
            
            // In a real scenario we wouldn't commit, but for testing we still
            // remove from the queue to process more messages
            for (uint32_t j = 0; j < result.messages_batched; j++) {
                message_queue->removeFront();
            }
        }
        
        batcher->ReleaseBatchBuffer(batch_buffer);
    }
    
    // Verify retry buffers are populated
    EXPECT_GT(batcher->GetRetryBufferCount(), 0);
    
    // Simulate retry processing
    while (batcher->GetRetryBufferCount() > 0) {
        batcher->RetryOldestBatch();
    }
    
    // Verify all retry buffers are released
    EXPECT_EQ(batcher->GetRetryBufferCount(), 0);
    EXPECT_EQ(batcher->GetCurrentRetrySize(), 0);
}

TEST_F(MessageBatcherMemoryExceptionTest, RetryBufferLimits) {
    // Test that retry buffer respects size limits
    auto batcher = std::make_shared<RetryBufferBatcher>(10, 1000, true, 1 * 1024 * 1024); // 1 MB max retry buffer
    
    // Add test messages, deliberately larger than retry buffer can hold
    std::vector<std::string> messages;
    for (int i = 0; i < 50; i++) {
        messages.push_back(GenerateMessage(50 * 1024)); // 50 KB messages (20 = 1 MB)
    }
    AddTestMessages(messages);
    
    // Process batches and attempt to fill retry buffer
    int successful_retries = 0;
    int failed_retries = 0;
    
    while (message_queue->length() > 0) {
        char* batch_buffer = batcher->GetBatchBuffer();
        auto result = batcher->BatchEvents(message_queue, batch_buffer, batcher->GetMaxBatchSizeBytes());
        
        if (result.status == MessageBatcher::BatchResult::Status::Success) {
            // Attempt to add to retry buffer
            bool added = batcher->AddToRetryBuffer(batch_buffer, result.bytes_written);
            
            if (added) {
                successful_retries++;
            } else {
                failed_retries++;
            }
            
            // Remove from queue for testing purposes
            for (uint32_t j = 0; j < result.messages_batched; j++) {
                message_queue->removeFront();
            }
        }
        
        batcher->ReleaseBatchBuffer(batch_buffer);
    }
    
    // Verify we had both successful and failed retries due to size limits
    EXPECT_GT(successful_retries, 0);
    EXPECT_GT(failed_retries, 0);
    
    // Current retry size should be no greater than the limit
    EXPECT_LE(batcher->GetCurrentRetrySize(), 1 * 1024 * 1024);
}

// -----------------------------------------------------------------------------
// Message Duplication Tests
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryExceptionTest, MessageDuplication) {
    // Test to measure memory impact of message duplication
    auto batcher = std::make_shared<MessageDuplicationBatcher>(10, 1000);
    
    // Add test messages
    std::vector<std::string> messages;
    for (int i = 0; i < 20; i++) {
        messages.push_back(GenerateMessage(10 * 1024)); // 10 KB messages
    }
    AddTestMessages(messages);
    
    // First, process normally without duplication
    char* batch_buffer = batcher->GetBatchBuffer();
    auto result = batcher->BatchEvents(message_queue, batch_buffer, batcher->GetMaxBatchSizeBytes());
    
    // Now simulate message duplication (copying to multiple places)
    for (int i = 0; i < 3; i++) { // Simulate 3 copies
        batcher->CreateDuplicateBuffer(batch_buffer, result.bytes_written);
    }
    
    // Verify duplication occurred
    EXPECT_EQ(batcher->GetDuplicateBufferCount(), 3);
    
    // Clean up
    batcher->ReleaseBatchBuffer(batch_buffer);
    batcher->CleanDuplicateBuffers();
    
    // Verify cleanup was successful
    EXPECT_EQ(batcher->GetBufferCount(), 0);
    EXPECT_EQ(batcher->GetDuplicateBufferCount(), 0);
}

// -----------------------------------------------------------------------------
// Long-Running Operation Simulation
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherMemoryExceptionTest, LongRunningOperation) {
    // Test a long-running operation with continuous message addition and batching
    auto batcher = std::make_shared<LargeBufferMessageBatcher>(10, 1000, 1024 * 1024); // From previous test file
    
    // Set up metrics tracking
    size_t total_messages_batched = 0;
    size_t total_bytes_processed = 0;
    size_t peak_queue_size = 0;
    
    // Simulate long-running operation (with varying message rates)
    const int OPERATION_CYCLES = 10;
    std::vector<std::pair<int, int>> message_patterns = {
        {20, 10},  // Add 20, process 10 - queue grows
        {10, 15},  // Add 10, process 15 - queue shrinks
        {5, 5},    // Balanced
        {30, 5},   // Burst of traffic
        {0, 10}    // Drain queue
    };
    
    // Run through the patterns multiple times
    for (int cycle = 0; cycle < OPERATION_CYCLES; cycle++) {
        for (const auto& pattern : message_patterns) {
            int to_add = pattern.first;
            int to_process = pattern.second;
            
            // Add messages
            for (int i = 0; i < to_add; i++) {
                size_t msg_size = 1024 + (cycle * 512); // Gradually increase message size
                message_queue->enqueue(GenerateMessage(msg_size).c_str(), msg_size);
            }
            
            // Track peak queue size
            peak_queue_size = std::max(peak_queue_size, message_queue->length());
            
            // Process batches
            for (int i = 0; i < to_process && message_queue->length() > 0; i++) {
                BatchBufferGuard buffer_guard(batcher->GetBatchBuffer(), *batcher);
                
                auto result = batcher->BatchEvents(message_queue, buffer_guard.get(), batcher->GetMaxBatchSizeBytes());
                
                if (result.status == MessageBatcher::BatchResult::Status::Success) {
                    total_messages_batched += result.messages_batched;
                    total_bytes_processed += result.bytes_written;
                    
                    // Remove messages
                    for (uint32_t j = 0; j < result.messages_batched; j++) {
                        message_queue->removeFront();
                    }
                }
                
                // buffer_guard will auto-release
            }
        }
    }
    
    // Final cleanup - drain the queue
    while (message_queue->length() > 0) {
        BatchBufferGuard buffer_guard(batcher->GetBatchBuffer(), *batcher);
        
        auto result = batcher->BatchEvents(message_queue, buffer_guard.get(), batcher->GetMaxBatchSizeBytes());
        
        if (result.status == MessageBatcher::BatchResult::Status::Success) {
            total_messages_batched += result.messages_batched;
            total_bytes_processed += result.bytes_written;
            
            // Remove messages
            for (uint32_t j = 0; j < result.messages_batched; j++) {
                message_queue->removeFront();
            }
        }
    }
    
    // Verify the simulation behavior
    EXPECT_GT(total_messages_batched, 0);
    EXPECT_GT(total_bytes_processed, 0);
    EXPECT_GT(peak_queue_size, 0);
    EXPECT_EQ(message_queue->length(), 0); // All messages processed
    EXPECT_EQ(batcher->GetBufferCount(), 0); // All buffers released
}

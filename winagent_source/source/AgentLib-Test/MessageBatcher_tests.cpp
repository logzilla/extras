#include "pch.h"
#include "../AgentLib/MessageBatcher.h"
#include "../AgentLib/MessageQueue.h"

#include <string>
#include <vector>
#include <memory>

using namespace Syslog_agent;
using namespace std;

// -----------------------------------------------------------------------------
// TestMessageBatcher: our test subclass using fixed header, separator, and trailer
// -----------------------------------------------------------------------------
class TestMessageBatcher : public MessageBatcher {
public:
    // Note: In production ordering is important so the batching code must flush
    // the current batch when the next message does not fit (instead of skipping it).
    TestMessageBatcher(uint32_t max_batch_size, uint32_t max_batch_age)
        : MessageBatcher(max_batch_size, max_batch_age) {
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        // Simple implementation for tests - allocate a fixed buffer
        return new char[GetMaxBatchSizeBytes()];
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        delete[] buffer;
        return true;
    }

    uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
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
// MessageBatcherTest fixture
// -----------------------------------------------------------------------------
class MessageBatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a message queue with initial pool sizes.
        message_queue = std::make_shared<MessageQueue>(10, 20);

        // By default, we create a batcher that batches at most 5 messages.
        batcher = std::make_unique<TestMessageBatcher>(5, 1000);
    }

    void TearDown() override {
        message_queue.reset();
        batcher.reset();
    }

    // Helper: Enqueue each message from a vector of strings.
    void AddTestMessages(const std::vector<std::string>& messages) {
        for (const auto& msg : messages) {
            message_queue->enqueue(msg.c_str(), msg.length());
        }
    }

    // Helper: Simulate commit of a batch by removing the first 'count' messages.
    // In production the commit happens only after a successful network send.
    void CommitBatch(uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            message_queue->removeFront();
        }
    }

    std::shared_ptr<MessageQueue> message_queue;
    std::unique_ptr<TestMessageBatcher> batcher;
};

// -----------------------------------------------------------------------------
// Basic tests (empty queue, invalid buffer, basic batching, etc.)
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherTest, EmptyQueue) {
    char buffer[1024];
    auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::NoMessages);
    EXPECT_EQ(result.messages_batched, 0);
    EXPECT_EQ(result.bytes_written, 0);
}

TEST_F(MessageBatcherTest, InvalidBuffer) {
    std::vector<std::string> messages = { "msg1" };
    AddTestMessages(messages);

    auto result = batcher->BatchEvents(message_queue, nullptr, 0);
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::InvalidBuffer);
    EXPECT_EQ(result.messages_batched, 0);
    EXPECT_EQ(result.bytes_written, 0);
}

TEST_F(MessageBatcherTest, BasicBatching) {
    std::vector<std::string> messages = { "msg1", "msg2", "msg3" };
    AddTestMessages(messages);

    char buffer[1024];
    auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 3);
    EXPECT_GT(result.bytes_written, 0);

    // Verify that the batch has the proper format:
    // It must start with the header, contain the messages (separated by '|'),
    // and end with the trailer.
    std::string batch(buffer, result.bytes_written);  // Include all bytes written
    EXPECT_TRUE(batch.find("[BATCH_START]") == 0);
    EXPECT_TRUE(batch.size() >= strlen("[BATCH_END]"));
    EXPECT_EQ(batch.substr(batch.size() - strlen("[BATCH_END]")), "[BATCH_END]");
    EXPECT_NE(batch.find("msg1|msg2|msg3"), std::string::npos);

    // Messages should still be in the queue since we haven't committed yet
    EXPECT_EQ(message_queue->length(), 3);
}

TEST_F(MessageBatcherTest, MaxBatchSize) {
    // Add more messages than our max batch size
    std::vector<std::string> messages = { "msg1", "msg2", "msg3", "msg4", "msg5", "msg6" };
    AddTestMessages(messages);

    char buffer[1024];
    auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 5);  // Should only batch up to max
    EXPECT_GT(result.bytes_written, 0);
}

TEST_F(MessageBatcherTest, BufferSizeConstraints) {
    std::vector<std::string> messages = { "msg1", "msg2", "msg3" };
    AddTestMessages(messages);

    // Test with buffer too small for even one message
    char tiny_buffer[10];
    auto result = batcher->BatchEvents(message_queue, tiny_buffer, sizeof(tiny_buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::BufferTooSmall);
    EXPECT_EQ(result.messages_batched, 0);
    EXPECT_EQ(result.bytes_written, 0);

    // Test with buffer that can fit only some messages plus safety margin
    // Calculate a buffer size that will fit exactly 2 messages but not 3:
    // Header: 13 bytes, Trailer: 11 bytes, 2 msgs (4 bytes each): 8 bytes, 1 separator: 1 byte, safety margin: 16 bytes
    // Total for 2 messages: 13 + 11 + 8 + 1 + 16 = 49, but not enough for 3rd message + separator
    char small_buffer[49];
    result = batcher->BatchEvents(message_queue, small_buffer, sizeof(small_buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_GT(result.messages_batched, 0);
    EXPECT_LT(result.messages_batched, 3);
}

TEST_F(MessageBatcherTest, LargeMessage) {
    // Create a message larger than max size
    std::string large_msg(2000, 'X');  // 2000 'X' characters
    std::vector<std::string> messages = { large_msg };
    AddTestMessages(messages);

    char buffer[4096];
    auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 0);
    EXPECT_EQ(result.bytes_written, 0);
}

TEST_F(MessageBatcherTest, BatchFormatConsistency) {
    std::vector<std::string> messages = { "msg1", "msg2" };
    AddTestMessages(messages);

    char buffer[1024];
    auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 2);

    std::string batch(buffer, result.bytes_written);
    EXPECT_TRUE(batch.find("[BATCH_START]") == 0);
    EXPECT_TRUE(batch.find("|") != std::string::npos);
    EXPECT_EQ(batch.substr(batch.size() - strlen("[BATCH_END]")), "[BATCH_END]");
    EXPECT_NE(batch.find("msg1|msg2"), std::string::npos);

    // Messages should still be in the queue since we haven't committed yet
    EXPECT_EQ(message_queue->length(), 2);
}

// -----------------------------------------------------------------------------
// New tests for multi-batch flushing and trailer integrity
// -----------------------------------------------------------------------------

// This test simulates a situation where the next message would not fit (including trailer space)
// so the current batch is flushed. The message ordering must be preserved.
TEST_F(MessageBatcherTest, FlushOnInsufficientTrailerSpace) {
    // Create messages that will fill most of the buffer
    std::string msg1(400, 'A');  // 400 bytes
    std::string msg2(400, 'B');  // 400 bytes
    std::string msg3(200, 'C');  // 200 bytes - should force flush due to trailer

    std::vector<std::string> messages = { msg1, msg2, msg3 };
    AddTestMessages(messages);

    char buffer[1024];  // Just enough for two large messages + overhead
    auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_GT(result.messages_batched, 0);
    EXPECT_LT(result.messages_batched, 3);  // Should not include msg3

    std::string batch(buffer, result.bytes_written);
    EXPECT_TRUE(batch.find("[BATCH_START]") == 0);
    EXPECT_EQ(batch.substr(batch.size() - strlen("[BATCH_END]")), "[BATCH_END]");

    // Commit the first batch
    CommitBatch(result.messages_batched);

    // Get the next batch
    result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_GT(result.messages_batched, 0);

    batch = std::string(buffer, result.bytes_written);
    EXPECT_TRUE(batch.find("[BATCH_START]") == 0);
    EXPECT_TRUE(batch.find(std::string(200, 'C')) != std::string::npos);
    EXPECT_EQ(batch.substr(batch.size() - strlen("[BATCH_END]")), "[BATCH_END]");
}

// This test enqueues many messages (with sequence numbers) so that multiple batches are produced.
// It then verifies that every batch ends with the trailer and that overall message order is preserved.
TEST_F(MessageBatcherTest, MultipleBatchFlushAndOrdering) {
    // Create 20 messages with sequence numbers
    std::vector<std::string> messages;
    for (int i = 0; i < 20; ++i) {
        messages.push_back("msg" + std::to_string(i));
    }
    AddTestMessages(messages);

    char buffer[1024];
    std::vector<std::string> batches;
    int total_messages = 0;

    while (total_messages < 20) {
        auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
        EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
        EXPECT_GT(result.messages_batched, 0);

        std::string batch(buffer, result.bytes_written);
        EXPECT_TRUE(batch.find("[BATCH_START]") == 0);
        EXPECT_EQ(batch.substr(batch.size() - strlen("[BATCH_END]")), "[BATCH_END]");

        batches.push_back(batch);
        total_messages += result.messages_batched;
        CommitBatch(result.messages_batched);
    }

    EXPECT_GT(batches.size(), 1);  // Should have produced multiple batches
}

// Stress test: simulate production conditions by enqueuing many JSON-like messages,
// using a large buffer (e.g. ~65,536 bytes) and verifying that every produced batch
// ends with the trailer. This test is designed to flush out any error that might cause
// the trailer to be dropped after many batches.
TEST_F(MessageBatcherTest, StressTestLongBatches) {
    // Create 100 JSON-like messages
    std::vector<std::string> messages;
    for (int i = 0; i < 100; ++i) {
        std::string msg = "{\"id\":" + std::to_string(i) + 
                         ",\"timestamp\":\"2023-01-01T00:00:00Z\"," +
                         "\"data\":\"" + std::string(100, 'X') + "\"}";
        messages.push_back(msg);
    }
    AddTestMessages(messages);

    char buffer[65536];
    std::vector<std::string> batches;
    int total_messages = 0;

    while (total_messages < 100) {
        auto result = batcher->BatchEvents(message_queue, buffer, sizeof(buffer));
        EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
        EXPECT_GT(result.messages_batched, 0);

        std::string batch(buffer, result.bytes_written);
        EXPECT_TRUE(batch.find("[BATCH_START]") == 0);
        EXPECT_EQ(batch.substr(batch.size() - strlen("[BATCH_END]")), "[BATCH_END]");

        batches.push_back(batch);
        total_messages += result.messages_batched;
        CommitBatch(result.messages_batched);
    }

    EXPECT_GT(batches.size(), 1);
}


// -----------------------------------------------------------------------------
// Test subclass that simulates a header failure (returns zero size)
// -----------------------------------------------------------------------------
class TestMessageBatcherFailHeader : public MessageBatcher {
public:
    TestMessageBatcherFailHeader(std::uint32_t max_batch_size, std::uint32_t max_batch_age)
        : MessageBatcher(max_batch_size, max_batch_age) {
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        return new char[GetMaxBatchSizeBytes()];
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        delete[] buffer;
        return true;
    }

    std::uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }

protected:
    std::uint32_t GetMaxBatchSizeBytes_() const override { return 1024; }

    // Simulate failure: header too large (or unable to produce header)
    void GetMessageHeader_(char* /*dest*/, size_t max_size, size_t& size_out) const override {
        size_out = max_size + 1; // Return a size larger than the available space
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
// Test subclass that simulates a trailer failure (returns zero size)
// -----------------------------------------------------------------------------
class TestMessageBatcherFailTrailer : public MessageBatcher {
public:
    TestMessageBatcherFailTrailer(uint32_t max_batch_size, uint32_t max_batch_age)
        : MessageBatcher(max_batch_size, max_batch_age) {
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        return new char[GetMaxBatchSizeBytes()];
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        delete[] buffer;
        return true;
    }

    uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }

protected:
    uint32_t GetMaxBatchSizeBytes_() const override { return 1024; }

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

    // Simulate a trailer that's too large for the buffer
    void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const override {
        // When called for size calculation in temp_buffer, return a very large size
        // that will trigger the buffer-too-small check
        size_out = 2000; // Much larger than our buffer size (1024)
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
// Test subclass that simulates separator failure (failing on second call)
// -----------------------------------------------------------------------------
class TestMessageBatcherFailSeparator : public MessageBatcher {
public:
    TestMessageBatcherFailSeparator(std::uint32_t max_batch_size, std::uint32_t max_batch_age)
        : MessageBatcher(max_batch_size, max_batch_age), call_count(0) {
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        return new char[GetMaxBatchSizeBytes()];
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        delete[] buffer;
        return true;
    }

    std::uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }

protected:
    std::uint32_t GetMaxBatchSizeBytes_() const override { return 1024; }

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

    // For the first call (first separator) succeed; then fail on subsequent calls.
    void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const override {
        if (call_count > 0) {
            size_out = 0; // simulate failure on second (or later) separator addition
        }
        else {
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
        call_count++;
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

private:
    mutable int call_count;
};

// -----------------------------------------------------------------------------
// Test subclass that throws an exception in GetMessageHeader_
// -----------------------------------------------------------------------------
class TestMessageBatcherThrowHeader : public MessageBatcher {
public:
    TestMessageBatcherThrowHeader(std::uint32_t max_batch_size, std::uint32_t max_batch_age)
        : MessageBatcher(max_batch_size, max_batch_age) {
    }

    char* GetBatchBuffer(const char* debug_identifier = nullptr) const override {
        return new char[GetMaxBatchSizeBytes()];
    }

    bool ReleaseBatchBuffer(char* buffer) const override {
        delete[] buffer;
        return true;
    }

    std::uint32_t GetMaxBatchSizeBytes() const override {
        return GetMaxBatchSizeBytes_();
    }

protected:
    std::uint32_t GetMaxBatchSizeBytes_() const override { return 1024; }

    // Throw an exception to simulate an unexpected error.
    void GetMessageHeader_(char* /*dest*/, size_t /*max_size*/, size_t& /*size_out*/) const override {
        throw std::runtime_error("Simulated exception in header");
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
// Fixture for additional MessageBatcher tests
// -----------------------------------------------------------------------------
class MessageBatcherAdditionalTest : public ::testing::Test {
protected:
    void SetUp() override {
        message_queue = std::make_shared<MessageQueue>(10, 20);
    }

    void TearDown() override {
        message_queue.reset();
    }

    std::shared_ptr<MessageQueue> message_queue;
};

// -----------------------------------------------------------------------------
// ExactFitMessage: The message exactly fills the available buffer.
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherAdditionalTest, ExactFitMessage) {
    // Define a small buffer where the sizes can be computed.
    // For TestMessageBatcher, header = "[BATCH_START]" (13 bytes),
    // trailer = "[BATCH_END]" (11 bytes), and our implementation now includes a safety margin of 16 bytes.
    // For a buffer of size 66, available for message = 66 - 13 - 11 - 16 = 26.
    const size_t buffer_size = 66;
    char buffer[66];
    std::string msg(26, 'A');

    // Use the base test batcher (from your current tests).
    TestMessageBatcher batcher(5, 1000);
    message_queue->enqueue(msg.c_str(), msg.length());

    auto result = batcher.BatchEvents(message_queue, buffer, buffer_size);
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 1);
    
    // The expected bytes written is the length of the header, message, and trailer
    size_t expected_bytes = strlen("[BATCH_START]") + msg.length() + strlen("[BATCH_END]");
    EXPECT_EQ(result.bytes_written, expected_bytes);

    std::string expected = "[BATCH_START]" + msg + "[BATCH_END]";
    std::string actual(buffer, result.bytes_written);
    
    // Due to the additional safety margin, the actual written bytes may not match buffer_size exactly
    // Instead, we'll check that the content is correct (header + message + trailer)
    EXPECT_EQ(actual, expected);
}

// -----------------------------------------------------------------------------
// MixedValidAndInvalidMessages: Test when one message is too large and skipped.
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherAdditionalTest, MixedValidAndInvalidMessages) {
    std::string valid1 = "valid1";
    // Create an invalid message (length > GetMaxMessageSize_ which is 1024)
    std::string invalid(1500, 'X');
    std::string valid2 = "valid2";

    message_queue->enqueue(valid1.c_str(), valid1.length());
    message_queue->enqueue(invalid.c_str(), invalid.length());
    message_queue->enqueue(valid2.c_str(), valid2.length());

    TestMessageBatcher batcher(5, 1000);
    char buffer[1024];
    auto result = batcher.BatchEvents(message_queue, buffer, sizeof(buffer));

    // Expect only the valid messages to be batched (skipping the invalid one)
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 2);

    std::string batch(buffer, result.bytes_written);
    EXPECT_NE(batch.find("valid1"), std::string::npos);
    EXPECT_NE(batch.find("valid2"), std::string::npos);
    // The invalid message should not appear in the batch.
    EXPECT_EQ(batch.find(invalid.substr(0, 10)), std::string::npos);
}

// -----------------------------------------------------------------------------
// HeaderFailureTest: When header generation fails, expect BufferTooSmall.
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherAdditionalTest, HeaderFailureTest) {
    TestMessageBatcherFailHeader batcher(5, 1000);
    std::string msg = "test message";
    message_queue->enqueue(msg.c_str(), msg.length());

    char buffer[1024];
    auto result = batcher.BatchEvents(message_queue, buffer, sizeof(buffer));

    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::BufferTooSmall);
    EXPECT_EQ(result.messages_batched, 0);
}

// -----------------------------------------------------------------------------
// TrailerFailureTest: When trailer generation fails, expect BufferTooSmall.
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherAdditionalTest, TrailerFailureTest) {
    TestMessageBatcherFailTrailer batcher(5, 1000);
    std::string msg = "test message";
    message_queue->enqueue(msg.c_str(), msg.length());

    char buffer[1024];
    auto result = batcher.BatchEvents(message_queue, buffer, sizeof(buffer));

    // Trailer failure forces the batch to be incomplete.
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::BufferTooSmall);
}

// -----------------------------------------------------------------------------
// SeparatorFailureTest: When separator addition fails, only the first message
// should be batched.
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherAdditionalTest, SeparatorFailureTest) {
    TestMessageBatcherFailSeparator batcher(5, 1000);
    std::string msg1 = "first";
    std::string msg2 = "second";

    // Enqueue two messages so that a separator is needed for the second.
    message_queue->enqueue(msg1.c_str(), msg1.length());
    message_queue->enqueue(msg2.c_str(), msg2.length());

    char buffer[1024];
    auto result = batcher.BatchEvents(message_queue, buffer, sizeof(buffer));

    // Expect that only the first message is batched (separator fails on second call).
    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::Success);
    EXPECT_EQ(result.messages_batched, 1);

    std::string expected = "[BATCH_START]" + msg1 + "[BATCH_END]";
    EXPECT_EQ(std::string(buffer, result.bytes_written), expected);
}

// -----------------------------------------------------------------------------
// ExceptionTest: If a virtual method throws an exception, BatchEvents should
// catch it and return InvalidBuffer.
// -----------------------------------------------------------------------------
TEST_F(MessageBatcherAdditionalTest, ExceptionTest) {
    TestMessageBatcherThrowHeader batcher(5, 1000);
    std::string msg = "test message";
    message_queue->enqueue(msg.c_str(), msg.length());

    char buffer[1024];
    auto result = batcher.BatchEvents(message_queue, buffer, sizeof(buffer));

    EXPECT_EQ(result.status, MessageBatcher::BatchResult::Status::InvalidBuffer);
    EXPECT_EQ(result.messages_batched, 0);
}
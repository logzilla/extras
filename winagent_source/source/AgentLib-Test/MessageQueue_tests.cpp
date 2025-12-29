#include "pch.h"
#include "../AgentLib/MessageQueue.h"
#include "MessageQueueTestExtensions.h"

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring> // for strlen

using namespace Syslog_agent;
using namespace std;

class MessageQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create queue with initial sizes
        queue = std::make_unique<MessageQueue>(10, 20);  // 10 messages, 20 buffers initially
    }

    void TearDown() override {
        queue.reset();
    }

    std::unique_ptr<MessageQueue> queue;
};

// Test empty queue behavior
TEST_F(MessageQueueTest, NewQueueIsEmpty) {
    EXPECT_TRUE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 0);
    EXPECT_EQ(queue->getOldestMessageTimestamp(), 0);
}

// Test basic enqueue/peek/dequeue operations
TEST_F(MessageQueueTest, BasicEnqueueAndDequeue) {
    const char* testMessage = "Test Message";
    const uint32_t msgLen = static_cast<uint32_t>(strlen(testMessage));

    // Test enqueue
    EXPECT_TRUE(queue->enqueue(testMessage, msgLen));
    EXPECT_FALSE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 1);

    // Test peek
    char peekBuffer[100] = { 0 };
    int peekedLen = queue->peek(nullptr, peekBuffer, sizeof(peekBuffer));
    EXPECT_EQ(peekedLen, msgLen);
    EXPECT_STREQ(peekBuffer, testMessage);

    // Test dequeue
    char dequeueBuffer[100] = { 0 };
    int dequeueLen = queue->dequeue(dequeueBuffer, sizeof(dequeueBuffer));
    EXPECT_EQ(dequeueLen, msgLen);
    EXPECT_STREQ(dequeueBuffer, testMessage);

    // Queue should be empty after dequeue
    EXPECT_TRUE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 0);
}

// Test enqueue with invalid parameters
TEST_F(MessageQueueTest, EnqueueInvalidParams) {
    // Null content
    EXPECT_FALSE(queue->enqueue(nullptr, 10));

    // Zero length
    EXPECT_FALSE(queue->enqueue("test", 0));

    // Too long message
    const uint32_t tooLong = MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS_PER_MESSAGE;
    std::string longMsg(tooLong, 'A');
    EXPECT_FALSE(queue->enqueue(longMsg.c_str(), static_cast<uint32_t>(longMsg.length())));
}

// Test multiple messages
TEST_F(MessageQueueTest, MultipleMessages) {
    const char* messages[] = { "First", "Second", "Third" };

    // Enqueue all messages
    for (const char* msg : messages) {
        EXPECT_TRUE(queue->enqueue(msg, static_cast<uint32_t>(strlen(msg))));
    }

    EXPECT_EQ(queue->length(), 3);

    // Dequeue and verify order
    char buffer[100];
    for (const char* expected : messages) {
        int len = queue->dequeue(buffer, sizeof(buffer));
        EXPECT_EQ(len, static_cast<int>(strlen(expected)));
        EXPECT_STREQ(buffer, expected);
    }
}

// Test timestamp behavior
TEST_F(MessageQueueTest, MessageTimestamp) {
    const char* msg = "Test";
    EXPECT_TRUE(queue->enqueue(msg, static_cast<uint32_t>(strlen(msg))));

    int64_t timestamp = queue->getOldestMessageTimestamp();
    EXPECT_GT(timestamp, 0);  // Should have a valid timestamp
}

// Test waiting for messages
TEST_F(MessageQueueTest, WaitForMessages) {
    // Should timeout when empty
    EXPECT_FALSE(queue->waitForMessages(100));  // 100ms timeout

    // Add a message
    const char* msg = "Test";
    EXPECT_TRUE(queue->enqueue(msg, static_cast<uint32_t>(strlen(msg))));

    // Should return immediately when message is available
    EXPECT_TRUE(queue->waitForMessages(100));
}

// Test shutdown behavior
TEST_F(MessageQueueTest, ShutdownBehavior) {
    const char* msg = "Test";
    EXPECT_TRUE(queue->enqueue(msg, static_cast<uint32_t>(strlen(msg))));

    queue->beginShutdown();
    EXPECT_TRUE(queue->isShuttingDown());
    EXPECT_TRUE(queue->isEmpty());  // Should report as empty during shutdown
    EXPECT_EQ(queue->length(), 0);  // Should report length 0 during shutdown
}

// -----------------------------------------------------------------------------
// Fixture for additional MessageQueue tests
// -----------------------------------------------------------------------------
class MessageQueueAdditionalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize a message queue with moderate pool sizes.
        queue = std::make_unique<MessageQueue>(50, 100);
    }
    void TearDown() override {
        queue.reset();
    }

    std::unique_ptr<MessageQueue> queue;
};

// -----------------------------------------------------------------------------
// Test concurrent producer and consumer threads, ensuring order preservation.
// -----------------------------------------------------------------------------
TEST_F(MessageQueueAdditionalTest, ConcurrentProducerConsumer) {
    const int num_messages = 100;
    vector<string> messages;
    for (int i = 0; i < num_messages; ++i) {
        messages.push_back("msg" + to_string(i));
    }

    // Producer thread: enqueue all messages.
    thread producer([&]() {
        for (const auto& msg : messages) {
            // Retry until enqueue succeeds.
            while (!queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.length()))) {
                this_thread::yield();
            }
            // Simulate work.
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    });

    // Consumer thread: dequeue messages until all are consumed.
    vector<string> dequeued;
    atomic<int> count{ 0 };
    thread consumer([&]() {
        while (count < num_messages) {
            char buffer[2048] = { 0 };
            int len = queue->dequeue(buffer, sizeof(buffer));
            if (len > 0) {
                dequeued.push_back(string(buffer, len));
                count++;
            }
            else {
                // If queue is empty, wait briefly.
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify that the dequeued messages are in FIFO order.
    ASSERT_EQ(dequeued.size(), messages.size());
    for (size_t i = 0; i < messages.size(); ++i) {
        EXPECT_EQ(dequeued[i], messages[i]);
    }
}

// -----------------------------------------------------------------------------
// Test that multiple peeks return the same message without removal.
// -----------------------------------------------------------------------------
TEST_F(MessageQueueAdditionalTest, MultiplePeeksSameMessage) {
    string msg = "peek_test";
    EXPECT_TRUE(queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.length())));

    char buffer1[1024] = { 0 };
    char buffer2[1024] = { 0 };
    int len1 = queue->peek(nullptr, buffer1, sizeof(buffer1));
    int len2 = queue->peek(nullptr, buffer2, sizeof(buffer2));

    EXPECT_EQ(len1, static_cast<int>(msg.length()));
    EXPECT_EQ(len2, static_cast<int>(msg.length()));
    EXPECT_STREQ(buffer1, msg.c_str());
    EXPECT_STREQ(buffer2, msg.c_str());

    // Message should still be in the queue.
    EXPECT_EQ(queue->length(), 1);
}

// -----------------------------------------------------------------------------
// Stress test: Enqueue and then dequeue a large number of messages.
// -----------------------------------------------------------------------------
TEST_F(MessageQueueAdditionalTest, StressTestLargeNumberOfMessages) {
    const int num_messages = 1000;
    vector<string> messages;
    for (int i = 0; i < num_messages; ++i) {
        messages.push_back("stress_msg_" + to_string(i));
    }

    // Enqueue all messages.
    for (const auto& msg : messages) {
        EXPECT_TRUE(queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.length())));
    }
    EXPECT_EQ(queue->length(), num_messages);

    // Dequeue and verify ordering.
    for (int i = 0; i < num_messages; ++i) {
        char buffer[4096] = { 0 };
        int len = queue->dequeue(buffer, sizeof(buffer));
        EXPECT_GT(len, 0);
        EXPECT_EQ(string(buffer, len), messages[i]);
    }
    // Queue should be empty after all dequeues.
    EXPECT_TRUE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 0);
}

// -----------------------------------------------------------------------------
// Test waitForMessages: A delayed enqueue should satisfy a waiting consumer.
// -----------------------------------------------------------------------------
TEST_F(MessageQueueAdditionalTest, WaitForMessagesAfterDelay) {
    // Start a thread that enqueues a message after a short delay.
    thread delayed_producer([&]() {
        this_thread::sleep_for(chrono::milliseconds(100));
        string msg = "delayed";
        queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.length()));
    });

    // Wait for a message (timeout longer than the delay).
    bool messageAvailable = queue->waitForMessages(500);
    EXPECT_TRUE(messageAvailable);

    // Dequeue the message and verify its content.
    char buffer[1024] = { 0 };
    int len = queue->dequeue(buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_STREQ(buffer, "delayed");

    delayed_producer.join();
}

// -----------------------------------------------------------------------------
// Test Shutdown: After beginShutdown(), the queue should report empty and
// dequeue operations should fail.
// -----------------------------------------------------------------------------
TEST_F(MessageQueueAdditionalTest, ShutdownDuringOperation) {
    vector<string> messages = { "msg1", "msg2", "msg3" };
    for (const auto& msg : messages) {
        EXPECT_TRUE(queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.length())));
    }

    // Begin shutdown.
    queue->beginShutdown();

    // The queue should now be treated as empty.
    EXPECT_TRUE(queue->isEmpty());
    EXPECT_EQ(queue->length(), 0);

    // Dequeue should fail.
    char buffer[1024] = { 0 };
    int len = queue->dequeue(buffer, sizeof(buffer));
    EXPECT_EQ(len, -1);
}

// -----------------------------------------------------------------------------
// Test peek with invalid parameters returns an error.
// -----------------------------------------------------------------------------
TEST_F(MessageQueueAdditionalTest, PeekInvalidParameters) {
    // Enqueue a valid message.
    string msg = "test_invalid";
    EXPECT_TRUE(queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.length())));

    // Peek with an invalid (null) destination buffer.
    int len = queue->peek(nullptr, nullptr, 0);
    EXPECT_EQ(len, -1);
}

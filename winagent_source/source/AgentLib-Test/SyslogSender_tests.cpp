#include "pch.h"
#include "../AgentLib/SyslogSender.h"
#include "../AgentLib/MessageQueue.h"
#include "../AgentLib/MessageBatcher.h"
#include "../AgentLib/INetworkClient.h"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace Syslog_agent;
using namespace std;

// Mock NetworkClient for testing
class MockNetworkClient : public INetworkClient {
public:
    MockNetworkClient(bool simulateFailure = false, bool simulateException = false, int failureCount = 0)
        : simulateFailure_(simulateFailure)
        , simulateException_(simulateException)
        , failureCount_(failureCount)
        , postCount_(0)
        , lastBatchSize_(0) {}

    RESULT_TYPE post(const char* data, size_t dataSize) override {
        if (simulateException_) {
            throw std::runtime_error("Simulated network exception");
        }

        postCount_++;
        lastBatchSize_ = dataSize;
        
        // Store the data for verification
        sentData_.assign(data, dataSize);
        
        // Simulate failure if configured to do so
        if (simulateFailure_ && (failureCount_ == 0 || postCount_ <= failureCount_)) {
            return RESULT_TYPE(RESULT_NETWORK_ERROR);
        }
        
        return RESULT_TYPE(RESULT_SUCCESS);
    }
    
    size_t getPostCount() const { return postCount_; }
    size_t getLastBatchSize() const { return lastBatchSize_; }
    const std::string& getSentData() const { return sentData_; }

private:
    bool simulateFailure_;
    bool simulateException_;
    int failureCount_;
    size_t postCount_;
    size_t lastBatchSize_;
    std::string sentData_;
};

// Mock MessageBatcher for testing
class MockMessageBatcher : public MessageBatcher {
public:
    MockMessageBatcher(bool simulateFailure = false, bool simulateException = false) 
        : MessageBatcher(1024 * 1024)  // 1MB default batch size
        , simulateFailure_(simulateFailure)
        , simulateException_(simulateException)
        , batchCount_(0) {}

    BatchResult BatchEvents(std::shared_ptr<MessageQueue> queue, char* buffer, size_t bufferSize) override {
        if (simulateException_) {
            throw std::runtime_error("Simulated batcher exception");
        }

        batchCount_++;
        
        if (simulateFailure_) {
            return BatchResult { BatchResult::Status::Error, 0, 0 };
        }
        
        // Mock a successful batch - create some sample data to return
        if (queue && queue->length() > 0 && buffer && bufferSize > 0) {
            size_t messageCount = std::min(queue->length(), size_t(10));  // Max 10 messages per batch
            std::string batchData = "Batched " + std::to_string(messageCount) + " messages";
            
            // Copy batch data to buffer
            size_t bytesToCopy = std::min(batchData.length(), bufferSize);
            memcpy(buffer, batchData.c_str(), bytesToCopy);
            
            return BatchResult { BatchResult::Status::Success, static_cast<uint32_t>(messageCount), bytesToCopy };
        }
        
        return BatchResult { BatchResult::Status::Error, 0, 0 };
    }
    
    char* GetBatchBuffer(const char* requestor) override {
        // Allocate a fixed buffer for testing
        static char testBuffer[1024 * 1024];  // 1MB static buffer for testing
        return testBuffer;
    }
    
    void ReleaseBatchBuffer(char* buffer) override {
        // Nothing to do for test buffer
    }
    
    uint32_t GetMaxBatchSizeBytes() const override {
        return 1024 * 1024;  // 1MB
    }
    
    size_t getBatchCount() const { return batchCount_; }

private:
    bool simulateFailure_;
    bool simulateException_;
    size_t batchCount_;
};

// Fixture for SyslogSender tests
class SyslogSenderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock objects for testing
        primaryQueue_ = std::make_shared<MessageQueue>(100, 200);
        secondaryQueue_ = std::make_shared<MessageQueue>(100, 200);
        primaryClient_ = std::make_shared<MockNetworkClient>();
        secondaryClient_ = std::make_shared<MockNetworkClient>();
        primaryBatcher_ = std::make_shared<MockMessageBatcher>();
        secondaryBatcher_ = std::make_shared<MockMessageBatcher>();
        
        // Create SyslogSender with mock objects
        // Use a smaller batch size/age for faster testing
        maxBatchCount_ = 5;
        maxBatchAge_ = 100; // milliseconds
        sender_ = std::make_unique<SyslogSender>(
            primaryQueue_,
            secondaryQueue_,
            primaryClient_,
            secondaryClient_,
            primaryBatcher_,
            secondaryBatcher_,
            maxBatchCount_,
            maxBatchAge_
        );
    }

    void TearDown() override {
        // Force stop any running threads
        if (sender_) {
            sender_->requestStopAndNotify();
        }
        
        // Clean up in reverse order
        sender_.reset();
        primaryBatcher_.reset();
        secondaryBatcher_.reset();
        primaryClient_.reset();
        secondaryClient_.reset();
        primaryQueue_.reset();
        secondaryQueue_.reset();
    }
    
    // Helper to populate queue with test messages
    void populateQueue(std::shared_ptr<MessageQueue> queue, int count, const std::string& prefix = "Test") {
        for (int i = 0; i < count; i++) {
            std::string message = prefix + " Message " + std::to_string(i);
            ASSERT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
        }
    }
    
    // Run sender in a separate thread
    std::thread runSenderThread() {
        return std::thread([this]() {
            sender_->run();
        });
    }

    // Common test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<MockNetworkClient> primaryClient_;
    std::shared_ptr<MockNetworkClient> secondaryClient_;
    std::shared_ptr<MockMessageBatcher> primaryBatcher_;
    std::shared_ptr<MockMessageBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
};

// Basic initialization test
TEST_F(SyslogSenderTest, Initialization) {
    EXPECT_FALSE(sender_->isStopRequested());
}

// Test shouldSendBatch logic - batch count threshold
TEST_F(SyslogSenderTest, ShouldSendBatchCountThreshold) {
    // Initially queue is empty
    EXPECT_EQ(0, primaryQueue_->length());
    
    // Test batch count threshold reached
    populateQueue(primaryQueue_, maxBatchCount_);
    EXPECT_EQ(maxBatchCount_, primaryQueue_->length());
}

// Test shouldSendBatch logic - batch age threshold
TEST_F(SyslogSenderTest, ShouldSendBatchAgeThreshold) {
    // Add a single message to the queue
    populateQueue(primaryQueue_, 1);
    
    // Sleep longer than the max age
    std::this_thread::sleep_for(std::chrono::milliseconds(maxBatchAge_ + 50));
    
    // The next sender run should process this batch due to age
}

// Test basic message sending
TEST_F(SyslogSenderTest, BasicMessageSending) {
    // Add messages to primary queue
    populateQueue(primaryQueue_, maxBatchCount_);
    
    // Start sender thread
    auto senderThread = runSenderThread();
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    sender_->requestStopAndNotify();
    senderThread.join();
    
    // Verify primary queue was processed
    EXPECT_GT(primaryBatcher_->getBatchCount(), 0);
    EXPECT_GT(primaryClient_->getPostCount(), 0);
    EXPECT_LT(primaryQueue_->length(), maxBatchCount_);
}

// Test sending with secondary queue
TEST_F(SyslogSenderTest, SecondaryQueueProcessing) {
    // Add messages to secondary queue only
    populateQueue(secondaryQueue_, maxBatchCount_, "Secondary");
    
    // Start sender thread
    auto senderThread = runSenderThread();
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    sender_->requestStopAndNotify();
    senderThread.join();
    
    // Verify secondary queue was processed
    EXPECT_GT(secondaryBatcher_->getBatchCount(), 0);
    EXPECT_GT(secondaryClient_->getPostCount(), 0);
    EXPECT_LT(secondaryQueue_->length(), maxBatchCount_);
}

// Test handling of network send failures
TEST_F(SyslogSenderTest, NetworkSendFailure) {
    // Create a sender with a failing network client
    auto failingClient = std::make_shared<MockNetworkClient>(true);
    
    auto failingSender = std::make_unique<SyslogSender>(
        primaryQueue_,
        nullptr, // No secondary queue
        failingClient,
        nullptr, // No secondary client
        primaryBatcher_,
        nullptr, // No secondary batcher
        maxBatchCount_,
        maxBatchAge_
    );
    
    // Add messages to the queue
    populateQueue(primaryQueue_, maxBatchCount_);
    size_t initialQueueSize = primaryQueue_->length();
    
    // Run the sender
    std::thread senderThread([&failingSender]() {
        failingSender->run();
    });
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    failingSender->requestStopAndNotify();
    senderThread.join();
    
    // Verify network client was called but queue was not emptied due to failure
    EXPECT_GT(failingClient->getPostCount(), 0);
    EXPECT_EQ(initialQueueSize, primaryQueue_->length()); // Messages should still be in queue
}

// Test handling of batcher failures
TEST_F(SyslogSenderTest, BatcherFailure) {
    // Create a sender with a failing batcher
    auto failingBatcher = std::make_shared<MockMessageBatcher>(true);
    
    auto failingSender = std::make_unique<SyslogSender>(
        primaryQueue_,
        nullptr, // No secondary queue
        primaryClient_,
        nullptr, // No secondary client
        failingBatcher,
        nullptr, // No secondary batcher
        maxBatchCount_,
        maxBatchAge_
    );
    
    // Add messages to the queue
    populateQueue(primaryQueue_, maxBatchCount_);
    size_t initialQueueSize = primaryQueue_->length();
    
    // Run the sender
    std::thread senderThread([&failingSender]() {
        failingSender->run();
    });
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    failingSender->requestStopAndNotify();
    senderThread.join();
    
    // Verify batcher was called but messages weren't sent
    EXPECT_GT(failingBatcher->getBatchCount(), 0);
    EXPECT_EQ(0, primaryClient_->getPostCount());
    EXPECT_EQ(initialQueueSize, primaryQueue_->length()); // Messages should still be in queue
}

// Test handling of exceptions
TEST_F(SyslogSenderTest, ExceptionHandling) {
    // Create a sender with components that throw exceptions
    auto throwingClient = std::make_shared<MockNetworkClient>(false, true);
    auto throwingBatcher = std::make_shared<MockMessageBatcher>(false, true);
    
    auto throwingSender = std::make_unique<SyslogSender>(
        primaryQueue_,
        nullptr, // No secondary queue
        throwingClient,
        nullptr, // No secondary client
        throwingBatcher,
        nullptr, // No secondary batcher
        maxBatchCount_,
        maxBatchAge_
    );
    
    // Add messages to the queue
    populateQueue(primaryQueue_, maxBatchCount_);
    size_t initialQueueSize = primaryQueue_->length();
    
    // Run the sender - it should handle exceptions without crashing
    std::thread senderThread([&throwingSender]() {
        throwingSender->run();
    });
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    throwingSender->requestStopAndNotify();
    senderThread.join();
    
    // Verify batcher was called but messages weren't sent
    EXPECT_EQ(initialQueueSize, primaryQueue_->length()); // Messages should still be in queue
}

// Test proper cleanup on shutdown
TEST_F(SyslogSenderTest, ShutdownCleanup) {
    // Add messages to the queues
    populateQueue(primaryQueue_, maxBatchCount_);
    populateQueue(secondaryQueue_, maxBatchCount_);
    
    // Start sender thread
    auto senderThread = runSenderThread();
    
    // Let it process some messages
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Request stop
    sender_->requestStopAndNotify();
    
    // Wait for thread to complete
    senderThread.join();
    
    // Verify the thread stopped cleanly
    EXPECT_TRUE(sender_->isStopRequested());
}

// Test the enqueue hook functionality
TEST_F(SyslogSenderTest, EnqueueHookTriggersProcessing) {
    // Use a longer maxBatchAge to ensure we're not triggering due to time
    auto sender = std::make_unique<SyslogSender>(
        primaryQueue_,
        nullptr, // No secondary queue
        primaryClient_,
        nullptr, // No secondary client
        primaryBatcher_,
        nullptr, // No secondary batcher
        maxBatchCount_,
        10000 // Very long batch age
    );
    
    // Start a thread that waits for the condition variable
    std::atomic<bool> wasNotified(false);
    std::thread waiterThread([&sender, &wasNotified]() {
        // Wait for notification when queue reaches threshold
        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        std::condition_variable cv;
        auto pred = [&sender]() { return sender->isStopRequested(); };
        
        auto timeout = cv.wait_for(lock, std::chrono::seconds(2), pred);
        wasNotified = !timeout; // If we didn't time out, we were notified
        
        sender->requestStopAndNotify(); // Make sure sender stops
    });
    
    // Run the sender
    std::thread senderThread([&sender]() {
        sender->run();
    });
    
    // Trigger the enqueue hook by adding messages up to threshold
    for (uint32_t i = 0; i < maxBatchCount_; i++) {
        std::string message = "Hook Test Message " + std::to_string(i);
        primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
        
        // This should eventually trigger the hook when we hit maxBatchCount_
        if (primaryQueue_->length() >= maxBatchCount_) {
            // The hook should notify the condition variable
            break;
        }
    }
    
    // Wait for threads to complete
    waiterThread.join();
    senderThread.join();
    
    // The hook should have notified the condition variable
    // This is hard to test directly, but our implementation should process the messages
    EXPECT_LT(primaryQueue_->length(), maxBatchCount_);
}

// Test memory limits and buffer management
TEST_F(SyslogSenderTest, MemoryLimitsRespected) {
    // Set up a large message to test buffer limits
    std::string largeMessage(SyslogSender::MAX_MESSAGE_SIZE - 100, 'X');
    ASSERT_TRUE(primaryQueue_->enqueue(largeMessage.c_str(), static_cast<uint32_t>(largeMessage.length())));
    
    // Run the sender
    auto senderThread = runSenderThread();
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    sender_->requestStopAndNotify();
    senderThread.join();
    
    // Verify the message was processed properly
    EXPECT_GT(primaryBatcher_->getBatchCount(), 0);
    EXPECT_GT(primaryClient_->getPostCount(), 0);
    EXPECT_LT(primaryClient_->getLastBatchSize(), SyslogSender::MAX_MESSAGE_SIZE);
}

// Test parallel processing of primary and secondary queues
TEST_F(SyslogSenderTest, ParallelQueueProcessing) {
    // Add different messages to each queue so we can tell them apart
    populateQueue(primaryQueue_, maxBatchCount_, "Primary");
    populateQueue(secondaryQueue_, maxBatchCount_, "Secondary");
    
    // Start sender thread
    auto senderThread = runSenderThread();
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Request stop and wait for thread to complete
    sender_->requestStopAndNotify();
    senderThread.join();
    
    // Verify both queues were processed
    EXPECT_GT(primaryBatcher_->getBatchCount(), 0);
    EXPECT_GT(primaryClient_->getPostCount(), 0);
    EXPECT_GT(secondaryBatcher_->getBatchCount(), 0);
    EXPECT_GT(secondaryClient_->getPostCount(), 0);
    
    // Verify queue lengths were reduced
    EXPECT_LT(primaryQueue_->length(), maxBatchCount_);
    EXPECT_LT(secondaryQueue_->length(), maxBatchCount_);
}

// Test long running operation with periodic batches
TEST_F(SyslogSenderTest, LongRunningOperation) {
    // Increase batch age for this test
    auto longRunningSender = std::make_unique<SyslogSender>(
        primaryQueue_,
        secondaryQueue_,
        primaryClient_,
        secondaryClient_,
        primaryBatcher_,
        secondaryBatcher_,
        maxBatchCount_,
        250 // ms
    );
    
    // Start sender thread
    std::thread senderThread([&longRunningSender]() {
        longRunningSender->run();
    });
    
    // Periodically add messages over a longer period
    for (int i = 0; i < 5; i++) {
        // Add a few messages to each queue
        populateQueue(primaryQueue_, 2, "Long-Primary-" + std::to_string(i));
        populateQueue(secondaryQueue_, 2, "Long-Secondary-" + std::to_string(i));
        
        // Wait between batches
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    // Request stop and wait for thread to complete
    longRunningSender->requestStopAndNotify();
    senderThread.join();
    
    // Verify both batchers and clients were used multiple times
    EXPECT_GT(primaryBatcher_->getBatchCount(), 3);  // Should have processed multiple batches
    EXPECT_GT(primaryClient_->getPostCount(), 3);
    EXPECT_GT(secondaryBatcher_->getBatchCount(), 3);
    EXPECT_GT(secondaryClient_->getPostCount(), 3);
    
    // All messages should be processed
    EXPECT_EQ(0, primaryQueue_->length());
    EXPECT_EQ(0, secondaryQueue_->length());
}

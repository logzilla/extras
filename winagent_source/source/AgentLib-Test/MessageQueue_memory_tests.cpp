#include "pch.h"
#include "../AgentLib/MessageQueue.h"
#include "MessageQueueTestExtensions.h"

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <random>

using namespace Syslog_agent;
using namespace std;

// Test fixture for memory-specific tests
class MessageQueueMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize with small pool sizes to more easily trigger growth
        queue = std::make_unique<MessageQueue>(5, 10);
    }

    void TearDown() override {
        queue.reset();
    }

    // Helper to generate a string of specific size
    string generateString(size_t size) {
        string result(size, 'A');
        for (size_t i = 0; i < size; i += 8) {
            size_t remain = min(size - i, size_t(8));
            snprintf(&result[i], remain, "%07zu", i);
        }
        return result;
    }

    // Helper to track memory usage
    size_t getCurrentMemoryUsage() {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
    }

    std::unique_ptr<MessageQueue> queue;
};

// Test for unbounded queue growth with slow consumer
TEST_F(MessageQueueMemoryTest, UnboundedQueueGrowthWithSlowConsumer) {
    // Track initial memory
    size_t initialMemory = getCurrentMemoryUsage();
    
    const int MESSAGE_COUNT = 10000;
    const size_t MESSAGE_SIZE = 1000;
    
    std::atomic<bool> stopProducer{false};
    std::atomic<int> messagesProduced{0};
    std::atomic<int> messagesConsumed{0};
    
    // Producer thread runs at max speed
    thread producer([&]() {
        string message = generateString(MESSAGE_SIZE);
        
        while (!stopProducer && messagesProduced < MESSAGE_COUNT) {
            if (queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()))) {
                messagesProduced++;
            }
            else {
                // Brief yield if enqueue fails
                this_thread::yield();
            }
        }
    });
    
    // Consumer thread runs slowly
    thread consumer([&]() {
        char buffer[4096];
        while (messagesConsumed < MESSAGE_COUNT) {
            int result = queue->dequeue(buffer, sizeof(buffer));
            if (result > 0) {
                messagesConsumed++;
                
                // Simulate slow consumer by sleeping
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        }
    });
    
    // Let it run for a while
    this_thread::sleep_for(chrono::seconds(2));
    
    // Check queue length mid-test
    uint32_t queueLengthMid = queue->length();
    size_t midMemory = getCurrentMemoryUsage();
    
    // Let the test finish
    stopProducer = true;
    producer.join();
    consumer.join();
    
    // Verify memory usage
    size_t finalMemory = getCurrentMemoryUsage();
    
    // Log memory usage for analysis
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Mid-test memory: " << midMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    cout << "Queue length mid-test: " << queueLengthMid << endl;
    cout << "Messages produced: " << messagesProduced << endl;
    cout << "Messages consumed: " << messagesConsumed << endl;
    
    // Verify that mid-test, queue had accumulated messages (producer faster than consumer)
    EXPECT_GT(queueLengthMid, 0u);
    
    // Verify that once test completes, memory usage returns close to initial state
    // Allow some tolerance for memory usage differences
    const double MEMORY_TOLERANCE_FACTOR = 1.5;  // 50% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

// Test pool growth and shrinking with burst traffic
TEST_F(MessageQueueMemoryTest, PoolGrowthAndShrinking) {
    // Track initial memory
    size_t initialMemory = getCurrentMemoryUsage();
    
    const int BURST_SIZE = 500;
    const int BURSTS = 5;
    const size_t MESSAGE_SIZE = 1000;
    
    // Generate test message
    string message = generateString(MESSAGE_SIZE);
    
    size_t peakMemory = initialMemory;
    
    for (int burst = 0; burst < BURSTS; burst++) {
        // Enqueue a burst of messages
        for (int i = 0; i < BURST_SIZE; i++) {
            ASSERT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
        }
        
        // Check memory after burst
        size_t afterBurstMemory = getCurrentMemoryUsage();
        peakMemory = max(peakMemory, afterBurstMemory);
        
        cout << "Memory after burst " << burst + 1 << ": " << afterBurstMemory << " bytes" << endl;
        
        // Dequeue all messages
        char buffer[4096];
        for (int i = 0; i < BURST_SIZE; i++) {
            ASSERT_GT(queue->dequeue(buffer, sizeof(buffer)), 0);
        }
        
        // Queue should be empty now
        ASSERT_TRUE(queue->isEmpty());
        
        // Force potential pool shrinking by running GC
        for (int i = 0; i < 10; i++) {
            // This has no direct call to force pool shrinking,
            // but we can try to allocate/deallocate to trigger internal shrinking
            for (int j = 0; j < 10; j++) {
                queue->enqueue("trigger shrink", 14);
            }
            
            for (int j = 0; j < 10; j++) {
                queue->dequeue(buffer, sizeof(buffer));
            }
        }
        
        // Check memory after dequeuing
        size_t afterDequeueMemory = getCurrentMemoryUsage();
        cout << "Memory after dequeue and GC " << burst + 1 << ": " << afterDequeueMemory << " bytes" << endl;
    }
    
    // Verify that final memory usage is less than peak
    size_t finalMemory = getCurrentMemoryUsage();
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Peak memory: " << peakMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    // The pools should have shrunk, but might not be back to initial size due to slack
    EXPECT_LT(finalMemory, peakMemory);
}

// Test large message handling
TEST_F(MessageQueueMemoryTest, LargeMessageHandling) {
    // This test verifies correct handling of messages approaching the maximum allowed size
    
    // Calculate sizes relative to the MessageQueue limits
    const uint32_t BUFFER_SIZE = MessageQueue::MESSAGE_BUFFER_SIZE;
    const uint32_t MAX_BUFFERS = MessageQueue::MAX_BUFFERS_PER_MESSAGE;
    
    // Test different sizes
    vector<uint32_t> messageSizes = {
        BUFFER_SIZE - 1,                  // Just under 1 buffer
        BUFFER_SIZE,                      // Exactly 1 buffer
        BUFFER_SIZE + 1,                  // Just over 1 buffer
        BUFFER_SIZE * 2 - 1,              // Just under 2 buffers
        BUFFER_SIZE * (MAX_BUFFERS - 1),  // Second to last buffer
        BUFFER_SIZE * MAX_BUFFERS - 1,    // Maximum allowed size - 1
        BUFFER_SIZE * MAX_BUFFERS,        // Maximum allowed size
        BUFFER_SIZE * MAX_BUFFERS + 1     // Exceeds maximum size
    };
    
    for (uint32_t size : messageSizes) {
        // Generate message of specific size
        string message = generateString(size);
        
        // Track memory before enqueue
        size_t beforeMemory = getCurrentMemoryUsage();
        
        // Try to enqueue
        bool success = queue->enqueue(message.c_str(), size);
        
        // Track memory after enqueue
        size_t afterMemory = getCurrentMemoryUsage();
        
        cout << "Message size: " << size << " bytes, Enqueue success: " << success
             << ", Memory diff: " << (afterMemory > beforeMemory ? afterMemory - beforeMemory : 0) << " bytes" << endl;
        
        // Verify expected behavior
        if (size <= BUFFER_SIZE * MAX_BUFFERS) {
            EXPECT_TRUE(success);
            
            // Dequeue and verify content
            char* buffer = new char[size + 1];
            memset(buffer, 0, size + 1);
            
            int result = queue->dequeue(buffer, size + 1);
            EXPECT_EQ(result, static_cast<int>(size));
            
            // Verify content
            EXPECT_EQ(string(buffer, min(size, 10u)), message.substr(0, min(size, 10u)));
            
            delete[] buffer;
            
            // Track memory after dequeue
            size_t afterDequeueMemory = getCurrentMemoryUsage();
            cout << "  After dequeue: " << afterDequeueMemory << " bytes, Diff from start: " 
                 << (afterDequeueMemory > beforeMemory ? afterDequeueMemory - beforeMemory : 0) << " bytes" << endl;
        }
        else {
            // Messages exceeding maximum size should be rejected
            EXPECT_FALSE(success);
        }
    }
}

// Test for memory leaks with error conditions
TEST_F(MessageQueueMemoryTest, ErrorConditionMemoryLeaks) {
    // Track starting memory
    size_t initialMemory = getCurrentMemoryUsage();
    
    // Test memory under various error conditions
    
    // 1. Dequeue from empty queue
    char buffer[4096];
    EXPECT_EQ(queue->dequeue(buffer, sizeof(buffer)), -1);
    
    // 2. Peek with invalid buffer
    const char* testMessage = "Test Message";
    uint32_t msgLen = static_cast<uint32_t>(strlen(testMessage));
    EXPECT_TRUE(queue->enqueue(testMessage, msgLen));
    
    EXPECT_EQ(queue->peek(nullptr, nullptr, 100), -1);
    
    // 3. Dequeue with buffer too small
    char smallBuffer[5];
    EXPECT_EQ(queue->dequeue(smallBuffer, sizeof(smallBuffer)), -1);
    
    // Actually dequeue the message to clean up
    EXPECT_GT(queue->dequeue(buffer, sizeof(buffer)), 0);
    
    // 4. Enqueue null pointer
    EXPECT_FALSE(queue->enqueue(nullptr, 10));
    
    // 5. Enqueue zero length
    EXPECT_FALSE(queue->enqueue("test", 0));
    
    // 6. Enqueue message too large
    string largeMsg(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS + 1, 'X');
    EXPECT_FALSE(queue->enqueue(largeMsg.c_str(), static_cast<uint32_t>(largeMsg.length())));
    
    // Verify final memory usage
    size_t finalMemory = getCurrentMemoryUsage();
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    // Allow some tolerance for memory usage differences
    const double MEMORY_TOLERANCE_FACTOR = 1.2;  // 20% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

// Test message buffer chaining and release
TEST_F(MessageQueueMemoryTest, MessageBufferChaining) {
    // Test proper handling of multi-buffer messages
    
    // Create messages of sizes that require multiple buffers
    vector<uint32_t> messageSizes = {
        MessageQueue::MESSAGE_BUFFER_SIZE + 1,      // 2 buffers
        MessageQueue::MESSAGE_BUFFER_SIZE * 2 + 1,  // 3 buffers
        MessageQueue::MESSAGE_BUFFER_SIZE * 3 + 1   // 4 buffers
    };
    
    size_t initialMemory = getCurrentMemoryUsage();
    
    // Enqueue and then dequeue messages of varying sizes
    for (uint32_t size : messageSizes) {
        string message = generateString(size);
        
        // Track memory before operation
        size_t beforeMemory = getCurrentMemoryUsage();
        
        // Enqueue message
        ASSERT_TRUE(queue->enqueue(message.c_str(), size));
        
        size_t afterEnqueueMemory = getCurrentMemoryUsage();
        
        // Estimated buffer count
        int expectedBuffers = (size + MessageQueue::MESSAGE_BUFFER_SIZE - 1) / MessageQueue::MESSAGE_BUFFER_SIZE;
        
        cout << "Size: " << size << ", Expected buffers: " << expectedBuffers
             << ", Memory change: " << (afterEnqueueMemory - beforeMemory) << " bytes" << endl;
        
        // Dequeue message
        char* buffer = new char[size + 1];
        memset(buffer, 0, size + 1);
        
        int result = queue->dequeue(buffer, size + 1);
        EXPECT_EQ(result, static_cast<int>(size));
        
        // Verify content
        EXPECT_EQ(string(buffer, min(size, 10u)), message.substr(0, min(size, 10u)));
        
        delete[] buffer;
        
        // Check memory after dequeue
        size_t afterDequeueMemory = getCurrentMemoryUsage();
        cout << "  After dequeue: " << afterDequeueMemory << " bytes, Diff from before: " 
             << (afterDequeueMemory > beforeMemory ? afterDequeueMemory - beforeMemory : 0) << " bytes" << endl;
    }
    
    // Final memory check
    size_t finalMemory = getCurrentMemoryUsage();
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    // Memory should not have grown significantly
    const double MEMORY_TOLERANCE_FACTOR = 1.5;  // 50% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

// Test multiple concurrent producers with different message sizes
TEST_F(MessageQueueMemoryTest, ConcurrentProducersWithVaryingSizes) {
    const int PRODUCER_COUNT = 4;
    const int MESSAGES_PER_PRODUCER = 1000;
    
    // Define message size patterns for different producers
    vector<vector<uint32_t>> producerSizePatterns(PRODUCER_COUNT);
    
    // Producer 1: Small messages (< 100 bytes)
    for (int i = 0; i < MESSAGES_PER_PRODUCER; i++) {
        producerSizePatterns[0].push_back(10 + (i % 90));
    }
    
    // Producer 2: Medium messages (500-1000 bytes)
    for (int i = 0; i < MESSAGES_PER_PRODUCER; i++) {
        producerSizePatterns[1].push_back(500 + (i % 500));
    }
    
    // Producer 3: Large messages (1500-2000 bytes)
    for (int i = 0; i < MESSAGES_PER_PRODUCER; i++) {
        producerSizePatterns[2].push_back(1500 + (i % 500));
    }
    
    // Producer 4: Mixed sizes with occasional large messages
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<uint32_t> dist(10, 3000);
    for (int i = 0; i < MESSAGES_PER_PRODUCER; i++) {
        producerSizePatterns[3].push_back(dist(rng));
    }
    
    size_t initialMemory = getCurrentMemoryUsage();
    std::atomic<int> totalEnqueued{0};
    std::atomic<int> totalDequeued{0};
    std::atomic<bool> stopConsumer{false};
    
    // Launch producer threads
    vector<thread> producers;
    for (int p = 0; p < PRODUCER_COUNT; p++) {
        producers.push_back(thread([&, p]() {
            for (uint32_t size : producerSizePatterns[p]) {
                string message = generateString(size);
                if (queue->enqueue(message.c_str(), size)) {
                    totalEnqueued++;
                }
                
                // Small delay to allow other threads to run
                if (p % 2 == 0) {
                    this_thread::sleep_for(chrono::microseconds(50));
                }
            }
        }));
    }
    
    // Launch consumer thread
    thread consumer([&]() {
        vector<char> buffer(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS);
        
        while (!stopConsumer || totalDequeued < totalEnqueued.load()) {
            int result = queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size()));
            if (result > 0) {
                totalDequeued++;
            }
            else {
                // Small yield if no message available
                this_thread::yield();
            }
        }
    });
    
    // Track mid-test memory
    this_thread::sleep_for(chrono::milliseconds(500));
    size_t midMemory = getCurrentMemoryUsage();
    uint32_t midQueueLength = queue->length();
    
    // Wait for producers to finish
    for (auto& p : producers) {
        p.join();
    }
    
    // Allow consumer to finish
    this_thread::sleep_for(chrono::milliseconds(100));
    stopConsumer = true;
    consumer.join();
    
    // Final memory and status check
    size_t finalMemory = getCurrentMemoryUsage();
    
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Mid-test memory: " << midMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    cout << "Mid-test queue length: " << midQueueLength << endl;
    cout << "Total messages enqueued: " << totalEnqueued << endl;
    cout << "Total messages dequeued: " << totalDequeued << endl;
    
    // Verify all messages were processed
    EXPECT_EQ(totalEnqueued, PRODUCER_COUNT * MESSAGES_PER_PRODUCER);
    EXPECT_EQ(totalDequeued, totalEnqueued);
    EXPECT_EQ(queue->length(), 0u);
    
    // Memory should return close to initial state
    const double MEMORY_TOLERANCE_FACTOR = 1.5;  // 50% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

// Test message enqueue hooks and potential reference leaks
TEST_F(MessageQueueMemoryTest, EnqueueHookReferenceLeak) {
    // Test that enqueue hooks don't cause reference leaks
    
    size_t initialMemory = getCurrentMemoryUsage();
    std::atomic<int> hookCallCount{0};
    std::atomic<int> messagesRejectedByHook{0};
    
    // Set an enqueue hook that rejects every 5th message
    queue->setEnqueueHook([&](size_t queue_length, MessageQueue::Message* message, bool is_pre_enqueue) {
        hookCallCount++;
        
        // For every 5th message, reject it in pre-enqueue
        if (is_pre_enqueue && hookCallCount % 5 == 0) {
            messagesRejectedByHook++;
            return false;
        }
        
        // Don't hold references to the message
        return true;
    });
    
    // Enqueue many messages
    const int MESSAGE_COUNT = 1000;
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        string message = "Hook test message " + to_string(i);
        queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
    }
    
    // Check rejection count
    cout << "Hook call count: " << hookCallCount << endl;
    cout << "Messages rejected by hook: " << messagesRejectedByHook << endl;
    
    // We expect about 20% of messages to be rejected
    EXPECT_GT(messagesRejectedByHook, 150);
    
    // Check queue length
    uint32_t queueLength = queue->length();
    cout << "Queue length after enqueues: " << queueLength << endl;
    EXPECT_LT(queueLength, static_cast<uint32_t>(MESSAGE_COUNT)); // Should be less due to rejections
    
    // Dequeue all messages
    char buffer[4096];
    int dequeueCount = 0;
    while (queue->dequeue(buffer, sizeof(buffer)) > 0) {
        dequeueCount++;
    }
    
    cout << "Dequeued message count: " << dequeueCount << endl;
    EXPECT_EQ(queue->length(), 0u);
    
    // Remove the hook
    queue->setEnqueueHook(nullptr);
    
    // Final memory check
    size_t finalMemory = getCurrentMemoryUsage();
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    // Memory should not have grown significantly
    const double MEMORY_TOLERANCE_FACTOR = 1.5;  // 50% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

// Test proper cleanup during shutdown with pending operations
TEST_F(MessageQueueMemoryTest, ShutdownWithPendingOperations) {
    size_t initialMemory = getCurrentMemoryUsage();
    
    // Create a separate queue for this test
    auto testQueue = std::make_unique<MessageQueue>(10, 20);
    
    // Fill it with some messages
    for (int i = 0; i < 100; i++) {
        string message = "Shutdown test " + to_string(i);
        ASSERT_TRUE(testQueue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
    }
    
    EXPECT_EQ(testQueue->length(), 100u);
    
    // Start threads that will be interrupted by shutdown
    std::atomic<bool> consumerRunning{true};
    std::atomic<int> dequeueAttempts{0};
    
    thread consumer([&]() {
        char buffer[4096];
        while (consumerRunning) {
            testQueue->dequeue(buffer, sizeof(buffer));
            dequeueAttempts++;
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    });
    
    thread producer([&]() {
        while (consumerRunning) {
            string message = "Late message";
            testQueue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    });
    
    // Let threads run briefly
    this_thread::sleep_for(chrono::milliseconds(100));
    
    // Begin shutdown while operations are in progress
    testQueue->beginShutdown();
    
    // Signal threads to stop
    consumerRunning = false;
    
    // Wait for threads to finish
    consumer.join();
    producer.join();
    
    // Check queue state
    EXPECT_TRUE(testQueue->isEmpty());
    EXPECT_EQ(testQueue->length(), 0u);
    EXPECT_TRUE(testQueue->isShuttingDown());
    
    cout << "Dequeue attempts during shutdown: " << dequeueAttempts << endl;
    
    // Destroy the queue
    testQueue.reset();
    
    // Final memory check
    size_t finalMemory = getCurrentMemoryUsage();
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    // Memory should not have grown significantly
    const double MEMORY_TOLERANCE_FACTOR = 1.5;  // 50% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

// Extended sustained load test to detect memory creep
TEST_F(MessageQueueMemoryTest, ExtendedLoadMemoryCreep) {
    // This test runs for a longer period to detect subtle memory creep issues
    
    const int CYCLE_COUNT = 10;
    const int MESSAGES_PER_CYCLE = 1000;
    const int MESSAGE_SIZE = 500;
    
    size_t initialMemory = getCurrentMemoryUsage();
    vector<size_t> memoryReadings;
    
    for (int cycle = 0; cycle < CYCLE_COUNT; cycle++) {
        // Enqueue messages
        for (int i = 0; i < MESSAGES_PER_CYCLE; i++) {
            string message = generateString(MESSAGE_SIZE);
            ASSERT_TRUE(queue->enqueue(message.c_str(), MESSAGE_SIZE));
        }
        
        EXPECT_EQ(queue->length(), MESSAGES_PER_CYCLE);
        
        // Take memory reading
        size_t cycleMemory = getCurrentMemoryUsage();
        memoryReadings.push_back(cycleMemory);
        
        // Dequeue all messages
        char buffer[4096];
        for (int i = 0; i < MESSAGES_PER_CYCLE; i++) {
            ASSERT_GT(queue->dequeue(buffer, sizeof(buffer)), 0);
        }
        
        EXPECT_EQ(queue->length(), 0u);
    }
    
    // Output memory readings
    cout << "Memory readings across cycles:" << endl;
    for (size_t i = 0; i < memoryReadings.size(); i++) {
        cout << "  Cycle " << i << ": " << memoryReadings[i] << " bytes" << endl;
    }
    
    // Check for continuous memory growth across cycles
    bool continuousGrowth = true;
    for (size_t i = 2; i < memoryReadings.size(); i++) {
        if (memoryReadings[i] <= memoryReadings[i-2]) {
            continuousGrowth = false;
            break;
        }
    }
    
    // We don't expect continuous growth every cycle
    EXPECT_FALSE(continuousGrowth) << "Memory appears to be continuously growing across cycles";
    
    // Final memory should be reasonable compared to initial
    size_t finalMemory = getCurrentMemoryUsage();
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    const double MEMORY_TOLERANCE_FACTOR = 2.0;  // 100% tolerance for extended test
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR);
}

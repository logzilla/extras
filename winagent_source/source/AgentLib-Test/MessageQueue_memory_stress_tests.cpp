#include "pch.h"
#include "../AgentLib/MessageQueue.h"
#include "MessageQueueTestExtensions.h"

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <algorithm>
#include <cstring>
#include <fstream>

using namespace Syslog_agent;
using namespace std;

// Helper class to track memory usage patterns over time
class MemoryTracker {
public:
    struct MemorySnapshot {
        uint64_t timestamp;
        size_t workingSetSize;
        size_t privateUsage;
        size_t queueLength;
    };

    void takeSnapshot(uint32_t queueLength = 0) {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), 
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), 
                              sizeof(pmc))) {
            auto now = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - startTime).count();
                
            snapshots.push_back({
                static_cast<uint64_t>(ms),
                pmc.WorkingSetSize,
                pmc.PrivateUsage,
                queueLength
            });
        }
    }
    
    void writeToCSV(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return;
        }
        
        file << "Timestamp_ms,WorkingSetSize,PrivateUsage,QueueLength\n";
        for (const auto& snapshot : snapshots) {
            file << snapshot.timestamp << ","
                 << snapshot.workingSetSize << ","
                 << snapshot.privateUsage << ","
                 << snapshot.queueLength << "\n";
        }
    }

    bool hasSignificantMemoryCreep() const {
        if (snapshots.size() < 10) return false;
        
        // Calculate linear regression to detect trend
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        size_t n = snapshots.size();
        
        for (size_t i = 0; i < n; i++) {
            double x = static_cast<double>(i);
            double y = static_cast<double>(snapshots[i].privateUsage);
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
        }
        
        double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
        double averageMemory = sumY / n;
        
        // If slope is positive and significant compared to average memory,
        // we consider it memory creep
        return slope > 0 && (slope * n / averageMemory > 0.2);
    }
    
    size_t getMaxMemoryUsage() const {
        size_t maxUsage = 0;
        for (const auto& snapshot : snapshots) {
            maxUsage = std::max(maxUsage, snapshot.privateUsage);
        }
        return maxUsage;
    }
    
    size_t getFinalMemoryUsage() const {
        return snapshots.empty() ? 0 : snapshots.back().privateUsage;
    }
    
private:
    std::vector<MemorySnapshot> snapshots;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

// Specialized test fixture for memory stress testing
class MessageQueueMemoryStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start with a clean queue with small initial pool sizes
        queue = std::make_unique<MessageQueue>(5, 10);
        memoryTracker.takeSnapshot(0);
    }

    void TearDown() override {
        // Ensure queue is properly cleaned up
        if (queue) {
            queue->beginShutdown();
            queue.reset();
        }
        
        // Take final memory snapshot
        memoryTracker.takeSnapshot(0);
    }
    
    // Helper method to generate message with controlled content
    string generateTestMessage(size_t size, int seed = 0) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<char> dist('A', 'Z');
        
        string message(size, ' ');
        for (size_t i = 0; i < size; i++) {
            message[i] = dist(rng);
        }
        
        // Add some identifiable prefix
        string prefix = "MSG" + std::to_string(seed) + ":";
        if (prefix.length() < size) {
            prefix.copy(&message[0], prefix.length());
        }
        
        return message;
    }

    std::unique_ptr<MessageQueue> queue;
    MemoryTracker memoryTracker;
};

// Test for pool allocation patterns under continuous load/unload cycles
TEST_F(MessageQueueMemoryStressTest, PoolAllocationPatterns) {
    const int TEST_DURATION_SECONDS = 10;
    const int SNAPSHOT_INTERVAL_MS = 250;
    const int MAX_MESSAGE_SIZE = 2000;
    
    std::atomic<bool> running{true};
    std::atomic<int> messagesProduced{0};
    std::atomic<int> messagesConsumed{0};
    
    // Producer thread that generates varying sizes
    thread producer([&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> sizeDist(50, MAX_MESSAGE_SIZE);
        int msgId = 0;
        
        while (running) {
            // Create message with varying size
            int size = sizeDist(rng);
            string message = generateTestMessage(size, msgId++);
            
            if (queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()))) {
                messagesProduced++;
            }
            
            // Small sleep to prevent overwhelming the system
            this_thread::sleep_for(chrono::microseconds(100));
        }
    });
    
    // Consumer thread with variable processing speed
    thread consumer([&]() {
        std::mt19937 rng(43);
        std::uniform_int_distribution<int> delayDist(0, 2000);
        char buffer[MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS];
        
        while (running || !queue->isEmpty()) {
            int result = queue->dequeue(buffer, sizeof(buffer));
            if (result > 0) {
                messagesConsumed++;
                
                // Variable processing time
                this_thread::sleep_for(chrono::microseconds(delayDist(rng)));
            }
            else {
                // Small yield if no message available
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        }
    });
    
    // Monitoring thread to take snapshots
    thread monitor([&]() {
        const auto startTime = std::chrono::steady_clock::now();
        auto nextSnapshotTime = startTime;
        
        while (running) {
            auto now = std::chrono::steady_clock::now();
            if (now >= nextSnapshotTime) {
                memoryTracker.takeSnapshot(queue->length());
                nextSnapshotTime += std::chrono::milliseconds(SNAPSHOT_INTERVAL_MS);
            }
            
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    });
    
    // Let test run for specified duration
    this_thread::sleep_for(chrono::seconds(TEST_DURATION_SECONDS));
    running = false;
    
    // Join all threads
    producer.join();
    consumer.join();
    monitor.join();
    
    // Write memory data to CSV for analysis
    memoryTracker.writeToCSV("pool_allocation_patterns.csv");
    
    // Output statistics
    cout << "Messages produced: " << messagesProduced << endl;
    cout << "Messages consumed: " << messagesConsumed << endl;
    cout << "Final queue length: " << queue->length() << endl;
    cout << "Max memory usage: " << memoryTracker.getMaxMemoryUsage() << " bytes" << endl;
    
    // Check for memory creep
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep()) 
        << "Detected memory creep in pool allocations";
    
    // Queue should be empty after consumer finishes
    EXPECT_EQ(queue->length(), 0u);
    EXPECT_EQ(messagesProduced, messagesConsumed);
}

// Test for memory fragmentation with alternating large and small messages
TEST_F(MessageQueueMemoryStressTest, MemoryFragmentation) {
    const int CYCLE_COUNT = 20;
    const int MESSAGES_PER_CYCLE = 100;
    
    for (int cycle = 0; cycle < CYCLE_COUNT; cycle++) {
        cout << "Fragmentation test cycle " << cycle << endl;
        memoryTracker.takeSnapshot(queue->length());
        
        // First enqueue large messages
        for (int i = 0; i < MESSAGES_PER_CYCLE; i++) {
            // Large messages close to buffer size to force multiple buffers
            size_t size = MessageQueue::MESSAGE_BUFFER_SIZE * 2 - 10;
            string message = generateTestMessage(size, i);
            EXPECT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
        }
        
        memoryTracker.takeSnapshot(queue->length());
        
        // Dequeue half
        char buffer[MessageQueue::MESSAGE_BUFFER_SIZE * 4];
        for (int i = 0; i < MESSAGES_PER_CYCLE / 2; i++) {
            EXPECT_GT(queue->dequeue(buffer, sizeof(buffer)), 0);
        }
        
        memoryTracker.takeSnapshot(queue->length());
        
        // Enqueue many small messages
        for (int i = 0; i < MESSAGES_PER_CYCLE * 4; i++) {
            // Small messages
            size_t size = 50;
            string message = generateTestMessage(size, i + 10000);
            EXPECT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
        }
        
        memoryTracker.takeSnapshot(queue->length());
        
        // Dequeue all messages
        while (queue->dequeue(buffer, sizeof(buffer)) > 0) {
            // Just drain the queue
        }
        
        memoryTracker.takeSnapshot(queue->length());
        
        // Force potential memory cleanup and fragmentation issues
        for (int i = 0; i < 10; i++) {
            string smallMsg = generateTestMessage(20, i);
            queue->enqueue(smallMsg.c_str(), static_cast<uint32_t>(smallMsg.length()));
            queue->dequeue(buffer, sizeof(buffer));
        }
    }
    
    memoryTracker.writeToCSV("memory_fragmentation.csv");
    
    // Queue should be empty after test
    EXPECT_EQ(queue->length(), 0u);
    
    // Check for memory creep, which would indicate fragmentation issues
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep suggesting fragmentation";
}

// Test for buffer reference handling with peek operations
TEST_F(MessageQueueMemoryStressTest, PeekReferenceHandling) {
    const int TEST_COUNT = 1000;
    const int MAX_PEEKS_PER_MESSAGE = 20;
    
    for (int i = 0; i < TEST_COUNT; i++) {
        // Generate and enqueue a message
        size_t size = 500 + (i % 1000);
        string message = generateTestMessage(size, i);
        EXPECT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
        
        // Perform multiple peeks on the same message
        for (int peek = 0; peek < MAX_PEEKS_PER_MESSAGE; peek++) {
            char buffer[4096];
            int result = queue->peek(nullptr, buffer, sizeof(buffer));
            EXPECT_EQ(result, static_cast<int>(message.length()));
            
            // Verify message content (first few bytes)
            EXPECT_EQ(string(buffer, 4), message.substr(0, 4));
        }
        
        // Take memory snapshot every 100 iterations
        if (i % 100 == 0) {
            memoryTracker.takeSnapshot(queue->length());
        }
        
        // Dequeue the message
        char buffer[4096];
        EXPECT_GT(queue->dequeue(buffer, sizeof(buffer)), 0);
    }
    
    memoryTracker.writeToCSV("peek_reference_handling.csv");
    
    // Memory should not grow significantly due to peek operations
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep from peek operations";
}

// Test for proper resource release during random operations
TEST_F(MessageQueueMemoryStressTest, RandomOperationResourceRelease) {
    const int OPERATION_COUNT = 10000;
    
    std::mt19937 rng(44);
    std::uniform_int_distribution<int> opDist(0, 3); // 4 operations
    std::uniform_int_distribution<int> sizeDist(10, 3000);
    
    // Keep track of expected queue state
    int expectedQueueLength = 0;
    
    for (int i = 0; i < OPERATION_COUNT; i++) {
        int op = opDist(rng);
        
        // Take memory snapshot every 1000 operations
        if (i % 1000 == 0) {
            memoryTracker.takeSnapshot(queue->length());
            EXPECT_EQ(queue->length(), static_cast<uint32_t>(expectedQueueLength));
        }
        
        switch (op) {
            case 0: { // Enqueue
                int size = sizeDist(rng);
                // Limit size to max allowed
                size = std::min(size, static_cast<int>(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS - 1));
                string message = generateTestMessage(size, i);
                if (queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()))) {
                    expectedQueueLength++;
                }
                break;
            }
            case 1: { // Dequeue
                char buffer[MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS];
                int result = queue->dequeue(buffer, sizeof(buffer));
                if (result > 0) {
                    expectedQueueLength--;
                }
                break;
            }
            case 2: { // Peek
                if (expectedQueueLength > 0) {
                    char buffer[MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS];
                    queue->peek(nullptr, buffer, sizeof(buffer));
                }
                break;
            }
            case 3: { // RemoveFront
                if (queue->removeFront()) {
                    expectedQueueLength--;
                }
                break;
            }
        }
    }
    
    // Final memory snapshot
    memoryTracker.takeSnapshot(queue->length());
    
    // Check for memory creep
    memoryTracker.writeToCSV("random_operations.csv");
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep during random operations";
    
    // Queue length should match expected
    EXPECT_EQ(queue->length(), static_cast<uint32_t>(expectedQueueLength));
    
    // Clean up remaining messages
    char buffer[MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS];
    while (queue->dequeue(buffer, sizeof(buffer)) > 0) {
        // Just drain the queue
    }
}

// Test for improper shutdown and cleanup
TEST_F(MessageQueueMemoryStressTest, ImproperShutdownCleanup) {
    // Create a series of queues and destroy them without proper shutdown
    size_t initialMemory = memoryTracker.getFinalMemoryUsage();
    
    for (int i = 0; i < 10; i++) {
        // Create a new queue
        auto tempQueue = std::make_unique<MessageQueue>(5, 10);
        
        // Fill it with messages
        for (int j = 0; j < 100; j++) {
            string message = generateTestMessage(500, j);
            tempQueue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
        }
        
        // Destroy the queue without proper shutdown
        tempQueue.reset();
        
        memoryTracker.takeSnapshot(0);
    }
    
    // Check memory usage pattern
    size_t finalMemory = memoryTracker.getFinalMemoryUsage();
    memoryTracker.writeToCSV("improper_shutdown.csv");
    
    cout << "Initial memory: " << initialMemory << " bytes" << endl;
    cout << "Final memory: " << finalMemory << " bytes" << endl;
    
    // Memory should not grow significantly
    const double MEMORY_TOLERANCE_FACTOR = 1.5;  // 50% tolerance
    EXPECT_LT(finalMemory, initialMemory * MEMORY_TOLERANCE_FACTOR)
        << "Memory leak detected in improper shutdown test";
}

// Test threading and synchronization edge cases
TEST_F(MessageQueueMemoryStressTest, ThreadSynchronizationEdgeCases) {
    const int TEST_DURATION_SECONDS = 5;
    
    std::atomic<bool> running{true};
    std::atomic<int> waitTimeouts{0};
    std::atomic<int> spuriousWakeups{0};
    
    // Thread that waits briefly and checks for messages
    thread waiter([&]() {
        while (running) {
            // Wait with a short timeout
            bool msgAvailable = queue->waitForMessages(10);
            
            if (msgAvailable) {
                // If waitForMessages reports a message is available
                if (queue->isEmpty()) {
                    // But queue is empty, this is a spurious wakeup
                    spuriousWakeups++;
                }
                else {
                    // Remove the message
                    queue->removeFront();
                }
            }
            else {
                // Timeout occurred
                waitTimeouts++;
            }
        }
    });
    
    // Thread that sporadically adds and removes messages
    thread sporadic([&]() {
        std::mt19937 rng(45);
        std::uniform_int_distribution<int> delayDist(1, 50);
        std::uniform_int_distribution<int> sizeDist(10, 1000);
        
        while (running) {
            // Add a message
            int size = sizeDist(rng);
            string message = generateTestMessage(size, 0);
            queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
            
            // Small random delay
            this_thread::sleep_for(chrono::milliseconds(delayDist(rng)));
            
            // Try to remove a message (may already be gone)
            queue->removeFront();
            
            // Another small delay
            this_thread::sleep_for(chrono::milliseconds(delayDist(rng)));
            
            // Take memory snapshot occasionally
            static int counter = 0;
            if (++counter % 10 == 0) {
                memoryTracker.takeSnapshot(queue->length());
            }
        }
    });
    
    // Let test run for specified duration
    this_thread::sleep_for(chrono::seconds(TEST_DURATION_SECONDS));
    running = false;
    
    // Join threads
    waiter.join();
    sporadic.join();
    
    // Output statistics
    cout << "Wait timeouts: " << waitTimeouts << endl;
    cout << "Spurious wakeups: " << spuriousWakeups << endl;
    
    // Write memory data
    memoryTracker.writeToCSV("thread_synchronization.csv");
    
    // Check for memory creep
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep in thread synchronization test";
    
    // Clean up any remaining messages
    char buffer[4096];
    while (queue->dequeue(buffer, sizeof(buffer)) > 0) {
        // Just drain the queue
    }
}

// Test for extremely large message handling (near size limits)
TEST_F(MessageQueueMemoryStressTest, ExtremeLargeMessageHandling) {
    // Test with messages approaching the size limits
    
    // Calculate the maximum allowed message size
    const uint32_t MAX_SIZE = MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS;
    
    // Series of test sizes approaching the limit
    vector<uint32_t> testSizes = {
        MAX_SIZE - 100,
        MAX_SIZE - 10,
        MAX_SIZE - 1,
        MAX_SIZE,
        MAX_SIZE + 1  // Should fail
    };
    
    for (uint32_t size : testSizes) {
        cout << "Testing message size: " << size << " bytes" << endl;
        
        // Take memory snapshot before test
        memoryTracker.takeSnapshot(queue->length());
        
        // Generate message of requested size
        std::unique_ptr<char[]> message(new char[size + 1]);
        memset(message.get(), 'X', size);
        message[size] = '\0';
        
        // Try to enqueue
        bool success = queue->enqueue(message.get(), size);
        
        // Take memory snapshot after enqueue attempt
        memoryTracker.takeSnapshot(queue->length());
        
        if (size <= MAX_SIZE) {
            EXPECT_TRUE(success) << "Failed to enqueue valid size: " << size;
            
            if (success) {
                // Allocate buffer for dequeue
                std::unique_ptr<char[]> buffer(new char[size + 1]);
                memset(buffer.get(), 0, size + 1);
                
                // Dequeue and verify
                int result = queue->dequeue(buffer.get(), size + 1);
                EXPECT_EQ(result, static_cast<int>(size));
                
                // Very basic content check (first and last characters)
                EXPECT_EQ(buffer[0], 'X');
                EXPECT_EQ(buffer[size-1], 'X');
            }
        }
        else {
            EXPECT_FALSE(success) << "Should not enqueue oversized message: " << size;
        }
    }
    
    // Check for memory issues
    memoryTracker.writeToCSV("extreme_large_messages.csv");
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep in large message handling";
}

// Test for queue traversal and potential reference issues
TEST_F(MessageQueueMemoryStressTest, QueueTraversalReferenceIssues) {
    const int MESSAGE_COUNT = 100;
    
    // Fill queue with messages
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        string message = generateTestMessage(100, i);
        EXPECT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
    }
    
    // Take memory snapshot before traversals
    memoryTracker.takeSnapshot(queue->length());
    
    // Perform multiple traversals
    for (int traversal = 0; traversal < 10; traversal++) {
        // Get a generator for traversing the queue
        auto generator = queue->traverseQueue();
        
        int count = 0;
        for (auto msg : generator) {
            // Access message properties but don't modify
            EXPECT_TRUE(msg != nullptr);
            EXPECT_GT(msg->data_length, 0u);
            count++;
        }
        
        EXPECT_EQ(count, MESSAGE_COUNT);
        
        // Take memory snapshot after each traversal
        if (traversal % 2 == 0) {
            memoryTracker.takeSnapshot(queue->length());
        }
    }
    
    // Check for memory issues
    memoryTracker.writeToCSV("queue_traversal.csv");
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep in queue traversal";
    
    // Verify queue is still intact
    EXPECT_EQ(queue->length(), MESSAGE_COUNT);
    
    // Dequeue all messages to clean up
    char buffer[4096];
    int dequeued = 0;
    while (queue->dequeue(buffer, sizeof(buffer)) > 0) {
        dequeued++;
    }
    
    EXPECT_EQ(dequeued, MESSAGE_COUNT);
}

// Test for extreme edge case: Create message then shutdown
TEST_F(MessageQueueMemoryStressTest, CreateMessageThenShutdown) {
    // This test creates messages and immediately begins shutdown
    // to test cleanup edge cases
    
    const int TEST_ITERATIONS = 100;
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // Create a new queue
        auto testQueue = std::make_unique<MessageQueue>(5, 10);
        
        // Create a message
        string message = generateTestMessage(1000, i);
        testQueue->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
        
        // Immediately begin shutdown
        testQueue->beginShutdown();
        
        // Destroy the queue
        testQueue.reset();
        
        // Take memory snapshot every 10 iterations
        if (i % 10 == 0) {
            memoryTracker.takeSnapshot(0);
        }
    }
    
    // Check for memory issues
    memoryTracker.writeToCSV("create_then_shutdown.csv");
    EXPECT_FALSE(memoryTracker.hasSignificantMemoryCreep())
        << "Detected memory creep in create-then-shutdown test";
}

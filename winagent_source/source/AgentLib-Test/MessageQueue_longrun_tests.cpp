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
#include <mutex>

using namespace Syslog_agent;
using namespace std;

// Helper for tracking memory usage over time
class MemoryMonitor {
public:
    void recordMemoryUsage(const string& label, uint32_t queueSize = 0) {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), 
                               reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), 
                               sizeof(pmc))) {
            std::lock_guard<std::mutex> lock(mutex_);
            readings_.push_back({
                label,
                std::chrono::system_clock::now(),
                pmc.WorkingSetSize,
                pmc.PrivateUsage,
                queueSize
            });
        }
    }
    
    void writeToCSV(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream file(filename);
        if (!file.is_open()) return;
        
        file << "Timestamp,Label,WorkingSet,PrivateUsage,QueueSize\n";
        
        for (const auto& reading : readings_) {
            auto timePoint = std::chrono::system_clock::to_time_t(reading.timestamp);
            file << timePoint << ","
                 << reading.label << ","
                 << reading.workingSetSize << ","
                 << reading.privateUsage << ","
                 << reading.queueSize << "\n";
        }
    }
    
    double calculateGrowthRate() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (readings_.size() < 10) return 0.0;
        
        // Simple linear regression on memory usage
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        int n = static_cast<int>(readings_.size());
        
        for (int i = 0; i < n; i++) {
            double x = static_cast<double>(i);
            double y = static_cast<double>(readings_[i].privateUsage);
            
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
        }
        
        // Calculate slope of regression line
        double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
        return slope;
    }
    
private:
    struct MemoryReading {
        string label;
        std::chrono::system_clock::time_point timestamp;
        size_t workingSetSize;
        size_t privateUsage;
        uint32_t queueSize;
    };
    
    std::vector<MemoryReading> readings_;
    std::mutex mutex_;
};

// Long-running test fixture
class MessageQueueLongRunTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create queue with small initial sizes to test growth behavior
        queue = std::make_unique<MessageQueue>(10, 20);
        memoryMonitor.recordMemoryUsage("Setup", 0);
    }
    
    void TearDown() override {
        // Clean up and record final memory state
        if (queue) {
            memoryMonitor.recordMemoryUsage("BeforeShutdown", queue->length());
            queue->beginShutdown();
            queue.reset();
        }
        memoryMonitor.recordMemoryUsage("AfterShutdown", 0);
    }
    
    // Helper to generate variable-sized test messages
    string generateMessage(size_t size, int id) {
        string msg(size, 'X');
        string prefix = "MSG" + std::to_string(id) + ":";
        if (prefix.size() < size) {
            memcpy(&msg[0], prefix.c_str(), prefix.size());
        }
        return msg;
    }
    
    std::unique_ptr<MessageQueue> queue;
    MemoryMonitor memoryMonitor;
};

// Tests sustained high-volume producer/consumer scenario 
// with varying message sizes and processing speeds
TEST_F(MessageQueueLongRunTest, HighVolumeSustained) {
    const int TEST_DURATION_SECONDS = 5;
    
    std::atomic<bool> running{true};
    std::atomic<int> producerCount{0};
    std::atomic<int> consumerCount{0};
    
    // Three producers with different message size patterns
    vector<thread> producers;
    
    // Producer 1: Small messages (50-200 bytes)
    producers.push_back(thread([&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> sizeDist(50, 200);
        
        int msgId = 0;
        while (running) {
            size_t size = sizeDist(rng);
            string msg = generateMessage(size, msgId++);
            
            if (queue->enqueue(msg.c_str(), static_cast<uint32_t>(size))) {
                producerCount++;
            }
            
            this_thread::sleep_for(chrono::microseconds(100));
        }
    }));
    
    // Producer 2: Medium messages (500-1500 bytes)
    producers.push_back(thread([&]() {
        std::mt19937 rng(43);
        std::uniform_int_distribution<int> sizeDist(500, 1500);
        
        int msgId = 10000;
        while (running) {
            size_t size = sizeDist(rng);
            string msg = generateMessage(size, msgId++);
            
            if (queue->enqueue(msg.c_str(), static_cast<uint32_t>(size))) {
                producerCount++;
            }
            
            this_thread::sleep_for(chrono::microseconds(500));
        }
    }));
    
    // Producer 3: Large messages (closer to buffer limits)
    producers.push_back(thread([&]() {
        std::mt19937 rng(44);
        std::uniform_int_distribution<int> sizeDist(1800, 
            MessageQueue::MESSAGE_BUFFER_SIZE * 3 - 100);
        
        int msgId = 20000;
        while (running) {
            size_t size = sizeDist(rng);
            string msg = generateMessage(size, msgId++);
            
            if (queue->enqueue(msg.c_str(), static_cast<uint32_t>(size))) {
                producerCount++;
            }
            
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }));
    
    // Multiple consumers with different processing speeds
    vector<thread> consumers;
    
    // Fast consumer
    consumers.push_back(thread([&]() {
        vector<char> buffer(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS);
        
        while (running || !queue->isEmpty()) {
            int result = queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size()));
            if (result > 0) {
                consumerCount++;
            } else {
                this_thread::sleep_for(chrono::milliseconds(1));
            }
        }
    }));
    
    // Slow consumer
    consumers.push_back(thread([&]() {
        vector<char> buffer(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS);
        std::mt19937 rng(45);
        std::uniform_int_distribution<int> delayDist(1, 10);
        
        while (running || !queue->isEmpty()) {
            int result = queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size()));
            if (result > 0) {
                consumerCount++;
                
                // Simulate variable processing time
                this_thread::sleep_for(chrono::milliseconds(delayDist(rng)));
            } else {
                this_thread::sleep_for(chrono::milliseconds(2));
            }
        }
    }));
    
    // Monitoring thread
    thread monitor([&]() {
        for (int i = 0; i < TEST_DURATION_SECONDS * 2; i++) {
            memoryMonitor.recordMemoryUsage("Monitor-" + std::to_string(i), queue->length());
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    });
    
    // Let test run
    this_thread::sleep_for(chrono::seconds(TEST_DURATION_SECONDS));
    running = false;
    
    // Join all threads
    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();
    monitor.join();
    
    // Record final statistics and write report
    memoryMonitor.recordMemoryUsage("AfterTest", queue->length());
    memoryMonitor.writeToCSV("high_volume_test.csv");
    
    cout << "Messages produced: " << producerCount << endl;
    cout << "Messages consumed: " << consumerCount << endl;
    cout << "Final queue length: " << queue->length() << endl;
    
    // Ensure all messages were consumed
    EXPECT_EQ(queue->length(), 0u);
    EXPECT_EQ(producerCount, consumerCount);
    
    // Check for memory leaks
    double growthRate = memoryMonitor.calculateGrowthRate();
    cout << "Memory growth rate: " << growthRate << endl;
    
    // A positive growth rate indicates memory creep
    EXPECT_LT(growthRate, 10000.0) << "Excessive memory growth detected";
}

// Tests behavior with burst traffic followed by idle periods
TEST_F(MessageQueueLongRunTest, TrafficBurstsWithIdle) {
    const int BURST_COUNT = 5;
    const int MESSAGES_PER_BURST = 1000;
    const int IDLE_MS = 500;
    
    std::mt19937 rng(46);
    std::uniform_int_distribution<int> sizeDist(100, 2000);
    
    memoryMonitor.recordMemoryUsage("Start", 0);
    
    for (int burst = 0; burst < BURST_COUNT; burst++) {
        cout << "Starting burst " << burst << endl;
        memoryMonitor.recordMemoryUsage("BeforeBurst-" + std::to_string(burst), queue->length());
        
        // Enqueue burst of messages
        for (int i = 0; i < MESSAGES_PER_BURST; i++) {
            size_t size = sizeDist(rng);
            string msg = generateMessage(size, burst * 10000 + i);
            ASSERT_TRUE(queue->enqueue(msg.c_str(), static_cast<uint32_t>(size)));
        }
        
        memoryMonitor.recordMemoryUsage("AfterBurst-" + std::to_string(burst), queue->length());
        
        // Verify queue size
        EXPECT_EQ(queue->length(), MESSAGES_PER_BURST);
        
        // Dequeue all messages
        vector<char> buffer(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS);
        for (int i = 0; i < MESSAGES_PER_BURST; i++) {
            int result = queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size()));
            ASSERT_GT(result, 0);
        }
        
        EXPECT_EQ(queue->length(), 0u);
        memoryMonitor.recordMemoryUsage("AfterDequeue-" + std::to_string(burst), queue->length());
        
        // Idle period allowing for potential pool shrinking
        cout << "Idle period..." << endl;
        this_thread::sleep_for(chrono::milliseconds(IDLE_MS));
        
        // Force some memory management
        for (int i = 0; i < 10; i++) {
            string msg = "small";
            queue->enqueue(msg.c_str(), static_cast<uint32_t>(msg.size()));
            queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size()));
        }
        
        memoryMonitor.recordMemoryUsage("AfterIdle-" + std::to_string(burst), queue->length());
    }
    
    memoryMonitor.writeToCSV("traffic_bursts.csv");
    
    // Check for memory creep
    double growthRate = memoryMonitor.calculateGrowthRate();
    cout << "Memory growth rate: " << growthRate << endl;
    
    // Growth should be minimal after bursts and idle periods
    EXPECT_LT(growthRate, 10000.0) << "Memory not properly released after traffic bursts";
}

// Tests chained message buffer memory management
TEST_F(MessageQueueLongRunTest, ChainedBufferManagement) {
    const int TEST_ITERATIONS = 100;
    
    // Test with messages requiring exact chain lengths
    for (int buffers = 1; buffers <= 5; buffers++) {
        // Calculate size to require exactly 'buffers' number of buffer objects
        uint32_t size = MessageQueue::MESSAGE_BUFFER_SIZE * buffers - 10;
        
        memoryMonitor.recordMemoryUsage("BeforeChain-" + std::to_string(buffers), queue->length());
        
        // Repeatedly enqueue and dequeue messages with this chain length
        for (int i = 0; i < TEST_ITERATIONS; i++) {
            string msg = generateMessage(size, i);
            ASSERT_TRUE(queue->enqueue(msg.c_str(), size));
            
            // Record memory periodically
            if (i % 25 == 0) {
                memoryMonitor.recordMemoryUsage(
                    "Iter" + std::to_string(i) + "-Chain" + std::to_string(buffers), 
                    queue->length());
            }
            
            // Dequeue and verify
            vector<char> buffer(size + 1);
            int result = queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size()));
            EXPECT_EQ(result, static_cast<int>(size));
            
            // Verify start of message (prefix)
            string prefix = "MSG" + std::to_string(i) + ":";
            EXPECT_EQ(strncmp(buffer.data(), prefix.c_str(), prefix.size()), 0);
        }
        
        memoryMonitor.recordMemoryUsage("AfterChain-" + std::to_string(buffers), queue->length());
    }
    
    memoryMonitor.writeToCSV("chained_buffers.csv");
    
    // Check for memory creep
    double growthRate = memoryMonitor.calculateGrowthRate();
    cout << "Memory growth rate: " << growthRate << endl;
    
    // No significant memory growth should occur
    EXPECT_LT(growthRate, 5000.0) << "Memory growth detected in chained buffer test";
}

// Tests memory management under continuous enqueue/dequeue cycles
// with message hook usage (which could cause reference leaks)
TEST_F(MessageQueueLongRunTest, MemoryManagementWithHook) {
    const int TEST_ITERATIONS = 1000;
    
    std::mt19937 rng(47);
    std::uniform_int_distribution<int> sizeDist(100, 3000);
    
    memoryMonitor.recordMemoryUsage("BeforeHookTest", queue->length());
    
    // Track calls and rejections
    int hookCalls = 0;
    int hookRejections = 0;
    
    // Install a message hook that rejects some messages
    queue->setEnqueueHook([&](size_t queue_length, MessageQueue::Message* message, bool is_pre_enqueue) {
        hookCalls++;
        
        // Every 7th pre-enqueue call, reject the message
        if (is_pre_enqueue && hookCalls % 7 == 0) {
            hookRejections++;
            return false;
        }
        
        // Don't hold any references to the message!
        return true;
    });
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // Create message with random size
        uint32_t size = sizeDist(rng);
        size = std::min(size, MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS - 100);
        
        string msg = generateMessage(size, i);
        queue->enqueue(msg.c_str(), size);
        
        // Every 50 iterations check memory
        if (i % 50 == 0) {
            memoryMonitor.recordMemoryUsage("HookTest-" + std::to_string(i), queue->length());
            
            // Dequeue all accumulated messages
            vector<char> buffer(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS);
            while (queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size())) > 0) {
                // Just drain the queue
            }
        }
    }
    
    // Remove hook before cleanup
    queue->setEnqueueHook(nullptr);
    
    // Final checks
    memoryMonitor.recordMemoryUsage("AfterHookTest", queue->length());
    memoryMonitor.writeToCSV("hook_memory.csv");
    
    cout << "Hook calls: " << hookCalls << endl;
    cout << "Hook rejections: " << hookRejections << endl;
    
    // Verify hook behavior
    EXPECT_GT(hookCalls, 0);
    EXPECT_GT(hookRejections, 0);
    
    // Dequeue all remaining messages
    vector<char> buffer(MessageQueue::MESSAGE_BUFFER_SIZE * MessageQueue::MAX_BUFFERS);
    while (queue->dequeue(buffer.data(), static_cast<uint32_t>(buffer.size())) > 0) {
        // Just drain the queue
    }
    
    // Check for memory creep
    double growthRate = memoryMonitor.calculateGrowthRate();
    cout << "Memory growth rate: " << growthRate << endl;
    
    // Growth should be minimal
    EXPECT_LT(growthRate, 10000.0) << "Memory leak detected with message hook usage";
}

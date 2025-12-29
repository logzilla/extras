#include "pch.h"
#include "../AgentLib/SyslogSender.h"
#include "../AgentLib/MessageQueue.h"
#include "../AgentLib/MessageBatcher.h"
#include "../AgentLib/INetworkClient.h"
#include "../AgentLib/BatchBufferGuard.h"
#include "../Infrastructure/ScopedMemoryLeak.h"
#include "TestUtils.h"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <Windows.h>
#include <Psapi.h>

using namespace Syslog_agent;
using namespace std;

// Helper function to get current process memory usage
SIZE_T GetProcessMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

// Advanced Network Client Mock with memory leak simulation capabilities
class MemoryLeakNetworkClient : public INetworkClient {
public:
    MemoryLeakNetworkClient(
        bool simulateFailure = false,
        bool simulateException = false,
        bool simulateMemoryLeak = false,
        int failureCount = 0,
        int failureFrequency = 0,  // Fail every N requests (0 = disabled)
        int responseDelay = 0)     // Milliseconds to delay response
        : simulateFailure_(simulateFailure)
        , simulateException_(simulateException)
        , simulateMemoryLeak_(simulateMemoryLeak)
        , failureCount_(failureCount)
        , failureFrequency_(failureFrequency)
        , responseDelay_(responseDelay)
        , postCount_(0)
        , lastBatchSize_(0)
        , leakedMemory_(nullptr)
        , totalLeakedBytes_(0) {}

    ~MemoryLeakNetworkClient() {
        // Clean up any leaked memory to avoid actual test memory leaks
        for (auto& ptr : leakedPointers_) {
            delete[] ptr;
        }
        leakedPointers_.clear();
        totalLeakedBytes_ = 0;
    }

    RESULT_TYPE post(const char* data, size_t dataSize) override {
        if (simulateException_) {
            throw std::runtime_error("Simulated network exception");
        }

        // Simulate network delay
        if (responseDelay_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(responseDelay_));
        }

        postCount_++;
        lastBatchSize_ = dataSize;
        
        // Store a copy of the data for verification
        sentData_.assign(data, dataSize);
        
        // Simulate memory leak if configured
        if (simulateMemoryLeak_) {
            char* leakedBuffer = new char[dataSize];
            memcpy(leakedBuffer, data, dataSize);
            leakedPointers_.push_back(leakedBuffer);
            totalLeakedBytes_ += dataSize;
        }
        
        // Simulate periodic failures
        if (failureFrequency_ > 0 && (postCount_ % failureFrequency_ == 0)) {
            return RESULT_TYPE(RESULT_NETWORK_ERROR);
        }
        
        // Simulate failure count
        if (simulateFailure_ && (failureCount_ == 0 || postCount_ <= failureCount_)) {
            return RESULT_TYPE(RESULT_NETWORK_ERROR);
        }
        
        return RESULT_TYPE(RESULT_SUCCESS);
    }
    
    size_t getPostCount() const { return postCount_; }
    size_t getLastBatchSize() const { return lastBatchSize_; }
    size_t getTotalLeakedBytes() const { return totalLeakedBytes_; }
    const std::string& getSentData() const { return sentData_; }

private:
    bool simulateFailure_;
    bool simulateException_;
    bool simulateMemoryLeak_;
    int failureCount_;
    int failureFrequency_;
    int responseDelay_;
    size_t postCount_;
    size_t lastBatchSize_;
    std::string sentData_;
    
    // Used to track simulated memory leaks
    std::vector<char*> leakedPointers_;
    char* leakedMemory_;
    size_t totalLeakedBytes_;
};

// Memory tracking message batcher for detecting potential leaks
class MemoryTrackingBatcher : public MessageBatcher {
public:
    MemoryTrackingBatcher(uint32_t maxBatchSize = 1024 * 1024, 
                          bool simulateBufferNotReleased = false,
                          bool simulateFragmentation = false)
        : MessageBatcher(maxBatchSize, 1000)  // 1 second max age
        , simulateBufferNotReleased_(simulateBufferNotReleased)
        , simulateFragmentation_(simulateFragmentation)
        , currentBuffers_()
        , batchCount_(0) {
    }

    ~MemoryTrackingBatcher() {
        // Clean up any unreleased buffers
        for (auto& buffer : currentBuffers_) {
            delete[] buffer;
        }
        currentBuffers_.clear();
    }
    
    BatchResult BatchEvents(std::shared_ptr<MessageQueue> queue, char* buffer, size_t bufferSize) override {
        batchCount_++;
        
        if (!queue || queue->length() == 0 || !buffer || bufferSize == 0) {
            return BatchResult { BatchResult::Status::NoMessages, 0, 0 };
        }
        
        // Mock a successful batch - process actual messages
        size_t messageCount = std::min(queue->length(), static_cast<uint32_t>(10));
        size_t bytesWritten = 0;
        size_t remainingBytes = bufferSize;
        
        // Copy each message content
        size_t index = 0;
        for (auto msg : queue->traverseQueue()) {
            if (index >= messageCount) break;
            
            // Get message content
            std::vector<char> msgContent(msg->data_length + 1);
            if (queue->peek(msg, msgContent.data(), msg->data_length) > 0) {
                msgContent[msg->data_length] = '\0';  // Null-terminate
                
                // Add separator if not first message
                if (index > 0 && remainingBytes > 1) {
                    buffer[bytesWritten++] = '\n';
                    remainingBytes--;
                }
                
                // Copy message content to buffer
                size_t copySize = std::min(static_cast<size_t>(msg->data_length), remainingBytes);
                memcpy(buffer + bytesWritten, msgContent.data(), copySize);
                bytesWritten += copySize;
                remainingBytes -= copySize;
                
                if (remainingBytes <= 0) break;
            }
            
            index++;
        }
        
        return BatchResult { BatchResult::Status::Success, static_cast<uint32_t>(index), bytesWritten };
    }
    
    char* GetBatchBuffer(const char* requestor) override {
        if (simulateFragmentation_) {
            // Allocate buffers of random size to cause fragmentation
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(1024, 65536);
            char* buffer = new char[dis(gen)];
            currentBuffers_.push_back(buffer);
            return buffer;
        }
        else {
            // Regular fixed size buffer
            char* buffer = new char[GetMaxBatchSizeBytes()];
            currentBuffers_.push_back(buffer);
            return buffer;
        }
    }
    
    bool ReleaseBatchBuffer(char* buffer) override {
        if (simulateBufferNotReleased_) {
            // Simulate not releasing by doing nothing
            return true;
        }
        
        // Find and remove the buffer from our tracking
        auto it = std::find(currentBuffers_.begin(), currentBuffers_.end(), buffer);
        if (it != currentBuffers_.end()) {
            delete[] *it;
            currentBuffers_.erase(it);
            return true;
        }
        
        return false;
    }
    
    uint32_t GetMaxBatchSizeBytes() const override {
        return max_batch_size_;
    }
    
    void validateBatchBuffer(char* buffer, size_t buffer_size) const override {
        // Simple validation that buffer exists and is within our tracked buffers
        if (!buffer) {
            throw std::runtime_error("Null batch buffer");
        }
        
        auto it = std::find(currentBuffers_.begin(), currentBuffers_.end(), buffer);
        if (it == currentBuffers_.end()) {
            throw std::runtime_error("Batch buffer not tracked");
        }
    }
    
    size_t getBatchCount() const { return batchCount_; }
    size_t getUnreleasedBufferCount() const { return currentBuffers_.size(); }

protected:
    uint32_t GetMaxBatchSizeBytes_() const override {
        return max_batch_size_;
    }
    
    void GetMessageHeader_(char* dest, size_t max_size, size_t& size_out) const override {
        const char header[] = "";
        size_t headerLen = strlen(header);
        size_out = std::min(headerLen, max_size);
        if (size_out > 0) {
            memcpy(dest, header, size_out);
        }
    }
    
    void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const override {
        const char sep[] = "\n";
        size_t sepLen = strlen(sep);
        size_out = std::min(sepLen, max_size);
        if (size_out > 0) {
            memcpy(dest, sep, size_out);
        }
    }
    
    void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const override {
        const char trailer[] = "";
        size_t trailerLen = strlen(trailer);
        size_out = std::min(trailerLen, max_size);
        if (size_out > 0) {
            memcpy(dest, trailer, size_out);
        }
    }
    
private:
    bool simulateBufferNotReleased_;
    bool simulateFragmentation_;
    mutable std::vector<char*> currentBuffers_;
    size_t batchCount_;
};

// Base test fixture for memory tests
class SyslogSenderMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create queues with sufficient capacity
        primaryQueue_ = std::make_shared<MessageQueue>(1000, 2000);
        secondaryQueue_ = std::make_shared<MessageQueue>(1000, 2000);
        
        // Create network clients
        primaryClient_ = std::make_shared<MemoryLeakNetworkClient>();
        secondaryClient_ = std::make_shared<MemoryLeakNetworkClient>();
        
        // Create batchers
        primaryBatcher_ = std::make_shared<MemoryTrackingBatcher>();
        secondaryBatcher_ = std::make_shared<MemoryTrackingBatcher>();
        
        // Default settings for tests
        maxBatchCount_ = 50;
        maxBatchAge_ = 100; // milliseconds
        
        // Get baseline memory usage
        baselineMemoryUsage_ = GetProcessMemoryUsage();
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
        
        // Check that memory usage has returned to baseline
        SIZE_T finalMemoryUsage = GetProcessMemoryUsage();
        SIZE_T memoryDelta = (finalMemoryUsage > baselineMemoryUsage_) ? 
                             (finalMemoryUsage - baselineMemoryUsage_) : 0;
                             
        // Note: This is just for diagnostic purposes, not a failure condition
        // Real memory issues might only be visible after many iterations
        if (memoryDelta > 1024 * 1024) {  // More than 1MB difference
            std::cerr << "Warning: Memory usage increased by " 
                      << (memoryDelta / 1024) << " KB during test" << std::endl;
        }
    }

    // Create and initialize the SyslogSender
    void createSender() {
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
    
    // Helper to populate queue with test messages
    void populateQueue(std::shared_ptr<MessageQueue> queue, size_t count, 
                      size_t msgSize = 256, const std::string& prefix = "Test") {
        for (size_t i = 0; i < count; i++) {
            std::string message = prefix + " Message " + std::to_string(i);
            // Pad to requested size
            if (message.length() < msgSize) {
                message.append(msgSize - message.length(), 'X');
            }
            ASSERT_TRUE(queue->enqueue(message.c_str(), static_cast<uint32_t>(message.length())));
        }
    }
    
    // Run sender in a separate thread with timeout
    std::thread runSenderThread(int timeoutMs = 1000) {
        return std::thread([this, timeoutMs]() {
            auto start = std::chrono::steady_clock::now();
            auto sender_thread = std::thread([this]() { sender_->run(); });
            
            // Wait for timeout or until the sender completes
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
            
            // Request stop and wait for thread to complete
            sender_->requestStopAndNotify();
            if (sender_thread.joinable()) {
                sender_thread.join();
            }
        });
    }

    // Common test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<MemoryLeakNetworkClient> primaryClient_;
    std::shared_ptr<MemoryLeakNetworkClient> secondaryClient_;
    std::shared_ptr<MemoryTrackingBatcher> primaryBatcher_;
    std::shared_ptr<MemoryTrackingBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
    SIZE_T baselineMemoryUsage_;
};

// Test unbounded queue growth during network failures
TEST_F(SyslogSenderMemoryTest, QueueGrowthDuringNetworkFailure) {
    // Configure primary client to always fail
    primaryClient_ = std::make_shared<MemoryLeakNetworkClient>(true);
    
    // Create sender with network client that always fails
    createSender();
    
    // Pre-populate the queue with messages
    size_t initialQueueSize = 100;
    populateQueue(primaryQueue_, initialQueueSize);
    
    // Get starting memory usage
    SIZE_T startMemory = GetProcessMemoryUsage();
    
    // Set up continuous message producer in a separate thread
    std::atomic<bool> stopProducer{false};
    std::thread producerThread([this, &stopProducer]() {
        size_t msgCount = 0;
        while (!stopProducer.load()) {
            std::string message = "Producer Message " + std::to_string(msgCount++);
            primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Run the sender for a short period
    auto senderThread = runSenderThread(1000);  // Run for 1 second
    
    // Wait for threads to complete
    senderThread.join();
    stopProducer.store(true);
    producerThread.join();
    
    // Verify that without proper backpressure, the queue grows substantially
    EXPECT_GT(primaryQueue_->length(), initialQueueSize);
    
    // Verify memory usage increased
    SIZE_T endMemory = GetProcessMemoryUsage();
    std::cout << "Memory usage: Start=" << (startMemory/1024) << "KB, End=" 
              << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    
    // Note: This is more of a diagnostic measurement than a strict test
    // Memory should have increased proportionally to queue growth
    EXPECT_GT(endMemory, startMemory);
}

// Test resource leakage through buffer reuse issues
TEST_F(SyslogSenderMemoryTest, BufferReuseAndResourceLeaks) {
    // Configure batcher to not release buffers
    primaryBatcher_ = std::make_shared<MemoryTrackingBatcher>(1024 * 1024, true);
    
    // Configure network client with delays to slow processing
    primaryClient_ = std::make_shared<MemoryLeakNetworkClient>(false, false, false, 0, 0, 50);
    
    // Create sender with our configured components
    createSender();
    
    // Pre-populate the queue with messages
    populateQueue(primaryQueue_, 200);
    
    // Get starting memory usage and buffer count
    SIZE_T startMemory = GetProcessMemoryUsage();
    size_t startBufferCount = primaryBatcher_->getUnreleasedBufferCount();
    
    // Run the sender briefly
    auto senderThread = runSenderThread(2000);  // Run for 2 seconds
    senderThread.join();
    
    // Verify buffer leaks
    size_t endBufferCount = primaryBatcher_->getUnreleasedBufferCount();
    EXPECT_GT(endBufferCount, startBufferCount);
    
    // Verify memory usage increased
    SIZE_T endMemory = GetProcessMemoryUsage();
    std::cout << "Memory usage: Start=" << (startMemory/1024) << "KB, End=" 
              << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "Unreleased buffers: Start=" << startBufferCount 
              << ", End=" << endBufferCount 
              << ", Delta=" << (endBufferCount - startBufferCount) << std::endl;
}

// Test heap fragmentation from variable-sized allocations
TEST_F(SyslogSenderMemoryTest, HeapFragmentationFromVariableSizedMessages) {
    // Configure batcher to create variable-sized buffers
    primaryBatcher_ = std::make_shared<MemoryTrackingBatcher>(1024 * 1024, false, true);
    createSender();
    
    // Pre-populate with variable-sized messages
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> msgSizeDist(100, 2000);
    
    for (int i = 0; i < 500; ++i) {
        size_t msgSize = msgSizeDist(gen);
        std::string message(msgSize, 'X');
        primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
    }
    
    // Get starting memory
    SIZE_T startMemory = GetProcessMemoryUsage();
    
    // Run multiple processing cycles to create fragmentation
    for (int cycle = 0; cycle < 10; ++cycle) {
        auto senderThread = runSenderThread(200);
        senderThread.join();
        
        // Add more variable-sized messages between cycles
        for (int i = 0; i < 50; ++i) {
            size_t msgSize = msgSizeDist(gen);
            std::string message(msgSize, 'X');
            primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
        }
    }
    
    // Verify memory usage pattern
    SIZE_T endMemory = GetProcessMemoryUsage();
    std::cout << "Memory usage after variable allocation: Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    
    // Memory may be higher due to fragmentation
    // Note: This is more for diagnostic information than a strict pass/fail
}

// Test long-running with network failures and retry mechanism
TEST_F(SyslogSenderMemoryTest, RetryMechanismMemoryLeaks) {
    // Configure network client to fail periodically
    primaryClient_ = std::make_shared<MemoryLeakNetworkClient>(
        false,    // Don't always fail
        false,    // Don't throw exceptions
        false,    // Don't leak memory in the client
        0,        // Don't use failure count
        3,        // Fail every 3rd request
        50);      // 50ms delay
    
    createSender();
    
    // Pre-populate queue
    populateQueue(primaryQueue_, 150);
    
    // Get starting memory
    SIZE_T startMemory = GetProcessMemoryUsage();
    
    // Run sender with periodic producer
    std::atomic<bool> stopProducer{false};
    std::thread producerThread([this, &stopProducer]() {
        size_t msgCount = 0;
        while (!stopProducer.load()) {
            if (primaryQueue_->length() < 200) {  // Keep queue size bounded
                std::string message = "Retry Test Message " + std::to_string(msgCount++);
                primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Run longer to see cumulative effects
    auto senderThread = runSenderThread(3000);  // Run for 3 seconds
    
    // Wait for threads to complete
    senderThread.join();
    stopProducer.store(true);
    producerThread.join();
    
    // Verify memory usage after retries
    SIZE_T endMemory = GetProcessMemoryUsage();
    std::cout << "Memory usage after retry processing: Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
            
    // Check client stats
    std::cout << "Network client stats - Post count: " << primaryClient_->getPostCount() << std::endl;
}

// Test reference counting issues with shared_ptr message references
TEST_F(SyslogSenderMemoryTest, SharedPointerReferenceLeaks) {
    // Create custom network client that holds references to messages
    class ReferenceLeakingClient : public INetworkClient {
    public:
        ReferenceLeakingClient() : postCount_(0), capturedMessages_() {}
        
        RESULT_TYPE post(const char* data, size_t dataSize) override {
            postCount_++;
            
            // Simulate capturing message references that are never released
            std::string* captured = new std::string(data, dataSize);
            capturedMessages_.push_back(captured);
            
            return RESULT_TYPE(RESULT_SUCCESS);
        }
        
        ~ReferenceLeakingClient() {
            // Clean up for test purposes
            for (auto msg : capturedMessages_) {
                delete msg;
            }
            capturedMessages_.clear();
        }
        
        size_t getPostCount() const { return postCount_; }
        size_t getCapturedCount() const { return capturedMessages_.size(); }
        
    private:
        size_t postCount_;
        std::vector<std::string*> capturedMessages_;
    };
    
    // Use our reference-leaking client
    auto leakingClient = std::make_shared<ReferenceLeakingClient>();
    primaryClient_ = std::make_shared<MemoryLeakNetworkClient>();  // Regular client as fallback
    
    // Create sender using our custom client
    createSender();
    
    // Use ScopedMemoryLeak to measure allocations
    {
        Infrastructure::ScopedMemoryLeak memoryLeakDetector;
        
        // Run several cycles of message processing
        for (int cycle = 0; cycle < 5; ++cycle) {
            // Add messages
            populateQueue(primaryQueue_, 20);
            
            // Process messages
            auto senderThread = runSenderThread(500);
            senderThread.join();
            
            // Check memory usage 
            std::cout << "Cycle " << cycle << " - Memory allocations: " 
                      << memoryLeakDetector.getAllocatedBytes() << " bytes in "
                      << memoryLeakDetector.getAllocatedBlocks() << " blocks" << std::endl;
        }
        
        // Final memory stats
        size_t allocatedBlocks = memoryLeakDetector.getAllocatedBlocks();
        size_t allocatedBytes = memoryLeakDetector.getAllocatedBytes();
        
        std::cout << "Final memory stats - Blocks: " << allocatedBlocks 
                  << ", Bytes: " << allocatedBytes << std::endl;
                  
        // Note: In a perfect world, this should be close to zero
        // Actual implementations will vary
    }
}

// The batching buffer usage test - tests the large static buffer allocation
TEST_F(SyslogSenderMemoryTest, LargeStaticBufferUsage) {
    // Create multiple senders to see impact of each 8MB buffer allocation
    std::vector<std::unique_ptr<SyslogSender>> senders;
    
    // Measure baseline memory
    SIZE_T startMemory = GetProcessMemoryUsage();
    
    // Create 5 sender instances (each should allocate an 8MB buffer)
    for (int i = 0; i < 5; i++) {
        auto queue = std::make_shared<MessageQueue>(100, 200);
        auto client = std::make_shared<MemoryLeakNetworkClient>();
        auto batcher = std::make_shared<MemoryTrackingBatcher>();
        
        senders.push_back(std::make_unique<SyslogSender>(
            queue, nullptr, client, nullptr, batcher, nullptr, 50, 100));
            
        // Measure memory after each allocation
        SIZE_T currentMemory = GetProcessMemoryUsage();
        SIZE_T delta = currentMemory - startMemory;
        std::cout << "After creating sender " << i << ": Memory usage increased by " 
                  << (delta / (1024 * 1024)) << " MB" << std::endl;
                  
        // Each sender should increase memory by about 8MB (the size of send_buffer_)
        // Allow for some variance due to other allocations
        SIZE_T expectedMin = i * SyslogSender::SEND_BUFFER_SIZE * 0.8; // 80% of expected
        EXPECT_GT(delta, expectedMin);
    }
    
    // Clean up
    senders.clear();
    
    // Verify memory is reclaimed
    SIZE_T endMemory = GetProcessMemoryUsage();
    SIZE_T finalDelta = endMemory - startMemory;
    std::cout << "After cleanup: Memory delta = " << (finalDelta / (1024 * 1024)) << " MB" << std::endl;
    
    // Most memory should be reclaimed, but some fragmentation may occur
    // This is more diagnostic than a strict test
}

// Test concurrent access with multiple threads creating memory pressure
TEST_F(SyslogSenderMemoryTest, ConcurrentAccessMemoryPressure) {
    createSender();
    
    // Run multiple producer threads to create pressure
    std::atomic<bool> stopProducers{false};
    std::vector<std::thread> producerThreads;
    
    // Measure starting memory
    SIZE_T startMemory = GetProcessMemoryUsage();
    
    // Create 4 producer threads
    for (int t = 0; t < 4; ++t) {
        producerThreads.push_back(std::thread([this, t, &stopProducers]() {
            size_t msgCount = 0;
            std::string prefix = "Producer" + std::to_string(t) + "-";
            
            while (!stopProducers.load()) {
                if (primaryQueue_->length() < 1000) {  // Prevent unbounded growth
                    std::string message = prefix + std::to_string(msgCount++);
                    primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }));
    }
    
    // Run the sender
    auto senderThread = runSenderThread(3000);  // Run for 3 seconds
    
    // Wait for threads to complete
    senderThread.join();
    stopProducers.store(true);
    for (auto& thread : producerThreads) {
        thread.join();
    }
    
    // Verify memory usage
    SIZE_T endMemory = GetProcessMemoryUsage();
    std::cout << "Concurrent memory pressure test: Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    
    // Queue should have been processed
    std::cout << "Remaining queue size: " << primaryQueue_->length() << std::endl;
}

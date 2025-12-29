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

// Helper to get current process memory usage
SIZE_T GetCurrentMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

// Mock ExceptionThrowingNetworkClient for testing
class ExceptionThrowingNetworkClient : public INetworkClient {
public:
    ExceptionThrowingNetworkClient(
        int throwOnCount = 0,       // Throw on specific call number
        bool throwRandomly = false,  // Throw randomly with 10% chance
        bool leakOnException = false) // Leak memory when exception thrown
        : throwOnCount_(throwOnCount)
        , throwRandomly_(throwRandomly)
        , leakOnException_(leakOnException)
        , postCount_(0)
        , exceptionCount_(0) {
    }

    ~ExceptionThrowingNetworkClient() {
        // Clean up any leaked memory
        for (auto& ptr : leakedBuffers_) {
            delete[] ptr;
        }
        leakedBuffers_.clear();
    }

    RESULT_TYPE post(const char* data, size_t dataSize) override {
        postCount_++;
        
        bool shouldThrow = false;
        if (throwOnCount_ > 0 && postCount_ == throwOnCount_) {
            shouldThrow = true;
        }
        else if (throwRandomly_) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(1, 10);
            shouldThrow = (dis(gen) == 1); // 10% chance
        }
        
        if (shouldThrow) {
            exceptionCount_++;
            
            // Simulate memory leak if configured
            if (leakOnException_) {
                char* leakedBuffer = new char[dataSize];
                memcpy(leakedBuffer, data, dataSize);
                leakedBuffers_.push_back(leakedBuffer);
            }
            
            throw std::runtime_error("Simulated network exception on post #" + 
                std::to_string(postCount_));
        }
        
        // Store data for verification
        lastData_.assign(data, dataSize);
        return RESULT_TYPE(RESULT_SUCCESS);
    }
    
    size_t getPostCount() const { return postCount_; }
    size_t getExceptionCount() const { return exceptionCount_; }
    const std::string& getLastData() const { return lastData_; }

private:
    int throwOnCount_;
    bool throwRandomly_;
    bool leakOnException_;
    size_t postCount_;
    size_t exceptionCount_;
    std::string lastData_;
    std::vector<char*> leakedBuffers_;
};

// Large Object Creation Batcher
class LargeObjectBatcher : public MessageBatcher {
public:
    LargeObjectBatcher(uint32_t maxBatchSize = 1024 * 1024, 
                       bool createLargeObjects = false,
                       bool simulateGarbage = false)
        : MessageBatcher(maxBatchSize, 1000)  // 1 second max age
        , createLargeObjects_(createLargeObjects)
        , simulateGarbage_(simulateGarbage)
        , largeObjects_()
        , batchCount_(0) {
    }

    ~LargeObjectBatcher() {
        // Cleanup large objects
        largeObjects_.clear();
    }
    
    BatchResult BatchEvents(std::shared_ptr<MessageQueue> queue, char* buffer, size_t bufferSize) override {
        if (!buffer || bufferSize == 0) {
            return BatchResult{ BatchResult::Status::InvalidBuffer, 0, 0 };
        }
        
        batchCount_++;
        
        // Create large objects to trigger Large Object Heap allocation if requested
        if (createLargeObjects_) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> sizeDist(85000, 200000); // >85KB goes to LOH
            
            // Create 1-3 large objects each time
            static std::uniform_int_distribution<> countDist(1, 3);
            int objectCount = countDist(gen);
            
            for (int i = 0; i < objectCount; ++i) {
                // Add large vector to simulate large object allocations
                size_t objSize = sizeDist(gen);
                auto largeObject = std::make_shared<std::vector<char>>(objSize);
                
                // If simulating garbage, create lots of small temp objects too
                if (simulateGarbage_) {
                    for (int j = 0; j < 100; ++j) {
                        std::string temp = "Garbage object " + std::to_string(j);
                        temp.append(1024, 'X'); // 1KB of garbage
                        // Use the string to prevent optimization
                        largeObject->at(j % 100) = static_cast<char>(temp.length() % 256);
                    }
                }
                
                largeObjects_.push_back(largeObject);
            }
            
            // Occasionally clear some objects to create LOH fragmentation
            if (largeObjects_.size() > 10) {
                static std::uniform_int_distribution<> dropDist(0, 3);
                if (dropDist(gen) == 0) {
                    // Clear random half of objects
                    size_t toRemove = largeObjects_.size() / 2;
                    for (size_t i = 0; i < toRemove; ++i) {
                        size_t idx = rand() % largeObjects_.size();
                        largeObjects_.erase(largeObjects_.begin() + idx);
                    }
                }
            }
        }
        
        // Process messages
        if (!queue || queue->length() == 0) {
            return BatchResult{ BatchResult::Status::NoMessages, 0, 0 };
        }
        
        // Only process up to 10 messages per batch
        size_t maxMessages = std::min(queue->length(), size_t(10));
        size_t bytesWritten = 0;
        size_t messageCount = 0;
        
        for (auto msg : queue->traverseQueue()) {
            if (messageCount >= maxMessages) break;
            
            // Get message content and add to buffer
            std::vector<char> content(msg->data_length + 1);
            if (queue->peek(msg, content.data(), msg->data_length) > 0) {
                // Add separator if not first message
                if (messageCount > 0 && bytesWritten < bufferSize) {
                    buffer[bytesWritten++] = '\n';
                }
                
                // Ensure we don't overflow the buffer
                size_t bytesToCopy = std::min(static_cast<size_t>(msg->data_length), 
                                             bufferSize - bytesWritten);
                                             
                if (bytesToCopy > 0) {
                    memcpy(buffer + bytesWritten, content.data(), bytesToCopy);
                    bytesWritten += bytesToCopy;
                }
                else {
                    // Buffer full, can't copy more
                    break;
                }
                
                messageCount++;
            }
        }
        
        return BatchResult{ BatchResult::Status::Success, 
                           static_cast<uint32_t>(messageCount), 
                           bytesWritten };
    }
    
    char* GetBatchBuffer(const char* requestor) override {
        return new char[GetMaxBatchSizeBytes()];
    }
    
    bool ReleaseBatchBuffer(char* buffer) override {
        delete[] buffer;
        return true;
    }
    
    uint32_t GetMaxBatchSizeBytes() const override {
        return max_batch_size_;
    }
    
    void validateBatchBuffer(char* buffer, size_t buffer_size) const override {
        // Simple validation that buffer exists
        if (!buffer) {
            throw std::runtime_error("Null batch buffer in validateBatchBuffer");
        }
    }
    
    size_t getBatchCount() const { return batchCount_; }
    size_t getLargeObjectCount() const { return largeObjects_.size(); }

protected:
    uint32_t GetMaxBatchSizeBytes_() const override {
        return max_batch_size_;
    }
    
    void GetMessageHeader_(char* dest, size_t max_size, size_t& size_out) const override {
        const char header[] = "";
        size_out = std::min(strlen(header), max_size);
        if (size_out > 0) {
            memcpy(dest, header, size_out);
        }
    }
    
    void GetMessageSeparator_(char* dest, size_t max_size, size_t& size_out) const override {
        const char sep[] = "\n";
        size_out = std::min(strlen(sep), max_size);
        if (size_out > 0) {
            memcpy(dest, sep, size_out);
        }
    }
    
    void GetMessageTrailer_(char* dest, size_t max_size, size_t& size_out) const override {
        const char trailer[] = "";
        size_out = std::min(strlen(trailer), max_size);
        if (size_out > 0) {
            memcpy(dest, trailer, size_out);
        }
    }

private:
    bool createLargeObjects_;
    bool simulateGarbage_;
    std::vector<std::shared_ptr<std::vector<char>>> largeObjects_;
    size_t batchCount_;
};

// String handling test fixture - focused on string-related memory issues
class SyslogSenderStringTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic test components
        primaryQueue_ = std::make_shared<MessageQueue>(200, 200);
        secondaryQueue_ = std::make_shared<MessageQueue>(200, 200);
        primaryClient_ = std::make_shared<INetworkClient>();
        secondaryClient_ = std::make_shared<INetworkClient>();
        primaryBatcher_ = std::make_shared<MessageBatcher>();
        secondaryBatcher_ = std::make_shared<MessageBatcher>();
        
        // Default settings
        maxBatchCount_ = 20;
        maxBatchAge_ = 200; // milliseconds
        
        // Get baseline memory usage
        baselineMemoryUsage_ = GetCurrentMemoryUsage();
    }

    void TearDown() override {
        // Force stop any running threads
        if (sender_) {
            sender_->requestStopAndNotify();
        }
        
        // Clean up in reverse order of creation
        sender_.reset();
        primaryBatcher_.reset();
        secondaryBatcher_.reset();
        primaryClient_.reset();
        secondaryClient_.reset();
        primaryQueue_.reset();
        secondaryQueue_.reset();
    }

    // Common test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<INetworkClient> primaryClient_;
    std::shared_ptr<INetworkClient> secondaryClient_;
    std::shared_ptr<MessageBatcher> primaryBatcher_;
    std::shared_ptr<MessageBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
    SIZE_T baselineMemoryUsage_;
};

// Test for exception handling memory leaks
class SyslogSenderExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic test components
        primaryQueue_ = std::make_shared<MessageQueue>(200, 200);
        secondaryQueue_ = std::make_shared<MessageQueue>(200, 200);
        primaryClient_ = std::make_shared<ExceptionThrowingNetworkClient>();
        secondaryClient_ = std::make_shared<ExceptionThrowingNetworkClient>();
        primaryBatcher_ = std::make_shared<MessageBatcher>(1024 * 1024, 1000);
        secondaryBatcher_ = std::make_shared<MessageBatcher>(1024 * 1024, 1000);
        
        // Default settings
        maxBatchCount_ = 20;
        maxBatchAge_ = 200; // milliseconds
        
        // Start from clean baseline
        baselineMemoryUsage_ = GetCurrentMemoryUsage();
    }

    void TearDown() override {
        // Force stop any running threads
        if (sender_) {
            sender_->requestStopAndNotify();
        }
        
        // Clean up in reverse order of creation
        sender_.reset();
        primaryBatcher_.reset();
        secondaryBatcher_.reset();
        primaryClient_.reset();
        secondaryClient_.reset();
        primaryQueue_.reset();
        secondaryQueue_.reset();
    }

    // Create SyslogSender
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
    
    // Create and populate queue with test messages
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
    
    // Run sender in a thread with timeout
    std::thread runSenderThread(int timeoutMs = 1000) {
        return std::thread([this, timeoutMs]() {
            auto start = std::chrono::steady_clock::now();
            auto senderThread = std::thread([this]() { sender_->run(); });
            
            // Wait for timeout or until the sender completes
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
            
            // Request stop and wait for thread to complete
            sender_->requestStopAndNotify();
            if (senderThread.joinable()) {
                senderThread.join();
            }
        });
    }
    
    // Common test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<ExceptionThrowingNetworkClient> primaryClient_;
    std::shared_ptr<ExceptionThrowingNetworkClient> secondaryClient_;
    std::shared_ptr<MessageBatcher> primaryBatcher_;
    std::shared_ptr<MessageBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
    SIZE_T baselineMemoryUsage_;
};

// Test for large object heap memory management
TEST_F(SyslogSenderExceptionTest, ExceptionHandlingMemoryLeaks) {
    // Configure primary client to throw exception on specific call
    primaryClient_ = std::make_shared<ExceptionThrowingNetworkClient>(
        3,       // Throw on 3rd call
        false,   // Don't throw randomly
        true);   // Leak memory on exception
    
    // Create sender
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 30);
    
    // Measure starting memory
    SIZE_T startMemory = GetCurrentMemoryUsage();
    
    // Run sender - should encounter exception after a few batches
    auto senderThread = runSenderThread(2000);
    senderThread.join();
    
    // Check that memory increased due to simulated leak
    SIZE_T endMemory = GetCurrentMemoryUsage();
    std::cout << "Memory after exception: Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
              
    // Verify exception was thrown
    EXPECT_GT(primaryClient_->getExceptionCount(), 0);
    
    // In a real system, we'd expect memory to grow slightly due to the leak
    // Though the test mock cleans up at the end
}

// Test for random exceptions
TEST_F(SyslogSenderExceptionTest, RandomExceptionSurvivability) {
    // Configure primary client to throw exceptions randomly
    primaryClient_ = std::make_shared<ExceptionThrowingNetworkClient>(
        0,       // Don't throw on specific call
        true,    // Throw randomly
        false);  // Don't leak memory
    
    // Create sender
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 100);
    
    // Measure starting memory and queue size
    SIZE_T startMemory = GetCurrentMemoryUsage();
    size_t startQueueSize = primaryQueue_->length();
    
    // Run sender with random exceptions
    auto senderThread = runSenderThread(3000);
    senderThread.join();
    
    // Check memory and queue state
    SIZE_T endMemory = GetCurrentMemoryUsage();
    size_t endQueueSize = primaryQueue_->length();
    
    std::cout << "Random exceptions test: Memory delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "Queue sizes: Start=" << startQueueSize 
              << ", End=" << endQueueSize << std::endl;
    std::cout << "Exceptions thrown: " << primaryClient_->getExceptionCount() << std::endl;
    
    // Expect some messages were processed despite exceptions
    EXPECT_LT(endQueueSize, startQueueSize);
    
    // Verify exceptions were actually thrown
    EXPECT_GT(primaryClient_->getExceptionCount(), 0);
}

// Test for large object heap management issues
class SyslogSenderLOHTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test components
        primaryQueue_ = std::make_shared<MessageQueue>(200, 200);
        secondaryQueue_ = std::make_shared<MessageQueue>(200, 200);
        primaryClient_ = std::make_shared<INetworkClient>();
        secondaryClient_ = std::make_shared<INetworkClient>();
        
        // Use large object batcher
        primaryBatcher_ = std::make_shared<LargeObjectBatcher>(
            1024 * 1024,  // 1MB max batch size
            true,         // Create large objects
            false);        // Don't create garbage
            
        secondaryBatcher_ = std::make_shared<LargeObjectBatcher>();
        
        // Default settings
        maxBatchCount_ = 20;
        maxBatchAge_ = 200; // milliseconds
        
        // Baseline memory
        baselineMemoryUsage_ = GetCurrentMemoryUsage();
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

    // Create SyslogSender
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
    
    // Populate queue with test messages
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
    
    // Run sender
    std::thread runSenderThread(int timeoutMs = 1000) {
        return std::thread([this, timeoutMs]() {
            auto senderThread = std::thread([this]() { sender_->run(); });
            
            // Wait for timeout
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
            
            // Request stop and wait for thread to complete
            sender_->requestStopAndNotify();
            if (senderThread.joinable()) {
                senderThread.join();
            }
        });
    }

    // Common test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<INetworkClient> primaryClient_;
    std::shared_ptr<INetworkClient> secondaryClient_;
    std::shared_ptr<LargeObjectBatcher> primaryBatcher_;
    std::shared_ptr<LargeObjectBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
    SIZE_T baselineMemoryUsage_;
};

// Test LOH fragmentation
TEST_F(SyslogSenderLOHTest, LargeObjectHeapFragmentation) {
    // Create sender
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 100);
    
    // Measure starting memory
    SIZE_T startMemory = GetCurrentMemoryUsage();
    
    // Run multiple processing cycles to create LOH fragmentation
    for (int cycle = 0; cycle < 10; ++cycle) {
        auto senderThread = runSenderThread(500);
        senderThread.join();
        
        // Add more messages between cycles
        populateQueue(primaryQueue_, 10);
        
        // Measure memory after each cycle
        SIZE_T currentMemory = GetCurrentMemoryUsage();
        std::cout << "Cycle " << cycle << " memory: " 
                 << (currentMemory/1024) << "KB, Delta from start: " 
                 << ((currentMemory-startMemory)/1024) << "KB" << std::endl;
    }
    
    // Final memory measurement
    SIZE_T endMemory = GetCurrentMemoryUsage();
    
    // Check batcher stats
    auto largeObjectCount = primaryBatcher_->getLargeObjectCount();
    std::cout << "Final large object count: " << largeObjectCount << std::endl;
    std::cout << "Final memory: " << (endMemory/1024) << "KB, Total delta: " 
             << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    
    // Memory should increase due to LOH fragmentation
    // Since we're creating and releasing large objects
}

// Test LOH fragmentation with garbage
TEST_F(SyslogSenderLOHTest, LOHWithGarbageCreation) {
    // Use batcher that creates garbage along with large objects
    primaryBatcher_ = std::make_shared<LargeObjectBatcher>(
        1024 * 1024,  // 1MB max batch size
        true,         // Create large objects
        true);        // Create garbage
        
    // Create sender
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 50);
    
    // Measure starting memory
    SIZE_T startMemory = GetCurrentMemoryUsage();
    
    // Run processing
    auto senderThread = runSenderThread(3000);
    senderThread.join();
    
    // Measure final memory
    SIZE_T endMemory = GetCurrentMemoryUsage();
    
    std::cout << "LOH with garbage creation: Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    
    // Memory usage should be higher with garbage creation
}

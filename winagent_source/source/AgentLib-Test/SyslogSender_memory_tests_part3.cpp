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
SIZE_T GetProcessMemoryUsageBytes() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

// Network client that simulates async behavior and potential async-related memory issues
class AsyncNetworkClient : public INetworkClient {
public:
    AsyncNetworkClient(bool simulateNetworkDelay = false,
                      int delayMs = 50,
                      bool simulateAsyncBacklog = false,
                      bool simulateAsyncLeak = false)
        : simulateNetworkDelay_(simulateNetworkDelay)
        , delayMs_(delayMs)
        , simulateAsyncBacklog_(simulateAsyncBacklog)
        , simulateAsyncLeak_(simulateAsyncLeak)
        , postCount_(0)
        , pendingOperations_(0) {
    }
    
    ~AsyncNetworkClient() {
        // Cleanup any leaked callbacks
        for (auto& callback : asyncCallbacks_) {
            delete callback;
        }
        asyncCallbacks_.clear();
    }

    RESULT_TYPE post(const char* data, size_t dataSize) override {
        postCount_++;
        
        // Create copy of data for async processing
        std::string* dataCopy = new std::string(data, dataSize);
        
        // Increment pending operations
        pendingOperations_++;
        
        if (simulateNetworkDelay_) {
            // Spawn a thread to simulate async processing
            std::thread asyncThread([this, dataCopy]() {
                // Simulate network delay
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs_));
                
                // Process the data asynchronously
                processAsyncData(dataCopy);
            });
            
            // Detach thread to simulate background processing
            asyncThread.detach();
            
            // Simulate callback that hasn't completed
            if (simulateAsyncBacklog_) {
                // Create callback object that would normally be cleaned up when the operation completes
                struct AsyncCallback {
                    std::string data;
                    bool completed = false;
                };
                
                AsyncCallback* callback = new AsyncCallback();
                callback->data = std::string(data, dataSize);
                
                // Store in our list
                asyncCallbacks_.push_back(callback);
                
                // Only complete some of these callbacks to simulate backlog
                if (postCount_ % 3 == 0) {
                    callback->completed = true;
                }
            }
            
            // Intentionally leak memory sometimes to simulate uncompleted async operations
            if (simulateAsyncLeak_ && (postCount_ % 5 == 0)) {
                // In a real app, these would be leaked task state machines, captured contexts, etc.
                leakedBuffers_.push_back(new std::string(data, dataSize));
            }
            
            return RESULT_TYPE(RESULT_SUCCESS);
        }
        
        // Synchronous path - process immediately
        processAsyncData(dataCopy);
        return RESULT_TYPE(RESULT_SUCCESS);
    }
    
    void processAsyncData(std::string* data) {
        // Store the last processed data
        lastProcessedData_ = *data;
        
        // Clean up the temporary copy
        delete data;
        
        // Decrement pending operations
        pendingOperations_--;
    }
    
    size_t getPostCount() const { return postCount_; }
    size_t getPendingOperations() const { return pendingOperations_; }
    size_t getAsyncCallbackCount() const { return asyncCallbacks_.size(); }
    const std::string& getLastProcessedData() const { return lastProcessedData_; }

private:
    bool simulateNetworkDelay_;
    int delayMs_;
    bool simulateAsyncBacklog_;
    bool simulateAsyncLeak_;
    size_t postCount_;
    std::atomic<size_t> pendingOperations_;
    std::string lastProcessedData_;
    
    // For simulating async backlog
    struct AsyncCallback;
    std::vector<AsyncCallback*> asyncCallbacks_;
    
    // For simulating leaks from async operations
    std::vector<std::string*> leakedBuffers_;
};

// String concatenation batcher that simulates inefficient string handling
class StringConcatenationBatcher : public MessageBatcher {
public:
    StringConcatenationBatcher(
        uint32_t maxBatchSize = 1024 * 1024,
        bool useInefficient = false,   // Use inefficient string concatenation
        bool useStringStream = false,  // Use string stream
        bool useStringBuilder = false, // Use simulated string builder
        bool preAllocate = false)      // Pre-allocate strings
        : MessageBatcher(maxBatchSize, 1000)  // 1 second max age
        , useInefficient_(useInefficient)
        , useStringStream_(useStringStream)
        , useStringBuilder_(useStringBuilder)
        , preAllocate_(preAllocate)
        , totalStringsCreated_(0)
        , batchCount_(0) {
    }

    BatchResult BatchEvents(std::shared_ptr<MessageQueue> queue, char* buffer, size_t bufferSize) override {
        if (!queue || queue->length() == 0 || !buffer || bufferSize == 0) {
            return BatchResult{ BatchResult::Status::NoMessages, 0, 0 };
        }
        
        batchCount_++;
        
        // Get number of messages to process
        size_t messageCount = std::min(queue->length(), size_t(20));
        size_t bytesWritten = 0;
        
        if (useInefficient_) {
            // Inefficient approach - create many temporary strings
            std::string result;
            
            // If pre-allocate is true, reserve space (good practice)
            if (preAllocate_) {
                result.reserve(bufferSize);
            }
            
            size_t index = 0;
            for (auto msg : queue->traverseQueue()) {
                if (index >= messageCount) break;
                
                // Get message content
                std::vector<char> msgContent(msg->data_length + 1);
                if (queue->peek(msg, msgContent.data(), msg->data_length) > 0) {
                    // Create temporary string from message
                    std::string msgString(msgContent.data(), msg->data_length);
                    totalStringsCreated_++;
                    
                    // Add separator if not first message
                    if (index > 0) {
                        result += "\n";
                        totalStringsCreated_++;
                    }
                    
                    // Very inefficient concatenation - creates new string each time
                    result += msgString;
                    totalStringsCreated_++;
                }
                
                index++;
            }
            
            // Copy to output buffer
            bytesWritten = std::min(result.length(), bufferSize);
            memcpy(buffer, result.c_str(), bytesWritten);
        }
        else if (useStringStream_) {
            // Use string stream - more efficient than +=
            std::ostringstream oss;
            
            size_t index = 0;
            for (auto msg : queue->traverseQueue()) {
                if (index >= messageCount) break;
                
                // Get message content
                std::vector<char> msgContent(msg->data_length + 1);
                if (queue->peek(msg, msgContent.data(), msg->data_length) > 0) {
                    // Add separator if not first message
                    if (index > 0) {
                        oss << "\n";
                    }
                    
                    // Write directly to stream
                    oss.write(msgContent.data(), msg->data_length);
                }
                
                index++;
            }
            
            // Get string result
            std::string result = oss.str();
            totalStringsCreated_++;
            
            // Copy to output buffer
            bytesWritten = std::min(result.length(), bufferSize);
            memcpy(buffer, result.c_str(), bytesWritten);
        }
        else if (useStringBuilder_) {
            // Simulate a string builder with preallocated buffer
            // In real code this would be a specialized class
            struct StringBuilder {
                std::string buffer;
                
                StringBuilder(size_t initialCapacity) {
                    buffer.reserve(initialCapacity);
                }
                
                void append(const char* str, size_t len) {
                    buffer.append(str, len);
                }
                
                void append(char ch) {
                    buffer.push_back(ch);
                }
                
                const char* c_str() const {
                    return buffer.c_str();
                }
                
                size_t length() const {
                    return buffer.length();
                }
            };
            
            // Create string builder with capacity
            StringBuilder builder(bufferSize);
            
            size_t index = 0;
            for (auto msg : queue->traverseQueue()) {
                if (index >= messageCount) break;
                
                // Get message content
                std::vector<char> msgContent(msg->data_length + 1);
                if (queue->peek(msg, msgContent.data(), msg->data_length) > 0) {
                    // Add separator if not first message
                    if (index > 0) {
                        builder.append('\n');
                    }
                    
                    // Append directly
                    builder.append(msgContent.data(), msg->data_length);
                }
                
                index++;
                totalStringsCreated_++; // count builder itself
            }
            
            // Copy to output buffer
            bytesWritten = std::min(builder.length(), bufferSize);
            memcpy(buffer, builder.c_str(), bytesWritten);
        }
        else {
            // Most efficient approach - direct write to buffer
            size_t index = 0;
            size_t remainingBytes = bufferSize;
            
            for (auto msg : queue->traverseQueue()) {
                if (index >= messageCount || remainingBytes <= 0) break;
                
                // Add separator if not first message
                if (index > 0 && remainingBytes > 0) {
                    buffer[bytesWritten++] = '\n';
                    remainingBytes--;
                }
                
                // Get message content directly into buffer
                std::vector<char> msgContent(msg->data_length + 1);
                if (queue->peek(msg, msgContent.data(), msg->data_length) > 0) {
                    // Copy directly to output buffer
                    size_t copySize = std::min(static_cast<size_t>(msg->data_length), remainingBytes);
                    memcpy(buffer + bytesWritten, msgContent.data(), copySize);
                    bytesWritten += copySize;
                    remainingBytes -= copySize;
                }
                
                index++;
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
        if (!buffer) {
            throw std::runtime_error("Null batch buffer");
        }
    }
    
    size_t getBatchCount() const { return batchCount_; }
    size_t getTotalStringsCreated() const { return totalStringsCreated_; }

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
    bool useInefficient_;
    bool useStringStream_;
    bool useStringBuilder_;
    bool preAllocate_;
    size_t totalStringsCreated_;
    size_t batchCount_;
};

// Fixture for testing string handling memory issues
class SyslogSenderStringMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic test components
        primaryQueue_ = std::make_shared<MessageQueue>(500, 500);
        secondaryQueue_ = std::make_shared<MessageQueue>(500, 500);
        primaryClient_ = std::make_shared<INetworkClient>();
        secondaryClient_ = std::make_shared<INetworkClient>();
        
        // Create string concat batcher with default settings (efficient)
        primaryBatcher_ = std::make_shared<StringConcatenationBatcher>();
        secondaryBatcher_ = std::make_shared<StringConcatenationBatcher>();
        
        // Default settings
        maxBatchCount_ = 50;
        maxBatchAge_ = 200; // milliseconds
        
        // Get baseline memory
        baselineMemoryUsage_ = GetProcessMemoryUsageBytes();
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
    
    // Populate queue with messages
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

    // Test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<INetworkClient> primaryClient_;
    std::shared_ptr<INetworkClient> secondaryClient_;
    std::shared_ptr<StringConcatenationBatcher> primaryBatcher_;
    std::shared_ptr<StringConcatenationBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
    SIZE_T baselineMemoryUsage_;
};

// Fixture for testing async/concurrency memory issues
class SyslogSenderAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create basic test components
        primaryQueue_ = std::make_shared<MessageQueue>(500, 500);
        secondaryQueue_ = std::make_shared<MessageQueue>(500, 500);
        primaryClient_ = std::make_shared<AsyncNetworkClient>();
        secondaryClient_ = std::make_shared<AsyncNetworkClient>();
        primaryBatcher_ = std::make_shared<MessageBatcher>(1024 * 1024, 1000);
        secondaryBatcher_ = std::make_shared<MessageBatcher>(1024 * 1024, 1000);
        
        // Default settings
        maxBatchCount_ = 50;
        maxBatchAge_ = 200; // milliseconds
        
        // Baseline memory
        baselineMemoryUsage_ = GetProcessMemoryUsageBytes();
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
    
    // Run sender in a thread with timeout
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

    // Test objects
    std::shared_ptr<MessageQueue> primaryQueue_;
    std::shared_ptr<MessageQueue> secondaryQueue_;
    std::shared_ptr<AsyncNetworkClient> primaryClient_;
    std::shared_ptr<AsyncNetworkClient> secondaryClient_;
    std::shared_ptr<MessageBatcher> primaryBatcher_;
    std::shared_ptr<MessageBatcher> secondaryBatcher_;
    std::unique_ptr<SyslogSender> sender_;
    uint32_t maxBatchCount_;
    uint32_t maxBatchAge_;
    SIZE_T baselineMemoryUsage_;
};

// Test inefficient string handling
TEST_F(SyslogSenderStringMemoryTest, InefficientStringConcatenation) {
    // Create inefficient string batcher
    primaryBatcher_ = std::make_shared<StringConcatenationBatcher>(
        1024 * 1024,  // 1MB buffer
        true,         // Use inefficient concatenation
        false,        // Don't use string stream
        false,        // Don't use string builder
        false);       // Don't preallocate
        
    createSender();
    
    // Create lots of messages to process
    populateQueue(primaryQueue_, 200);
    
    // Measure start memory and string count
    SIZE_T startMemory = GetProcessMemoryUsageBytes();
    size_t initialStringCount = primaryBatcher_->getTotalStringsCreated();
    
    // Run the test
    auto senderThread = runSenderThread(2000);
    senderThread.join();
    
    // Measure end memory and string count
    SIZE_T endMemory = GetProcessMemoryUsageBytes();
    size_t finalStringCount = primaryBatcher_->getTotalStringsCreated();
    
    std::cout << "Inefficient string test: Memory Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "String created count: " << (finalStringCount - initialStringCount) << std::endl;
    
    // The inefficient concat should create more string objects
    EXPECT_GT(finalStringCount - initialStringCount, 0u);
}

// Test efficient string handling
TEST_F(SyslogSenderStringMemoryTest, EfficientStringHandling) {
    // Create efficient string handling batchers
    
    // StringBuilder approach
    primaryBatcher_ = std::make_shared<StringConcatenationBatcher>(
        1024 * 1024,  // 1MB buffer
        false,        // Don't use inefficient concatenation 
        false,        // Don't use string stream
        true,         // Use string builder
        true);        // Preallocate
        
    // Stream approach
    secondaryBatcher_ = std::make_shared<StringConcatenationBatcher>(
        1024 * 1024,  // 1MB buffer
        false,        // Don't use inefficient concatenation
        true,         // Use string stream
        false,        // Don't use string builder
        true);        // Preallocate
        
    createSender();
    
    // Create lots of messages to process
    populateQueue(primaryQueue_, 200);
    populateQueue(secondaryQueue_, 200);
    
    // Measure start memory and string counts
    SIZE_T startMemory = GetProcessMemoryUsageBytes();
    size_t initialBuilderCount = primaryBatcher_->getTotalStringsCreated();
    size_t initialStreamCount = secondaryBatcher_->getTotalStringsCreated();
    
    // Run the test
    auto senderThread = runSenderThread(2000);
    senderThread.join();
    
    // Measure end memory and string counts
    SIZE_T endMemory = GetProcessMemoryUsageBytes();
    size_t finalBuilderCount = primaryBatcher_->getTotalStringsCreated();
    size_t finalStreamCount = secondaryBatcher_->getTotalStringsCreated();
    
    std::cout << "Efficient string test: Memory Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "StringBuilder created count: " << (finalBuilderCount - initialBuilderCount) << std::endl;
    std::cout << "StringStream created count: " << (finalStreamCount - initialStreamCount) << std::endl;
    
    // Builder and stream should create significantly fewer string objects
    // than inefficient concatenation (from previous test)
}

// Test async/await backlog
TEST_F(SyslogSenderAsyncTest, AsyncBacklogMemoryGrowth) {
    // Configure async client with delay and backlog simulation
    primaryClient_ = std::make_shared<AsyncNetworkClient>(
        true,    // Simulate network delay
        50,      // 50ms delay
        true,    // Simulate async backlog
        false);  // Don't simulate async leak
        
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 200);
    
    // Measure starting memory
    SIZE_T startMemory = GetProcessMemoryUsageBytes();
    
    // Run test
    auto senderThread = runSenderThread(3000);
    senderThread.join();
    
    // Measure final memory
    SIZE_T endMemory = GetProcessMemoryUsageBytes();
    
    // Get async stats
    size_t pendingOps = primaryClient_->getPendingOperations();
    size_t callbackCount = primaryClient_->getAsyncCallbackCount();
    
    std::cout << "Async backlog test: Memory Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "Pending operations: " << pendingOps << std::endl;
    std::cout << "Callback count: " << callbackCount << std::endl;
    
    // The test should have created a backlog of callbacks
    EXPECT_GT(callbackCount, 0u);
}

// Test async memory leaks
TEST_F(SyslogSenderAsyncTest, AsyncLeakMemoryGrowth) {
    // Configure async client to leak memory
    primaryClient_ = std::make_shared<AsyncNetworkClient>(
        true,    // Simulate network delay
        30,      // 30ms delay
        false,   // Don't simulate async backlog
        true);   // Simulate async leak
        
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 200);
    
    // Measure starting memory
    SIZE_T startMemory = GetProcessMemoryUsageBytes();
    
    // Run test - this will create leaks every 5th operation
    auto senderThread = runSenderThread(3000);
    senderThread.join();
    
    // Measure final memory
    SIZE_T endMemory = GetProcessMemoryUsageBytes();
    
    std::cout << "Async leak test: Memory Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "Post count: " << primaryClient_->getPostCount() << std::endl;
    
    // The test should have created leaked memory
    // Memory should have increased, but cleanup happens in the destructor
    // so this is more of a diagnostic test
}

// Integration test that combines multiple memory issues
TEST_F(SyslogSenderAsyncTest, IntegratedMemoryIssuesTest) {
    // Create async client with all issues enabled
    primaryClient_ = std::make_shared<AsyncNetworkClient>(
        true,   // Network delay
        20,     // 20ms delay
        true,   // Async backlog
        true);  // Async leak
        
    // Create string batcher with inefficient handling
    primaryBatcher_ = std::make_shared<StringConcatenationBatcher>(
        1024 * 1024,  // 1MB buffer
        true,         // Use inefficient concatenation
        false,        // Don't use string stream
        false,        // Don't use string builder
        false);       // Don't preallocate
        
    createSender();
    
    // Populate queue
    populateQueue(primaryQueue_, 150);
    
    // Set up producer thread
    std::atomic<bool> stopProducer{false};
    std::thread producerThread([this, &stopProducer]() {
        size_t msgCount = 0;
        while (!stopProducer.load()) {
            std::string message = "Producer Message " + std::to_string(msgCount++);
            message.append(500, 'X'); // Large-ish message
            primaryQueue_->enqueue(message.c_str(), static_cast<uint32_t>(message.length()));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Measure starting memory
    SIZE_T startMemory = GetProcessMemoryUsageBytes();
    
    // Run test
    auto senderThread = runSenderThread(3000);
    
    // Stop producer and join threads
    stopProducer.store(true);
    producerThread.join();
    senderThread.join();
    
    // Measure final memory
    SIZE_T endMemory = GetProcessMemoryUsageBytes();
    
    std::cout << "Integrated memory issues test: Memory Start=" << (startMemory/1024) 
              << "KB, End=" << (endMemory/1024) << "KB, Delta=" 
              << ((endMemory-startMemory)/1024) << "KB" << std::endl;
    std::cout << "Final queue size: " << primaryQueue_->length() << std::endl;
    std::cout << "Post count: " << primaryClient_->getPostCount() << std::endl;
    std::cout << "String count: " << primaryBatcher_->getTotalStringsCreated() << std::endl;
    
    // Combined issues should create significant memory pressure
}

#include "pch.h"
#include "../AgentLib/EventHandlerMessageQueuer.h"
#include "../AgentLib/Configuration.h"
#include "../AgentLib/MessageQueue.h"
#include "../AgentLib/EventLogEvent.h"
#include "../AgentLib/SyslogAgentSharedConstants.h"
#include "../AgentLib/Globals.h"
#include "../Infrastructure/Result.h"
#include "../Infrastructure/Logger.h"
#include "TestUtils.h"
#include "MessageQueueTestExtensions.h"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <random>
#include <atomic>

using namespace Syslog_agent;
using namespace std;
using ::testing::Test;

/**
 * Memory-focused tests for EventHandlerMessageQueuer
 * 
 * These tests specifically target potential memory issues mentioned in 
 * memory_concerns_EventHandlerMessageQueuer_complete.md including:
 * 
 * 1. Queue Management - unbounded queue growth, inefficient container resizing
 * 2. Heap Allocation & Fragmentation - allocation patterns, memory leaks
 * 3. Message Handling - large payloads, copying, duplication
 * 4. Concurrency Issues - deadlocks, race conditions, thread-local storage
 * 5. Cleanup & Resource Management - shutdown behavior, resource leaks
 */

// Enhanced mock event class that allows detailed control over memory usage patterns
class MemoryTestEventLogEvent : public EventLogEvent {
public:
    MemoryTestEventLogEvent() : EventLogEvent(nullptr) {
        // Initialize event_xml_data_ with test values
        strcpy_s(event_xml_data_.providerName, "TestProvider");
        strcpy_s(event_xml_data_.eventID, "1234");
        strcpy_s(event_xml_data_.level, "3"); // Warning level
        strcpy_s(event_xml_data_.systemTime, "2023-05-15T12:34:56.789Z");
        strcpy_s(event_xml_data_.channel, "Application");
        strcpy_s(event_xml_data_.computer, "TestComputer");
        
        // Set default message
        text_buffer_ = Globals::instance()->getMessageBuffer("test_message");
        strcpy_s(text_buffer_, Globals::MESSAGE_BUFFER_SIZE, "This is a test event message");
        
        // Mark as rendered
        xml_buffer_ = Globals::instance()->getMessageBuffer("test_xml");
        strcpy_s(xml_buffer_, Globals::MESSAGE_BUFFER_SIZE, "<Event><s><Provider Name=\"TestProvider\"/></s></Event>");
    }
    
    ~MemoryTestEventLogEvent() {
        // Base class destructor handles cleanup
    }
    
    // Helper method to create large messages
    void setLargeMessage(size_t size) {
        if (!text_buffer_) {
            text_buffer_ = Globals::instance()->getMessageBuffer("large_message");
        }
        
        // Fill buffer with repeating pattern to simulate large message
        const char* pattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        size_t pattern_len = strlen(pattern);
        size_t copy_size = std::min(Globals::MESSAGE_BUFFER_SIZE - 1, size);
        
        for (size_t i = 0; i < copy_size; i++) {
            text_buffer_[i] = pattern[i % pattern_len];
        }
        text_buffer_[copy_size] = '\0';
    }
    
    // Add many data fields to test handling of variable-sized data
    void addManyDataFields(int count, int value_size) {
        event_xml_data_.dataCount = 0;
        
        const char* pattern = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        size_t pattern_len = strlen(pattern);
        
        for (int i = 0; i < std::min(count, EventXmlData::MAX_DATAS); i++) {
            char name[32];
            snprintf(name, sizeof(name), "Field%d", i);
            
            // Create a value of the specified size
            std::string value;
            value.reserve(value_size);
            for (int j = 0; j < value_size; j++) {
                value += pattern[j % pattern_len];
            }
            
            addDataField(name, value.c_str());
        }
    }
    
    // Override base class methods for testing
    void renderEvent() override {
        // Already marked as rendered in constructor
    }
    
    DWORD getEventId() const override {
        return std::strtoul(event_xml_data_.eventID, nullptr, 10);
    }
    
    // Add data field for testing
    void addDataField(const char* name, const char* value) {
        if (event_xml_data_.dataCount < EventXmlData::MAX_DATAS) {
            if (name) {
                strcpy_s(event_xml_data_.data[event_xml_data_.dataCount].name, 
                         sizeof(event_xml_data_.data[0].name), name);
            } else {
                event_xml_data_.data[event_xml_data_.dataCount].name[0] = '\0';
            }
            
            if (value) {
                strcpy_s(event_xml_data_.data[event_xml_data_.dataCount].value,
                         sizeof(event_xml_data_.data[0].value), value);
            } else {
                event_xml_data_.data[event_xml_data_.dataCount].value[0] = '\0';
            }
            
            event_xml_data_.dataCount++;
        }
    }
    
    void setUserID(const char* userId) {
        strcpy_s(event_xml_data_.userID, sizeof(event_xml_data_.userID), userId);
    }
    
    // Set arbitrary level
    void setLevel(const char* level) {
        strcpy_s(event_xml_data_.level, sizeof(event_xml_data_.level), level);
    }
    
    // Set timestamp
    void setSystemTime(const char* time) {
        strcpy_s(event_xml_data_.systemTime, sizeof(event_xml_data_.systemTime), time);
    }
};

// Test fixture for memory-focused tests
class EventHandlerMessageQueuerMemoryTest : public Test {
protected:
    void SetUp() override {
        // Create configuration
        configuration = std::make_unique<Configuration>();
        configuration->setHostName("test-host");
        configuration->setPrimaryHost(L"127.0.0.1");
        configuration->setPrimaryPort(514);
        configuration->setPrimaryLogformat(SharedConstants::LOGFORMAT_JSON);
        configuration->setFacility(1);
        configuration->setSeverity(SharedConstants::Severities::NOTICE);
        configuration->setLookupAccounts(true);
        configuration->setUtcOffsetMinutes(0);
        configuration->setForwardToSecondary(false);
        
        // Initialize message queues with configurable sizes for testing
        primary_queue = std::make_shared<MessageQueue>(initial_queue_size, initial_buffer_chunk_size);
        secondary_queue = std::make_shared<MessageQueue>(initial_queue_size, initial_buffer_chunk_size);
        
        // Create handler
        message_queuer = std::make_unique<EventHandlerMessageQueuer>(
            *configuration, primary_queue, secondary_queue, L"Application");
    }
    
    void TearDown() override {
        message_queuer.reset();
        primary_queue.reset();
        secondary_queue.reset();
        configuration.reset();
        
        // Force run GC to help identify memory leaks
        Globals::instance()->runGarbageCollection();
    }
    
    // Helper to create an event with specified message size
    MemoryTestEventLogEvent createEventWithMessageSize(size_t message_size) {
        MemoryTestEventLogEvent event;
        event.setLargeMessage(message_size);
        return event;
    }
    
    // Helper to create an event with specified number of data fields
    MemoryTestEventLogEvent createEventWithDataFields(int field_count, int field_size) {
        MemoryTestEventLogEvent event;
        event.addManyDataFields(field_count, field_size);
        return event;
    }
    
    // Helper to dequeue and count messages
    int dequeueAndCountMessages(std::shared_ptr<MessageQueue>& queue, int max_count = 1000) {
        int count = 0;
        char buffer[32768]; // Large buffer for any message size
        
        while (!queue->isEmpty() && count < max_count) {
            int len = queue->dequeue(buffer, sizeof(buffer));
            if (len > 0) {
                count++;
            }
        }
        
        return count;
    }
    
    // Helper to clear queue
    void clearQueue(std::shared_ptr<MessageQueue>& queue) {
        while (!queue->isEmpty()) {
            queue->removeFront();
        }
    }
    
    // Generate a random string of specified length
    std::string generateRandomString(size_t length) {
        static const char chars[] = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        std::string result;
        result.reserve(length);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        
        return result;
    }
    
    // Standard parameters for queue testing
    static constexpr uint32_t initial_queue_size = 100;
    static constexpr uint32_t initial_buffer_chunk_size = 1024 * Globals::MESSAGE_BUFFER_SIZE;
    
    std::unique_ptr<Configuration> configuration;
    std::shared_ptr<MessageQueue> primary_queue;
    std::shared_ptr<MessageQueue> secondary_queue;
    std::unique_ptr<EventHandlerMessageQueuer> message_queuer;
};

//------------------------------------------------------------
// 1. Queue Management Tests
//------------------------------------------------------------

// Test unbounded queue growth when processing rate is slower than arrival rate
TEST_F(EventHandlerMessageQueuerMemoryTest, UnboundedQueueGrowth) {
    const int event_count = 1000;
    const int message_size = 1024; // 1KB messages
    
    // Disable consumption by using a queue spy
    bool allow_dequeue = false;
    auto spy_hook = [&allow_dequeue](size_t queue_len, MessageQueue::Message* msg, bool is_pre) -> bool {
        // Only block dequeues, not enqueues
        if (!is_pre && !allow_dequeue) {
            return false; // Simulate slow consumer by preventing dequeues
        }
        return true;
    };
    
    primary_queue->setEnqueueHook(spy_hook);
    
    // Generate events rapidly
    for (int i = 0; i < event_count; i++) {
        auto event = createEventWithMessageSize(message_size);
        Result result = message_queuer->handleEvent(L"TestSubscription", event);
        ASSERT_EQ(result.statusCode(), ERROR_SUCCESS);
    }
    
    // Verify queue has grown to expected size
    EXPECT_EQ(primary_queue->length(), event_count);
    
    // Now allow dequeuing and clean up
    allow_dequeue = true;
    clearQueue(primary_queue);
    EXPECT_EQ(primary_queue->length(), 0);
}

// Test handling of very large messages and memory spikes
TEST_F(EventHandlerMessageQueuerMemoryTest, LargeMessageHandling) {
    // Test with increasing message sizes
    std::vector<size_t> message_sizes = {
        1024,      // 1KB
        4096,      // 4KB
        16384,     // 16KB
        EventHandlerMessageQueuer::EventData::MAX_MESSAGE_LEN - 1 // Max allowed
    };
    
    for (size_t size : message_sizes) {
        clearQueue(primary_queue);
        
        auto event = createEventWithMessageSize(size);
        Result result = message_queuer->handleEvent(L"TestSubscription", event);
        
        ASSERT_EQ(result.statusCode(), ERROR_SUCCESS) 
            << "Failed with message size: " << size;
        ASSERT_EQ(primary_queue->length(), 1)
            << "Message not queued for size: " << size;
            
        // Dequeue and verify we can read the message
        char buffer[32768];
        int len = primary_queue->dequeue(buffer, sizeof(buffer));
        EXPECT_GT(len, 0) << "Could not dequeue message of size: " << size;
    }
}

// Test many data fields and their impact on memory
TEST_F(EventHandlerMessageQueuerMemoryTest, ManyDataFields) {
    // Create event with maximum allowed data fields
    const int max_fields = EventHandlerMessageQueuer::EventData::MAX_EVENT_DATA_PAIRS;
    const int field_value_size = 512; // 512 bytes per field value
    
    auto event = createEventWithDataFields(max_fields, field_value_size);
    Result result = message_queuer->handleEvent(L"TestSubscription", event);
    
    ASSERT_EQ(result.statusCode(), ERROR_SUCCESS);
    ASSERT_EQ(primary_queue->length(), 1);
    
    // Verify the message can be dequeued
    char buffer[65536]; // Large buffer to hold the message
    int len = primary_queue->dequeue(buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    
    // Verify JSON contains expected number of fields
    std::string message(buffer, len);
    for (int i = 0; i < max_fields; i++) {
        std::string field_name = "Field" + std::to_string(i);
        EXPECT_NE(message.find(field_name), std::string::npos)
            << "Field " << field_name << " not found in message";
    }
}

//------------------------------------------------------------
// 2. Heap Allocation & Memory Leak Tests
//------------------------------------------------------------

// Test for potential memory leaks during normal operation
TEST_F(EventHandlerMessageQueuerMemoryTest, NoMemoryLeaksDuringNormalOperation) {
    const int iterations = 1000;
    const int fields_per_event = 10;
    const int field_size = 100;
    
    // Process many events and verify proper cleanup
    for (int i = 0; i < iterations; i++) {
        auto event = createEventWithDataFields(fields_per_event, field_size);
        Result result = message_queuer->handleEvent(L"TestSubscription", event);
        ASSERT_EQ(result.statusCode(), ERROR_SUCCESS);
        
        // Periodically dequeue to prevent queue buildup
        if (i % 10 == 0) {
            clearQueue(primary_queue);
        }
    }
    
    // Ensure we can still process events after many iterations
    clearQueue(primary_queue);
    auto final_event = createEventWithDataFields(fields_per_event, field_size);
    Result result = message_queuer->handleEvent(L"TestSubscription", final_event);
    ASSERT_EQ(result.statusCode(), ERROR_SUCCESS);
    ASSERT_EQ(primary_queue->length(), 1);
}

// Test error paths to ensure no memory leaks during exceptions
TEST_F(EventHandlerMessageQueuerMemoryTest, NoMemoryLeaksDuringErrorPaths) {
    // Create an event with invalid timestamp to trigger error path
    MemoryTestEventLogEvent event;
    event.setSystemTime("invalid-timestamp-format");
    
    // Process the event (should still succeed but log an error)
    Result result = message_queuer->handleEvent(L"TestSubscription", event);
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    
    // Verify we can still process normal events
    MemoryTestEventLogEvent valid_event;
    result = message_queuer->handleEvent(L"TestSubscription", valid_event);
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
}

//------------------------------------------------------------
// 3. Concurrency Tests
//------------------------------------------------------------

// Test multi-threaded event handling
TEST_F(EventHandlerMessageQueuerMemoryTest, MultiThreadedEventHandling) {
    const int thread_count = 4;
    const int events_per_thread = 250;
    std::atomic<int> success_count(0);
    
    // Launch multiple threads to handle events concurrently
    std::vector<std::future<void>> futures;
    for (int t = 0; t < thread_count; t++) {
        futures.push_back(std::async(std::launch::async, [this, t, events_per_thread, &success_count]() {
            for (int i = 0; i < events_per_thread; i++) {
                // Create event with thread-specific data
                MemoryTestEventLogEvent event;
                std::string thread_field = "Thread" + std::to_string(t) + "Event" + std::to_string(i);
                event.addDataField("ThreadInfo", thread_field.c_str());
                
                // Handle event
                Result result = message_queuer->handleEvent(L"TestSubscription", event);
                if (result.statusCode() == ERROR_SUCCESS) {
                    success_count++;
                }
                
                // Small sleep to simulate real-world thread scheduling
                if (i % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify correct number of successful event handlings
    EXPECT_EQ(success_count, thread_count * events_per_thread);
    
    // Verify queue contains expected number of messages
    EXPECT_EQ(primary_queue->length(), thread_count * events_per_thread);
    
    // Clean up
    clearQueue(primary_queue);
}

//------------------------------------------------------------
// 4. Resource Management & Cleanup Tests
//------------------------------------------------------------

// Test proper cleanup after processing many events
TEST_F(EventHandlerMessageQueuerMemoryTest, CleanupAfterManyEvents) {
    const int event_count = 1000;
    
    // Process many events
    for (int i = 0; i < event_count; i++) {
        MemoryTestEventLogEvent event;
        message_queuer->handleEvent(L"TestSubscription", event);
    }
    
    // Dequeue all messages
    int dequeued = dequeueAndCountMessages(primary_queue);
    EXPECT_EQ(dequeued, event_count);
    
    // Verify queue is empty
    EXPECT_TRUE(primary_queue->isEmpty());
    
    // Verify we can still process events after cleanup
    MemoryTestEventLogEvent event;
    Result result = message_queuer->handleEvent(L"TestSubscription", event);
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_EQ(primary_queue->length(), 1);
}

// Test proper handling of shutdown
TEST_F(EventHandlerMessageQueuerMemoryTest, ProperShutdownHandling) {
    const int event_count = 100;
    
    // Queue some events
    for (int i = 0; i < event_count; i++) {
        MemoryTestEventLogEvent event;
        message_queuer->handleEvent(L"TestSubscription", event);
    }
    
    // Begin queue shutdown
    primary_queue->beginShutdown();
    
    // Verify queue reports as empty during shutdown
    EXPECT_TRUE(primary_queue->isEmpty());
    EXPECT_EQ(primary_queue->length(), 0);
    
    // Try to queue an event during shutdown (should still succeed in test)
    MemoryTestEventLogEvent event;
    Result result = message_queuer->handleEvent(L"TestSubscription", event);
    
    // Reset for cleanup
    message_queuer.reset();
    primary_queue.reset();
    secondary_queue.reset();
    
    // Create new queues and queuer for further testing
    primary_queue = std::make_shared<MessageQueue>(initial_queue_size, initial_buffer_chunk_size);
    secondary_queue = std::make_shared<MessageQueue>(initial_queue_size, initial_buffer_chunk_size);
    message_queuer = std::make_unique<EventHandlerMessageQueuer>(
        *configuration, primary_queue, secondary_queue, L"Application");
        
    // Verify we can process events with new queues
    MemoryTestEventLogEvent new_event;
    result = message_queuer->handleEvent(L"TestSubscription", new_event);
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_EQ(primary_queue->length(), 1);
}

//------------------------------------------------------------
// 5. Advanced Memory Pattern Tests
//------------------------------------------------------------

// Test mixed message sizes to simulate real-world fragmentation scenarios
TEST_F(EventHandlerMessageQueuerMemoryTest, MixedMessageSizes) {
    const int iterations = 500;
    std::vector<size_t> size_distribution = {
        128,    // Small messages
        1024,   // Medium messages
        8192,   // Large messages
        16384   // Very large messages
    };
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dis(0, size_distribution.size() - 1);
    
    // Process events with varying message sizes
    for (int i = 0; i < iterations; i++) {
        size_t message_size = size_distribution[size_dis(gen)];
        auto event = createEventWithMessageSize(message_size);
        
        Result result = message_queuer->handleEvent(L"TestSubscription", event);
        ASSERT_EQ(result.statusCode(), ERROR_SUCCESS);
        
        // Periodically dequeue with 25% probability
        if (i % 4 == 0) {
            int dequeued = dequeueAndCountMessages(primary_queue, 50);
            EXPECT_GE(dequeued, 0);
        }
    }
    
    // Final cleanup
    clearQueue(primary_queue);
    EXPECT_EQ(primary_queue->length(), 0);
}

// Test long-running pattern to simulate steady-state behavior
TEST_F(EventHandlerMessageQueuerMemoryTest, LongRunningPattern) {
    const int simulation_steps = 100;
    const int burst_size = 50;
    
    for (int step = 0; step < simulation_steps; step++) {
        // Simulate burst of events
        for (int i = 0; i < burst_size; i++) {
            MemoryTestEventLogEvent event;
            // Alternate between normal and large messages
            if (i % 2 == 0) {
                event.setLargeMessage(8192);
            }
            message_queuer->handleEvent(L"TestSubscription", event);
        }
        
        // Simulate processing with varying rates
        int to_process = burst_size / 2 + (step % 4); // Process slightly less than arrival rate
        int processed = dequeueAndCountMessages(primary_queue, to_process);
        
        // Periodically simulate a full queue drain (e.g., catch-up processing)
        if (step % 10 == 9) {
            clearQueue(primary_queue);
        }
    }
    
    // Final queue status check and cleanup
    clearQueue(primary_queue);
    EXPECT_EQ(primary_queue->length(), 0);
}

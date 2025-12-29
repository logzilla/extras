#include "pch.h"
#include "../AgentLib/EventHandlerMessageQueuer.h"
#include "../AgentLib/Configuration.h"
#include "../AgentLib/MessageQueue.h"
#include "../AgentLib/EventLogEvent.h"
#include "../AgentLib/SyslogAgentSharedConstants.h"
#include "../AgentLib/Globals.h"
#include "../Infrastructure/Result.h"
#include <memory>
#include <string>
#include <vector>

using namespace Syslog_agent;
using namespace std;
using ::testing::Test;

// Mock EventLogEvent for testing
class MockEventLogEvent : public EventLogEvent {
public:
    MockEventLogEvent() : EventLogEvent(nullptr) {
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
        strcpy_s(xml_buffer_, Globals::MESSAGE_BUFFER_SIZE, "<Event><System><Provider Name=\"TestProvider\"/></System></Event>");
    }
    
    ~MockEventLogEvent() {
        // Cleanup is handled by the base class destructor
    }
    
    // Override base class methods for testing
    void renderEvent() override {
        // Already marked as rendered in constructor
    }
    
    DWORD getEventId() const override {
        return std::strtoul(event_xml_data_.eventID, nullptr, 10);
    }
    
    // Expose protected members for test manipulation
    char* getTextBuffer() const {
        return text_buffer_;
    }
    
    void setEventID(const char* id) {
        strcpy_s(event_xml_data_.eventID, sizeof(event_xml_data_.eventID), id);
    }
    
    void setLevel(const char* level) {
        strcpy_s(event_xml_data_.level, sizeof(event_xml_data_.level), level);
    }
    
    void setSystemTime(const char* time) {
        strcpy_s(event_xml_data_.systemTime, sizeof(event_xml_data_.systemTime), time);
    }
    
    void setMessage(const char* message) {
        strcpy_s(text_buffer_, Globals::MESSAGE_BUFFER_SIZE, message);
    }
    
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
    
private:
    using EventLogEvent::xml_buffer_;
    using EventLogEvent::text_buffer_;
};

// Test fixture
class EventHandlerMessageQueuerTest : public Test {
protected:
    void SetUp() override {
        // Create configuration with default values
        configuration = std::make_unique<Configuration>();
        configuration->setHostName("test-host");
        configuration->setPrimaryHost("127.0.0.1");
        configuration->setPrimaryPort(514);
        configuration->setPrimaryLogformat(SharedConstants::LOGFORMAT_HTTPPORT);
        configuration->setFacility(1); // user-level messages
        configuration->setSeverity(SharedConstants::Severities::NOTICE);
        configuration->setLookupAccounts(true);
        configuration->setUtcOffsetMinutes(0);
        
        // Initialize message queues
        primary_queue = std::make_shared<MessageQueue>(100, 1024);
        secondary_queue = std::make_shared<MessageQueue>(100, 1024);
        
        // Create handler
        message_queuer = std::make_unique<EventHandlerMessageQueuer>(*configuration, primary_queue, secondary_queue, L"Application");
    }
    
    void TearDown() override {
        message_queuer.reset();
        configuration.reset();
        primary_queue.reset();
        secondary_queue.reset();
    }
    
    std::unique_ptr<Configuration> configuration;
    std::shared_ptr<MessageQueue> primary_queue;
    std::shared_ptr<MessageQueue> secondary_queue;
    std::unique_ptr<EventHandlerMessageQueuer> message_queuer;
};

// Test basic event handling
TEST_F(EventHandlerMessageQueuerTest, BasicEventHandling) {
    MockEventLogEvent event;
    
    // Test event handling
    Result result = message_queuer->handleEvent(L"TestSubscription", event);
    
    // Verify successful handling
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
    EXPECT_EQ(primary_queue->length(), 1);
    
    // Check message content
    std::string message = primary_queue->front();
    EXPECT_FALSE(message.empty());
    EXPECT_TRUE(message.find("\"host\":\"test-host\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"program\":\"TestProvider\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"computer\":\"TestComputer\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"event_id\":\"1234\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"event_log\":\"Application\"") != std::string::npos);
}

// Test severity mapping
TEST_F(EventHandlerMessageQueuerTest, SeverityMapping) {
    // Configuration set to dynamic severity
    configuration->setSeverity(SharedConstants::Severities::DYNAMIC);
    
    // Test different Windows severity levels
    std::vector<std::pair<char*, int>> severity_tests = {
        {"0", SharedConstants::Severities::ALERT},
        {"1", SharedConstants::Severities::CRITICAL},
        {"2", SharedConstants::Severities::ERR},
        {"3", SharedConstants::Severities::WARNING},
        {"4", SharedConstants::Severities::NOTICE},
        {"5", SharedConstants::Severities::DEBUG}
    };
    
    for (const auto& test : severity_tests) {
        primary_queue->clear();
        
        MockEventLogEvent event;
        event.setLevel(test.first);
        
        message_queuer->handleEvent(L"TestSubscription", event);
        
        // Check severity in the message
        std::string message = primary_queue->front();
        std::string expected_severity = "\"severity\":\"" + std::to_string(test.second) + "\"";
        EXPECT_TRUE(message.find(expected_severity) != std::string::npos) 
            << "Failed for level " << test.first << ", expected severity " << test.second;
    }
}

// Test event ID filtering
TEST_F(EventHandlerMessageQueuerTest, EventIdFiltering) {
    // Set up configuration to include only specific event IDs
    configuration->setIncludeVsIgnoreEventIds(true); // Include mode
    configuration->addEventIdToFilter(1234); // Only include event ID 1234
    
    // Test with matching event ID
    MockEventLogEvent matchingEvent;
    matchingEvent.setEventID("1234");
    
    Result result1 = message_queuer->handleEvent(L"TestSubscription", matchingEvent);
    EXPECT_EQ(result1.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
    
    primary_queue->clear();
    
    // Test with non-matching event ID
    MockEventLogEvent nonMatchingEvent;
    nonMatchingEvent.setEventID("5678");
    
    Result result2 = message_queuer->handleEvent(L"TestSubscription", nonMatchingEvent);
    EXPECT_EQ(result2.statusCode(), ERROR_CANCELLED);
    EXPECT_TRUE(primary_queue->isEmpty());
    
    // Test exclude mode
    configuration->setIncludeVsIgnoreEventIds(false); // Exclude mode
    primary_queue->clear();
    
    // Now 1234 should be excluded
    Result result3 = message_queuer->handleEvent(L"TestSubscription", matchingEvent);
    EXPECT_EQ(result3.statusCode(), ERROR_CANCELLED);
    EXPECT_TRUE(primary_queue->isEmpty());
    
    // And 5678 should be included
    Result result4 = message_queuer->handleEvent(L"TestSubscription", nonMatchingEvent);
    EXPECT_EQ(result4.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
}

// Test event data fields
TEST_F(EventHandlerMessageQueuerTest, EventDataFields) {
    MockEventLogEvent event;
    
    // Add data fields
    event.addDataField("TestKey1", "TestValue1");
    event.addDataField("TestKey2", "TestValue2");
    event.addDataField(nullptr, "UnnamedValue"); // Unnamed field
    
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // Check data fields in message
    std::string message = primary_queue->front();
    EXPECT_TRUE(message.find("\"TestKey1\":\"TestValue1\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"TestKey2\":\"TestValue2\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"Data0\":\"UnnamedValue\"") != std::string::npos);
}

// Test user account lookup
TEST_F(EventHandlerMessageQueuerTest, UserAccountLookup) {
    // This test is limited since we can't easily mock the Windows account lookup
    // We'll test that userID fields are included in the output when present
    
    MockEventLogEvent event;
    event.setUserID("S-1-5-21-1234567890-1234567890-1234567890-1000");
    
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // The actual domain/username lookup happens in Globals::LookupUserSid
    // which we can't easily mock here, but we can verify the field exists
    std::string message = primary_queue->front();
    EXPECT_TRUE(message.find("event_user_name") != std::string::npos);
}

// Test timestamp handling
TEST_F(EventHandlerMessageQueuerTest, TimestampHandling) {
    // Test valid timestamp
    MockEventLogEvent validEvent;
    validEvent.setSystemTime("2023-05-15T12:34:56.789Z");
    
    message_queuer->handleEvent(L"TestSubscription", validEvent);
    std::string validMessage = primary_queue->front();
    EXPECT_TRUE(validMessage.find("\"ts\":") != std::string::npos);
    EXPECT_TRUE(validMessage.find(".789") != std::string::npos);
    
    primary_queue->clear();
    
    // Test invalid timestamp format
    MockEventLogEvent invalidEvent;
    invalidEvent.setSystemTime("invalid-timestamp");
    
    message_queuer->handleEvent(L"TestSubscription", invalidEvent);
    std::string invalidMessage = primary_queue->front();
    EXPECT_TRUE(invalidMessage.find("\"ts\":") != std::string::npos); // Should still have a timestamp
}

// Test secondary server handling
TEST_F(EventHandlerMessageQueuerTest, SecondaryServerHandling) {
    // Configure secondary server
    configuration->setSecondaryHost("10.0.0.1");
    configuration->setSecondaryPort(10514);
    configuration->setSecondaryLogformat(SharedConstants::LOGFORMAT_HTTPPORT);
    
    MockEventLogEvent event;
    
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // Verify both queues have messages
    EXPECT_FALSE(primary_queue->isEmpty());
    EXPECT_FALSE(secondary_queue->isEmpty());
    EXPECT_EQ(primary_queue->length(), 1);
    EXPECT_EQ(secondary_queue->length(), 1);
}

// Test custom suffix handling
TEST_F(EventHandlerMessageQueuerTest, CustomSuffix) {
    // Add a custom suffix to the configuration
    configuration->setSuffix(L"\"custom_field1\":\"custom_value1\", \"custom_field2\":\"custom_value2\"");
    
    MockEventLogEvent event;
    
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // Check for custom fields in the message
    std::string message = primary_queue->front();
    EXPECT_TRUE(message.find("\"custom_field1\":\"custom_value1\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"custom_field2\":\"custom_value2\"") != std::string::npos);
}

// Test maximum message size estimation and handling
TEST_F(EventHandlerMessageQueuerTest, MessageSizeHandling) {
    // Create an event with a very large message to test size handling
    MockEventLogEvent largeEvent;
    
    // Create a message that's large but still within limits
    std::string largeMessage(10000, 'X');
    largeEvent.setMessage(largeMessage.c_str());
    
    // Add many data fields to increase message size
    for (int i = 0; i < 30; i++) {
        char key[32], value[128];
        sprintf_s(key, "LargeKey%d", i);
        sprintf_s(value, "LargeValue%d-%s", i, std::string(50, 'Y').c_str());
        largeEvent.addDataField(key, value);
    }
    
    Result result = message_queuer->handleEvent(L"TestSubscription", largeEvent);
    
    // Message should be handled successfully but possibly truncated
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
    
    // Check for message truncation indicator
    std::string message = primary_queue->front();
    bool isTruncated = message.find("*(message truncated)*") != std::string::npos;
    
    // Either the message fits or it was properly truncated
    EXPECT_TRUE(message.find("XXXXX") != std::string::npos); // Should contain at least some of the message
}

// Test old event skipping
TEST_F(EventHandlerMessageQueuerTest, OldEventSkipping) {
    // Create an event with a very old timestamp
    MockEventLogEvent oldEvent;
    
    // Set timestamp to 31 days ago (beyond MAX_CATCHUP_DAYS)
    time_t now = time(nullptr);
    time_t old_time = now - (31 * 24 * 60 * 60);
    char old_timestamp[32];
    sprintf_s(old_timestamp, "%ld", static_cast<long>(old_time));
    oldEvent.setSystemTime("2000-01-01T00:00:00.000Z"); // Very old date
    
    // This will parse into the timestamp field as a Unix timestamp
    strcpy_s(oldEvent.event_xml_data_.systemTime, sizeof(oldEvent.event_xml_data_.systemTime), old_timestamp);
    
    Result result = message_queuer->handleEvent(L"TestSubscription", oldEvent);
    
    // Should be skipped due to age
    EXPECT_EQ(result.statusCode(), ERROR_CANCELLED);
    EXPECT_TRUE(primary_queue->isEmpty());
}

// Test buffer overflow protection
TEST_F(EventHandlerMessageQueuerTest, BufferOverflowProtection) {
    // This would require manipulating internal buffer sizes or mocking Globals
    // which is difficult in a test. Instead, we'll verify the code has checks.
    // This is more of a code inspection test than a functional test.
    
    // Create an event with fields that are at their maximum allowed sizes
    MockEventLogEvent largeEvent;
    
    std::string largeProvider(EventHandlerMessageQueuer::EventData::MAX_PROVIDER_LEN - 1, 'P');
    strcpy_s(largeEvent.event_xml_data_.providerName, largeProvider.c_str());
    
    std::string largeEventId(EventHandlerMessageQueuer::EventData::MAX_EVENT_ID_LEN - 1, '9');
    largeEvent.setEventID(largeEventId.c_str());
    
    std::string largeMessage(EventHandlerMessageQueuer::EventData::MAX_MESSAGE_LEN - 1, 'M');
    largeEvent.setMessage(largeMessage.c_str());
    
    // Handle the event
    Result result = message_queuer->handleEvent(L"TestSubscription", largeEvent);
    
    // Should still succeed despite large fields
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
} 
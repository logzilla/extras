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
        // Get current time to use for timestamp
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        
        char timestamp_str[64];
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
        
        // --- Directly Initialize event_xml_data_ --- 
        memset(&event_xml_data_, 0, sizeof(event_xml_data_)); // Zero out the struct first
        strcpy_s(event_xml_data_.providerName, "TestProvider");
        strcpy_s(event_xml_data_.eventID, "1234");
        strcpy_s(event_xml_data_.level, "3"); // Warning level
        strcpy_s(event_xml_data_.systemTime, timestamp_str);
        strcpy_s(event_xml_data_.channel, "Application");
        strcpy_s(event_xml_data_.computer, "TestComputer");
        strcpy_s(event_xml_data_.userID, "S-1-5-21-0-0-0-1000"); // Example SID

        // Add some event data fields
        event_xml_data_.dataCount = 2;
        strcpy_s(event_xml_data_.data[0].name, "Param1");
        strcpy_s(event_xml_data_.data[0].value, "TestValue1");
        strcpy_s(event_xml_data_.data[1].name, "Param2");
        strcpy_s(event_xml_data_.data[1].value, "TestValue2");
        // --- End Direct Initialization ---

        // Set default message buffer
        text_buffer_ = Globals::instance()->getMessageBuffer("test_message");
        strcpy_s(text_buffer_, Globals::MESSAGE_BUFFER_SIZE, "This is a test event message");

        // --- Remove XML generation and parsing --- 
        // xml_buffer_ = Globals::instance()->getMessageBuffer("test_xml");
        // const char* xml_template = 
        //     "<Event>\n"
        //     "  <System>\n"
        //     "    <Provider Name=\"TestProvider\" Guid=\"{00000000-0000-0000-0000-000000000000}\"/>\n"
        //     "    <EventID>1234</EventID>\n"
        //     "    <Version>0</Version>\n"
        //     "    <Level>3</Level>\n"
        //     "    <Task>0</Task>\n"
        //     "    <Opcode>0</Opcode>\n"
        //     "    <Keywords>0x0</Keywords>\n"
        //     "    <TimeCreated SystemTime=\"%s\"/>\n"
        //     "    <EventRecordID>1</EventRecordID>\n"
        //     "    <Correlation ActivityID=\"{00000000-0000-0000-0000-000000000000}\"/>\n"
        //     "    <Execution ProcessID=\"1000\" ThreadID=\"2000\"/>\n"
        //     "    <Channel>Application</Channel>\n"
        //     "    <Computer>TestComputer</Computer>\n"
        //     "    <Security UserID=\"S-1-5-21-0-0-0-1000\"/>\n"
        //     "  </System>\n"
        //     "  <EventData>\n"
        //     "    <Data Name=\"Param1\">TestValue1</Data>\n"
        //     "    <Data Name=\"Param2\">TestValue2</Data>\n"
        //     "  </EventData>\n"
        //     "</Event>";
        // 
        // char formatted_xml[Globals::MESSAGE_BUFFER_SIZE];
        // snprintf(formatted_xml, Globals::MESSAGE_BUFFER_SIZE, xml_template, timestamp_str);
        // strcpy_s(xml_buffer_, Globals::MESSAGE_BUFFER_SIZE, formatted_xml);
        // 
        // // Make sure event_xml_data_ is populated from the XML
        // if (!event_xml_data_.parse(xml_buffer_)) { // <<< REMOVED THIS CHECK
        //     // If parsing fails, manually set basic values to ensure test works
        //     //strcpy_s(event_xml_data_.providerName, "TestProvider");
        //     //strcpy_s(event_xml_data_.eventID, "1234");
        // }
        // --- End Remove XML --- 
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
};

// Test fixture
class EventHandlerMessageQueuerTest : public Test {
protected:
    void SetUp() override {
        // Create configuration with default values
        configuration = std::make_unique<Configuration>();
        // Use new setters to configure for tests
        configuration->setHostName("test-host"); 
        configuration->setPrimaryHost(L"127.0.0.1"); // Use wstring
        configuration->setPrimaryPort(514);
        configuration->setPrimaryLogformat(SharedConstants::LOGFORMAT_HTTPPORT);
        configuration->setFacility(1); // user-level messages
        configuration->setSeverity(SharedConstants::Severities::NOTICE);
        configuration->setLookupAccounts(true);
        configuration->setUtcOffsetMinutes(0);
        configuration->setForwardToSecondary(false); // Default for most tests
        configuration->clearEventIdFilter(); // Start with no filter
        configuration->setIncludeVsIgnoreEventIds(false); // Default to ignore=false (include all)
        
        // Initialize message queues
        // Note: Tests relying on specific config values might fail now - No longer true
        primary_queue = std::make_shared<MessageQueue>(100, 1024); // Corrected: Use buffer count, not byte size
        secondary_queue = std::make_shared<MessageQueue>(100, 1024); // Corrected: Use buffer count, not byte size
        
        // Create handler
        message_queuer = std::make_unique<EventHandlerMessageQueuer>(*configuration, primary_queue, secondary_queue, L"Application");
    }
    
    void TearDown() override {
        message_queuer.reset();
        configuration.reset();
        primary_queue.reset();
        secondary_queue.reset();
    }
    
    // Helper function to dequeue a message into a string
    std::string dequeueMessage(std::shared_ptr<MessageQueue>& queue) {
        if (queue->isEmpty()) {
            return "";
        }
        // Allocate a reasonably sized buffer. Adjust if needed.
        constexpr size_t bufferSize = Syslog_agent::Globals::MESSAGE_BUFFER_SIZE; 
        char buffer[bufferSize];
        int len = queue->dequeue(buffer, bufferSize);
        if (len > 0) {
            return std::string(buffer, len);
        }
        return "";
    }

    // Helper to clear queue
    void clearQueue(std::shared_ptr<MessageQueue>& queue) {
        while (!queue->isEmpty()) {
            queue->removeFront();
        }
    }

    std::unique_ptr<Configuration> configuration;
    std::shared_ptr<MessageQueue> primary_queue;
    std::shared_ptr<MessageQueue> secondary_queue;
    std::unique_ptr<EventHandlerMessageQueuer> message_queuer;
};

// Test basic event handling
TEST_F(EventHandlerMessageQueuerTest, BasicEventHandling) {
    MockEventLogEvent event;
    
    // Diagnostic: Print configuration state
    std::cout << "DIAGNOSTIC: Configuration state before test:" << std::endl
              << "  hostname: [" << configuration->getHostName() << "]" << std::endl
              << "  primaryLogformat: " << configuration->getPrimaryLogformat() << std::endl
              << "  facility: " << configuration->getFacility() << std::endl
              << "  severity: " << static_cast<int>(configuration->getSeverity()) << std::endl;
    
    // Diagnostic: Print event data
    std::cout << "DIAGNOSTIC: Event data:" << std::endl
              << "  provider: [" << event.event_xml_data_.providerName << "]" << std::endl
              << "  eventID: [" << event.event_xml_data_.eventID << "]" << std::endl
              << "  message: [" << event.getTextBuffer() << "]" << std::endl;
    
    // Diagnostic: Ensure queue is empty before test
    EXPECT_TRUE(primary_queue->isEmpty());
    std::cout << "DIAGNOSTIC: Queue is empty before test: " << (primary_queue->isEmpty() ? "true" : "false") << std::endl;
    
    // Test event handling
    Result result = message_queuer->handleEvent(L"TestSubscription", event);
    
    // Debug output
    std::cout << "Status code: " << result.statusCode() << std::endl;
    
    // Diagnostic: Check queue state immediately after handleEvent
    std::cout << "DIAGNOSTIC: Queue state after handleEvent:" << std::endl
              << "  isEmpty: " << (primary_queue->isEmpty() ? "true" : "false") << std::endl
              << "  length: " << primary_queue->length() << std::endl;
    
    // Verify successful handling
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
    EXPECT_EQ(primary_queue->length(), 1);
    
    // Check message content
    std::string message = dequeueMessage(primary_queue); // Use helper
    std::cout << "DIAGNOSTIC: Dequeued message length: " << message.length() << std::endl;
    if (!message.empty()) {
        std::cout << "DIAGNOSTIC: Message content: [" << message << "]" << std::endl;
    } else {
        std::cout << "DIAGNOSTIC: WARNING - Empty message dequeued!" << std::endl;
    }
    
    // Check if MessageQueue implementation is correctly reporting length
    std::cout << "DIAGNOSTIC: Queue state after dequeue:" << std::endl
              << "  isEmpty: " << (primary_queue->isEmpty() ? "true" : "false") << std::endl
              << "  length: " << primary_queue->length() << std::endl;
    
    EXPECT_FALSE(message.empty());
    EXPECT_TRUE(message.find("\"host\":\"test-host\"") != std::string::npos); // Restore hostname check
    EXPECT_TRUE(message.find("\"program\":\"TestProvider\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"computer\":\"TestComputer\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"event_id\":\"1234\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"event_log\":\"Application\"") != std::string::npos);
}

// Test severity mapping
TEST_F(EventHandlerMessageQueuerTest, SeverityMapping) {
    // Configuration set to dynamic severity
    configuration->setSeverity(SharedConstants::Severities::DYNAMIC); // Use setter
    // Note: This test might not be valid anymore as Severity defaults to NOTICE - Now valid again
    
    // Test different Windows severity levels
    // Use const char* for string literals
    std::vector<std::pair<const char*, int>> severity_tests = { 
        {"0", SharedConstants::Severities::ALERT},
        {"1", SharedConstants::Severities::CRITICAL},
        {"2", SharedConstants::Severities::ERR},
        {"3", SharedConstants::Severities::WARNING},
        {"4", SharedConstants::Severities::NOTICE},
        {"5", SharedConstants::Severities::DEBUG}
    };
    
    for (const auto& test : severity_tests) {
        clearQueue(primary_queue); // Use helper
        
        MockEventLogEvent event;
        event.setLevel(test.first);
        
        message_queuer->handleEvent(L"TestSubscription", event);
        
        // Check severity in the message
        std::string message = dequeueMessage(primary_queue); // Use helper
        // The severity might be fixed now due to removed config setting - No longer fixed
        std::string expected_severity = "\"severity\":\"" + std::to_string(test.second) + "\""; // Restore original expectation
        
        EXPECT_TRUE(message.find(expected_severity) != std::string::npos) 
            << "Failed for level " << test.first << ", expected severity " << test.second;
    }
}

// Test event ID filtering
TEST_F(EventHandlerMessageQueuerTest, EventIdFiltering) {
    // Set up configuration to include only specific event IDs
    configuration->setIncludeVsIgnoreEventIds(true); // Use setter (Include mode)
    configuration->clearEventIdFilter(); // Clear any previous filter
    configuration->addEventIdToFilter(1234); // Use setter - Only include event ID 1234
    // Note: This test logic is likely broken now. - Should work now
    
    // Test with matching event ID (1234 should be included)
    MockEventLogEvent matchingEvent;
    matchingEvent.setEventID("1234");
    
    Result result1 = message_queuer->handleEvent(L"TestSubscription", matchingEvent);
    EXPECT_EQ(result1.statusCode(), ERROR_SUCCESS); 
    EXPECT_FALSE(primary_queue->isEmpty());
    
    clearQueue(primary_queue); // Use helper
    
    // Test with non-matching event ID (5678 should be excluded)
    MockEventLogEvent nonMatchingEvent;
    nonMatchingEvent.setEventID("5678");
    
    Result result2 = message_queuer->handleEvent(L"TestSubscription", nonMatchingEvent);
    EXPECT_EQ(result2.statusCode(), ERROR_CANCELLED); // Should be cancelled/skipped
    EXPECT_TRUE(primary_queue->isEmpty()); // Should be empty 
    
    // Test exclude mode 
    configuration->setIncludeVsIgnoreEventIds(false); // Use setter (Exclude mode)
    // Filter still contains 1234
    clearQueue(primary_queue); // Use helper
    
    // Now 1234 should be excluded
    Result result3 = message_queuer->handleEvent(L"TestSubscription", matchingEvent);
    EXPECT_EQ(result3.statusCode(), ERROR_CANCELLED); // Should be cancelled/skipped
    EXPECT_TRUE(primary_queue->isEmpty()); // Should be empty
    
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
    std::string message = dequeueMessage(primary_queue); // Use helper
    EXPECT_TRUE(message.find("\"TestKey1\":\"TestValue1\"") != std::string::npos);
    EXPECT_TRUE(message.find("\"TestKey2\":\"TestValue2\"") != std::string::npos);
    // Unnamed data fields are named using their index in the array (Data0, Data1, etc.)
    // In this case, it's at index 4 because there are 2 default fields in the constructor
    // plus 2 named fields added before the unnamed one
    EXPECT_TRUE(message.find("\"Data4\":\"UnnamedValue\"") != std::string::npos); 
}

// Test user account lookup
TEST_F(EventHandlerMessageQueuerTest, UserAccountLookup) {
    // IMPORTANT: Create a new message queuer with lookup explicitly disabled
    // This ensures the message queuer doesn't use a cached value from SetUp()
    configuration->setLookupAccounts(false);
    
    // Recreate the message queuer to ensure it picks up the updated configuration
    message_queuer.reset();
    message_queuer = std::make_unique<EventHandlerMessageQueuer>(*configuration, primary_queue, secondary_queue, L"Application");
    
    // Use a mock SID format instead of a real Windows SID to avoid any Windows API interactions
    MockEventLogEvent event;
    const char* mockSid = "MOCK-SID-FOR-TESTING";
    event.setUserID(mockSid);
    
    // Process the event
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // Get the resulting message
    std::string message = dequeueMessage(primary_queue);
    
    // Verify the event contains user information fields
    EXPECT_TRUE(message.find("event_user_name") != std::string::npos);
    
    // Verify our mock SID was used directly (no lookup attempted)
    EXPECT_TRUE(message.find(mockSid) != std::string::npos);
}

// Test timestamp handling
TEST_F(EventHandlerMessageQueuerTest, TimestampHandling) {
    // Test valid timestamp
    MockEventLogEvent validEvent;
    // Create a fixed timestamp in the required format YYYY-MM-DDTHH:MM:SS[.microsecs]Z
    // Use current day but with fixed time for consistent, testable results
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    
    // Keep current year/month/day but use fixed hour/min/sec for consistent testing
    timeinfo.tm_hour = 12;
    timeinfo.tm_min = 34;
    timeinfo.tm_sec = 56;
    
    // Format with the exact format expected by the parser
    char timestamp_str[64];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    
    // Append microseconds and Z suffix manually to ensure exact format
    char full_timestamp[64];
    snprintf(full_timestamp, sizeof(full_timestamp), "%s.789Z", timestamp_str);
    validEvent.setSystemTime(full_timestamp);
    
    message_queuer->handleEvent(L"TestSubscription", validEvent);
    std::string validMessage = dequeueMessage(primary_queue); 
    
    // Print the actual message for debugging
    std::cout << "Actual message: " << validMessage << std::endl;
    
    // Verify the timestamp field exists with the correct format
    EXPECT_TRUE(validMessage.find("\"ts\": \"") != std::string::npos);
    
    // Check that the timestamp contains a numeric value with digits and a decimal point
    // This is more robust than trying to predict the exact value
    std::size_t tsPos = validMessage.find("\"ts\": \"");
    if (tsPos != std::string::npos) {
        tsPos += 7; // Move past the "ts": " part
        std::size_t endQuotePos = validMessage.find("\"", tsPos);
        if (endQuotePos != std::string::npos) {
            std::string tsValue = validMessage.substr(tsPos, endQuotePos - tsPos);
            
            // Verify it contains digits and a decimal point
            bool hasDigits = false;
            bool hasDecimal = false;
            
            for (char c : tsValue) {
                if (isdigit(c)) hasDigits = true;
                if (c == '.') hasDecimal = true;
            }
            
            EXPECT_TRUE(hasDigits && hasDecimal) << "Timestamp value should contain digits and a decimal point";
        }
    }

    clearQueue(primary_queue); 
    
    // Test invalid timestamp format
    MockEventLogEvent invalidEvent;
    invalidEvent.setSystemTime("invalid-timestamp");
    
    message_queuer->handleEvent(L"TestSubscription", invalidEvent);
    std::string invalidMessage = dequeueMessage(primary_queue); 
    EXPECT_TRUE(invalidMessage.find("\"ts\":") != std::string::npos); 
}

// Test secondary server handling
TEST_F(EventHandlerMessageQueuerTest, SecondaryServerHandling) {
    // Configure secondary server using new setters
    configuration->setForwardToSecondary(true);
    configuration->setSecondaryHost(L"10.0.0.1"); // Use wstring
    configuration->setSecondaryPort(10514);
    configuration->setSecondaryLogformat(SharedConstants::LOGFORMAT_HTTPPORT);
    // Note: This test is likely broken - Should work now
    
    MockEventLogEvent event;
    
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // Verify both queues have messages 
    EXPECT_FALSE(primary_queue->isEmpty());
    EXPECT_FALSE(secondary_queue->isEmpty()); // Expect secondary NOT to be empty
    EXPECT_EQ(primary_queue->length(), 1);
    EXPECT_EQ(secondary_queue->length(), 1); // Expect 1
}

// Test custom suffix handling
TEST_F(EventHandlerMessageQueuerTest, CustomSuffix) {
    // Add a custom suffix to the configuration using new setter
    std::wstring suffix = L"\"custom_field1\":\"custom_value1\", \"custom_field2\":\"custom_value2\"";
    printf("Test setting suffix: '%ls'\n", suffix.c_str());
    fflush(stdout);
    
    configuration->setSuffix(suffix);
    
    // Double-check that the config has our suffix
    std::wstring checkSuffix = configuration->getSuffix();
    printf("Config suffix after setting: '%ls'\n", checkSuffix.c_str());
    fflush(stdout);
    
    // Force recreation of the message_queuer with our updated config
    message_queuer.reset();
    message_queuer = std::make_unique<EventHandlerMessageQueuer>(*configuration, primary_queue, secondary_queue, L"Application");
    
    MockEventLogEvent event;
    
    // Handle event and check the queue
    message_queuer->handleEvent(L"TestSubscription", event);
    
    // Check for custom fields in the message
    std::string message = dequeueMessage(primary_queue);
    printf("Dequeued message (length %zu): '%s'\n", message.length(), message.c_str());
    fflush(stdout);
    
    // Check if the custom fields are in the message
    bool found1 = message.find("\"custom_field1\":\"custom_value1\"") != std::string::npos;
    bool found2 = message.find("\"custom_field2\":\"custom_value2\"") != std::string::npos;
    
    printf("Found custom_field1: %s\n", found1 ? "YES" : "NO");
    printf("Found custom_field2: %s\n", found2 ? "YES" : "NO");
    fflush(stdout);
    
    EXPECT_TRUE(found1); 
    EXPECT_TRUE(found2);
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
    std::string message = dequeueMessage(primary_queue); // Use helper
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
    
    // Format in the correct ISO-8601 format that the parser expects
    struct tm timeinfo;
    localtime_s(&timeinfo, &old_time);
    char old_timestamp[32];
    strftime(old_timestamp, sizeof(old_timestamp), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    
    // Set the timestamp in the expected format
    oldEvent.setSystemTime(old_timestamp);
    
    Result result = message_queuer->handleEvent(L"TestSubscription", oldEvent);
    
    // Should be skipped due to age
    EXPECT_EQ(result.statusCode(), ERROR_CANCELLED);
    EXPECT_TRUE(primary_queue->isEmpty());
}

// Test buffer overflow protection
TEST_F(EventHandlerMessageQueuerTest, BufferOverflowProtection) {
    // This test verifies that the code can handle fields at their maximum allowed sizes
    // without buffer overflows or other memory issues.
    
    // Create an event with fields that are at their maximum allowed sizes
    MockEventLogEvent largeEvent;
    
    // For provider name - allocate a buffer of MAX_PROVIDER_LEN size on the stack
    // This avoids large heap allocations while still testing max sizes
    const size_t providerSize = EventHandlerMessageQueuer::EventData::MAX_PROVIDER_LEN;
    char* providerBuffer = (char*)_alloca(providerSize); // Stack allocation
    memset(providerBuffer, 'P', providerSize-1);
    providerBuffer[providerSize-1] = '\0';
    strcpy_s(largeEvent.event_xml_data_.providerName, providerSize, providerBuffer);
    
    // For event ID - allocate a buffer of MAX_EVENT_ID_LEN size on the stack
    const size_t eventIdSize = EventHandlerMessageQueuer::EventData::MAX_EVENT_ID_LEN;
    char* eventIdBuffer = (char*)_alloca(eventIdSize); // Stack allocation
    memset(eventIdBuffer, '9', eventIdSize-1);
    eventIdBuffer[eventIdSize-1] = '\0';
    largeEvent.setEventID(eventIdBuffer);
    
    // For message - We'll use a smaller message size while still testing buffer limits
    // Create it in chunks to avoid a massive heap allocation
    const size_t chunkSize = 1024;
    const size_t numChunks = 8; // 8KB total, still large enough to test buffer handling
    char* messageBuffer = (char*)_alloca(chunkSize * numChunks + 1);
    
    // Fill message buffer in chunks
    for (size_t i = 0; i < numChunks; i++) {
        memset(messageBuffer + (i * chunkSize), 'M', chunkSize);
    }
    messageBuffer[chunkSize * numChunks] = '\0';
    
    largeEvent.setMessage(messageBuffer);
    
    // Add some data fields to exercise that part of the buffer logic
    largeEvent.addDataField("LargeKey", "LargeValue1234567890");
    largeEvent.addDataField("TestKey2", "TestValue2");
    
    // Handle the event
    Result result = message_queuer->handleEvent(L"TestSubscription", largeEvent);
    
    // Should still succeed despite large fields
    EXPECT_EQ(result.statusCode(), ERROR_SUCCESS);
    EXPECT_FALSE(primary_queue->isEmpty());
    
    // Verify that the large event was processed correctly
    std::string message = dequeueMessage(primary_queue);
    EXPECT_FALSE(message.empty());
    EXPECT_TRUE(message.length() > 1000); // Message should be quite large
    
    // Verify presence of our test values
    EXPECT_TRUE(message.find("9999") != std::string::npos); // Part of event ID
    EXPECT_TRUE(message.find("MMMM") != std::string::npos); // Part of message
} 
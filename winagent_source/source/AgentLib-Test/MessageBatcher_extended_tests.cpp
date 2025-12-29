#include "pch.h"
#include <gtest/gtest.h>
#include "../AgentLib/HttpMessageBatcher.h"  // Your HTTPMessageBatcher subclass
#include "../AgentLib/MessageQueue.h"
#include "MessageQueueTestExtensions.h"
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <ctime>
#include <iostream>

using json = nlohmann::json;

// Helper function to generate a variable‑length message (between 50 and 2048 characters)
std::string generateVariableSizeMessage() {
    // Generate a random size between 50 and 2048.
    size_t msgSize = 50 + (std::rand() % (2048 - 50 + 1));
    // Return a string consisting of 'A' repeated msgSize times.
    return std::string(msgSize, 'A');
}

TEST(JsonBatchingTest, ValidJsonBatchTerminationWithVaryingEventSizes) {
    // Seed the random number generator.
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    // Configure the test duration (in seconds). For quick automated runs use a lower value;
    // for in-depth manual testing, set this to a higher value (e.g., 600 seconds).
    int testDurationSeconds = 5; 
    auto startTime = std::chrono::steady_clock::now();
    
    // Create a MessageQueue and instantiate the HTTPMessageBatcher.
    auto msgQueue = std::make_shared<Syslog_agent::MessageQueue>(100, 10);
    Syslog_agent::HTTPMessageBatcher batcher(50, 1000);
    
    // Loop for the test duration, generating and enqueuing fake events.
    while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(testDurationSeconds)) {
        // Generate a variable‑length message.
        std::string variableMessage = generateVariableSizeMessage();
        
        // Construct a fake JSON event log message.
        // This mimics the output of your generateJson() method.
        std::string fakeEvent = 
            std::string("{")
            + "\"host\": \"TestHost\", "
            + "\"program\": \"TestProgram\", "
            + "\"extra_fields\": {"
                + "\"_source_type\": \"WindowsAgent\", "
                + "\"_source_tag\": \"windows_agent\", "
                + "\"_log_type\": \"eventlog\", "
                + "\"event_id\": \"4624\", "
                + "\"event_log\": \"Security\", "
                + "\"severity\": \"4\", "
                + "\"facility\": \"Security\", "
                + "\"ts\": \"2025-02-20.123456\""
            + "}, "
            + "\"message\": \"" + variableMessage + "\""
            + "}";
        
        bool enqResult = msgQueue->enqueue(fakeEvent.c_str(), static_cast<uint32_t>(fakeEvent.size()));
        ASSERT_TRUE(enqResult) << "Failed to enqueue fake event";
        
        // Sleep briefly to simulate the interval between events.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Now invoke the batcher to batch up the events.
    const size_t bufferSize = 65536;
    char batchBuffer[bufferSize] = {0};
    auto batchResult = batcher.BatchEvents(msgQueue, batchBuffer, bufferSize);
    ASSERT_EQ(batchResult.status, Syslog_agent::MessageBatcher::BatchResult::Success);
    ASSERT_GT(batchResult.messages_batched, 0) << "No messages were batched";
    
    // Validate that batchBuffer contains a valid JSON object with an "events" array.
    try {
        json j = json::parse(batchBuffer);
        // The root should be an object.
        ASSERT_TRUE(j.is_object()) << "Batch JSON is not an object";
        ASSERT_TRUE(j.contains("events")) << "Batch JSON does not contain an 'events' key";
        auto events = j["events"];
        ASSERT_TRUE(events.is_array()) << "'events' is not an array";
        
        // Optionally, verify each event has the expected keys and that the "message"
        // length falls within the intended range.
        for (const auto& event : events) {
            ASSERT_TRUE(event.is_object());
            ASSERT_TRUE(event.contains("host"));
            ASSERT_TRUE(event.contains("program"));
            ASSERT_TRUE(event.contains("message"));
            std::string msg = event["message"];
            ASSERT_GE(msg.size(), 50);
            ASSERT_LE(msg.size(), 2048);
        }
        
        // For debugging, print the final JSON batch size.
        size_t finalSize = strlen(batchBuffer);
        std::cout << "Final JSON batch size: " << finalSize << " bytes." << std::endl;
    } catch (json::parse_error& ex) {
        FAIL() << "JSON parse error: " << ex.what() << "\nBatch buffer: " << batchBuffer;
    }
}

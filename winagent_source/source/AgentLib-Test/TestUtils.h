#pragma once

#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <functional>
#include <iostream>

namespace TestUtils {

// Helper function to sleep for a specific duration with seconds
inline void sleep_seconds(unsigned int seconds) {
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

// Helper function to sleep for a specific duration with milliseconds
inline void sleep(unsigned int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Helper function to compare strings, ignoring whitespace
inline bool compareIgnoreWhitespace(const std::string& str1, const std::string& str2) {
    std::string s1, s2;
    
    // Remove whitespace from first string
    for (char c : str1) {
        if (!std::isspace(c)) {
            s1 += c;
        }
    }
    
    // Remove whitespace from second string
    for (char c : str2) {
        if (!std::isspace(c)) {
            s2 += c;
        }
    }
    
    return s1 == s2;
}

// Helper function to check if a string contains valid JSON
inline bool isValidJson(const std::string& json) {
    // This is a simple check - just looks for matching braces
    // A proper implementation would use a JSON parser
    int braceCount = 0;
    for (char c : json) {
        if (c == '{') braceCount++;
        if (c == '}') braceCount--;
        if (braceCount < 0) return false; // Unmatched closing brace
    }
    return braceCount == 0; // All braces should be matched
}

// Execute function with timeout
template<typename Func>
inline bool executeWithTimeout(Func func, unsigned int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    bool completed = false;
    
    // Execute the function
    func();
    completed = true;
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    
    return completed && (elapsed < timeoutMs);
}

// Common test data
inline std::string getTestJsonMessage() {
    return "{\"test\":\"value\"}";
}

// Common batch test messages
inline std::vector<std::string> getTestBatchMessages(int count) {
    std::vector<std::string> messages;
    for (int i = 0; i < count; i++) {
        messages.push_back("{\"index\":" + std::to_string(i) + ",\"value\":\"test" + std::to_string(i) + "\"}");
    }
    return messages;
}

// Common setup for test fixtures
template<typename BatcherType, typename QueueType>
inline void setupTestBatcher(
    std::unique_ptr<BatcherType>& batcher,
    std::shared_ptr<QueueType>& queue,
    uint32_t maxBatchCount,
    uint32_t maxBatchAgeSec
) {
    // Create a fresh batcher
    batcher = std::make_unique<BatcherType>(maxBatchCount, maxBatchAgeSec);
    // Initialize message queue with capacity
    queue = std::make_shared<QueueType>(maxBatchCount * 2, maxBatchCount * 3);
}

// Common teardown for test fixtures
template<typename BatcherType, typename QueueType>
inline void teardownTestBatcher(
    std::unique_ptr<BatcherType>& batcher,
    std::shared_ptr<QueueType>& queue
) {
    batcher.reset();
    queue.reset();
}

} // namespace TestUtils 
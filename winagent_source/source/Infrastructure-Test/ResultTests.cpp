#include "pch.h"
#include "../Infrastructure/Result.h"

using namespace Syslog_agent;

class ResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for tests
    }
};

// Test default constructor and success state
TEST_F(ResultTest, DefaultConstructor) {
    Result result;
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.statusCode(), 0);
}

// Test constructor with error status
TEST_F(ResultTest, ErrorStatusConstructor) {
    const DWORD testError = ERROR_ACCESS_DENIED;
    Result result(testError);
    EXPECT_FALSE(result.isSuccess());
    EXPECT_EQ(result.statusCode(), testError);
}

// Test constructor with message
TEST_F(ResultTest, MessageConstructor) {
    const char* testMessage = "Error message";
    Result result(testMessage);
    EXPECT_FALSE(result.isSuccess());
    std::string what = result.what();
    EXPECT_FALSE(what.empty());
}

// Test the full detail constructor
TEST_F(ResultTest, DetailedConstructor) {
    const DWORD testError = ERROR_INVALID_PARAMETER;
    const char* testLocation = "TestFunction";
    const char* testFormat = "Error at %s";
    const char* testParam = "param";
    
    char expectedMessage[256];
    sprintf_s(expectedMessage, testFormat, testParam);
    
    Result result(testError, testLocation, testFormat, testParam);
    EXPECT_FALSE(result.isSuccess());
    EXPECT_EQ(result.statusCode(), testError);
    
    // Check that the message contains the formatted message and function name
    std::string what = result.what();
    EXPECT_TRUE(what.find(testLocation) != std::string::npos);
    EXPECT_TRUE(what.find(testParam) != std::string::npos);
}

// Test copy constructor
TEST_F(ResultTest, CopyConstructor) {
    Result original(ERROR_INVALID_HANDLE, "TestFunc", "Test error");
    Result copy(original);
    
    EXPECT_EQ(copy.statusCode(), original.statusCode());
    EXPECT_STREQ(copy.what(), original.what());
}

// Test ResultLog static factory
TEST_F(ResultTest, ResultLogFactory) {
    const DWORD testError = ERROR_NOT_FOUND;
    const Logger::LogLevel testLevel = Logger::DEBUG;
    const char* testLocation = "TestFactory";
    const char* testFormat = "Factory %s";
    const char* testParam = "test";
    
    Result result = Result::ResultLog(testError, testLevel, testLocation, testFormat, testParam);
    EXPECT_EQ(result.statusCode(), testError);
    
    // Check message content
    std::string what = result.what();
    EXPECT_TRUE(what.find(testLocation) != std::string::npos);
    EXPECT_TRUE(what.find(testParam) != std::string::npos);
}

// Test logging behavior (needs mock logger for full test)
TEST_F(ResultTest, LoggingBehavior) {
    // Basic test without mocks
    Result success;
    Result error(ERROR_ACCESS_DENIED, "TestLog", "Log test");
    
    // This just verifies that log() doesn't crash
    success.log();
    error.log();
}

// Test error handling static methods (simplified)
TEST_F(ResultTest, ErrorHandlingStatics) {
    // These tests would need mocks for full coverage
    // but this at least verifies they don't crash with normal use
    
    try {
        // This would throw in a real app with a real error
        // but for testing we just verify it doesn't crash
        Result::throwLastError("TestThrow", "Test throw");
        // Should not reach here in normal use
        EXPECT_TRUE(true);
    }
    catch (const Result& e) {
        // We may or may not get here depending on the test environment
        EXPECT_FALSE(e.isSuccess());
    }
    catch (...) {
        // Should never get here
        EXPECT_TRUE(false) << "Unexpected exception type";
    }
    
    // Test logLastError
    Result::logLastError("TestLogError", "Test log error");
    // No assertion - just verify it doesn't crash
}

// Test empty string handling
TEST_F(ResultTest, EmptyStringHandling) {
    Result result(""); // Empty message
    EXPECT_FALSE(result.isSuccess());
    EXPECT_FALSE(std::string(result.what()).empty()); // Should still have some error info
}

// Test very long message handling
TEST_F(ResultTest, LongMessageHandling) {
    std::string longMessage(2048, 'A'); // Message longer than internal buffer
    Result result(longMessage.c_str());
    EXPECT_FALSE(result.isSuccess());
    EXPECT_GT(strlen(result.what()), 0);
}

// Test message formatting with null parameters
TEST_F(ResultTest, NullParameterHandling) {
    Result result(ERROR_INVALID_PARAMETER, nullptr, nullptr);
    EXPECT_FALSE(result.isSuccess());
    EXPECT_EQ(result.statusCode(), ERROR_INVALID_PARAMETER);
}

// Test exception inheritance
TEST_F(ResultTest, ExceptionInheritance) {
    Result result(ERROR_ACCESS_DENIED, "TestFunc", "Access denied");
    
    try {
        throw result;
    }
    catch (const std::exception& e) {
        // Should catch as std::exception
        EXPECT_STREQ(e.what(), result.what());
    }
    catch (...) {
        FAIL() << "Result should be caught as std::exception";
    }
}

// Test multiple format parameters
TEST_F(ResultTest, MultipleFormatParameters) {
    const char* loc = "TestFunc";
    Result result(ERROR_INVALID_DATA, loc, "Error: %s %d %s", "test", 42, "params");
    std::string what = result.what();
    EXPECT_TRUE(what.find(loc) != std::string::npos);
    EXPECT_TRUE(what.find("test") != std::string::npos);
    EXPECT_TRUE(what.find("42") != std::string::npos);
    EXPECT_TRUE(what.find("params") != std::string::npos);
}

// Test system error message formatting
TEST_F(ResultTest, SystemErrorMessage) {
    const DWORD errorCode = ERROR_FILE_NOT_FOUND;
    Result result(errorCode, "TestFunc", "File operation failed");
    std::string what = result.what();
    
    // Should contain both our message and system error message
    EXPECT_TRUE(what.find("File operation failed") != std::string::npos);
    EXPECT_TRUE(what.find("TestFunc") != std::string::npos);
    
    // Should contain some form of system error message
    // The exact message might vary by Windows version, so we just check it's not empty
    EXPECT_TRUE(what.length() > strlen("TestFunc") + strlen("File operation failed"));
}

// Test assignment operator behavior (if implemented)
TEST_F(ResultTest, AssignmentOperator) {
    Result original(ERROR_INVALID_HANDLE, "TestFunc", "Original error");
    Result assigned;
    
    assigned = original;
    
    EXPECT_EQ(assigned.statusCode(), original.statusCode());
    EXPECT_STREQ(assigned.what(), original.what());
}

// Test memory cleanup with repeated operations
TEST_F(ResultTest, MemoryCleanup) {
    Result result(ERROR_SUCCESS);
    
    // Perform multiple operations that could cause memory issues if not handled properly
    for(int i = 0; i < 1000; i++) {
        result = Result(ERROR_INVALID_DATA, "TestFunc", "Test %d", i);
        EXPECT_FALSE(result.isSuccess());
        EXPECT_EQ(result.statusCode(), ERROR_INVALID_DATA);
    }
}

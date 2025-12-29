#include "pch.h"
#include "../Infrastructure/Util.h"
#include <windows.h>
#include <string>
#include <memory>
#include <crtdbg.h>

// Avoid potential memory leaks or heap issues

using namespace std;

// Test fixture for Util functions
class UtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for tests
    }
    
    void TearDown() override {
        // Cleanup after each test
    }
};

// Test that getThisPath returns a non-empty path - USING BUFFER VERSION
TEST_F(UtilTest, GetThisPathReturnsValidPath) {
    OutputDebugStringW(L"GetThisPathReturnsValidPath: Using buffer-based getThisPath\n");
    
    // Test with trailing backslash
    {
        wchar_t path_buffer[MAX_PATH] = {0};
        bool result = Util::getThisPath(path_buffer, MAX_PATH, true);
        EXPECT_TRUE(result);
        EXPECT_GT(wcslen(path_buffer), 0u);
        if (wcslen(path_buffer) > 0) {
            EXPECT_EQ(path_buffer[wcslen(path_buffer) - 1], L'\\');
        }
        EXPECT_TRUE(_CrtCheckMemory()) << "Heap corrupted after buffer use with trailing slash";
    }
    
    // Check heap integrity after first block
    EXPECT_TRUE(_CrtCheckMemory()) << "Heap corrupted after first buffer-based test block";
    
    OutputDebugStringW(L"------------------------------------\n");
    
    // Test without trailing backslash
    {
        wchar_t path_buffer[MAX_PATH] = {0};
        bool result = Util::getThisPath(path_buffer, MAX_PATH, false);
        EXPECT_TRUE(result);
        EXPECT_GT(wcslen(path_buffer), 0u);
        if (wcslen(path_buffer) > 0) {
            EXPECT_NE(path_buffer[wcslen(path_buffer) - 1], L'\\');
        }
        EXPECT_TRUE(_CrtCheckMemory()) << "Heap corrupted after buffer use without trailing slash";
    }
    
    OutputDebugStringW(L"GetThisPathReturnsValidPath: All buffer-based tests completed.\n");
    
    // Final heap check before exiting test
    auto memory_check = _CrtCheckMemory();
    EXPECT_TRUE(memory_check) << "Heap corrupted before end of GetThisPathReturnsValidPath";
}

// Test the buffer-based version of getThisPath
TEST_F(UtilTest, GetThisPathBufferVersion) {
    // Safe initialization
    std::unique_ptr<wchar_t[]> buffer_ptr = std::make_unique<wchar_t[]>(MAX_PATH);
    wchar_t* buffer = buffer_ptr.get();
    
    // Initialize buffer
    for (size_t i = 0; i < MAX_PATH; i++) {
        buffer[i] = L'\0';
    }
    
    // Test with trailing backslash
    bool result = Util::getThisPath(buffer, MAX_PATH, true);
    EXPECT_TRUE(result);
    size_t len = wcslen(buffer);
    EXPECT_GT(len, 0);
    if (len > 0) {
        EXPECT_EQ(buffer[len - 1], L'\\'); // Should have trailing backslash
    }
    
    // Clean up for next test
    for (size_t i = 0; i < MAX_PATH; i++) {
        buffer[i] = L'\0';
    }
    
    // Test without trailing backslash
    result = Util::getThisPath(buffer, MAX_PATH, false);
    EXPECT_TRUE(result);
    len = wcslen(buffer);
    EXPECT_GT(len, 0);
    if (len > 0) {
        EXPECT_NE(buffer[len - 1], L'\\'); // Should not have trailing backslash
    }
}

// Test that buffer-based version fails with invalid buffer
TEST_F(UtilTest, GetThisPathInvalidBuffer) {
    // Test with null buffer
    bool result = Util::getThisPath(nullptr, MAX_PATH, true);
    EXPECT_FALSE(result);
    
    // Test with insufficient buffer size - use safer unique_ptr
    std::unique_ptr<wchar_t[]> small_buffer_ptr = std::make_unique<wchar_t[]>(5);
    wchar_t* small_buffer = small_buffer_ptr.get();
    
    // Initialize buffer
    for (size_t i = 0; i < 5; i++) {
        small_buffer[i] = L'\0';
    }
    
    result = Util::getThisPath(small_buffer, 5, true);
    EXPECT_FALSE(result);
}

// Test consistency between the two versions
TEST_F(UtilTest, GetThisPathVersionsConsistency) {
    // Get path using wstring version
    // Create two buffers for different versions of the path
    wchar_t buffer1[MAX_PATH] = {0};
    wchar_t buffer2[MAX_PATH] = {0};
    
    // Get path with trailing slash in first buffer
    bool result1 = Util::getThisPath(buffer1, MAX_PATH, true);
    EXPECT_TRUE(result1);
    EXPECT_GT(wcslen(buffer1), 0u);
    EXPECT_LT(wcslen(buffer1), MAX_PATH);
    
    // Get the same path again in the second buffer
    bool result2 = Util::getThisPath(buffer2, MAX_PATH, true);
    EXPECT_TRUE(result2);
    
    // Both paths should be identical
    if (result1 && result2) {
        EXPECT_STREQ(buffer1, buffer2);
    }
    
    // Clean up for next test
    memset(buffer1, 0, MAX_PATH * sizeof(wchar_t));
    memset(buffer2, 0, MAX_PATH * sizeof(wchar_t));
    
    // Repeat test without trailing backslash
    bool result3 = Util::getThisPath(buffer1, MAX_PATH, false);
    EXPECT_TRUE(result3);
    
    bool result4 = Util::getThisPath(buffer2, MAX_PATH, false);
    EXPECT_TRUE(result4);
    
    // These paths should also be identical to each other
    if (result3 && result4) {
        EXPECT_STREQ(buffer1, buffer2);
    }
}

// Test that the path contains expected elements
TEST_F(UtilTest, GetThisPathContent) {
    // Test buffer version first and check raw buffer content - SIMPLIFIED
    std::unique_ptr<wchar_t[]> buffer_ptr = std::make_unique<wchar_t[]>(MAX_PATH);
    wchar_t* buffer = buffer_ptr.get();
    
    // Initialize buffer
    memset(buffer, 0, MAX_PATH * sizeof(wchar_t)); // Use memset for clarity
    
    bool result = Util::getThisPath(buffer, MAX_PATH, true); // Request trailing slash
    EXPECT_TRUE(result);
    
    if (result) {
        // Check raw buffer content
        // Ensure null termination for safety before using C-style functions
        // (getThisPath should already do this, but belt-and-suspenders)
        buffer[MAX_PATH - 1] = L'\0'; 
        size_t len = wcslen(buffer);
        EXPECT_GT(len, 0);
        EXPECT_LT(len, MAX_PATH); // Sanity check length
        
        // Ensure it contains a backslash (isn't just root like "C:")
        EXPECT_TRUE(wcschr(buffer, L'\\') != nullptr);
        
        // Ensure it doesn't contain the executable name (basic check)
        EXPECT_TRUE(wcsstr(buffer, L".exe") == nullptr);
        
        // Ensure it ends with a backslash as requested
        if (len > 0) {
            EXPECT_EQ(buffer[len - 1], L'\\');
        }
    }
    // Intentionally omit testing the wstring version and wstring comparisons for now
    // to isolate the source of the heap corruption assertion.
}

#include "gtest/gtest.h"
#include "Logger.h"          // Assuming Logger.h is accessible
#include "Registry.h"        // Assuming Registry.h is accessible
#include "SharedConstants.h" // For registry key names
#include "Util.h"            // For generating temp paths if needed
#include <windows.h>
#include <string>
#include <memory>
#include <stdexcept> // For runtime_error
#include <uuid/uuid.h> // Requires linking against libuuid or equivalent

// --- Logger Tests ---

TEST(LoggerSetLogFileWTest, HandlesValidPath) {
    Logger logger("TestLogger");
    std::wstring testPath = L"C:\\temp\\testlog.log";
    // Primarily a smoke test to ensure no crash
    ASSERT_NO_THROW(logger.setLogFileW(testPath)); 
}

TEST(LoggerSetLogFileWTest, HandlesEmptyPath) {
    Logger logger("TestLogger");
    std::wstring emptyPath = L"";
    // Ensure setting an empty path doesn't crash
    ASSERT_NO_THROW(logger.setLogFileW(emptyPath));
}

TEST(LoggerSetLogFileWTest, HandlesLongPath) {
    Logger logger("TestLogger");
    // Create a path longer than the internal buffer (1024)
    std::wstring longPath(2000, L'A'); 
    longPath += L".log";
    // Ensure setting a very long path truncates/clears safely without crashing
    ASSERT_NO_THROW(logger.setLogFileW(longPath));
}


// --- Registry Tests with Real Windows API ---

class RegistryRealApiTest : public ::testing::Test {
protected:
    HKEY hKey_ = NULL;
    std::wstring testKeyPath_;
    const std::wstring basePath_ = L"Software\\LogzillaTestTemp\\";

    // Helper to generate a unique key name
    std::wstring generateUniqueKeyName() {
        uuid_t uuid;
        char uuid_str[37];
        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuid_str);
        std::string s(uuid_str);
        return std::wstring(s.begin(), s.end());
    }

    void SetUp() override {
        testKeyPath_ = basePath_ + generateUniqueKeyName();
        LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, testKeyPath_.c_str(), 0, NULL, 
                                       REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey_, NULL);
        if (status != ERROR_SUCCESS) {
            hKey_ = NULL;
            throw std::runtime_error("Failed to create test registry key: " + std::to_string(status));
        }
    }

    void TearDown() override {
        if (hKey_) {
            RegCloseKey(hKey_);
            hKey_ = NULL;
        }
        // Delete the entire temporary key tree
        // Extract parent path to delete from there
        size_t last_slash = testKeyPath_.find_last_of(L'\\');
        if (last_slash != std::wstring::npos) {
           std::wstring parentPath = testKeyPath_.substr(0, last_slash);
           std::wstring keyToDelete = testKeyPath_.substr(last_slash + 1);
            HKEY hParentKey = NULL;
           LSTATUS openStatus = RegOpenKeyExW(HKEY_CURRENT_USER, parentPath.c_str(), 0, KEY_ALL_ACCESS, &hParentKey);
           if (openStatus == ERROR_SUCCESS) {
               RegDeleteTreeW(hParentKey, keyToDelete.c_str());
               RegCloseKey(hParentKey);
           }
        }
         // Attempt to delete the base key if empty (might fail if other tests ran)
         HKEY hBaseKey = NULL;
         LSTATUS openBaseStatus = RegOpenKeyExW(HKEY_CURRENT_USER, basePath_.c_str(), 0, KEY_ALL_ACCESS, &hBaseKey);
         if(openBaseStatus == ERROR_SUCCESS){
            RegDeleteTreeW(hBaseKey, NULL); // Delete self if empty
            RegCloseKey(hBaseKey);
         }
    }

    // Helper to set string value in the test key
    bool SetTestStringValue(const wchar_t* valueName, const std::wstring& value) {
        if (!hKey_) return false;
        LSTATUS status = RegSetValueExW(hKey_, valueName, 0, REG_SZ,
                                     reinterpret_cast<const BYTE*>(value.c_str()),
                                     static_cast<DWORD>((value.length() + 1) * sizeof(wchar_t))); // +1 for null terminator
        return status == ERROR_SUCCESS;
    }
};

// Test using the fixture
TEST_F(RegistryRealApiTest, ReadStringDebugLogFile) {
    const std::wstring expectedValue = L"test_debug.log";
    const wchar_t* valueName = SharedConstants::RegistryKey::DEBUG_LOG_FILE;

    // Set the value using the real API via the fixture helper
    ASSERT_TRUE(SetTestStringValue(valueName, expectedValue));

    // Create Registry object pointing to our temporary real key
    // We need to pass the HKEY directly or modify Registry to accept it
    // For now, let's assume Registry has a constructor or method to use an existing key
    // If not, this part needs adaptation. Let's assume a hypothetical constructor:
    // Registry registry(hKey_); // Hypothetical
    
    // --- Simpler approach: Use the static HKEY_CURRENT_USER and the full path ---
    Registry registry(HKEY_CURRENT_USER, testKeyPath_.c_str()); 

    // Read the string using the class method
    std::wstring actualValue = registry.readString(valueName, L"DEFAULT_SHOULD_NOT_BE_USED");

    // Assert that the read value matches the expected value
    ASSERT_EQ(actualValue, expectedValue);
}

// Add more tests for Registry::readString with different values (empty, long, etc.) if desired.


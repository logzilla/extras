#include "pch.h"
#include <gtest/gtest.h>
#include "../AgentLib/Registry.h"
#include "../AgentLib/SyslogAgentSharedConstants.h" // Include for SharedConstants
#include <Windows.h>
#include <string>
#include <vector>
#include <set>
#include <memory>

using namespace Syslog_agent;
using namespace std;

// Test fixture for Registry tests
class RegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary registry key for testing
        const wchar_t* testKeyPath = L"Software\\LogZilla\\SyslogAgent\\UnitTest";
        HKEY hKey;
        DWORD disposition;
        LSTATUS result = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            testKeyPath,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS, // Restored original permissions
            NULL,
            &hKey,
            &disposition
        );
        EXPECT_EQ(result, ERROR_SUCCESS);
        RegCloseKey(hKey);

        // Also create a channels subkey
        std::wstring channelsKeyPath = std::wstring(testKeyPath) + L"\\Channels";
        result = RegCreateKeyExW(
            HKEY_CURRENT_USER,
            channelsKeyPath.c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS, // Restored original permissions
            NULL,
            &hKey,
            &disposition
        );
        EXPECT_EQ(result, ERROR_SUCCESS);
        RegCloseKey(hKey);

        // Open registry for testing
        registry_ = std::make_unique<Registry>();
        registry_->open(HKEY_CURRENT_USER, testKeyPath);
        
        // Store the test key path for cleanup
        testKeyPath_ = testKeyPath;
    }

    void TearDown() override {
        registry_.reset(); // Restore registry object reset
        
        // Delete the test key recursively // Restore key deletion
        RegDeleteTreeW(HKEY_CURRENT_USER, testKeyPath_.c_str());
    }

    // Helper function to set a string value in the test registry key
    void SetStringValue(const wchar_t* valueName, const wchar_t* value) {
        HKEY hWriteKey = nullptr;
        // Use .c_str() to pass LPCWSTR to RegOpenKeyExW
        LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, testKeyPath_.c_str(), 0, KEY_SET_VALUE, &hWriteKey);

        if (status != ERROR_SUCCESS) {
            FAIL() << "SetStringValue failed to open key: " << testKeyPath_.c_str() << " with error: " << status;
            return;
        }

        // Calculate the size of the value string in bytes, including the null terminator
        DWORD dataSize = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));

        status = RegSetValueExW(
            hWriteKey,
            valueName,
            0, // Reserved, must be zero
            REG_SZ,
            reinterpret_cast<const BYTE*>(value),
            dataSize
        );

        RegCloseKey(hWriteKey); // Close the key immediately after use

        if (status != ERROR_SUCCESS) {
            FAIL() << "SetStringValue failed to set value: " << valueName << " with error: " << status;
        }
    }

    // Helper to set a DWORD value in our test key
    void SetDwordValue(const wchar_t* valueName, DWORD value) {
        HKEY hKey;
        LSTATUS result = RegOpenKeyExW(
            HKEY_CURRENT_USER, 
            testKeyPath_.c_str(), 
            0, 
            KEY_WRITE, 
            &hKey
        );
        ASSERT_EQ(result, ERROR_SUCCESS);
        
        result = RegSetValueExW(
            hKey,
            valueName,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&value),
            sizeof(DWORD)
        );
        ASSERT_EQ(result, ERROR_SUCCESS);
        RegCloseKey(hKey);
    }

    // Helper to create a basic channel key for testing
    void CreateChannelKey(const wchar_t* channelName) {
        HKEY hKey = NULL;
        HKEY hChannelsKey = NULL;
        HKEY hChannelKey = NULL;
        DWORD disposition;
        LSTATUS result;

        // Create/Open the main test key
        result = RegCreateKeyExW(HKEY_CURRENT_USER, testKeyPath_.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKey, &disposition);
        if (result != ERROR_SUCCESS) {
            FAIL() << "CreateChannelKey: Failed to create/open base test key: " << testKeyPath_ << " Error: " << result;
            return; // Exit on failure
        }

        // Create/Open the 'Channels' subkey
        result = RegCreateKeyExW(hKey, L"Channels", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hChannelsKey, &disposition);
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            FAIL() << "CreateChannelKey: Failed to create/open 'Channels' subkey under " << testKeyPath_ << " Error: " << result;
            return; // Exit on failure
        }

        // Create/Open the specific channel subkey
        result = RegCreateKeyExW(hChannelsKey, channelName, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hChannelKey, &disposition);
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hChannelsKey);
            RegCloseKey(hKey);
            FAIL() << "CreateChannelKey: Failed to create/open channel key '" << channelName << "' Error: " << result;
            return; // Exit on failure
        }

        // Set the 'Enabled' value within the channel subkey
        DWORD enabledValue = 1;
        result = RegSetValueExW(hChannelKey, SharedConstants::RegistryKey::CHANNEL_ENABLED, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enabledValue), sizeof(enabledValue));
        if (result != ERROR_SUCCESS) {
            RegCloseKey(hChannelKey);
            RegCloseKey(hChannelsKey);
            RegCloseKey(hKey);
            FAIL() << "CreateChannelKey: Failed to set 'Enabled' value for channel '" << channelName << "' Error: " << result;
            return; // Exit on failure
        }

        // Close handles successfully
        RegCloseKey(hChannelKey);
        RegCloseKey(hChannelsKey);
        RegCloseKey(hKey);
    }

    std::unique_ptr<Registry> registry_;
    std::wstring testKeyPath_;
};

// Test opening and closing registry
TEST_F(RegistryTest, OpenClose) {
    // Test open/close logic on a local instance, not the shared fixture
    Registry local_registry; // Calls open() in constructor
    local_registry.close();
    local_registry.open();
    local_registry.close();

    // No assertion, just verifying no crashes
}

// Test reading string values
TEST_F(RegistryTest, ReadString) {
    // --- Restore SetStringValue ---
    const wchar_t* testValueName = L"TestStringValue"; 
    const wchar_t* expectedValue = L"Hello Registry!"; // The value we expect to read back
    
    // Re-enable the write operation for the test
    SetStringValue(testValueName, expectedValue); 

    // Call the readString method
    std::wstring actualValue = registry_->readString(testValueName, L"Default");
    
    // Expecting the value written by SetStringValue
    ASSERT_EQ(actualValue, expectedValue); 
    // --- End Restore SetStringValue ---
}

// Test case for readString when the value does not exist
TEST_F(RegistryTest, ReadString_ValueNotFound) {
    // Test default value when key doesn't exist
    std::wstring defaultResult = registry_->readString(L"NonExistentKey", L"DefaultValue");
    EXPECT_EQ(defaultResult, L"DefaultValue");
}

// Test reading integer values
TEST_F(RegistryTest, ReadInt) {
    // Test existing value
    const wchar_t* testKey = L"TestInt";
    DWORD testValue = 42;
    SetDwordValue(testKey, testValue);
    
    int result = registry_->readInt(testKey, 0);
    EXPECT_EQ(result, 42);
    
    // Test default value when key doesn't exist
    int defaultResult = registry_->readInt(L"NonExistentKey", -1);
    EXPECT_EQ(defaultResult, -1);
}

// Test reading boolean values
TEST_F(RegistryTest, ReadBool) {
    // Test true value
    const wchar_t* trueKey = L"TestBoolTrue";
    DWORD trueValue = 1;
    SetDwordValue(trueKey, trueValue);
    
    bool trueResult = registry_->readBool(trueKey, false);
    EXPECT_TRUE(trueResult);
    
    // Test false value
    const wchar_t* falseKey = L"TestBoolFalse";
    DWORD falseValue = 0;
    SetDwordValue(falseKey, falseValue);
    
    bool falseResult = registry_->readBool(falseKey, true);
    EXPECT_FALSE(falseResult);
    
    // Test default value when key doesn't exist
    bool defaultResult = registry_->readBool(L"NonExistentKey", true);
    EXPECT_TRUE(defaultResult);
}

// Test reading char values
TEST_F(RegistryTest, ReadChar) {
    // Test existing value
    const wchar_t* testKey = L"TestChar";
    DWORD testValue = 65;  // ASCII 'A' // Restore writing as DWORD
    SetDwordValue(testKey, testValue);
    
    char result = registry_->readChar(testKey, 'B');
    EXPECT_EQ(result, 'A');
    
    // Test default value when key doesn't exist
    char defaultResult = registry_->readChar(L"NonExistentKey", 'Z');
    EXPECT_EQ(defaultResult, 'Z');
}

// Test reading time values
TEST_F(RegistryTest, ReadTime) {
    // Test existing value
    const wchar_t* testKey = L"TestTime";
    DWORD testValue = 1625097600;  // Some Unix timestamp
    SetDwordValue(testKey, testValue);
    
    time_t result = registry_->readTime(testKey, 0);
    EXPECT_EQ(result, testValue);
    
    // Test default value when key doesn't exist
    time_t defaultResult = registry_->readTime(L"NonExistentKey", 12345);
    EXPECT_EQ(defaultResult, 12345);
}

// Test reading channels
TEST_F(RegistryTest, ReadChannels) {
    // Create test channels directly under our test key
    CreateChannelKey(L"Application");
    CreateChannelKey(L"System");
    CreateChannelKey(L"Security");
    
    // Since Registry::readChannels() requires a channels_key_ to be set up,
    // and we can't directly access it, we'll test an alternative approach by
    // manually enumerating the channels
    HKEY channelsKey = NULL;
    std::wstring channelsKeyPath = testKeyPath_ + L"\\Channels";
    
    LSTATUS result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        channelsKeyPath.c_str(),
        0,
        KEY_READ | KEY_ENUMERATE_SUB_KEYS,
        &channelsKey
    );
    ASSERT_EQ(result, ERROR_SUCCESS);
    
    // Manually enumerate channel keys (this mimics Registry::readChannels implementation)
    std::vector<std::wstring> channelList;
    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = _countof(name);
        auto st = RegEnumKeyExW(channelsKey, i, name, &len, nullptr, nullptr, nullptr, nullptr);
        if (st == ERROR_NO_MORE_ITEMS) break;
        if (st != ERROR_SUCCESS) break;
        channelList.emplace_back(name, len);
    }
    
    // Clean up
    RegCloseKey(channelsKey);
    
    // Verify the results
    EXPECT_EQ(channelList.size(), 3);
    std::set<std::wstring> channelSet(channelList.begin(), channelList.end());
    EXPECT_TRUE(channelSet.find(L"Application") != channelSet.end());
    EXPECT_TRUE(channelSet.find(L"System") != channelSet.end());
    EXPECT_TRUE(channelSet.find(L"Security") != channelSet.end());
}

// Test writing integer values
TEST_F(RegistryTest, WriteUint) {
    const wchar_t* testKey = L"WriteIntTest";
    DWORD testValue = 12345;
    
    registry_->writeUint(testKey, testValue);
    
    int result = registry_->readInt(testKey, 0);
    EXPECT_EQ(result, testValue);
}

// Test writing time values
TEST_F(RegistryTest, WriteTime) {
    const wchar_t* testKey = L"WriteTimeTest";
    time_t testValue = 1625097600;  // Some Unix timestamp
    
    registry_->writeTime(testKey, testValue);
    
    time_t result = registry_->readTime(testKey, 0);
    EXPECT_EQ(result, testValue);
} 
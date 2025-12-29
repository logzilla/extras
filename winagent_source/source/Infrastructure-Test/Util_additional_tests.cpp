#include "pch.h"
#include "../Infrastructure/Util.h"
#include <crtdbg.h>
#include <string>

// ------------------------------
// 1. Mock GetModuleFileNameW test
// ------------------------------
// Provide a mock implementation using the new injection hook

static DWORD WINAPI FakeGetModuleFileNameW(
    HMODULE /*unused*/, LPWSTR lpFilename, DWORD nSize)
{
    const wchar_t* mockPath = L"C:\\MockDir\\";
    size_t len = wcslen(mockPath);
    if (len + 1 > nSize) return 0;
    wcsncpy_s(lpFilename, nSize, mockPath, _TRUNCATE);
    return static_cast<DWORD>(len);
}

TEST(UtilAdditionalTests, GetThisPath_WithMockedGMFNW) {
    // Install mock
    Util::setModulePathProvider(&FakeGetModuleFileNameW);

    wchar_t buf[MAX_PATH] = {0};
    bool ok = Util::getThisPath(buf, MAX_PATH, true);
    ASSERT_TRUE(ok);
    EXPECT_STREQ(buf, L"C:\\MockDir\\");
    EXPECT_TRUE(_CrtCheckMemory()) << "Heap corruption detected with mocked GMFNW";

    // Restore default provider
    Util::setModulePathProvider(nullptr);
}

// -----------------------------------------------------
// 2. Isolation / stress test – call getThisPath in a loop
// -----------------------------------------------------
TEST(UtilAdditionalTests, GetThisPath_IsolationStress_NoHeapCorruption) {
    const int kIterations = 1000;
    for (int i = 0; i < kIterations; ++i) {
        ASSERT_TRUE(_CrtCheckMemory()) << "Heap corrupted before call iteration " << i;
        
        // Use buffer-based approach to avoid heap allocation issues
        wchar_t path_buffer[MAX_PATH] = {0};
        bool ok = Util::getThisPath(path_buffer, MAX_PATH, true);
        ASSERT_TRUE(ok) << "Iteration " << i << " failed to get path";
        ASSERT_TRUE(path_buffer[0] != L'\0') << "Iteration " << i << " returned empty path";
        
        ASSERT_TRUE(_CrtCheckMemory()) << "Heap corrupted after call iteration " << i;
    }
    EXPECT_TRUE(_CrtCheckMemory()) << "Heap corrupted after stress loop";
}

// ---------------------------------------------------------------------------
// 3. Create a wstring from the buffer-based approach and use it safely within the same module
// ---------------------------------------------------------------------------
TEST(UtilAdditionalTests, GetThisPath_AssignToExistingWstring_NoCorruption) {
    // Get the path using buffer-based approach
    wchar_t path_buffer[MAX_PATH] = {0};
    bool ok = Util::getThisPath(path_buffer, MAX_PATH, true);
    ASSERT_TRUE(ok);
    EXPECT_TRUE(path_buffer[0] != L'\0');
    
    // Create a wstring within this module (safe since no DLL boundary crossing)
    std::wstring path(path_buffer);
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(_CrtCheckMemory()) << "Heap corrupted after construction of wstring";

    // Use the path in a concatenation similar to production code to mimic
    // real-world usage (should still be safe within the same module)
    std::wstring fullPath = path + L"debug.log";
    EXPECT_GT(fullPath.size(), path.size());
    EXPECT_TRUE(_CrtCheckMemory()) << "Heap corrupted after concatenation";
}

#include "pch.h"
#include "../AgentLib/SidCache.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

using namespace std;

// Test fixture for SidCache tests
class SidCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create SidCache with default parameters (no expiry)
        sidCache = make_unique<SidCache>(128, 20, 0);
    }

    void TearDown() override {
        sidCache.reset();
    }

    unique_ptr<SidCache> sidCache;
    
    // Helper methods for frequently used operations
    void AddTestEntry(const char* sid, const char* domain, const char* name, bool notFound = false) {
        EXPECT_TRUE(sidCache->add(sid, domain, name, notFound));
    }
    
    bool LookupTestEntry(const char* sid, char* domainBuf, DWORD domainSize, char* nameBuf, DWORD nameSize) {
        return sidCache->lookup(sid, domainBuf, domainSize, nameBuf, nameSize);
    }
};

// Test initialization and size
TEST_F(SidCacheTest, InitializationAndSize) {
    EXPECT_NE(nullptr, sidCache);
    EXPECT_EQ(0, sidCache->size());
}

// Test basic add and lookup functionality
TEST_F(SidCacheTest, AddAndLookup) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* testDomain = "TESTDOMAIN";
    const char* testAccount = "TestUser";
    
    // Add to cache
    AddTestEntry(testSid, testDomain, testAccount);
    EXPECT_EQ(1, sidCache->size());
    
    // Lookup should return the data
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    EXPECT_TRUE(LookupTestEntry(testSid, domain, sizeof(domain), name, sizeof(name)));
    EXPECT_STREQ(testDomain, domain);
    EXPECT_STREQ(testAccount, name);
}

// Test not found entries
TEST_F(SidCacheTest, NotFoundEntries) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* testDomain = "(account not found)";
    const char* testAccount = testSid; // SID is used as the account name for not found entries
    
    // Add as not found entry
    AddTestEntry(testSid, testDomain, testAccount, true);
    
    // lookup should return false for not found entries
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    EXPECT_FALSE(LookupTestEntry(testSid, domain, sizeof(domain), name, sizeof(name)));
    EXPECT_STREQ(testDomain, domain);
    EXPECT_STREQ(testAccount, name);
}

// Test updating existing entry
TEST_F(SidCacheTest, UpdateExistingEntry) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* initialDomain = "INITIAL";
    const char* initialAccount = "Initial";
    const char* updatedDomain = "UPDATED";
    const char* updatedAccount = "Updated";
    
    // Add initial entry
    AddTestEntry(testSid, initialDomain, initialAccount);
    
    // Update with new values
    AddTestEntry(testSid, updatedDomain, updatedAccount, true);
    EXPECT_EQ(1, sidCache->size()); // Size should remain the same
    
    // Lookup should return updated values
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    EXPECT_FALSE(LookupTestEntry(testSid, domain, sizeof(domain), name, sizeof(name)));
    EXPECT_STREQ(updatedDomain, domain);
    EXPECT_STREQ(updatedAccount, name);
}

// Test removing entries
TEST_F(SidCacheTest, RemoveEntry) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* testDomain = "TESTDOMAIN";
    const char* testAccount = "TestUser";
    
    // Add to cache
    AddTestEntry(testSid, testDomain, testAccount);
    EXPECT_EQ(1, sidCache->size());
    
    // Remove entry
    EXPECT_TRUE(sidCache->removeSid(testSid));
    EXPECT_EQ(0, sidCache->size());
    
    // Lookup should now fail
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    EXPECT_FALSE(LookupTestEntry(testSid, domain, sizeof(domain), name, sizeof(name)));
    
    // Removing non-existent entry should return false
    EXPECT_FALSE(sidCache->removeSid("S-1-5-21-NONEXISTENT"));
}

// Test clearing the cache
TEST_F(SidCacheTest, ClearCache) {
    // Add multiple entries
    AddTestEntry("S-1-5-21-1", "Domain1", "User1");
    AddTestEntry("S-1-5-21-2", "Domain2", "User2");
    AddTestEntry("S-1-5-21-3", "Domain3", "User3");
    
    EXPECT_EQ(3, sidCache->size());
    
    // Clear the cache
    sidCache->clear();
    EXPECT_EQ(0, sidCache->size());
    
    // Lookups should now fail
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    EXPECT_FALSE(LookupTestEntry("S-1-5-21-1", domain, sizeof(domain), name, sizeof(name)));
    EXPECT_FALSE(LookupTestEntry("S-1-5-21-2", domain, sizeof(domain), name, sizeof(name)));
    EXPECT_FALSE(LookupTestEntry("S-1-5-21-3", domain, sizeof(domain), name, sizeof(name)));
}

// Test null parameters (error cases)
TEST_F(SidCacheTest, NullParameters) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* testDomain = "TESTDOMAIN";
    const char* testAccount = "TestUser";
    
    // Test add with null parameters
    EXPECT_FALSE(sidCache->add(nullptr, testDomain, testAccount));
    EXPECT_FALSE(sidCache->add(testSid, nullptr, testAccount));
    EXPECT_FALSE(sidCache->add(testSid, testDomain, nullptr));
    
    // Test lookup with null SID
    char domain[100] = { 0 };
    char name[100] = { 0 };
    EXPECT_FALSE(sidCache->lookup(nullptr, domain, sizeof(domain), name, sizeof(name)));
    
    // Add valid entry
    AddTestEntry(testSid, testDomain, testAccount);
    
    // Test lookup with null buffers - this should fail
    EXPECT_FALSE(sidCache->lookup(testSid, nullptr, sizeof(domain), name, sizeof(name)));
    EXPECT_FALSE(sidCache->lookup(testSid, domain, sizeof(domain), nullptr, sizeof(name)));
}

// Test buffer overflow protection
TEST_F(SidCacheTest, BufferOverflowProtection) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* testDomain = "TESTDOMAIN";
    const char* testAccount = "TestUser";
    
    // Add to cache
    AddTestEntry(testSid, testDomain, testAccount);
    
    // Test with buffers that are too small
    char tinyDomain[5] = { 0 }; // Too small for "TESTDOMAIN"
    char tinyName[5] = { 0 };   // Too small for "TestUser"
    
    EXPECT_FALSE(LookupTestEntry(testSid, tinyDomain, sizeof(tinyDomain), tinyName, sizeof(tinyName)));
    
    // Test with one buffer too small
    char normalDomain[100] = { 0 };
    EXPECT_FALSE(LookupTestEntry(testSid, normalDomain, sizeof(normalDomain), tinyName, sizeof(tinyName)));
    
    // Test with the other buffer too small
    char normalName[100] = { 0 };
    EXPECT_FALSE(LookupTestEntry(testSid, tinyDomain, sizeof(tinyDomain), normalName, sizeof(normalName)));
}

// Test extremely long strings (exceeding capacity)
TEST_F(SidCacheTest, ExtremelyLongStrings) {
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    string longDomain(SidCache::DOMAIN_CAPACITY + 10, 'D');
    string longName(SidCache::NAME_CAPACITY + 10, 'A');
    
    // This should fail due to capacity limits
    EXPECT_FALSE(sidCache->add(testSid, longDomain.c_str(), longName.c_str()));
    EXPECT_EQ(0, sidCache->size());
    
    // Try with strings exactly at capacity limit
    string exactDomain(SidCache::DOMAIN_CAPACITY - 1, 'D');
    string exactName(SidCache::NAME_CAPACITY - 1, 'A');
    
    EXPECT_TRUE(sidCache->add(testSid, exactDomain.c_str(), exactName.c_str()));
    EXPECT_EQ(1, sidCache->size());
    
    // Verify retrieval
    char domain[SidCache::DOMAIN_CAPACITY] = { 0 };
    char name[SidCache::NAME_CAPACITY] = { 0 };
    
    EXPECT_TRUE(LookupTestEntry(testSid, domain, sizeof(domain), name, sizeof(name)));
    EXPECT_STREQ(exactDomain.c_str(), domain);
    EXPECT_STREQ(exactName.c_str(), name);
}

// Test entry expiration
TEST_F(SidCacheTest, EntryExpiration) {
    // Create a cache with short expiry time (100ms)
    auto expiringCache = make_unique<SidCache>(128, 20, 100);
    
    const char* testSid = "S-1-5-21-12345678-87654321-12345678-1234";
    const char* testDomain = "TESTDOMAIN";
    const char* testAccount = "TestUser";
    
    // Add to cache
    EXPECT_TRUE(expiringCache->add(testSid, testDomain, testAccount));
    
    // Verify it's there immediately
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    EXPECT_TRUE(expiringCache->lookup(testSid, domain, sizeof(domain), name, sizeof(name)));
    
    // Sleep for longer than the expiry time
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Lookup should now fail due to expiration
    memset(domain, 0, sizeof(domain));
    memset(name, 0, sizeof(name));
    EXPECT_FALSE(expiringCache->lookup(testSid, domain, sizeof(domain), name, sizeof(name)));
    EXPECT_EQ(0, expiringCache->size()); // Entry should be removed on lookup
}

// Test the LookupAccountFromSidWithCache helper function
TEST_F(SidCacheTest, LookupAccountFromSidWithCache) {
    // This is a complex test - we need a known SID that we can resolve
    // For testing, we'll use the "Everyone" SID which should be available on all Windows systems
    const char* everyoneSid = "S-1-1-0";
    
    char domain[100] = { 0 };
    char name[100] = { 0 };
    
    // First call should do an actual Windows lookup
    DWORD result = LookupAccountFromSidWithCache(*sidCache, everyoneSid, 
                                              domain, sizeof(domain),
                                              name, sizeof(name));
    
    EXPECT_EQ(ERROR_SUCCESS, result);
    EXPECT_EQ(1, sidCache->size()); // Should cache the result
    
    // The Everyone account usually resolves to BUILTIN\Everyone or similar
    EXPECT_STRNE("", domain);
    EXPECT_STRNE("", name);
    
    // Second call should use the cache
    char domain2[100] = { 0 };
    char name2[100] = { 0 };
    
    result = LookupAccountFromSidWithCache(*sidCache, everyoneSid,
                                         domain2, sizeof(domain2),
                                         name2, sizeof(name2));
    
    EXPECT_EQ(ERROR_SUCCESS, result);
    EXPECT_STREQ(domain, domain2); // Should match first call
    EXPECT_STREQ(name, name2);     // Should match first call
    
    // Test with invalid SID
    char badDomain[100] = { 0 };
    char badName[100] = { 0 };
    
    result = LookupAccountFromSidWithCache(*sidCache, "INVALID-SID-FORMAT",
                                         badDomain, sizeof(badDomain),
                                         badName, sizeof(badName));
    
    EXPECT_EQ(ERROR_INVALID_SID, result);
}

// Test multi-threaded access
TEST_F(SidCacheTest, ThreadSafety) {
    const int numThreads = 10;
    const int opsPerThread = 100;
    
    // Vector to hold our threads
    vector<thread> threads;
    
    // Function for each thread to run
    auto threadFunc = [this, opsPerThread](int threadId) {
        for (int i = 0; i < opsPerThread; i++) {
            // Create unique SID strings for this thread
            string sid = "S-1-5-21-" + to_string(threadId) + "-" + to_string(i);
            string domain = "Domain" + to_string(threadId);
            string name = "User" + to_string(i);
            
            // Add to cache
            sidCache->add(sid.c_str(), domain.c_str(), name.c_str());
            
            // Look it up
            char domainBuf[100] = { 0 };
            char nameBuf[100] = { 0 };
            
            bool found = sidCache->lookup(sid.c_str(), domainBuf, sizeof(domainBuf),
                                         nameBuf, sizeof(nameBuf));
            
            EXPECT_TRUE(found);
            EXPECT_STREQ(domain.c_str(), domainBuf);
            EXPECT_STREQ(name.c_str(), nameBuf);
            
            // Remove from cache occasionally
            if (i % 10 == 0) {
                sidCache->removeSid(sid.c_str());
            }
        }
    };
    
    // Start threads
    for (int t = 0; t < numThreads; t++) {
        threads.push_back(thread(threadFunc, t));
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Final size should be consistent with operations
    EXPECT_LE(sidCache->size(), numThreads * opsPerThread);
}
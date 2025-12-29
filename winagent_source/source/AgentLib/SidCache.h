#pragma once
#include "../Infrastructure/BitmappedObjectPool.h"
#include <Windows.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include "../Infrastructure/Logger.h"

/**
 * Cache for SID lookups to improve performance when resolving the same SIDs repeatedly
 */
class SidCache
{
public:
    /**
     * Create a SID lookup cache with the specified capacity
     * @param chunkSize Number of entries per pool chunk
     * @param percentSlack Memory reuse policy (0-100, or -1 for never release)
     * @param maxAge Maximum age of cache entries in milliseconds (0 for no expiry)
     */
    SidCache(int chunkSize = 128, int percentSlack = 20, ULONGLONG maxAge = 3600000);
    
    /**
     * Destructor - cleans up all resources
     */
    ~SidCache();
    
    /**
     * Look up a SID in the cache
     * @param sidString The SID to look up
     * @param domainBuffer Buffer to receive domain name (UTF-8)
     * @param domainBufSize Size of domain buffer in characters
     * @param nameBuffer Buffer to receive account name (UTF-8)
     * @param nameBufSize Size of name buffer in characters
     * @return true if found in cache, false if not found
     */
    bool lookup(const char* sidString, char* domainBuffer, DWORD domainBufSize,
                char* nameBuffer, DWORD nameBufSize);
    
    /**
     * Add or update SID information in the cache
     * @param sidString The SID string
     * @param domainName The domain name for the SID
     * @param accountName The account name for the SID
     * @param isNotFound Flag indicating if this is a "not found" entry
     * @return true if added successfully
     */
    bool add(const char* sidString, const char* domainName, const char* accountName, bool isNotFound = false);
    
    /**
     * Remove a SID from the cache
     * @param sidString The SID to remove
     * @return true if found and removed
     */
    bool removeSid(const char* sidString);
    
    /**
     * Clear all entries from the cache
     */
    void clear();
    
    /**
     * Get the current number of entries in the cache
     */
    size_t size() const;

private:
    // Structure to hold cached account information
    struct SidCacheEntry {
        std::string* sidString = nullptr;
        std::string* domainName = nullptr;
        std::string* accountName = nullptr;
        ULONGLONG timeStamp = 0;
        bool notFound = false;  // Flag to indicate a cached "not found" entry
    };

    // Constants for string capacities
    static constexpr size_t SID_CAPACITY = 64;      // Max SID length
    static constexpr size_t DOMAIN_CAPACITY = 128;  // Max domain name length
    static constexpr size_t NAME_CAPACITY = 128;    // Max account name length
    
    // Pools for the different string types
    BitmappedObjectPool<std::string> sidPool_;
    BitmappedObjectPool<std::string> domainPool_;
    BitmappedObjectPool<std::string> namePool_;
    BitmappedObjectPool<SidCacheEntry> entryPool_;
    
    // Map from SID string to info object - using std::string as key
    std::unordered_map<std::string, SidCacheEntry*> sidToEntryMap_;
    
    // Maximum age of cache entries (0 for no expiry)
    ULONGLONG maxAge_;
    
    // Mutex for thread safety
    mutable std::mutex mutex_;
};

/**
 * Convenience function for looking up a SID with the cache
 * 
 * @param cache The SID cache to use
 * @param sidString The SID to look up
 * @param domainBuffer Buffer to receive domain name
 * @param domainBufSize Size of domain buffer in characters
 * @param nameBuffer Buffer to receive account name
 * @param nameBufSize Size of name buffer in characters
 * @return Windows error code (0 for success)
 */
DWORD LookupAccountFromSidWithCache(
    SidCache& cache,
    const char* sidString,
    char* domainBuffer,
    DWORD domainBufSize,
    char* nameBuffer,
    DWORD nameBufSize
);

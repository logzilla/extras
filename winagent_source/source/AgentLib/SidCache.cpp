#include "pch.h"

/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include <cstring>
#include "SidCache.h"
#include "../Infrastructure/WindowsUser.h"
#include <stdio.h> // Add for printf

SidCache::SidCache(int chunkSize, int percentSlack, ULONGLONG maxAge)
    : sidPool_(chunkSize, percentSlack),
    domainPool_(chunkSize, percentSlack),
    namePool_(chunkSize, percentSlack),
    entryPool_(chunkSize, percentSlack),
    maxAge_(maxAge) {
}

SidCache::~SidCache() {
    clear();
}

bool SidCache::lookup(const char* sidString, char* domainBuffer, DWORD domainBufSize,
    char* nameBuffer, DWORD nameBufSize) {
    if (!sidString || !domainBuffer || !nameBuffer) {
        return false;
    }

    // Initialize output buffers to ensure strcmp safety in caller if we return false later
    if (domainBufSize > 0) domainBuffer[0] = '\0';
    if (nameBufSize > 0) nameBuffer[0] = '\0';

    std::lock_guard<std::mutex> lock(mutex_);

    // Try to find in map - construct a temporary std::string for lookup
    auto it = sidToEntryMap_.find(sidString);
    if (it == sidToEntryMap_.end()) {
        return false;
    }

    SidCacheEntry* entry = it->second;

    // Check if entry has expired
    if (maxAge_ > 0) {
        ULONGLONG currentTime = GetTickCount64();
        if (currentTime - entry->timeStamp > maxAge_) {
            // Entry expired, remove from cache
            removeSid(sidString);
            return false;
        }
    }

    // Copy to output buffers with overflow protection
    if (entry->domainName->length() >= domainBufSize ||
        entry->accountName->length() >= nameBufSize) {
        return false;  // Buffers too small
    }

    strcpy_s(domainBuffer, domainBufSize, entry->domainName->c_str());
    strcpy_s(nameBuffer, nameBufSize, entry->accountName->c_str());

    // Return whether this is a found entry (true) or a not-found entry (false)
    return !entry->notFound;
}

bool SidCache::add(const char* sidString, const char* domainName, const char* accountName, bool isNotFound) {
    if (!sidString || !domainName || !accountName) {
        return false;
    }

    if (strlen(sidString) >= SID_CAPACITY ||
        strlen(domainName) >= DOMAIN_CAPACITY ||
        strlen(accountName) >= NAME_CAPACITY) {
        return false;  // Strings too long for our capacity
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if SID already in cache
    auto it = sidToEntryMap_.find(sidString);
    SidCacheEntry* entry = nullptr;

    if (it != sidToEntryMap_.end()) {
        // Update existing entry
        entry = it->second;

        // Update the strings (no reallocation since we reserved capacity)
        *entry->domainName = domainName;
        *entry->accountName = accountName;
        entry->notFound = isNotFound;
    }
    else {
        // Create a new entry
        entry = entryPool_.getAndMarkNextUnused();
        if (!entry) {
            return false;  // Failed to allocate entry
        }

        // Get strings from pools
        std::string* sid = sidPool_.getAndMarkNextUnused();
        std::string* domain = domainPool_.getAndMarkNextUnused();
        std::string* name = namePool_.getAndMarkNextUnused();

        if (!sid || !domain || !name) {
            // Failed to allocate one of the strings, clean up
            if (sid) sidPool_.markAsUnused(sid);
            if (domain) domainPool_.markAsUnused(domain);
            if (name) namePool_.markAsUnused(name);
            entryPool_.markAsUnused(entry);
            return false;
        }

        // Reserve capacity if not already reserved
        if (sid->capacity() < SID_CAPACITY) {
            sid->reserve(SID_CAPACITY);
        }
        if (domain->capacity() < DOMAIN_CAPACITY) {
            domain->reserve(DOMAIN_CAPACITY);
        }
        if (name->capacity() < NAME_CAPACITY) {
            name->reserve(NAME_CAPACITY);
        }

        // Set the strings
        *sid = sidString;
        *domain = domainName;
        *name = accountName;

        // Set up the entry object
        entry->sidString = sid;
        entry->domainName = domain;
        entry->accountName = name;
        entry->notFound = isNotFound;

        // Add to the map using the string content as key
        sidToEntryMap_[*sid] = entry;
    }

    // Update the timestamp
    entry->timeStamp = GetTickCount64();

    return true;
}

bool SidCache::removeSid(const char* sidString) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sidToEntryMap_.find(sidString);
    if (it == sidToEntryMap_.end()) {
        return false;
    }

    SidCacheEntry* entry = it->second;

    // Return strings to their pools
    sidPool_.markAsUnused(entry->sidString);
    domainPool_.markAsUnused(entry->domainName);
    namePool_.markAsUnused(entry->accountName);

    // Clear the entry object and return to pool
    entry->sidString = nullptr;
    entry->domainName = nullptr;
    entry->accountName = nullptr;
    entryPool_.markAsUnused(entry);

    // Remove from map
    sidToEntryMap_.erase(it);

    return true;
}

void SidCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Return all objects to their pools
    for (auto& pair : sidToEntryMap_) {
        SidCacheEntry* entry = pair.second;

        // Return strings to their pools
        if (entry->sidString) sidPool_.markAsUnused(entry->sidString);
        if (entry->domainName) domainPool_.markAsUnused(entry->domainName);
        if (entry->accountName) namePool_.markAsUnused(entry->accountName);

        // Clear and return the entry object
        entry->sidString = nullptr;
        entry->domainName = nullptr;
        entry->accountName = nullptr;
        entryPool_.markAsUnused(entry);
    }

    sidToEntryMap_.clear();
}

size_t SidCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sidToEntryMap_.size();
}

DWORD LookupAccountFromSidWithCache(
    SidCache& cache,
    const char* sidString,
    char* domainBuffer,
    DWORD domainBufSize,
    char* nameBuffer,
    DWORD nameBufSize
) {
    printf("[DEBUG] Enter LookupAccountFromSidWithCache for SID: %s\n", sidString ? sidString : "(null)");
    // Step 1: Always do SID lookup in cache first
    printf("[DEBUG] LookupAccountFromSidWithCache: Calling cache.lookup...\n");
    bool cacheFoundResult = cache.lookup(sidString, domainBuffer, domainBufSize, nameBuffer, nameBufSize);
    printf("[DEBUG] LookupAccountFromSidWithCache: cache.lookup returned %s\n", cacheFoundResult ? "true" : "false");

    if (cacheFoundResult) {
        // Step 2a: If retrieved with notFound == false, return retrieved information successful
        printf("[DEBUG] Exit LookupAccountFromSidWithCache (Cache Hit - Found)\n");
        return ERROR_SUCCESS;
    }
    else {
        printf("[DEBUG] LookupAccountFromSidWithCache: Checking if cache miss was 'account not found' entry...\n");
        // Need to check domainBuffer content AFTER ensuring lookup initialized it
        if (strcmp(domainBuffer, "(account not found)") == 0) {
             printf("[DEBUG] Exit LookupAccountFromSidWithCache (Cache Hit - Not Found)\n");
            // Step 2b: If retrieved with notFound == true, return the special values and ERROR_NONE_MAPPED
            return ERROR_NONE_MAPPED;
        }
        printf("[DEBUG] LookupAccountFromSidWithCache: Cache miss was genuine (not found or buffer too small or expired)\n");
    }

    // Step 3: Not in cache, do Windows SID lookup
    char tempDomain[128] = { 0 };
    char tempName[128] = { 0 };
    auto logger = LOG_THIS; // Keep existing logger for potential future use

    printf("[DEBUG] LookupAccountFromSidWithCache: Calling WindowsUser::LookupAccountFromSid...\n");
    WindowsUser::LookupResult result = WindowsUser::LookupAccountFromSid(
        sidString,
        tempDomain, sizeof(tempDomain),
        tempName, sizeof(tempName)
    );
    printf("[DEBUG] LookupAccountFromSidWithCache: WindowsUser::LookupAccountFromSid returned result code: %d\n", static_cast<int>(result));

    if (result == WindowsUser::LookupResult::Success) {
        // Step 4a: Found in Windows, add to cache and return info successful
        printf("[DEBUG] LookupAccountFromSidWithCache: Windows lookup success. Calling cache.add...\n");
        cache.add(sidString, tempDomain, tempName, false);  // false = found
        printf("[DEBUG] LookupAccountFromSidWithCache: cache.add returned.\n");

        // Copy to output buffers
        printf("[DEBUG] LookupAccountFromSidWithCache: Checking output buffer sizes for result...\n");
        if (strlen(tempDomain) >= domainBufSize || strlen(tempName) >= nameBufSize) {
            printf("[DEBUG] Exit LookupAccountFromSidWithCache (Insufficient buffer for Windows result)\n");
            return ERROR_INSUFFICIENT_BUFFER;
        }
        printf("[DEBUG] LookupAccountFromSidWithCache: Output buffer sizes sufficient.\n");

        printf("[DEBUG] LookupAccountFromSidWithCache: Copying result to output buffers...\n");
        strcpy_s(domainBuffer, domainBufSize, tempDomain);
        strcpy_s(nameBuffer, nameBufSize, tempName);
        printf("[DEBUG] LookupAccountFromSidWithCache: Result copied.\n");
        // logger->debug("Successful first lookup for SID %s: %s\\%s\n", domainBuffer, nameBuffer); // Keep original log if needed

        printf("[DEBUG] Exit LookupAccountFromSidWithCache (Windows Lookup Success)\n");
        return ERROR_SUCCESS;
    }
    else {
        // Map WindowsUser::LookupResult to Windows error codes
        printf("[DEBUG] LookupAccountFromSidWithCache: Windows lookup failed. Mapping error...\n");
        switch (result) {
        case WindowsUser::LookupResult::ErrorInvalidSid:
            // logger->debug("Invalid SID format for %s\n", sidString);
            printf("[DEBUG] Exit LookupAccountFromSidWithCache (Windows Lookup Error: Invalid SID)\n");
            return ERROR_INVALID_SID;
        case WindowsUser::LookupResult::ErrorDomainBufferTooSmall:
        case WindowsUser::LookupResult::ErrorNameBufferTooSmall:
            // logger->debug("Buffer too small for SID %s\n", sidString);
            printf("[DEBUG] Exit LookupAccountFromSidWithCache (Windows Lookup Error: Insufficient Buffer during lookup)\n");
             // This indicates internal buffers in WindowsUser::LookupAccountFromSid might be too small, OR API reported insufficient buffer
            return ERROR_INSUFFICIENT_BUFFER;
        case WindowsUser::LookupResult::ErrorAccountNotFound:
            // Step 4b: Not found in Windows, construct not found element and add to cache
            printf("[DEBUG] LookupAccountFromSidWithCache: Windows lookup reported Account Not Found. Creating 'not found' entry...\n");
            strcpy_s(tempDomain, sizeof(tempDomain), "(account not found)");
            strcpy_s(tempName, sizeof(tempName), sidString);

            // Add to cache as a "not found" entry
            cache.add(sidString, tempDomain, tempName, true);  // true = not found
            printf("[DEBUG] LookupAccountFromSidWithCache: cache.add returned.\n");

            // ALSO do debug log
            // logger->debug("SID %s not found, caching failed lookup", sidString);

            // Copy to output buffers
            printf("[DEBUG] LookupAccountFromSidWithCache: Checking output buffer sizes for 'not found' entry...\n");
            if (strlen(tempDomain) >= domainBufSize || strlen(tempName) >= nameBufSize) {
                // logger->debug("Buffer too small for not found SID %s\n", sidString);
                printf("[DEBUG] Exit LookupAccountFromSidWithCache (Insufficient buffer for 'not found' entry)\n");
                return ERROR_INSUFFICIENT_BUFFER;
            }
             printf("[DEBUG] LookupAccountFromSidWithCache: Output buffer sizes sufficient.\n");

            printf("[DEBUG] LookupAccountFromSidWithCache: Copying 'not found' entry to output buffers...\n");
            strcpy_s(domainBuffer, domainBufSize, tempDomain);
            strcpy_s(nameBuffer, nameBufSize, tempName);
            printf("[DEBUG] LookupAccountFromSidWithCache: 'Not found' entry copied.\n");

            printf("[DEBUG] Exit LookupAccountFromSidWithCache (Windows Lookup Account Not Found)\n");
            return ERROR_NONE_MAPPED;
        default:
            // logger->warning("Unexpected error %d for SID %s\n", static_cast<int>(result), sidString);
            printf("[DEBUG] Exit LookupAccountFromSidWithCache (Windows Lookup Unknown Error: %d)\n", static_cast<int>(result));
            return ERROR_GEN_FAILURE;
        }
    }
}

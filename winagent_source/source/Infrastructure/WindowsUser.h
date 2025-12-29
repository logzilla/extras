#pragma once
#include <Windows.h>

// Define proper import/export macro for DLL
#ifdef INFRASTRUCTURE_EXPORTS
#define INFRASTRUCTURE_API __declspec(dllexport)
#else
#define INFRASTRUCTURE_API __declspec(dllimport)
#endif

/**
 * Utility class for Windows user-related operations
 */
class INFRASTRUCTURE_API WindowsUser
{
public:
    /**
     * Possible results from LookupAccountFromSid operation.
     */
    enum class LookupResult : int
    {
        Success = 0,
        ErrorInvalidSid = 1,
        ErrorDomainBufferTooSmall = 2,
        ErrorNameBufferTooSmall = 3,
        ErrorAccountNotFound = 4,
        ErrorGeneralFailure = 5
    };

    /**
     * Looks up account information from a SID string.
     *
     * @param sidString      Null-terminated SID string (e.g. "S-1-5-18")
     * @param domainBuffer   Buffer to receive domain name
     * @param domainBufSize  Size of domain buffer in characters (not bytes)
     * @param nameBuffer     Buffer to receive account name
     * @param nameBufSize    Size of name buffer in characters (not bytes)
     *
     * @return LookupResult indicating success or the specific failure reason
     */
    static LookupResult LookupAccountFromSid(
        const char* sidString,
        char* domainBuffer,
        DWORD domainBufSize,
        char* nameBuffer,
        DWORD nameBufSize
    );
};

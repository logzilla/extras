#include "pch.h"
#include "WindowsUser.h"
#include <Sddl.h>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

WindowsUser::LookupResult WindowsUser::LookupAccountFromSid(
    const char* sidString,
    char* domainBuffer,
    DWORD domainBufSize,
    char* nameBuffer,
    DWORD nameBufSize
) {
    printf("[DEBUG] Enter WindowsUser::LookupAccountFromSid for SID: %s\n", sidString ? sidString : "(null)");
    // Validate input
    if (!sidString || !domainBuffer || !nameBuffer || domainBufSize == 0 || nameBufSize == 0) {
        printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (Input validation failed)\n");
        return WindowsUser::LookupResult::ErrorGeneralFailure;
    }

    // Initialize output buffers to empty strings
    if (domainBufSize > 0) domainBuffer[0] = '\0';
    if (nameBufSize > 0) nameBuffer[0] = '\0';

    // Convert input to wide string for Windows API
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Converting SID string to wide char...\n");
    WCHAR wideSidBuf[256] = { 0 };
    if (MultiByteToWideChar(CP_UTF8, 0, sidString, -1, wideSidBuf, 256) == 0) {
        printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (MultiByteToWideChar failed)\n");
        return WindowsUser::LookupResult::ErrorGeneralFailure;
    }
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: SID string converted.\n");

    // Convert string SID to SID structure
    PSID sid = nullptr;
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Converting string SID to SID struct...\n");
    if (!ConvertStringSidToSidW(wideSidBuf, &sid)) {
        DWORD err = GetLastError();
        printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (ConvertStringSidToSidW failed, Error: %lu)\n", err);
        return (err == ERROR_INVALID_SID) ? WindowsUser::LookupResult::ErrorInvalidSid
                                          : WindowsUser::LookupResult::ErrorGeneralFailure;
    }
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: String SID converted.\n");

    // Lookup account information
    WCHAR wDomainName[128] = { 0 };
    WCHAR wAccountName[128] = { 0 };
    DWORD domainSize = 128;
    DWORD nameSize = 128;
    SID_NAME_USE accountType;

    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Calling LookupAccountSidW...\n");
    BOOL success = LookupAccountSidW(
        nullptr,         // Local computer
        sid,
        wAccountName,
        &nameSize,
        wDomainName,
        &domainSize,
        &accountType
    );
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: LookupAccountSidW returned %s.\n", success ? "TRUE" : "FALSE");

    // Free the SID allocated by ConvertStringSidToSidW
    if (sid) {
        printf("[DEBUG] WindowsUser::LookupAccountFromSid: Freeing SID structure...\n");
        LocalFree(sid);
        printf("[DEBUG] WindowsUser::LookupAccountFromSid: SID structure freed.\n");
    }

    if (!success) {
        DWORD err = GetLastError();
        printf("[DEBUG] WindowsUser::LookupAccountFromSid: LookupAccountSidW failed, Error: %lu\n", err);
        if (err == ERROR_NONE_MAPPED) {
            printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (ERROR_NONE_MAPPED)\n");
            return WindowsUser::LookupResult::ErrorAccountNotFound;
        }
        else if (err == ERROR_INSUFFICIENT_BUFFER) {
            printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (ERROR_INSUFFICIENT_BUFFER)\n");
            // Determine which buffer was too small
            if (domainSize > 128 && nameSize > 128) {
                return WindowsUser::LookupResult::ErrorDomainBufferTooSmall;
            }
            else if (domainSize > 128) {
                return WindowsUser::LookupResult::ErrorDomainBufferTooSmall;
            }
            else {
                return WindowsUser::LookupResult::ErrorNameBufferTooSmall;
            }
        }
        printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (Other LookupAccountSidW error)\n");
        return WindowsUser::LookupResult::ErrorGeneralFailure;
    }
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: LookupAccountSidW succeeded.\n");

    // Convert wide strings back to multibyte for output
    char tempDomainBuffer[128] = { 0 };
    char tempNameBuffer[128] = { 0 };

    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Converting domain WCHAR to char...\n");
    WideCharToMultiByte(CP_UTF8, 0, wDomainName, -1, tempDomainBuffer, sizeof(tempDomainBuffer), nullptr, nullptr);
    tempDomainBuffer[sizeof(tempDomainBuffer) - 1] = '\0'; // Ensure null termination
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Domain converted.\n");

    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Converting name WCHAR to char...\n");
    WideCharToMultiByte(CP_UTF8, 0, wAccountName, -1, tempNameBuffer, sizeof(tempNameBuffer), nullptr, nullptr);
    tempNameBuffer[sizeof(tempNameBuffer) - 1] = '\0'; // Ensure null termination
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Name converted.\n");

    // Check if the output buffers are large enough
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Checking output buffer sizes...\n");
    if (strlen(tempDomainBuffer) >= domainBufSize) {
        printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (Domain buffer too small for result)\n");
        return WindowsUser::LookupResult::ErrorDomainBufferTooSmall;
    }
    if (strlen(tempNameBuffer) >= nameBufSize) {
        printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (Name buffer too small for result)\n");
        return WindowsUser::LookupResult::ErrorNameBufferTooSmall;
    }
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Output buffer sizes sufficient.\n");

    // Copy the results to the output buffers
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Copying domain to output buffer...\n");
    strcpy_s(domainBuffer, domainBufSize, tempDomainBuffer);
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Domain copied.\n");
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Copying name to output buffer...\n");
    strcpy_s(nameBuffer, nameBufSize, tempNameBuffer);
    printf("[DEBUG] WindowsUser::LookupAccountFromSid: Name copied.\n");

    printf("[DEBUG] Exit WindowsUser::LookupAccountFromSid (Success)\n");
    return WindowsUser::LookupResult::Success;
}

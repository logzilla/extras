#include "pch.h"
#include "WindowsEventLog.h"
#include <cstring>

WindowsEventLog::WindowsEventLog(const char* sourceName)
    : sourceName_(sourceName) {
    // Make sure the source is registered
    EnsureSourceRegistered(sourceName);
}

WindowsEventLog::~WindowsEventLog() {
    // Nothing to do here
}

bool WindowsEventLog::WriteEvent(EventType eventType, DWORD eventId, const char* message) {
    if (!message) {
        return false;
    }

    HANDLE hEventLog = RegisterEventSourceA(NULL, sourceName_.c_str());
    if (!hEventLog) {
        return false;
    }

    const char* messages[] = { message };
    BOOL result = ReportEventA(
        hEventLog,                       // Event log handle
        static_cast<WORD>(eventType),    // Event type
        0,                               // Category (0 = no category)
        eventId,                         // Event identifier
        NULL,                            // User security identifier (NULL = no SID)
        1,                               // Number of strings to merge with message
        0,                               // Size of binary data (in bytes)
        messages,                        // Array of strings to merge with message
        NULL                             // Binary data buffer
    );

    DeregisterEventSource(hEventLog);
    return (result != FALSE);
}

bool WindowsEventLog::WriteEvent(EventType eventType, DWORD eventId, const char* title, const char* message) {
    if (!title || !message) {
        return false;
    }

    // Combine title and message
    std::string fullMessage = std::string(title) + "\r\n" + message;
    return WriteEvent(eventType, eventId, fullMessage.c_str());
}

bool WindowsEventLog::EnsureSourceRegistered(const char* sourceName) {
    // Check if source is already registered
    HKEY hKey;
    std::string keyPath = "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" + std::string(sourceName);
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true; // Source already registered
    }

    // Source not registered, try to register it
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH) == 0) {
        return false;
    }

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, NULL, 
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    // Set EventMessageFile registry value to point to our executable
    if (RegSetValueExA(hKey, "EventMessageFile", 0, REG_EXPAND_SZ, 
                       reinterpret_cast<BYTE*>(modulePath), static_cast<DWORD>(strlen(modulePath) + 1)) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    // Set the supported event types
    DWORD typesSupported = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    if (RegSetValueExA(hKey, "TypesSupported", 0, REG_DWORD, 
                       reinterpret_cast<BYTE*>(&typesSupported), sizeof(DWORD)) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

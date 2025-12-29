#pragma once

#ifdef INFRASTRUCTURE_EXPORTS
#define INFRASTRUCTURE_API __declspec(dllexport)
#else
#define INFRASTRUCTURE_API __declspec(dllimport)
#endif

#include <windows.h>
#include <string>

/**
 * @brief WindowsEventLog class for writing to the Windows Event Log
 */
class INFRASTRUCTURE_API WindowsEventLog {
public:
    /**
     * @brief Severity levels for Windows Event Log
     */
    enum class EventType : WORD {
        INFORMATION_EVENT = EVENTLOG_INFORMATION_TYPE,
        WARNING_EVENT = EVENTLOG_WARNING_TYPE,
        ERROR_EVENT = EVENTLOG_ERROR_TYPE
    };

    /**
     * @brief Constructor - initializes the source name
     * @param sourceName The event source name to register/use
     */
    WindowsEventLog(const char* sourceName = "LogZilla");

    /**
     * @brief Destructor - cleans up any resources
     */
    ~WindowsEventLog();

    /**
     * @brief Writes an event to the Windows Application Event Log
     * 
     * @param eventType Type of event (INFORMATION, WARNING, ERROR)
     * @param eventId Numeric event ID (application-defined)
     * @param message The event message to log
     * @return bool True if successful, false otherwise
     */
    bool WriteEvent(EventType eventType, DWORD eventId, const char* message);

    /**
     * @brief Writes an event to the Windows Application Event Log with a title
     * 
     * @param eventType Type of event (INFORMATION, WARNING, ERROR)
     * @param eventId Numeric event ID (application-defined)
     * @param title Title/category of the event
     * @param message The event message to log
     * @return bool True if successful, false otherwise
     */
    bool WriteEvent(EventType eventType, DWORD eventId, const char* title, const char* message);

    /**
     * @brief Ensures that the event source is registered in the registry
     * 
     * @param sourceName The event source name to register
     * @return bool True if successfully registered or already exists
     */
    static bool EnsureSourceRegistered(const char* sourceName);

private:
    std::string sourceName_;
};


#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED


#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <queue>
#include <cstdint>

using std::string;
using std::queue;

namespace Syslog_agent {

class EventLogger {
public:
    enum class LogDestination {
        SubscribedEvents,
        GeneratedEvents,
        SentEvents,
        SentData
    };

    static bool log(const LogDestination dest, const char* format, ...);
    static bool logNetworkSend(const char* buf, size_t length);
    static bool logNetworkReceive(const char* result, size_t result_length);
    static bool isQueueEmpty();
    static void enqueueEventForLogging(const char* event);
    static string queuePopFront();

    // Delete copy constructor and assignment operator
    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;
    // Delete move constructor and assignment operator
    EventLogger(EventLogger&&) = delete;
    EventLogger& operator=(EventLogger&&) = delete;
    
    ~EventLogger();

private:
    struct LoggedEvent {
        char* message_buffer;
        uint32_t data_length;
        uint64_t timestamp;
    };

    static EventLogger& singleton();
    static std::mutex logger_lock_;
    static std::unique_ptr<EventLogger, std::default_delete<EventLogger>> instance_;
    std::queue<LoggedEvent> _queued_events_to_log;

    // File names (as wide char arrays to avoid heap allocations)
    static inline constexpr wchar_t SUBSCRIBED_EVENTS_FILENAME[] = L"subscribed_events.txt";
    static inline constexpr wchar_t GENERATED_EVENTS_FILENAME[] = L"generated_events.txt";
    static inline constexpr wchar_t SENT_EVENTS_FILENAME[] = L"sent_events.txt";
    static inline constexpr wchar_t SENT_DATA_FILENAME[] = L"sent_data.txt";

    // Private constructor for singleton
    EventLogger() = default;

    // Helper methods
    static const wchar_t* getFilenameForDestination(const LogDestination dest);
    void writeToFile(const LogDestination dest, const char* message);
    void writeToFile(const LogDestination dest, const char* message, size_t length);
    void writeSentData(const char* data, size_t length);
};


} // namespace Syslog_agent

#endif
/*
SyslogAgent: a syslog agent for Windows
Copyright c 2021 Logzilla Corp.
*/

#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED

#pragma once
#include <stdarg.h>
#include "Logger.h"

using namespace std;

namespace Syslog_agent {

    class StatefulLogger
    {
    public:
        static constexpr size_t MAX_EVENT_ID_LENGTH = 256;
        static constexpr size_t MAX_EVENT_LOG_LENGTH = 1024;
        static constexpr size_t MAX_EVENT_DATETIME_LENGTH = 64;

        static void setEventId(const char* eventId);
        static void setEventLog(const char* eventLog);
        static void setEventDatetime(const char* eventDatetime);
        static void logEvent();
        static StatefulLogger* singleton();

    private:
        StatefulLogger() = default;  // Private constructor
        StatefulLogger(const StatefulLogger&) = delete;  // Prevent copying
        StatefulLogger& operator=(const StatefulLogger&) = delete;

        char current_event_id_[MAX_EVENT_ID_LENGTH] = {};
        char current_event_log_[MAX_EVENT_LOG_LENGTH] = {};
        char current_event_datetime_[MAX_EVENT_DATETIME_LENGTH] = {};
    };

} // namespace Syslog_agent

#endif

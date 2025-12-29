#include "stdafx.h"
#include "StatefulLogger.h"
#include "Util.h"
#include "Logger.h"


#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED

namespace Syslog_agent {

    StatefulLogger* StatefulLogger::singleton() {
        static StatefulLogger instance;
        return &instance;
    }

    void StatefulLogger::setEventId(const char* eventId) {
        if (!eventId)
            return;
#ifdef _WIN32
        strncpy_s(singleton()->current_event_id_, MAX_EVENT_ID_LENGTH, eventId, _TRUNCATE);
#else
        strncpy(singleton()->current_event_id_, eventId, MAX_EVENT_ID_LENGTH - 1);
        singleton()->current_event_id_[MAX_EVENT_ID_LENGTH - 1] = '\0';
#endif
    }

    void StatefulLogger::setEventLog(const char* eventLog) {
        if (!eventLog)
            return;
#ifdef _WIN32
        strncpy_s(singleton()->current_event_log_, MAX_EVENT_LOG_LENGTH, eventLog, _TRUNCATE);
#else
        strncpy(singleton()->current_event_log_, eventLog, MAX_EVENT_LOG_LENGTH - 1);
        singleton()->current_event_log_[MAX_EVENT_LOG_LENGTH - 1] = '\0';
#endif
    }

    void StatefulLogger::setEventDatetime(const char* eventDatetime) {
        if (!eventDatetime)
            return;
#ifdef _WIN32
        strncpy_s(singleton()->current_event_datetime_, MAX_EVENT_DATETIME_LENGTH, eventDatetime, _TRUNCATE);
#else
        strncpy(singleton()->current_event_datetime_, eventDatetime, MAX_EVENT_DATETIME_LENGTH - 1);
        singleton()->current_event_datetime_[MAX_EVENT_DATETIME_LENGTH - 1] = '\0';
#endif
    }

    void StatefulLogger::logEvent() {
        // Replace undefined LOG_THIS with an explicit call to obtain a logger.
        Logger* logger = Logger::getLoggerByKey("DefaultLogger");
        char buf[256];

        // Call epochToDateTime from Util in the global namespace.
        Util::epochToDateTime(singleton()->current_event_datetime_, buf);

        // Call the logger using the global Logger class. Note the log level is now qualified in the global namespace.
        logger->log(::Logger::DEBUG2, "Queuing Event ID: %s, Event Log: %s, Event Datetime: %s (%s)\n",
            singleton()->current_event_id_,
            singleton()->current_event_log_,
            buf,  // Use the converted datetime string
            singleton()->current_event_datetime_);
    }

} // namespace Syslog_agent

#endif

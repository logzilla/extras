#include "stdafx.h"
#include "EventLogger.h"
#include "Globals.h"
#include "Logger.h"
#include "Util.h"
#include <chrono>
#include <ctime>
#include <string>
#include <fstream>


#if ONLY_FOR_DEBUGGING_CURRENTLY_DISABLED

using std::string;

namespace Syslog_agent {

// Static member definitions
std::mutex EventLogger::logger_lock_;
std::unique_ptr<EventLogger, std::default_delete<EventLogger>> EventLogger::instance_;

EventLogger& EventLogger::singleton() {
    std::lock_guard<std::mutex> lock(logger_lock_);
    if (!instance_) {
        instance_ = std::unique_ptr<EventLogger, std::default_delete<EventLogger>>(new EventLogger());
    }
    return *instance_;
}

EventLogger::~EventLogger() {
    // Clean up any remaining message buffers in the queue
    std::lock_guard<std::mutex> lock(logger_lock_);
    while (!_queued_events_to_log.empty()) {
        LoggedEvent& event = _queued_events_to_log.front();
        Globals::instance()->releaseMessageBuffer(event.message_buffer);
        _queued_events_to_log.pop();
    }
}

const wchar_t* EventLogger::getFilenameForDestination(const LogDestination dest) {
    switch (dest) {
        case LogDestination::SubscribedEvents:
            return SUBSCRIBED_EVENTS_FILENAME;
        case LogDestination::GeneratedEvents:
            return GENERATED_EVENTS_FILENAME;
        case LogDestination::SentEvents:
            return SENT_EVENTS_FILENAME;
        case LogDestination::SentData:
            return SENT_DATA_FILENAME;
        default:
            return nullptr;
    }
}

void EventLogger::writeToFile(const LogDestination dest, const char* message) {
    if (!message) return;
    writeToFile(dest, message, strlen(message));
}

void EventLogger::writeToFile(const LogDestination dest, const char* message, size_t length) {
    auto logger = LOG_THIS;
    if (!message || length == 0) return;

    const wchar_t* filename = getFilenameForDestination(dest);
    if (!filename) {
        logger->recoverable_error("EventLogger::writeToFile()> Invalid destination\n");
        return;
    }

    wchar_t path_buffer[MAX_PATH] = {0};
    if (!Util::getThisPath(path_buffer, MAX_PATH, true)) {
        logger->recoverable_error("EventLogger::writeToFile()> Failed to get path\n");
        return;
    }
    
    if (wcslen(path_buffer) + wcslen(filename) >= MAX_PATH) {
        logger->recoverable_error("EventLogger::writeToFile()> Path too long\n");
        return;
    }
    wcscat_s(path_buffer, MAX_PATH, filename);
    
    FILE* file = nullptr;
    if (_wfopen_s(&file, path_buffer, L"ab") != 0 || !file) {
        logger->recoverable_error("EventLogger::writeToFile()> Failed to open file %S\n", path_buffer);
        return;
    }

    // Write the message
    if (fwrite(message, 1, length, file) != length) {
        logger->recoverable_error("EventLogger::writeToFile()> Failed to write to file %S\n", path_buffer);
    }
    fclose(file);
}

void EventLogger::writeSentData(const char* data, size_t length) {
    auto logger = LOG_THIS;
    if (!data || length == 0) return;

    wchar_t path_buffer[MAX_PATH] = {0};
    if (!Util::getThisPath(path_buffer, MAX_PATH, true)) {
        logger->recoverable_error("EventLogger::writeSentData()> Failed to get path\n");
        return;
    }
    
    if (wcslen(path_buffer) + wcslen(SENT_DATA_FILENAME) >= MAX_PATH) {
        logger->recoverable_error("EventLogger::writeSentData()> Path too long\n");
        return;
    }
    wcscat_s(path_buffer, MAX_PATH, SENT_DATA_FILENAME);
    
    FILE* file = nullptr;
    if (_wfopen_s(&file, path_buffer, L"ab") != 0 || !file) {
        logger->recoverable_error("EventLogger::writeSentData()> Failed to open file %S\n", path_buffer);
        return;
    }

    // Get timestamp
    char timestamp[40];
    Logger::getDateTimeStr(timestamp, sizeof(timestamp));

    // Write header with timestamp and byte count
    fprintf(file, "## [%s] LogZilla Windows Agent: sent %zu bytes\n", timestamp, length);
    
    // Write opening markdown code block
    fputs("```\n", file);

    // Write the actual data
    fwrite(data, 1, length, file);

    // Add newline if data doesn't end with one
    if (length > 0 && data[length - 1] != '\n') {
        fputc('\n', file);
    }

    // Write closing markdown code block and blank line
    fputs("```\n\n", file);
    fclose(file);
}

bool EventLogger::log(const LogDestination dest, const char* format, ...) {
    auto logger = LOG_THIS;
    try {
        char buffer[2048];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        EventLogger& instance = singleton();
        std::lock_guard<std::mutex> lock(logger_lock_);
        instance.writeToFile(dest, buffer);
        return true;
    }
    catch (const std::exception& e) {
        logger->recoverable_error("EventLogger::log()> %s\n", e.what());
        return false;
    }
}


void EventLogger::enqueueEventForLogging(const char* event) {
    auto logger = LOG_THIS;
    if (!event) {
        return;
    }
    EventLogger& instance = singleton();
    std::lock_guard<std::mutex> lock(logger_lock_);
    
    LoggedEvent event_struct;
    event_struct.message_buffer = Globals::instance()->getMessageBuffer("EventLogger::enqueueEventForLogging()");
    if (!event_struct.message_buffer) {
        logger->recoverable_error("EventLogger::enqueueEventForLogging()> Failed to get message buffer\n");
        return;
    }

    event_struct.data_length = static_cast<uint32_t>(strlen(event));
    memcpy(event_struct.message_buffer, event, event_struct.data_length);
    event_struct.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    instance._queued_events_to_log.push(event_struct);
}

string EventLogger::queuePopFront() {
    EventLogger& instance = singleton();
    std::lock_guard<std::mutex> lock(logger_lock_);
    
    if (instance._queued_events_to_log.empty()) {
        return "";
    }
    
    LoggedEvent& event = instance._queued_events_to_log.front();
    string result(event.message_buffer, event.data_length);
    Globals::instance()->releaseMessageBuffer(event.message_buffer);
    instance._queued_events_to_log.pop();
    return result;
}

bool EventLogger::isQueueEmpty() {
    EventLogger& instance = singleton();
    std::lock_guard<std::mutex> lock(logger_lock_);
    return instance._queued_events_to_log.empty();
}

bool EventLogger::logNetworkSend(const char* buf, size_t length) {
    auto logger = LOG_THIS;
    if (!buf || length == 0) {
        return false;
    }
    try {
        EventLogger& instance = singleton();
        std::lock_guard<std::mutex> lock(logger_lock_);

        // Get timestamp
        char timestamp[40];
        Logger::getDateTimeStr(timestamp, sizeof(timestamp));

        // Write header with timestamp
        FILE* file = nullptr;
        wchar_t path_buffer[MAX_PATH] = {0};
        if (!Util::getThisPath(path_buffer, MAX_PATH, true)) {
            logger->recoverable_error("EventLogger::logNetworkSend()> Failed to get path\n");
            return false;
        }
        
        if (wcslen(path_buffer) + wcslen(SENT_DATA_FILENAME) >= MAX_PATH) {
            logger->recoverable_error("EventLogger::logNetworkSend()> Path too long\n");
            return false;
        }
        wcscat_s(path_buffer, MAX_PATH, SENT_DATA_FILENAME);
        
        if (_wfopen_s(&file, path_buffer, L"ab") != 0 || !file) {
            logger->recoverable_error("EventLogger::logNetworkSend()> Failed to open file %S\n", path_buffer);
            return false;
        }

        fprintf(file, "## [%s] LogZilla Windows Agent: sending %zu bytes\n", timestamp, length);
        fputs("```\n", file);
        fwrite(buf, 1, length, file);
        if (length > 0 && buf[length - 1] != '\n') {
            fputc('\n', file);
        }
        fputs("```\n\n", file);
        fclose(file);
        return true;
    }
    catch (const std::exception& e) {
        logger->recoverable_error("EventLogger::logNetworkSend()> %s\n", e.what());
        return false;
    }
}

bool EventLogger::logNetworkReceive(const char* result, size_t result_length) {
    auto logger = LOG_THIS;
    if (!result || result_length == 0) {
        return false;
    }
    try {
        EventLogger& instance = singleton();
        std::lock_guard<std::mutex> lock(logger_lock_);

        // Get timestamp
        char timestamp[40];
        Logger::getDateTimeStr(timestamp, sizeof(timestamp));

        // Write header with timestamp
        FILE* file = nullptr;
        wchar_t path_buffer[MAX_PATH] = {0};
        if (!Util::getThisPath(path_buffer, MAX_PATH, true)) {
            logger->recoverable_error("EventLogger::logNetworkReceive()> Failed to get path\n");
            return false;
        }
        
        if (wcslen(path_buffer) + wcslen(SENT_DATA_FILENAME) >= MAX_PATH) {
            logger->recoverable_error("EventLogger::logNetworkReceive()> Path too long\n");
            return false;
        }
        wcscat_s(path_buffer, MAX_PATH, SENT_DATA_FILENAME);
        
        if (_wfopen_s(&file, path_buffer, L"ab") != 0 || !file) {
            logger->recoverable_error("EventLogger::logNetworkReceive()> Failed to open file %S\n", path_buffer);
            return false;
        }

        // Find the first line ending
        const char* newline = (const char*)memchr(result, '\n', result_length);
        size_t first_line_length = newline ? (newline - result) : result_length;
        
        // Write result header with first line
        fprintf(file, "### [%s] LogZilla Windows Agent network receive result: %.*s\n", 
            timestamp, (int)first_line_length, result);

        // If there's more content after the first line, write it in a code block
        if (newline && (result_length > first_line_length + 1)) {
            const char* json_start = newline + 1;
            size_t json_length = result_length - (json_start - result);
            
            fputs("```\n", file);
            fwrite(json_start, 1, json_length, file);
            if (json_length > 0 && json_start[json_length - 1] != '\n') {
                fputc('\n', file);
            }
            fputs("```\n\n", file);
        }

        fclose(file);
        return true;
    }
    catch (const std::exception& e) {
        logger->recoverable_error("EventLogger::logNetworkReceive()> %s\n", e.what());
        return false;
    }
}

} // namespace Syslog_agent

#endif

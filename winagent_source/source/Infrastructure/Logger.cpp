#include "pch.h"
#include "Logger.h"
#include <cstdarg>
#include <string>
#include <cstring>
#include <cstdio>
#include "Util.h"
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward declarations for C-style SEH helper functions
void WriteEmergencyLog(const char* source, const char* file, int line);
void SafeCallFatalHandler(Logger::FATAL_ERROR_HANDLER handler, const char* message);

// C-style SEH helper functions that don't have C++ objects requiring unwinding

// Helper function to safely call the fatal error handler with SEH
void SafeCallFatalHandler(Logger::FATAL_ERROR_HANDLER handler, const char* message) {
    __try {
        handler(message);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        // If the handler crashes, write to emergency log
        HANDLE hFile = CreateFileA("syslogagent_emergency.log", 
                                 FILE_APPEND_DATA, 
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            const char* crash_msg = "FATAL: Error handler crashed\r\n";
            DWORD bytesWritten;
            WriteFile(hFile, crash_msg, (DWORD)strlen(crash_msg), &bytesWritten, NULL);
            CloseHandle(hFile);
        }
    }
}

// Helper function to write to emergency log with SEH
void WriteEmergencyLog(const char* source, const char* file, int line) {
    __try {
        DWORD exceptionCode = GetLastError(); // Not truly an exception code, but useful info
        
        char emergencyMessage[256];
        sprintf_s(emergencyMessage, "CRITICAL ERROR in %s at %s:%d (error: %d)\r\n", 
                 source, file, line, exceptionCode);
        
        HANDLE hFile = CreateFileA("syslogagent_emergency.log", 
                                 FILE_APPEND_DATA, 
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten;
            WriteFile(hFile, emergencyMessage, (DWORD)strlen(emergencyMessage), &bytesWritten, NULL);
            CloseHandle(hFile);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        // If even this fails, we're in real trouble - try absolute last resort
        fprintf(stderr, "CRITICAL: Failed to write emergency log for %s at %s:%d\n", 
                source, file, line);
    }
}

using namespace std;

// Static member initialization.
std::mutex Logger::registry_mutex_;
std::map<const char*, Logger*, Logger::CStringCompare> Logger::logger_registry_;
Logger* Logger::default_logger_ = nullptr;
int Logger::is_unittest_running_ = -1;

// Define the static dummy logger.
Logger Logger::dummyLogger_("DummyLogger");

// --- Setup methods ---
void Logger::setDefaultLogger(shared_ptr<Logger> logger) {
    lock_guard<mutex> lock(registry_mutex_);
    default_logger_ = logger.get();
}

void Logger::setLogger(shared_ptr<Logger> logger, const vector<string>& names) {
    lock_guard<mutex> lock(registry_mutex_);
    for (const auto& name : names) {
        // Using c_str() here is acceptable because this is setup code.
        logger_registry_[name.c_str()] = logger.get();
    }
}

Logger* Logger::getLoggerByKey(const char* key) {
    if (!key) return &dummyLogger_;
    auto it = logger_registry_.find(key);
    return (it != logger_registry_.end()) ? it->second : &dummyLogger_;
}

void Logger::setFatalErrorHandler(FATAL_ERROR_HANDLER fatal_error_handler) {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    fatal_error_handler_ = fatal_error_handler;
}

void Logger::setLogLevel(const Logger::LogLevel log_level)
{
    {
        std::lock_guard<std::mutex> lock(pimpl_->logger_lock_);
        current_log_level_ = log_level;
    }
    // Now log outside of the locked section.
    log(ALWAYS, "Log level set to: %s\n", LOGLEVEL_ABBREVS[static_cast<int>(log_level)]);
}

Logger::LogLevel Logger::getLogLevel() {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    return current_log_level_;
}

Logger::LogDestination Logger::getLogDestination() {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    return log_destination_;
}

void Logger::setLogDestination(Logger::LogDestination log_destination) {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    log_destination_ = log_destination;
}

void Logger::setCloseAfterWrite(bool close_after_write) {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    close_after_write_ = close_after_write;
}

bool Logger::getCloseAfterWrite() {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    return close_after_write_;
}

// --- Constructor / Destructor ---
Logger::Logger(const char* name)
    : current_log_level_(INFO),
    log_destination_(DEST_CONSOLE),
    close_after_write_(false),
    fatal_error_handler_(nullptr),
    pimpl_(std::make_unique<LoggerImpl>())
{
    // Initialize pimpl members.
#ifdef _WIN32
#define SAFE_STRNCPY(dest, destsize, src) strncpy_s(dest, destsize, src, _TRUNCATE)
#else
#define SAFE_STRNCPY(dest, destsize, src) do { \
    strncpy(dest, src, destsize-1); \
    dest[destsize-1] = '\0'; \
} while(0)
#endif

    SAFE_STRNCPY(pimpl_->log_path_and_filename_, sizeof(pimpl_->log_path_and_filename_), DEFAULT_LOG_FILENAME);

    // Zero-initialize the internal buffers.
    memset(log_message_buffer_, 0, MAX_LOGMSG_LENGTH);
    memset(unit_test_messages_, 0, MAX_LOGMSG_LENGTH);

    // Initialize the preformatted log level strings exactly once.
    static bool levels_initialized = false;
    if (!levels_initialized) {
        lock_guard<mutex> lock(registry_mutex_);
        levels_initialized = true;
    }
}

Logger::~Logger() = default;

// --- Core logging methods ---
bool Logger::log(const LogLevel log_level, const char* format, ...) {
    // Skip logging if the log level is below the threshold (except FORCE and ALWAYS).
    if (log_level != FORCE &&
        ((current_log_level_ == NONE && log_level != ALWAYS) || log_level < current_log_level_))
    {
        return true;
    }

    bool result = true;
    char dt_buf[40] = {0};
    char header[128] = {0};
    
    try {
        lock_guard<mutex> lock(pimpl_->logger_lock_);

        // Prepare a timestamp.
        Logger::getDateTimeStr(dt_buf, sizeof(dt_buf));

        const char* level_str = (log_level >= NONE && log_level <= FATAL) ? 
            LOGLEVEL_ABBREVS[static_cast<int>(log_level)] : "";

        // Format the full log header.
        snprintf(header, sizeof(header), "[%s %s] ", dt_buf, level_str);

        // Write the log header.
        switch (log_destination_) {
        case DEST_CONSOLE:
            result = logToConsole(header);
            break;
        case DEST_FILE:
            result = logToFile(header);
            break;
        case DEST_CONSOLE_AND_FILE:
            result = logToConsoleAndFile(header);
            break;
        default:
            break;
        }

        if (result) {
            va_list args;
            va_start(args, format);
            #ifdef _WIN32
                vsnprintf_s(log_message_buffer_, MAX_LOGMSG_LENGTH, _TRUNCATE, format, args);
            #else
                vsnprintf(log_message_buffer_, MAX_LOGMSG_LENGTH, format, args);
            #endif
            va_end(args);

            switch (log_destination_) {
            case DEST_CONSOLE:
                result = logToConsole(log_message_buffer_);
                break;
            case DEST_FILE:
                result = logToFile(log_message_buffer_);
                break;
            case DEST_CONSOLE_AND_FILE:
                result = logToConsoleAndFile(log_message_buffer_);
                break;
            default:
                break;
            }
        }
    }
    catch (const std::exception& e) {
        // Last resort: try to output to stderr
        fprintf(stderr, "Exception in Logger::log: %s\n", e.what());
        WriteEmergencyLog("Logger::log exception", __FILE__, __LINE__);
        return false;
    }
    catch (...) {
        // Unknown exception
        fprintf(stderr, "Unknown exception in Logger::log\n");
        WriteEmergencyLog("Logger::log unknown exception", __FILE__, __LINE__);
        return false;
    }
    return result;
}

bool Logger::log_no_datetime(const LogLevel log_level, const char* format, ...) {
    if (log_level != FORCE &&
        ((current_log_level_ == NONE && log_level != ALWAYS) || log_level < current_log_level_))
    {
        return true;
    }

    bool result = true;
    
    try {
        lock_guard<mutex> lock(pimpl_->logger_lock_);

        const char* level_str = (log_level >= NONE && log_level <= FATAL) ? 
            LOGLEVEL_ABBREVS[static_cast<int>(log_level)] : "";

        // Write the log level header (without a timestamp).
        switch (log_destination_) {
        case DEST_CONSOLE:
            result = logToConsole(level_str);
            break;
        case DEST_FILE:
            result = logToFile(level_str);
            break;
        case DEST_CONSOLE_AND_FILE:
            result = logToConsoleAndFile(level_str);
            break;
        default:
            return false;
        }

        if (result) {
            va_list args;
            va_start(args, format);
            #ifdef _WIN32
                vsnprintf_s(log_message_buffer_, MAX_LOGMSG_LENGTH, _TRUNCATE, format, args);
            #else
                vsnprintf(log_message_buffer_, MAX_LOGMSG_LENGTH, format, args);
            #endif
            va_end(args);

            switch (log_destination_) {
            case DEST_CONSOLE:
                result = logToConsole(log_message_buffer_);
                break;
            case DEST_FILE:
                result = logToFile(log_message_buffer_);
                break;
            case DEST_CONSOLE_AND_FILE:
                result = logToConsoleAndFile(log_message_buffer_);
                break;
            default:
                break;
            }
        }
    }
    catch (const std::exception& e) {
        // Last resort: try to output to stderr
        fprintf(stderr, "Exception in Logger::log_no_datetime: %s\n", e.what());
        WriteEmergencyLog("Logger::log_no_datetime exception", __FILE__, __LINE__);
        return false;
    }
    catch (...) {
        fprintf(stderr, "Unknown exception in Logger::log_no_datetime\n");
        WriteEmergencyLog("Logger::log_no_datetime unknown exception", __FILE__, __LINE__);
        return false;
    }
    return result;
}

bool Logger::logToConsole(const char* log_message_cstring) {
    try {
        if (!log_message_cstring) return false;
        
        fputs(log_message_cstring, stdout);
        fflush(stdout); // Ensure output is flushed
        return true;
    }
    catch (...) {
        // If we can't even log to console, we're in serious trouble
        // Try writing to stderr as absolute last resort
        fprintf(stderr, "EXCEPTION in Logger::logToConsole at %s:%d\n", __FILE__, __LINE__);
        WriteEmergencyLog("Logger::logToConsole failure", __FILE__, __LINE__);
        return false;
    }
}

bool Logger::logToFile(const char* log_message_cstring) {
    try {
        if (!log_message_cstring) {
            return false;
        }

        // Open file if not already open.
        if (!pimpl_->log_file_) {
            pimpl_->log_file_ = safe_fopen(pimpl_->log_path_and_filename_, "a");
            if (!pimpl_->log_file_) {
                // Try to create a backup emergency log if normal log file fails
                char emergency_path[1024];
                sprintf_s(emergency_path, "%s.emergency", pimpl_->log_path_and_filename_);
                pimpl_->log_file_ = safe_fopen(emergency_path, "a");
                if (!pimpl_->log_file_) {
                    WriteEmergencyLog("Failed to open log file", __FILE__, __LINE__);
                    return false;
                }
            }
        }

        // Write to file.
        if (fputs(log_message_cstring, pimpl_->log_file_) < 0) {
            // If write fails, close and try to reopen the file
            FILE* old_file = pimpl_->log_file_;
            pimpl_->log_file_ = nullptr;
            fclose(old_file);
            
            // Try to reopen
            pimpl_->log_file_ = safe_fopen(pimpl_->log_path_and_filename_, "a");
            if (!pimpl_->log_file_ || fputs(log_message_cstring, pimpl_->log_file_) < 0) {
                WriteEmergencyLog("Failed to write to log file", __FILE__, __LINE__);
                return false;
            }
        }

        fflush(pimpl_->log_file_);
        
        // Close file after write if requested
        if (close_after_write_) {
            fclose(pimpl_->log_file_);
            pimpl_->log_file_ = nullptr;
        }
        
        return true;
    }
    catch (...) {
        // Try to write to a separate emergency log file
        WriteEmergencyLog("Exception in Logger::logToFile", __FILE__, __LINE__);
        
        // Reset file pointer to ensure next attempt will reopen the file
        pimpl_->log_file_ = nullptr;
        return false;
    }
}

bool Logger::logToConsoleAndFile(const char* log_message_cstring) {
    return logToConsole(log_message_cstring) && (logToFile(log_message_cstring));
}

void Logger::getDateTimeStr(char* buf, int bufsize) {
    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);
    tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_time_t);
#else
    localtime_r(&now_time_t, &now_tm);
#endif
    auto now_ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    snprintf(buf, bufsize, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
        now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec,
        static_cast<int>(now_ms));
}

#if _UNITTEST
void Logger::logUnittest(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char temp_buffer[MAX_LOGMSG_LENGTH];
#ifdef _WIN32
    vsnprintf_s(temp_buffer, MAX_LOGMSG_LENGTH, _TRUNCATE, format, args);
#else
    vsnprintf(temp_buffer, MAX_LOGMSG_LENGTH, format, args);
#endif
    va_end(args);

    size_t current_len = std::strlen(unit_test_messages_);
    size_t msg_len = std::strlen(temp_buffer);
    if (current_len + msg_len < MAX_LOGMSG_LENGTH - 1) {
#ifdef _WIN32
        strcat_s(unit_test_messages_, MAX_LOGMSG_LENGTH, temp_buffer);
#else
        strncat(unit_test_messages_, temp_buffer, MAX_LOGMSG_LENGTH - current_len - 1);
#endif
    }
}

const char* Logger::getUnitTestLog() {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    const char* ret = unit_test_messages_;
    unit_test_messages_[0] = '\0';
    return ret;
}
#endif

void Logger::setLogFile(const char* log_path_and_filename_param) {
    lock_guard<mutex> lock(pimpl_->logger_lock_);
    const char* new_path = (log_path_and_filename_param == nullptr || *log_path_and_filename_param == '\0') ?
        DEFAULT_LOG_FILENAME : log_path_and_filename_param;

    if (pimpl_->log_file_) {
        fclose(pimpl_->log_file_);
        pimpl_->log_file_ = nullptr;
    }

    SAFE_STRNCPY(pimpl_->log_path_and_filename_, sizeof(pimpl_->log_path_and_filename_), new_path);

    pimpl_->log_file_ = safe_fopen(pimpl_->log_path_and_filename_, "a");
    if (!pimpl_->log_file_) {
        this->recoverable_error("Logger::setLogFile() Failed to open log file: %s\n", pimpl_->log_path_and_filename_);
    }
}

#ifdef _WIN32
void Logger::setLogFileW(const std::wstring& log_path_and_filename_param) {
    char narrow_path[1024];
    size_t converted;
    wcstombs_s(&converted, narrow_path, sizeof(narrow_path),
        log_path_and_filename_param.c_str(), _TRUNCATE);
    setLogFile(narrow_path);
}
#endif

int Logger::writeToFile(const char* filename, bool append, const char* format, ...) {
    char buffer[MAX_LOGMSG_LENGTH];
    va_list args;
    va_start(args, format);
#ifdef _WIN32
    int num_chars = vsnprintf_s(buffer, MAX_LOGMSG_LENGTH, _TRUNCATE, format, args);
#else
    int num_chars = vsnprintf(buffer, MAX_LOGMSG_LENGTH, format, args);
#endif
    va_end(args);
    if (num_chars < 1) {
        return num_chars;
    }

    FILE* output_file = safe_fopen(filename, append ? "a" : "w");
    if (!output_file) {
        return -1;
    }

    int num_chars_written = 0;
    for (; num_chars_written < num_chars && buffer[num_chars_written] != '\0'; ++num_chars_written) {
        if (fputc(buffer[num_chars_written], output_file) == EOF) {
            fclose(output_file);
            return num_chars_written;
        }
    }
    fclose(output_file);
    return num_chars_written;
}

void Logger::fatal(const char* format, ...) {
    try {
        va_list args;
        va_start(args, format);
        char formatted_message[MAX_LOGMSG_LENGTH] = {0};
        #ifdef _WIN32
        vsnprintf_s(formatted_message, MAX_LOGMSG_LENGTH, _TRUNCATE, format, args);
        #else
        vsnprintf(formatted_message, MAX_LOGMSG_LENGTH, format, args);
        #endif
        va_end(args);

        // Log the fatal message first
        log(FATAL, "%s", formatted_message);
        
        // Also write to the emergency log file (unconditionally)
        char emergency_message[MAX_LOGMSG_LENGTH + 100];
        char dt_buf[40];
        Logger::getDateTimeStr(dt_buf, sizeof(dt_buf));
        sprintf_s(emergency_message, "[%s FATAL] %s\r\n", dt_buf, formatted_message);
        
        HANDLE hFile = CreateFileA("syslogagent_emergency.log", 
                                 FILE_APPEND_DATA, 
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten;
            WriteFile(hFile, emergency_message, (DWORD)strlen(emergency_message), &bytesWritten, NULL);
            CloseHandle(hFile);
        }

        // Call the fatal error handler if available
        FATAL_ERROR_HANDLER handler = fatal_error_handler_;
        if (handler) {
            // We need to use a separate C-style function to handle SEH for the handler
            SafeCallFatalHandler(handler, formatted_message);
        }
    }
    catch (...) {
        // Ultimate fallback for catastrophic failures
        WriteEmergencyLog("Unhandled exception in Logger::fatal", __FILE__, __LINE__);
        
        // Try stderr as an absolute last resort
        fprintf(stderr, "Critical exception in Logger::fatal at %s:%d\n", __FILE__, __LINE__);
    }
}

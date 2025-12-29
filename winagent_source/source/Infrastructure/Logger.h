#pragma once

#ifdef _WIN32
#include "pch.h"
#ifdef INFRASTRUCTURE_STATIC
#define INFRASTRUCTURE_API
#else
#ifdef INFRASTRUCTURE_EXPORTS
#define INFRASTRUCTURE_API __declspec(dllexport)
#else
#define INFRASTRUCTURE_API __declspec(dllimport)
#endif
#endif
#else
#define INFRASTRUCTURE_API
#endif

#include <mutex>
#include <string>
#include <cstdarg>
#include <vector>
#include <fstream>
#include <map>
#include <memory>
#include <cstring>
#include <cstdio>
#include "framework.h"
#ifdef _WIN32
#include <share.h>
#endif

#define LOG_HERE(logger) logger->debug3("LOG_HERE> %s:%s (#%d)\n", __FILE__, __func__, __LINE__)

// Macro for printf format checking (MSVC doesn't support __attribute__ so we disable it)
#ifdef _MSC_VER
#define FORMAT_PRINTF(a, b)
#else
#define FORMAT_PRINTF(a, b) __attribute__((format(printf, a, b)))
#endif

// Add safe string copy macro
#ifdef _WIN32
#define SAFE_STRNCPY(dest, destsize, src) strncpy_s(dest, destsize, src, _TRUNCATE)
#else
#define SAFE_STRNCPY(dest, destsize, src) do { \
    strncpy(dest, src, destsize-1); \
    dest[destsize-1] = '\0'; \
} while(0)
#endif

// Platform-specific file open function
inline FILE* safe_fopen(const char* filename, const char* mode) {
#ifdef _WIN32
    return _fsopen(filename, mode, _SH_DENYWR); // allow other processes to read
#else
    return fopen(filename, mode);
#endif
}

// Logger class with fixed enums and methods.
class INFRASTRUCTURE_API Logger {
public:

    static constexpr const char* LAST_RESORT_LOGGER_NAME = "last_resort_logger";
    // Only used during setup û it is acceptable to use heap here.
    static constexpr const char* DEFAULT_LOG_FILENAME = "syslogagent.log";
    static constexpr size_t MAX_LOGMSG_LENGTH = 2048;
    static constexpr size_t MAX_LOGLEVEL_LENGTH = 8;

    // Comparator for C-string keys.
    struct CStringCompare {
        bool operator()(const char* a, const char* b) const {
            return std::strcmp(a, b) < 0;
        }
    };

    // Forward declaration of the private implementation.
    class LoggerImpl;

    // Static members used for logger management.
    static std::mutex registry_mutex_;  // Used during setup only.
    static std::map<const char*, Logger*, CStringCompare> logger_registry_;
    static Logger* default_logger_;
    static int is_unittest_running_;

    typedef void (*FATAL_ERROR_HANDLER)(const char* fatal_error_message);

    // Updated log levels (exactly 12 levels, matching the abbreviations array)
    enum LogLevel {
        NONE = 0,              // No logging
        DEBUG3 = 1,            // Most verbose debugging level -- intended for only in-house dev & debugging
        DEBUG2 = 2,            // Mid-level detailed debugging -- 
                               // detailed information inside module functioning
                               // can be set by customer at support instruction
        DEBUG = 3,             // Standard debugging information --
                               // generally indications of major module execution
                               // along with in/out data
        VERBOSE = 4,           // corresponds to "VERB" - Detailed operational logging for all normal processing
        INFO = 5,              // Normal operational information about normal processing
        WARN = 6,              // Potential issues that may need attention eventually,
                               // but not immediately
        
        // Error severity levels:
        // 
        // RECOVERABLE_ERROR - Use when:
        // - The program can continue functioning despite this error
        // - The operation failed, but overall system functionality is not compromised
        // - No manual intervention is needed; the system will handle the failure gracefully
        // - Example: Skipping a non-critical event log subscription
        RECOVERABLE_ERROR = 7, // corresponds to "RERR"
        
        // CRITICAL - Use when:
        // - Manual attention is required to resolve the issue
        // - The operation can be resumed once the error is corrected without restarting
        // - System functionality can be completely stopped, as long as manual intervention will resume operations
        // - Example: A retryable network issue that requires configuration/connectivity fix
        CRITICAL = 8,          // corresponds to "CRIT"
        
        ALWAYS = 9,            // corresponds to "ALWY" - Always logged regardless of level, except NOLOG setting
        FORCE = 10,            // corresponds to "FORC" - Forces output even in constrained situations
        
        // FATAL - Use when:
        // - No way for the program to recover without being restarted
        // - The error will be reported to the OS (Windows Service Control Manager)
        // - Manual correction and service restart are required
        // - Core initialization or critical subsystem failures
        // - Example: Failed network initialization preventing proper system operation
        FATAL = 11             // Signals unrecoverable condition, triggers service shutdown
    };

    // Updated log destination values matching the implementation.
    enum LogDestination {
        DEST_NONE = 0,
        DEST_CONSOLE = 1,
        DEST_FILE = 2,
        DEST_CONSOLE_AND_FILE = 3
    };

    // Preformatted log level abbreviations array
    static constexpr const char* LOGLEVEL_ABBREVS[12] = {
        "NONE", "DBG3", "DBG2", "DBUG", "VERB", "INFO",
        "WARN", "RERR", "CRIT", "ALWY", "FORC", "FATL"
    };

    // Constructor and destructor
    explicit Logger(const char* name = "DefaultLogger");
    ~Logger();

    // Static factory and management methods (configuration/setup only).
    static void setDefaultLogger(std::shared_ptr<Logger> logger);
    static void setLogger(std::shared_ptr<Logger> logger, const std::vector<std::string>& names);

    // Fast logger lookup û used in the hot logging path.
    // Never returns null û will return dummy logger if no specific logger exists
    static Logger* getLoggerByKey(const char* key);

    // Setup methods.
    void setFatalErrorHandler(FATAL_ERROR_HANDLER fatal_error_handler);
    void setLogLevel(const LogLevel log_level);
    void setLogDestination(LogDestination log_destination);
    void setLogFile(const char* log_path_and_filename);
    void setCloseAfterWrite(bool close_after_write);
#ifdef _WIN32
    void setLogFileW(const std::wstring& log_path_and_filename);
#endif

    // Getter methods.
    LogLevel getLogLevel();
    LogDestination getLogDestination();
    bool getLogEventsToFile();
    bool getCloseAfterWrite();

    // Core logging methods.
    bool log(const LogLevel log_level, const char* format, ...) FORMAT_PRINTF(3, 4);
    bool log_no_datetime(const LogLevel log_level, const char* format, ...) FORMAT_PRINTF(3, 4);
    void fatal(const char* format, ...) FORMAT_PRINTF(2, 3);

    // Convenience logging methods.
    template<typename... Args> bool debug3(const char* format, Args... args) {
        return log(DEBUG3, format, args...);
    }
    template<typename... Args> bool debug2(const char* format, Args... args) {
        return log(DEBUG2, format, args...);
    }
    template<typename... Args> bool debug(const char* format, Args... args) {
        return log(DEBUG, format, args...);
    }
    template<typename... Args> bool verbose(const char* format, Args... args) {
        return log(VERBOSE, format, args...);
    }
    template<typename... Args> bool info(const char* format, Args... args) {
        return log(INFO, format, args...);
    }
    template<typename... Args> bool warning(const char* format, Args... args) {
        return log(WARN, format, args...);
    }
    template<typename... Args> bool recoverable_error(const char* format, Args... args) {
        return log(RECOVERABLE_ERROR, format, args...);
    }
    template<typename... Args> bool critical(const char* format, Args... args) {
        return log(CRITICAL, format, args...);
    }
    template<typename... Args> bool always(const char* format, Args... args) {
        return log(ALWAYS, format, args...);
    }
    template<typename... Args> bool force(const char* format, Args... args) {
        return log(FORCE, format, args...);
    }

    // Static utility methods.
    static void getDateTimeStr(char* buf, int bufsize);
    static int writeToFile(const char* filename, bool append, const char* format, ...) FORMAT_PRINTF(3, 4);

#if _UNITTEST
    // Unit-test logging (rarely called). Uses a fixed buffer.
    void logUnittest(const char* format, ...);
    // Returns a pointer to the fixed internal unit-test log buffer.
    const char* getUnitTestLog();
#else
    inline void logUnittest(const char* format, ...) {}
    inline const char* getUnitTestLog() { return ""; }
#endif

#if RELEASE
    void breakPoint() const {}
#else
    void breakPoint() {
#ifdef _WIN32
        __debugbreak();
#else
        __builtin_trap();
#endif
    }
#endif

private:
    // Disable copying.
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Private implementation pointer (pimpl idiom)
    std::unique_ptr<LoggerImpl> pimpl_;

    // Member variables that need to be directly accessible.
    LogLevel current_log_level_;
    LogDestination log_destination_;
    bool close_after_write_;
    FATAL_ERROR_HANDLER fatal_error_handler_;
    char log_message_buffer_[MAX_LOGMSG_LENGTH];  // Used for formatting each log message.
    char unit_test_messages_[MAX_LOGMSG_LENGTH];  // Used in unit test logging.

    // Static dummy logger that will be returned if no specific logger exists.
    static Logger dummyLogger_;

    // Private helper methods.
    bool logToConsole(const char* log_message_cstring);
    bool logToFile(const char* log_message_cstring);
    bool logToConsoleAndFile(const char* log_message_cstring);
};

// Definition of LoggerImpl.
class Logger::LoggerImpl {
public:
    char log_path_and_filename_[1024];  // Fixed-size buffer for path.
    mutable std::mutex logger_lock_;
    FILE* log_file_;  // Standard C file handle.

    LoggerImpl() : log_file_(nullptr) {
        SAFE_STRNCPY(log_path_and_filename_, sizeof(log_path_and_filename_), "syslogagent.log");
    }

    ~LoggerImpl() {
        if (log_file_) {
            fclose(log_file_);
        }
    }
};

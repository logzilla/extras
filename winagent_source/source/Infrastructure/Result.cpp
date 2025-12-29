/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "pch.h"
#include "Logger.h"
#include "Result.h"

Result::Result() {
    status_ = ERROR_SUCCESS;
}

Result::Result(const Result& other) {
    status_ = other.status_;
    message_str_ = other.message_str_;
}

Result::~Result() {
    // Default destructor implementation
}

Result::Result(DWORD status) {
    setResult(status, "", "");
}

Result::Result(const char* message) {
    status_ = ERROR_INVALID_FUNCTION;
    this->message_str_ = message;
}

Result::Result(DWORD status, const char* name, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char message[1024];
    _vsnprintf_s(message, 1024, _TRUNCATE, format, args);
    setResult(status, name, message);
    va_end(args);
}

Result Result::ResultLog(DWORD status, Logger::LogLevel log_level, 
    const char* name, const char* format, ...) {
    auto logger = LOG_THIS;
    va_list args;
    va_start(args, format);
    char message[1024];
    _vsnprintf_s(message, 1024, _TRUNCATE, format, args);
    Result retval;
    retval.setResult(status, name, message);
    logger->log(log_level, "%s\n", message);
    va_end(args);
    return retval;
}

void Result::setResult(DWORD status, const char* from, const char* message) {
    this->status_ = status;
    if (status != 0 && strlen(from) > 0) {
        char buffer[1024];
        sprintf_s(buffer, "%s returned %u", from, status);
        this->message_str_ = buffer;
        if (*message) {
            this->message_str_.append("> ");
            this->message_str_.append(message);
        }
        LPSTR status_message;
        auto size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM 
            | FORMAT_MESSAGE_ARGUMENT_ARRAY,
            nullptr, status, LANG_NEUTRAL, (LPSTR)&status_message, 0, nullptr);
        if (size == 0) return;
        while (size > 0 && iscntrl(status_message[size - 1])) size--;
        this->message_str_.append(": ");
        this->message_str_.append(status_message, size);
        LocalFree(status_message);
    }
}

bool Result::isSuccess() const { return status_ == ERROR_SUCCESS; }

DWORD Result::statusCode() const { return this->status_; }

const char* Result::what() const { return message_str_.c_str(); }

void Result::log() const { 
    auto logger = LOG_THIS;
    logger->log(isSuccess() ? Logger::INFO : Logger::CRITICAL, "%s\n", what()); 
}

void Result::logLastError(const char* from, const char* message) { 
    auto logger = LOG_THIS;
    Result(GetLastError(), from, message).log(); 
}

void Result::throwLastError(const char* from, const char* message) { 
    throw Result(GetLastError(), from, message); 
}

void Result::anchor() {}

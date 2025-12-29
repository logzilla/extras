/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#pragma once

#include <string>
#ifndef INFRASTRUCTURE_API 
#define INFRASTRUCTURE_API 
#endif
#include "Logger.h"

    class INFRASTRUCTURE_API Result : public std::exception {
    public:
        Result();
        Result(const char* message);
        Result(DWORD status);
        Result(DWORD status, const char* from, const char* format, ...);
        Result(const Result& other);  
        virtual ~Result() override;
        static Result ResultLog(DWORD status, Logger::LogLevel log_level, 
            const char* name, const char* format, ...);
        const char* what() const override;
        bool isSuccess() const;
        DWORD statusCode() const;
        void log() const;
        static void logLastError(const char* from, const char* message);
        static void throwLastError(const char* from, const char* message);
        
        // Add anchor method to force vtable emission
        virtual void anchor();

    private:
        void setResult(DWORD status, const char* name, const char* message);
        DWORD status_;
        std::string message_str_;
    };

/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <Windows.h>

namespace Syslog_agent {

    class RecordNumber {
    public:
        explicit RecordNumber(DWORD value);
        RecordNumber(RecordNumber& other);
        bool is_greater(RecordNumber& other) const;
        void increment();
        operator DWORD() const;
        DWORD operator=(DWORD new_value);

    private:
        DWORD value;
    };
}

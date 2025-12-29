/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#pragma once

namespace Syslog_agent {

    class Options {
    public:
        Options(int count, const wchar_t* const* values);
        bool has(const wchar_t* option) const;
        const wchar_t* getArgument(const wchar_t* option) const;
    private:
        int count;
        const wchar_t* const* values;  // Array of pointers to const wchar_t
    };
}

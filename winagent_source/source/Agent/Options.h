/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

namespace Syslog_agent {

    class Options {
    public:
        Options(int count, wchar_t** values);
        bool has(wchar_t* option) const;
        wchar_t* getArgument(wchar_t* option) const;
    private:
        int count;
        wchar_t** values;
    };
}

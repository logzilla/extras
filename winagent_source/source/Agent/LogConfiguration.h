/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include "RecordNumber.h"
#include "Registry.h"

namespace Syslog_agent {

    class LogConfiguration {
    public:
        LogConfiguration() {};
        std::wstring channel_;
        std::wstring name_;
        std::string nname_;
        std::wstring bookmark_;
        void loadFromRegistry(Registry& parent);
        void saveToRegistry(Registry& parent) const;
    };
}

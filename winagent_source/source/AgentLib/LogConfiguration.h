#pragma once

#include "Registry.h"
#include <string>

namespace Syslog_agent {

    class LogConfiguration {
    public:
        LogConfiguration() = default;
        ~LogConfiguration() = default;

        // Properties
        std::wstring channel_;
        std::wstring name_;        // Stores the display name for the log (usually same as channel)
        std::string nname_;        // Narrow string version of the name
        std::wstring bookmark_;
        bool enabled_ = false;
        bool only_while_running_ = false;

        // Methods
        void loadFromRegistry(Registry& registry);
        void saveToRegistry(Registry& registry) const;
    };
}

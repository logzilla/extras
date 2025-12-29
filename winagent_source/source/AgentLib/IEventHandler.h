#pragma once

#include "../Infrastructure/Result.h"

namespace Syslog_agent {

    // Forward declarations
    class EventLogEvent;

    // Interface for event handlers
    class IEventHandler {
    public:
        virtual ~IEventHandler() = default;

        // Handle a Windows event
        virtual Result handleEvent(const wchar_t* subscription_name, EventLogEvent& event) = 0;
    };

} // namespace Syslog_agent

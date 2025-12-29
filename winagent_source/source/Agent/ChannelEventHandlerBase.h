/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include "EventLogEvent.h"
#include "Result.h"

namespace Syslog_agent {
	class ChannelEventHandlerBase {
	public:
		virtual Result handleEvent(const wchar_t* subscription_name, EventLogEvent& event) = 0;
	};
}
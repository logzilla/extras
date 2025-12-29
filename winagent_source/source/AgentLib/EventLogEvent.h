/*
SyslogAgent: a syslog agent for Windows
Copyright  2021 Logzilla Corp.
*/

#pragma once

// #include "stdafx.h" // Removed, pch.h is used
#include <winevt.h>
#include "../Infrastructure/BitmappedObjectPool.h"
#include "EventXmlData.h"
#include "include/AgentLibExports.h"

namespace Syslog_agent {
    using namespace std;

    class AGENTLIB_API EventLogEvent {
    public:
        EventLogEvent(EVT_HANDLE windows_event_handle);
        ~EventLogEvent();
        virtual void renderEvent();
        bool isRendered() const { return xml_buffer_ != nullptr; }
        char* getEventXml() const { return xml_buffer_; }
        char* getEventText() const { return text_buffer_; }
        virtual DWORD getEventId() const;

        // Make event_xml_data_ public so it can be accessed directly
        EventXmlData event_xml_data_;

    protected:
        virtual void renderXml();
        virtual void renderText(const char* publisher_name);

        // these two stored as utf-8
        char* xml_buffer_;
        char* text_buffer_;
        EVT_HANDLE windows_event_handle_;
    };
}
#pragma once

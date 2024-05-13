/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <memory>
#include "pugixml.hpp"
#include <Windows.h>
#include <winevt.h>
#include "BitmappedObjectPool.h"

using namespace std;

namespace Syslog_agent {
	class EventLogEvent {
	public:
		EventLogEvent(EVT_HANDLE windows_event_handle);
		~EventLogEvent();
		void renderEvent();
		pugi::xml_document& getXmlDoc() { return xml_doc_; }
		bool isRendered() const { return xml_buffer_ != nullptr; }
		char* getEventXml() const { return xml_buffer_; }
		char* getEventText() const { return text_buffer_; }

	private:
		void renderXml();
		void renderText(const char* publisher_name);

		// these two stored as utf-8
		char* xml_buffer_;
		char* text_buffer_;
		EVT_HANDLE windows_event_handle_;
		pugi::xml_document xml_doc_;
	};
}
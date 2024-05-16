/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include <iomanip>
#include <locale>
#include "pugixml.hpp"
#include <sstream>
#include "EventHandlerMessageQueuer.h"
#include "Globals.h"
#include "Logger.h"
#include "OStreamBuf.h"
#include "SyslogSender.h"
#include "Util.h"

using namespace std;

namespace Syslog_agent {

	EventHandlerMessageQueuer::EventHandlerMessageQueuer(
		Configuration& configuration,
		shared_ptr<MessageQueue> primary_message_queue,
		shared_ptr<MessageQueue> secondary_message_queue,
		const wchar_t* log_name)
		: configuration_(configuration),
		primary_message_queue_(primary_message_queue),
		secondary_message_queue_(secondary_message_queue)
	{
		DWORD chars_written;
		size_t length = wcslen(log_name);
		if (length > INT_MAX) {
			// Handle the error, perhaps by truncating or by throwing an exception
			throw std::runtime_error("String too long");
		}
		chars_written = (DWORD)WideCharToMultiByte(CP_UTF8, 0, log_name,
			static_cast<int>(length), log_name_utf8_, 999, NULL, NULL);
		log_name_utf8_[chars_written] = 0;
		if (configuration_.suffix_.length() > 0) {
			if (configuration_.suffix_.length() > SharedConstants::MAX_SUFFIX_LENGTH) {
				strcpy_s(suffix_utf8_, SharedConstants::MAX_SUFFIX_LENGTH, 
					"\"error_suffix\": \"too long\"");
			}
			else {
				size_t length = configuration_.suffix_.length();
				if (length > INT_MAX) {
					// Handle the error, perhaps by truncating the string or throwing an exception
					Logger::recoverable_error("Suffix length exceeds the maximum allowable"
						" limit for WideCharToMultiByte.");
					// Optionally, truncate length or take other corrective measures
					length = INT_MAX;
				}

				// Now safely cast length to int since we've ensured it doesn't exceed INT_MAX
				chars_written = WideCharToMultiByte(CP_UTF8, 0,
					configuration_.suffix_.c_str(),
					static_cast<int>(length),
					suffix_utf8_, 1000, NULL, NULL);
				suffix_utf8_[chars_written] = 0;
			}
		}
		else {
			suffix_utf8_[0] = 0;
		}
	}

	Result EventHandlerMessageQueuer::handleEvent(const wchar_t* subscription_name, 
		EventLogEvent& event) {
		event.renderEvent();
		char* json_buffer = Globals::instance()->getMessageBuffer("json_buffer");
		int primary_logformat = configuration_.getPrimaryLogformat();
		if (generateLogMessage(event, primary_logformat, json_buffer, 
			Globals::MESSAGE_BUFFER_SIZE)) {
			while (primary_message_queue_->isFull()) {
				primary_message_queue_->removeFront();
			}
			primary_message_queue_->enqueue(json_buffer, (const int)strlen(json_buffer));
			if (secondary_message_queue_ != nullptr) {
				bool have_secondary_message = true;
				int secondary_logformat = configuration_.getSecondaryLogformat();
				if (primary_logformat != secondary_logformat) {
					have_secondary_message = generateLogMessage(event, 
						secondary_logformat, json_buffer, Globals::MESSAGE_BUFFER_SIZE);
				}
				if (have_secondary_message) {
					while (secondary_message_queue_->isFull()) {
						secondary_message_queue_->removeFront();
					}
					secondary_message_queue_->enqueue(json_buffer, 
						(const int)strlen(json_buffer));
				}
				else {
					Logger::warn("EventHandlerMessageQueuer::handleEvent()>"
						" secondary generateLogMessage() failed");
				}
            }
		}
		else {
            Logger::warn("EventHandlerMessageQueuer::handleEvent()> primary"
				" generateLogMessage() failed");
        }
		Globals::instance()->releaseMessageBuffer("json_buffer", json_buffer);
		return Result((DWORD)ERROR_SUCCESS);
	}

	unsigned char EventHandlerMessageQueuer::unixSeverityFromWindowsSeverity(
		char windows_severity_num) {
		unsigned char result = 0;
		switch (windows_severity_num) {
		case '0':
			// "LogAlways"
			result = SharedConstants::Severities::NOTICE;
			break;
		case '1':
			// "Critical"
			result = SharedConstants::Severities::CRITICAL;
			break;
		case '2':
			// "Error"
			result = SharedConstants::Severities::ERR;
			break;
		case '3':
			// "Warning":
			result = SharedConstants::Severities::WARNING;
			break;
		case '4':
			// "Informational"
			result = SharedConstants::Severities::INFORMATIONAL;
			break;
		case '5':
			// "Verbose"
			result = SharedConstants::Severities::DEBUG;
			break;
		}
		return result;
	}


	bool EventHandlerMessageQueuer::generateLogMessage(EventLogEvent& event, 
		const int logformat, char* json_buffer, size_t buflen) {

		// figure out some message details
		auto event_id_str 
			= event.getXmlDoc().child("Event").child("System").child("EventID").child_value();
		DWORD event_id;
		sscanf_s(event_id_str, "%u", &event_id);
		if (configuration_.include_vs_ignore_eventids_) {
			if (configuration_.event_id_filter_.find(event_id) 
				== configuration_.event_id_filter_.end()) {
				return false;
			}
		}
		else
		{
			if (configuration_.event_id_filter_.find(event_id) 
				!= configuration_.event_id_filter_.end()) {
				return false;
			}
		}
		char severity;
		if (configuration_.severity_ == SharedConstants::Severities::DYNAMIC) {
			auto level 
				= event.getXmlDoc().child("Event").child("System").child("Level").child_value();
			severity = unixSeverityFromWindowsSeverity(level[0]);
		}
		else {
			severity = configuration_.severity_;
		}

		auto time_field 
			= event.getXmlDoc().child("Event").child("System").child("TimeCreated").attribute("SystemTime").value();

		// Parse the date-time string
		// this is more verbose than absolutely necessary because we are 
		// following a strict no-heap policy across the board to prevent
		// heap fragmentation over operation times potentially spanning
		// years
		char timestamp[30];
		char microsec[12]; // Buffer for microseconds string
		timestamp[0] = '\0'; // Initialize to empty string
		int year, month, day, hour, minute, second;
		int microseconds = 0;

		int num_scanned = sscanf_s(time_field, "%d-%d-%dT%d:%d:%d.%dZ", 
			&year, &month, &day, &hour, &minute, &second, &microseconds);
		if (num_scanned >= 6) {
			struct tm tm = {};
			tm.tm_year = year - 1900;
			tm.tm_mon = month - 1;
			tm.tm_mday = day;
			tm.tm_hour = hour;
			tm.tm_min = minute;
			tm.tm_sec = second;
			time_t time = mktime(&tm);
			time -= configuration_.utc_offset_minutes_ * 60;

			snprintf(timestamp, sizeof(timestamp), "%ld", static_cast<long>(time));

			sprintf_s(microsec, sizeof(microsec), "%d", microseconds);  // Format microseconds to string

			// Pad with zeros on right if necessary
			for (int i = static_cast<int>(strlen(microsec)); i < 6; i++) {
				microsec[i] = '0';
			}
			microsec[6] = '\0'; 
		}

		auto provider
			= event.getXmlDoc().child("Event").child("System").child("Provider").attribute("Name").value();
		// generate json
		// we're going to copy the message text into the json so we need
		// to escape certain characters for valid json
		auto escaped_buf = Globals::instance()->getMessageBuffer("escaped_buf");
		Util::jsonEscape(event.getEventText(), escaped_buf, Globals::MESSAGE_BUFFER_SIZE);
		if (strlen(escaped_buf) == 0) {
			const char* no_msg = "(no event message given)";
			size_t no_msg_len = strlen(no_msg) + 1;
			strncpy_s(escaped_buf, Globals::MESSAGE_BUFFER_SIZE, no_msg, no_msg_len);
		}
		OStreamBuf<char> ostream_buffer(json_buffer, buflen);
		ostream json_output(&ostream_buffer);
		json_output.fill('0');
		json_output << "{";
		if (configuration_.host_name_ != "") {
			json_output << "\"host\": \"" << configuration_.host_name_ << "\", ";
		}
		json_output
			<< "\"program\": \"" << provider << "\""
			<< ", \"severity\": " << ((char)(severity + '0'))
			<< ", \"facility\": " << configuration_.facility_;
		switch (logformat) {
		case SharedConstants::LOGFORMAT_HTTPPORT:
			if (timestamp[0] != 0) {
				// new win app expects int (long) microsec
				json_output << ", \"first_occurrence\": " << timestamp << microsec;
			}
			json_output << ", \"message\": \"" << escaped_buf << "\"";
			break;

		case SharedConstants::LOGFORMAT_JSONPORT:
			json_output << ", \"message\": \"JSON log event\""
				<< ", \"_source_type\": \"WindowsAgent\"";
			break;

		default:
			Logger::fatal("EventHandlerMessageQueuer::generateLogMessage()> Unknown"
				" logformat: %d", logformat);
		}
		json_output << ", \"extra_fields\": { "
			<< " \"_source_tag\": \"windows_agent\""
			<< ", \"log_type\": \"eventlog\""
			<< ", \"event_id\": \"" << event_id_str << "\""
			<< ", \"event_log\": \"" << log_name_utf8_ << "\"";
		if (logformat == SharedConstants::LOGFORMAT_JSONPORT) {
			json_output
				<< ", \"program\": \"" << provider << "\""
				<< ", \"severity\": " << ((char)(severity + '0'))
				<< ", \"facility\": " << configuration_.facility_
				<< ", \"_source_type\": \"WindowsAgent\"";
			if (timestamp[0] != 0) {
				// old win app expects decimal sec
				json_output << ", \"ts\": " << timestamp << "." << microsec;
			}
			if (configuration_.host_name_ != "") {
				json_output << ", \"host\": \"" << configuration_.host_name_ << "\" ";
			}
			json_output << ", \"message\": \"" << escaped_buf << "\" ";
		}
		pugi::xml_node event_data = event.getXmlDoc().child("Event").child("EventData");
		for (pugi::xml_node data_item = event_data.first_child(); data_item;
			data_item = data_item.next_sibling()) {
			auto data_name = data_item.attribute("Name").value();
			if (data_name[0] != 0) {
				auto value = data_item.child_value();
				// just in case there's any chars in value to escape:
				Util::jsonEscape((char*)value, escaped_buf, Globals::MESSAGE_BUFFER_SIZE - 1);
				json_output << ", \"" << data_name << "\": \"" << escaped_buf << "\"";
			}
		}
		if (logformat == SharedConstants::LOGFORMAT_JSONPORT) {
			json_output << " }";
		}	
		Globals::instance()->releaseMessageBuffer("escaped_buf", escaped_buf);
		if (suffix_utf8_[0] != 0) {
			json_output << "," << suffix_utf8_;
		}
		if (logformat != SharedConstants::LOGFORMAT_JSONPORT) {
			json_output << " }";
		}
		json_output << " }" << (char)10 << (char)0;

		return true;
	}
}

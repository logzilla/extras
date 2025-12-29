#include "pch.h"

/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "EventLogEvent.h"
#include "Globals.h"
#include "../Infrastructure/Logger.h"
#include "../Infrastructure/Util.h"

#pragma comment(lib, "wevtapi.lib")

namespace Syslog_agent {
    EventLogEvent::EventLogEvent(EVT_HANDLE windows_event_handle)
        : windows_event_handle_(windows_event_handle), xml_buffer_(nullptr), text_buffer_(nullptr) {
    }

    EventLogEvent::~EventLogEvent() {
        if (xml_buffer_)
            Globals::instance()->releaseMessageBuffer(xml_buffer_);
        if (text_buffer_)
            Globals::instance()->releaseMessageBuffer(text_buffer_);
    }

    void EventLogEvent::renderXml() {
        auto logger = LOG_THIS;
        DWORD buffer_size_needed;
        DWORD count;
        if (xml_buffer_ != nullptr)
            return;
        xml_buffer_ = Globals::instance()->getMessageBuffer("xml_buffer_");
        auto xml_buffer_w = reinterpret_cast<wchar_t*>(Globals::instance()->getMessageBuffer("xml_buffer_w_renderXml"));

        BOOL succeeded = FALSE;
        DWORD err = 0;

        __try {
            succeeded = EvtRender(
                nullptr,
                windows_event_handle_,
                EvtRenderEventXml,
                static_cast<DWORD>(Globals::MESSAGE_BUFFER_SIZE / sizeof(wchar_t)),
                static_cast<PVOID>(xml_buffer_w),
                &buffer_size_needed,
                &count);
            if (!succeeded) {
                err = GetLastError();
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            err = GetExceptionCode();
            logger->critical("EventLogEvent::renderXml()> SEH Exception 0x%08X during EvtRender for event handle %p. Forcing failure.\n", err, windows_event_handle_);
            succeeded = FALSE; 
            buffer_size_needed = 0;
        }

        if (!succeeded) {
            logger->recoverable_error("EventLogEvent::renderXml()> EvtRender failed with error %lu (0x%08X) for event handle %p\n", err, err, windows_event_handle_);
            Globals::instance()->releaseMessageBuffer(reinterpret_cast<char*>(xml_buffer_w));
            if (xml_buffer_) xml_buffer_[0] = '\0';
            return;
        }
        if (buffer_size_needed < (Globals::MESSAGE_BUFFER_SIZE / sizeof(wchar_t))) {
            xml_buffer_w[buffer_size_needed] = 0;
            int utf8_size = WideCharToMultiByte(CP_UTF8, 0, xml_buffer_w,
                static_cast<int>(buffer_size_needed), nullptr, 0, nullptr, nullptr);
            if (utf8_size >= Globals::MESSAGE_BUFFER_SIZE - 1)
                utf8_size = Globals::MESSAGE_BUFFER_SIZE - 1;
            WideCharToMultiByte(CP_UTF8, 0, xml_buffer_w, static_cast<int>(buffer_size_needed),
                xml_buffer_, utf8_size, nullptr, nullptr);
            xml_buffer_[utf8_size] = '\0';  // Ensure null termination
        }
        else {
            xml_buffer_[0] = 0;
        }
        Globals::instance()->releaseMessageBuffer(reinterpret_cast<char*>(xml_buffer_w));
    }

    void EventLogEvent::renderEvent() {
        if (isRendered())
            return;
        renderXml();
        if (xml_buffer_ && xml_buffer_[0] != '\0') {
            event_xml_data_.parse(xml_buffer_);
        }
        else {
            auto logger = LOG_THIS;
            if (logger) {
                logger->warning("XML buffer is empty or null, cannot parse.");
            }
        }
        // Call renderText through virtual dispatch - this will call the override if one exists
        // in the derived class, otherwise it will call our implementation
        this->renderText(event_xml_data_.providerName);
    }

    void EventLogEvent::renderText(const char* publisher_name) {
        auto logger = LOG_THIS;
        if (text_buffer_ != nullptr)
            return;
        text_buffer_ = Globals::instance()->getMessageBuffer("text_buffer_");
        text_buffer_[0] = 0;
        wchar_t* text_buffer_w = reinterpret_cast<wchar_t*>(Globals::instance()->getMessageBuffer("text_buffer_w_renderText"));
        wchar_t publisher_name_w[1000];
        MultiByteToWideChar(CP_UTF8, 0, publisher_name, -1, publisher_name_w,
            sizeof(publisher_name_w) / sizeof(wchar_t));

        const char* event_id_str = (event_xml_data_.eventID[0] ? event_xml_data_.eventID : "unknown");
        const char* event_time_str = (event_xml_data_.systemTime[0] ? event_xml_data_.systemTime : "unknown");
        const char* event_source_str = (event_xml_data_.providerName[0] ? event_xml_data_.providerName : "unknown");
        const char* event_log_name_str = (event_xml_data_.channel[0] ? event_xml_data_.channel : "unknown");
        const char* provider_name_str = (publisher_name && *publisher_name) ? publisher_name : "unknown";
        const char* provider_base_str = provider_name_str;

        // Some providers come in as "Provider/Operational" (channel appended). EvtOpenPublisherMetadata
        // needs just the provider name, so trim anything after the first '/' for a fallback attempt.
        char provider_base[1000] = { 0 };
        const char* slash = strchr(provider_name_str, '/');
        if (slash && slash != provider_name_str) {
            size_t len = static_cast<size_t>(slash - provider_name_str);
            if (len >= sizeof(provider_base)) len = sizeof(provider_base) - 1;
            strncpy_s(provider_base, provider_name_str, len);
            provider_base_str = provider_base;
        }
        
        EVT_HANDLE metadata_handle = NULL;
        DWORD err = 0;

        __try {
            metadata_handle = EvtOpenPublisherMetadata(nullptr, publisher_name_w, nullptr, 0, 0);
            if (!metadata_handle) {
                err = GetLastError();
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            err = GetExceptionCode();
            logger->critical("EventLogEvent::renderText()> SEH Exception 0x%08X during EvtOpenPublisherMetadata for Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s. Forcing failure.\n", err, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
            metadata_handle = NULL; 
        }

        if (!metadata_handle && err == ERROR_FILE_NOT_FOUND && provider_base_str != provider_name_str) {
            wchar_t provider_base_w[1000];
            MultiByteToWideChar(CP_UTF8, 0, provider_base_str, -1, provider_base_w,
                sizeof(provider_base_w) / sizeof(wchar_t));
            logger->debug("EventLogEvent::renderText()> Retrying EvtOpenPublisherMetadata with trimmed Provider=\"%s\" (from \"%s\")\n", provider_base_str, provider_name_str);
            metadata_handle = EvtOpenPublisherMetadata(nullptr, provider_base_w, nullptr, 0, 0);
            if (!metadata_handle) {
                err = GetLastError();
            } else {
                provider_name_str = provider_base_str; // use trimmed provider for subsequent formatting/logging
            }
        }

        if (!metadata_handle) {
            logger->debug2("EventLogEvent::renderText()> EvtOpenPublisherMetadata failed with error %lu (0x%08X) for Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s\n", err, err, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
            Globals::instance()->releaseMessageBuffer(reinterpret_cast<char*>(text_buffer_w));
            if (text_buffer_) strcpy_s(text_buffer_, Globals::MESSAGE_BUFFER_SIZE, "(Failed to open publisher metadata)");
            return;
        }

        DWORD buffer_size_needed = 0;
        BOOL succeeded = FALSE;
        err = 0;

        auto attempt_format = [&](EVT_HANDLE meta_handle)->BOOL {
            BOOL local_success = FALSE;
            buffer_size_needed = 0;
            err = 0;
            __try {
                local_success = EvtFormatMessage(meta_handle, windows_event_handle_, 0, 0, nullptr,
                    EvtFormatMessageEvent, Globals::MESSAGE_BUFFER_SIZE / sizeof(wchar_t) - 1, text_buffer_w,
                    &buffer_size_needed);
                if (!local_success) {
                    err = GetLastError();
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                err = GetExceptionCode();
                logger->critical("EventLogEvent::renderText()> SEH Exception 0x%08X during EvtFormatMessage for event handle %p, Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s. Forcing failure.\n", err, windows_event_handle_, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
                local_success = FALSE;
                buffer_size_needed = 0;
            }
            return local_success;
        };

        succeeded = attempt_format(metadata_handle);

        if (!succeeded && err == ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND) {
            // Retry using an explicit en-US locale to bypass missing locale resources
            EvtClose(metadata_handle);
            metadata_handle = EvtOpenPublisherMetadata(nullptr, publisher_name_w, nullptr,
                MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), 0);
            if (!metadata_handle) {
                err = GetLastError();
                logger->debug("EventLogEvent::renderText()> Locale fallback EvtOpenPublisherMetadata failed with error %lu (0x%08X) for Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s\n", err, err, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
            }
            else {
                logger->debug("EventLogEvent::renderText()> Retrying EvtFormatMessage with en-US locale for event handle %p, Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s\n", windows_event_handle_, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
                succeeded = attempt_format(metadata_handle);
            }
        }
        
        if (!succeeded) {
            if (err == ERROR_EVT_MESSAGE_NOT_FOUND) {
                logger->debug("EventLogEvent::renderText()> Message template not found (Error %lu / 0x%08X) for event handle %p, Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s\n", err, err, windows_event_handle_, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
                if (text_buffer_) strcpy_s(text_buffer_, Globals::MESSAGE_BUFFER_SIZE, "(Message template unavailable)");
            }
            else {
                logger->recoverable_error("EventLogEvent::renderText()> EvtFormatMessage failed with error %lu (0x%08X) for event handle %p, Provider=\"%s\", Log=\"%s\" Source=\"%s\" EventID=%s Time=%s\n", err, err, windows_event_handle_, provider_name_str, event_log_name_str, event_source_str, event_id_str, event_time_str);
                if (text_buffer_) strcpy_s(text_buffer_, Globals::MESSAGE_BUFFER_SIZE, "(Failed to format message)");
            }
            text_buffer_w[0] = L'\0';
            buffer_size_needed = 0;
        }
        text_buffer_w[buffer_size_needed] = L'\0';

        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, text_buffer_w,
            static_cast<int>(buffer_size_needed), nullptr, 0, nullptr, nullptr);
        if (utf8_size >= Globals::MESSAGE_BUFFER_SIZE - 1)
            utf8_size = Globals::MESSAGE_BUFFER_SIZE - 1;
        WideCharToMultiByte(CP_UTF8, 0, text_buffer_w, static_cast<int>(buffer_size_needed),
            text_buffer_, utf8_size, nullptr, nullptr);
        text_buffer_[utf8_size] = '\0';  // Ensure null termination
        if (metadata_handle) {
            EvtClose(metadata_handle);
        }
        Globals::instance()->releaseMessageBuffer(reinterpret_cast<char*>(text_buffer_w));
    }

    DWORD EventLogEvent::getEventId() const {
        if (!isRendered()) {
            // XML hasn't been rendered/parsed yet. Cannot get Event ID.
            // This might happen if renderEvent() wasn't called or failed.
            // Return 0 or some indicator of failure. Consider logging.
            auto logger = LOG_THIS;
            if (logger) {
                logger->warning("getEventId() called before event was rendered.");
            }
            return 0; // Or potentially throw an exception
        }

        const char* event_id_str = event_xml_data_.eventID;
        if (!event_id_str || !*event_id_str) {
            auto logger = LOG_THIS;
            if (logger) {
                logger->warning("EventID value is empty in parsed data.");
            }
            return 0; // EventID value is empty
        }

        // Check for negative numbers first (since EventID should be a positive DWORD)
        if (event_id_str[0] == '-') {
            auto logger = LOG_THIS;
            if (logger) {
                logger->warning("Failed to convert EventID '%s' to DWORD.", event_id_str);
            }
            return 0; // Negative values are not valid for DWORD EventIDs
        }

        // Convert the string to DWORD
        char* end_ptr;
        DWORD event_id = std::strtoul(event_id_str, &end_ptr, 10);

        if (*end_ptr != '\0') {
            // Conversion failed or partial conversion
            auto logger = LOG_THIS;
            if (logger) {
                logger->warning("Failed to convert EventID '%s' to DWORD.", event_id_str);
            }
            return 0; // Return 0 to indicate conversion failure
        }

        return event_id;
    }
}

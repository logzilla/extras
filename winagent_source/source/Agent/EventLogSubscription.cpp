/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "EventLogSubscription.h"
#include "Logger.h"

#pragma comment(lib, "wevtapi.lib")

namespace Syslog_agent {

    EventLogSubscription::EventLogSubscription(EventLogSubscription&& source) noexcept
        : subscription_name_(source.subscription_name_),
        channel_(source.channel_),
        query_(source.query_),
        event_handler_(std::move(source.event_handler_)),
        subscription_handle_(source.subscription_handle_),
        subscription_active_(false),
        bookmark_(NULL) {
        if (source.subscription_active_) {
            source.cancelSubscription();
            auto bookmark = source.getBookmark();
            subscribe(bookmark.c_str());
        }
    }

    EventLogSubscription::~EventLogSubscription() {
        if (subscription_active_) {
            cancelSubscription();
        }
    }

    void EventLogSubscription::subscribe(const wstring& bookmark_xml) {
        bookmark_xml_ = wstring(bookmark_xml);
        if (bookmark_xml_.empty()) {
            bookmark_ = EvtCreateBookmark(NULL);
        }
        else {
            bookmark_ = EvtCreateBookmark(bookmark_xml_.c_str());
            if (bookmark_ == NULL) {
                bookmark_ = EvtCreateBookmark(NULL);
            }
        }
        if (bookmark_ == NULL) {
            Logger::fatal("EventLogSubscription::subscribe()> could not create "
                " bookmark for %s\n", channel_.c_str());
            exit(1); // shouldn't be necessary
        }
        EVT_HANDLE new_subscription = EvtSubscribe(NULL, NULL, channel_.c_str(), 
            query_.c_str(), bookmark_, this,
            EventLogSubscription::handleSubscriptionEvent, EvtSubscribeStartAfterBookmark);
        subscription_active_ = true;
    }

    void EventLogSubscription::cancelSubscription() {
        if (subscription_active_) {
            saveBookmark();
            EvtClose(subscription_handle_);
            subscription_active_ = false;
        }
    }

    DWORD WINAPI EventLogSubscription::handleSubscriptionEvent(
        EVT_SUBSCRIBE_NOTIFY_ACTION action, 
        PVOID pContext, 
        EVT_HANDLE hEvent) {
        DWORD status = ERROR_SUCCESS;
        EventLogSubscription* subscription = reinterpret_cast<EventLogSubscription*>(pContext);
        if (!subscription->subscription_active_)
            return ERROR_SUCCESS;

        switch (action)
        {
            // You should only get the EvtSubscribeActionError action if your subscription flags 
            // includes EvtSubscribeStrict and the channel contains missing event records.
        case EvtSubscribeActionError:
        {
            DWORD errorCode = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(hEvent));
            // Cast hEvent to DWORD to get the error code.
            if (ERROR_EVT_QUERY_RESULT_STALE == errorCode)
            {
                Logger::recoverable_error("EventLogSubscription::handleSubscriptionEvent()> The "
                    "subscription callback was notified that event records are missing.\n");
                // Additional handling if needed.
            }
            else
            {
                Logger::recoverable_error("EventLogSubscription::handleSubscriptionEvent()>The "
                    "subscription callback received the following Win32 error: %lu\n", errorCode);
            }
            break;
        }
        case EvtSubscribeActionDeliver:
        {
            EventLogEvent processed_event(hEvent);
            subscription->event_handler_->handleEvent(subscription->subscription_name_.c_str(),
                processed_event);
            if (!EvtUpdateBookmark(subscription->bookmark_, hEvent)) {
                Logger::recoverable_error("EventLogSubscription::handleSubscriptionEvent()>"
                    " could not update bookmark.\n");
            }
            break;
        }

        default:
            Logger::recoverable_error("EventLogSubscription::handleSubscriptionEvent()>"
                " Unknown action.\n");
        }

        return ERROR_SUCCESS;
    }

    void EventLogSubscription::saveBookmark() {
        wchar_t xml_buffer[2048];
        DWORD buffer_used;
        DWORD property_count;
        if (subscription_active_) {
            if (!EvtRender(NULL, bookmark_, EvtRenderBookmark, sizeof(xml_buffer), 
                xml_buffer, &buffer_used, &property_count)) {
                auto status = GetLastError();
                if (status == ERROR_INSUFFICIENT_BUFFER) {
                    Logger::recoverable_error("EventLogSubscription::saveBookmark()>"
                        " insufficient xml buffer\n");
                }
                else {
                    Logger::recoverable_error("EventLogSubscription::saveBookmark()>"
                        " error %d\n", status);
                }
            }
            else {
                xml_buffer[buffer_used] = 0;
                bookmark_xml_ = wstring(xml_buffer);
            }
        }
    }

}

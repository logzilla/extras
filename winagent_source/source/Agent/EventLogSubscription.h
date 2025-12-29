#pragma once

#include <memory>
#include <string>
#include <vector>
#include <Windows.h>
#include <winevt.h>
#include "EventLogEvent.h"
// Include your IEventHandler interface:
#include "IEventHandler.h"  
#include <atomic> // Added for std::atomic

namespace Syslog_agent {
    using namespace std;

    class EventLogSubscription {
    public:
        EventLogSubscription(
            const wstring& subscription_name,
            const wstring& channel,
            const wstring& query,
            const wstring& bookmark_xml,
            const bool only_while_running,
            unique_ptr<IEventHandler> event_handler)
            : subscription_name_(subscription_name),
            channel_(channel),
            query_(query),
            only_while_running_(only_while_running),
            event_handler_(move(event_handler)),
            bookmark_(NULL),
            subscription_handle_(NULL),
            subscription_active_(false),
            bookmark_modified_(false),
            last_bookmark_save_(0),
            events_since_last_save_(0)
        {
            bookmark_xml_buffer_[0] = 0;
        }

        // Move constructor
        EventLogSubscription(EventLogSubscription&& source) noexcept;

        // Move assignment operator
        EventLogSubscription& operator=(EventLogSubscription&& source) noexcept;
        
        // Default constructor - creates an empty, uninitialized subscription
        // This is needed for vector operations
        EventLogSubscription() : 
            bookmark_(NULL),
            subscription_handle_(NULL),
            subscription_active_(false),
            bookmark_modified_(false),
            last_bookmark_save_(0),
            events_since_last_save_(0),
            only_while_running_(false)
        {
            bookmark_xml_buffer_[0] = 0;
        }

        ~EventLogSubscription();

        void subscribe(const wstring& bookmark_xml, const bool only_while_running);
        void cancelSubscription();
        bool saveBookmark();  // Returns true if bookmark was saved
        wstring getName() const { return subscription_name_; }
        wstring getChannel() const { return channel_; }
        void markBookmarkModified() { bookmark_modified_ = true; }
        bool incrementedSaveBookmark() {
            if (++events_since_last_save_ >= MAX_EVENTS_BETWEEN_SAVES) {
                saveBookmark();
                return true;
            }
            return false;
        }
        EVT_HANDLE getBookmark() const { return bookmark_; }
        bool updateBookmark(EVT_HANDLE hEvent);

        // Static methods for event counters
        static uint64_t getSuccessfullyHandledEventsCount();
        static uint64_t getUnsuccessfullyHandledEventsCount();

    private:
        static DWORD WINAPI handleSubscriptionEvent(
            EVT_SUBSCRIBE_NOTIFY_ACTION action,
            PVOID pContext,
            EVT_HANDLE hEvent);

        static constexpr DWORD MAX_BOOKMARK_SIZE = 4096;  // 4KB should be more than enough for bookmark XML

        // Disable copy constructor and assignment
        EventLogSubscription(const EventLogSubscription&) = delete;
        EventLogSubscription& operator=(const EventLogSubscription&) = delete;

        static constexpr int MAX_EVENTS_BETWEEN_SAVES = 100;

        wstring subscription_name_;
        wstring channel_;
        wstring query_;
        wchar_t bookmark_xml_buffer_[MAX_BOOKMARK_SIZE];
        bool only_while_running_;

        // Store a unique_ptr to IEventHandler
        unique_ptr<IEventHandler> event_handler_;

        EVT_HANDLE bookmark_;
        EVT_HANDLE subscription_handle_;
        bool subscription_active_;
        bool bookmark_modified_;  // Flag to track if bookmark needs saving
        time_t last_bookmark_save_;  // Last time bookmark was saved
        int events_since_last_save_;

        // Static counters for handled events
        static std::atomic<uint64_t> s_successfullyHandledEvents;
        static std::atomic<uint64_t> s_unsuccessfullyHandledEvents;
    };
}

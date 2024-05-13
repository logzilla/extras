/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <Windows.h>
#include <winevt.h>
#include "EventLogEvent.h"
#include "ChannelEventHandlerBase.h"

using namespace std;

namespace Syslog_agent {
	class EventLogSubscription {
	public:
		EventLogSubscription(
			const wstring& subscription_name, 
			const wstring& channel, 
			const wstring& query, 
			const wstring& bookmark_xml,
			unique_ptr<ChannelEventHandlerBase> event_handler) :
			subscription_name_(wstring(subscription_name)),
			channel_(wstring(channel)),
			query_(wstring(query)),
			bookmark_(NULL),
			bookmark_xml_(wstring(bookmark_xml)),
			event_handler_(std::move(event_handler)),
			subscription_handle_(NULL),
			subscription_active_(false) {
			printf("test\n");
		}
		EventLogSubscription(EventLogSubscription&& source) noexcept;
		~EventLogSubscription();
		void subscribe(const wstring& bookmark_xml);
		void cancelSubscription();
		void saveBookmark();
		wstring getBookmark() const { return bookmark_xml_; }
		wstring getName() const { return subscription_name_; }
		wstring getChannel() const { return channel_; }
	private:
		static DWORD WINAPI handleSubscriptionEvent(EVT_SUBSCRIBE_NOTIFY_ACTION action, 
			PVOID pContext, EVT_HANDLE hEvent);
		EventLogSubscription(const EventLogSubscription&);
		wstring subscription_name_;
		wstring channel_;
		wstring query_;
		wstring bookmark_xml_;
		unique_ptr<ChannelEventHandlerBase> event_handler_;
		EVT_HANDLE subscription_handle_;
		EVT_HANDLE bookmark_;
		bool subscription_active_;

	};
}
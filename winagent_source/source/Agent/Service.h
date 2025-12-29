/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Configuration.h"
#include "EventLogSubscription.h"
#include "FileWatcher.h"
#include "HTTPMessageBatcher.h"
#include "INetworkClient.h"
#include "JSONMessageBatcher.h"
#include "LogConfiguration.h"
#include "MessageBatcher.h"
#include "MessageQueue.h"
#include "SyslogSender.h"
#include "WindowsEvent.h"
#include "../AgentLib/HttpNetworkClient.h"
#include "../AgentLib/INetworkClient.h"

namespace Syslog_agent {

#define VERSION_MAJOR	    	"6"
#define VERSION_MINOR			"34"
#define VERSION_FIXVERSION      "1"
#define VERSION_MINORFIXVERSION "0"
#define APP_NAME    			"LZ Syslog Agent"
#define SERVICE_NAME			L"LZ Syslog Agent"
    
class Service {
public:

    static constexpr size_t MESSAGE_QUEUE_SIZE = 10000;
    static constexpr size_t MESSAGE_BUFFERS_CHUNK_SIZE = 1000;
    static constexpr int DEFAULT_EVENT_LOG_POLL_INTERVAL = 1;
    static constexpr int RATE_CHECK_INTERVAL_SEC{ 30 };  
    static constexpr double RATE_THRESHOLD_RATIO{ 1.5 };

    // Static member variables, visible for use by sendMessagesThread
    static Configuration config_;
    static shared_ptr<MessageQueue> primary_message_queue_;
    static shared_ptr<MessageQueue> secondary_message_queue_;
    static shared_ptr<INetworkClient> primary_network_client_;
    static shared_ptr<INetworkClient> secondary_network_client_;
    static shared_ptr<MessageBatcher> primary_batcher_;
    static shared_ptr<MessageBatcher> secondary_batcher_;
    static unique_ptr<SyslogSender> sender_;

    // Service control handler
    static DWORD WINAPI ServiceHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
    static void RegisterServiceCtrlHandler();

    // Public interface
    static void run(bool running_as_console);
    static void shutdown();
    static void fatalErrorHandler(const char* msg);
    static void loadConfiguration(bool running_from_console, bool override_log_level, Logger::LogLevel override_log_level_setting);
    static void incrementHandledEventCount(uint64_t count);

protected:
    // Initialization methods
    static bool initializeNetworkComponents();
    static bool initializePrimaryCertificate();
    static bool initializeSecondaryComponents();
    static bool getAndSetLogZillaVersion(const shared_ptr<INetworkClient>& client, bool isPrimary);
    static void initializeEventLogSubscriptions(const vector<LogConfiguration>& logs);

    // Main loop control methods
    static void mainLoop(bool running_as_console, bool& first_loop, int& restart_needed);
    static bool checkForShutdown(bool running_as_console, int& restart_needed);
    static void handleQueueStatusAndConfig();
    static void cleanupAndShutdown(bool running_as_console, int restart_needed);

private:
    // Internal state
    static std::atomic<bool> fatal_shutdown_in_progress;
    static unique_ptr<thread> send_thread_;
    static volatile bool shutdown_requested_;
    static volatile bool service_shutdown_requested_;
    static WindowsEvent shutdown_event_;
    static shared_ptr<FileWatcher> filewatcher_;
    static vector<EventLogSubscription> subscriptions_;
    static std::atomic<uint64_t> handled_event_count_;
    static HANDLE g_StopEvent;
    static HANDLE g_ShutdownCompleteEvent;

    // Prevent instantiation
    Service() = delete;
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;

    // Static helper methods
    static bool initializeNetworkComponentsInternal(bool is_primary_target);
    static bool initializeCertificate(std::shared_ptr<HttpNetworkClient> client, const std::wstring& cert_filename_segment, bool is_primary);
    static void logNetworkInitializationSuggestions(InitializeError error, bool is_primary);
};

// App–specific service start/stop functions to be called by WindowsService
#ifdef __cplusplus
extern "C" {
#endif
void AppServiceStart(DWORD argc, const wchar_t* const* argv);
void AppServiceStop();
#ifdef __cplusplus
}
#endif

} // namespace Syslog_agent
/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once
#include <memory>
#include <thread>
#include "Configuration.h"
#include "EventLogSubscription.h"
#include "FileWatcher.h"
#include "Logger.h"
#include "MessageQueue.h"
#include "NetworkClient.h"
#include "WindowsEvent.h"

namespace Syslog_agent {

#define VERSION_MAJOR	    	"6"
#define VERSION_MINOR			"30"
#define VERSION_FIXVERSION      "2"
#define VERSION_MINORFIXVERSION "0"
#define APP_NAME    			"LZ Syslog Agent"
#define SERVICE_NAME			L"LZ Syslog Agent"


    class Service {
    public:
        static void run(bool running_as_console);
        static void shutdown();

        static constexpr int MESSAGE_QUEUE_SIZE = 100000;
        static constexpr int MESSAGE_BUFFERS_CHUNK_SIZE = 100;
        static constexpr int MSEC_BETWEEN_CONNECTION_ATTEMPTS = 4000;
        static unique_ptr<thread> send_thread_;
        static shared_ptr<MessageQueue> primary_message_queue_;
        static shared_ptr<MessageQueue> secondary_message_queue_;
        static Configuration config_;
        static shared_ptr<NetworkClient> primary_network_client_;
        static shared_ptr<NetworkClient> secondary_network_client_;
        static volatile bool shutdown_requested_;
        static volatile bool service_shutdown_requested_;
        static WindowsEvent shutdown_event_;
        static void loadConfiguration(bool running_from_console, 
            bool override_log_level, Logger::LogLevel override_log_level_setting)
        {
            config_.loadFromRegistry(running_from_console, override_log_level, override_log_level_setting);
        }

    private:
        static shared_ptr<FileWatcher> filewatcher_;
        static bool restart(); // returns false for failure
        static bool setForRestart(); // returns false for failure
        static bool setForNoRestart(); // returns false for failure
        static vector<EventLogSubscription> subscriptions_;
        // Flag to indicate that a fatal shutdown is in progress
        static std::atomic<bool> fatal_shutdown_in_progress;

        static void fatalErrorHandler(const char* msg);

    };
}

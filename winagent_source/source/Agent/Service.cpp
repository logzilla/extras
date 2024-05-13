/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include <conio.h>
#include <fileapi.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <tchar.h>
#include <time.h>
#include <vector>
#include <windows.h>

#include "Logger.h"
#include "Configuration.h"
#include "EventHandlerMessageQueuer.h"
#include "EventLogEvent.h"
#include "EventLogSubscription.h"
#include "FileWatcher.h"
#include "Service.h"
#include "SyslogAgentSharedConstants.h"
#include "SyslogSender.h"
#include "Util.h"

using namespace Syslog_agent;


std::atomic<bool> Service::fatal_shutdown_in_progress = false; 
unique_ptr<thread> Service::send_thread_ = nullptr;
Configuration Service::config_;
shared_ptr<MessageQueue> Service::primary_message_queue_ = nullptr;
shared_ptr<MessageQueue> Service::secondary_message_queue_ = nullptr;
shared_ptr<NetworkClient> Service::primary_network_client_ = nullptr;
shared_ptr<NetworkClient> Service::secondary_network_client_ = nullptr;
volatile bool Service::shutdown_requested_ = false;
volatile bool Service::service_shutdown_requested_ = false;
WindowsEvent Service::shutdown_event_{ L"LogZilla_SyslogAgent_Service_Shutdown" };
shared_ptr<FileWatcher> Service::filewatcher_ = nullptr;
vector<EventLogSubscription> Service::subscriptions_;


void sendMessagesThread() {
	Logger::debug2("sendMessagesThread()> starting\n");
	try {
		SyslogSender sender(
			Service::config_,
			Service::primary_message_queue_,
			Service::secondary_message_queue_,
			Service::primary_network_client_,
			Service::secondary_network_client_);
		Logger::debug2("sendMessagesThread()> sender running\n");
		sender.run();
	}
	catch (std::exception& exception) {
		Logger::critical("%s\n", exception.what());
	}
	Logger::debug2("sendMessagesThread()> exiting\n");
}


void Service::run(bool running_as_console) {

	Logger::setFatalErrorHandler(Service::fatalErrorHandler);

	Logger::debug("Service::run()> loading setup file (if present)\n");
	Registry::loadSetupFile();

	config_.use_log_agent_ = true;

	if (config_.tail_filename_ != L"") {
		string program_name = Util::wstr2str(config_.tail_program_name_);
		Logger::debug("Service::run()> starting file watcher for %s\n", 
			Util::wstr2str(config_.tail_filename_).c_str());
		filewatcher_ = make_shared<FileWatcher>(
			config_,
			config_.tail_filename_.c_str(),
			config_.MAX_TAIL_FILE_LINE_LENGTH,
			program_name.c_str(),
			config_.host_name_.c_str(),
			(config_.severity_ == SharedConstants::Severities::DYNAMIC 
				? SharedConstants::Severities::INFORMATIONAL : config_.severity_),
			config_.facility_
			);
	}

	Logger::debug("Service::run()> starting message queue\n");
	Service::primary_message_queue_ 
		= make_shared<MessageQueue>(Service::MESSAGE_QUEUE_SIZE, 
			Service::MESSAGE_BUFFERS_CHUNK_SIZE);

	Logger::debug("Service::run()> starting network clients\n");
	Service::primary_network_client_ = make_shared<NetworkClient>();
	Logger::debug2("Service::run()> initializing primary_network_client\n");

	if (!Service::primary_network_client_->initialize(&config_, 
		config_.primary_api_key_, config_.primary_host_)) {
		Logger::fatal("Could not initialize primary network client\n");
	}

	// read primary cert file
	Logger::debug2("Service::run()> checking for primary tls\n");
	if (config_.primary_use_tls_) {
		wstring primary_cert_path = Util::getThisPath(true) 
			+ config_.PRIMARY_CERT_FILENAME;
		if (!primary_network_client_->loadCertificate(primary_cert_path)) {
			Logger::fatal("Could not read primary cert from %s\n", 
				Util::wstr2str(primary_cert_path).c_str());
		}
	}

	string logzilla_version;

	Logger::info("Service::run()> getting primary LogZilla version...\n");
	logzilla_version = primary_network_client_->getLogzillaVersion();
	if (logzilla_version == "") {
		Logger::info("Error getting version.\n");
        Logger::critical("Could not get primary LogZilla version\n");
    }
	else {
		Logger::info("LogZilla version %s\n", logzilla_version.c_str());
		if (logzilla_version[0] == 'v') {
			logzilla_version = logzilla_version.substr(1);
		}
		config_.setPrimaryLogzillaVersion(logzilla_version);
	}

	Logger::debug2("Service::run()> checking for secondary host\n");
	if (config_.hasSecondaryHost()) {
		Logger::debug2("Service::run()> has secondary host, making"
			" message queue and client\n");
		Service::secondary_message_queue_ 
			= make_shared<MessageQueue>(Service::MESSAGE_QUEUE_SIZE, 
				Service::MESSAGE_BUFFERS_CHUNK_SIZE);
		Service::secondary_network_client_ = make_shared<NetworkClient>();
		Logger::debug2("Service::run()> initializing"
			" secondary_network_client\n");
		if (!Service::secondary_network_client_->initialize(&config_, 
			config_.secondary_api_key_, config_.secondary_host_)) {
			Logger::fatal("Could not initialize secondary network client\n");
		}

		// read secondary cert file
		Logger::debug2("Service::run()> checking for secondary tls\n");
		if (config_.secondary_use_tls_) {
			wstring secondary_cert_path = Util::getThisPath(true) 
				+ config_.SECONDARY_CERT_FILENAME;
			if (!secondary_network_client_->loadCertificate(secondary_cert_path)) {
                Logger::fatal("Could not read secondary cert from %s\n", 
					Util::wstr2str(secondary_cert_path).c_str());
            }
		}

		Logger::info("Service::run()> getting secondary LogZilla version...\n");
		logzilla_version = secondary_network_client_->getLogzillaVersion();
		if (logzilla_version == "") {
			Logger::info("Error getting version.\n");
			Logger::critical("Could not get secondary LogZilla version\n");
		}
		else {
			Logger::info("LogZilla version %s\n", logzilla_version.c_str());
			if (logzilla_version[0] == 'v') {
				logzilla_version = logzilla_version.substr(1);
			}
			config_.setSecondaryLogzillaVersion(logzilla_version);
		}

	}
	else {
		Service::secondary_message_queue_ = nullptr;
		Service::secondary_network_client_ = nullptr;
	}

	Logger::debug2("Service::run()> pushing network clients\n");

	vector<shared_ptr<NetworkClient>> clients;
	clients.push_back(primary_network_client_);
	if (secondary_network_client_) {
		clients.push_back(secondary_network_client_);
	}

	Logger::debug2("Service::run()> starting sender thread\n");
	thread sender(sendMessagesThread);
	bool first_loop = true;
	int restart_needed = 0;
	Logger::debug2("Service::run()> starting heartbeat monitoring\n");

	Logger::debug("Service::run()> starting event log subscriptions\n");
	if (config_.use_log_agent_) {
		subscriptions_.reserve(config_.logs_.size());
		for (auto& log : config_.logs_) {
			unique_ptr<EventHandlerMessageQueuer> handler
				= make_unique<EventHandlerMessageQueuer>(
					config_,
					primary_message_queue_,
					secondary_message_queue_,
					const_cast<const wchar_t*>(log.name_.c_str()));
			EventLogSubscription subscription(
				log.name_,
				log.channel_,
				wstring(L"*"),
				(config_.only_while_running_ ? L"" : log.bookmark_),
				std::move(handler));
			subscriptions_.push_back(std::move(subscription));
			auto bookmark = Registry::readBookmark(log.channel_.c_str());
			subscriptions_.back().subscribe(bookmark);
		}
	}

	Logger::debug("Service::run()> starting main loop\n");

	while (!shutdown_requested_) {
		Logger::debug2("service run first !shutdown_requested\n");
		auto this_run_time = time(nullptr);

		if (filewatcher_ != nullptr) {
			Result tail_result = filewatcher_->process();
			if (!tail_result.isSuccess() && tail_result.statusCode() != FileWatcher::NoNewData) {
				Logger::debug("FileWatcher result: %s\n", tail_result.what());
			}
		}

		first_loop = false;
		Logger::debug("waiting for key hit/shutdown requested...\n");
		for (auto i = 0; i < config_.event_log_poll_interval_; i++) {
			if (shutdown_requested_) {
				break;
			}
			if (primary_message_queue_->length() > 0) {
				Logger::debug("Primary Queue length==%d\n", primary_message_queue_->length());
			}
			if (config_.use_log_agent_ && primary_message_queue_->isEmpty() 
				&& (secondary_message_queue_ == nullptr || secondary_message_queue_->isEmpty())) {
				Logger::debug2("Saving config to registry\n");
				config_.saveToRegistry();
			}
			if (_kbhit() || restart_needed) {
				if (restart_needed) {
					Logger::debug("restart needed\n");
				}
				else {
					Logger::debug("key hit\n");
				}
				shutdown_requested_ = true;
				break;
			}
			if (!shutdown_requested_) {
				shutdown_event_.wait(1000);
			}
		}
		Logger::debug2("no key hit & no shutdown requested...\n");
	}

	for (auto& sub : subscriptions_) {
		sub.cancelSubscription();
		auto channel = sub.getChannel();
		auto bookmark = sub.getBookmark();
		Registry::writeBookmark(channel.c_str(), bookmark.c_str());
	}

	Logger::debug("service run joining sender\n");
	SyslogSender::stop();
	sender.join();

	// handle restart only for automated restarts, not for manual shutdown...
	if (!shutdown_requested_ && (!service_shutdown_requested_) && restart_needed) {
		if (running_as_console) {
			Logger::debug("restart needed but running as console, exiting\n");
		}
		Logger::debug("attempting to restart\n");
		Sleep(5000);
		exit(1);
	}
	else {
		Logger::debug("LZ Syslog Agent exiting\n");
	}

	Sleep(2000); // to allow any overlapping callbacks to finish
	Logger::debug("service run exiting\n");

}


void Service::shutdown() {
	Logger::info("Service shutdown requested\n");
	service_shutdown_requested_ = true;
	shutdown_requested_ = true;
	shutdown_event_.signal();
}


void Service::fatalErrorHandler(const char* msg) {
	// Check if a fatal shutdown is already in progress
	if (fatal_shutdown_in_progress.exchange(true)) {
		// Fatal shutdown already in progress, return immediately to avoid recursion or repeated shutdown attempts
		return;
	}

	// Perform a graceful shutdown if possible
	// (Implement this method to try to shut down your application components safely)
	try {
		shutdown();
	}
	catch (...) {
		// Catch all exceptions to avoid any throw from leaving the fatal handler
	}
	Sleep(5000); // Wait for 5 seconds to allow the graceful shutdown to complete

	// Forcefully exit the application if the graceful shutdown didn't work or isn't possible
	// Use _exit instead of exit to avoid calling static destructors or atexit handlers
	_exit(1);
}


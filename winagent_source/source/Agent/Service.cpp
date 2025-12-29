/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN
#include <chrono>
#include <stdio.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <conio.h>
#include <fileapi.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <time.h>
#include <vector>
#include <excpt.h>
#include <eh.h> // For GetExceptionCode macro in C++
#include <windows.h>
#include <atomic>
#include <winhttp.h> // For INTERNET_DEFAULT_xxx_PORT constants & InitializeErrorToString if moved

#include "Configuration.h"
#include "EventHandlerMessageQueuer.h"
#include "EventLogEvent.h"
#include "EventLogSubscription.h"
#include "FileWatcher.h"
#include "Globals.h"
#include "HttpNetworkClient.h" // Ensure this is included
#include "INetworkClient.h"
#include "JsonNetworkClient.h" // Assuming this exists for JSON path
#include "Logger.h"
#include "Service.h"
#include "SlidingWindowMetrics.h"
#include "SyslogAgentSharedConstants.h"
#include "SyslogSender.h"
#include "Util.h"
#include "WindowsService.h"
#include "Registry.h"

#undef  _exception_code        // get rid of the macro
extern "C" unsigned long __cdecl _exception_code(void);  // declare it


using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::string;
using std::thread;
using std::make_shared;
using std::atomic;
using std::wstring;

namespace Syslog_agent {

// Define static members
Configuration Service::config_;
shared_ptr<MessageQueue> Service::primary_message_queue_;
shared_ptr<MessageQueue> Service::secondary_message_queue_;
shared_ptr<INetworkClient> Service::primary_network_client_;
shared_ptr<INetworkClient> Service::secondary_network_client_;
shared_ptr<MessageBatcher> Service::primary_batcher_;
shared_ptr<MessageBatcher> Service::secondary_batcher_;
unique_ptr<SyslogSender> Service::sender_;
std::atomic<bool> Service::fatal_shutdown_in_progress = false;
unique_ptr<thread> Service::send_thread_ = nullptr;
volatile bool Service::shutdown_requested_ = false;
volatile bool Service::service_shutdown_requested_ = false;
WindowsEvent Service::shutdown_event_(L"LogZilla_SyslogAgent_Service_Shutdown");
shared_ptr<FileWatcher> Service::filewatcher_;
vector<EventLogSubscription> Service::subscriptions_;
static SERVICE_STATUS_HANDLE service_status_handle_ = nullptr;
HANDLE Service::g_StopEvent = nullptr;
HANDLE Service::g_ShutdownCompleteEvent = nullptr;
std::atomic<uint64_t> Service::handled_event_count_{0}; // Added definition

namespace { 
    void cleanupNetworkClient(shared_ptr<INetworkClient>& client) {
        if (client) {
            client->close();
            client.reset();
        }
    }
    void cleanupMessageQueue(shared_ptr<MessageQueue>& queue) {
        if (queue) {
            queue->beginShutdown();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            while (!queue->isEmpty()) {
                queue->removeFront();
            }
            queue.reset();
        }
    }
} 

void Service::loadConfiguration(bool running_from_console, bool override_log_level, Logger::LogLevel override_log_level_setting) {
    config_.loadFromRegistry(running_from_console, override_log_level, override_log_level_setting);
    auto logger = LOG_THIS;
    LAST_RESORT_LOGGER->always( "Service::loadConfiguration() log level: %s\n", Logger::LOGLEVEL_ABBREVS[static_cast<int>(logger->getLogLevel())]);
}

static void sendMessagesThread() {
    auto logger = LOG_THIS;
    logger->debug2("sendMessagesThread() starting\n");
    Service::sender_ = make_unique<SyslogSender>(
        Service::primary_message_queue_,
        Service::secondary_message_queue_,
        Service::primary_network_client_,
        Service::secondary_network_client_,
        Service::primary_batcher_,
        Service::secondary_batcher_,
        Service::config_.getMaxBatchCount(),
        Service::config_.getMaxBatchAge()
    );
    try {
        Service::sender_->run();
    } catch (const std::exception& e) {
        logger->fatal("Exception in sendMessagesThread: %s\n", e.what());
        Service::fatalErrorHandler("Fatal error in send thread");
    } catch (...) {
        logger->fatal("Unknown exception in sendMessagesThread.\n");
        Service::fatalErrorHandler("Fatal unknown error in send thread");
    }
    logger->debug2("sendMessagesThread() ending\n");
}

void Service::RegisterServiceCtrlHandler() {
    auto logger = LOG_THIS;
    service_status_handle_ = ::RegisterServiceCtrlHandlerExW(SERVICE_NAME, ServiceHandlerEx, nullptr);
    if (!service_status_handle_) {
        DWORD error = GetLastError();
        logger->fatal("Failed to register service control handler. Win32 Error: %lu", error);
        throw std::runtime_error("Failed to register service control handler");
    }
}

DWORD WINAPI Service::ServiceHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext) {
    // auto logger = LOG_THIS; // Careful with logger in this static context if not thread-safe or initialized
    switch (dwControl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            LAST_RESORT_LOGGER->info( "Service control STOP/SHUTDOWN received.");
            if (g_StopEvent) {
                SetEvent(g_StopEvent);
                if (WaitForSingleObject(g_ShutdownCompleteEvent, 10000) != WAIT_OBJECT_0) {
                    LAST_RESORT_LOGGER->warning( "ServiceHandlerEx: Shutdown did not complete in 10s, forcing exit.");
                    ExitProcess(1); // Force exit if clean shutdown fails
                }
            }
            break;
    }
    return NO_ERROR;
}

void Service::run(bool running_as_console) {
    auto logger = LOG_THIS;
    LAST_RESORT_LOGGER->always( "Service::run() starting\n");
    logger->always("%s starting. Version %s.%s.%s.%s\n", APP_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_FIXVERSION, VERSION_MINORFIXVERSION);
    LAST_RESORT_LOGGER->always( "Service::run() initial log level: %s\n", Logger::LOGLEVEL_ABBREVS[static_cast<int>(logger->getLogLevel())]);

    try {
        g_StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        g_ShutdownCompleteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!g_StopEvent || !g_ShutdownCompleteEvent) {
            DWORD error = GetLastError();
            logger->fatal("Failed to create global shutdown event handles. Win32 Error: %lu\n", error);
            LAST_RESORT_LOGGER->always( "Failed to create global shutdown event handles. Win32 Error: %lu\n", error);
            return; // Cannot proceed
        }

        if (!running_as_console) {
            RegisterServiceCtrlHandler();
        }
        logger->setFatalErrorHandler(Service::fatalErrorHandler);

        LAST_RESORT_LOGGER->info( "Service::run()> Loading setup file (if present)");
        try {
            Registry::loadSetupFile();
        } catch (const std::exception& e) {
            logger->fatal("Service::run()> Error loading setup file: %s", e.what());
            LAST_RESORT_LOGGER->always( "Service::run()> Error loading setup file: %s", e.what());
            throw; // Rethrow to be caught by outer try-catch for shutdown
        }
        config_.setUseLogAgent(true);

        LAST_RESORT_LOGGER->always( "Service::run()> Initializing file watcher (if configured)");
        // Initialize file watcher if configured
        try {
            LAST_RESORT_LOGGER->always("Service::run()> Initializing file watcher (if configured)");
            if (!config_.getTailFilename().empty()) {
                char program_name_buf[1024];
                char filename_buf[1024];
                Util::wstr2str(program_name_buf, sizeof(program_name_buf), config_.getTailProgramName().c_str());
                Util::wstr2str(filename_buf, sizeof(filename_buf), config_.getTailFilename().c_str());

                // Convert filename to wide string
                wstring wfilename;
                int wlen = MultiByteToWideChar(CP_UTF8, 0, filename_buf, -1, nullptr, 0);
                if (wlen > 0) {
                    vector<wchar_t> wbuf(wlen);
                    MultiByteToWideChar(CP_UTF8, 0, filename_buf, -1, wbuf.data(), wlen);
                    wfilename = wbuf.data();
                }

                string program_name = program_name_buf;
                if (program_name.length() == 0) {
                    logger->info("Service::run()> Starting file tail on %s\n",
                        filename_buf);
                }
                else {
                    logger->info("Service::run()> Starting file tail on %s for program %s\n",
                        filename_buf, program_name.c_str());
                }
                filewatcher_ = make_shared<FileWatcher>(
                    config_,
                    wfilename.c_str(),
                    config_.MAX_TAIL_FILE_LINE_LENGTH,
                    program_name.c_str(),
                    config_.getHostName().c_str(),
                    (config_.getSeverity() == SharedConstants::Severities::DYNAMIC
                        ? SharedConstants::Severities::NOTICE
                        : config_.getSeverity()),
                    config_.getFacility()
                );
            }
        }
        catch (const std::exception& e) {
            logger->fatal("Service::run()> Error initializing file watcher: %s\n", e.what());
            throw;
        }

        LAST_RESORT_LOGGER->debug( "Service::run()> Initializing network components\n");
        if (!initializeNetworkComponents()) {
            // Fatal error already logged by initializeNetworkComponents
            logger->fatal("Service::run()> Network component initialization failed. Service cannot start.\n");
            LAST_RESORT_LOGGER->always( "Service::run()> Network component initialization failed. Service cannot start.\n");
            shutdown_requested_ = true; // Signal for cleanup
        } else {
            LAST_RESORT_LOGGER->debug2( "Service::run()> Starting send thread");
            send_thread_ = make_unique<thread>(sendMessagesThread);

            LAST_RESORT_LOGGER->always( "Service::run()> Initializing event log subscriptions");
            initializeEventLogSubscriptions(config_.getLogs()); // Ensure this logs errors robustly
        }

        if (!shutdown_requested_) {
            LAST_RESORT_LOGGER->always( "Service::run()> Entering main loop");
            bool first_loop = true;
            int restart_needed = 0;
            mainLoop(running_as_console, first_loop, restart_needed);
        }

    } catch (const std::exception& e) {
        logger->fatal("Service::run()> Unhandled exception during service startup or main operation: %s\n", e.what());
        LAST_RESORT_LOGGER->always( "Service::run()> Unhandled exception: %s\n", e.what());
        shutdown_requested_ = true; // Ensure shutdown is triggered
    } catch (...) {
        logger->fatal("Service::run()> Unknown unhandled exception during service startup or main operation.\n");
        LAST_RESORT_LOGGER->always( "Service::run()> Unknown unhandled exception.\n");
        shutdown_requested_ = true;
    }

    LAST_RESORT_LOGGER->always( "Service::run()> Proceeding to cleanup and shutdown.\n");
    cleanupAndShutdown(running_as_console, 0); // Assuming restart_needed is not set by this point from main catch blocks
    LAST_RESORT_LOGGER->always( "Service::run() completed.\n");
}

// Helper to log suggestions for network initialization failures
void Service::logNetworkInitializationSuggestions(InitializeError error, bool is_primary) {
    auto logger = LOG_THIS;
    const char* target_type = is_primary ? "Primary" : "Secondary";

    switch (error) {
        case InitializeError::InvalidHost:
            logger->fatal("Suggestion (%s): The host name parsed from the URL is invalid or empty. Verify '%sHost' in registry.\n", target_type, target_type);
            break;
        case InitializeError::InvalidApiKey:
        case InitializeError::ApiKeyTooLong:
            logger->fatal("Suggestion (%s): The API key is null or too long. Verify '%sApiKey' in registry.\n", target_type, target_type);
            break;
        case InitializeError::InvalidUrl:
        case InitializeError::UrlTooLong:
        case InitializeError::WinHttpCrackUrlFailed:
            logger->fatal("Suggestion (%s): The URL is invalid, empty, too long, or could not be parsed. Verify '%sHost' in registry. Ensure it's a well-formed URL (e.g., http://server or https://server:port/path).\n", target_type, target_type);
            break;
        case InitializeError::WinHttpOpenFailed:
            logger->fatal("Suggestion (%s): WinHttpOpen (WinHTTP session creation) failed. Check Windows System Event Log for WinHTTP errors. Ensure the 'WinHTTP Web Proxy Auto-Discovery Service' is running if proxy auto-detection is used. Check system-wide proxy settings (netsh winhttp show proxy). Antivirus or firewall software could also interfere.\n", target_type);
            break;
         case InitializeError::WinHttpSetOptionFailed:
            logger->fatal("Suggestion (%s): Failed to set WinHTTP options (e.g., timeouts, TLS settings). Check previous logs for specific WinAPI errors.\n", target_type);
            break;
        case InitializeError::TlsConfigError: // Placeholder, more specific errors might be added to HttpNetworkClient
            logger->fatal("Suggestion (%s): A TLS/SSL configuration error occurred. Verify '%sUseTLS' and related certificate settings.\n", target_type, target_type);
            break;
        case InitializeError::AllocationFailed:
            logger->fatal("Suggestion (%s): Memory allocation failed during network client initialization. Check system RAM usage.\n", target_type);
            break;
        case InitializeError::UnknownError:
        default:
            logger->fatal("Suggestion (%s): An unknown or unspecified error occurred during network client initialization. Review previous log messages for details or Win32 error codes from WinHTTP.\n", target_type);
            break;
    }
}

// Helper to initialize a certificate for an HTTP client
bool Service::initializeCertificate(std::shared_ptr<HttpNetworkClient> client, const std::wstring& cert_filename_segment, bool is_primary) {
    auto logger = LOG_THIS;
    if (!client) {
        logger->fatal("initializeCertificate: HTTP client is null for %s certificate.\n", is_primary ? "primary" : "secondary");
        return false;
    }

    wchar_t cert_full_path[MAX_PATH] = {0};
    if (!Util::getThisPath(cert_full_path, MAX_PATH, true)) { // true = get module path
        logger->fatal("initializeCertificate: Failed to get agent's installation path for %s certificate '%ls'.\n", 
            is_primary ? "primary" : "secondary", cert_filename_segment.c_str());
        return false;
    }

    if (wcslen(cert_full_path) + cert_filename_segment.length() + 1 >= MAX_PATH) { // +1 for potential path separator if needed
        logger->fatal("initializeCertificate: Calculated path for %s certificate '%ls' is too long.\n", 
            is_primary ? "primary" : "secondary", cert_filename_segment.c_str());
        return false;
    }
    // Ensure path separator if not already present at end of module path
    if (cert_full_path[wcslen(cert_full_path) - 1] != L'\\' && cert_full_path[wcslen(cert_full_path) - 1] != L'/') {
        wcscat_s(cert_full_path, MAX_PATH, L"\\");
    }
    wcscat_s(cert_full_path, MAX_PATH, cert_filename_segment.c_str());

    logger->info("initializeCertificate: Attempting to load %s TLS certificate settings using path: %ls\n", 
        is_primary ? "primary" : "secondary", cert_full_path);

    if (!client->loadCertificate(cert_full_path)) {
        logger->fatal("initializeCertificate: HttpNetworkClient::loadCertificate call failed for %s certificate path '%ls'. Check client logs.\n",
                      is_primary ? "primary" : "secondary", cert_full_path);
        return false;
    }
    logger->info("initializeCertificate: Successfully processed/stored %s TLS certificate settings related to path: %ls\n", 
        is_primary ? "primary" : "secondary", cert_full_path);
    return true;
}

// Helper to get version and set in config
bool Service::getAndSetLogZillaVersion(const shared_ptr<INetworkClient>& client, bool is_primary) {
    auto logger = LOG_THIS;
    const char* target_desc = is_primary ? "primary" : "secondary";

    // Check if dynamic detection is actually required based on configuration
    if (is_primary) {
        if (!config_.isPrimaryDynamicDetectionRequired()) {
            logger->info("Dynamic version detection for the %s server is SKIPPED due to pre-configuration (e.g., registry setting). Format: %d, Version: %s", 
                target_desc, config_.getPrimaryLogformat(), "version_str_from_config_not_shown_here"); // TODO: get version string if needed for log
            return true; // Indicate success as no detection was needed or it was already handled.
        }
    } else { // Secondary
        if (!config_.isSecondaryDynamicDetectionRequired()) {
            logger->info("Dynamic version detection for the %s server is SKIPPED due to pre-configuration (e.g., registry setting). Format: %d, Version: %s", 
                target_desc, config_.getSecondaryLogformat(), "version_str_from_config_not_shown_here"); // TODO: get version string if needed for log
            return true;
        }
    }

    logger->info("Dynamic version detection IS required for %s server. Proceeding with detection.", target_desc);

    if (!client) {
        logger->fatal("getAndSetLogZillaVersion: Network client is null for %s server version check. Defaulting to HTTP format.", target_desc);
        if (is_primary) {
            config_.setPrimaryLogFormatToHttpDefaultOnError();
        } else {
            config_.setSecondaryLogFormatToHttpDefaultOnError();
        }
        return false;
    }

    char version_buffer[256]; 
    size_t bytes_written = 0;

    logger->info("Querying LogZilla version from %s server...", target_desc);
    if (!client->getLogzillaVersion(version_buffer, sizeof(version_buffer), bytes_written)) {
        logger->warning("Failed to retrieve LogZilla version from %s server. Defaulting to HTTP format. Check connectivity, server status, API key, and previous logs for network error details.", target_desc);
        if (is_primary) {
            config_.setPrimaryLogFormatToHttpDefaultOnError();
        } else {
            config_.setSecondaryLogFormatToHttpDefaultOnError();
        }
        return false;
    }

    if (bytes_written >= sizeof(version_buffer)) { 
        bytes_written = sizeof(version_buffer) - 1;
    }
    version_buffer[bytes_written] = '\0'; 

    logger->info("Received %s LogZilla version string: '%s' (%zu bytes)", target_desc, version_buffer, bytes_written);

    const char* version_str_to_parse = version_buffer;
    if (version_buffer[0] == 'v' || version_buffer[0] == 'V') {
        version_str_to_parse++; 
    }

    try {
        if (is_primary) {
            config_.setPrimaryLogzillaVersion(version_str_to_parse); // This will call the new setLogformatForVersion
        } else {
            config_.setSecondaryLogzillaVersion(version_str_to_parse); // This will call the new setLogformatForVersion
        }
    } catch (const std::exception& e) {
        logger->fatal("Error processing/setting %s LogZilla version ('%s'): %s. Defaulting to HTTP format.", target_desc, version_buffer, e.what());
        if (is_primary) {
            config_.setPrimaryLogFormatToHttpDefaultOnError();
        } else {
            config_.setSecondaryLogFormatToHttpDefaultOnError();
        }
        return false;
    }
    // At this point, setPrimaryLogzillaVersion/setSecondaryLogzillaVersion has run, which internally calls setLogformatForVersion.
    // The log format and version string in config_ are now definitively set (either from detection or keyword).
    logger->info("Successfully processed and set %s LogZilla version to: '%s'. Effective log format: %d", 
        target_desc, version_str_to_parse, 
        (is_primary ? config_.getPrimaryLogformat() : config_.getSecondaryLogformat()));
    return true;
}

bool Service::initializeNetworkComponentsInternal(bool is_primary_target) {
    auto logger = LOG_THIS;
    const char* target_desc = is_primary_target ? "Primary" : "Secondary";

    // --- Configuration Values ---
    bool use_self_signed_cert = is_primary_target ? config_.getPrimaryUseSelfSignedCert() : config_.getSecondaryUseSelfSignedCert();
    std::wstring host_url = (is_primary_target ? config_.getPrimaryHost() : config_.getSecondaryHost()) + SharedConstants::HTTP_API_PATH;
    if (!host_url.starts_with(L"http://") && !host_url.starts_with(L"https://")) {
        if (use_self_signed_cert) {
            host_url = L"https://" + host_url;
        } else {
            host_url = L"http://" + host_url;
        }
    }
    std::wstring api_key = is_primary_target ? config_.getPrimaryApiKey() : config_.getSecondaryApiKey();
    unsigned int configured_port = is_primary_target ? config_.getPrimaryPort() : config_.getSecondaryPort();

    shared_ptr<INetworkClient>& network_client_ref = is_primary_target ? primary_network_client_ : secondary_network_client_;
    shared_ptr<MessageBatcher>& batcher_ref = is_primary_target ? primary_batcher_ : secondary_batcher_;
    shared_ptr<MessageQueue>& queue_ref = is_primary_target ? primary_message_queue_ : secondary_message_queue_;

    // --- Initialize Queue (if not already done for primary) ---
    if (is_primary_target && !primary_message_queue_) { // Primary queue only
        try {
            primary_message_queue_ = make_shared<MessageQueue>(MESSAGE_QUEUE_SIZE, MESSAGE_BUFFERS_CHUNK_SIZE);
            logger->debug("Primary message queue initialized.\n");
        } catch (const std::exception& e) {
            logger->fatal("Failed to initialize primary message queue: %s\n", e.what());
            return false;
        }
    } else if (!is_primary_target && !secondary_message_queue_) { // Secondary queue only
         try {
            secondary_message_queue_ = make_shared<MessageQueue>(MESSAGE_QUEUE_SIZE, MESSAGE_BUFFERS_CHUNK_SIZE);
            logger->debug("Secondary message queue initialized.\n");
        } catch (const std::exception& e) {
            logger->fatal("Failed to initialize secondary message queue: %s\n", e.what());
            return false;
        }
    }

    bool use_ssl = (host_url.substr(0, 8) == L"https://");
    // --- Temporary Client for Version Check ---
    unsigned int version_check_port = use_ssl ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    logger->info("Initializing temporary %s client for version check. Target URL: '%ls', Effective Port for check: %u, self-signed cert for check: %d\n", 
        target_desc, host_url.c_str(), version_check_port, use_self_signed_cert);

    auto temp_client = make_shared<HttpNetworkClient>();
    InitializeError temp_init_error = temp_client->initialize(
        is_primary_target,
        &config_,
        api_key.c_str(),
        host_url.c_str(),
        version_check_port 
    );

    if (temp_init_error != InitializeError::Success) {
        logger->fatal("Failed to initialize temporary %s network client for version check. Error: %s. Target URL: '%ls', Port used: %u, self-signed cert: %d.\n",
                      target_desc, InitializeErrorToString(temp_init_error),
                      host_url.c_str(), version_check_port, use_self_signed_cert);
        logNetworkInitializationSuggestions(temp_init_error, is_primary_target);
        return false;
    }

    logger->debug("Temporary %s client for version check: connecting to %ls:%u...\n", target_desc, host_url.c_str(), version_check_port);
    if (!temp_client->connect()) {
        logger->fatal("Failed to connect temporary %s client for version check to %ls:%u. Check network, firewall, proxy, and server status. See client logs for Win32 errors.\n", 
            target_desc, host_url.c_str(), version_check_port);
        return false;
    }

    if (use_self_signed_cert) {
        if (!initializeCertificate(temp_client, 
            is_primary_target ? Configuration::PRIMARY_CERT_FILENAME : Configuration::SECONDARY_CERT_FILENAME, 
            is_primary_target)) {
            temp_client->close();
            return false; 
        }
    }

    logger->debug("Temporary %s client connected. Getting LogZilla version...\n", target_desc);
    bool detection_succeeded = getAndSetLogZillaVersion(temp_client, is_primary_target);

    if (detection_succeeded) {
        logger->info("LogZilla version and log format for %s server have been successfully established (either detected or pre-configured).", target_desc);
    } else {
        // getAndSetLogZillaVersion has already logged the specific error and called setPrimary/SecondaryLogFormatToHttpDefaultOnError.
        logger->warning("Dynamic version detection for %s server FAILED or was skipped and defaulted. The agent will proceed using the HTTP log format for this target.", target_desc);
        // IMPORTANT: This is no longer a fatal error for initialization. The config_ now has a default HTTP format set.
    }
    
    temp_client->close();
    temp_client.reset(); // Release temp client

    // Get the *now definitive* log format from config. It's either from user config, successful detection, or HTTP default on error.
    int current_log_format = is_primary_target ? config_.getPrimaryLogformat() : config_.getSecondaryLogformat();
    logger->info("Proceeding with %s server setup. Effective LogFormat after detection/defaulting: %s\n", 
        target_desc,
        (current_log_format == SharedConstants::LOGFORMAT_JSONPORT ? "JSONPORT" :
        (current_log_format == SharedConstants::LOGFORMAT_HTTPPORT ? "HTTPPORT" : "DETECT/ErrorState")));

    // --- Actual Client Initialization (using current_log_format) ---
    logger->info("Initializing actual %s network client. Target URL: '%ls', Configured Port: %u, Effective TLS: %d, LogFormat: %s\n",
                 target_desc, host_url.c_str(), configured_port, use_self_signed_cert, 
                 (current_log_format == SharedConstants::LOGFORMAT_JSONPORT ? "JSONPORT" : "HTTPPORT"));

    if (current_log_format == SharedConstants::LOGFORMAT_JSONPORT) {
        Util::UrlComponents urlComponents; // Now contains fixed character arrays instead of std::wstring

        if (!Util::ParseUrl(host_url.c_str(), urlComponents)) {
            logger->fatal("Failed to parse %s host URL '%ls' which is required for JSON client type.\n", target_desc, host_url.c_str());
            return false;
        }

        // Create persistent std::wstring objects directly from the fixed character arrays
        // This ensures all string memory is allocated within the same module
        std::wstring hostName(urlComponents.hostName);  // Persistent string for use in this function

        unsigned int json_port = configured_port;
        if (json_port < 1) {
            json_port = urlComponents.port;
        }
        if (json_port < 1) {
            if (!urlComponents.hasExplicitPort || json_port < 1) {
                json_port = SharedConstants::LZ_JSON_PORT;
            }
            logger->debug("%s port not explicitly configured for JSON, using parsed/default: %u\n", target_desc, json_port);
        }
        logger->debug("Creating %s JSON client for %ls:%u\n", target_desc, hostName.c_str(), json_port);
        
        // Use our persistent std::wstring with the client
        auto json_client = make_shared<JsonNetworkClient>(hostName, json_port);
        network_client_ref = json_client;
        batcher_ref = make_shared<JSONMessageBatcher>(config_.getMaxBatchCount(), config_.getMaxBatchAge());

        // Use our persistent string's c_str() method for the initialization
        InitializeError json_init_error = json_client->initialize(is_primary_target, &config_, api_key.c_str(), hostName.c_str(), json_port);
        if (json_init_error != InitializeError::Success) {
            logger->fatal("Failed to initialize actual %s JSON network client for %ls:%u. Check logs for specific errors.\n",
                         target_desc, hostName.c_str(), json_port);
            return false;
        }
        logger->info("%s JSON client initialized for %ls:%u\n", target_desc, hostName.c_str(), json_port);
    } else { // HTTP Client (or default if LOGFORMAT_DETECT was somehow still here, which it shouldn't be)
        auto http_client = make_shared<HttpNetworkClient>();
        network_client_ref = http_client;
        batcher_ref = make_shared<HTTPMessageBatcher>(config_.getMaxBatchCount(), config_.getMaxBatchAge());
        
        // For HTTP, the URL for initialize should be the base URL, and port from config or parsed.
        // The actual path for POST (/api/syslog/...) is usually part of HttpNetworkClient::post or configured there.
        InitializeError actual_init_error = http_client->initialize(
            is_primary_target,
            &config_,
            api_key.c_str(),
            host_url.c_str(), // Pass the full host URL
            configured_port // Pass configured port, HttpNetworkClient::initialize will derive if 0
        );

        if (actual_init_error != InitializeError::Success) {
            logger->fatal("Failed to initialize actual %s HTTP network client. Error: %s. Target URL: '%ls', Configured Port: %u, TLS: %d.\n",
                          target_desc, InitializeErrorToString(actual_init_error),
                          host_url.c_str(), configured_port, use_self_signed_cert);
            logNetworkInitializationSuggestions(actual_init_error, is_primary_target);
            return false;
        }
         logger->info("%s HTTP client initialized. Target: %ls:%u, Path: %ls\n", target_desc, 
            host_url.c_str(), configured_port, config_.getApiPath().c_str());
    }

    logger->debug("Actual %s client: connecting to configured target...\n", target_desc);
    if (!network_client_ref->connect()) {
        logger->fatal("Failed to connect actual %s network client. Check network, firewall, proxy, and server status. Client logs should show Win32 errors.\n", target_desc);
        return false;
    }

    if (use_self_signed_cert) {
        if (auto http_client_for_cert = std::dynamic_pointer_cast<HttpNetworkClient>(network_client_ref)) {
             if (!initializeCertificate(http_client_for_cert, 
                is_primary_target ? Configuration::PRIMARY_CERT_FILENAME : Configuration::SECONDARY_CERT_FILENAME, 
                is_primary_target)) {
                network_client_ref->close();
                return false; 
            }
        } else if (current_log_format == SharedConstants::LOGFORMAT_HTTPPORT) {
            // This case should ideally not be hit if dynamic_pointer_cast worked for HttpNetworkClient
            logger->warning("%s client is HTTP and uses TLS, but could not cast to HttpNetworkClient to load certificate. This is unexpected.\n", target_desc);
        }
        // JSON client is assumed to handle its own TLS certs if applicable through its own mechanisms or system store.
    }

    logger->info("Actual %s network client successfully initialized and connected.\n", target_desc);
    return true;
}

bool Service::initializeNetworkComponents() {
    auto logger = Logger::getLoggerByKey("Service"); // Replaced LOG_THIS
    if (!initializeNetworkComponentsInternal(true)) { // Initialize Primary
        return false;
    }
    if (config_.hasSecondaryHost()) {
        if (!initializeNetworkComponentsInternal(false)) { // Initialize Secondary
            // If secondary fails, we might still want primary to run depending on policy.
            // For now, failing secondary also fails overall initialization.
            logger->fatal("Secondary network component initialization failed. Aborting service startup.\n");
            if (primary_network_client_) primary_network_client_->close(); // Ensure primary is closed if secondary fails
            return false;
        }
    }
    return true;
}

void Service::incrementHandledEventCount(uint64_t count) {
    handled_event_count_.fetch_add(count, std::memory_order_relaxed);
}

void Service::initializeEventLogSubscriptions(const vector<LogConfiguration>& logs) {
    auto logger = LOG_THIS;
    logger->info("Initializing event log subscriptions. Number of logs configured: %zu\n", logs.size());
    subscriptions_.clear(); // Clear any previous subscriptions
    try {
        subscriptions_.reserve(logs.size());
        for (const auto& log_config : logs) {
            if (log_config.name_.empty() || log_config.channel_.empty()) {
                logger->recoverable_error("Invalid event log configuration: Log name ('%ls') or channel ('%ls') is empty. Skipping this log.\n", 
                    log_config.name_.c_str(), log_config.channel_.c_str());
                continue;
            }

            char log_name_utf8[1024];
            if (Util::wstr2str(log_name_utf8, sizeof(log_name_utf8), log_config.name_.c_str()) == 0) {
                logger->recoverable_error("Failed to convert log name '%ls' to UTF-8. Skipping this log.\n", log_config.name_.c_str());
                continue;
            }

            wstring bookmark = Registry::readBookmark(log_config.channel_.c_str());
            if (bookmark.empty() && !config_.getOnlyWhileRunning()) {
                bookmark = log_config.bookmark_;
                logger->debug("Using configured bookmark for channel '%ls' as registry one is empty and not OnlyWhileRunning.\n", log_config.channel_.c_str());
            } else if (!bookmark.empty()) {
                logger->debug("Using bookmark from registry for channel '%ls'.\n", log_config.channel_.c_str());
            } else {
                 logger->debug("No bookmark found for channel '%ls', will read from beginning or current based on OnlyWhileRunning.\n", log_config.channel_.c_str());
            }

            const wstring query_string(L"*"); // Basic query, can be made more complex if needed

            logger->info("Creating event log subscription for: LogName='%ls', Channel='%ls', BookmarkPresent=%s, OnlyWhileRunning=%d\n",
                         log_config.name_.c_str(), log_config.channel_.c_str(), (bookmark.empty() ? "No" : "Yes"), config_.getOnlyWhileRunning());
            
            try {
                subscriptions_.emplace_back(
                    log_config.name_, 
                    log_config.channel_,
                    query_string, 
                    bookmark,
                    config_.getOnlyWhileRunning(),
                    make_unique<EventHandlerMessageQueuer>(
                        config_, 
                        primary_message_queue_, 
                        secondary_message_queue_, 
                        log_config.name_.c_str() // Pass log name for context in queuer
                    )
                );

                // Attempt to subscribe immediately
                subscriptions_.back().subscribe(bookmark, config_.getOnlyWhileRunning());
                logger->info("Successfully subscribed to event log: %s (Channel: %ls)\n", log_name_utf8, log_config.channel_.c_str());
            }
            catch (const std::exception& sub_ex) {
                logger->recoverable_error("Failed to subscribe to %ls (%ls): %s\n", 
                    log_config.name_.c_str(), log_config.channel_.c_str(), sub_ex.what());
                // If emplace_back succeeded but subscribe failed, it might leave a partially constructed object.
                // Consider popping it back if subscribe is critical for its existence in the vector.
                if (!subscriptions_.empty() && subscriptions_.back().getName() == log_config.name_) { // Check by name
                    // subscriptions_.pop_back(); // Or handle this state appropriately.
                }
            }
        }
    } catch (const std::exception& e) {
        logger->fatal("Critical error during event log subscription setup: %s. Event logging may be impaired.\n", e.what());
        // Depending on policy, might rethrow or just log and continue with potentially no event logs.
    }
    logger->info("Event log subscription initialization complete. %zu subscriptions active.\n", subscriptions_.size());
}

void Service::handleQueueStatusAndConfig() {
    auto logger = LOG_THIS;
    if (primary_message_queue_ && primary_message_queue_->length() > 0) {
        logger->debug3("Primary Queue length: %zu\n", primary_message_queue_->length());
    }
    if (secondary_message_queue_ && secondary_message_queue_->length() > 0) {
        logger->debug3("Secondary Queue length: %zu\n", secondary_message_queue_->length());
    }

    if (config_.getUseLogAgent()) {
        bool primary_empty = (!primary_message_queue_ || primary_message_queue_->isEmpty());
        bool secondary_empty_or_null = (!secondary_message_queue_ || secondary_message_queue_->isEmpty());
        if (primary_empty && secondary_empty_or_null) {
            // logger->debug3("Queues are empty, saving config to registry (bookmarks handled by subscriptions).\n");
            // Config saving might be better tied to specific config changes, not queue status.
            // config_.saveToRegistry(); // Bookmarks are saved by subscriptions periodically.
        }
    }
}

static void LogDetailedMemoryStats() {
    auto logger = LOG_THIS;
    PROCESS_MEMORY_COUNTERS_EX pmc = {0};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        logger->debug("MEMORY_STATS|WorkingSet=%llu KB|PrivateUsage=%llu KB|PagefileUsage=%llu KB|PeakWorkingSet=%llu KB\n", 
            pmc.WorkingSetSize/1024, pmc.PrivateUsage/1024, pmc.PagefileUsage/1024, pmc.PeakWorkingSetSize/1024);
    }
}

static void LogHandleUsage() {
    auto logger = LOG_THIS;
    DWORD handleCount = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handleCount)) {
        logger->debug("HANDLES|Count=%u\n", handleCount);
    }
}

void Service::mainLoop(bool running_as_console, bool& first_loop, int& restart_needed_out)
{
    auto logger = LOG_THIS;
    logger->info("Service::mainLoop()> Entering main service loop.\n");

    int loop_count = 0;
    auto lastMemoryCheck = std::chrono::steady_clock::now();
    const std::chrono::seconds memoryCheckInterval(10); // Changed from 15 to 10
    const std::chrono::milliseconds mainLoopSleep(100);
    const int bookmarkSaveIntervalLoops = 10; // Approx every 1 second (10 * 100ms)
    const int heartbeatLoopCount = 100; // Approx every 10 seconds

    while (!checkForShutdown(running_as_console, restart_needed_out)) {
        try {
            if (first_loop) {
                first_loop = false;
                logger->always("Service::mainLoop()> Service is now running and processing.\n");
                 WindowsService::ReportStatus(SERVICE_RUNNING, NO_ERROR, 0); // Report running if as service
            }

            handleQueueStatusAndConfig();
            Sleep(mainLoopSleep.count());

            if (++loop_count % bookmarkSaveIntervalLoops == 0) {
                for (auto& subscription : subscriptions_) {
                    subscription.incrementedSaveBookmark(); 
                }
            }
            if (loop_count >= heartbeatLoopCount) {
                logger->debug2("Service::mainLoop()> Heartbeat (%d loops completed).\n", heartbeatLoopCount);
                loop_count = 0;
            }
            
            auto currentTime = std::chrono::steady_clock::now();
            if (currentTime - lastMemoryCheck >= memoryCheckInterval) {
                LogDetailedMemoryStats();
                LogHandleUsage();

                // Log event handling statistics
                uint64_t successful_events = EventLogSubscription::getSuccessfullyHandledEventsCount();
                uint64_t unsuccessful_events = EventLogSubscription::getUnsuccessfullyHandledEventsCount();
                logger->debug("EVENT_STATS|CumulativeSuccessful=%llu|CumulativeUnsuccessful=%llu\n", successful_events, unsuccessful_events);

                lastMemoryCheck = currentTime;
            }

        } catch (const std::exception& e) {
            logger->recoverable_error("Service::mainLoop()> Recoverable exception: %s\n", e.what());
        } catch (...) {
            logger->recoverable_error("Service::mainLoop()> Unknown recoverable exception caught in file %s at line %d\n", __FILE__, __LINE__);
        }
    }
    logger->info("Service::mainLoop()> Exited main loop. Shutdown requested: %s, Restart needed: %d\n", 
        (shutdown_requested_ ? "Yes" : "No"), restart_needed_out);
}

bool Service::checkForShutdown(bool running_as_console, int& restart_needed_flag) {
    // Check for global shutdown request first (e.g. from fatalErrorHandler or service control)
    if (g_StopEvent && WaitForSingleObject(g_StopEvent, 0) == WAIT_OBJECT_0) {
        if (!shutdown_requested_) { // Log only on first detection via event
            LAST_RESORT_LOGGER->always( "checkForShutdown: g_StopEvent signaled. Initiating shutdown.\n");
        }
        shutdown_requested_ = true;
        service_shutdown_requested_ = true; // Assume if g_StopEvent is set, it's a service stop or similar
        return true;
    }

    if (shutdown_requested_) { // Already requested by other means
        return true;
    }

    // Check for console key press if running in console mode
    if (running_as_console && _kbhit()) {
        char ch = _getch();
        LAST_RESORT_LOGGER->always( "checkForShutdown: Console key '%c' hit. Initiating shutdown.\n", ch);
        shutdown_requested_ = true;
        return true;
    }

    // Check for restart flag (placeholder, actual restart logic would be external or via service controller)
    if (restart_needed_flag) { // This flag would be set by some other mechanism if a restart is required
        LAST_RESORT_LOGGER->always( "checkForShutdown: Restart needed flag is set. Initiating shutdown for restart.\n");
        shutdown_requested_ = true;
        return true;
    }

    return false; 
}

static void LogShutdownException(DWORD exceptionCode) {
    // This is a last resort, C-style logger for crashes during critical phases.
    HANDLE hFile = CreateFileA("syslogagent_crash.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buffer[1024];
        time_t now_time = time(NULL);
        struct tm tm_info;
        localtime_s(&tm_info, &now_time);
        int len = sprintf_s(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d] EXCEPTION DURING SHUTDOWN: Code=0x%08X\r\n", 
                        tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday, 
                        tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
                        exceptionCode);
        if (len > 0) { WriteFile(hFile, buffer, len, NULL, NULL); }
        CloseHandle(hFile);
    }
}

static void LogNormalExit() {
    HANDLE hFile = CreateFileA("syslogagent_exit.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        char buffer[1024];
        time_t now_time = time(NULL);
        struct tm tm_info;
        localtime_s(&tm_info, &now_time);
        int len = sprintf_s(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d] APPLICATION EXIT: Normal shutdown sequence completed.\r\n", 
                     tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday, 
                     tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
        if (len > 0) { WriteFile(hFile, buffer, len, NULL, NULL); }
        CloseHandle(hFile);
    }
}


void Service::cleanupAndShutdown(bool running_as_console, int restart_needed) {
    auto logger = Logger::getLoggerByKey("Service"); // Replaced LOG_THIS
    LAST_RESORT_LOGGER->always( "cleanupAndShutdown: Starting. Restart needed: %d\n", restart_needed);
    logger->always("Service cleanup and shutdown initiated...\n");

    // Report SERVICE_STOP_PENDING if running as a service and not already stopped/stopping
    if (!running_as_console && service_status_handle_ != nullptr) {
        WindowsService::ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 30000); // Give 30s hint
    }

    __try {
        // 1. Signal queues to stop accepting new messages and wake up sender.
        logger->debug("cleanupAndShutdown: Signaling message queues to begin shutdown.\n");
        if (primary_message_queue_) primary_message_queue_->beginShutdown();
        if (secondary_message_queue_) secondary_message_queue_->beginShutdown();
        if (sender_) sender_->requestStopAndNotify();

        // 2. Stop event log subscriptions from generating new events.
        logger->debug("cleanupAndShutdown: Cancelling %zu event log subscriptions and saving bookmarks.\n", subscriptions_.size());
        for (auto& sub : subscriptions_) {
            sub.saveBookmark(); // Save final bookmark before cancelling
            sub.cancelSubscription();
        }
        subscriptions_.clear();
        logger->debug("cleanupAndShutdown: Event log subscriptions cleared.\n");

        // 3. Stop file watcher if active.
        if (filewatcher_) {
            logger->debug("cleanupAndShutdown: Stopping file watcher.\n");
            filewatcher_.reset(); 
        }

        // 4. Wait for the sender thread to finish processing remaining messages.
        if (send_thread_ && send_thread_->joinable()) {
            logger->info("cleanupAndShutdown: Waiting for message sending thread to complete...\n");
            if (!send_thread_->joinable()) { // Re-check after notify
                 logger->warning("cleanupAndShutdown: Send thread no longer joinable before wait.\n");
            } else {
                send_thread_->join(); // This blocks until sender_->run() exits
                logger->info("cleanupAndShutdown: Message sending thread has completed.\n");
            }
        }
        sender_.reset(); // Release sender unique_ptr

        // 5. Close network connections (clients).
        logger->debug("cleanupAndShutdown: Closing network clients.\n");
        cleanupNetworkClient(primary_network_client_);
        cleanupNetworkClient(secondary_network_client_);

        // 6. Clear message queues (should be empty if sender processed all).
        logger->debug("cleanupAndShutdown: Clearing message queues.\n");
        cleanupMessageQueue(primary_message_queue_);
        cleanupMessageQueue(secondary_message_queue_);

        // 7. Signal that the service's core operations are shut down.
        if (g_ShutdownCompleteEvent) {
            logger->debug("cleanupAndShutdown: Signaling g_ShutdownCompleteEvent.\n");
            SetEvent(g_ShutdownCompleteEvent);
        }

        // 8. Final service status update if running as a service.
        if (!running_as_console && service_status_handle_ != nullptr) {
            logger->info("cleanupAndShutdown: Reporting SERVICE_STOPPED.\n");
            WindowsService::ReportStatus(SERVICE_STOPPED, restart_needed ? ERROR_SUCCESS_REBOOT_REQUIRED : NO_ERROR, 0);
        }

        logger->always("Service cleanup and shutdown completed successfully.\n");
        LAST_RESORT_LOGGER->always( "cleanupAndShutdown: Completed successfully.\n");
        LogNormalExit();

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // DWORD exception_code = _exception_code();  // intrinsic gives same value
        DWORD exception_code = ::_exception_code();
        LAST_RESORT_LOGGER->always( "cleanupAndShutdown: CRITICAL EXCEPTION (Code: 0x%08X) during cleanup and shutdown process! Forcing exit.\n", exception_code);
        LAST_RESORT_LOGGER->always( "cleanupAndShutdown: CRITICAL EXCEPTION (Code: 0x%08X) during cleanup! Forcing exit.\n", exception_code);
        LogShutdownException(exception_code);
        // If critical error during shutdown, may need to terminate more abruptly.
        if (g_ShutdownCompleteEvent) SetEvent(g_ShutdownCompleteEvent); // Still signal for any waiters
        ExitProcess(2); // Non-zero exit code indicates error during shutdown
    }
    
    // Cleanup global event handles outside SEH if possible
    if (g_StopEvent) { CloseHandle(g_StopEvent); g_StopEvent = nullptr; }
    if (g_ShutdownCompleteEvent) { CloseHandle(g_ShutdownCompleteEvent); g_ShutdownCompleteEvent = nullptr; }
}

void Service::shutdown() {
    auto logger = LOG_THIS;
    logger->always("Service::shutdown() called externally. Initiating graceful shutdown.\n");
    service_shutdown_requested_ = true; // Indicate it's a service control type shutdown
    shutdown_requested_ = true;
    if (g_StopEvent) {
        SetEvent(g_StopEvent); // Signal the main loop and service handler
    }
    shutdown_event_.signal(); // For any other internal waiters if used (currently less used with g_StopEvent)
}

void Service::fatalErrorHandler(const char* msg) {
    // This handler should be very careful about what it does, as it might be called in a bad state.
    // Using LAST_RESORT_LOGGER is safer here.
    LAST_RESORT_LOGGER->always( "FATAL ERROR HANDLER TRIGGERED: %s\n", (msg ? msg : "Unknown error"));

    bool already_handling = fatal_shutdown_in_progress.exchange(true);
    if (already_handling) {
        LAST_RESORT_LOGGER->always( "Fatal error handler re-entered, ignoring subsequent call.\n");
        return;
    }

    // Report service status as stopping if running as a service and we have a handle.
    if (!service_shutdown_requested_ && service_status_handle_ != nullptr) {
        LAST_RESORT_LOGGER->always( "Fatal error: Reporting SERVICE_STOP_PENDING due to fatal error.\n");
        WindowsService::ReportStatus(SERVICE_STOP_PENDING, ERROR_SERVICE_SPECIFIC_ERROR, 5000); // Short hint
    }
    
    // Signal all shutdown mechanisms.
    shutdown_requested_ = true;
    if (g_StopEvent) {
        LAST_RESORT_LOGGER->always( "Fatal error: Setting g_StopEvent.\n");
        SetEvent(g_StopEvent);
    }
    shutdown_event_.signal(); // Legacy/internal event

    // Optional: If the send_thread_ is stuck, a more forceful stop might be considered,
    // but this is risky. For now, rely on requestStopAndNotify() and join() timeout in cleanup.
    // If sender_ is stuck in a blocking network call, join() will wait.
    // A more robust solution might involve interrupting the network client from another thread if possible.

    LAST_RESORT_LOGGER->always( "Fatal error handler: Shutdown signaled. Main loop should terminate and cleanupAndShutdown will run.\n");
}

} // namespace Syslog_agent

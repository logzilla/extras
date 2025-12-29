#include "pch.h"

/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

#include "../Agent/stdafx.h"
#include "timezoneapi.h"
#include "../Infrastructure/Logger.h"
#include "Configuration.h"
#include "RecordNumber.h"
#include "Registry.h"
#include "SyslogAgentSharedConstants.h"
#include "../Infrastructure/Util.h"
#include "../Infrastructure/WindowsEventLog.h"
#include "LogConfiguration.h"
#include <stdexcept>
#include <ShlObj.h>
#include <string>
#include <set>
#include <vector>
#include <sstream>
#include <debugapi.h>
#include <algorithm>
#include <cctype>
#include <winsock2.h> // For Winsock functions
#include <ws2tcpip.h> // For getaddrinfo, inet_ntop, etc.
#pragma comment(lib, "Ws2_32.lib") // Link against Ws2_32.lib

// Logger is defined in the global namespace, not in Syslog_agent

using namespace Syslog_agent;

// Local implementation of wstr2str to resolve compiler errors
// This is a temporary solution until project dependencies are properly updated
namespace {
    size_t local_wstr2str(char* dest, size_t dest_size, const wchar_t* src) {
        if (!dest || !src || dest_size == 0) return 0;

        size_t converted = 0;
        if (WideCharToMultiByte(CP_UTF8, 0, src, -1, dest, static_cast<int>(dest_size), NULL, NULL)) {
            converted = strlen(dest);
        }
        dest[dest_size - 1] = '\0';  // Ensure null termination
        return converted;
    }
}

constexpr int REG_BUFFER_LEN = 2048;

int Configuration::debug_level_setting_ = ::Logger::NONE;
int Configuration::event_log_poll_interval_ = SharedConstants::Defaults::POLL_INTERVAL_SEC;

// Add definitions for static accessors
int Configuration::getDebugLevelSetting() {
    return debug_level_setting_;
}

int Configuration::getEventLogPollInterval() {
    return event_log_poll_interval_;
}

Configuration::Configuration() :
    primary_logformat_(SharedConstants::LOGFORMAT_DETECT),
    secondary_logformat_(SharedConstants::LOGFORMAT_DETECT),
    primary_logzilla_version_(SharedConstants::Defaults::VERSION_DETECT_STR),
    secondary_logzilla_version_(SharedConstants::Defaults::VERSION_DETECT_STR)
{
    owned_registry_ = std::make_unique<Registry>(); // Create and own registry
    registry_p_ = owned_registry_.get(); // Point to the owned registry
    registry_p_->open(); // Open the default production key
    getTimeZoneOffset();
    setHostName();
}

Configuration::Configuration(Registry& registry) :
    registry_p_(&registry), // Point to the provided registry
    primary_logformat_(SharedConstants::LOGFORMAT_DETECT),
    secondary_logformat_(SharedConstants::LOGFORMAT_DETECT),
    primary_logzilla_version_(SharedConstants::Defaults::VERSION_DETECT_STR),
    secondary_logzilla_version_(SharedConstants::Defaults::VERSION_DETECT_STR)
{
    setHostName();
    getTimeZoneOffset();
}

// Add the new constructor implementation
Configuration::Configuration(const std::wstring& config_file_path, Registry& registry) :
    registry_p_(&registry), // Point to the provided registry
    primary_logformat_(SharedConstants::LOGFORMAT_DETECT),
    secondary_logformat_(SharedConstants::LOGFORMAT_DETECT),
    primary_logzilla_version_(SharedConstants::Defaults::VERSION_DETECT_STR),
    secondary_logzilla_version_(SharedConstants::Defaults::VERSION_DETECT_STR)
{
    // config_file_path is currently unused, but available for future implementation
    // For now, behave like the Registry-only constructor
    setHostName();
    getTimeZoneOffset();
    // Potentially, load settings from config_file_path here in the future
}

bool Configuration::hasSecondaryHost() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // Check forwarding enabled AND host is not empty AND contains non-whitespace
    return forward_to_secondary_ && 
           !secondary_host_.empty() && 
           secondary_host_.find_first_not_of(L" \t\n\v\f\r") != std::wstring::npos;
}

void Configuration::loadFromRegistry(bool running_from_console, bool override_log_level,
    ::Logger::LogLevel override_log_level_setting) {
    auto logger = ::Logger::getLoggerByKey("Configuration");
    // Use unique_lock for write operations
    std::unique_lock lock(mutex_);

    ::Logger::getLoggerByKey(::Logger::LAST_RESORT_LOGGER_NAME)->always("Configuration::loadFromRegistry() using provided registry\n");

    if (override_log_level) {
        debug_level_setting_ = override_log_level_setting;
    }
    else {
        debug_level_setting_ = registry_p_->readInt(SharedConstants::RegistryKey::DEBUG_LEVEL_SETTING, 0);
    }

    debug_log_file_ = registry_p_->readString(SharedConstants::RegistryKey::DEBUG_LOG_FILE, L"");

    // Handle both absolute and relative paths for the debug log file
    if (!debug_log_file_.empty()) {
        // Check if the path starts with a drive letter (e.g., C:), a UNC path (\\server), or a single leading backslash
        bool is_absolute_path = false;

        // Check for drive letter (contains ':')
        if (debug_log_file_.find(L':') != std::wstring::npos) {
            is_absolute_path = true;
        }
        // Check for UNC path (starts with "\\")
        else if (debug_log_file_.size() >= 2 && debug_log_file_[0] == L'\\' && debug_log_file_[1] == L'\\') {
            is_absolute_path = true;
        }
        // Check for a single leading backslash (assume it's from the root of C:)
        else if (debug_log_file_.size() >= 1 && debug_log_file_[0] == L'\\') {
            debug_log_file_ = L"C:" + debug_log_file_;
            is_absolute_path = true;
        }

        if (is_absolute_path) {
             logger->setLogFileW(debug_log_file_);
        }
        else {
            wchar_t pathBuffer[MAX_PATH];
            if (Util::getThisPath(pathBuffer, MAX_PATH, true)) {
                std::wstring fullPath = pathBuffer; // Construct wstring from buffer
                fullPath += debug_log_file_;        // Append the log file name
                logger->setLogFileW(fullPath);
            } else {
                // Handle error: Util::getThisPath failed.
                // Log an error or use a default path
                auto lastResortLogger = ::Logger::getLoggerByKey(::Logger::LAST_RESORT_LOGGER_NAME);
                if (lastResortLogger) {
                    lastResortLogger->critical("Failed to get current path for debug log file configuration.");
                }
            }
        }
    }

    if (debug_level_setting_ != (int)::Logger::NONE) {
        if (debug_log_file_.length() > 0) {
            logger->setLogDestination(running_from_console ?
                ::Logger::DEST_CONSOLE_AND_FILE : ::Logger::DEST_FILE);
        }
        else {
            logger->setLogDestination(::Logger::DEST_CONSOLE);
        }
        logger->setLogLevel((::Logger::LogLevel)debug_level_setting_);
        if ((::Logger::LogLevel)debug_level_setting_ == ::Logger::DEBUG3) {
            if (::Logger::getLoggerByKey(::Logger::LAST_RESORT_LOGGER_NAME)->getLogDestination() == ::Logger::DEST_NONE) {
                logger_holder_ = std::make_shared<::Logger>(::Logger::LAST_RESORT_LOGGER_NAME);
                std::string logFilePath = Util::getAppropriateLogPath("syslogagent_failsafe.log");
                logger_holder_->setLogFile(logFilePath.c_str());
                logger_holder_->setLogDestination(::Logger::DEST_FILE);
                logger_holder_->setCloseAfterWrite(true);
                ::Logger::setLogger(logger_holder_, { ::Logger::LAST_RESORT_LOGGER_NAME });
                // Log the location of the last resort log file to the Windows Event Log
                WindowsEventLog eventLog;
                eventLog.WriteEvent(
                    WindowsEventLog::EventType::INFORMATION_EVENT,
                    1000,  // Event ID
                    "LogZilla SyslogAgent started",
                    ("Last resort log file is located at: " + logFilePath).c_str());
            }
        }
    }
    else {
        logger->setLogLevel(::Logger::NONE);
    }
    // Handle legacy only_while_running_ string conversion
    try {
        only_while_running_ = registry_p_->readBool(SharedConstants::RegistryKey::ONLY_WHILE_RUNNING, false);
    }
    catch (const std::exception&) {
        try {
            wstring bad_reg_string = registry_p_->readString(SharedConstants::RegistryKey::ONLY_WHILE_RUNNING, L"");
            wstring lower_str = Util::toLowercase(bad_reg_string);
            only_while_running_ = (lower_str == L"true")
                || (lower_str == L"yes")
                || (bad_reg_string == L"1");
        }
        catch (const std::exception&) {
            only_while_running_ = false;
        }
    }

    api_path_ = SharedConstants::HTTP_API_PATH;
    event_log_poll_interval_ = registry_p_->readInt(
        SharedConstants::RegistryKey::EVENT_LOG_POLL_INTERVAL,
        SharedConstants::Defaults::POLL_INTERVAL_SEC);

    if (event_log_poll_interval_ == 0) {
        event_log_poll_interval_ = SharedConstants::Defaults::POLL_INTERVAL_SEC;
    }

    primary_host_ = registry_p_->readString(SharedConstants::RegistryKey::PRIMARY_HOST, L"localhost");
    primary_api_key_ = registry_p_->readString(SharedConstants::RegistryKey::PRIMARY_API_KEY, L"");
    logger->debug2("Configuration::loadFromRegistry() primary api key: %ls\n", primary_api_key_.c_str());

    // Try to read primary port from registry
    try {
        primary_port_ = registry_p_->readInt(SharedConstants::RegistryKey::PRIMARY_PORT, 0);
        if (primary_port_ > 0) {
            logger->debug2("Configuration::loadFromRegistry() primary port from registry: %d\n", primary_port_);
        }
    }
    catch (const std::exception&) {
        primary_port_ = 0;  // Will be determined by format and TLS
    }

    // Try to read secondary port from registry
    try {
        secondary_port_ = registry_p_->readInt(SharedConstants::RegistryKey::SECONDARY_PORT, 0);
        if (secondary_port_ > 0) {
            logger->debug2("Configuration::loadFromRegistry() secondary port from registry: %d\n", secondary_port_);
        }
    }
    catch (const std::exception&) {
        secondary_port_ = 0; // Will be determined by format and TLS
    }

    try {
        primary_use_self_signed_cert_ = registry_p_->readBool(SharedConstants::RegistryKey::PRIMARY_USE_SELF_SIGNED_CERT, false); // CORRECTED DEFAULT ARG
    }
    catch (const std::exception&) {
        primary_use_self_signed_cert_ = false; // Default if readBool throws
    }

    secondary_host_ = registry_p_->readString(SharedConstants::RegistryKey::SECONDARY_HOST, L"");

    secondary_api_key_ = registry_p_->readString(SharedConstants::RegistryKey::SECONDARY_API_KEY, L"");
    logger->debug2("Configuration::loadFromRegistry() secondary api key: %ls\n", secondary_api_key_.c_str());
    try {
        secondary_use_self_signed_cert_ = registry_p_->readBool(SharedConstants::RegistryKey::SECONDARY_USE_SELF_SIGNED_CERT, false); // CORRECTED DEFAULT ARG
    }
    catch (const std::exception&) {
        secondary_use_self_signed_cert_ = false; // Default if readBool throws
    }

    // Try reading forward_to_secondary_ from the registry
    try {
        forward_to_secondary_ = registry_p_->readBool(SharedConstants::RegistryKey::FORWARD_TO_SECONDARY, false); // CORRECTED DEFAULT ARG
    }
    catch (const std::exception&) {
        // If readBool fails (e.g., key not found or wrong type), fall back to default (true)
        forward_to_secondary_ = false; // Default if readBool throws
    }

    lookup_accounts_ = registry_p_->readBool(SharedConstants::RegistryKey::LOOKUP_ACCOUNTS, true);
    facility_ = registry_p_->readInt(SharedConstants::RegistryKey::FACILITY, SharedConstants::Defaults::FACILITY /* 16 local0 */); // CORRECTED DEFAULT
    severity_ = registry_p_->readInt(SharedConstants::RegistryKey::SEVERITY, SharedConstants::Defaults::SEVERITY /* 6 Informational */); // CORRECTED DEFAULT
    max_batch_size_ = registry_p_->readInt(SharedConstants::RegistryKey::MAX_BATCH_SIZE, 500);
    max_batch_age_ = registry_p_->readInt(SharedConstants::RegistryKey::MAX_BATCH_AGE, 2000); // milliseconds

    // If max batch size is zero, set to default 500
    if (max_batch_size_ == 0) {
        max_batch_size_ = 500;
    }

    // If max batch age is zero, set to default 2000ms
    if (max_batch_age_ == 0) {
        max_batch_age_ = 2000;
    }

    suffix_ = registry_p_->readString(SharedConstants::RegistryKey::SUFFIX, L"");

    // Read event ID filter and ignore setting
    include_vs_ignore_eventids_ = registry_p_->readBool(SharedConstants::RegistryKey::INCLUDE_VS_IGNORE_EVENT_IDS, true); 
    wstring filterIdsValue = registry_p_->readString(SharedConstants::RegistryKey::EVENT_ID_FILTER, L"");
    loadFilterIds(filterIdsValue);

    // Read channels AFTER basic settings are loaded, so logging is configured
    try {
        channels_ = registry_p_->readChannels();
        
        // Log success with channel count
        logger->info("Read %zu channels from registry\n", channels_.size());
        
        // Log each channel at debug level
        for (const auto& channel : channels_) {
            logger->debug2("  - Channel: %ls\n", channel.c_str()); 
        }
        
        // Check if we have at least one channel, if channels are required
        if (channels_.empty()) {
            logger->fatal("No channels found in registry. SyslogAgent requires at least one channel to function.\n");
            throw std::runtime_error("No channels available - agent cannot start\n");
        }
    }
    catch (const std::exception& e) {
        // This is now a fatal error that will terminate the application
        logger->fatal("Failed to read channels from registry: %s\n", e.what());
        throw; // Re-throw the exception to propagate it upward and terminate
    }

    // After loading channels_ from the registry
    logs_.clear();
    logs_.reserve(channels_.size());

    for (const auto& channel : channels_) {
        logs_.push_back(LogConfiguration());
        logs_.back().channel_ = channel;
        // Set the name_ field to be the same as channel_ initially
        logs_.back().name_ = channel;
        char channel_buf[1024];
        local_wstr2str(channel_buf, sizeof(channel_buf), channel.c_str());
        logs_.back().nname_ = channel_buf;
        logs_.back().loadFromRegistry(*registry_p_); // Use registry pointer

        if (!logs_.back().enabled_) {
            logger->info("Configuration::loadFromRegistry() skipping disabled channel %ls", channel.c_str());
            logs_.pop_back();
            continue;
        }

        logger->debug("Configuration::loadFromRegistry() event log %ls", channel.c_str());
    }

    // Load backwards compatibility settings
    std::wstring default_version_detect_ws = L"detect"; // Default for readString if key is missing
    std::string primary_compat_setting_str;
    std::wstring ws_primary_compat_setting = registry_p_->readString(SharedConstants::RegistryKey::PRIMARY_BACKWARDS_COMPAT_VER, default_version_detect_ws.c_str());
    if (!ws_primary_compat_setting.empty()) {
        char primary_buffer[256]; // Max length for version string + keywords
        local_wstr2str(primary_buffer, sizeof(primary_buffer), ws_primary_compat_setting.c_str());
        primary_compat_setting_str = primary_buffer;
    } else {
        primary_compat_setting_str = SharedConstants::Defaults::VERSION_DETECT_STR; // Fallback if readString returns empty for some reason
    }
    logger->debug("Primary backwards compatible version setting from registry: '%s'", primary_compat_setting_str.c_str());
    setLogformatForVersion(primary_compat_setting_str, primary_logformat_, primary_logzilla_version_, true);

    std::string secondary_compat_setting_str;
    std::wstring ws_secondary_compat_setting = registry_p_->readString(SharedConstants::RegistryKey::SECONDARY_BACKWARDS_COMPAT_VER, default_version_detect_ws.c_str());
    if (!ws_secondary_compat_setting.empty()) {
        char secondary_buffer[256];
        local_wstr2str(secondary_buffer, sizeof(secondary_buffer), ws_secondary_compat_setting.c_str());
        secondary_compat_setting_str = secondary_buffer;
    } else {
        secondary_compat_setting_str = SharedConstants::Defaults::VERSION_DETECT_STR;
    }
    logger->debug("Secondary backwards compatible version setting from registry: '%s'", secondary_compat_setting_str.c_str());
    setLogformatForVersion(secondary_compat_setting_str, secondary_logformat_, secondary_logzilla_version_, true);
}

void Configuration::saveToRegistry() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    for (const auto& log : logs_) {
        log.saveToRegistry(*registry_p_); // Pass by reference
    }
}

void Configuration::loadFilterIds(wstring value) {
    auto logger = ::Logger::getLoggerByKey("Configuration");
    if (value.empty()) {
        // Clear the filter when no value is provided
        event_id_filter_.clear();
        return;
    }

    set<DWORD> new_filter;
    auto input = value + L",";
    auto id = 0u;

    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == L',') {
            if (id > 0) {
                logger->debug2("Configuration::loadFilterIds() adding event filter id for %u\n", id);
                new_filter.insert(id);
            }
            id = 0;
            continue;
        }

        if (input[i] >= L'0' && input[i] <= L'9') {
            id = id * 10 + (input[i] - L'0');
        }
    }

    event_id_filter_ = std::move(new_filter);
}

void Configuration::getTimeZoneOffset() {
    TIME_ZONE_INFORMATION time_zone_info;
    GetTimeZoneInformation(&time_zone_info);
    utc_offset_minutes_ = static_cast<int>(time_zone_info.Bias);
}

void Configuration::setHostName() {
    auto logger = LOG_THIS;
    static constexpr size_t HOSTNAME_BUFFER_SIZE_WCHAR = MAX_COMPUTERNAME_LENGTH + 1;
    WCHAR computerNameW[HOSTNAME_BUFFER_SIZE_WCHAR];
    DWORD size = HOSTNAME_BUFFER_SIZE_WCHAR;
    bool retrieved_hostname = false;

    if (GetComputerNameW(computerNameW, &size) == TRUE) {
        char computer_name_buf[1024]; // Should be sufficient
        local_wstr2str(computer_name_buf, sizeof(computer_name_buf), computerNameW);
        host_name_ = string(computer_name_buf);
        retrieved_hostname = true;
        logger->info("Configuration::setHostName() using GetComputerNameW: %s", host_name_.c_str());
    } else {
        logger->warning("Configuration::setHostName() GetComputerNameW() failed: %u. Attempting fallback.", GetLastError());

        // Fallback 1: gethostname (standard C, might return same as GetComputerName but worth trying)
        char hn_buffer[256]; // NI_MAXHOST is often 1025, but gethostname typically smaller
        if (gethostname(hn_buffer, sizeof(hn_buffer)) == 0) {
            host_name_ = string(hn_buffer);
            retrieved_hostname = true;
            logger->info("Configuration::setHostName() using gethostname fallback: %s", host_name_.c_str());
        } else {
            logger->warning("Configuration::setHostName() gethostname() failed: %d. Attempting IP address.", WSAGetLastError());

            // Fallback 2: IP Address
            // Note: This assumes WSAStartup has been called elsewhere in the application.
            // If not, WSAStartup/WSACleanup would be needed here, which is not ideal for a library function.
            ADDRINFOA* result = nullptr;
            ADDRINFOA hints = {0};
            hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6, or AF_UNSPEC for both
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            // Try with a null hostname to get local IPs, as gethostname might have failed
            int gai_error = getaddrinfo(nullptr, nullptr, &hints, &result);
            if (gai_error == 0) {
                std::string first_ipv4;
                std::string first_ipv6;

                for (ADDRINFOA* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
                    char ip_string_buffer[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN is large enough for IPv4 and IPv6

                    if (ptr->ai_family == AF_INET) { // IPv4
                        struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
                        if (inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ip_string_buffer, sizeof(ip_string_buffer)) != nullptr) {
                            if (first_ipv4.empty() && strcmp(ip_string_buffer, "127.0.0.1") != 0) { // Prefer non-loopback
                                first_ipv4 = ip_string_buffer;
                            }
                        }
                    } else if (ptr->ai_family == AF_INET6) { // IPv6
                        struct sockaddr_in6* sockaddr_ipv6 = (struct sockaddr_in6*)ptr->ai_addr;
                         // Check if it's not a loopback address (::1)
                        if (!IN6_IS_ADDR_LOOPBACK(&sockaddr_ipv6->sin6_addr)) {
                            if (inet_ntop(AF_INET6, &sockaddr_ipv6->sin6_addr, ip_string_buffer, sizeof(ip_string_buffer)) != nullptr) {
                                if (first_ipv6.empty()) {
                                    first_ipv6 = ip_string_buffer;
                                }
                            }
                        }
                    }
                }
                freeaddrinfo(result);

                if (!first_ipv4.empty()) {
                    host_name_ = first_ipv4;
                    retrieved_hostname = true;
                    logger->info("Configuration::setHostName() using IPv4 address fallback: %s", host_name_.c_str());
                } else if (!first_ipv6.empty()) {
                    host_name_ = first_ipv6; // Use IPv6 if no non-loopback IPv4 was found
                    retrieved_hostname = true;
                    logger->info("Configuration::setHostName() using IPv6 address fallback: %s", host_name_.c_str());
                } else {
                     logger->warning("Configuration::setHostName() Could not find a suitable non-loopback IP address.");
                }
            } else {
                logger->warning("Configuration::setHostName() getaddrinfo() failed: %d.", gai_error);
            }
        }
    }

    if (!retrieved_hostname) {
        logger->recoverable_error("Configuration::setHostName() All attempts to get hostname or IP failed. Defaulting to 'unknown'.");
        host_name_ = string("unknown");
    }
}

const string& Configuration::getHostName() const {
    shared_lock<shared_mutex> lock(mutex_);
    return host_name_;
}

void Configuration::setLogformatForVersion(const std::string& version_input_str, std::atomic<int>& target_log_format_atomic_ref, std::string& target_version_storage, bool is_from_config_load) {
    auto logger = ::Logger::getLoggerByKey("Configuration");
    std::string version_str_processed = version_input_str;
    std::transform(version_str_processed.begin(), version_str_processed.end(), version_str_processed.begin(),
        [](unsigned char c){ return std::tolower(c); });

    target_version_storage = version_input_str; // Store the original input

    logger->debug("setLogformatForVersion called. Input: '%s', Processed: '%s', IsFromConfigLoad: %s",
        version_input_str.c_str(), version_str_processed.c_str(), is_from_config_load ? "true" : "false");

    if (version_str_processed == SharedConstants::Defaults::VERSION_DETECT_STR) {
        target_log_format_atomic_ref.store(SharedConstants::LOGFORMAT_DETECT);
        logger->info("Log format set to DETECT for target based on input '%s'. Dynamic detection will be attempted.", version_input_str.c_str());
        return;
    }

    if (is_from_config_load) {
        if (version_str_processed == "5" || version_str_processed == "json" || version_str_processed == "jsonport") {
            target_log_format_atomic_ref.store(SharedConstants::LOGFORMAT_JSONPORT);
            logger->info("Log format explicitly set to JSONPORT for target based on registry config: '%s'.", version_input_str.c_str());
            return;
        }
        if (version_str_processed == "6" || version_str_processed == "http" || version_str_processed == "httpport") {
            target_log_format_atomic_ref.store(SharedConstants::LOGFORMAT_HTTPPORT);
            logger->info("Log format explicitly set to HTTPPORT for target based on registry config: '%s'.", version_input_str.c_str());
            return;
        }
        // If is_from_config_load is true, and it wasn't "detect" or a keyword,
        // it means a specific version string was provided. Log this and proceed to comparison.
        logger->info("Specific version string '%s' provided in registry. Proceeding to version comparison for log format.", version_input_str.c_str());
    }

    // If not returned yet (i.e., not "detect", not a keyword during config load, or if is_from_config_load is false)
    // Perform version comparison.
    // Ensure LOGFORMAT_LZ_VERSION_HTTP is a std::string for toLower
    std::string http_threshold_version_str = SharedConstants::LOGFORMAT_LZ_VERSION_HTTP; 
    if (Util::compareSoftwareVersions(version_str_processed.c_str(), http_threshold_version_str.c_str()) >= 0) {
        target_log_format_atomic_ref.store(SharedConstants::LOGFORMAT_HTTPPORT);
        logger->info("Log format set to HTTPPORT for target. Input version '%s' is GTE '%s'.",
            version_input_str.c_str(), http_threshold_version_str.c_str());
    }
    else {
        target_log_format_atomic_ref.store(SharedConstants::LOGFORMAT_JSONPORT);
        logger->info("Log format set to JSONPORT for target. Input version '%s' is LT '%s'.",
            version_input_str.c_str(), http_threshold_version_str.c_str());
    }
}

void Configuration::setPrimaryLogzillaVersion(const string& version) {
    unique_lock lock(mutex_);
    // primary_logzilla_version_ = version; // This is now set inside setLogformatForVersion
    setLogformatForVersion(version, primary_logformat_, primary_logzilla_version_, false);
    auto logger = ::Logger::getLoggerByKey("Configuration");
    logger->debug("Primary LogZilla version set to '%s', resulting log format: %d", primary_logzilla_version_.c_str(), primary_logformat_.load());
}

void Configuration::setSecondaryLogzillaVersion(const string& version) {
    unique_lock lock(mutex_);
    // secondary_logzilla_version_ = version; // This is now set inside setLogformatForVersion
    setLogformatForVersion(version, secondary_logformat_, secondary_logzilla_version_, false);
    auto logger = ::Logger::getLoggerByKey("Configuration");
    logger->debug("Secondary LogZilla version set to '%s', resulting log format: %d", secondary_logzilla_version_.c_str(), secondary_logformat_.load());
}

void Configuration::setPrimaryLogFormatToHttpDefaultOnError() {
    unique_lock lock(mutex_);
    primary_logformat_.store(SharedConstants::LOGFORMAT_HTTPPORT);
    primary_logzilla_version_ = "default_http_fallback_primary";
    auto logger = ::Logger::getLoggerByKey("Configuration");
    logger->warning("Primary log format defaulted to HTTPPORT due to an error or explicit override. Version set to '%s'.", primary_logzilla_version_.c_str());
}

void Configuration::setSecondaryLogFormatToHttpDefaultOnError() {
    unique_lock lock(mutex_);
    secondary_logformat_.store(SharedConstants::LOGFORMAT_HTTPPORT);
    secondary_logzilla_version_ = "default_http_fallback_secondary";
    auto logger = ::Logger::getLoggerByKey("Configuration");
    logger->warning("Secondary log format defaulted to HTTPPORT due to an error or explicit override. Version set to '%s'.", secondary_logzilla_version_.c_str());
}

bool Configuration::isPrimaryDynamicDetectionRequired() const {
    // No lock needed here as atomics are used for primary_logformat_ and std::string is thread-safe for const access.
    // However, if primary_logzilla_version_ could be written to by another thread during this read,
    // a race condition could occur if it's modified between the two parts of the expression.
    // Using a shared_lock for consistency and safety if version string could change.
    shared_lock<shared_mutex> lock(mutex_);
    return primary_logformat_.load(std::memory_order_relaxed) == SharedConstants::LOGFORMAT_DETECT &&
           primary_logzilla_version_ == SharedConstants::Defaults::VERSION_DETECT_STR;
}

bool Configuration::isSecondaryDynamicDetectionRequired() const {
    shared_lock<shared_mutex> lock(mutex_);
    return secondary_logformat_.load(std::memory_order_relaxed) == SharedConstants::LOGFORMAT_DETECT &&
           secondary_logzilla_version_ == SharedConstants::Defaults::VERSION_DETECT_STR;
}

int Configuration::getPrimaryLogformat() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // If the format is still DETECT, it means detection hasn't run or completed.
    // The caller (e.g., network client setup) should handle this.
    // For now, directly return the stored atomic value.
    // The old behavior of defaulting to JSONPORT if DETECT is no longer appropriate here
    // as DETECT is a valid state until resolution.
    return primary_logformat_.load(std::memory_order_relaxed);
}

int Configuration::getSecondaryLogformat() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // Similar to getPrimaryLogformat, return the actual stored atomic value.
    return secondary_logformat_.load(std::memory_order_relaxed);
}

int Configuration::getPrimaryPort() const {
    shared_lock<shared_mutex> lock(mutex_);

#if THIS_CODE_IS_OBSOLETE
    if (primary_port_ > 0) {
        return primary_port_;
    }

    // If no port specified in registry, determine by format and TLS
    if (primary_logformat_ == SharedConstants::LOGFORMAT_DETECT ||
        primary_logformat_ == SharedConstants::LOGFORMAT_JSONPORT) {
        return 514;  // Default syslog port for JSON format
    }

    return primary_use_tls_ ? 443 : 80;  // HTTP ports
#endif
    return primary_port_;
}

int Configuration::getSecondaryPort() const {
    shared_lock<shared_mutex> lock(mutex_);

    if (secondary_port_ > 0) {
        return secondary_port_;
    }

    // If no port specified in registry, determine by format and TLS
    if (secondary_logformat_ == SharedConstants::LOGFORMAT_DETECT ||
        secondary_logformat_ == SharedConstants::LOGFORMAT_JSONPORT) {
        return 514;  // Default syslog port for JSON format
    }

    return secondary_use_self_signed_cert_ ? 443 : 80;  // HTTP ports
}

// --- Start Added Setters for Testing ---

void Configuration::setHostName(const string& name) {
    unique_lock lock(mutex_);
    host_name_ = name;
}

void Configuration::setPrimaryLogformat(int format) {
    unique_lock lock(mutex_);
    primary_logformat_ = format;
}

void Configuration::setSecondaryLogformat(int format) {
    unique_lock lock(mutex_);
    secondary_logformat_ = format;
}

void Configuration::setPrimaryPort(int port) {
    unique_lock lock(mutex_);
    primary_port_ = port;
}

void Configuration::setSecondaryPort(int port) {
    unique_lock lock(mutex_);
    secondary_port_ = port;
}

void Configuration::setLookupAccounts(bool lookup) {
    unique_lock lock(mutex_);
    lookup_accounts_ = lookup;
}

void Configuration::setForwardToSecondary(bool forward) {
    unique_lock lock(mutex_);
    forward_to_secondary_ = forward;
}

void Configuration::setPrimaryUseSelfSignedCert(bool use_tls) {
    unique_lock lock(mutex_);
    primary_use_self_signed_cert_ = use_tls;
}

void Configuration::setSecondaryUseSelfSignedCert(bool use_tls) {
    unique_lock lock(mutex_);
    secondary_use_self_signed_cert_ = use_tls;
}

void Configuration::setFacility(int facility) {
    unique_lock lock(mutex_);
    facility_ = facility;
}

void Configuration::setSeverity(int severity) {
    unique_lock lock(mutex_);
    severity_ = severity;
}

void Configuration::setPrimaryHost(const wstring& host) {
    unique_lock lock(mutex_);
    primary_host_ = host;
}

void Configuration::setPrimaryApiKey(const wstring& key) {
    unique_lock lock(mutex_);
    primary_api_key_ = key;
}

void Configuration::setSecondaryHost(const wstring& host) {
    unique_lock lock(mutex_);
    secondary_host_ = host;
}

void Configuration::setSecondaryApiKey(const wstring& key) {
    unique_lock lock(mutex_);
    secondary_api_key_ = key;
}

void Configuration::setSuffix(const wstring& suffix) {
    unique_lock lock(mutex_);
    suffix_ = suffix;
}

void Configuration::setEventIdFilter(const set<DWORD>& filter) {
    unique_lock lock(mutex_);
    event_id_filter_ = filter;
}

void Configuration::addEventIdToFilter(DWORD eventId) {
    unique_lock lock(mutex_);
    event_id_filter_.insert(eventId);
}

void Configuration::clearEventIdFilter() {
    unique_lock lock(mutex_);
    event_id_filter_.clear();
}

void Configuration::setIncludeVsIgnoreEventIds(bool include) {
    unique_lock lock(mutex_);
    include_vs_ignore_eventids_ = include;
}

void Configuration::setOnlyWhileRunning(bool only_while_running) {
    unique_lock lock(mutex_);
    only_while_running_ = only_while_running;
}

void Configuration::setUseHTTP2(bool use_http2) {
    unique_lock lock(mutex_);
    use_http2_ = use_http2;
}

void Configuration::setUtcOffsetMinutes(int offset) {
    unique_lock lock(mutex_);
    utc_offset_minutes_ = offset;
}

void Configuration::setMaxBatchCount(uint32_t count) {
    unique_lock lock(mutex_);
    max_batch_size_ = count;
}

void Configuration::setMaxBatchAge(uint32_t age) {
    unique_lock lock(mutex_);
    max_batch_age_ = age;
}

// --- End Added Setters for Testing ---

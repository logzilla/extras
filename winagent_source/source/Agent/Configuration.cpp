/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#include "stdafx.h"
#include "timezoneapi.h"
#include "Logger.h"
#include "Configuration.h"
#include "RecordNumber.h"
#include "Registry.h"
#include "SyslogAgentSharedConstants.h"
#include "Util.h"

using namespace Syslog_agent;

#define REG_BUFFER_LEN				2048

int Configuration::debug_level_setting_ = Logger::NOLOG;
int Configuration::event_log_poll_interval_ = SharedConstants::Defaults::POLL_INTERVAL_SEC;

bool Configuration::hasSecondaryHost() const {
    return forward_to_secondary_ && secondary_host_.size() > 0;
}

void Configuration::loadFromRegistry(bool running_from_console, bool override_log_level, Logger::LogLevel override_log_level_setting) {

	char hostname_buf[MAX_COMPUTERNAME_LENGTH + 1];
	strcpy_s(hostname_buf, sizeof(hostname_buf), "");
    Registry registry;
    registry.open();

    if (override_log_level) {
        debug_level_setting_ = override_log_level_setting;
    }
    else {
        debug_level_setting_ = registry.readInt(SharedConstants::RegistryKey::DEBUG_LEVEL_SETTING, 0);
    }
    debug_log_file_ = registry.readString(SharedConstants::RegistryKey::DEBUG_LOG_FILE, L"");
    Logger::setLogFile(debug_log_file_);
    if (debug_level_setting_ != (int)Logger::NOLOG) {
        if (debug_log_file_.length() > 0) {
            if (running_from_console) {
                Logger::setLogDestination(Logger::DEST_CONSOLE_AND_FILE);
            }
            else {
                Logger::setLogDestination(Logger::DEST_FILE);
            }
        }
        else {
            Logger::setLogDestination(Logger::DEST_CONSOLE);
        }
        Logger::setLogLevel((Logger::LogLevel)debug_level_setting_);
    }
    else {
        Logger::setLogLevel(Logger::NOLOG);
    }

    // need this because originally only_while_running_ was a string
    try {
        only_while_running_ = registry.readBool(SharedConstants::RegistryKey::ONLY_WHILE_RUNNING, false);
    }
    catch (std::exception) {
        try {
            wstring bad_reg_string = registry.readString(SharedConstants::RegistryKey::ONLY_WHILE_RUNNING, L"");
            if ((Util::toLowercase(bad_reg_string) == L"true")
                || (Util::toLowercase(bad_reg_string) == L"yes")
                || (bad_reg_string == L"1")) {
                only_while_running_ = true;
            }
            else {
                only_while_running_ = false;
            }
        }
        catch (std::exception) {
            only_while_running_ = false;
        }
    }
	api_path_ = SharedConstants::HTTP_API_PATH;
    event_log_poll_interval_ = registry.readInt(SharedConstants::RegistryKey::EVENT_LOG_POLL_INTERVAL, SharedConstants::Defaults::POLL_INTERVAL_SEC);
    if (event_log_poll_interval_ == 0) {
        event_log_poll_interval_ = SharedConstants::Defaults::POLL_INTERVAL_SEC;
    }
    primary_host_ = registry.readString(SharedConstants::RegistryKey::PRIMARY_HOST, L"localhost");
    primary_api_key_ = registry.readString(SharedConstants::RegistryKey::PRIMARY_API_KEY, L"");
    secondary_host_ = registry.readString(SharedConstants::RegistryKey::SECONDARY_HOST, L"");
    secondary_api_key_ = registry.readString(SharedConstants::RegistryKey::SECONDARY_API_KEY, L"");
    suffix_ = registry.readString(SharedConstants::RegistryKey::SUFFIX, L"");
    forward_to_secondary_ = registry.readBool(SharedConstants::RegistryKey::FORWARD_TO_SECONDARY, false);
    primary_use_tls_ = registry.readBool(SharedConstants::RegistryKey::PRIMARY_USE_TLS, false);
    secondary_use_tls_ = registry.readBool(SharedConstants::RegistryKey::SECONDARY_USE_TLS, false);
    lookup_accounts_ = registry.readBool(SharedConstants::RegistryKey::LOOKUP_ACCOUNTS, true);
    batch_interval_ = registry.readInt(SharedConstants::RegistryKey::BATCH_INTERVAL, SharedConstants::Defaults::BATCH_INTERVAL);
    facility_ = registry.readInt(SharedConstants::RegistryKey::FACILITY, SharedConstants::Defaults::FACILITY);
    severity_ = registry.readInt(SharedConstants::RegistryKey::SEVERITY, SharedConstants::Defaults::SEVERITY);
    tail_filename_ = registry.readString(SharedConstants::RegistryKey::TAIL_FILENAME, L"");
    string tail_file = Util::wstr2str(tail_filename_);
    Logger::debug("Tail requested for file %s\n", tail_file.c_str());
    tail_program_name_ = registry.readString(SharedConstants::RegistryKey::TAIL_PROGRAM_NAME, L"");

    include_vs_ignore_eventids_ = registry.readBool(SharedConstants::RegistryKey::INCLUDE_VS_IGNORE_EVENT_IDS, false);
    loadFilterIds(registry.readString(SharedConstants::RegistryKey::EVENT_ID_FILTER, L""));

    auto channels = registry.readChannels();
    logs_.clear();
    logs_.reserve(channels.size());
    for (auto& channel : registry.readChannels()) {
        logs_.push_back(LogConfiguration());
        logs_.back().channel_ = channel;
        logs_.back().name_ = channel;
        logs_.back().nname_ = Util::wstr2str(channel);
        logs_.back().loadFromRegistry(registry);
        Logger::debug("Configuration::loadFromRegistry() event log %ls\n", channel.c_str());
    }

    registry.close();

	host_name_ = getHostName();

    getTimeZoneOffset();
    Logger::debug("Loaded configuration from registry (from console: %s)\n", (running_from_console ? "true" : "false"));

}

void Configuration::saveToRegistry() const {
    Registry registry;
    registry.open();
    for (auto& log : logs_) {
        log.saveToRegistry(registry);
    }
    registry.close();
}

void Configuration::loadFilterIds(wstring value) {
    if (value.size() == 0) return;
    auto input = value + L",";
    auto id = 0;
    for (auto i = 0; i < input.size(); i++) {
        if (input[i] == L',') {
            Logger::debug2("Configuration::loadFilterIds() adding event filter id for %d\n", id);
            event_id_filter_.insert(id);
            id = 0;
        }
        if (input[i] < L'0' || input[i] > L'9') continue;
        id = id * 10 + input[i] - L'0';
    }
}

void Configuration::getTimeZoneOffset() {
    TIME_ZONE_INFORMATION time_zone_info;
    auto result = GetTimeZoneInformation(&time_zone_info);
    utc_offset_minutes_ = (int) time_zone_info.Bias;
}

string Configuration::getHostName() const {
    TCHAR computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName) / sizeof(computerName[0]);

    if (!GetComputerName(computerName, &size)) {
        return string("");
    }
    if (sizeof(TCHAR) == sizeof(wchar_t)) {
        return Util::wstr2str(wstring(computerName));
    }
    else {
        return string((char*)computerName);
    }
}

void Configuration::setLogformatForVersion(int& logformat, const string& version) {
    if (Util::compareSoftwareVersions(version, SharedConstants::LOGFORMAT_LZ_VERSION_HTTP) < 0) {
        primary_logformat_ = SharedConstants::LOGFORMAT_JSONPORT;
    }
    else {
        primary_logformat_ = SharedConstants::LOGFORMAT_HTTPPORT;
    }
}

void Configuration::setPrimaryLogzillaVersion(const string& version) {
    primary_logzilla_version_ = version;
	setLogformatForVersion(primary_logformat_, version);
}

void Configuration::setSecondaryLogzillaVersion(const string& version) {
    secondary_logzilla_version_ = version;
	setLogformatForVersion(secondary_logformat_, version);
}

int Configuration::getPrimaryLogformat() const {
    if (primary_logformat_ == SharedConstants::LOGFORMAT_DETECT)
        return SharedConstants::LOGFORMAT_JSONPORT;
    return primary_logformat_;
}

int Configuration::getSecondaryLogformat() const {
    if (secondary_logformat_ == SharedConstants::LOGFORMAT_DETECT)
        return SharedConstants::LOGFORMAT_JSONPORT;
    return secondary_logformat_;
}


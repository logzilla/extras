/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
*/

#pragma once

#include <set>
#include <vector>
#include "Logger.h"
#include "LogConfiguration.h"
#include "SyslogAgentSharedConstants.h"

using namespace std;

namespace Syslog_agent {

    class Configuration {
    public:
        static constexpr const wchar_t* PRIMARY_CERT_FILENAME 
            = SharedConstants::Defaults::PRIMARY_CERT;
        static constexpr const wchar_t* SECONDARY_CERT_FILENAME 
            = SharedConstants::Defaults::SECONDARY_CERT;

        wstring api_path_ = SharedConstants::HTTP_API_PATH;
        wstring version_path_ = SharedConstants::LOGZILLA_VERSION_PATH;
        bool lookup_accounts_ = false;
        bool forward_to_secondary_ = false;
        const bool use_ping_ = false;
        const bool use_tcp_ = true;
        const bool use_RFC3164_ = false;
        const bool use_json_message_ = true;
        bool use_log_agent_ = true;
        bool primary_use_tls_ = false;
        bool secondary_use_tls_ = false;
        static int debug_level_setting_;
        static int event_log_poll_interval_;
        int facility_;
        int severity_;
        string host_name_;
        int batch_interval_;
        wstring primary_host_ = SharedConstants::Defaults::PRIMARY_HOST;;
        wstring primary_api_key_ = L"";
        wstring secondary_host_ = SharedConstants::Defaults::SECONDARY_HOST;
        wstring secondary_api_key_ = L"";
        wstring suffix_ = SharedConstants::Defaults::SUFFIX;
        wstring debug_log_file_ = SharedConstants::Defaults::DEBUG_LOG_FILENAME;
        vector<LogConfiguration> logs_;
        set<DWORD> event_id_filter_;
        wstring tail_filename_;
        wstring tail_program_name_;
        int utc_offset_minutes_;
        bool include_vs_ignore_eventids_;
        bool only_while_running_;
        string primary_logzilla_version_ = SharedConstants::Defaults::LOGZILLA_VER;
        string secondary_logzilla_version_ = SharedConstants::Defaults::LOGZILLA_VER;
        int primary_logformat_ = SharedConstants::LOGFORMAT_DETECT;
        int secondary_logformat_ = SharedConstants::LOGFORMAT_DETECT;


        void loadFromRegistry(bool running_from_console, bool override_log_level, 
            Logger::LogLevel override_log_level_setting);
        void saveToRegistry() const;
        bool hasSecondaryHost() const;
        string getHostName() const;
        void setPrimaryLogzillaVersion(const string& version);
        void setSecondaryLogzillaVersion(const string& version);
        int getPrimaryLogformat() const;
        int getSecondaryLogformat() const;

        static constexpr int MAX_TAIL_FILE_LINE_LENGTH = 16000;

    private:
        static constexpr int MAX_COMPUTERNAME_LENGH = 200;
        void loadFilterIds(wstring value);
        void getTimeZoneOffset();

        void setLogformatForVersion(int& logformat, const string& version);
    };
}


/*
SyslogAgent: a syslog agent for Windows
Copyright 2021 Logzilla Corp.
*/

/* SyslogAgentSharedContants.h
* single file to collect constants that must be shared between
* Syslog Agent apps (such as the config app)
* unfortunately this file still must be manually synched to
* the config app but at least having them in this single point
* reduces the chances of mismatch
*/

#pragma once

namespace Syslog_agent {

#define SYSLOGAGENT_CURRENT_VERSION L"6.34.1.0"

    class SharedConstants {
    public:
        // Misc
        static constexpr const wchar_t* USER_AGENT = L"LZ Syslog Agent/" SYSLOGAGENT_CURRENT_VERSION;
        static constexpr const wchar_t* HTTP_API_PATH = L"/incoming";
        static constexpr const wchar_t* LOGZILLA_VERSION_PATH = L"/version";
        static constexpr const wchar_t* CERT_FILE_PRIMARY = L"primary.pfx";
        static constexpr const wchar_t* CERT_FILE_SECONDARY = L"secondary.pfx";
        static constexpr unsigned int   LZ_JSON_PORT = 515;
        static constexpr unsigned int   MAX_CATCHUP_DAYS = 7;

        // Version strings
        static constexpr const wchar_t* CURRENT_VERSION = SYSLOGAGENT_CURRENT_VERSION;
        static constexpr const wchar_t* CURRENT_CONFIG_VERSION = L"6.30.0.0";

        // HTTP settings
        static constexpr bool USE_HTTP2 = true;  // Enable HTTP/2 if available
        static constexpr bool USE_COMPRESSION = true;  // Enable compression if available
        static constexpr int  HTTP2_PING_TIMEOUT_MS = 60000;  // Keep HTTP/2 connections alive for 60 seconds

        // Retry settings
        static constexpr int MAX_RETRY_ATTEMPTS = 3;
        static constexpr int INITIAL_RETRY_DELAY_MS = 1000;    // 1 second
        static constexpr int MAX_RETRY_DELAY_MS = 8000;        // 8 seconds

        // Certificate settings
        static constexpr int CERT_EXPIRY_WARNING_DAYS = 30;    // Warn when cert within 30 days of expiry
        static constexpr bool LENIENT_CERT_DATE_CHECK = false; // Default to strict date checking

        // Default values
        class Defaults {
        public:
            static constexpr unsigned int       FACILITY = 20;
            static constexpr unsigned int       SEVERITY = 8;
            static constexpr const wchar_t*     PRIMARY_HOST = L"";
            static constexpr const wchar_t*     SECONDARY_HOST = L"";
            static constexpr bool               USE_FORWARDER = false;
            static constexpr const wchar_t*     FORWARDER_DEST = L"";
            static constexpr const wchar_t*     UDP_FWD_PORT_S = L"";
            static constexpr const wchar_t*     TCP_FWD_PORT_S = L"";
            static constexpr const wchar_t*     SUFFIX = L"";
            static constexpr const wchar_t*     DEBUG_LOG_FILENAME = L"";
            static constexpr const wchar_t*     TAIL_FILENAME = L"";
            static constexpr const wchar_t*     TAIL_PROGRAMNAME = L"";
            static constexpr int                BATCH_INTERVAL = 1000;
            // Note: Using a string literal for version detection to ensure it's a compile-time constant
            static constexpr const char         VERSION_DETECT_STR_DATA[] = "detect";
            static constexpr const char* const  VERSION_DETECT_STR = VERSION_DETECT_STR_DATA;
            static constexpr const char* const  LOGZILLA_VER = VERSION_DETECT_STR;
            static constexpr int                POLL_INTERVAL_SEC = 2;
            static constexpr uint32_t           MAX_BATCH_SIZE = 1000;
            static constexpr uint32_t           MAX_BATCH_AGE = 1000;
        };

        // Severity levels
        class Severities {
        public:
            static constexpr int                EMERGENCY = 0;
            static constexpr int                ALERT = 1;
            static constexpr int                CRITICAL = 2;
            static constexpr int                ERR = 3;
            static constexpr int                WARNING = 4;
            static constexpr int                NOTICE = 5;
            static constexpr int                INFORMATIONAL = 6;
            static constexpr int                DEBUG = 7;
            static constexpr int                DYNAMIC = 8;
        };

        // Facility codes
        class Facilities {
        public:
            static constexpr int                KERNEL = 0;
            static constexpr int                USER = 1;
            static constexpr int                MAIL = 2;
            static constexpr int                SYSTEM = 3;
            static constexpr int                SECAUTH = 4;
            static constexpr int                SYSLOGD = 5;
            static constexpr int                LPRINTER = 6;
            static constexpr int                NETNEWS = 7;
            static constexpr int                UUCP = 8;
            static constexpr int                CLOCK = 9;
            static constexpr int                SECAUTH2 = 10;
            static constexpr int                FTP = 11;
            static constexpr int                NTP = 12;
            static constexpr int                LOGAUDIT = 13;
            static constexpr int                LOGALERT = 14;
            static constexpr int                CLOCK2 = 15;
            static constexpr int                LOCALUSE0 = 16;
            static constexpr int                LOCALUSE1 = 17;
            static constexpr int                LOCALUSE2 = 18;
            static constexpr int                LOCALUSE3 = 19;
            static constexpr int                LOCALUSE4 = 20;
            static constexpr int                LOCALUSE5 = 21;
            static constexpr int                LOCALUSE6 = 22;
            static constexpr int                LOCALUSE7 = 23;
        };

        // Maximum lengths and sizes
        static constexpr int                MAX_SUFFIX_LENGTH = 1000;
        static constexpr int                SENDER_MAINLOOP_DURATION = 1000;
        static constexpr int                MAX_LOG_NAME_LENGTH = 1000;

        // Log formats
        static constexpr int                LOGFORMAT_DETECT = 0;
        static constexpr int                LOGFORMAT_JSONPORT = 1;
        static constexpr int                LOGFORMAT_HTTPPORT = 2;
        static constexpr const char* LOGFORMAT_LZ_VERSION_HTTP = "6.34";

        class RegistryKey {
        public:
            static constexpr const wchar_t* MAIN_KEY = L"SOFTWARE\\LogZilla\\SyslogAgent";
            static constexpr const wchar_t* CHANNELS_KEY = L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels";
            static constexpr const wchar_t* CONFIG_VERSION = L"ConfigVersion";
            static constexpr const wchar_t* INCLUDE_VS_IGNORE_EVENT_IDS = L"IncludeVsIgnoreEventIds";
            static constexpr const wchar_t* EVENT_ID_FILTER = L"EventIDFilterList";
            static constexpr const wchar_t* ONLY_WHILE_RUNNING = L"OnlyWhileRunning";
            static constexpr const wchar_t* EVENT_LOG_POLL_INTERVAL = L"EventLogPollInterval";
            static constexpr const wchar_t* FORWARD_TO_SECONDARY = L"ForwardToMirror";
            static constexpr const wchar_t* LOOKUP_ACCOUNTS = L"LookupAccountSID";
            static constexpr const wchar_t* EXTRA_KEY_VALUE_PAIRS = L"ExtraKeyValuePairs";
            static constexpr const wchar_t* PRIMARY_HOST = L"Syslog";
            static constexpr const wchar_t* PRIMARY_PORT = L"SendToPort"; // deprecated
            static constexpr const wchar_t* PRIMARY_API_KEY = L"PrimaryLogZillaApiKey";
            static constexpr const wchar_t* PRIMARY_USE_SELF_SIGNED_CERT = L"PrimaryUseTLS";
            static constexpr const wchar_t* PRIMARY_USE_HTTP2 = L"PrimaryUseHTTP2";
            static constexpr const wchar_t* PRIMARY_USE_COMPRESSION = L"PrimaryUseCompression";
            static constexpr const wchar_t* SECONDARY_HOST = L"Syslog1";
            static constexpr const wchar_t* SECONDARY_PORT = L"SendToBackupPort";
            static constexpr const wchar_t* SECONDARY_API_KEY = L"SecondaryLogZillaApiKey";
            static constexpr const wchar_t* SECONDARY_USE_SELF_SIGNED_CERT = L"SecondaryUseTLS";
            static constexpr const wchar_t* SECONDARY_USE_HTTP2 = L"SecondaryUseHTTP2";
            static constexpr const wchar_t* SECONDARY_USE_COMPRESSION = L"SecondaryUseCompression";
            static constexpr const wchar_t* FACILITY = L"Facility";
            static constexpr const wchar_t* SEVERITY = L"Severity";
            static constexpr const wchar_t* SUFFIX = L"Suffix";
            static constexpr const wchar_t* DEBUG_LEVEL_SETTING = L"DebugLevel";
            static constexpr const wchar_t* DEBUG_LOG_FILE = L"DebugLogFile";
            static constexpr const wchar_t* TAIL_FILENAME = L"TailFilename";
            static constexpr const wchar_t* TAIL_PROGRAM_NAME = L"TailProgramName";
            static constexpr const wchar_t* CHANNEL_ENABLED = L"Enabled";
            static constexpr const wchar_t* CHANNEL_BOOKMARK = L"Bookmark";
            static constexpr const wchar_t* PRIMARY_TLS_FILENAME = L"PrimaryTlsFileName";
            static constexpr const wchar_t* SECONDARY_TLS_FILENAME = L"SecondaryTlsFileName";
            static constexpr const wchar_t* LOGZILLA_REGISTRY_KEY = L"SOFTWARE\\LogZilla\\SyslogAgent";
            static constexpr const wchar_t* WINDOWS_EVENT_CHANNELS_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Channels";
            static constexpr const wchar_t* SELECTED_EVENT_CHANNELS_KEY = L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels";
            static constexpr const wchar_t* INITIAL_SETUP_REG_FILE_KEY = L"InitialSetupRegFile";
            static constexpr const wchar_t* BATCH_INTERVAL = L"BatchInterval";
            static constexpr const wchar_t* PRIMARY_BACKWARDS_COMPAT_VER = L"PrimaryBackwardsCompatibleVersion";
            static constexpr const wchar_t* SECONDARY_BACKWARDS_COMPAT_VER = L"SecondaryBackwardsCompatibleVersion";
            static constexpr const wchar_t* INITIAL_SETUP_FILE = L"InitialSetupRegFile";
            static constexpr const wchar_t* MAX_BATCH_SIZE = L"MaxBatchSize";
            static constexpr const wchar_t* MAX_BATCH_AGE = L"MaxBatchAge";
        };
    };

#undef SYSLOGAGENT_CURRENT_VERSION

} // namespace SyslogAgent#pragma once

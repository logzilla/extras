/*
SyslogAgent: a syslog agent for Windows
Copyright © 2021 Logzilla Corp.
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

#define SYSLOGAGENT_CURRENT_VERSION L"6.30.2.0"
    
    class SharedConstants {
    public:
        // Misc
        static constexpr wchar_t const* USER_AGENT              = L"LZ Syslog Agent/" SYSLOGAGENT_CURRENT_VERSION;
        static constexpr wchar_t const* HTTP_API_PATH           = L"/incoming";
        static constexpr wchar_t const* LOGZILLA_VERSION_PATH   = L"/version";
        static constexpr wchar_t const* CERT_FILE_PRIMARY       = L"primary.pfx";
        static constexpr wchar_t const* CERT_FILE_SECONDARY     = L"secondary.pfx";

        // Version strings
        static constexpr wchar_t const* CURRENT_VERSION         = SYSLOGAGENT_CURRENT_VERSION;
        static constexpr wchar_t const* CURRENT_CONFIG_VERSION  = L"6.30.0.0";

        // Default values
        class Defaults {
        public:
            static constexpr unsigned int       FACILITY            = 20;
            static constexpr unsigned int       SEVERITY            = 8;
            static constexpr wchar_t const*     PRIMARY_HOST        = L"";
            static constexpr wchar_t const*     SECONDARY_HOST      = L"";
            static constexpr bool               USE_FORWARDER       = false;
            static constexpr wchar_t const*     FORWARDER_DEST      = L"";
            static constexpr wchar_t const*     UDP_FWD_PORT_S      = L"";
            static constexpr wchar_t const*     TCP_FWD_PORT_S      = L"";
            static constexpr wchar_t const*     SUFFIX              = L"";
            static constexpr wchar_t const*     DEBUG_LOG_FILENAME  = L"";
            static constexpr wchar_t const*     TAIL_FILENAME       = L"";
            static constexpr wchar_t const*     TAIL_PROGRAMNAME    = L"";
            static constexpr int                BATCH_INTERVAL      = 1000;
            static constexpr wchar_t*           PRIMARY_CERT        = L"PrimaryCert.pem";
            static constexpr wchar_t*           SECONDARY_CERT      = L"SecondaryCert.pem";
			static constexpr char*              VERSION_DETECT_STR  = "detect";
            static constexpr char*              LOGZILLA_VER        = VERSION_DETECT_STR;
            static constexpr int                POLL_INTERVAL_SEC   = 2;
        };

        // Severity levels
        class Severities {
        public:
            static constexpr int                EMERGENCY           = 0;
            static constexpr int                ALERT               = 1;
            static constexpr int                CRITICAL            = 2;
            static constexpr int                ERR                 = 3;
            static constexpr int                WARNING             = 4;
            static constexpr int                NOTICE              = 5;
            static constexpr int                INFORMATIONAL       = 6;
            static constexpr int                DEBUG               = 7;
            static constexpr int                DYNAMIC             = 8;
        };

        // Facility codes
        class Facilities {
        public:
            static constexpr int                KERNEL              = 0;
            static constexpr int                USER                = 1;
            static constexpr int                MAIL                = 2;
            static constexpr int                SYSTEM              = 3;
            static constexpr int                SECAUTH             = 4;
            static constexpr int                SYSLOGD             = 5;
            static constexpr int                LPRINTER            = 6;
            static constexpr int                NETNEWS             = 7;
            static constexpr int                UUCP                = 8;
            static constexpr int                CLOCK               = 9;
            static constexpr int                SECAUTH2            = 10;
            static constexpr int                FTP                 = 11;
            static constexpr int                NTP                 = 12;
            static constexpr int                LOGAUDIT            = 13;
            static constexpr int                LOGALERT            = 14;
            static constexpr int                CLOCK2              = 15;
            static constexpr int                LOCALUSE0           = 16;
            static constexpr int                LOCALUSE1           = 17;
            static constexpr int                LOCALUSE2           = 18;
            static constexpr int                LOCALUSE3           = 19;
            static constexpr int                LOCALUSE4           = 20;
            static constexpr int                LOCALUSE5           = 21;
            static constexpr int                LOCALUSE6           = 22;
            static constexpr int                LOCALUSE7           = 23;
        };

        // Maximum lengths and sizes
        static constexpr int                MAX_SUFFIX_LENGTH       = 1000;
        static constexpr int                SENDER_MAINLOOP_DURATION = 1000;

        // Log formats
        static constexpr int                LOGFORMAT_DETECT        = 0;
        static constexpr int                LOGFORMAT_JSONPORT      = 1;
        static constexpr int                LOGFORMAT_HTTPPORT      = 2;
        static constexpr char const*        LOGFORMAT_LZ_VERSION_HTTP = "6.34";

        class RegistryKey {
        public:
            static constexpr wchar_t const* MAIN_KEY                    = L"SOFTWARE\\LogZilla\\SyslogAgent";
            static constexpr wchar_t const* CHANNELS_KEY                = L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels";
            static constexpr wchar_t const* CONFIG_VERSION              = L"ConfigVersion";
            static constexpr wchar_t const* INCLUDE_VS_IGNORE_EVENT_IDS = L"IncludeVsIgnoreEventIds";
            static constexpr wchar_t const* EVENT_ID_FILTER             = L"EventIDFilterList";
            static constexpr wchar_t const* ONLY_WHILE_RUNNING          = L"OnlyWhileRunning";
            static constexpr wchar_t const* EVENT_LOG_POLL_INTERVAL     = L"EventLogPollInterval";
            static constexpr wchar_t const* FORWARD_TO_SECONDARY        = L"ForwardToMirror";
            static constexpr wchar_t const* LOOKUP_ACCOUNTS             = L"LookupAccountSID";
            static constexpr wchar_t const* EXTRA_KEY_VALUE_PAIRS       = L"ExtraKeyValuePairs";
            static constexpr wchar_t const* PRIMARY_HOST                = L"Syslog";
            static constexpr wchar_t const* PRIMARY_PORT                = L"SendToPort"; // deprecated
            static constexpr wchar_t const* PRIMARY_API_KEY             = L"PrimaryLogZillaApiKey";
            static constexpr wchar_t const* PRIMARY_USE_TLS             = L"PrimaryUseTLS";
            static constexpr wchar_t const* SECONDARY_HOST              = L"Syslog1";
            static constexpr wchar_t const* SECONDARY_PORT              = L"SendToBackupPort";
            static constexpr wchar_t const* SECONDARY_API_KEY           = L"SecondaryLogZillaApiKey";
            static constexpr wchar_t const* SECONDARY_USE_TLS           = L"SecondaryUseTLS";
            static constexpr wchar_t const* FACILITY                    = L"Facility";
            static constexpr wchar_t const* SEVERITY                    = L"Severity";
            static constexpr wchar_t const* SUFFIX                      = L"Suffix";
            static constexpr wchar_t const* DEBUG_LEVEL_SETTING         = L"DebugLevel";
            static constexpr wchar_t const* DEBUG_LOG_FILE              = L"DebugLogFile";
            static constexpr wchar_t const* TAIL_FILENAME               = L"TailFilename";
            static constexpr wchar_t const* TAIL_PROGRAM_NAME           = L"TailProgramName";
            static constexpr wchar_t const* CHANNEL_ENABLED             = L"Enabled";
            static constexpr wchar_t const* CHANNEL_BOOKMARK            = L"Bookmark";
            static constexpr wchar_t const* PRIMARY_TLS_FILENAME        = L"PrimaryTlsFileName";
            static constexpr wchar_t const* SECONDARY_TLS_FILENAME      = L"SecondaryTlsFileName";
            static constexpr wchar_t const* LOGZILLA_REGISTRY_KEY       = L"SOFTWARE\\LogZilla\\SyslogAgent";
            static constexpr wchar_t const* WINDOWS_EVENT_CHANNELS_KEY  = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Channels";
            static constexpr wchar_t const* SELECTED_EVENT_CHANNELS_KEY = L"SOFTWARE\\LogZilla\\SyslogAgent\\Channels";
            static constexpr wchar_t const* INITIAL_SETUP_REG_FILE_KEY  = L"InitialSetupRegFile";
            static constexpr wchar_t const* BATCH_INTERVAL              = L"BatchInterval";
            static constexpr wchar_t const* PRIMARY_BACKWARDS_COMPAT_VER = L"PrimaryBackwardsCompatibleVersion";
            static constexpr wchar_t const* SECONDARY_BACKWARDS_COMPAT_VER = L"SecondaryBackwardsCompatibleVersion";
            static constexpr wchar_t const* INITIAL_SETUP_FILE          = L"InitialSetupRegFile";
        };
    };

#undef SYSLOGAGENT_CURRENT_VERSION

} // namespace SyslogAgent

/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace SyslogAgent
{
    public static class SharedConstants
    {

        public enum Severity { Emergency = 0, Alert = 1, Critical, Error, Warning, 
            Notice, Information, Debug, Dynamic };

        public enum Facility { Kernel = 0, User = 1, Mail, System, SecAuth, Syslogd, 
            LPrinter, NetNews, UUCP, Clock, SecAuth2, FTP, NTP, LogAudit, LogAlert, 
            Clock2, LocalUse0, LocalUse1, LocalUse2, LocalUse3, LocalUse4, LocalUse5, 
            LocalUse6, LocalUse7}

        public static class RegistryKey
        {
            public const string ConfigVersion               =   "ConfigVersion";
            public const string IncludeVsIgnoreEventIds     =   "IncludeVsIgnoreEventIds";
            public const string EventIdFilter               =   "EventIDFilterList";
            public const string OnlyWhileRunning            =   "OnlyWhileRunning";
            public const string EventLogPollInterval        =   "EventLogPollInterval";
            public const string SendToSecondary             =   "ForwardToMirror";
            public const string LookupAccounts              =   "LookupAccountSID";
            //public const string IncludeKeyValuePairs        =   "IncludeKeyValuePairs";
            public const string ExtraKeyValuePairs          =   "ExtraKeyValuePairs";
            public const string SecondaryPort               =   "SendToBackupPort";
            public const string PrimaryHost                 =   "Syslog";
            public const string PrimaryPort                 =   "SendToPort"; // deprecated
            public const string PrimaryApiKey               =   "PrimaryLogZillaApiKey";
            public const string SecondaryHost               =   "Syslog1";
            public const string SecondaryApiKey             =   "SecondaryLogZillaApiKey";
            public const string PrimaryUseTls               =   "PrimaryUseTLS";
            public const string SecondaryUseTls             =   "SecondaryUseTLS";
            public const string UseTCP                      =   "TCPDelivery"; // deprecated
            public const string UsePing                     =   "UsePingBeforeSend"; // deprecated
            public const string Facility                    =   "Facility";
            public const string Severity                    =   "Severity";
            public const string Suffix                      =   "Suffix";
            public const string UseRFC3164                  =   "UseRFC3164";
            public const string UseJSONMessage              =   "UseJsonMessage";
            public const string DebugLevelSetting           =   "DebugLevel";
            public const string DebugLogFile                =   "DebugLogFile";
            public const string TailFilename                =   "TailFilename";
            public const string TailProgramName             =   "TailProgramName";
            public const string ChannelsSubkey              =   "Channels";
            public const string ChannelEnabledName          =   "Enabled";
            public const string PrimaryTlsFileName          =   "PrimaryTlsFileName";
            public const string SecondaryTlsFileName        =   "SecondaryTlsFileName";
            public const string LogzillaRegistryKey         =   @"SOFTWARE\LogZilla\SyslogAgent";
            public const string WindowsEventChannelsKey     =   @"SOFTWARE\Microsoft\Windows\CurrentVersion\WINEVT\Channels";
            public const string SelectedEventChannelsKey    =   @"SOFTWARE\LogZilla\SyslogAgent\Channels";
            public const string InitialSetupRegFileKey      =   @"InitialSetupRegFile";
            public const string ApiTestPath                 =   @"api/";
            public const string BatchInterval               =   "BatchInterval";
            public const string PrimaryBackwardsCompatVer   =   "PrimaryBackwardsCompatibleVersion";
            public const string SecondaryBackwardsCompatVer =   "SecondaryBackwardsCompatibleVersion";
        }

        public static class ConfigDefaults
        {
            public const byte       IncludeVsIgnoreEventIdsB=   0;
            public const string     EventIdFilter           =   "";
            public const byte       OnlyWhileRunning        =   0;
            public const int        EventLogPollInterval    =   10;
            public const byte       SendToSecondaryB        =   0;
            public const byte       LookupAccountsB         =   1;
            public const byte       IncludeKeyValuePairsB   =   0;
            public const byte       Facility                =   20;
            public const byte       Severity                =   8;
            public const string     PrimaryHost             =   "";
            public const string     PrimaryApiKey           =   "";
            public const string     SecondaryHost           =   "";
            public const string     SecondaryApiKey         =   "";
            public const bool       UseForwarder            =   false;
            public const string     ForwarderDest           =   "";
            public const string     UdpFwdPortS             =   "";
            public const string     TcpFwdPortS             =   "";
            public const string     Suffix                  =   "";
            public const byte       PrimaryUseTlsB          =   0;
            public const byte       SecondaryUseTlsB        =   0;
            public const byte       DebugLogLevelB          =   0;
            public const string     DebugLogFilename        =   "syslogagent.log";
            public const string     TailFilename            =   "";
            public const string     TailProgramName         =   "";
            public const string     SyslogAgentHttpPath     =   "SyslogAgentHttpPath";
            public const int        BatchInterval           =   1000;
            public const string     BackwardsCompatVer      =   "detect";
        }


        public const string CurrentConfigVersion        =   "6.30.0.0";
        public const string SyslogAgentExeFilename      =   "syslogagent.exe";
        public const string PrimaryCertFilename         =   "primary.pfx";
        public const string SecondaryCertFilename       =   "secondary.pfx";
        public const string ApiPath                     =   "/api/";
        public const string LogzillaVersionPath         =   "/api/";
        public const string HttpFormatVersion           =   "1.0";
        public static readonly string[] BackwardsCompatVersions = new string[] { "detect", "current", "6.33" };


    }
}


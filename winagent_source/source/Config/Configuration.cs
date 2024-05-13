/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Collections.Generic;

namespace SyslogAgent.Config {

    public class Configuration {

        public int PollInterval { get; set; }
        public bool LookUpAccountIDs { get; set; }
        public bool IncludeVsIgnoreEventIds { get; set; }
        public string EventIdFilter { get; set; }
        public bool OnlyWhileRunning { get; set; }
        public int Facility { get; set; }
        public int Severity { get; set; }
        public bool IncludeKeyValuePairs { get; set; }
        public string Suffix { get; set; }
        public int BatchInterval { get; set; }
        public string PrimaryHost { get; set; }
        public string PrimaryApiKey { get; set; }
        public string SecondaryHost { get; set; }
        public string SecondaryApiKey { get; set; }
        public bool SendToSecondary { get; set; }
        public bool PrimaryUseTls { get; set; }
        public bool SecondaryUseTls { get; set; }
        public int DebugLevel { get; set; }
        public string DebugLogFilename { get; set; }
        public string TailFilename { get; set; }
        public string TailProgramName { get; set; }
        public string PrimaryBackwardsCompatVer { get; set; }
        public string SecondaryBackwardsCompatVer { get; set; }
        public List<EventLogCandidate> EventLogs = new List<EventLogCandidate>();
        public List<string> AllEventLogPaths;
        public IEnumerable<string> SelectedEventLogPaths;
    }
}

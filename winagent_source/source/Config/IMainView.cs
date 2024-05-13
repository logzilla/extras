/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

namespace SyslogAgent.Config {
    public interface IMainView {
        IOptionView LookUpAccount { get; }
        IOptionView SendToSecondary { get; }
        IValidatedOptionView IncludeEventIds { get; }
        IValidatedOptionView IgnoreEventIds { get; }
        IValidatedStringView EventIdFilter { get; }
        IValidatedOptionView OnlyWhileRunning { get; }
        IValidatedOptionView CatchUp { get; }
        IValidatedStringView PrimaryHost { get; }
        IValidatedStringView PrimaryApiKey { get; }
        IValidatedOptionView PrimaryUseTls { get; }
        IValidatedStringView SecondaryHost { get; }
        IValidatedStringView SecondaryApiKey { get; }
        IValidatedOptionView SecondaryUseTls { get; }
        IValidatedStringView Suffix { get; }
        IValidatedStringView BatchInterval { get; }
        string Message { set; }
        void SetFailureMessage(string message);
        void SetSuccessMessage(string message);
        string ChooseImportFileButton();
        string ChooseExportFileButton();
        void UpdateDisplay(Configuration config);
        string Status { set; }
        string LogzillaFileVersion { set; }
        IOptionListView Facility { get; }
        IOptionListView Severity { get; }
        IOptionListView DebugLevel { get; }
        IValidatedStringView DebugLogFilename { get; }
        IValidatedStringView TailFilename { get; }
        IValidatedStringView TailProgramName { get; }

        IOptionListView PrimaryBackwardsCompatVer { get; }

        IOptionListView SecondaryBackwardsCompatVer { get; }
    }
}

/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

namespace SyslogAgent.Config {
    public interface IValidatedStringView {
        string Content { get; set; }
        bool IsValid { set; }
    }
}

/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

namespace SyslogAgent.Config {
    public interface IStringView {
        string Content { get; set; }
    }
}

/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

namespace SyslogAgent.Config {
    public interface ConfigurationModel {
        Configuration Configuration { get; set; }
    }
}

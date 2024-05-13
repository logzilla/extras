/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;

namespace SyslogAgent.Config {
    public interface ServiceModel {
        string Status { get; }
        void Restart(Action<string> showStatus );
    }
}

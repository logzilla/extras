/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

namespace SyslogAgent.Config {
    public interface ISelectionListView {
        void Add(string name, bool isChosen);
        bool IsChosen(int index);
        void SetIsChosen(int index, bool isChosen);
        int Count { get; }
    }
}

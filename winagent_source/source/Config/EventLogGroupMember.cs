/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace SyslogAgent.Config
{
    public class EventLogGroupMember : DependencyObject
    {
        public string Name { get; set; }
        public List<EventLogGroupMember> ChildMembers { get; set; }
        public ObservableCollection<EventLogGroupMember> ObservableChildren
        {
            get
            {
                if (ChildMembers == null)
                    return null;
                return new ObservableCollection<EventLogGroupMember>(ChildMembers);
            }
        }
    }
}

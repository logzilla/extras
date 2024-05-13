/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Collections.Generic;
using System.Windows.Controls;

namespace SyslogAgent.Config {
    public class OptionListButtons : IOptionListView {

        public OptionListButtons(RadioButton[] radioButtons) {
            this.radioButtons = new List<RadioButton>(radioButtons);
        }

        public int Option {
            get { return radioButtons.FindIndex(b => b.IsChecked.GetValueOrDefault()); }
            set { radioButtons[value].IsChecked = true; }
        }

        readonly List<RadioButton> radioButtons;
    }
}

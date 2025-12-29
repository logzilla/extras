/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Windows.Controls;

namespace SyslogAgent.Config {
    public class OptionListCombo: IOptionListView {
        public OptionListCombo(ComboBox comboBox) {
            this.comboBox = comboBox;
        }

        public int Option {
            get { return comboBox.SelectedIndex; }
            set { comboBox.SelectedIndex = value; }
        }

        readonly ComboBox comboBox;
    }
}

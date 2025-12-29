/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Windows.Controls;

namespace SyslogAgent.Config {
    public class ValidatedOptionCheckBox: IValidatedOptionView {

        public ValidatedOptionCheckBox(CheckBox checkBox) {
            this.checkBox = checkBox;
        }

        public bool IsSelected {
            get { return checkBox.IsChecked.GetValueOrDefault(); }
            set { checkBox.IsChecked = value; }
        }

        bool IValidatedOptionView.IsValid { get; set; }

        readonly CheckBox checkBox;
    }
}

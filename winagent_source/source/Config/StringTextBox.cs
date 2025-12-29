/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Windows.Controls;

namespace SyslogAgent.Config {
    public class StringTextBox: IStringView {
        public StringTextBox(TextBox textBox) {
            this.textBox = textBox;
        }

        public string Content {
            get { return textBox.Text; }
            set { textBox.Text = value; }
        }
        readonly TextBox textBox;
    }
}

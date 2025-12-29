/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace SyslogAgent.Config {
    public class ValidatedTextBox: IValidatedStringView {
        public ValidatedTextBox(TextBox textBox) {
            this.textBox = textBox;
        }

        public string Content {
            get { return textBox.Text; }
            set { textBox.Text = value; }
        }

        public bool IsValid {
            set { textBox.Foreground = value ? SystemColors.ControlTextBrush 
                    : new SolidColorBrush(Color.FromRgb(255, 0, 0)); }
        }

        readonly TextBox textBox;
    }
}

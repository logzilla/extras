/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Windows.Controls;

namespace SyslogAgent.Config {
    public class ValidatedOptionRadioButton : IValidatedOptionView {
        private readonly RadioButton _radioButton;

        public ValidatedOptionRadioButton( RadioButton radioButton )
        {
            _radioButton = radioButton;
        }

        public bool IsSelected
        {
            get => _radioButton.IsChecked ?? false;
            set => _radioButton.IsChecked = value;
        }
        bool IValidatedOptionView.IsValid { get; set; }
    }
}

using System.Windows.Controls;

namespace SyslogAgent.Config
{
    public class DebugLevelOptionListCombo : IOptionListView
    {
        private readonly ComboBox _comboBox;

        public DebugLevelOptionListCombo(ComboBox comboBox)
        {
            _comboBox = comboBox;
        }

        public int Option
        {
            get
            {
                // SelectedValue will be the ConfigValue from LogLevelChoice, which is an int.
                if (_comboBox.SelectedValue is int selectedValue)
                {
                    return selectedValue;
                }
                // Default or error case: if nothing is selected or value is unexpected.
                // Corresponds to "INFO" (ConfigValue 4 from our table, which is agent level INFO via registry value 5)
                return 4; 
            }
            set
            {
                // 'value' will be config.DebugLevel when loading.
                // This will make the ComboBox select the LogLevelChoice item
                // whose ConfigValue matches 'value'.
                _comboBox.SelectedValue = value;
            }
        }
    }
}

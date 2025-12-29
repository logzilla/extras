/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;

namespace SyslogAgent.Config {
    public class SelectionListBox : ISelectionListView {
        public SelectionListBox(ListBox listBox) {
            this.listBox = listBox;
        }

        public void Add(string name, bool isChosen) {
            var panel = new StackPanel {Orientation = Orientation.Horizontal};
            panel.Children.Add(new CheckBox {IsChecked = isChosen, Margin 
                = new Thickness(1)});
            var binding = new Binding {
                Path = new PropertyPath("Foreground"),
                RelativeSource = new RelativeSource(RelativeSourceMode.FindAncestor, 
                typeof(ListBoxItem), 1)
            };
            var text = new TextBlock {
                Text = name,
                Margin = new Thickness(1)
            };
            text.SetBinding(TextBlock.ForegroundProperty, binding);
            panel.Children.Add(text);
            panel.Tag = listBox.Items.Count;
            listBox.Items.Add(panel);
        }

        public bool IsChosen(int index) {
            var panel = (Panel) listBox.Items[index];
            var checkBox = (CheckBox)panel.Children[0];
            return checkBox.IsChecked.GetValueOrDefault();
        }

        public void SetIsChosen(int index, bool isChosen) {
            var panel = (Panel) listBox.Items[index];
            var checkBox = (CheckBox)panel.Children[0];
            checkBox.IsChecked = isChosen;
        }

        public int Count { get { return listBox.Items.Count; } }

        readonly ListBox listBox;
    }
}

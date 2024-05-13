/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Forms;
using System.Windows.Media;
using System.Collections;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace SyslogAgent.Config
{
    public class MenuItem
    {
        public MenuItem()
        {
            this.Items = new ObservableCollection<MenuItem>();
        }

        public string Name { get; set; }

        public ObservableCollection<MenuItem> Items { get; set; }
    }

    public partial class MainWindow : IMainView
    {

        public string previous_debug_level_string = "";
        public string previous_debug_log_filename_string = "";
        public ObservableCollection<EventLogGroupMember> logMembers;
        public EventLogGroupMember member;
        public ObservableCollection<EventLogTreeviewItem> treeviewItems;
        public List<MenuItem> menuItems;
        public ItemCollection itemCollection;

        void SetParents(EventLogGroupMember parent, ObservableCollection<EventLogGroupMember> children)
        {
            if (parent.ObservableChildren == null)
                return;
            foreach (var member in parent.ObservableChildren)
            {
                member.SetValue(EventLogTreeViewItemHelper.ParentProperty, parent);
                if (member.ObservableChildren != null)
                {
                    SetParents(member, member.ObservableChildren);
                }
            }
        }

        public MainWindow()
        {
            InitializeComponent();
            presenter = new MainPresenter(this, new Registry(), new Configuration(), new AgentService());
            presenter.Load();
            SetSecondaryUseTLSAvailable(secondaryUseTlsCheck.IsChecked ?? false);
            SetTailProgramAvailable();
            primaryBackwardsCompatVerCombo.ItemsSource = SharedConstants.BackwardsCompatVersions;
            secondaryBackwardsCompatVerCombo.ItemsSource = SharedConstants.BackwardsCompatVersions;

            var root = presenter.eventLogTreeviewRoot;
            //root.SetIsCheckedAll(false);
            treeView.ItemsSource = root.Children;

        }

        // public IValidatedStringView PollInterval => new ValidatedTextBox(pollIntervalText);
        public IOptionView LookUpAccount => new ValidatedOptionCheckBox(lookUpAccountCheck);
        //public IOptionView IncludeKeyValuePairs => new ValidatedOptionCheckBox(includeKeyValueCheck);
        public IOptionView SendToSecondary => new ValidatedOptionCheckBox(sendToSecondaryCheck);
        public IValidatedOptionView PrimaryUseTls => new ValidatedOptionCheckBox(primaryUseTlsCheck);
        public IValidatedOptionView SecondaryUseTls => new ValidatedOptionCheckBox(secondaryUseTlsCheck);
        public IValidatedOptionView IncludeEventIds => new ValidatedOptionRadioButton(radioInclude);
        public IValidatedOptionView IgnoreEventIds => new ValidatedOptionRadioButton(radioIgnore);
        public IValidatedStringView EventIdFilter => new ValidatedTextBox(eventIdFilterText);
        public IValidatedOptionView OnlyWhileRunning => new ValidatedOptionRadioButton(radioOnlyWhileRunning);
        public IValidatedOptionView CatchUp => new ValidatedOptionRadioButton(radioCatchUp);
        public IValidatedStringView Suffix => new ValidatedTextBox(suffixText);
        public IValidatedStringView PrimaryHost => new ValidatedTextBox( primaryHostText );
        public IValidatedStringView PrimaryApiKey => new ValidatedTextBox( primaryApiKeyText );
        public IValidatedStringView SecondaryHost => new ValidatedTextBox(secondaryHostText);
        public IValidatedStringView SecondaryApiKey => new ValidatedTextBox(secondaryApiKeyText);
        // public SelectionListView Logs => new SelectionListBox(logsList);
        public IOptionListView Facility => new OptionListCombo(facilityCombo);
        public IOptionListView Severity => new OptionListCombo(severityCombo);
        public IOptionListView DebugLevel => new OptionListCombo(debugLevelCombo);
        public IValidatedStringView DebugLogFilename => new ValidatedTextBox(debugLogFilename);
        public IValidatedStringView TailFilename => new ValidatedTextBox(txtTailFilename);
        public IValidatedStringView TailProgramName => new ValidatedTextBox(txtTailProgramName);
        public IValidatedStringView BatchInterval => new ValidatedTextBox(batchIntervalText);
        public IOptionListView PrimaryBackwardsCompatVer => new OptionListCombo(primaryBackwardsCompatVerCombo);
        public IOptionListView SecondaryBackwardsCompatVer => new OptionListCombo(secondaryBackwardsCompatVerCombo);
        public string Message { set { txtBlockStatusBarLeft.Text = value; } }
        public string LogzillaFileVersion { set { tbkLogzillaVersion.Text = "LogZilla Syslog Agent version " + value; } }


        public string Status
        {
            set { txtBlockStatusBarRight.Dispatcher.BeginInvoke(new Action(() => { txtBlockStatusBarRight.Text = value; })); }
        }

        public string MessageBlock
        {
            set { txtBlockStatusBarLeft.Dispatcher.BeginInvoke(new Action(() => { txtBlockStatusBarLeft.Text = value; })); }
        }


        public void SetFailureMessage(string message)
        {
            txtBlockStatusBarLeft.Text = message;
            txtBlockStatusBarLeft.Foreground = new SolidColorBrush(Color.FromRgb(255, 0, 0));
        }

        public void SetSuccessMessage(string message)
        {
            txtBlockStatusBarLeft.Text = message;
            txtBlockStatusBarLeft.Foreground = SystemColors.ControlTextBrush;
        }

        void ImportButton_OnClick(object sender, RoutedEventArgs e)
        {
            presenter.Import();
        }

        void ExportButton_OnClick(object sender, RoutedEventArgs e)
        {
            presenter.Export();
        }

        void SaveButton_OnClick( object sender, RoutedEventArgs e )
        {
            // Set the cursor to wait
            Mouse.OverrideCursor = System.Windows.Input.Cursors.Wait;
            saveButton.IsEnabled = false;

            try
            {
                presenter.Save();
            }
            finally
            {
                // Reset the cursor to the default arrow cursor
                Mouse.OverrideCursor = null;
                saveButton.IsEnabled = true;
            }
        }


        void RestartButton_OnClick(object sender, RoutedEventArgs e)
        {
            presenter.Restart();
        }

        void SelectAllButton_OnClick(object sender, RoutedEventArgs e)
        {
            presenter.SetAllChosen(true);
        }

        void UnselectAllButton_OnClick(object sender, RoutedEventArgs e)
        {
            presenter.SetAllChosen(false);
        }

        void UIElement_OnPreviewMouseDown(object sender, MouseButtonEventArgs e)
        {
            presenter.PreviewInput();
        }

        void UIElement_OnPreviewKeyDown(object sender, System.Windows.Input.KeyEventArgs e)
        {
            presenter.PreviewInput();
        }

        readonly MainPresenter presenter;

        private void DebugLevelChangedEventHandler(object sender, System.Windows.Controls.SelectionChangedEventArgs args)
        {
            ComboBoxItem old_box = (ComboBoxItem)debugLevelCombo.SelectedItem;
            string old_box_value = (string)old_box.Content;
            ComboBoxItem new_box = (ComboBoxItem)args.AddedItems[0];
            string new_box_value = (string)new_box.Content;
            debugLogFilename.IsEnabled = new_box_value != "None";
            if (previous_debug_level_string == "") // this is the initial load
            {
                previous_debug_level_string = old_box_value;
                previous_debug_log_filename_string = debugLogFilename.Text == "" ? SharedConstants.ConfigDefaults.DebugLogFilename : debugLogFilename.Text;
                if (new_box_value == "None")
                {
                    debugLogFilename.Text = "";
                }
            }
            else
            {
                if (previous_debug_level_string == "None" && new_box_value != "None" && debugLogFilename.Text == "")
                {
                    previous_debug_level_string = new_box_value;
                    debugLogFilename.Text = previous_debug_log_filename_string;
                }
                else if (previous_debug_level_string != "None" && new_box_value == "None")
                {
                    previous_debug_level_string = new_box_value;
                    previous_debug_log_filename_string = debugLogFilename.Text;
                    debugLogFilename.Text = "";
                }
            }
        }

        private void ChooseCertFileButton_Click( object sender, RoutedEventArgs e )
        {
            System.Windows.Controls.Button button_clicked = (System.Windows.Controls.Button)sender;
            bool is_secondary = button_clicked.Name == "chooseSecondaryCertFileButton";
            using( OpenFileDialog open_file_dialog = new OpenFileDialog() )
            {
                // Set the default file filter to .pfx files
                open_file_dialog.Filter = "PFX files (*.pfx)|*.pfx";

                // Optionally set FilterIndex to 1 if you want the first (and in this case, the only) filter option to be selected by default
                open_file_dialog.FilterIndex = 1;

                DialogResult dialog_result = open_file_dialog.ShowDialog();
                if( dialog_result == System.Windows.Forms.DialogResult.OK )
                {
                    WriteCertFile( open_file_dialog.FileName, is_secondary );
                    if( is_secondary )
                    {
                        Globals.SecondaryTlsFilename = open_file_dialog.FileName;
                    }
                    else
                    {
                        Globals.PrimaryTlsFilename = open_file_dialog.FileName;
                    }
                }
                else
                {
                    System.Windows.MessageBox.Show( "Cancelled" );
                }
            }
        }

        private string GetCertFileDirectory()
        {
            if (File.Exists(Globals.ExeFilePath + SharedConstants.SyslogAgentExeFilename))
            {
                return Globals.ExeFilePath;
            }
            using (FolderBrowserDialog folder_browser_dialog = new FolderBrowserDialog())
            {
                folder_browser_dialog.Description = "Choose the directory with syslogagent.exe";
                while (true)
                {
                    DialogResult dialog_result = folder_browser_dialog.ShowDialog();
                    if (dialog_result == System.Windows.Forms.DialogResult.OK)
                    {
                        string selected_path = folder_browser_dialog.SelectedPath;
                        if (File.Exists(selected_path + "\\" + SharedConstants.SyslogAgentExeFilename))
                        {
                            Globals.ExeFilePath = selected_path + "\\";
                            return selected_path + "\\";
                        }
                        // otherwise
                        System.Windows.Forms.MessageBox.Show("syslogagent.exe not found in that directory");
                    }
                    else if (dialog_result == System.Windows.Forms.DialogResult.Cancel)
                    {
                        return null;
                    }
                }
            }
        }

        private void WriteCertFile(string source_filename, bool is_secondary)
        {
            string certfile_directory = GetCertFileDirectory();
            if (certfile_directory == null)
            {
                System.Windows.MessageBox.Show("Cancelled");
                return;
            }
            string certfile_path = certfile_directory + (is_secondary ? SharedConstants.SecondaryCertFilename : SharedConstants.PrimaryCertFilename);
            if (source_filename == certfile_path)
            {
                System.Windows.MessageBox.Show("That is the existing cert file. No changes made.");
                return;
            }
            File.Copy(source_filename, certfile_path, true);
            System.Windows.MessageBox.Show((is_secondary ? "Secondary" : "Primary") + " cert loaded from " + source_filename + ", saved to " + certfile_path);
        }

        private void TailFilename_OnClick(object sender, RoutedEventArgs e)
        {
            using (OpenFileDialog open_file_dialog = new OpenFileDialog())
            {
                DialogResult dialog_result = open_file_dialog.ShowDialog();
                if (dialog_result == System.Windows.Forms.DialogResult.OK)
                {
                    txtTailFilename.Text = open_file_dialog.FileName;
                    txtTailProgramName.IsEnabled = true;
                }
                else
                {
                    System.Windows.MessageBox.Show("Cancelled");
                }
            }
        }

        private void sendToSecondaryCheck_Checked(object sender, RoutedEventArgs e)
        {
            SetSecondaryUseTLSAvailable(true);
        }

        private void sendToSecondaryCheck_Unchecked(object sender, RoutedEventArgs e)
        {
            SetSecondaryUseTLSAvailable(false);
        }

        private void SetSecondaryUseTLSAvailable(bool available)
        {
            if (!available)
                secondaryUseTlsCheck.IsChecked = false;
            secondaryUseTlsCheck.IsEnabled = available;
            chooseSecondaryCertFileButton.IsEnabled = available;

        }


        private void SetTailProgramAvailable()
        {
            txtTailProgramName.IsEnabled = !string.IsNullOrEmpty(txtTailFilename.Text);
        }

        private void txtTailFilename_LostFocus(object sender, RoutedEventArgs e)
        {
            txtTailFilename.Text = txtTailFilename.Text.Trim();
            if (txtTailFilename.Text == "") { 
                txtTailProgramName.Text = "";
                txtTailProgramName.IsEnabled = false;
            }
            else
            {
                txtTailProgramName.IsEnabled = true;
            }
            SetTailProgramAvailable();
        }

        public string ChooseImportFileButton()
        {
            using (var import_file_dialog = new OpenFileDialog())
            {
                import_file_dialog.Filter = "reg files (*.reg)|*.reg|All files (*.*)|*.*";
                if (import_file_dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
                {
                    return import_file_dialog.FileName;
    }
            }
            return null;
        }

        public string ChooseExportFileButton()
        {
            using (var save_file_dialog = new SaveFileDialog())
            {
                save_file_dialog.Filter = "Registry Files (*.reg)|*.reg";
                save_file_dialog.DefaultExt = "reg";
                save_file_dialog.AddExtension = true;
                DialogResult dialog_result = save_file_dialog.ShowDialog();
                if (dialog_result == System.Windows.Forms.DialogResult.OK)
                {
                    return save_file_dialog.FileName;
                }
                else
                {
                    return null;
                }
            }
        }

        public void UpdateDisplay(Configuration config)
        {
            LookUpAccount.IsSelected = config.LookUpAccountIDs;
            SendToSecondary.IsSelected = config.SendToSecondary;
            PrimaryUseTls.IsSelected = config.PrimaryUseTls;
            SecondaryUseTls.IsSelected = config.SecondaryUseTls;
            if (config.IncludeVsIgnoreEventIds)
            {
                radioInclude.IsChecked = true;
                radioIgnore.IsChecked = false;
            }
            else
            {
                radioInclude.IsChecked = false;
                radioIgnore.IsChecked = true;
            }
            EventIdFilter.Content = config.EventIdFilter;
            Suffix.Content = config.Suffix;
            PrimaryHost.Content = config.PrimaryHost;
            PrimaryApiKey.Content = config.PrimaryApiKey;
            SecondaryHost.Content = config.SecondaryHost;
            SecondaryApiKey.Content = config.SecondaryApiKey;
            Facility.Option = config.Facility;
            Severity.Option = config.Severity;
            DebugLevel.Option = config.DebugLevel;
            DebugLogFilename.Content = config.DebugLogFilename;
            TailFilename.Content = config.TailFilename;
            TailProgramName.Content = config.TailProgramName;
            BatchInterval.Content = Convert.ToString(config.BatchInterval);
            presenter.RecheckEventPaths(config.SelectedEventLogPaths);
        }

        private void primaryHost_LostFocus( object sender, RoutedEventArgs e )
        {
            // Your logic here
            // For example, you can retrieve the current text of the TextBox like this:
            var textBox = sender as System.Windows.Controls.TextBox;
            if( textBox != null )
            {
                string currentText = textBox.Text;
                if (currentText.StartsWith("http:"))
                {
                    primaryUseTlsCheck.IsChecked = false;
                }
                else if (currentText.StartsWith("https:"))
                {
                    primaryUseTlsCheck.IsChecked = true;
                }
                else
                {
                    if (primaryUseTlsCheck.IsChecked == true)
                    {
                        textBox.Text = "https://" + currentText;
                    }
                    else
                    {
                        textBox.Text = "http://" + currentText;
                    }
                }
            }
        }

        private void secondaryHost_LostFocus( object sender, RoutedEventArgs e )
        {
            // Your logic here
            // For example, you can retrieve the current text of the TextBox like this:
            var textBox = sender as System.Windows.Controls.TextBox;
            if( textBox != null )
            {
                string currentText = textBox.Text;
                if( currentText.StartsWith( "http:" ) )
                {
                    secondaryUseTlsCheck.IsChecked = false;
                }
                else if( currentText.StartsWith( "https:" ) )
                {
                    secondaryUseTlsCheck.IsChecked = true;
                }
                else
                {
                    if( secondaryUseTlsCheck.IsChecked == true )
                    {
                        textBox.Text = "https://" + currentText;
                    }
                    else
                    {
                        textBox.Text = "http://" + currentText;
                    }
                }
            }
        }


        private void primaryUseTlsCheck_Checked( object sender, RoutedEventArgs e )
        {
            // Logic for when the CheckBox is checked
            if (primaryHostText.Text.StartsWith("http://"))
            {
                primaryHostText.Text = "https://" + primaryHostText.Text.Substring(7);
            }
            else if (!primaryHostText.Text.StartsWith("https://"))
            {
                primaryHostText.Text = "https://" + primaryHostText.Text;
            }
        }

        private void primaryUseTlsCheck_Unchecked( object sender, RoutedEventArgs e )
        {
            // Logic for when the CheckBox is unchecked
            if (primaryHostText.Text.StartsWith("https://"))
            {
                primaryHostText.Text = "http://" + primaryHostText.Text.Substring(8);
            }
            else if (!primaryHostText.Text.StartsWith("http://"))
            {
                primaryHostText.Text = "http://" + primaryHostText.Text;
            }
        }

        private void secondaryUseTlsCheck_Checked(object sender, RoutedEventArgs e)
        {
            if (secondaryHostText.Text.StartsWith("http://"))
            {
                secondaryHostText.Text = "https://" + secondaryHostText.Text.Substring(7);
            }
            else if (!secondaryHostText.Text.StartsWith("https://"))
            {
                secondaryHostText.Text = "https://" + secondaryHostText.Text;
            }
        }

        private void secondaryUseTlsCheck_Unchecked(object sender, RoutedEventArgs e)
        {
            if (secondaryHostText.Text.StartsWith("https://"))
            {
                secondaryHostText.Text = "http://" + secondaryHostText.Text.Substring(8);
            }
            else if (!secondaryHostText.Text.StartsWith("http://"))
            {
                secondaryHostText.Text = "http://" + secondaryHostText.Text;
            }
        }

    }


}

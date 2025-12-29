/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright 2021 LogZilla Corp.
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
using System.Windows.Threading;

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

    public partial class MainWindow : Window, IMainView
    {
        private readonly MainPresenter presenter;
        public string previous_debug_level_string = ""; // This will now store the DisplayName from LogLevelChoice
        public string previous_debug_log_filename_string = "";
        public ObservableCollection<EventLogGroupMember> logMembers;
        public EventLogGroupMember member;
        public ObservableCollection<EventLogTreeviewItem> treeviewItems;
        public List<MenuItem> menuItems;
        public ItemCollection itemCollection;
        private DispatcherTimer statusCheckTimer; // Timer to check service status periodically

        void SetParents(EventLogGroupMember parent, ObservableCollection<EventLogGroupMember> children)
        {
            Logger.LogMethodEntry();
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
            Logger.LogMethodExit();
        }

        public MainWindow()
        {
            using (Logger.LogScope("MainWindow Initialization"))
            {
                try
                {
                    InitializeComponent();
                    Logger.LogInfo("Window components initialized");

                    // Populate DebugLevel ComboBox
                    debugLevelCombo.ItemsSource = new List<LogLevelChoice>
                    {
                        new LogLevelChoice { DisplayName = "FATAL", ConfigValue = -2 },
                        new LogLevelChoice { DisplayName = "ALWAYS", ConfigValue = 0 },
                        new LogLevelChoice { DisplayName = "CRITICAL", ConfigValue = 1 },
                        new LogLevelChoice { DisplayName = "RECOVERABLE_ERROR", ConfigValue = 2 },
                        new LogLevelChoice { DisplayName = "WARN", ConfigValue = 3 },
                        new LogLevelChoice { DisplayName = "INFO", ConfigValue = 4 },    // Default for DebugLevelOptionListCombo getter if no selection
                        new LogLevelChoice { DisplayName = "VERBOSE", ConfigValue = 5 },
                        new LogLevelChoice { DisplayName = "DEBUG", ConfigValue = 6 },
                        new LogLevelChoice { DisplayName = "DEBUG2", ConfigValue = 7 },
                        new LogLevelChoice { DisplayName = "DEBUG3", ConfigValue = 8 },
                        new LogLevelChoice { DisplayName = "NONE", ConfigValue = 9 }
                    };
                    // debugLevelCombo.SelectedValuePath = "ConfigValue"; // Already set in XAML
                    // debugLevelCombo.DisplayMemberPath = "DisplayName"; // Already set in XAML

                    presenter = new MainPresenter(this, new Registry(), new Configuration(), new AgentService());
                    Logger.LogInfo("Presenter initialized");

                    presenter.Load(); // This will call UpdateDisplay, which sets debugLevelCombo.SelectedValue
                    Logger.LogInfo("Configuration loaded");

                    SetSecondaryUseSelfSignedCertAvailable(sendToSecondaryCheck.IsChecked ?? false);
                    SetTailProgramAvailable();
                    primaryBackwardsCompatVerCombo.ItemsSource = SharedConstants.BackwardsCompatVersions;
                    secondaryBackwardsCompatVerCombo.ItemsSource = SharedConstants.BackwardsCompatVersions;

                    var root = presenter.eventLogTreeviewRoot;
                    treeView.ItemsSource = root.Children;
                    Logger.LogInfo("Main window initialization complete");
                    
                    // Initialize and start the service status check timer
                    InitializeStatusCheckTimer();
                    
                    // Subscribe to the window closing event to properly clean up the timer
                    this.Closing += MainWindow_Closing;
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to initialize MainWindow", ex);
                    throw;
                }
            }
        }

        private void InitializeStatusCheckTimer()
        {
            using (Logger.LogScope("InitializeStatusCheckTimer"))
            {
                try
                {
                    // Create and configure the timer with a 1-second interval
                    statusCheckTimer = new DispatcherTimer
                    {
                        Interval = TimeSpan.FromSeconds(1)
                    };
                    
                    // Add the tick event handler
                    statusCheckTimer.Tick += StatusCheckTimer_Tick;
                    
                    // Start the timer
                    statusCheckTimer.Start();
                    
                    Logger.LogInfo("Status check timer initialized and started");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to initialize status check timer", ex);
                    // Don't throw the exception as this is not critical functionality
                }
            }
        }
        
        private void StatusCheckTimer_Tick(object sender, EventArgs e)
        {
            try
            {
                // Get the current service status and update the UI
                presenter.UpdateServiceStatus();
            }
            catch (Exception ex)
            {
                Logger.LogError("Error in status check timer tick", ex);
                // Don't throw as this is running on the UI thread
            }
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            using (Logger.LogScope("MainWindow_Closing"))
            {
                try
                {
                    // Stop the timer when the window is closing
                    if (statusCheckTimer != null)
                    {
                        statusCheckTimer.Stop();
                        statusCheckTimer.Tick -= StatusCheckTimer_Tick;
                        statusCheckTimer = null;
                        Logger.LogInfo("Status check timer stopped and cleaned up");
                    }
                }
                catch (Exception ex)
                {
                    Logger.LogError("Error during window closing cleanup", ex);
                    // Don't throw as this would interfere with window closing
                }
            }
        }

        // Interface implementations
        public IOptionView LookUpAccount => new ValidatedOptionCheckBox(lookUpAccountCheck);
        public IOptionView SendToSecondary => new ValidatedOptionCheckBox(sendToSecondaryCheck);
        public IValidatedOptionView IncludeEventIds => new ValidatedOptionRadioButton(radioInclude);
        public IValidatedOptionView IgnoreEventIds => new ValidatedOptionRadioButton(radioIgnore);
        public IValidatedStringView EventIdFilter => new ValidatedTextBox(eventIdFilterText);
        public IValidatedOptionView OnlyWhileRunning => new ValidatedOptionRadioButton(radioOnlyWhileRunning);
        public IValidatedOptionView CatchUp => new ValidatedOptionRadioButton(radioCatchUp);
        public IValidatedStringView PrimaryHost => new ValidatedTextBox(primaryHostText);
        public IValidatedStringView PrimaryApiKey => new ValidatedTextBox(primaryApiKeyText);
        public IValidatedOptionView PrimaryUseSelfSignedCert => new ValidatedOptionCheckBox(primaryUseSelfSignedCertCheck);
        public IValidatedStringView SecondaryHost => new ValidatedTextBox(secondaryHostText);
        public IValidatedStringView SecondaryApiKey => new ValidatedTextBox(secondaryApiKeyText);
        public IValidatedOptionView SecondaryUseSelfSignedCert => new ValidatedOptionCheckBox(secondaryUseSelfSignedCertCheck);
        public IValidatedStringView Suffix => new ValidatedTextBox(suffixText);
        public IOptionListView Facility => new OptionListCombo(facilityCombo);
        public IOptionListView Severity => new OptionListCombo(severityCombo);
        public IOptionListView DebugLevel => new DebugLevelOptionListCombo(debugLevelCombo); // Changed to use new wrapper
        public IValidatedStringView DebugLogFilename => new ValidatedTextBox(debugLogFilename);
        public IValidatedStringView TailFilename => new ValidatedTextBox(txtTailFilename);
        public IValidatedStringView TailProgramName => new ValidatedTextBox(txtTailProgramName);
        public IOptionListView PrimaryBackwardsCompatVer => new OptionListCombo(primaryBackwardsCompatVerCombo);
        public IOptionListView SecondaryBackwardsCompatVer => new OptionListCombo(secondaryBackwardsCompatVerCombo);

        public IValidatedStringView MaxBatchSize => new ValidatedTextBox(maxBatchSizeText);
        public IValidatedStringView MaxBatchAge => new ValidatedTextBox(maxBatchAgeText);

        public string Message
        {
            set
            {
                Logger.LogInfo($"Setting message: {value}");
                txtBlockStatusBarLeft.Text = value;
            }
        }

        public string Status
        {
            set
            {
                txtBlockStatusBarRight.Dispatcher.BeginInvoke(new Action(() => { txtBlockStatusBarRight.Text = value; }));
            }
        }

        public string LogzillaFileVersion
        {
            set
            {
                Logger.LogInfo($"Setting LogZilla version: {value}");
                tbkLogzillaVersion.Text = "LogZilla Syslog Agent version " + value;
            }
        }

        public void SetFailureMessage(string message)
        {
            Logger.LogWarning($"Failure message: {message}");
            txtBlockStatusBarLeft.Text = message;
            txtBlockStatusBarLeft.Foreground = new SolidColorBrush(Color.FromRgb(255, 0, 0));
        }

        public void SetSuccessMessage(string message)
        {
            Logger.LogInfo($"Success message: {message}");
            txtBlockStatusBarLeft.Text = message;
            txtBlockStatusBarLeft.Foreground = SystemColors.ControlTextBrush;
        }

        void SaveButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("SaveOperation"))
            {
                Mouse.OverrideCursor = System.Windows.Input.Cursors.Wait;
                saveButton.IsEnabled = false;

                try
                {
                    Logger.LogInfo("About to call presenter.Save()");
                    presenter.Save();
                    Logger.LogInfo("Save operation completed successfully");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Save operation failed", ex);
                    throw;
                }
                finally
                {
                    Mouse.OverrideCursor = null;
                    saveButton.IsEnabled = true;
                    Logger.LogInfo("Save operation cleanup completed");
                }
            }
        }

        void ImportButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("ImportOperation"))
            {
                try
                {
                    presenter.Import();
                    Logger.LogInfo("Import operation completed");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Import operation failed", ex);
                    throw;
                }
            }
        }

        void ExportButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("ExportOperation"))
            {
                try
                {
                    presenter.Export();
                    Logger.LogInfo("Export operation completed");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Export operation failed", ex);
                    throw;
                }
            }
        }

        private void DebugLevelChangedEventHandler(object sender, SelectionChangedEventArgs args)
        {
            using (Logger.LogScope("DebugLevelChange"))
            {
                try
                {
                    LogLevelChoice oldChoice = debugLevelCombo.SelectedItem as LogLevelChoice;
                    string oldBoxValue = oldChoice?.DisplayName ?? previous_debug_level_string; // Get display name or last known

                    if (args.AddedItems.Count > 0 && args.AddedItems[0] is LogLevelChoice newChoice)
                    {
                        string newBoxValue = newChoice.DisplayName;
                        Logger.LogInfo($"Debug level changing from {oldBoxValue} to {newBoxValue}");

                        debugLogFilename.IsEnabled = newChoice.DisplayName != "NONE";

                        // If the new selection is "None" and previous wasn't, clear and save filename
                        if (previous_debug_level_string != "NONE" && newChoice.DisplayName == "NONE")
                        {
                            previous_debug_log_filename_string = debugLogFilename.Text;
                            debugLogFilename.Text = "";
                        }
                        // If the new selection is not "None" but previous was "None", restore filename
                        else if (previous_debug_level_string == "NONE" && newChoice.DisplayName != "NONE")
                        {
                            debugLogFilename.Text = previous_debug_log_filename_string;
                        }
                        previous_debug_level_string = newChoice.DisplayName; // Update for next change
                    }
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to change debug level", ex);
                    throw;
                }
            }
        }

        // ... [Rest of the original methods with logging added]

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
                return null;
            }
        }

        public void UpdateDisplay(Configuration config)
        {
            using (Logger.LogScope("UpdateDisplay"))
            {
                try
                {
                    Logger.LogInfo("Updating display with new configuration");
                    LookUpAccount.IsSelected = config.LookUpAccountIDs;
                    SendToSecondary.IsSelected = config.SendToSecondary;
                    PrimaryUseSelfSignedCert.IsSelected = config.PrimaryUseSelfSignedCert;
                    SecondaryUseSelfSignedCert.IsSelected = config.SecondaryUseSelfSignedCert;

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

                    presenter.RecheckEventPaths(config.SelectedEventLogPaths);
                    Logger.LogInfo("Display update completed");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to update display", ex);
                    throw;
                }
            }
        }

        private void sendToSecondaryCheck_Checked(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("SecondaryCheckChange"))
            {
                Logger.LogInfo("Secondary server option enabled");
                SetSecondaryUseSelfSignedCertAvailable(true);
            }
        }

        private void sendToSecondaryCheck_Unchecked(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("SecondaryCheckChange"))
            {
                Logger.LogInfo("Secondary server option disabled");
                SetSecondaryUseSelfSignedCertAvailable(false);
            }
        }

        private void SetSecondaryUseSelfSignedCertAvailable(bool available)
        {
            Logger.LogMethodEntry();
            Logger.LogInfo($"Setting secondary TLS availability: {available}");

            if (!available)
                secondaryUseSelfSignedCertCheck.IsChecked = false;
            secondaryUseSelfSignedCertCheck.IsEnabled = available;
            chooseSecondaryCertFileButton.IsEnabled = available;

            Logger.LogMethodExit();
        }

        private void SetTailProgramAvailable()
        {
            Logger.LogMethodEntry();
            bool isEnabled = !string.IsNullOrEmpty(txtTailFilename.Text);
            Logger.LogInfo($"Setting tail program availability: {isEnabled}");
            txtTailProgramName.IsEnabled = isEnabled;
            Logger.LogMethodExit();
        }

        private string GetCertFileDirectory()
        {
            using (Logger.LogScope("GetCertFileDirectory"))
            {
                if (File.Exists(Globals.ExeFilePath + SharedConstants.SyslogAgentExeFilename))
                {
                    Logger.LogInfo($"Found existing exe path: {Globals.ExeFilePath}");
                    return Globals.ExeFilePath;
                }

                using (FolderBrowserDialog folderBrowserDialog = new FolderBrowserDialog())
                {
                    folderBrowserDialog.Description = "Choose the directory with syslogagent.exe";
                    while (true)
                    {
                        Logger.LogInfo("Prompting user to select directory");
                        DialogResult dialogResult = folderBrowserDialog.ShowDialog();

                        if (dialogResult == System.Windows.Forms.DialogResult.OK)
                        {
                            string selectedPath = folderBrowserDialog.SelectedPath;
                            Logger.LogInfo($"User selected path: {selectedPath}");

                            if (File.Exists(selectedPath + "\\" + SharedConstants.SyslogAgentExeFilename))
                            {
                                Globals.ExeFilePath = selectedPath + "\\";
                                Logger.LogInfo($"Valid exe path found: {Globals.ExeFilePath}");
                                return selectedPath + "\\";
                            }

                            Logger.LogWarning("syslogagent.exe not found in selected directory");
                            System.Windows.Forms.MessageBox.Show("syslogagent.exe not found in that directory");
                        }
                        else if (dialogResult == System.Windows.Forms.DialogResult.Cancel)
                        {
                            Logger.LogInfo("User cancelled directory selection");
                            return null;
                        }
                    }
                }
            }
        }

        private void WriteCertFile(string sourceFilename, bool isSecondary)
        {
            using (Logger.LogScope("WriteCertFile"))
            {
                Logger.LogInfo($"Writing {(isSecondary ? "secondary" : "primary")} cert file");
                Logger.LogInfo($"Source filename: {sourceFilename}");

                string certfileDirectory = GetCertFileDirectory();
                if (certfileDirectory == null)
                {
                    Logger.LogWarning("Cert file directory selection cancelled");
                    System.Windows.MessageBox.Show("Cancelled");
                    return;
                }

                string certfilePath = certfileDirectory + (isSecondary ?
                    SharedConstants.SecondaryCertFilename :
                    SharedConstants.PrimaryCertFilename);

                if (sourceFilename == certfilePath)
                {
                    Logger.LogInfo("Source file is same as destination - no changes needed");
                    System.Windows.MessageBox.Show("That is the existing cert file. No changes made.");
                    return;
                }

                try
                {
                    File.Copy(sourceFilename, certfilePath, true);
                    Logger.LogInfo($"Successfully copied cert file to: {certfilePath}");
                    System.Windows.MessageBox.Show(
                        $"{(isSecondary ? "Secondary" : "Primary")} cert loaded from {sourceFilename}, saved to {certfilePath}");
                }
                catch (Exception ex)
                {
                    Logger.LogError($"Failed to copy cert file", ex);
                    throw;
                }
            }
        }

        private void primaryHost_LostFocus(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("PrimaryHost_LostFocus"))
            {
                HandleHostLostFocus(sender, true);
            }
        }

        private void secondaryHost_LostFocus(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("SecondaryHost_LostFocus"))
            {
                HandleHostLostFocus(sender, false);
            }
        }

        private void HandleHostLostFocus(object sender, bool isPrimary)
        {
            var textBox = sender as System.Windows.Controls.TextBox;
            if (textBox == null) return;

            string currentText = textBox.Text;
            string hostType = isPrimary ? "Primary" : "Secondary";
            Logger.LogInfo($"{hostType} host text changed to: {currentText}");
            if (currentText.Trim().Length == 0)
            {
                return;
            }

            var selfSignedCertCheckBox = isPrimary ? primaryUseSelfSignedCertCheck : secondaryUseSelfSignedCertCheck;

            if (! currentText.StartsWith("http:") && ! currentText.StartsWith("https:"))
            {
                string prefix = selfSignedCertCheckBox.IsChecked == true ? "https://" : "http://";
                Logger.LogInfo($"Adding {prefix} prefix to {hostType} host");
                textBox.Text = prefix + currentText;
            }
        }

        private void primaryUseSelfSignedCertCheck_Checked(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("PrimarySelfSignedCertChecked"))
            {
                try
                {
                    if (primaryHostText.Text.StartsWith("http://"))
                    {
                        Logger.LogInfo("Converting HTTP to HTTPS for primary host");
                        primaryHostText.Text = "https://" + primaryHostText.Text.Substring(7);
                    }
                    else if (!primaryHostText.Text.StartsWith("https://"))
                    {
                        Logger.LogInfo("Adding HTTPS prefix to primary host");
                        primaryHostText.Text = "https://" + primaryHostText.Text;
                    }
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to process primary TLS check", ex);
                    throw;
                }
            }
        }

        private void secondaryUseSelfSignedCertCheck_Checked(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("SecondarySelfSignedCertChecked"))
            {
                try
                {
                    if (secondaryHostText.Text.StartsWith("http://"))
                    {
                        Logger.LogInfo("Converting HTTP to HTTPS for secondary host");
                        secondaryHostText.Text = "https://" + secondaryHostText.Text.Substring(7);
                    }
                    else if (!secondaryHostText.Text.StartsWith("https://"))
                    {
                        Logger.LogInfo("Adding HTTPS prefix to secondary host");
                        secondaryHostText.Text = "https://" + secondaryHostText.Text;
                    }
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to process secondary TLS check", ex);
                    throw;
                }
            }
        }


        private void TailFilename_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("TailFilename_Selection"))
            {
                using (OpenFileDialog openFileDialog = new OpenFileDialog())
                {
                    Logger.LogInfo("Opening file dialog for tail file selection");
                    DialogResult dialogResult = openFileDialog.ShowDialog();

                    if (dialogResult == System.Windows.Forms.DialogResult.OK)
                    {
                        Logger.LogInfo($"Selected tail file: {openFileDialog.FileName}");
                        txtTailFilename.Text = openFileDialog.FileName;
                        txtTailProgramName.IsEnabled = true;
                    }
                    else
                    {
                        Logger.LogInfo("Tail file selection cancelled");
                        System.Windows.MessageBox.Show("Cancelled");
                    }
                }
            }
        }

        private void txtTailFilename_LostFocus(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("TailFilename_LostFocus"))
            {
                try
                {
                    txtTailFilename.Text = txtTailFilename.Text.Trim();
                    Logger.LogInfo($"Tail filename after trim: {txtTailFilename.Text}");

                    if (txtTailFilename.Text == "")
                    {
                        Logger.LogInfo("Tail filename is empty, disabling program name");
                        txtTailProgramName.Text = "";
                        txtTailProgramName.IsEnabled = false;
                    }
                    else
                    {
                        Logger.LogInfo("Tail filename is not empty, enabling program name");
                        txtTailProgramName.IsEnabled = true;
                    }

                    SetTailProgramAvailable();
                }
                catch (Exception ex)
                {
                    Logger.LogError("Error processing tail filename focus change", ex);
                    throw;
                }
            }
        }

        private void ChooseCertFileButton_Click(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("CertFile_Selection"))
            {
                try
                {
                    System.Windows.Controls.Button buttonClicked = (System.Windows.Controls.Button)sender;
                    bool isSecondary = buttonClicked.Name == "chooseSecondaryCertFileButton";
                    Logger.LogInfo($"Selecting cert file for {(isSecondary ? "secondary" : "primary")} server");

                    using (OpenFileDialog openFileDialog = new OpenFileDialog())
                    {
                        openFileDialog.Filter = "PFX files (*.pfx)|*.pfx";
                        openFileDialog.FilterIndex = 1;

                        Logger.LogInfo("Opening file dialog for cert file selection");
                        DialogResult dialogResult = openFileDialog.ShowDialog();

                        if (dialogResult == System.Windows.Forms.DialogResult.OK)
                        {
                            Logger.LogInfo($"Selected cert file: {openFileDialog.FileName}");
                            WriteCertFile(openFileDialog.FileName, isSecondary);

                            if (isSecondary)
                            {
                                Globals.SecondaryTlsFilename = openFileDialog.FileName;
                                Logger.LogInfo($"Set secondary TLS filename: {openFileDialog.FileName}");
                            }
                            else
                            {
                                Globals.PrimaryTlsFilename = openFileDialog.FileName;
                                Logger.LogInfo($"Set primary TLS filename: {openFileDialog.FileName}");
                            }
                        }
                        else
                        {
                            Logger.LogInfo("Cert file selection cancelled");
                            System.Windows.MessageBox.Show("Cancelled");
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.LogError("Error during cert file selection", ex);
                    throw;
                }
            }
        }

        void SelectAllButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("SelectAll"))
            {
                try
                {
                    Logger.LogInfo("Setting all event log items to selected");
                    presenter.SetAllChosen(true);
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to select all items", ex);
                    throw;
                }
            }
        }

        void UnselectAllButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("UnselectAll"))
            {
                try
                {
                    Logger.LogInfo("Setting all event log items to unselected");
                    presenter.SetAllChosen(false);
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to unselect all items", ex);
                    throw;
                }
            }
        }

        void UIElement_OnPreviewMouseDown(object sender, MouseButtonEventArgs e)
        {
            using (Logger.LogScope("PreviewMouseDown"))
            {
                try
                {
                    Logger.LogInfo("Mouse down preview detected, notifying presenter");
                    presenter.PreviewInput();
                }
                catch (Exception ex)
                {
                    Logger.LogError("Error handling mouse down preview", ex);
                    throw;
                }
            }
        }

        void UIElement_OnPreviewKeyDown(object sender, System.Windows.Input.KeyEventArgs e)
        {
            using (Logger.LogScope("PreviewKeyDown"))
            {
                try
                {
                    Logger.LogInfo("Key down preview detected, notifying presenter");
                    presenter.PreviewInput();
                }
                catch (Exception ex)
                {
                    Logger.LogError("Error handling key down preview", ex);
                    throw;
                }
            }
        }

        void RestartButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("RestartOperation"))
            {
                try
                {
                    Logger.LogInfo("Initiating restart operation");
                    presenter.Restart();
                    Logger.LogInfo("Restart operation completed");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to restart", ex);
                    throw;
                }
            }
        }

        void StopButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("StopOperation"))
            {
                try
                {
                    Logger.LogInfo("Initiating stop operation");
                    presenter.Stop();
                    Logger.LogInfo("Stop operation initiated");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to stop", ex);
                    throw;
                }
            }
        }

        void StartButton_OnClick(object sender, RoutedEventArgs e)
        {
            using (Logger.LogScope("StartOperation"))
            {
                try
                {
                    Logger.LogInfo("Initiating start operation");
                    presenter.Start();
                    Logger.LogInfo("Start operation initiated");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to start", ex);
                    throw;
                }
            }
        }
    }
}

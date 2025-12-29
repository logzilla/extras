/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright 2021 LogZilla Corp.
*/

using Newtonsoft.Json;
using System;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Controls;
using System.Globalization;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Security.Policy;
using System.Windows.Forms;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;
using System.Runtime.InteropServices;
using System.Net.Http;
using System.Security.Cryptography.X509Certificates;
using System.Threading.Tasks;
using System.Net.Security;
using Newtonsoft.Json.Linq;

namespace SyslogAgent.Config
{

    public class MainPresenter
    {
        // Windows error codes:
        private const uint ERROR_SUCCESS = 0;
        private const uint ERROR_INVALID_ACCESS = 12;
        private const uint ERROR_INVALID_DATA = 13;
        private const uint ERROR_ACCESS_DENIED = 5;
        private const uint ERROR_PATH_NOT_FOUND = 3;
        private const uint ERROR_NO_DATA = 232;
        private const uint ERROR_INVALID_FUNCTION = 1;

        protected Registry registry_;
        protected Configuration config_;
        public MainPresenter(IMainView view, Registry registry, Configuration configuration,
            ServiceModel serviceModel)
        {
            this.view = view;
            this.registry_ = registry;
            this.config_ = configuration;
            registry_.ReadConfigFromRegistry(ref this.config_);
            this.serviceModel = serviceModel;
            this.eventLogTreeviewRoot = BuildTreeviewFromEventPaths(configuration.AllEventLogPaths);
            this.eventLogTreeviewRoot.SetIsCheckedAll(false);
            CheckEventPaths(this.eventLogTreeviewRoot, configuration.SelectedEventLogPaths);
        }

        public string GetLogzillaFileVersion()
        {
            try
            {
                string file_path = Globals.ExeFilePath + SharedConstants.SyslogAgentExeFilename;
                FileVersionInfo verInfo = FileVersionInfo.GetVersionInfo(Globals.ExeFilePath
                    + SharedConstants.SyslogAgentExeFilename);
                return verInfo.ProductVersion;
            }
            catch
            {
                return "not available";
            }
        }

        public void AddTreeviewItemPath(string leaf_path, EventLogTreeviewItem parent,
            IList<string> path_parts)
        {
            if (path_parts.Count < 1)
                return;
            List<string> remaining_parts = new List<string>(path_parts.Skip(1));
            EventLogTreeviewItem cur_node = null;
            foreach (var child in parent.Children)
            {
                if (child.Name == path_parts[0])
                {
                    cur_node = child;
                    break;
                }
            }
            if (cur_node == null)
            {
                cur_node = parent.AddChild(path_parts[0]);
            }
            if (remaining_parts.Count > 0)
            {
                AddTreeviewItemPath(leaf_path, cur_node, remaining_parts);
            }
            else
            {
                cur_node.LeafPath = leaf_path;
            }
        }

        public EventLogTreeviewItem BuildTreeviewFromEventPaths(IEnumerable<string> path_names)
        {
            var root = new EventLogTreeviewItem() { Name = "(root)" };
            foreach (var path in path_names)
            {
                var slash_parts = path.Split('/');
                var key_parts = new List<string>(slash_parts[0].Split('-'));
                if (slash_parts.Length > 1)
                {
                    key_parts.Add(slash_parts[1]);
                }
                AddTreeviewItemPath(path, root, key_parts);
            }
            return root;
        }

        public void CheckEventPath(EventLogTreeviewItem parent, IList<string> path_parts)
        {
            if (path_parts.Count < 1)
                return;
            List<string> remaining_parts = new List<string>(path_parts.Skip(1));
            foreach (var child in parent.Children)
            {
                if (child.Name == path_parts[0])
                {
                    if (remaining_parts.Count == 0)
                    {
                        child.IsChecked = true;
                    }
                    else
                    {
                        CheckEventPath(child, remaining_parts);
                    }
                    break;
                }
            }
        }

        public void CheckEventPaths(EventLogTreeviewItem root, IEnumerable<string> path_names)
        {
            foreach (var path in path_names)
            {
                var slash_parts = path.Split('/');
                var key_parts = new List<string>(slash_parts[0].Split('-'));
                if (slash_parts.Length > 1)
                {
                    key_parts.Add(slash_parts[1]);
                }
                CheckEventPath(root, key_parts);
            }
        }

        public void CheckAllEventPaths(EventLogTreeviewItem parent)
        {
            parent.IsChecked = true;
            foreach (var child in parent.Children)
            {
                CheckAllEventPaths(child);
            }
        }

        public void RecheckEventPaths(IEnumerable<string> path_names)
        {
            this.eventLogTreeviewRoot.SetIsCheckedAll(false);
            CheckEventPaths(this.eventLogTreeviewRoot, path_names);
        }

        public IEnumerable<string> GetSelectedLogPaths(EventLogTreeviewItem node)
        {
            if (node.Children == null || node.Children.Count == 0)
            {
                if (node.IsChecked == true)
                {
                    yield return node.LeafPath;
                }
            }
            else
            {
                foreach (var child in node.Children)
                {
                    foreach (var leaf in GetSelectedLogPaths(child))
                    {
                        yield return leaf;
                    }
                }
            }
        }

        public void Load()
        {
            registry_.ReadConfigFromRegistry(ref config_);
            view.IncludeEventIds.IsSelected = config_.IncludeVsIgnoreEventIds;
            view.IgnoreEventIds.IsSelected = !config_.IncludeVsIgnoreEventIds;
            view.OnlyWhileRunning.IsSelected = config_.OnlyWhileRunning;
            view.CatchUp.IsSelected = !config_.OnlyWhileRunning;
            view.EventIdFilter.Content = config_.EventIdFilter;
            view.Suffix.Content = config_.Suffix;
            view.Facility.Option = config_.Facility;
            // view.IncludeKeyValuePairs.IsSelected = config.IncludeKeyValuePairs;
            // view.PollInterval.Content = config.PollInterval.ToString();
            view.PrimaryHost.Content = config_.PrimaryHost;
            view.PrimaryApiKey.Content = config_.PrimaryApiKey;
            view.SendToSecondary.IsSelected = config_.SendToSecondary;
            view.SecondaryHost.Content = config_.SecondaryHost;
            view.SecondaryApiKey.Content = config_.SecondaryApiKey;
            view.Severity.Option = (config_.Severity + 1) % 9;
            view.Facility.Option = config_.Facility % 24;
            view.DebugLevel.Option = config_.DebugLevel % 9;
            view.PrimaryUseSelfSignedCert.IsSelected = config_.PrimaryUseSelfSignedCert;
            view.SecondaryUseSelfSignedCert.IsSelected = config_.SecondaryUseSelfSignedCert;
            view.DebugLevel.Option = config_.DebugLevel;
            view.DebugLogFilename.Content = config_.DebugLogFilename;
            view.TailFilename.Content = config_.TailFilename;
            view.TailProgramName.Content = config_.TailProgramName;
            view.PrimaryBackwardsCompatVer.Option
                = Array.IndexOf(SharedConstants.BackwardsCompatVersions,
                config_.PrimaryBackwardsCompatVer);
            view.SecondaryBackwardsCompatVer.Option
                = Array.IndexOf(SharedConstants.BackwardsCompatVersions,
                config_.SecondaryBackwardsCompatVer);
            view.MaxBatchSize.Content = config_.MaxBatchSize.ToString();
            view.MaxBatchAge.Content = config_.MaxBatchAge.ToString();
            view.LogzillaFileVersion = GetLogzillaFileVersion();

            //foreach (var log in config.EventLogs) view.Logs.Add(log.DisplayName, log.IsChosen);

            SetServiceStatus(serviceModel.Status);

        }

        public void SetAllChosen(bool isChosen)
        {
            CheckAllEventPaths(this.eventLogTreeviewRoot);
        }

        Configuration LoadConfigurationFromView()
        {
            var config = new Configuration();

            config.IncludeVsIgnoreEventIds = view.IncludeEventIds.IsSelected;
            config.OnlyWhileRunning = view.OnlyWhileRunning.IsSelected;
            config.EventIdFilter = view.EventIdFilter.Content;
            config.Suffix = view.Suffix.Content;
            config.Facility = view.Facility.Option;
            // config.PollInterval = Convert.ToInt32(view.PollInterval.Content);
            config.LookUpAccountIDs = view.LookUpAccount.IsSelected;
            //config.IncludeKeyValuePairs = view.IncludeKeyValuePairs.IsSelected;
            config.PrimaryHost = view.PrimaryHost.Content;
            config.PrimaryApiKey = view.PrimaryApiKey.Content;
            config.SecondaryHost = view.SecondaryHost.Content;
            config.SecondaryApiKey = view.SecondaryApiKey.Content;
            config.SendToSecondary = view.SendToSecondary.IsSelected;
            config.PrimaryUseSelfSignedCert = view.PrimaryUseSelfSignedCert.IsSelected;
            config.SecondaryUseSelfSignedCert = view.SecondaryUseSelfSignedCert.IsSelected;
            config.Severity = (view.Severity.Option + 8) % 9;
            config.DebugLevel = view.DebugLevel.Option;
            config.DebugLogFilename = view.DebugLogFilename.Content;
            config.TailFilename = view.TailFilename.Content;
            config.TailProgramName = view.TailProgramName.Content;
            config.PrimaryBackwardsCompatVer
                = SharedConstants.BackwardsCompatVersions[view.PrimaryBackwardsCompatVer.Option];
            config.SecondaryBackwardsCompatVer
                = SharedConstants.BackwardsCompatVersions[view.SecondaryBackwardsCompatVer.Option];
            config.MaxBatchSize = Convert.ToInt32(view.MaxBatchSize.Content);
            config.MaxBatchAge = Convert.ToInt32(view.MaxBatchAge.Content);

            var selected_logs = GetSelectedLogPaths(this.eventLogTreeviewRoot);
            config.SelectedEventLogPaths = selected_logs;

            return config;
        }

        public void Save()
        {
            using (Logger.LogScope("Save Operation"))
            {
                Logger.LogInfo("Starting save operation");

                try
                {
                    // Log validation start
                    Logger.LogInfo("Starting configuration validation");
                    if (!Validate())
                    {
                        Logger.LogWarning("Validation failed - save operation aborted");
                        return;
                    }
                    Logger.LogInfo("Validation successful");

                    // Log configuration loading
                    Logger.LogInfo("Loading configuration from view");
                    config_ = LoadConfigurationFromView();
                    Logger.LogInfo("Configuration loaded successfully");

                    // Log registry write
                    Logger.LogInfo("Writing configuration to registry");
                    registry_.WriteConfigToRegistry(config_);
                    Logger.LogInfo("Registry write completed");

                    view.SetSuccessMessage("Data saved successfully.");
                    Logger.LogInfo("Save operation completed successfully");
                }
                catch (Exception ex)
                {
                    Logger.LogError("Save operation failed", ex);
                    throw;
                }
            }
        }

        public void Import()
        {
            try
            {
                string import_file_name = view.ChooseImportFileButton();
                if ((import_file_name ?? "") == "")
                {
                    view.SetFailureMessage("Import aborted.");
                    return;
                }
                Registry.ReadRegistryImportFile(ref config_, import_file_name);
                view.UpdateDisplay(config_);
                view.SetSuccessMessage("Configuration imported");
            }
            catch (Exception ex)
            {
                view.SetFailureMessage("Configuration error: " + ex.Message);
            }
        }

        public void Export()
        {
            if (!Validate())
            {
                return;
            }
            string export_file_name = view.ChooseExportFileButton();
            if ((export_file_name ?? "") == "")
            {
                view.SetFailureMessage("Export Aborted");
                return;
            }
            Configuration config = LoadConfigurationFromView();
            Registry.CreateExportFileFromConfig(config, export_file_name);
            view.SetSuccessMessage("Export file created.");
        }

        public void PreviewInput()
        {
            view.Message = string.Empty;
        }

        public void Restart()
        {
            new Thread(() =>
            {
                serviceModel.Restart(SetServiceStatus);
            }).Start();
        }

        public void Start()
        {
            new Thread(() =>
            {
                serviceModel.Start(SetServiceStatus);
            }).Start();
        }

        public void Stop()
        {
            new Thread(() =>
            {
                serviceModel.Stop(SetServiceStatus);
            }).Start();
        }

        public void UpdateServiceStatus()
        {
            // This method will be called by the status check timer
            // Just update the UI with the current service status
            // We'll do this in a non-blocking way without creating threads
            // to minimize memory usage
            SetServiceStatus(serviceModel.Status);
        }

        void SetServiceStatus(string status)
        {
            view.Status = "Agent service is " + status;
        }

        static string GetLogzillaServerVersion(string logzilla_server_url, string apiKey)
        {
            return null;
        }

        static int CompareLogzillaServerVersion(string version_a, string version_b)
        {
            // If all parts are equal
            return 0;
        }

        bool Validate()
        {
            using (Logger.LogScope("Validation"))
            {
                try
                {
                    // Create validation functions with descriptive names
                    List<(int, string, Func<string>)> validationSteps =
                        new List<(int, string, Func<string>)> {
                        (1, "Primary Host", () =>
                            ValidateInternetHost(view.PrimaryHost, true, "Invalid primary host")),

                        (2, "Primary Host Connectivity", () =>
                            ValidateHostConnectivity(view.PrimaryHost, view.PrimaryUseSelfSignedCert,
                            true, "Primary host")),

                        (4, "Primary API Key", () =>
                            ValidateApiKeyInternal(view, true, view.PrimaryUseSelfSignedCert.IsSelected, view.PrimaryHost,
                            view.PrimaryApiKey, Path.Combine(Globals.ExeFilePath, SharedConstants.PrimaryCertFilename), "Invalid primary API key")),

                        (5, "Secondary Host", () =>
                            ValidateInternetHost(view.SecondaryHost, false, "Invalid secondary host")),

                        (6, "Secondary Host Connectivity", () =>
                            ValidateHostConnectivity(view.SecondaryHost, view.SecondaryUseSelfSignedCert,
                            false, "Secondary host")),

                        (8, "Secondary API Key", () =>
                        {
                            // Only validate secondary API key if sending to secondary is enabled
                            if (!view.SendToSecondary.IsSelected)
                            {
                                Logger.LogInfo("Skipping secondary API key validation as 'Send to Secondary' is not checked.");
                                view.SecondaryApiKey.IsValid = true; // Mark as valid since it's not being used/required
                                return null; // Pass validation
                            }
                            // Otherwise, perform the original validation based on whether host is specified
                            return ValidateApiKeyInternal(view, !string.IsNullOrEmpty(view.SecondaryHost.Content), view.SecondaryUseSelfSignedCert.IsSelected, view.SecondaryHost,
                                view.SecondaryApiKey, Path.Combine(Globals.ExeFilePath, SharedConstants.SecondaryCertFilename), "Invalid secondary API key");
                        }),

                        (9, "Event IDs", () =>
                            ValidateEventIds(view.EventIdFilter, "Invalid event ID filter")),

                        (10, "Include/Ignore Event IDs", () =>
                            ValidateIgnoreVsIncludeEventIds(view.EventIdFilter, view.IncludeEventIds, view.IgnoreEventIds, "Event ID filter mode must be exclusively Include or Ignore if IDs are specified")),

                        (11, "Event Logs", () =>
                            ValidateEventLogs(view, this.eventLogTreeviewRoot, "Must select at least one event log")),

                        (13, "Debug Log Filename", () =>
                            ValidateFilename(view.DebugLogFilename, "Invalid debug log filename")),

                        (14, "Tail Filename", () =>
                            ValidateFilename(view.TailFilename, "Invalid tail filename")),

                        (15, "Tail Program Name", () =>
                            ValidateTailProgramName(view.TailProgramName, view.TailFilename.Content,
                            "Set a short program name for the tail log messages")),

                        (16, "Extra JSON", () =>
                            ValidatedSuffix(view.Suffix, "Invalid Extra JSON"))
                    };

                    // Run each validation function unless it's skipped
                    foreach (var (index, name, validator) in validationSteps)
                    {
                        if (App.SkippedValidations.Contains(index))
                        {
                            Logger.LogInfo($"Skipping validation {index}: {name} due to command line argument -s{index}");
                            continue;
                        }

                        Logger.LogInfo($"Starting validation {index}: {name}");

                        try
                        {
                            string validationResult = validator();

                            if (validationResult != null)
                            {
                                Logger.LogWarning($"Validation {index} failed for {name}: {validationResult}");
                                view.SetFailureMessage(validationResult);
                                return false;
                            }

                            Logger.LogInfo($"Validation {index} passed: {name}");
                        }
                        catch (Exception ex)
                        {
                            Logger.LogError($"Exception during validation {index} of {name}", ex);
                            view.SetFailureMessage($"Error validating {name}: {ex.Message}");
                            return false;
                        }
                    }

                    Logger.LogInfo("All non-skipped validations passed successfully");
                    return true;
                }
                catch (Exception ex)
                {
                    Logger.LogError("Unexpected error during validation", ex);
                    view.SetFailureMessage($"Unexpected error during validation: {ex.Message}");
                    return false;
                }
            }
        }

        static string ValidateInterval(IValidatedStringView interval, string failureMsg)
        {
            int result;
            var isValid = int.TryParse(interval.Content, out result);
            isValid &= result > 0;
            interval.IsValid = isValid;
            return isValid ? null : failureMsg;
        }

        static string ValidateNumericRange(IValidatedStringView value, int min, int max, string failureMsg)
        {
            int result;
            var isValid = int.TryParse(value.Content, out result);
            isValid &= result >= min && result <= max;
            value.IsValid = isValid;
            return isValid ? null : failureMsg;
        }

        static string ValidateFilename(IValidatedStringView filename, string failureMsg)
        {
            using (Logger.LogScope("ValidateFilename"))
            {
                try
                {
                    var content = filename.Content.Trim();
                    Logger.LogInfo($"Validating filename: '{content}'");

                    // Empty filename is valid (used when debug level is None)
                    if (content.Length < 1)
                    {
                        filename.IsValid = true;
                        Logger.LogInfo("Empty filename is valid");
                        return null;
                    }

                    // For simple filenames without path, only check basic filename characters
                    if (!content.Contains("\\") && !content.Contains("/"))
                    {
                        var isValid = Regex.Match(content, @"^[\w\-. ]+$").Success;
                        filename.IsValid = isValid;
                        Logger.LogInfo($"Simple filename validation {(isValid ? "passed" : "failed")}: '{content}'");
                        return isValid ? null : failureMsg;
                    }

                    // For paths, validate the full path format
                    var isPathValid = Regex.Match(content, @"^[a-zA-Z]:\\[\\\w\-. ]+$").Success;
                    filename.IsValid = isPathValid;
                    Logger.LogInfo($"Full path validation {(isPathValid ? "passed" : "failed")}: '{content}'");
                    return isPathValid ? null : failureMsg;
                }
                catch (Exception ex)
                {
                    Logger.LogError("Filename validation failed", ex);
                    throw;
                }
            }
        }

        static string ValidateStringLength(IValidatedStringView value, int minLen, int maxLen, string failureMsg)
        {
            bool isValid = value.IsValid = !(value.Content.Length < minLen || value.Content.Length > maxLen);
            return isValid ? null : failureMsg;
        }

        public static string ValidateInternetHost(IValidatedStringView host, bool required, string failureMsg)
        {
            if (!required) return null;

            string host_address = host.Content.Trim();
            if (host_address == "")
            {
                if (required)
                {
                    host.IsValid = false;
                    return failureMsg;
                }
                else
                {
                    return null;
                }
            }

            // Regex to validate IP address with optional port
            var regex_valid_ip
                    = @"^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.)"
                    + @"{3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])(:\d{1,5})?$";

            // Regex to validate hostname with optional port
            var regex_valid_host = @"^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*"
                        + @"([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])(:\d{1,5})?$";

            // Remove protocol prefix if exists to validate the host or IP address
            if (host_address.StartsWith("http://") || host_address.StartsWith("https://"))
            {
                host_address = host_address.Substring(host_address.IndexOf("://") + 3);
            }

            // Check if the host address is valid
            bool isValid = host.IsValid = Regex.Match(host_address, regex_valid_ip).Success
                || Regex.Match(host_address, regex_valid_host).Success;

            // Return null if valid, otherwise return the failure message
            return isValid ? null : failureMsg;
        }

        static string ValidateHostConnectivity(IValidatedStringView host,
            IValidatedOptionView useTls, bool required, string failureMsg)
        {
            if (!required)
                return null;

            string url = host.Content;
            if (!url.Contains("://"))
            {
                url = "http://" + url; // Prepend with default scheme (http) if no
                                       // scheme is specified
            }

            string scheme;
            string hostpart;
            int port;
            string path;
            try
            {
                var uri = new Uri(url);

                scheme = uri.Scheme; // http or https
                hostpart = uri.Host; // Hostname
                port = uri.IsDefaultPort ? (scheme == "https" ? 443 : 80) : uri.Port;
                // Port (if specified and not the default for the scheme)
                path = uri.AbsolutePath; // Path (if specified)

            }
            catch (UriFormatException)
            {
                return failureMsg;
            }

            if (port == 0)
            {
                return failureMsg;
            }

            if (scheme == "http")
            {
                if (useTls.IsSelected)
                {
                    return failureMsg;
                }
            }

            string errMsg = Communications.TestTcpConnection(hostpart, port);
            return (errMsg == null ? null : $"{failureMsg} {errMsg}");
        }

        public string GetWindowsHostName() {
            using (Logger.LogScope("GetWindowsHostName"))
            {
                try
                {
                    return System.Environment.MachineName;
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to get Windows Host Name", ex);
                    return "UnknownHost";
                }
            }
        }

        public string GetIPAddress() {
            using (Logger.LogScope("GetIPAddress"))
            {
                try
                {
                    var host = System.Net.Dns.GetHostEntry(System.Net.Dns.GetHostName());
                    foreach (var ip in host.AddressList)
                    {
                        if (ip.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
                        {
                            return ip.ToString();
                        }
                    }
                    // Fallback if no IPv4 found
                    if (host.AddressList.Length > 0)
                    {
                        return host.AddressList[0].ToString();
                    }
                    return "127.0.0.1"; // Final fallback
                }
                catch (Exception ex)
                {
                    Logger.LogError("Failed to get IP Address", ex);
                    return "0.0.0.0";
                }
            }
        }

        

        public string ValidateApiKeyInternal(
            IMainView currentView,
            bool required,
            bool useTls,
            IValidatedStringView host,
            IValidatedStringView apiKey,
            string pfxPath,
            string genericFailureMsg)
        {
            using (Logger.LogScope("API Key Validation (POST Test Event)"))
            {
                if (!required && string.IsNullOrWhiteSpace(apiKey.Content))
                {
                    Logger.LogInfo("API Key validation not required and key is empty - skipping");
                    apiKey.IsValid = true;
                    return null;
                }
                 if (string.IsNullOrWhiteSpace(host?.Content)) {
                    Logger.LogWarning("Host is not configured, cannot validate API key.");
                    bool shouldError = required && !string.IsNullOrWhiteSpace(apiKey?.Content);
                    apiKey.IsValid = !shouldError;
                    return shouldError ? "Host must be configured to validate API key." : null;
                }


                Logger.LogInfo($"Starting API key validation for host: {host.Content}");
                Logger.LogInfo($"TLS Enabled: {useTls}, PFX Path: {pfxPath}");

                // 1. Validate API key format
                Logger.LogInfo("Validating API key format");
                string keyToValidate = apiKey.Content?.Trim() ?? string.Empty;
                 bool isFormatValid = !string.IsNullOrWhiteSpace(keyToValidate) &&
                    Regex.IsMatch(keyToValidate, @"^[a-zA-Z0-9-]{48,128}$");

                if (!isFormatValid)
                {
                    if (!string.IsNullOrWhiteSpace(keyToValidate)) {
                        Logger.LogWarning($"API key format validation failed for key: '{keyToValidate}'");
                        apiKey.IsValid = false;
                        return genericFailureMsg;
                    }
                    else if (required) {
                         Logger.LogWarning("API key is required but empty.");
                         apiKey.IsValid = false;
                         return "API key is required but missing.";
                    }
                     else {
                         Logger.LogInfo("API key not required and is empty.");
                         apiKey.IsValid = true;
                         return null;
                     }
                }
                 apiKey.IsValid = true;


                // 2. Normalize URL and append API path
                Logger.LogInfo("Normalizing URL");
                string baseUrl = host.Content.Trim();
                if (!baseUrl.Contains("://"))
                {
                    baseUrl = (useTls ? "https://" : "http://") + baseUrl;
                }
                if (baseUrl.EndsWith("/")) {
                    baseUrl = baseUrl.Substring(0, baseUrl.Length - 1);
                }
                string targetUrl = baseUrl + (SharedConstants.HttpApiPath ?? "/incoming");
                Logger.LogInfo($"Target URL for POST: {targetUrl}");

                // 3. Gather configuration data from the view
                Logger.LogInfo("Gathering configuration snapshot data from view");
                List<string> selectedChannels = new List<string>();
                string eventIdMode = "none";
                List<int> eventIds = new List<int>();
                Dictionary<string, object> extraKvPairs = new Dictionary<string, object>();
                object tailConfig = null;

                try
                {
                    selectedChannels = GetSelectedLogPaths(this.eventLogTreeviewRoot).ToList();

                    string eventIdString = currentView.EventIdFilter?.Content?.Trim() ?? "";
                    if (!string.IsNullOrEmpty(eventIdString)) {
                         if (currentView.IncludeEventIds?.IsSelected ?? false) {
                             eventIdMode = "include";
                         } else if (currentView.IgnoreEventIds?.IsSelected ?? false) {
                             eventIdMode = "ignore";
                         }
                         eventIds = eventIdString.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries)
                                                 .Select(idStr => int.TryParse(idStr.Trim(), out int id) ? id : -1)
                                                 .Where(id => id != -1)
                                                 .ToList();
                    }


                    string suffixContent = currentView.Suffix?.Content?.Trim() ?? "";
                    if (!string.IsNullOrEmpty(suffixContent))
                    {
                        try {
                            string jsonToParse = $"{{{suffixContent}}}";
                            extraKvPairs = JsonConvert.DeserializeObject<Dictionary<string, object>>(jsonToParse) ?? new Dictionary<string, object>();
                        } catch (JsonException jsonEx) {
                            Logger.LogWarning($"Failed to parse Extra JSON Suffix: {jsonEx.Message}. Skipping for validation message.");
                        }
                    }

                    bool tailEnabled = !string.IsNullOrWhiteSpace(currentView.TailFilename?.Content);
                    if (tailEnabled) {
                        tailConfig = new {
                            enabled = true,
                            file_path = currentView.TailFilename.Content.Trim(),
                            program_name = currentView.TailProgramName?.Content?.Trim() ?? "DefaultTailProgram"
                        };
                    }

                    string hostname = GetWindowsHostName();
                    string ipAddress = GetIPAddress();


                    // Prepare Unix timestamp for consistency with agent
                    var now = DateTime.UtcNow;
                    var unixTime = (long)(now - new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc)).TotalSeconds;
                    var microseconds = now.Millisecond * 1000; // Convert milliseconds to microseconds
                    var tsUnixFormat = $"{unixTime}.{microseconds:D6}";
                    
                    var payload = new
                    {
                        event_type = "agent_configuration_snapshot",
                        source_app = "SyslogAgentConfig",
                        windows_agent_version = GetLogzillaFileVersion(),
                        timestamp_utc = tsUnixFormat, // Using Unix timestamp format to match the agent
                        message = "Agent configuration saved and API key validated.",
                        hostname = hostname,
                        ip_address = ipAddress,
                        configuration = new {
                            event_log_channels = selectedChannels,
                            event_id_filter = new {
                                mode = eventIdMode,
                                ids = eventIds
                            },
                            extra_key_value_pairs = extraKvPairs,
                            tail_configuration = tailConfig
                        },
                         validation_target = new {
                             host = host.Content,
                             use_tls = useTls
                         }
                    };

                    string jsonPayload = JsonConvert.SerializeObject(payload, Formatting.Indented,
                                                                     new JsonSerializerSettings { NullValueHandling = NullValueHandling.Ignore });
                    
                    // Anonymous type properties are read-only, create a new JObject instead to modify the message
                    JObject payloadObj = JObject.FromObject(payload);
                    payloadObj["message"] = payloadObj["message"] + "\n" + jsonPayload;

                    // Get Unix epoch timestamp with 6 decimal places (microseconds) to match agent format
                    // var now = DateTime.UtcNow; // Duplicated code - removing
                    // var unixTime = (long)(now - new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc)).TotalSeconds; // Duplicated code - removing
                    // var microseconds = now.Millisecond * 1000; // Convert milliseconds to microseconds // Duplicated code - removing
                    // var tsUnixFormat = $"{unixTime}.{microseconds:D6}"; // Duplicated code - removing
                    
                    var event_message = new {
                        host = hostname,
                        program = "SyslogAgentConfig",
                        extra_fields = new {
                            _source_type = "WindowsAgent",
                            _source_tag = "windows_agent",
                            _log_type = "configuration_snapshot",
                            event_log = "SyslogAgentConfig",
                            // Use appropriate severity for informational message (5-Notice or 6-Informational)
                            severity = "5", 
                            // Use User facility (1)
                            facility = "1",
                            hostname = hostname,
                            program = "SyslogAgentConfig",
                            ts = tsUnixFormat,
                        },
                        use_tls = useTls
                    };

                    // Convert anonymous objects to JObjects for proper merging
                    JObject eventMessageJObj = JObject.FromObject(event_message);
                    
                    // Merge the payload into the event message
                    eventMessageJObj.Merge(payloadObj);
                    
                    // Wrap the merged event in the required "events" array structure
                    JObject finalPayload = new JObject();
                    JArray eventsArray = new JArray();
                    eventsArray.Add(eventMessageJObj);
                    finalPayload["events"] = eventsArray;
                    
                    // Use the final payload JObject for the request
                    string finalJsonPayload = finalPayload.ToString(Formatting.None); // Use None formatting for compactness
                    
                    using (HttpClientHandler handler = new HttpClientHandler())
                    {
                        X509Certificate2 clientCert = null;

                        if (useTls)
                        {
                            if (!string.IsNullOrWhiteSpace(pfxPath) && File.Exists(pfxPath))
                            {
                                try
                                {
                                    clientCert = new X509Certificate2(pfxPath, "", X509KeyStorageFlags.UserKeySet | X509KeyStorageFlags.EphemeralKeySet);
                                    handler.ClientCertificates.Add(clientCert);
                                    Logger.LogInfo($"Loaded client certificate from {pfxPath}");
                                }
                                catch (Exception certEx)
                                {
                                    Logger.LogError($"Failed to load client certificate from {pfxPath}: {certEx.Message}", certEx);
                                     apiKey.IsValid = false;
                                    return $"Failed to load required client certificate: {certEx.Message}";
                                }
                            } else if (!string.IsNullOrWhiteSpace(pfxPath)) {
                                 Logger.LogWarning($"PFX file specified but not found: {pfxPath}");
                                 apiKey.IsValid = false;
                                 return $"Client certificate file not found: {pfxPath}";
                             } else {
                                 Logger.LogInfo("TLS enabled but no specific client certificate path provided.");
                             }


                            handler.ServerCertificateCustomValidationCallback = (sender, cert, chain, sslPolicyErrors) => {
                                 Logger.LogInfo($"Server certificate validation callback: Subject={cert?.Subject}, Errors={sslPolicyErrors}");
                                 if (sslPolicyErrors == SslPolicyErrors.None) return true;

                                Logger.LogWarning($"Server certificate validation failed: {sslPolicyErrors}. Allowing connection.");
                                return true;
                            };
                         }

                         using (HttpClient client = new HttpClient(handler))
                         {
                             client.Timeout = TimeSpan.FromSeconds(30);

                             HttpRequestMessage request = new HttpRequestMessage(HttpMethod.Post, targetUrl);

                             request.Headers.Add("Authorization", "token " + keyToValidate);
                             request.Headers.UserAgent.ParseAdd($"SyslogAgentConfig/{GetLogzillaFileVersion() ?? "Unknown"}");


                             request.Content = new StringContent(finalJsonPayload, Encoding.UTF8, "application/json");

                             Logger.LogInfo($"Sending validation POST to {targetUrl} with payload size: {finalJsonPayload.Length} bytes");

                             HttpResponseMessage response = client.SendAsync(request).GetAwaiter().GetResult();

                             Logger.LogInfo($"Received response status code: {response.StatusCode} ({(int)response.StatusCode})");

                             string responseBody = response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
                             if (!string.IsNullOrWhiteSpace(responseBody)) {
                                 Logger.LogInfo($"Response body: {responseBody}");
                             }


                             if (response.IsSuccessStatusCode)
                             {
                                 Logger.LogInfo("API key validation successful (POST test event successful)");
                                 apiKey.IsValid = true;
                                 return null;
                             }
                             else
                             {
                                 Logger.LogWarning($"API key validation failed. Status: {response.StatusCode}.");
                                 apiKey.IsValid = false;

                                switch (response.StatusCode)
                                {
                                    case System.Net.HttpStatusCode.Unauthorized:
                                        return "API key validation failed: Unauthorized (Check API Key)";
                                    case System.Net.HttpStatusCode.Forbidden:
                                        return "API key validation failed: Forbidden (Key may lack permission)";
                                    case System.Net.HttpStatusCode.NotFound:
                                         return $"API key validation failed: Endpoint not found ({targetUrl})";
                                    case System.Net.HttpStatusCode.BadRequest:
                                         return $"API key validation failed: Bad Request. Server Response: {responseBody}";
                                    default:
                                        return $"API key validation failed: {response.StatusCode}. Server Response: {responseBody}";
                                }
                             }
                         }
                     }
                }
                catch (HttpRequestException httpEx)
                {
                    Logger.LogError($"HTTP request failed during API key validation: {httpEx.Message}", httpEx);
                     string innerError = httpEx.InnerException?.Message ?? "No inner exception details";
                     apiKey.IsValid = false;
                     return $"Network error during validation: {innerError}";
                }
                catch (TaskCanceledException timeoutEx) {
                     Logger.LogError($"API key validation timed out: {timeoutEx.Message}", timeoutEx);
                     apiKey.IsValid = false;
                     return "API key validation timed out.";
                 }
                catch (Exception ex) when (ex is JsonException || ex is ArgumentException || ex is FormatException)
                 {
                     Logger.LogError($"Error preparing validation request data: {ex.Message}", ex);
                     apiKey.IsValid = false;
                     return $"Internal error preparing validation data: {ex.Message}";
                 }
                catch (Exception ex)
                {
                    Logger.LogError($"Unexpected error during API key validation: {ex.Message}", ex);
                    apiKey.IsValid = false;
                    return $"Unexpected error during validation: {ex.Message}";
                }
            }
        }

        static string ValidateIgnoreVsIncludeEventIds(IValidatedStringView eventIds,
            IValidatedOptionView includeEventIds, IValidatedOptionView ignoreEventIds, string failureMsg)
        {
            bool isValid = eventIds.Content.Trim().Length < 1
                || (includeEventIds.IsSelected ^ ignoreEventIds.IsSelected);
            return isValid ? null : failureMsg;
        }

        static string ValidateEventIds(IValidatedStringView eventIds, string failureMsg)
        {
            bool isValid = eventIds.IsValid = Regex.Match(eventIds.Content,
                @"^([0-9]{1,5},)*([0-9]{1,5})?$").Success;
            return isValid ? null : failureMsg;
        }

        static string ValidatedSuffix(IValidatedStringView suffix, string failureMsg)
        {
            suffix.IsValid = true;
            if (suffix.Content.Trim().Length > 0)
            {
                try
                {
                    dynamic deser = JsonConvert.DeserializeObject("{" + suffix.Content + "}");
                }
                catch (Exception ex)
                {
                    suffix.IsValid = false;
                    return "Invalid JSON body: " + ex.Message;
                }
            }
            suffix.IsValid = true;
            return null;
        }

        static string ValidatePrimaryTLS(IValidatedOptionView useTLS, string failureMsg)
        {
            bool isValid =
                (!useTLS.IsSelected)
                || (File.Exists(Globals.ExeFilePath + SharedConstants.PrimaryCertFilename));
            useTLS.IsValid = isValid;
            return isValid ? null : failureMsg;
        }

        static string ValidateSecondaryTLS(IValidatedOptionView useTLS, string failureMsg)
        {
            bool isValid =
                (!useTLS.IsSelected)
                || (File.Exists(Globals.ExeFilePath + SharedConstants.SecondaryCertFilename));
            useTLS.IsValid = isValid;
            return isValid ? null : failureMsg;
        }

        static string ValidateEventLogs(IMainView view, EventLogTreeviewItem root, string failureMsg)
        {
            // Get the selected paths from the root using a static helper method
            var selectedPaths = GetSelectedLogPathsStatic(root);
            bool isValid = selectedPaths.Any();
            return isValid ? null : failureMsg;
        }

        static string ValidateTailProgramName(IValidatedStringView tailProgram,
            string tailFilename, string failureMsg)
        {
            bool isValid = tailProgram.IsValid
                = (string.IsNullOrEmpty(tailFilename)
                ? true : tailProgram.Content.Trim() != string.Empty);
            return isValid ? null : failureMsg;
        }

        readonly IMainView view;
        readonly ServiceModel serviceModel;
        public readonly EventLogTreeviewItem eventLogTreeviewRoot;

        // Static version of the GetSelectedLogPaths method to use in static validation methods
        private static IEnumerable<string> GetSelectedLogPathsStatic(EventLogTreeviewItem node)
        {
            if (node.Children == null || node.Children.Count == 0)
            {
                if (node.IsChecked == true)
                {
                    yield return node.LeafPath;
                }
            }
            else
            {
                foreach (var child in node.Children)
                {
                    foreach (var leaf in GetSelectedLogPathsStatic(child))
                    {
                        yield return leaf;
                    }
                }
            }
        }

    }
}

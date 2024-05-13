/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

#define OTHER_NEW_METHOD
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Linq;
using System;

namespace SyslogAgent.Config
{
    public class Registry
    {

        public void ReadConfigFromRegistry(ref Configuration config)
                {
            openRegistryKey();
            config.PollInterval 
                = (int)mainKey.GetValue(SharedConstants.RegistryKey.EventLogPollInterval, 
                SharedConstants.ConfigDefaults.EventLogPollInterval);
            config.LookUpAccountIDs 
                = GetBinary(SharedConstants.RegistryKey.LookupAccounts, 
                SharedConstants.ConfigDefaults.LookupAccountsB) != 0;
            config.IncludeVsIgnoreEventIds 
                = GetBinary(SharedConstants.RegistryKey.IncludeVsIgnoreEventIds, 
                SharedConstants.ConfigDefaults.IncludeVsIgnoreEventIdsB) != 0;
            config.EventIdFilter 
                = mainKey.GetValue(SharedConstants.RegistryKey.EventIdFilter, 
                SharedConstants.ConfigDefaults.EventIdFilter).ToString();
            config.OnlyWhileRunning 
                = GetBinary( SharedConstants.RegistryKey.OnlyWhileRunning, 
                SharedConstants.ConfigDefaults.OnlyWhileRunning ) != 0;
            config.Facility 
                = (int)mainKey.GetValue(SharedConstants.RegistryKey.Facility, 
                (int)SharedConstants.ConfigDefaults.Facility);
            config.Severity 
                = (int)mainKey.GetValue(SharedConstants.RegistryKey.Severity, 
                (int)SharedConstants.ConfigDefaults.Severity);
            config.Suffix 
                = mainKey.GetValue(SharedConstants.RegistryKey.Suffix, 
                SharedConstants.ConfigDefaults.Suffix).ToString();
            config.SecondaryHost 
                = mainKey.GetValue(SharedConstants.RegistryKey.SecondaryHost, 
                SharedConstants.ConfigDefaults.SecondaryHost).ToString();
            config.PrimaryHost 
                = mainKey.GetValue(SharedConstants.RegistryKey.PrimaryHost, 
                SharedConstants.ConfigDefaults.PrimaryHost).ToString();
            config.PrimaryApiKey 
                = mainKey.GetValue(SharedConstants.RegistryKey.PrimaryApiKey, 
                SharedConstants.ConfigDefaults.PrimaryApiKey).ToString();
            config.SendToSecondary 
                = GetBinary(SharedConstants.RegistryKey.SendToSecondary, 
                SharedConstants.ConfigDefaults.SendToSecondaryB) != 0;
            config.SecondaryApiKey 
                = mainKey.GetValue(SharedConstants.RegistryKey.SecondaryApiKey, 
                SharedConstants.ConfigDefaults.SecondaryApiKey).ToString();
            config.PrimaryUseTls 
                = GetBinary(SharedConstants.RegistryKey.PrimaryUseTls, 
                SharedConstants.ConfigDefaults.PrimaryUseTlsB) != 0;
            config.SecondaryUseTls 
                = GetBinary(SharedConstants.RegistryKey.SecondaryUseTls, 
                SharedConstants.ConfigDefaults.SecondaryUseTlsB) != 0;
            config.DebugLevel 
                = 9 - (int)mainKey.GetValue(SharedConstants.RegistryKey.DebugLevelSetting, 0);
            config.DebugLogFilename 
                = mainKey.GetValue(SharedConstants.RegistryKey.DebugLogFile, "").ToString();
            config.TailFilename 
                = mainKey.GetValue(SharedConstants.RegistryKey.TailFilename, 
                string.Empty).ToString();
            config.TailProgramName 
                = mainKey.GetValue(SharedConstants.RegistryKey.TailProgramName, 
                string.Empty).ToString();
            config.AllEventLogPaths = Registry.AllEventLogPaths;
            config.SelectedEventLogPaths = Registry.SelectedEventLogPaths;
            config.BatchInterval 
                = (int)mainKey.GetValue( SharedConstants.RegistryKey.BatchInterval, 
                SharedConstants.ConfigDefaults.BatchInterval );
            config.PrimaryBackwardsCompatVer 
                = mainKey.GetValue(SharedConstants.RegistryKey.PrimaryBackwardsCompatVer, 
                SharedConstants.ConfigDefaults.BackwardsCompatVer).ToString();
            if (!SharedConstants.BackwardsCompatVersions.Contains(config.PrimaryBackwardsCompatVer))
            {
                config.PrimaryBackwardsCompatVer = SharedConstants.ConfigDefaults.BackwardsCompatVer;
            }
            config.PrimaryBackwardsCompatVer 
                = mainKey.GetValue( SharedConstants.RegistryKey.SecondaryBackwardsCompatVer, 
                SharedConstants.ConfigDefaults.BackwardsCompatVer).ToString();
            if (!SharedConstants.BackwardsCompatVersions.Contains(config.SecondaryBackwardsCompatVer))
            {
                config.SecondaryBackwardsCompatVer = SharedConstants.ConfigDefaults.BackwardsCompatVer;
            }
        }

        public void WriteConfigToRegistry(Configuration config) {
            openRegistryKey();

            mainKey.SetValue(SharedConstants.RegistryKey.ConfigVersion, 
                SharedConstants.CurrentConfigVersion);
            mainKey.SetValue(SharedConstants.RegistryKey.EventLogPollInterval, 
                config.PollInterval, RegistryValueKind.DWord);
            PutBool(SharedConstants.RegistryKey.LookupAccounts, config.LookUpAccountIDs);
            PutBool(SharedConstants.RegistryKey.IncludeVsIgnoreEventIds, 
                config.IncludeVsIgnoreEventIds);
            mainKey.SetValue(SharedConstants.RegistryKey.EventIdFilter, 
                config.EventIdFilter, RegistryValueKind.String);
            PutBool( SharedConstants.RegistryKey.OnlyWhileRunning, config.OnlyWhileRunning );
            mainKey.SetValue(SharedConstants.RegistryKey.Facility, 
                config.Facility, RegistryValueKind.DWord);
            mainKey.SetValue(SharedConstants.RegistryKey.Severity, 
                config.Severity, RegistryValueKind.DWord);
            mainKey.SetValue(SharedConstants.RegistryKey.BatchInterval, 
                config.BatchInterval, RegistryValueKind.DWord);
            mainKey.SetValue(SharedConstants.RegistryKey.Suffix, config.Suffix, 
                RegistryValueKind.String);
            mainKey.SetValue(SharedConstants.RegistryKey.PrimaryHost, 
                config.PrimaryHost, RegistryValueKind.String);
            mainKey.SetValue(SharedConstants.RegistryKey.PrimaryApiKey, 
                config.PrimaryApiKey, RegistryValueKind.String );
            mainKey.SetValue(SharedConstants.RegistryKey.SecondaryHost, 
                config.SecondaryHost, RegistryValueKind.String );
            mainKey.SetValue(SharedConstants.RegistryKey.SecondaryApiKey, 
                config.SecondaryApiKey, RegistryValueKind.String );
            PutBool( SharedConstants.RegistryKey.SendToSecondary, config.SendToSecondary);
            PutBool(SharedConstants.RegistryKey.PrimaryUseTls, config.PrimaryUseTls);
            PutBool(SharedConstants.RegistryKey.SecondaryUseTls, config.SecondaryUseTls);
            mainKey.SetValue(SharedConstants.RegistryKey.DebugLevelSetting, 
                9 - config.DebugLevel, RegistryValueKind.DWord);
            mainKey.SetValue(SharedConstants.RegistryKey.DebugLogFile, 
                config.DebugLogFilename, RegistryValueKind.String);
            mainKey.SetValue(SharedConstants.RegistryKey.TailFilename, 
                config.TailFilename, RegistryValueKind.String);
            mainKey.SetValue(SharedConstants.RegistryKey.TailProgramName, 
                config.TailProgramName, RegistryValueKind.String);
            mainKey.SetValue(SharedConstants.RegistryKey.PrimaryBackwardsCompatVer, 
                config.PrimaryBackwardsCompatVer, RegistryValueKind.String);
            mainKey.SetValue(SharedConstants.RegistryKey.SecondaryBackwardsCompatVer, 
                config.SecondaryBackwardsCompatVer, RegistryValueKind.String);

            saveSelectedEventChannelNames(config.SelectedEventLogPaths);
            disableMissingEventLogNames(config.SelectedEventLogPaths);

                foreach (var deprecated_key in deprecatedRegistryEntries)
                {
                    try
                    {  // if this doesn't work just go on, it won't affect operation
                        mainKey.DeleteValue(deprecated_key);
                    }
                    catch { }
                }

                removeOldSubkeys();
            }

        byte GetBinary(string key, byte defaultValue)
        {
            return GetBinary(mainKey, key, defaultValue);
        }

        static byte GetBinary(RegistryKey parent, string key, byte defaultValue)
        {
            var binaryDefault = new[] { defaultValue };
            return ((byte[])parent.GetValue(key, binaryDefault))[0];
        }

        void PutBinary(string key, byte value)
        {
            mainKey.SetValue(key, new[] { value }, RegistryValueKind.Binary);
        }

        void PutBool(string key, bool value)
        {
            PutBool(mainKey, key, value);
        }

        static void PutBool(RegistryKey parent, string key, bool value)
        {
            parent.SetValue(key, 
                new byte[] { value ? (byte)1 : (byte)0 }, RegistryValueKind.Binary);
        }

        RegistryKey mainKey = null;

        private void openRegistryKey()
        {
            if (mainKey == null)
            {
                mainKey = Microsoft.Win32.Registry.LocalMachine
                    .CreateSubKey(SharedConstants.RegistryKey.LogzillaRegistryKey, true);
            }
        }

        public readonly string[] deprecatedRegistryEntries =
        {
            "CarrigeReturnReplacementCharInASCII",
            "LineFeedReplacementCharInASCII",
            "TabReplacementCharInASCII",
            "SendToBackupPort",
            "SendToPort",
            "UseRFC3164",
            "TCPDelivery",
            "UsePingBeforeSend",
            "UseJsonMessage",
            "UseForwarder",
            "ForwarderTcpListenPort",
            "ForwarderUdpListenPort"
        };


        public static string[] getSavedEventChannelNames()
        {
            RegistryKey saved_event_channels_key 
                = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(
                    SharedConstants.RegistryKey.SelectedEventChannelsKey, false);
            if (saved_event_channels_key == null)
            {
                // key does not exist, return empty set
                return new string[0];
            }
            string[] result = saved_event_channels_key.GetSubKeyNames();
            saved_event_channels_key.Close();
            return result;
        }

        public static List<string> getSelectedEventChannelNames()
        {
            var result = new List<string>();
            var saved_names = getSavedEventChannelNames();
            foreach (var name in saved_names)
            {
                var channel_key = Microsoft.Win32.Registry.LocalMachine
                    .OpenSubKey(SharedConstants.RegistryKey.SelectedEventChannelsKey 
                    + @"\" + name, false);
                if (channel_key == null)
                    continue;
                var value = channel_key.GetValue(SharedConstants.RegistryKey.ChannelEnabledName);
                channel_key.Close();
                if (((int?)value ?? 0) == 1)
                    result.Add(name);
            }
            return result;
        }

        public void saveSelectedEventChannelNames(IEnumerable<string> channel_names)
        {
            var channels_key = Microsoft.Win32.Registry.LocalMachine
                .CreateSubKey(SharedConstants.RegistryKey.SelectedEventChannelsKey, true);
            foreach (var name in channel_names)
            {
                var log_key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(
                    SharedConstants.RegistryKey.SelectedEventChannelsKey + @"\" + name, true);
                log_key.SetValue(SharedConstants.RegistryKey.ChannelEnabledName, 1);
                log_key.Close();
            }
            channels_key.Close();
        }

        public void disableMissingEventLogNames(IEnumerable<string> selected_event_log_names)
        {
            var saved_channels = getSavedEventChannelNames();
            var missing = saved_channels.Where(x => !selected_event_log_names.Contains(x));
            foreach (var path in missing)
            {
                var key = Microsoft.Win32.Registry.LocalMachine
                    .OpenSubKey(SharedConstants.RegistryKey.SelectedEventChannelsKey
                    + @"\" + path, true);
                if (key != null)
                    key.SetValue(SharedConstants.RegistryKey.ChannelEnabledName, 0);
            }
        }

        public static List<string> excludeDisabledEventLogPaths(IEnumerable<string> event_log_paths)
        {
            var result = new List<string>();
            foreach (var path in event_log_paths)
            {
                if (WindowsEventLog.IsChannelEnabled(path))
                    result.Add(path);
            }
            return result;
        }

        public void removeOldSubkeys()
        {
            try
            { // if there's any error, this status isn't really a problem so just let it be
                RegistryKey sub_key = Microsoft.Win32.Registry.LocalMachine
                    .OpenSubKey(SharedConstants.RegistryKey.LogzillaRegistryKey, true);
                if (sub_key == null)
                {
                    // key does not exist, just return
                    return;
                }
                string[] subkey_names = sub_key.GetSubKeyNames();
                foreach (var key in subkey_names)
                {
                    if (key == "Channels") // this is the subkey we want
                        continue;
                    sub_key.DeleteSubKeyTree(key);
                }
                sub_key.Close();
            }
            catch { }
        }

        public static void WriteRegfileKeyValue(StreamWriter writer, string key, object value)
        {
            string value_str;
            switch (value.GetType().Name)
            {
                case "Boolean":
                    value_str = (bool)value ? "hex:01" : "hex:00";
                    break;

                case "Int32":
                    value_str = string.Format("dword:{0:X8}", (int)value);
                    break;

                case "String":
                    string a = ((string)value).Replace('\\', (char)13);
                    string b = a.Replace("\"", "\\\"");
                    string c = b.Replace(((char)33).ToString(), @"\\");
                    value_str = "\"" + c + "\"";
                    break;

                default:
                    throw new System.Exception("invalid value type");

            }
            writer.WriteLine("\"{0}\"={1}", key, value_str);
        }

        public static void CreateExportFileFromConfig(Configuration config, string file_name)
        {
            using (StreamWriter writer = new StreamWriter(file_name))
            {
                writer.WriteLine("Windows Registry Editor Version 5.00");
                writer.WriteLine("");
                writer.WriteLine(@"[HKEY_LOCAL_MACHINE\SOFTWARE\LogZilla]");
                writer.WriteLine("");
                writer.WriteLine(@"[HKEY_LOCAL_MACHINE\SOFTWARE\LogZilla\SyslogAgent]");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.ConfigVersion, SharedConstants.CurrentConfigVersion);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.EventLogPollInterval, 
                    SharedConstants.ConfigDefaults.EventLogPollInterval);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.LookupAccounts, config.LookUpAccountIDs);
                WriteRegfileKeyValue( writer, 
                    SharedConstants.RegistryKey.IncludeVsIgnoreEventIds, config.IncludeVsIgnoreEventIds );
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.EventIdFilter, config.EventIdFilter ?? "");
                WriteRegfileKeyValue( writer, 
                    SharedConstants.RegistryKey.OnlyWhileRunning, config.OnlyWhileRunning );
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.Facility, config.Facility);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.Severity, config.Severity);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.BatchInterval, config.BatchInterval);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.Suffix, config.Suffix ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.PrimaryHost, config.PrimaryHost ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.SecondaryHost, config.SecondaryHost ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.SendToSecondary, config.SendToSecondary);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.PrimaryUseTls, config.PrimaryUseTls);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.SecondaryUseTls, config.SecondaryUseTls);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.DebugLevelSetting, config.DebugLevel);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.DebugLogFile, config.DebugLogFilename ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.TailFilename, config.TailFilename ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.TailProgramName, config.TailProgramName ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.PrimaryTlsFileName, Globals.PrimaryTlsFilename ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.SecondaryTlsFileName, Globals.SecondaryTlsFilename ?? "");
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.PrimaryBackwardsCompatVer, config.PrimaryBackwardsCompatVer);
                WriteRegfileKeyValue(writer, 
                    SharedConstants.RegistryKey.SecondaryBackwardsCompatVer, config.SecondaryBackwardsCompatVer);

                writer.WriteLine("");
                writer.WriteLine(@"[HKEY_LOCAL_MACHINE\SOFTWARE\LogZilla\SyslogAgent\Channels]");
                writer.WriteLine("");

                foreach (var channel_name in config.SelectedEventLogPaths)
                {
                    writer.WriteLine(@"[HKEY_LOCAL_MACHINE\{1}\{2}]", 
                        SharedConstants.RegistryKey.LogzillaRegistryKey, 
                        SharedConstants.RegistryKey.SelectedEventChannelsKey, channel_name);
                    WriteRegfileKeyValue(writer, "Enabled", (int)1);
                    writer.WriteLine("");
                }
            }
        }

        private static string DeQuote(string st)
        {
            return st.Substring(1, st.Length - 2);
        }

        private static string ValuePortion(string value_str)
        {
            string[] values = value_str.Split(':');
            if (values.Length > 1)
            {
                return values[1];
            }
            else
            {
                return values[0];
            }
        }

        public static void ReadRegistryImportFile(ref Configuration config, string file_name)
        {

            List<string> channel_list = new List<string>();
            using (StreamReader sr = new StreamReader(file_name))
            {
                string line;
                string section = null;
                while ((line = sr.ReadLine()) != null)
                {
                    {
                        var trimmed = line.Trim();
                        if (trimmed == "")
                        {
                            continue;
                        }
                        else if (trimmed[0] == '[' && trimmed.Last() == ']')
                        {
                            section = trimmed.Substring(1, trimmed.Length - 2);
                        }
                        else
                        {
                            string[] parts = trimmed.Split('=');
                            switch (DeQuote(parts[0]))
                            {
                                // for now we're not going to worry about config version matching
                                case SharedConstants.RegistryKey.ConfigVersion:
                                    break;

                                case SharedConstants.RegistryKey.EventLogPollInterval:
                                    config.PollInterval 
                                        = System.Convert.ToInt32(ValuePortion(parts[1]), 16);
                                    break;

                                case SharedConstants.RegistryKey.BatchInterval:
                                    config.BatchInterval 
                                        = System.Convert.ToInt32( ValuePortion( parts[1]), 16);
                                    break;

                                case SharedConstants.RegistryKey.LookupAccounts:
                                    config.LookUpAccountIDs = ValuePortion(parts[1]) == "01";
                                    break;

                                case SharedConstants.RegistryKey.IncludeVsIgnoreEventIds:
                                    config.IncludeVsIgnoreEventIds = ValuePortion( parts[1]) == "01";
                                    break;

                                case SharedConstants.RegistryKey.EventIdFilter:
                                    config.EventIdFilter = DeQuote(parts[1]);
                                    break;

                                case SharedConstants.RegistryKey.OnlyWhileRunning:
                                    config.OnlyWhileRunning = ValuePortion( parts[1] ) == "01";
                                    break;

                                case SharedConstants.RegistryKey.Facility:
                                    config.Facility 
                                        = System.Convert.ToInt32(DeQuote(ValuePortion(parts[1])), 16);
                                    break;

                                case SharedConstants.RegistryKey.Severity:
                                    config.Severity 
                                        = System.Convert.ToInt32(DeQuote(ValuePortion(parts[1])), 16);
                                    break;

                                case SharedConstants.RegistryKey.Suffix:
                                    config.Suffix = DeQuote(parts[1]).Replace("\\", "");
                                    break;

                                case SharedConstants.RegistryKey.PrimaryHost:
                                    config.PrimaryHost = DeQuote(parts[1]);
                                    break;

                                case SharedConstants.RegistryKey.SecondaryHost:
                                    config.SecondaryHost = DeQuote(parts[1]);
                                    break;

                                case SharedConstants.RegistryKey.SendToSecondary:
                                    config.SendToSecondary = ValuePortion(parts[1]) == "01"; ;
                                    break;

                                case SharedConstants.RegistryKey.PrimaryUseTls:
                                    config.PrimaryUseTls = ValuePortion(parts[1]) == "01";
                                    break;

                                case SharedConstants.RegistryKey.SecondaryUseTls:
                                    config.SecondaryUseTls = ValuePortion(parts[1]) == "01";
                                    break;

                                case SharedConstants.RegistryKey.DebugLevelSetting:
                                    config.DebugLevel = System.Convert.ToInt32(ValuePortion(parts[1]), 16);
                                    break;

                                case SharedConstants.RegistryKey.DebugLogFile:
                                    config.DebugLogFilename = DeQuote(parts[1]).Replace("\\\\", "\\");
                                    break;

                                case SharedConstants.RegistryKey.TailFilename:
                                    config.TailFilename 
                                        = DeQuote(parts[1]).Replace("\"", "").Replace("\\\\", "\\");
                                    break;

                                case SharedConstants.RegistryKey.TailProgramName:
                                    config.TailProgramName = DeQuote(parts[1]);
                                    break;

                                case SharedConstants.RegistryKey.PrimaryBackwardsCompatVer:
                                    config.PrimaryBackwardsCompatVer = DeQuote(parts[1]);
                                    break;

                                case SharedConstants.RegistryKey.SecondaryBackwardsCompatVer:
                                    config.SecondaryBackwardsCompatVer = DeQuote(parts[1]);
                                    break;

                                // we're not going to do anything with the tls
                                // filenames

                                case "Enabled":
                                    // we must be in a channel def
                                    // [HKEY_LOCAL_MACHINE\SOFTWARE\LogZilla\SyslogAgent
                                    // \Channels\Microsoft-Client-Licensing-Platform/Admin]
                                    string[] channel_parts = section.Split('\\');
                                    if (channel_parts.Length > 1 
                                        && channel_parts[channel_parts.Length - 2] == "Channels")
                                    {
                                        channel_list.Add(channel_parts[channel_parts.Length - 1]);
                                    }
                                    break;

                                default:
                                    break;
                            }

                        }

                    }
    }
                config.SelectedEventLogPaths = channel_list.AsEnumerable();
            }
        }

        public static List<string> AllEventLogPaths = null;
        public static List<string> SelectedEventLogPaths = null;

        public static void LoadEventLogChannels()
        {
            List<string> selectedLogPaths = new List<string>(getSelectedEventChannelNames());
            var event_log_paths_temp = WindowsEventLog.GetWindowsEventChannelNames();
            AllEventLogPaths = excludeDisabledEventLogPaths(event_log_paths_temp);
            AllEventLogPaths.Sort();
            SelectedEventLogPaths = selectedLogPaths;
        }
    }

}

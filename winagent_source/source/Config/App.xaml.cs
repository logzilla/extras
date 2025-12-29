/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Windows;
using System.Linq;
using System.Collections.Generic;

namespace SyslogAgent.Config
{
    public partial class App
    {
        // Static property to store skipped validations (1-16)
        public static HashSet<int> SkippedValidations { get; private set; } = new HashSet<int>();

        void App_OnStartup(object sender, StartupEventArgs e)
        {
            // Handle debug argument
            bool showDebugWindow = e.Args.Contains("-d");

            // Process skip validation arguments (s1-s16)
            for (int i = 1; i <= 16; i++)
            {
                if (e.Args.Contains($"-s{i}"))
                {
                    SkippedValidations.Add(i);
                }
            }

            /* skip validation arguments, can specify multiple:
                -s1:  Primary Host validation
                -s2:  Primary Host Connectivity validation
                -s3:  Primary TLS Certificate validation
                -s4:  Primary API Key validation
                -s5:  Secondary Host validation
                -s6:  Secondary Host Connectivity validation
                -s7:  Secondary TLS Certificate validation
                -s8:  Secondary API Key validation
                -s9:  Event ID Selection validation
                -s10: Event IDs validation
                -s11: Debug Log Filename validation
                -s12: Tail Filename validation
                -s13: JSON Suffix validation
                -s14: Primary TLS Configuration validation
                -s15: Secondary TLS Configuration validation
                -s16: Tail Program Name validation
            */

            // Initialize debug window but don't show it yet
            var debugWindow = DebugWindow.Instance;
            if (showDebugWindow)
            {
                debugWindow.Show();
            }

            DispatcherUnhandledException += ShowUnhandledException;
        }

        void ShowUnhandledException(object sender, System.Windows.Threading.DispatcherUnhandledExceptionEventArgs e)
        {
            var showException = e.Exception.InnerException ?? e.Exception;
            var result = MessageBox.Show("An unexpected exception has occured: "
                + showException.Message + Environment.NewLine + Environment.NewLine
                + "Continuing may result in undefined behavior" + Environment.NewLine
                + " Do you want to continue?", "Unexpected Exception", MessageBoxButton.YesNo);
            e.Handled = true;
            if (result == MessageBoxResult.No)
            {
                Shutdown();
            }
        }
    }
}


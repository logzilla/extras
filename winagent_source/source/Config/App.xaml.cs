/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Windows;

namespace SyslogAgent.Config {
    public partial class App {
        void App_OnStartup(object sender, StartupEventArgs e) {
            DispatcherUnhandledException += ShowUnhandledException;
        }

        void ShowUnhandledException(object sender, System.Windows.Threading.DispatcherUnhandledExceptionEventArgs e) {
            var showException = e.Exception.InnerException ?? e.Exception;
            var result = MessageBox.Show("An unexpected exception has occured: "
                + showException.Message + Environment.NewLine + Environment.NewLine
                + "Continuing may result in undefined behavior" + Environment.NewLine
                +" Do you want to continue?", "Unexpected Exception", MessageBoxButton.YesNo);
            e.Handled = true;
            if (result == MessageBoxResult.No) {
                Shutdown();
            }
        }
    }
}

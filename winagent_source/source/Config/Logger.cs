using SyslogAgent.Config;
using System;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using System.Windows;

namespace SyslogAgent.Config
{
    public static class Logger
    {
        private static bool _isEnabled = true;

        public static bool IsEnabled
        {
            get => _isEnabled;
            set => _isEnabled = value;
        }

        public static void Log(string message)
        {
            if (!_isEnabled) return;

            // Write to both Debug window and Visual Studio output
            System.Diagnostics.Debug.WriteLine(message);

            try
            {
                DebugWindow.Instance.Log(message);
            }
            catch
            {
                // Suppress any errors if debug window isn't available
            }
        }

        public static void LogError(string message, Exception ex = null)
        {
            string logMessage = message;
            if (ex != null)
            {
                logMessage += $"\nException: {ex.Message}\nStack Trace: {ex.StackTrace}";
            }
            Log($"ERROR: {logMessage}");
        }

        public static void LogWarning(string message)
        {
            Log($"WARNING: {message}");
        }

        public static void LogInfo(string message)
        {
            Log($"INFO: {message}");
        }

        public static void LogMethodEntry([CallerMemberName] string memberName = "")
        {
            Log($"Entering method: {memberName}");
        }

        public static void LogMethodExit([CallerMemberName] string memberName = "")
        {
            Log($"Exiting method: {memberName}");
        }

        public static IDisposable LogScope(string scopeName)
        {
            return new LoggerScope(scopeName);
        }

        private class LoggerScope : IDisposable
        {
            private readonly string _scopeName;
            private readonly DateTime _startTime;

            public LoggerScope(string scopeName)
            {
                _scopeName = scopeName;
                _startTime = DateTime.Now;
                Log($"=== Begin {_scopeName} ===");
            }

            public void Dispose()
            {
                var duration = DateTime.Now - _startTime;
                Log($"=== End {_scopeName} (Duration: {duration.TotalMilliseconds:F2}ms) ===");
            }
        }
    }
}






//void SaveButton_OnClick(object sender, RoutedEventArgs e)
//{
//    using (Logger.LogScope("SaveOperation"))  // This will automatically log the start and end times
//    {
//        Logger.Log("Save operation starting...");
//        Mouse.OverrideCursor = System.Windows.Input.Cursors.Wait;
//        saveButton.IsEnabled = false;

//        try
//        {
//            Logger.Log("About to call presenter.Save()");
//            presenter.Save();
//            Logger.Log("presenter.Save() completed successfully");
//        }
//        catch (Exception ex)
//        {
//            Logger.LogError("Save operation failed", ex);  // This will log both the message and the exception details
//            throw;
//        }
//        finally
//        {
//            Mouse.OverrideCursor = null;
//            saveButton.IsEnabled = true;
//            Logger.Log("Save operation cleanup completed");
//        }
//    }  // This will automatically log how long the entire operation took
//}
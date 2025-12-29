using System;
using System.Windows;
using System.Windows.Threading;
using System.ComponentModel;
using System.Text;
using System.Threading.Tasks;

namespace SyslogAgent.Config
{
    public partial class DebugWindow : Window
    {
        private static DebugWindow _instance;
        private static readonly object _lock = new object();
        private StringBuilder _pendingMessages = new StringBuilder();
        private DispatcherTimer _updateTimer;

        public static DebugWindow Instance
        {
            get
            {
                if (_instance == null)
                {
                    lock (_lock)
                    {
                        if (_instance == null)
                        {
                            var dispatcher = Application.Current?.Dispatcher;
                            if (dispatcher != null && !dispatcher.CheckAccess())
                            {
                                return dispatcher.Invoke(() => Instance);
                            }
                            _instance = new DebugWindow();
                        }
                    }
                }
                return _instance;
            }
        }

        private DebugWindow()
        {
            InitializeComponent();
            this.Closing += DebugWindow_Closing;

            // Setup timer for batched updates
            _updateTimer = new DispatcherTimer(DispatcherPriority.Send)
            {
                Interval = TimeSpan.FromMilliseconds(100)
            };
            _updateTimer.Tick += UpdateTimer_Tick;
            _updateTimer.Start();
        }

        private void UpdateTimer_Tick(object sender, EventArgs e)
        {
            FlushPendingMessages();
        }

        private void FlushPendingMessages()
        {
            if (_pendingMessages.Length > 0)
            {
                if (!Dispatcher.CheckAccess())
                {
                    try
                    {
                        Dispatcher.Invoke(new Action(FlushPendingMessages), DispatcherPriority.Send);
                    }
                    catch (Exception)
                    {
                        // If invoke fails, messages will be flushed on next timer tick
                    }
                    return;
                }

                LogTextBox.AppendText(_pendingMessages.ToString());
                LogTextBox.ScrollToEnd();
                _pendingMessages.Clear();
            }
        }

        public void Log(string message, bool forceFlush = true)
        {
            string timeStamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff");
            string formattedMessage = $"[{timeStamp}] {message}{Environment.NewLine}";

            lock (_pendingMessages)
            {
                _pendingMessages.Append(formattedMessage);
            }

            if (forceFlush)
            {
                try
                {
                    // Try to update immediately with high priority
                    Dispatcher.BeginInvoke(new Action(FlushPendingMessages), DispatcherPriority.Send);
                }
                catch
                {
                    // If BeginInvoke fails, message will be flushed on next timer tick
                }
            }

            // Also write to debug output (visible in Visual Studio's Debug window)
            System.Diagnostics.Debug.WriteLine(formattedMessage);
        }

        private void DebugWindow_Closing(object sender, CancelEventArgs e)
        {
            e.Cancel = true;
            this.Hide();
        }

        private void ClearButton_Click(object sender, RoutedEventArgs e)
        {
            LogTextBox.Clear();
        }

        private void CopyButton_Click(object sender, RoutedEventArgs e)
        {
            Clipboard.SetText(LogTextBox.Text);
        }
    }
}

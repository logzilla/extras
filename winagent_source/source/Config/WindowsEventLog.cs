/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace SyslogAgent.Config
{
    public class WindowsEventLog
    {
        /* we must use an external DLL to get the list of Windows event log channels,
         * since as of this version of .NET (4.72) these Windows event APIs are 
         * not supported */

        [DllImport("EventLogInterface.dll", CharSet = CharSet.Unicode)]
        static extern UInt32 GetChannelNames(
            [Out] byte[] output_buffer,
            int output_buffer_length,
            [Out] byte[] error_buffer,
            int error_buffer_length);

        [DllImport("EventLogInterface.dll", CharSet = CharSet.Unicode)]
        static extern UInt32 IsChannelDisabled(String channel_name);

        public static List<string> GetWindowsEventChannelNames()
        {
            var result = new List<string>();
            byte[] channels_buffer = new byte[100000];
            byte[] error_buffer = new byte[1000];
            UInt32 num_chars = GetChannelNames(channels_buffer, 100000, error_buffer, 1000);

            Encoding enc = Encoding.UTF8;
            int name_start_idx = 0;
            for (int i = 0; i < num_chars; ++i)
            {
                if (channels_buffer[i] == 0)
                {
                    var name = enc.GetString(channels_buffer, name_start_idx, 
                        i - name_start_idx);
                    result.Add(name);
                    name_start_idx = i + 1;
                }
            }
            return result;
        }

        public static bool IsChannelEnabled(string channel_name)
        {
            return (IsChannelDisabled(channel_name) == 0);
        }
    }
}

/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

/* Not going to refactor whole program to pass base state, just using global */


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SyslogAgent.Config
{
    static class Globals
    {
        private static string exe_file_path_;
        public static string ExeFilePath
        {
            get
            {
                if (exe_file_path_ == null)
                    exe_file_path_ = AppDomain.CurrentDomain.BaseDirectory;
                return exe_file_path_;
            }
            set
            {
                exe_file_path_ = value;
            }
        }

        public static EventLogGroupMember EventLogTop { get; set; }
        public static string PrimaryTlsFilename { get; set; }
        public static string SecondaryTlsFilename { get; set; }
    }
}

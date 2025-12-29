/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;

namespace EventGenerator
{
    public static class EventLogCreator
    {
        const string MESSAGE_FILENAME = "EventLogMessages.dll";
        public const string LOG_SOURCE_NAME = "EventGenerator";
        
        public static void CreateEventLog()
        {
            if (EventLog.SourceExists(LOG_SOURCE_NAME))
                return;

            EventSourceCreationData creation_data 
                = new EventSourceCreationData(LOG_SOURCE_NAME, "Application")
            {
                MessageResourceFile = MESSAGE_FILENAME,
                CategoryResourceFile = MESSAGE_FILENAME,
                ParameterResourceFile = MESSAGE_FILENAME,
                CategoryCount = 3,
                MachineName = "."
            };

            EventLog.CreateEventSource(creation_data);
        }
    }
}  

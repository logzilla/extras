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
    public class EventGenerator
    {
        // public const string EVENT_SOURCE = "SyslogAgent_EventGenerator";
        // public const string EVENT_MESSAGE = "Test generated event message";

        public const string EVENT_MESSAGE = @"
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.

Сейчас мы вовлечены в великую гражданскую войну, проверяя, сможет ли этот 
народ или любой другой народ, задуманный и преданный своему делу, долго 
существовать. Нас встречают на великом поле битвы той войны. Мы пришли, 
чтобы освятить часть этого поля, как место последнего упокоения тех, 
кто отдал здесь свои жизни, чтобы этот народ мог жить. Вполне уместно 
и правильно, что мы должны это сделать.
";


        public const EventLogEntryType EVENT_TYPE = EventLogEntryType.Information;
        public const Int32 EVENT_ID = 1000;
        public const Int16 EVENT_CATEGORY = 1;
        public byte[] event_data_;

        public EventGenerator()
        {
            event_data_ = new byte[250];
            for (byte b = 0; b < 250; ++b)
            {
                event_data_[b] = (byte) (b + 1);
            }
        }

        public void WriteFakeEvent(/* string msg_addendum */)
        {
            EventLog.WriteEntry(EventLogCreator.LOG_SOURCE_NAME, EVENT_MESSAGE 
                /* + msg_addendum */, EVENT_TYPE, EVENT_ID, EVENT_CATEGORY, event_data_);
        }
    }
}

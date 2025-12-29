/* SyslogAgentConfig: configuring a syslog agent for Windows
Copyright © 2021 LogZilla Corp.
*/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

using EventGenerator;

namespace EventGenerator
{
    class Program
    {
        static void Main(string[] args)
        {
            EventLogCreator.CreateEventLog();
            var gen = new EventGenerator();
            Console.WriteLine("Sending event...");
            gen.WriteFakeEvent();
        }
    }
}

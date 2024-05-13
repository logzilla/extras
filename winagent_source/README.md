# SyslogAgent - An Agent to Transmit Windows Events to LogZilla
Copyright © 2021 LogZilla Corp.

## Description

This is a Visual Studio 2022 solution for "syslogagent", which
is a Windows service that reads and watches Windows event logs
to send those events to a LogZilla server.  
[LogZilla](http://logzilla.net) is a network event orchestrator:
it watches information streams such as log files; it de-duplicates, 
filters, and parses the log event messages; and reacts to those 
events in real-time, from sending out email alerts, up to 
and including automatically remediating problem conditions.

Note that "syslogagent" and references to "syslog" are no longer
accurate, but the name is kept for historical reasons. Now
the Windows (syslog) Agent communicates to the LogZilla server
via `http`/`https`.

There 6 projects in this solution:
* Agent (source) - C++ for the Windows service core app
* Config (source) - C# app to configure the Agent via the registry
* EventGenerator (source) - C# app to generate test Events
* EventLogInterface - C++ library DLL to provide newest Windows event APIs for use by the Config app, since the Config app .NET version (.NET 4.72) doesn't provide the newest API methods.
* Setup (Wix setup) - A MS Setup project, using Wix extensions, for the package of: agent service app, config app, and documentation.
* Documents (docs) - User documentation for the Config app with descriptions of the options.

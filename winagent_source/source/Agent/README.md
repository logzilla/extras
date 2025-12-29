# Agent - C++ Project

Part of: SyslogAgent: a syslog agent for Windows
Copyright © 2021 LogZilla Corp.

## Description

This project is a C++ project that provides the core functionality of the
LogZilla Windows Syslog Agent service. This service reads and watches
Windows Event Logs and communicates these events to LogZilla server(s).

The project is built as a 64-bit Windows service. A separate setup
project is provided in this solution to provide automatic installation
of the service. The service `.exe` can also be run manually via the
command line for testing.

Note that although the project is named "SyslogAgent", it no longer
uses the syslog protocol. The name is retained for historical reasons.
Now the agent communicates with LogZilla servers using the LogZilla
`http`/`https` event ingestion API.

The service is configured via the Windows Registry. The configuration
settings are stored in the `HKEY_LOCAL_MACHINE\SOFTWARE\LogZilla\SyslogAgent`.
Configuration of these settings is accomplished via the *Config* app in
this same solution.

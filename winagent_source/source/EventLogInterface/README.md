# EventLogInterface - C++ Project

Part of: SyslogAgent: a syslog agent for Windows
Copyright © 2021 LogZilla Corp.

## Description

This project produces a C++ DLL that provides Windows event log
API calls that are not provided by the version of .NET used in
this solution (.NET 4.72). The DLL is used by the SyslogAgent
Configuration Utility to read and display Windows event logs.

The functions provided by the DLL are:
* GetChannelNames
* IsChannelDisabled

The DLL is built as a 32-bit DLL, and is placed in the same
directory as the SyslogAgent Configuration Utility executable.

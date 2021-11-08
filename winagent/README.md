# Introduction

LogZilla NEO Windows Eventlog Agent

The NEO Windows Syslog Agent is a Windows service that sends Windows event log messages to a LogZilla server.  For a Windows environment it takes the place of a unix syslog service.

[Download](LogZilla_SyslogAgent_2.1.0.0.msi) Here

# History

Parts of this Syslog Agent are based the Datagram Syslog Agent, which in turn was based on SaberNet's NTSyslog. 

The LogZilla development team has added specific support to the agent for LogZilla via TCP and JSON, as well as fixing bugs and many other features and enhancements.

# Prerequisites

The Syslog Agent Configuration, `SyslogAgentConfig.exe`, requires .NET Framework 4.6.2 or later. The Syslog Agent service itself, `SyslogAgent.exe`, has no prerequisites.

# Install and configuration

1. If installing over a previous version of the Syslog Agent it is recommended to shut down the Syslog Agent Windows service before proceeding, but then upon install it should take the place of the old agent automatically.
2. Run the MSI installer file downloaded from GitHub.
3. The MSI installer creates the path and subfolder and places the system files needed in that folder.
4. The folder and path containing the needed files will be located at `C:\Program Files\LogZilla\SyslogAgent\` .
5. Run the program from the newly created shortcut on the desktop and set the options as pictured below (changing the server address to yours) and then click the **save** and **restart** button at the bottom.
6. If you had not already done so, within the LogZilla web ui using the LogZilla app store you must install the MS Windows app.


##### Screenshot: Agent Configuration

![Screenshot](images/agent_config.png)

# Configuration

The operation of the Syslog Agent service is controlled by registry settings.  These can be maintained with the Syslog Agent configuration program, `SyslogAgentConfig.exe`. This program must be run as administrator.

### Permissions
Although the installer will automatically attempt to set the option, some windows systems may require you to Right-click and "Run as administrator" (depending on the security settings in place on the system/OS version being used).

You may also change the advanced settings of the executable to always "run as administrator" by selecting the `syslogagentconfig.exe` file, then right click and choose `advanced` and tick the box labeled `always run as administrator`

## Servers
The address and port for the primary Syslog server, and optionally for a secondary server can be entered.  The address can be either a host name or an IP address.

## Send to secondary
There is an option to send messages to a secondary Syslog server.  If selected, every message successfully sent to the primary server will also be sent to the secondary server.

## Event Logs
A list of all event logs on the local system is displayed.  Messages in the event logs that are checked will be sent to the server.

## Poll Interval
This is the number of seconds between each time the event logs are read to check for new messages to send.

## Ignore Event Ids
To reduce the volume of messages sent, it is possible to ignore certain event ids.  This is entered as a comma-separated list of event id numbers.

## Look up Account IDs
Looking up the domain and user name of the account that generated a message can be expensive, as it may involve a call to a domain server, if the account is not local.  To improve performance, this look up can be disabled and messages will be sent to the server without any account information.

## Include key-value pairs
To aid parsing on the syslog server, the message content is enhanced by appending the following key-value pairs:
    • "event_id": ”nnnn” contains the Windows event id
    • "_source_type": "WindowsAgent" identifies this program as the sender of the message
    • "S1": ”xxx”, "S2": ”xxx”, … contain the substitution strings, if any

## Facility
The selected facility is included in all messages sent.

## Severity
By selecting ‘Dynamic’, the severity for each message is determined from the Windows event log type.  Otherwise, the selected severity is included in all messages sent.

## Suffix
The suffix is an optional string that is appended to all messages sent.  This should be in the form of: "key1": "value1", "key2": "value2", etc.

## Debug Log Level
This configures the “level” of log messages produced by the Syslog Agent.  The “level” means the type or importance of a given message.  Any given log level will produce messages at that level and those levels that are more important.  For example if “RECOVERABLE” is chosen, the Syslog Agent will also produce log messages of levels “FATAL” and “CRITICAL”.  Logging is optional, so this can be left set to “None”.  

## Log File Name
This configures the path and name of the file to which log messages will be saved.   If a path and directory are specified that specific combination will be used for the log file, otherwise the log file will be saved in the directory with the SyslogAgent.exe file.

## File Watcher (tail)
The agent has the capability to “tail” a specified text file – this means that the
agent will continually read the end of the given text file and send each new line that
is appended to that text file as a separate message to the LogZilla server.  A program
name should be specified here to indicate the source of those log messages. 

## Further Help
The installation process will place a file [LogZillaSyslogAgentManual.pdf](LogZillaSyslogAgentManual.pdf) in the installation directory (`C:\Program Files\LogZilla\SyslogAgent\`).  That file is also available for direct download at the link above.

## Introduction
### LogZilla NEO Windows Eventlog Agent
The NEO Windows Syslog Agent is a Windows service that sends Windows event log 
messages to a LogZilla server. For a Windows environment, it takes the place of
a unix syslog service. (note: the name is no longer completely accurate; although
this agent performs the role that typically unix syslog does, the agent now uses
HTTP/HTTPS rather than syslog protocol.)

### Download
[Download](LogZilla_SyslogAgent_6.30.3.0.msi) Here

### Features
This program supports the following:

* Simple configuration and ease of use.
* Selection of specific event logs
* Configuration of primary and secondary LogZilla recipient servers
* Configuration of optional TLS transport for log messages
* Optional ignoring of specified Windows event IDs
* Optional "tail"-ing of specified file

## Prerequisites

### Syslog Agent Configuration Tool

The Syslog Agent Configuration, `SyslogAgentConfig.exe`, requires .NET Framework 4.7.2 or later.

### Syslog Agent Service

The Syslog Agent service itself, `SyslogAgent.exe`, has no prerequisites.

## Installation and Configuration

### Installation Steps

1. If installing over a previous version of the Syslog Agent it is recommended to
shut down the Syslog Agent Windows service before proceeding, but then upon install
it should take the place of the old agent automatically.
2. Run the MSI installer file downloaded from GitHub.
3. The MSI installer creates the path and subfolder and places the system files
needed in that folder.
4. The folder and path containing the needed files will be located at
`C:\Program Files\LogZilla\SyslogAgent\`.
5. Run the program from the newly created shortcut on the desktop and set the options
as pictured below (changing the server address to yours) and then click the **save**
and **restart** button at the bottom.
6. If you had not already done so, within the LogZilla web UI using the LogZilla
app store you must install the MS Windows app.

### Screenshot: Agent Configuration
![Screenshot](images/agent_config.png)

## Configuration

### Permissions

Although the installer will automatically attempt to set the option, some Windows
systems may require you to Right-click and "Run as administrator" (depending on the
security settings in place on the system/OS version being used).

You may also change the advanced settings of the executable to always "run as
administrator" by selecting the `syslogagentconfig.exe` file, then right-click and
choose `advanced` and tick the box labeled `always run as administrator`.

### Primary LogZilla Server

This is the HTTP/HTTPS address (optionally with port) for the primary LogZilla server.

### Primary / Secondary API Key

In order to send events to LogZilla, a LogZilla auth token / API key must be used. This
can be created using the logzilla command line tool as follows:

```
root[~]: # logzilla authtoken create
No user specified (missing -U option). I'll create key for admin
b2d8c210f54ed85511f1867cb6cc4faa8ae85bff42c3dd26
```

The last line shows the auth token.  This is what you would put in the API Key
text box here.

More information about this can be found at
[LogZilla Online Documentation](https://docs.logzilla.net/09_LogZilla_API/01_Using_The_LogZilla_API/)

### Secondary LogZilla Server

This is the HTTP/HTTPS address (optionally with port) for the secondary LogZilla
server to receive the events, if desired. 

### Primary/Secondary Use TLS

There is an option to use TLS to send messages to one or both LogZilla servers. If selected,
every message sent to the primary or secondary server will use a TLS communications link.

### Select Primary / Secondary Cert
These buttons are used to select (PFX format) certificate files for the TLS communications
to the primary or secondary server.  When the button is clicked a window will pop up
allowing selection of the file from which the cert is to be read.  Please note that once
the cert is read and imported (using the button) that certificate information is copied
into the LogZilla settings and the source cert file is no longer used.  The loaded
certificate files are named primary.pfx and secondary.pfx, in the LogZilla installation
directory (default c:\Program Files\Logzilla\SyslogAgent).
If you do not have a .pfx file, but instead have .key and .crt files, if you have access
to a unix machine with openssl installed, you can use the following command to produce a
.pfx file from the .key and .crt files:

```
root@agent-http # openssl pkcs12 -export -out cert.pfx -inkey cert.key\
 -in cert.crt
```

Note that the .pfx file must not use a password.

### LogZilla Compatible Version ###

Sometimes the Agent behavior must be configured to work with a particular revision level
of LogZilla server. Ordinarily this should be set at “detect”. LogZilla support will
indicate whether to force a particular compatibility version level, in which case these
drop-downs should be set as specified.

### Event Logs

A list of all event logs on the local system is displayed. Messages in the event logs that are
checked will be sent to the server.

### Look up Account IDs

Looking up the domain and user name of the account that generated a message can be
expensive, as it may involve a call to a domain server, if the account is not local.
To improve performance, this look-up can be disabled and messages will be sent to
the server without any account information.

### Ignore / Include Event IDs

To reduce the volume of messages sent, it is possible to ignore certain event IDs.
Alternatively, if only certain event IDs are of interest, those can be specified
here. Choose the appropriate button, then enter a comma-separated list of event ID
numbers.

### Catch-up / Only while running

The Windows Agent can either operate on every event received, regardless of
the Windows Agent being running; or only send events received while running.
The former case is called "catch-up" because if events occur while the Agent
is not running, the next time the Agent is started, it will send those missed
events.

### Facility

The selected facility is included in all messages sent.

### Severity

By selecting `Dynamic`, the severity for each message is determined from the Windows
event log type. Otherwise, the selected severity is included in all messages sent.

### Extra Key-Values

This configures whether any supplemental key-value pairs will be included with the
log messages, for processing by LogZilla rules. Key-value pairs should be separated
by commas. In addition to the manually specified key-values, LogZilla includes some
default key-value pairs for use in the LogZilla rules:

* "`_source_tag`" : "windows_agent" identifies this program as the sender of the message
* "`log_type`": "eventlog" OR "`log_type`": "file" ... indicates whether the log message originated in a Windows event log or originated from the "tail" operation
* "`event_id`": the Windows event ID
* "`event_log`": the name of the Windows event log from which the message originated

### Log Level

This configures the "level" of log messages produced by the Syslog Agent. The "level"
means the type or importance of a given message. Any given log level will produce
messages at that level and those levels that are more important. For example, if
"RECOVERABLE" is chosen, the Syslog Agent will also produce log messages of levels
"FATAL" and "CRITICAL". Logging is optional, so this can be left set to "None".

### Log File Name

This configures the path and name of the file to which log messages will be saved. If
a path and directory are specified that specific combination will be used for the log
file, otherwise, the log file will be saved in the directory with the SyslogAgent.exe file.

### Batch Interval

In order to reduce the frequency / speed of connections being opened between the Windows
Agent and LogZIlla, events can be “batched up” before sending. Then, instead of having a
new connection for each event, a connection is opened and many events are sent during that
connection, before it is closed.  For events to be batched up, there must be a duration of
time from the first event being received to the last event for that batch being received.

This value is in milliseconds, and the default is 1000. This means that when a new Windows
Event is received, if it’s the start of a new batch then there will be a one second delay
while subsequent events are collected for sending in that batch. So there may be a maximum
of 1 second (or whatever you specify here) from an event being received to the event being
sent to LogZilla, though of course for subsequent events in that batch the length of time
from the event generation in Windows to the event being sent by the Agent is correspondingly
less.

Set this to zero to have each event set immediately with no batching.

### File Watcher (tail)

The agent has the capability to "tail" a specified text file – this means that the agent
will continually read the end of the given text file and send each new line that is
appended to that text file as a separate message to the LogZilla server. A program name
should be specified here to indicate the source of those log messages.

### LogZilla Configuration

In order for LogZilla to make use of the Windows Syslog Agent, the LogZilla rule for the
agent must be installed. The preferred means of accomplishing this is by installing the MS
Windows app from the LogZilla app store, by going to `Settings` -> `App store` then choosing
Microsoft Windows and then choosing `Install`.

![Screenshot - Appstore add app](images/appstore_add_app.png)

After the app is installed, LogZilla will be set to properly handle event and file log
messages coming from the Windows Syslog Agent.

## Further Help

### Installation Manual

The installation process will place a file
[LogZillaSyslogAgentManual.pdf](LogZillaSyslogAgentManual.pdf) in the installation
directory (`C:\Program Files\LogZilla\SyslogAgent\`). That file is also available
for direct download at the link above.

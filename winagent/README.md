# Introduction 
(Version 2.0.0.0)

LogZilla NEO Windows Eventlog to Syslog


The NEO syslog agent is a Windows service that sends Windows event log messages to a syslog server.  Syslog is a widely used protocol of event notification and Syslog Agent allows Windows machines to be part of this environment.

[Download](logzilla_syslog_agent.2.0.0.0.msi) Here


# Features

This program supports the following:

- Simple configuration and ease of use.
- UDP and TCP transport on IPV4 and IPV6.
- Syslog protocols RFC3164 and RFC5424.

# History

Parts of this Syslog Agent are based the Datagram Syslog Agent, which in turn was based on SaberNet's NTSyslog. 

The original agents did not support RFC5424 or TCP. We've had our development team fix the bugs and add support for these protocols and standards as well as many other choices/customizations.

# Prerequisites

The Syslog Agent Configuration, `SyslogAgentConfig.exe`, requires .NET Framework 4.6.2 or later. The Syslog Agent service itself, `SyslogAgent.exe`, has no prerequisites.

# Install and configuration

1. If any previous versions exist - uninstall them and delete the folder that the files were previously housed in.
2. Run the MSI installer pkg downloaded from GitHub
3. The MSI installer creates the path and subfolder and places the system files needed in that folder.
4. The folder and path containing the needed files will be located at `C:\Program Files\LogZilla\SyslogAgent\`
5. Run the program from the newly created shortcut on the desktop and set the options as pictured below (changing the server address to yours) and then click the **save** and **restart** button at the bottom
6. Load the rule files for categorizing Microsoft events:

```
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/599-LZ-Winagent.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/600-Microsoft-ATP-Gateway.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/601-lz-mswin-program.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/602-Microsoft-Events.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/603-Microsoft-Event-Crits.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/604-Microsoft-Compliance.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/605-Microsoft-Categories.yaml
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/Microsoft/606-Microsoft-User-Tracking.yaml
sudo logzilla rules add 599-LZ-Winagent.yaml -f -R
sudo logzilla rules add 601-lz-mswin-program.yaml -f -R
sudo logzilla rules add 602-Microsoft-Events.yaml -f -R
sudo logzilla rules add 603-Microsoft-Event-Crits.yaml -f -R
sudo logzilla rules add 604-Microsoft-Compliance.yaml -f -R
sudo logzilla rules add 605-Microsoft-Categories.yaml -f -R
sudo logzilla rules add 606-Microsoft-User-Tracking.yaml -f
```


##### Screenshot: Agent Configuration

![Screenshot](images/agent_config.png)

# Configuration

The operation of the Syslog Agent service is controlled by registry settings.  These can be maintained with the Syslog Agent configuration program, `SyslogAgentConfig.exe`. This program must be run as administrator.

### Permissions
Although the installer will automatically attempt to set the option, some windows systems may require you to Right-click and "Run as administrator" (depending on the security settings in place on the system/OS version being used).

You may also change the advanced settings of the executable to always "run as administrator" by selecting the `syslogagentconfig.exe` file, then right click and choose `advanced` and tick the box labeled `always run as administrator`

# Protocols

Messages can be delivered to the LogZilla server with the `UDP` protocol or the `TCP` protocol.

If the UDP protocol is chosen, there is an option to ping the Syslog server before sending messages (UDP after ping).  Since UDP is a connectionless protocol, the ping can provide some assurance that the server is actually receiving messages.

# JSON

LogZilla can accept log messages in JSON format. This format is more efficient for LogZilla to process, leading to higher potential events-per-second rates.  If you are using LogZilla version x.xx or above this option is available to you, in which case you can check the appropriate box (per server), and make sure the server port listed above is correct for the port on which LogZilla is listening for JSON messages (which is different from the syslog port).

# Forwarder

The Syslog Agent can now be used to forward syslog traffic from other systems to the specified (LogZilla) destination.  To use this capability check the "Use Forwarder" box, then put in the port numbers on which to listen for incoming TCP and UDP traffic.  Traffic to those ports will be forwarded to the designated primary (and secondary) servers as specified above.

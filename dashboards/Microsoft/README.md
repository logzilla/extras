# LogZilla Dashboard For Microsoft Windows

This dashboard provides an overview for Windows-based Network Events. Widgets included:

* Top Windows Hosts
* Most Recent Windows Sources
* EPS: Windows Sources
* EPD: Windows Sources
* New Process Started
* User Logon Success
* File Share Accessed
* New Service Installed
* Network Connection Established
* File Audit
* Registry Audit
* Power Shell Command Line Execution
* Windows Firewall: Change Detection
* Scheduled Task Added
* Host File Shares Opened
* New Network Connections Per Hour

# Import/Export
Import
---
```
wget https://raw.githubusercontent.com/logzilla/extras/master/dashboards/Microsoft/dashboard-microsoft-windows.json

logzilla dashboards import -I dashboard-microsoft-windows.json

rm dashboard-microsoft-windows.json
```

Export
---
```
logzilla dashboards export -O mydashboards.json
```

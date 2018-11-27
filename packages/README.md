# About
"Packages" in LogZilla consist of multiple files which might include rules, triggers, dashboards, etc.

As a result, we've put them into a single `.tgz` file for easy download.

Some of the packages may have a shell script to save you the trouble of having to add each component manually.

## Cisco ISE Package

This package greatly enhances the data sent from Cisco's Identity Services Engine.

The original ISE events sent by Cisco have a `Step=nnn` where `nnn` is an integer value.

The reason Cisco does this is that:

1. It saves space in their local database to help their system scale better.
2. It lets them send more information to log receivers with fewer bytes.

Unfortunately, that also means that the end users are forced to look up Cisco's documentation on what each "Step" value means.

Instead, we've written a (very long) rule in syslog-ng that will rewrite each of these integer values into their original text.

All step values were obtained from [Cisco's documentation](https://www.cisco.com/c/dam/en/us/td/docs/security/ise/2-0/message_catalog/Cisco_Identity_Services_Engine_Log_Messages_20.xlsx).

There are over 2000 translations included so please pay attention to your server's health and make sure it can handle the load.

There may not be any impact, but these rules have not been tested under heavy load conditions.

# Installation

On the docker host, `sudo su -` to root

Then:

```
cd /root
mkdir -p lz-files
cd lz-files
wget "https://github.com/logzilla/extras/raw/master/packages/cisco-ise.tgz"
tar xzvf cisco-ise.tgz
cd cisco-ise
bash ./add2neo
```

# Cisco ISE Logs

Be sure to set up logging to your NEO server from within Cisco ISE.

####Screenshot: ISE Categories####

![ISE Screenshot](images/cisco_ise_categories.jpg)


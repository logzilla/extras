# Bro/Zeek Rules

# Work in progress, please do not use yet


From your Bro/Zeek server, set file format to TSV


Edit:
```
vi /usr/local/zeek/share/zeek/site/local.zeek
```

Comment out:
```
# Output in JSON format
#@load policy/tuning/json-logs.zeek
```

```
/usr/local/zeek/bin/zeekctl stop
/usr/local/zeek/bin/zeekctl deploy
```

[Zeek syslog-ng config](zeek2logzilla.conf)


You may get errors on RHEL/CentOS from syslog-ng about `Can't resolve to absolute path; path='/usr/local/zeek/logs/current', error='Permission denied (13)'`

I had to `setenforce permissive`. There's a right way to do it, but that's up to you :)

# Bro/Zeek Rules

Note - everything here works, but I need to document this better. For now, here are my notes:

# Rules

If you want to just use the rules already generates (which would probably work fine without having to do all the acripts below), then just run:

```
./addrules_to_logzilla.sh
```
and skip the section below for using the `makerules.sh` script.


# Bro/Zeek Server setup
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

### Get fields list from your zeek server:

```
cd /usr/local/zeek/logs/current/
grep '^#' *.log >./fields
```

# syslog-ng

RHEL/CentOS requires a more recent version of syslog-ng, the one in their repos is horribly old.

Also, you may get errors from `systemctl status syslog-ng` on RHEL/CentOS from about `Can't resolve to absolute path; path='/usr/local/zeek/logs/current', error='Permission denied (13)'`
I had to `setenforce permissive`. There's probably a better way to do it, but that's up to you :)

```
wget -O /etc/yum.repos.d/czanik-syslog-ng331-epel-7.repo 'https://copr.fedorainfracloud.org/coprs/czanik/syslog-ng331/repo/epel-7/czanik-syslog-ng331-epel-7.repo'
yum install -y syslog-ng
systemctl enable syslog-ng --now
yum -y erase rsyslog
```

## syslog-ng config

Use the syslog-ng config located [here](syslog-ng/zeek2logzilla.conf) on your Zeek server.

# makerules

After you have your `fields` list from your zeek server, run:

```
./makerules.sh fields
```

then run:

```
./addrules_to_logzilla.sh
```

# makedashboards

>CAUTION: If you import dashboards multiple times, it will create duplicate dashboards. Dashboards are not overwritten.
>So if there are 13 dashboards imported and you run the import again, you will have 26 dashboards.

This script won't create perfect dashboards, but it will get you 99% of the way there.

To generate dashboards, run:

```
./makedashboards.sh fields > dashboards.yaml
```

After running the script, you can import all dashboards using:

```
logzilla dashboards import -I dashboards.yaml
```

# Added bonus

The [401-zeek-portmap-src.yaml](401-zeek-portmap-dst.yaml) and [401-zeek-portmap-dst.yaml](401-zeek-portmap-dst.yaml) rules are included to map port numbers to human feiendly names. For example, port 22 shows as `ssh` in the UI.


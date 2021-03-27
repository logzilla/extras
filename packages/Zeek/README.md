# Bro/Zeek Rules

Note - everything here works, but I need to document this better. For now, here are my notes:

# Rules

If you want to just use the rules already generated (which would probably work fine without having to do all the scripts below), then just run:

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

### Get fields list from your zeek server

Option 1: Use the `fields.tsv` included here already

Option 2: 

On your Zeek server (not the logzilla server):

Be sure to `cd` to the logs directory first, otherwise, parsing in `makerules.sh` will fail.
```
cd /usr/local/zeek/logs/current/
grep '^#' *.log >./fields.tsv
```
Upload the field.tsv to your LogZilla server.

DO NOT copy/paste the fields.tsv file since we need the tab separated values for parsing.


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

# Dashboards

>CAUTION: If you import dashboards multiple times, it will create duplicate dashboards. Dashboards are not overwritten.
>So if there are 22 dashboards imported and you run the import again, you will have 26 dashboards.

To generate dashboards, use `-d` with `./makerules.sh` or just use the ones located in [dashboards](dashboards/):

Dashboards can be imported via command line using `logzilla dashboards import -I dashboards/filename.yaml`

or, to import all, run the following command:

```
for dash in dashboards/*.yaml
do
 logzilla dashboards import -I $dash
done
```

# Added bonus

Static rules are included in [static](static/) for various extras such as mapping port numbers to human friendly names. e.g.: port 22 shows as `ssh` in the UI instead of the number.

They will be added when running the `addrules_to_logzilla.sh` script.


# Use

1. Copy the custom syslog-ng rule to the syslog volume:

> Note, if you already have any custom configs, you may want to merge the two - or at least make sure you only use a single `log{}` statement

```
docker cp syslog-ng/eset-syslog.conf lz_syslog:/etc/logzilla/syslog-ng/

```

2. Import rules:

> As of LogZilla NEO v6.5, rules may be written in either YAML or JSON

```
logzilla rules add rules.d/500-eset-mgmnt-console.yaml
```

3. Import the dashboard:

```
logzilla dashboards import -I dashboards/eset-management-console.json
```

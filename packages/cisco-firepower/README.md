# Use

1. Copy the custom syslog-ng rule to the syslog volume:

> Note, if you already have any custom configs, you may want to merge the two - or at least make sure you only use a single `log{}` statement

```
cp syslog-ng/custom.conf /var/lib/docker/volumes/lz_config/_data/syslog-ng/

```

2. Import rules:

> As of LogZilla NEO v6.5, rules may be written in either YAML or JSON

```
logzilla rules add rules.d/500-cisco-firepower.yaml
logzilla rules add rules.d/998-lz-dst-ports.json
logzilla rules add rules.d/998-lz-src-ports.json
```

3. Import the dashboard:

```
logzilla dashboards import -I dashboards/cisco-firepower-dashboard.json
```

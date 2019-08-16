# Use

1. Import rules:

> As of LogZilla NEO v6.5, rules may be written in either YAML or JSON

```
logzilla rules add rules.d/100-meraki.yaml
logzilla rules add rules.d/998-lz-dst-ports.json
logzilla rules add rules.d/998-lz-src-ports.json
```

2. Import the dashboard:

```
logzilla dashboards import -I dashboards/meraki-dashboard.json
```

3. Refresh your browser in the LogZilla NEO UI


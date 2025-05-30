# Cisco ASA Rules

This package includes Dashboards and Rules for ASA Buildup/Teardown events

<font color="red">WARNING:</font> If your server is not properly sized, then you run the risk of causing problems. Please do not attempt to run these on a large network with something like a small/slow virtual machine.

You can test your server's capabilities by running `logzilla speedtest` or `logzilla rules performance`


# Integration

## Import rules

From this directory, paste the following:

```
for rule in ls rules.d/*.yaml
do
  [ -f "${rule}" ] || continue
  sudo logzilla rules add ${rule} -f -R
done
```

```
sudo logzilla rules reload
```

## Import the dashboards

From this directory, paste the following:

```
for dashboard in dashboards/*.yaml
do
    [ -f "${dashboard}" ] || continue
    sudo logzilla dashboards import -I ${dashboard}
done
```

Refresh your browser in the LogZilla NEO UI


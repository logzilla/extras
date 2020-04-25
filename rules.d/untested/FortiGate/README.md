# FortiGate Rules


Be sure to load the associated dashboard for these rules located in .../../../dashboards/FortiGate

[LINK](../../../dashboards/FortiGate)

# Or do this from your LogZilla Server:

```
sudo su -
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/untested/FortiGate/700-fortigate.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/untested/FortiGate/701-fortigate-normalize.yaml'
logzilla rules add 700-fortigate.yaml
logzilla rules add 701-fortigate-normalize.yaml
wget 'https://raw.githubusercontent.com/logzilla/extras/master/dashboards/FortiGate/dashboard-fortigate.yaml'
logzilla dashboards import -I dashboard-fortigate.yaml
```


# Sample Logs
To replay sample_logs.lzlog, you can use the following command:

Replace `-r 100` below with the rate to replay the events.
```
logzilla sender --zero-ts --read-full sample_logs.lzlog -w --syslog-target=localhost:514 --syslog-transport=udp --syslog-protocol=bsd -r 100 -v 5
```

##### Sample

![FortiGate Dashboard](../../../dashboards/FortiGate/fortigate-dashboard-sample.png)


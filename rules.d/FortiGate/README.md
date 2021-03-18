# FortiGate Rules


# Or do this from your LogZilla Server:

```
sudo su -
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/FortiGate/700-fortigate.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/FortiGate/701-fortigate-normalize.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/FortiGate/702-fortigate-portmap-dst.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/rules.d/FortiGate/702-fortigate-portmap-src.yaml'

for rule in 7*fortigate*.yaml
do
  logzilla rules add $rule -f -R
done
logzilla rules reload
```


# Sample Logs
To replay sample_logs.lzlog, you can use the following command:

Replace `-r 10` below with the rate to replay the events.
```
logzilla sender --zero-ts --read-full sample_logs.lzlog -w --syslog-target=localhost:514 --syslog-transport=udp --syslog-protocol=bsd -r 10 -v 5
```



# FortiGate Rules

The following rules are based on the [FortiGate / FortiOS 6.2.0 Cookbook](https://docs.fortinet.com/document/fortigate/6.2.0/cookbook/986892/sample-logs-by-log-type)


# Installation

```
sudo su -
wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/700-fortigate.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/701-fortigate-src-dst-ip.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/702-fortigate-normalize.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/703-fortigate-portmap-dst.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/703-fortigate-portmap-src.yaml'


for rule in 7*fortigate*.yaml
do
  logzilla rules add $rule -f -R
done
logzilla rules reload

### Customers running LogZilla v6.12 or lower must run the following commands:

```
logzilla config HIGH_CARDINALITY_TAGS "ut_Fortigate SrcIP, ut_Fortigate DstIP"
logzilla restart
```

wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/fortigate-event-dashboard.yaml'
wget 'https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/fortigate-traffic-dashboard.yaml'
wget https://raw.githubusercontent.com/logzilla/extras/master/packages/FortiGate/fortigate-utm-dashboard.yaml'

for dashboard in fort*dash*.yaml
do
  logzilla dashboards import -I $dashboard
done
```


# Sample Logs
To replay sample_logs.lzlog, you can use the following command:

Replace `-r 10` below with the rate to replay the events.
```
logzilla sender --zero-ts --read-full sample_logs.lzlog -w --syslog-target=localhost:514 --syslog-transport=udp --syslog-protocol=bsd -r 10 -v 5
```

# Updated Rules

The script in this directory may be used when FortiNet updates their documentation. No guarantees that it will work, but we have supplied it just in case.

## Usage

```
sudo su -
./makerules.sh > 700-fortigate.yaml
logzilla rules add 700-fortigate.yaml -f
```



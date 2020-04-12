# Sample Logs
To replay sample_logs.lzlog, you can use the following command:

Replace `-r 100` below with the rate to replay the events.
```
logzilla sender --zero-ts --read-full sample_logs.lzlog -w --syslog-target=localhost:514 --syslog-transport=udp --syslog-protocol=bsd -r 100 -v 5
```

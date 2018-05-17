# About

A collection of various scripts which we find useful in day to day operations or support.


## tcpcap.sh

This script is used to capture tcpdumps from a LogZilla server

It's mainly used for customer support, but we've added it here for general use by the community

Note that this script assumes an Ubuntu-based server.

It could easily be modified for Redhat/CentOS



```bash

#!/bin/bash

#---------------------------------------------------------------
# This script is used to capture tcpdumps from a LogZilla server
# It's mainly used for customer support, but we've added it here
#  for general use by the community
#
# Note that this script assumes an Ubuntu-based server.
# It could easily be modified for Redhat/CentOS
#---------------------------------------------------------------


__test_root() {
    if [ "$(id -u)" != "0" ]; then
        err "This script must be run as root"
        exit 1
    fi
}

__check_apparmor() {
    if [ `(dpkg-query -W -f='${Status}' apparmor-utils 2>/dev/null | grep -c "ok installed")` -eq 0 ]; then
        apt-get -qqy install apparmor-utils
        if [ $? -eq 0 ]; then
            aa-complain /usr/sbin/tcpdump
        else
            echo "Error setting up apparmor utils from 'apt-get install apparmor-utils'"
            exit 1
        fi
    fi
}

__test_root
__check_apparmor

PID=0
PID=$(pidof syslog-ng)
# Interface is set to the one using a default gateway - run with -i <interface> to specify a different one
int=$(awk '$2 == 00000000 { print $1 }' /proc/net/route)
echo -n "Please enter the port number that syslog-ng listens on: "
read port
echo -n "Please enter the amount of time (in seconds) to capture packets.\nE.g., 1 Day = 86400 : "
read secs

dir="/tmp"
h=`hostname`
ts=`date +%s`
fn="$dir/$h-$ts.pcap"
echo "Port set to $port, interface capture set to $int and time limit (in seconds) set to $secs"
echo "Running Command:"
echo "tcpdump -i $int port $port -nnvvXSs 0 -G $secs -W 1 -z gzip -w $fn"
nohup tcpdump -i $int port $port -nnvvXSs 0 -G $secs -W 1 -z gzip -w $fn > /tmp/tcpdump.log & 
echo ""
echo "CTRL-C to exit (it's also safe to disconnect from this session)"
echo "After $secs seconds, please email or upload ${fn}.gz to LogZilla Support"
```


## iopingtest
We use this on various servers to gauge the disk performance. It's not purely scientific, but a very simple way to gauge your disk IOPS without having to install a bunch of benchmark tools.

The script was made for Ubuntu/Debian-based servers but could be altered to work with CentOS/RHEL assuming they have the packages available.

```bash
#!/bin/bash
# RAW STATISTICS
#ioping -p 100 -c 200 -i 0 -q .
#100 26694 3746 15344272 188 267 1923 228
#100 24165 4138 16950134 190 242 2348 214
#(1) (2)   (3)  (4)      (5) (6) (7)  (8)
#
#(1) number of requests
#(2) serving time         (usec)
#(3) requests per second  (iops)
#(4) transfer speed       (bytes/sec)
#(5) minimal request time (usec)
#(6) average request time (usec)
#(7) maximum request time (usec)
#(8) request time standard deviation (usec)
install_prereq() {
    if [ `(dpkg-query -W -f='${Status}' $1 2>/dev/null | grep -c "ok installed")` -eq 0 ]; then
        DEBIAN_FRONTEND=noninteractive
        OUTPUT=$(apt-get install -qqy --no-install-recommends $1 2>&1 >/dev/null)
        [[ $OUTPUT ]] && echo "line $LINENO: $OUTPUT"
    fi
}
install_prereq ioping
install_prereq bc

verbose=0
[[ $1 = "-v" ]] && verbose=1
if [ -f /etc/os-release ]; then
    # special for ram disk testing:
    m='/mnt/rdisk'
    [[ -d '/mnt/rdisk' ]] && rdisk=${m}

    for m in $(cat /proc/mounts | egrep -v "fuse|boot" | grep "^/dev" | awk '{print $2}') $rdisk
        #for m in '/mnt/rdisk'
    do
        disk=$(cat /proc/mounts | grep " ${m} " | awk '{print $1}')
        if [ $verbose -eq 1 ]; then
            echo "------------DD: ${disk} [${m}]-----------------------------"
            #echo "cmd: dd if=/dev/zero of=${m}/test bs=1M count=1024 conv=fdatasync"
            dd if=/dev/zero of=${m}/test bs=1M count=1024 conv=fdatasync
            rm "${m}/test"
        fi
        echo "## Disk: ${disk}"
        echo "## Mount: ${m}"
        for b in 1k 4k 512k 1M
            # for b in 1M
        do
            if [ $verbose -eq 1 ]; then
                # Get IOPING stats in raw format:
                #echo "-------${disk} [${m}]-[${b} Blocks]------------"
                #echo "cmd: ioping -R -s ${b} ${m}"
                ioping -R -s ${b} ${m}
            else
                # Get IOPING stats in raw format:
                read req_count time_usec iops bps min_time avg_time max_time stddev_time <<< $(ioping -R -s ${b} ${m} -B)
                time=$(echo "scale=2;${time_usec}/10000000" | bc)
                min_time=$(echo "scale=2;${min_time}/1000000" | bc)
                avg_time=$(echo "scale=2;${avg_time}/1000000" | bc)
                max_time=$(echo "scale=2;${max_time}/1000000" | bc)
                stddev_time=$(echo "scale=2;${stddev_time}/1000000" | bc)
                mbps=$(echo ${bps} | numfmt --to=iec)
                iops=$(echo ${iops} | numfmt --to=si)
                echo "[${b}] ${mbps}B/s, ${iops} iops"
                #echo "${req_count} requests/${iops} IOPS (time:min/avg/max/stddev): ${time}s/${min_time}ms/${avg_time}ms/${max_time}ms/${stddev_time}ms"
            fi
        done
    done
else
    echo "Not a debian-based system"
fi
``` 
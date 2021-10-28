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
echo "Please enter the amount of time (in seconds) to capture packets."
echo -n "E.g., 1 Day = 86400 : "
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

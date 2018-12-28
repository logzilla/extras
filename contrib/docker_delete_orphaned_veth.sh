#!/bin/bash

veth_in_use=()
veth_unused=()
veth_all=()

function veth_interface_for_container() {
    local pid=$(docker inspect -f '{{.State.Pid}}' "${1}")
    mkdir -p /var/run/netns
    ln -sf /proc/$pid/ns/net "/var/run/netns/${1}"
    local index=$(ip netns exec "${1}" ip link show eth0 | head -n1 | sed s/:.*//)
    let index=index+1
    ip link show | grep "^${index}:" | sed "s/${index}: \(.*\):.*/\1/"
    rm -f "/var/run/netns/${1}"
}

for i in $(docker ps | grep Up | awk '{print $1}')
do
    if [ "$(veth_interface_for_container $i)" != "docker0" ]
    then
        veth_in_use+=($(veth_interface_for_container $i))
    fi
done

for i in $(brctl show | grep veth | awk '{print $(NF)}')
do
    veth_all+=($i)
done

for i in "${veth_all[@]}"
do
    for j in "${veth_in_use[@]}"
    do
        [[ $i == "$j" ]] && continue 2
    done

    ip link set $i down
    ip link delete $i
done

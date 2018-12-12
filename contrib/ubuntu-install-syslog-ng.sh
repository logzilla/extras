#!/bin/bash

# A POSIX variable
OPTIND=1

# Initialize our own variables:
verbose=0
LZ_3164=32514
LZ_5424=32601
LOCAL_3164=514
LOCAL_5424=601

usage="$(basename "$0") [-h] [-v] [-3 n] [-b n] [-5 n] [-s n] -- Sets up local server forwarding to NEO
where:
-h  show this help text
-v  Verbose mode
-3  set the port number that the LOCALHOST should listen on for RFC3164-style (standard BSD logs), default is 514
-b  set the port number that NEO is listening on for BSD-style (rfc3164), default is 32514
-5  set the port number that the LOCALHOST should listen on for RFC5424-style events, default is 601
-s  set the port number that NEO is listening on for RFC5424-style events, default is 32601
e.g.: $(basename "$0") -v -3 514 -b 32514 -5 601 -s 32601"

[[ "$#" -eq 0 ]] && { echo "$usage"; exit; }

while getopts ':hv5:3:b:s:' option; do
    case "$option" in
        h) echo "$usage"
            exit
            ;;
        5) LOCAL_5424=$OPTARG
            ;;
        3) LOCAL_3164=$OPTARG
            ;;
        b) LZ_3164=$OPTARG
            ;;
        s) LZ_5424=$OPTARG
            ;;
        v) verbose=1
            ;;
        :) printf "missing argument for -%s\n" "$OPTARG" >&2
            echo "$usage" >&2
            exit 1
            ;;
        \?) printf "illegal option: -%s\n" "$OPTARG" >&2
            echo "$usage" >&2
            exit 1
            ;;
    esac
done
shift $((OPTIND - 1))

config_neo_ports() {
    [[ $verbose -gt 0 ]] && echo "Checking NEO Ports"
    if [[ $(docker ps | grep -c "$LOCAL_3164->$LZ_3164/tcp, 0.0.0.0:$LOCAL_5424->$LZ_5424") -eq 0 ]]; then
        [[ $verbose -gt 0 ]] && echo "Checking NEO Version"
        v=$(logzilla version | awk -F. '{print $2}')
        if [[ $v -gt 1 ]]; then
            [[ $verbose -gt 0 ]] && echo "Setting NEO Ports to $LZ_3164 and $LZ_5424"
            logzilla config SYSLOG_PORT_MAPPING tcp/514:32514,udp/514:32514,tcp/601:32601
            [[ $verbose -gt 0 ]] && echo "Stopping container"
            docker stop lz_syslog
            [[ $verbose -gt 0 ]] && echo "Resetting container"
            docker rm lz_syslog
            [[ $verbose -gt 0 ]] && echo "Restarting NEO"
        else
            echo "This script only works on NEO version 6.1 or greater"
            exit
        fi
        [[ $(netstat -tulnp | grep -c ":$LOCAL_5424 ") -gt 0 ]] && { echo "LOCALHOST port $LOCAL_5424 already in use"; exit 1; }
        [[ $(netstat -tulnp | grep -c ":$LOCAL_3164 ") -gt 0 ]] && { echo "LOCALHOST port $LOCAL_3164 already in use"; exit 1; }
    fi
}
install_syslog_ng() {
    [[ $verbose -gt 0 ]] && echo "Installing syslog-ng"
    printf \
        'Package: syslog-ng*\nPin: version 3.16.1-1*\nPin-Priority: 1001' > /etc/apt/preferences.d/pin-syslog-ng
    grep -q \
        "deb http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_${DISTRIB_RELEASE} ./" \
        /etc/apt/sources.list /etc/apt/sources.list.d/*.list || \
        echo "deb http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_${DISTRIB_RELEASE} ./" \
        > /etc/apt/sources.list.d/syslog-ng.list
    wget -qO- \
        "http://download.opensuse.org/repositories/home:/laszlo_budai:/syslog-ng/xUbuntu_${DISTRIB_RELEASE}/Release.key" \
        | sudo apt-key add - >/dev/null
    apt-get update
    sudo apt-get -y purge rsyslog && \
        sudo apt-get -y install syslog-ng-core syslog-ng-mod-add-contextual-data
    if ! grep -q "net.core.rmem_max=1048576" /etc/sysctl.conf
    then
        echo "net.core.rmem_max=1048576" >> /etc/sysctl.conf
        sysctl -p
    fi
    [[ $verbose -gt 0 ]] && echo "Creating /etc/syslog-ng/conf.d/fwd-to-neo.conf"
    cat << EOF > /etc/syslog-ng/conf.d/fwd-to-neo.conf
# Local forwarding to NEO containers
# Generated on $(date)
options {
    chain_hostnames(off);
    flush_lines(10000);
    threaded(yes);
    use_dns(yes); # This should be set to no in high scale environments
    use_fqdn(no);
    keep_hostname(yes);
    dns-cache-size(2000);
    dns-cache-expire(87600);
    use-dns(persist_only);
    dns-cache-hosts(/etc/hosts);
    owner("root");
    group("root");
    perm(0640);
    stats_freq(0);
    time_reopen(5);
};

source s_local {
    system();
    internal();
};

source s_rfc3164 {
    network(
      transport("tcp")
      port($LOCAL_3164)
      log-iw-size(20000)
     );

    network(
      transport("udp")
      so_rcvbuf(1048576)
      flags("no-multi-line")
      port($LOCAL_3164)
    );
};

source s_rfc5424 {
    network(
      transport("tcp")
      flags(syslog-protocol)
      port($LOCAL_5424)
    );
};

destination d_rfc3164 {
   tcp("localhost" port($LZ_3164));
};
destination d_rfc5424 {
   tcp("localhost" port($LZ_5424));
};

log {
   source(s_rfc3164);
   destination(d_rfc3164);
};
log {
   source(s_rfc5424);
   destination(d_rfc5424);
};

EOF
[[ $verbose -gt 0 ]] && echo "Restarting syslog-ng"
service syslog-ng restart
}

source /etc/lsb-release
if [[ $DISTRIB_ID == "Ubuntu" ]]; then
    config_neo_ports
    #install_syslog_ng
else
    echo "This script is only meant for Ubuntu"
fi

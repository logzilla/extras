#!/bin/bash

# A POSIX variable
OPTIND=1

# Initialize our own variables:
verbose=0
LZ_3164=32514
LZ_5424=32601

usage="$(basename "$0") [-h] [-v] [-b n] [-s n] -- Sets up local server forwarding to specified [b]sd and [s]yslog ports.
where:
-h  show this help text
-v  Verbose mode
-b  set the port number that NEO is listening on for BSD-style (rfc3164), default is 32514
-s  set the port number that NEO is listening on for Syslog-style (rfc5424), default is 32601"
[[ "$#" -eq 0 ]] && { echo "$usage"; exit; }

while getopts ':hvs:b:' option; do
    case "$option" in
        h) echo "$usage"
            exit
            ;;
        s) LZ_5424=$OPTARG
            ;;
        b) LZ_3164=$OPTARG
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

exit

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
      port(514)
      log-iw-size(20000)
     );

    network(
      transport("udp")
      so_rcvbuf(1048576)
      flags("no-multi-line")
      port(514)
    );
};

source s_rfc5424 {
    network(
      transport("tcp")
      flags(syslog-protocol)
      port(601)
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
    install_syslog_ng
else
    echo "This script is only meant for Ubuntu"
fi

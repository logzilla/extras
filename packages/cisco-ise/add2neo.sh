#!/bin/bash

LZ=$(command -v logzilla)
if [[ ! -f "$LZ" ]]; then
    echo "This server doesn't have LogZilla NEO Installed"
    echo "Please run:"
    echo "curl -fsSL https://logzilla.sh | bash"
    exit 1
fi

version="$(logzilla version)"

if [[ "$version" -ne 61 ]]; then
    echo "This package is mean for NEO version 6.1.0"
    echo "It may work on newer packages but has not been tested"
    echo "To force, edit this script and comment out the 'exit 1' statement"
    exit 1
fi

docker cp 006.logzilla.log-outputs.conf lz_syslog:/etc/syslog-ng/conf.d/
docker cp cise.conf lz_syslog:/etc/syslog-ng/conf.d/
docker restart lz_syslog
$LZ rules add 500-cisco-ise.json
$LZ rules reload
$LZ dashboards import -I Cisco-ISE.json

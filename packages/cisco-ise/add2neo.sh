#!/bin/bash

DOCKER_ROOT=$(docker info 2>/dev/null | grep Root | awk '{print $4}')
LZ=$(command -v logzilla)
if [[ ! -f "$LZ" ]]; then
    echo "This server doesn't have LogZilla NEO Installed"
    echo "Please run:"
    echo "curl -fsSL https://logzilla.sh | bash"
    exit 1
fi

version="$(logzilla version | perl -pe 's/v(\d)\.(\d).+/$1$2/g')"

if [[ "$version" -lt 62 ]]; then
    echo "This package is intended for NEO version 6.2.x"
    echo "It may work on newer versions but has not been tested"
    echo "To force, edit this script and comment out the 'exit 1' statement"
    exit 1
fi

cp syslog-ng/*.conf "$DOCKER_ROOT/volumes/lz_config/_data/syslog-ng/"

for rule in ls rules.d/*.json
do
  [ -f "${rule}" ] || continue
  $LZ rules add "${rule}"
done

$LZ dashboards import -I dashboards/cisco-ise-dashboard.json
$LZ rules reload
docker restart lz_syslog

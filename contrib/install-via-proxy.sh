#!/bin/bash

###################################
# Manual Install via internal proxy
###################################

# This script is intended to allow
# users to manually install a specific
# version of LogZilla when docker.io
# is unavailable/routed through an
# internal corporate docker provider such
# as Sonatype Nexus (https://www.sonatype.com/products/repository-oss)

# Your internal corporate docker proxy
# must be defined in /etc/docker/daemon.json
# e.g.: 
# {
#   "insecure-registries" : ["https://docker.logzilla.io"],
#   "registry-mirrors" : ["https://docker.logzilla.io"]
# }
#
# Once defined, be sure to restart docker, e.g.:
# systemctl restart docker


###################################
# Change to the desired logzilla version
# NOTE: using docker alias tags will not 
# work since docker.io is unreachable
# e.g.: "latest" will not work, so
# you must specify the version to install
###################################
lzVersion="v6.12.5"




###################################
# Do not change anything below
###################################
DOCKER=$(command -v docker) || { echo "Error: Unable to locate docker executable"; exit 1; }
$DOCKER ps -a | grep lz_ && { 
  echo
  echo "WARNING: LogZilla images already exist"
  echo "on this server. If LogZilla is the only"
  echo "software installed, you should run"
  echo "'docker system prune -a --volumes'"
  echo "to start from a clean/fresh setup first"
  exit 0
  }
images=(
  "library/influxdb:1.8.2-alpine"
  "library/postgres:10.14-alpine"
  "library/redis:6.0.6-alpine"
  "library/telegraf:1.15.3-alpine"
  "logzilla/etcd:v3.4.3"
  "logzilla/front:${lzVersion}"
  "logzilla/kinesis:${lzVersion}"
  "logzilla/mailer:${lzVersion}"
  "logzilla/runtime:${lzVersion}"
  "logzilla/sec:${lzVersion}"
  "logzilla/syslogng:${lzVersion}"
)

for img in ${images[@]}; do
  $DOCKER pull "${img}"
done


BINDIR="/usr/local/bin"
[[ -d "$BINDIR" ]] || BINDIR="/usr/bin"
[[ -s $(command -v logzilla) ]] || rm -f "$(command -v logzilla)"
LZ=$(command -v logzilla)
if [[ ! -f "$LZ" ]]; then
  $DOCKER run --rm -v /var/run/docker.sock:/var/run/docker.sock "logzilla/runtime:${lzVersion}" lz-manager script > $BINDIR/logzilla
  chmod 755  $BINDIR/logzilla
  LZ=$(command -v logzilla)
fi

{
  $BINDIR/logzilla install \
    --pull=0 \
    --version ${lzVersion} \
    --http-port-mapping=tcp/80:80 \
    --syslog-port-mapping=tcp/514:514,udp/514:514,tcp/601:601 \
    2>&1 | tee /dev/fd/3
      err=$(cat<&3)
    } 3<<EOF
EOF
licURL=$(echo "$err" | grep "Cannot download license. Check network" | perl -pe 's/.+(https[^,]+).*/$1/g')

if [[ $licURL ]]; then
  # Since this system doesn't have internet access, the initial license download will fail
  vol=$($DOCKER inspect --format '{{.Mountpoint}}' lz_config)
  echo
  echo "###########################"
  echo "# ERROR: Air Gapped System Detected" 
  echo "###########################"
  echo "You must manually retrieve the license from:"
  echo "$licURL"
  echo
  echo "Paste the contents of that file in $vol/logzilla_license.json"
  echo "and run 'logzilla start'"
fi

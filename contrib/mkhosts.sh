#!/bin/bash

# This script can be used to easily create
# a hosts file for environments that do
# not have reverse DNS available
# but still want hostnames instead of IP's
# to show up in the UI

# Note: Requires jq (apt install -y jq)

# Obtained from 'logzilla authtoken create':
token="ac3e5526f03b77f3f0f4d316904495ce579cb51d2e53a508"
apiURL="http://192.168.10.135/api"
hostsFile="/etc/logzilla/hosts.in"

declare -A entries
ips=($(curl -sH "Content-Type: application/json; charset=utf-8" -H "Authorization: token $token" "$apiURL/dictionaries/host?limit=1000" | jq -r '.list[].name' | grep -P '^\d{1,3}\.'))

echo
for ip in "${ips[@]}"; do
  if ! grep -q "$ip" ${hostsFile}; then
  echo -n "Set hostname for $ip: ";
  read;
  #echo "$ip ${REPLY}"
  [[ "${REPLY}" ]] && entries["${REPLY}"]="$ip"
  else
    echo "[SKIPPED] IP "\"${ip}\"" already exists in ${hostsFile}"
  fi
done

echo
echo "### Adding entries to ${hostsFile}"
echo
for key in "${!entries[@]}"; do
  val="${entries[$key]}"
  if ! grep -q "$key\|$val" ${hostsFile}; then
    echo "${val} $key" >> ${hostsFile}
  else
    echo "[SKIPPED] Either host "\"$key\"" or IP "\"${val}\"" already exists in ${hostsFile}"
  fi
done

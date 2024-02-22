#!/bin/bash

# Simple script to create a LogZilla rule for
#   identifying the program types 
#   based on Fortinet's log id's
#
# This may or may not work in the future :)
# Prereqs:
# wget, bash (version 4+), and perl

docurl="https://docs.fortinet.com/document/fortigate/6.2.0/cookbook/986892/sample-logs-by-log-type"
wget "$docurl" -O $$.tmp

OLDIFS=${IFS}
IFS=$'\n'
lines=($(cat $$.tmp | grep -P '<pre>.*logid.*</pre>' | sed -e "s///" | sed 's/&nbsp;/ /g' | sed 's/<b>//g' | sed 's/<\/b>//g'))

# do not create tags for these highly variable items unless you know what you are doing
notags=(logid url ref filename analyticscksum filesize filetype cookies dstuuid poluuid srcmac srcuuid sn eventtime dstuuid sessionid duration sentbyte rcvdbyte sentpkt rcvdpkt mastersrcmac utmref date time countips sentdelta rcvddelta fctuid auditid audittime session_id bssid signal noise live age snclosest radioidclosest cldobjid hostname xid qname ipaddr qtypeval incidentserialno scertcname scertissuer count wanin wanout lanin lanout)

cat << 'EOF'
pre_match:
- comment:
  - Match on FortiGate events
  - "Rules are based on FortiGate's documentation from:"
  - 'https://docs.fortinet.com/document/fortigate/6.2.0/cookbook/986892/sample-logs-by-log-type'
  field: message
  op: =~
  value: date=\S+ time=\S+ logid="
rewrite_rules:
EOF

for k in "${!lines[@]}"
do
  type=$(echo ${lines[$k]} | perl -pe 's/.*date=.* type="([^"]+)".*/$1/g')
  subtype=$(echo ${lines[$k]} | perl -pe 's/.*date=.* subtype="([^"]+)".*/$1/g')
  logid=$(echo ${lines[$k]} | perl -pe 's/.*date=.* logid="([^"]+)".*/$1/g')
  sample=$(echo ${lines[$k]} | perl -pe 's/<pre>(.*)<\/pre>/$1/g' | sed "s/'//g" )
  keys=($(echo "$sample" | perl -pe 's/.*\s+(\S+)="\S+".*/$1/g'))
  #echo "TYPE = $type"
  #echo "SUBTYPE = $subtype"
  #echo "LOGID = $logid"
  #echo "SAMPLE = $sample"
  cat << EOF
- comment:
  - 'FortiGate: $type > $subtype'
  - 'Sample Log:'
  - '$sample'
  match:
    field: message
    op: =*
    value: logid="$logid
  tag:
EOF
keys=($(echo $sample | grep -Po '\S+="' | sed 's/="//g' | grep -Ev 'date|time' | sort -u | perl -pe 's/<\/?\S+>//g'))
for k in "${!keys[@]}"
do
  if [[ ! " ${notags[@]} " =~ " ${keys[$k]} " ]]; then
    echo "    FortiGate ${keys[$k]^}: \${${keys[$k]}}"
  fi
done
cat << EOF
  rewrite:
    program: FortiGate ${type^}
EOF
done
IFS=$OLDIFS
rm $$.tmp

#!/bin/bash
# script to create a LogZilla rule for
#   identifying the program types 
#   based on bro's tsv fields
#
# This may or may not work in the future :)
# req's:
# wget, bash (version 4+), and perl

[[ -z $1 ]] && { echo "Supply an input file, e.g: $0 fields"; exit 1; }

OLDIFS=${IFS}
IFS=$'\n'

# specify output directory for created rules:
OUTDIR="rules.d"
mkdir -p $OUTDIR

# do not create tags for these highly variable items unless you know what you are doing
# some of these may not be highly variable, but are just not useful as tags
notags=(ts id uid duration orig_bytes resp_bytes conn_state missed_bytes history orig_pkts orig_ip_bytes resp_pkts resp_ip_bytes mac lease_time trans_id rtt rcode TTLs fuid conn_uids filename seen_bytes total_bytes missing_bytes overflow_bytes timedout parent_fuid md5 sha1 sha256 extracted_size request_body_len response_body_len status_code info_code orig_fuids resp_fuids get_requests get_bulk_requests get_responses set_requests display_string up_since remote_location.latitude remote_location.longitude cert_chain_fuids client_cert_chain_fuids facility severity message basic_constraints.path_len uids id.orig_p software_type version.minor3 failure_reason requested_addr notice certificate.version certificate.serial certificate.not_valid_before certificate.not_valid_after certificate.key_length certificate.exponent certificate.curve host_key curve client_issuer client_subject next_protocol addl reassem_file_size events_proc events_queue depth pkts_link timers bytes_recv tcp_conns ts_delta pkt_lag tcp_conns pkts_proc mem trans_depth active_timers last_alert analyzers active_dns_requests auth_attempts auth_success established pkts_dropped rejected active_icmp_conns active_udp_conns dns_requests icmp_conns percent_lost reassem_frag_size reassem_tcp_size reassem_unknown_size suppress_for udp_conns acks active_files active_tcp_conns events_queued note src tunnel_parents)

# put fields here that you don't need in the message display of the logzilla console
# generally, things like timestamp are useless since the syslog packet itself also contains a ts
nomsg=(ts id uid facility severity)

# combine some fields for tagging
# this is useful for things like ip addresses
# because logzilla will also filter by the generating program
# thus, having the same ip listed for bro_conn and another log with the same ip would just create a single entry instead of two
declare -A tagmerge
tagmerge=(["id_orig_h"]="srcip" ["id_resp_h"]="dstip" ["id_orig_p"]="srcport" ["id_resp_p"]="dstport" ["tx_hosts"]="srcip" ["rx_hosts"]="dstip")

# TODO:
# Create the following automatically
hctags='hc_tags: ["Zeek san_ip","Zeek srcip","Zeek dstip","Zeek host_key", "Zeek host", "Zeek server_addr", "Zeek assigned_addr", "Zeek client_addr", "ip" ]'

lines=($(grep '#fields' $1))
seen=""
for k in "${!lines[@]}"
do
  prog=$(echo ${lines[$k]} | cut -d '.' -f1)
  if [[ ! "$seen" == *"$prog"* ]]; then
    fieldlist=$(echo ${lines[$k]} | cut -d '	' -f2-)
    re=$(echo -n $fieldlist | perl -pe 's/\S+\t?/\(\[^\\t\]+\)\\t/g' | sed 's/\\t$//')
    echo "Creating $OUTDIR/400-bro_$prog.yaml"
    cat << EOF > $OUTDIR/400-bro_$prog.yaml
$hctags
pre_match:
- comment:
  - Match on Zeek events
  - Note that this rule assumes a TSV incoming log from Zeek, not JSON
  field: program
  op: =*
  value: bro_$prog
rewrite_rules:
- comment:
  - 'Zeek $prog log'
  match:
    field: message
    op: =~
    value: ^$re\$
  tag:
EOF
NIFS=$IFS
IFS='	' read -r -a fields <<< "$fieldlist"
fields_used=""
for k in "${!fields[@]}"
do
  position=$(($k+1)) 
  name=$( echo "${fields[$k]}" | sed "s/[.-]/_/g")
  if [[ ${tagmerge[$name]+_} ]]; then
    name=${tagmerge[$name]}
    fields[$k]=${tagmerge[$name]}
  fi
  if [[ ! " ${nomsg[@]} " =~ " ${fields[$k]} " ]]; then
  fields_used="$fields_used $name=\"\$$position\""
    if [[ ! " ${notags[@]} " =~ " ${fields[$k]} " ]]; then
      echo "    Zeek $name: \$$position" >> $OUTDIR/400-bro_$prog.yaml
    fi
  fi
done
IFS=$NIFS
cat << EOF >> $OUTDIR/400-bro_$prog.yaml
  rewrite:
    message: $fields_used
EOF
    echo "Creating $OUTDIR/401-bro_${prog}_kvclean.yaml"
cat << EOF > $OUTDIR/401-bro_${prog}_kvclean.yaml
rewrite_rules:
- comment:
  - Remove kv pairs that have no values 
  - 'e.g: foo="-"'
  match:
    field: program
    op: =*
    value: bro_$prog
  replace:
    field: message
    expr: \S+="-"
    fmt: ""
EOF
seen="$seen $prog"
fi
done
IFS=$OLDIFS

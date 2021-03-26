#!/bin/bash
# Simple script to create dashboards
#
# This may or may not work in the future :)
# wget, bash (version 4+), and perl

[[ -z $1 ]] && { echo "Supply an input file, e.g: $0 fields"; exit 1; }

OLDIFS=${IFS}
IFS=$'\n'

# do not create widgets for these highly variable items unless you know what you are doing
# some of these may not be highly variable, but are just not useful as tags
notags=(ts id uid duration orig_bytes resp_bytes conn_state missed_bytes history orig_pkts orig_ip_bytes resp_pkts resp_ip_bytes mac lease_time trans_id rtt rcode TTLs fuid conn_uids filename seen_bytes total_bytes missing_bytes overflow_bytes timedout parent_fuid md5 sha1 sha256 extracted_size request_body_len response_body_len status_code info_code orig_fuids resp_fuids get_requests get_bulk_requests get_responses set_requests display_string up_since remote_location.latitude remote_location.longitude cert_chain_fuids client_cert_chain_fuids facility severity message basic_constraints.path_len uids id.orig_p software_type version.minor3 failure_reason requested_addr notice certificate.version certificate.serial certificate.not_valid_before certificate.not_valid_after certificate.key_length certificate.exponent certificate.curve host_key curve client_issuer client_subject next_protocol addl reassem_file_size events_proc events_queue depth pkts_link timers bytes_recv tcp_conns ts_delta pkt_lag tcp_conns pkts_proc mem trans_depth active_timers last_alert analyzers active_dns_requests auth_attempts auth_success established pkts_dropped rejected active_icmp_conns active_udp_conns dns_requests icmp_conns percent_lost reassem_frag_size reassem_tcp_size reassem_unknown_size suppress_for udp_conns acks active_files active_tcp_conns events_queued)

# combine some fields for tagging
# this is useful for things like ip addresses
# because logzilla will also filter by the generating program
# thus, having the same ip listed for bro_conn and another log with the same ip would just create a single entry instead of two
declare -A tagmerge
tagmerge=(["id_orig_h"]="srcip" ["id_resp_h"]="dstip" ["id_orig_p"]="srcport" ["id_resp_p"]="dstport" ["tx_hosts"]="srcip" ["rx_hosts"]="dstip")

lines=($(grep '#fields' $1 | sort -ut '.' -k 1,1))
#echo "${lines[*]}" && exit
for k in "${!lines[@]}"
do
  prog=$(echo "bro_${lines[$k]}" | cut -d '.' -f1)
  #echo "PROG: $prog"
  fieldlist=$(echo ${lines[$k]} | cut -d '	' -f2-)

  # Test only 1 prog for now - remove the if below
  #if [[ $prog == "bro_conn" ]]; then
  #echo "PROG = $prog"
  cat << EOF
- config:
    style_class: infographic
    time_range:
      preset: last_1_minutes
    title: Zeek $prog
  is_public: true
  widgets:
  - config:
      col: 0
      filter:
      - field: program
        op: eq
        value:
        - $prog
      limit: 10
      row: 0
      sizeX: 6
      sizeY: 2
      sort: -first_occurrence
      time_range:
        preset: last_1_minutes
      title: All $prog Events
    type: Search
EOF
NIFS=$IFS
IFS='	' read -r -a fields <<< "$fieldlist"
col=0
row=2
modulo=0
for k in "${!fields[@]}"
do
  #echo "FIELD: ${fields[$k]}"
  modulo=$(($col%6))
  name=$( echo "${fields[$k]}" | sed "s/[.-]/_/g")
    if [[ ${tagmerge[$name]+_} ]]; then
      # echo "MATCH: $name is set to ${tagmerge[$name]}  "
      name=${tagmerge[$name]}
      fields[$k]=${tagmerge[$name]}
    fi
  if [[ ! " ${notags[@]} " =~ " ${fields[$k]} " ]]; then
  cat << EOF
  - config:
      col: $modulo
      field: Zeek $name
      filter:
      - field: program
        op: eq
        value:
        - $prog
      limit: 5
      row: $row
      show_other: false
      time_range:
        preset: last_1_minutes
      title: $prog $name
      view_type: pie_chart
    type: TopN
EOF
  col=$(($col+2)) 
  row=$(($row+1)) 
  fi
done
IFS=$NIFS
#fi
done
IFS=$OLDIFS

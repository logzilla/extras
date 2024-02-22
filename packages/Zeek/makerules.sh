#!/bin/bash
# Script to create a LogZilla rule for
#   identifying the program types 
#   based on bro's tsv fields
# This may or may not work in the future :)
#
# req's:
# wget, bash (version 4+), and perl

while getopts 'f:n:v:sd' opt ; do
  case "$opt" in
    f) fieldsFile="$OPTARG"
      ;;
    n) notagsFile="$OPTARG"
      ;;
    s) skipRuleTags=1
      ;;
    d) dashboards=1
      ;;
    v) verbosity="$OPTARG"
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2; exit 1
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      ;;
  esac
done
shift $(( OPTIND - 1 ))
[[ "$fieldsFile" ]] || fieldsFile="./fields.tsv"
[[ -f $fieldsFile ]] || { echo "$fieldsFile is missing (e.g.: -f fields.tsv)" ; exit 1 ; }
[[ "$notagsFile" ]] || notagsFile="./notags.txt"
[[ -f $notagsFile ]] || { echo "$notagsFile is missing (e.g.: -n notags.txt)" ; exit 1 ; }
[[ "$verbosity" ]] || verbosity=0
outdir="rules.d" && mkdir -p $outdir
dashdir="dashboards" && mkdir -p $dashdir


declare -a notags
while IFS= read -r line; do
  [[ "$line" == *"#"* ]] && continue
  notags+=("$line")
done < $notagsFile
[[ $verbosity -gt 1 ]] && printf "[DEBUG] Notags Array:\n${notags[*]}\n"

# put fields here that you don't need in the message display of the logzilla console
# generally, things like timestamp are useless since the syslog packet itself also contains a ts
nomsg=(ts id uid facility severity uids fuid)

# combine some fields for tagging
# this is useful for things like ip addresses
# because logzilla will also filter by the generating program
# thus, having the same ip listed for bro_conn and another log with the same ip would just create a single entry instead of two
declare -A tagmerge
tagmerge=(["id_orig_h"]="srcip" ["id_resp_h"]="dstip" ["id_orig_p"]="srcport" ["id_resp_p"]="dstport" ["tx_hosts"]="srcip" ["rx_hosts"]="dstip")

# TODO: Create the following automatically
hctags='hc_tags: ["Zeek san_ip","Zeek srcip","Zeek dstip","Zeek host_key", "Zeek host", "Zeek server_addr", "Zeek assigned_addr", "Zeek client_addr", "ip", "Zeek uri", "Zeek query", "Zeek answers", "Zeek referrer", "Zeek dst" ]'

OLDIFS=${IFS}
IFS=$'\n'

seen=""
lines=($(grep '#fields' $fieldsFile))
for k in "${!lines[@]}"
do
  prog=$(echo ${lines[$k]} | cut -d '.' -f1)
  if [[ ! "$seen" == *"$prog"* ]]; then
    fieldlist=$(echo ${lines[$k]} | cut -d '	' -f2-)
    re=$(echo -n $fieldlist | perl -pe 's/\S+\t?/\(\[^\\t\]+\)\\t/g' | sed 's/\\t$//')
    [[ $verbosity -gt 0 ]] && echo "Creating $outdir/400-bro_$prog.yaml"
    cat << EOF > $outdir/400-bro_$prog.yaml
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
  - 'Zeek $prog events'
  match:
    field: message
    op: =~
    value: ^$re\$
EOF
[[ $skipRuleTags -eq 1 ]] || echo "  tag:" >> $outdir/400-bro_$prog.yaml

# Make dashboards if opted for
if [[ $dashboards -eq 1 ]]; then
  cat << EOF > $dashdir/dashboard-bro_$prog.yaml
- config:
    style_class: infographic
    time_range:
      preset: last_1_minutes
    title: Zeek $prog events
  is_public: true
  widgets:
  - config:
      col: 0
      filter:
      - field: program
        op: eq
        value:
        - bro_$prog
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
fi

NIFS=$IFS
IFS='	' read -r -a fields <<< "$fieldlist"
fields_used=""
col=0
row=2
modulo=0
for k in "${!fields[@]}"
do
  position=$(($k+1)) 
  modulo=$(($col%6))
  name=$( echo "${fields[$k]}" | sed "s/[.-]/_/g")
  if [[ ${tagmerge[$name]+_} ]]; then
    name=${tagmerge[$name]}
    fields[$k]=${tagmerge[$name]}
  fi
  if [[ ! " ${nomsg[@]} " =~ " ${fields[$k]} " ]]; then
  fields_used="$fields_used $name=\"\$$position\""
    if [[ ! " ${notags[@]} " =~ " ${fields[$k]} " ]]; then
      [[ $skipRuleTags -eq 1 ]] || echo "    Zeek $name: \$$position" >> $outdir/400-bro_$prog.yaml
if [[ $dashboards -eq 1 ]]; then
  cat << EOF >> $dashdir/dashboard-bro_$prog.yaml
  - config:
      col: $modulo
      field: Zeek $name
      filter:
      - field: program
        op: eq
        value:
        - bro_$prog
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
    fi
  fi
done
IFS=$NIFS
cat << EOF >> $outdir/400-bro_$prog.yaml
  rewrite:
    message:$fields_used
EOF
seen="$seen $prog"
fi
done
IFS=$OLDIFS
[[ $verbosity -eq 0 ]] && echo "Rules created in $outdir/"
[[ $dashboards -eq 1 ]] && echo "Dashboards created in $dashdir/"

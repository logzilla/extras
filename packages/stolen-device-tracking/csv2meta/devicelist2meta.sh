#!/bin/bash
DEBUG=0
[[ $# -ne 1 ]] && { echo "Usage: $0 <csv filename>"; exit 1; }

# Edit filenames as needed:
output_metadata_file="metaData.csv"
output_metadata_conf="metaData.conf"
input_source_ip_file="sourceIPs.txt"

[[ -f "${input_source_ip_file}" ]] || { echo "Missing file: 'sourceIPs.txt'"; exit 1; }
echo "Creating $output_metadata_file..."
[[ -f "${output_metadata_file}" ]] && { echo "${output_metadata_file} already exists, remove it first please"; exit 1; }
while IFS=, read -r serial mac1 mac2 name
do
    if [[ $DEBUG -gt 0 ]]; then
        echo "Serial = $serial"
        echo "MAC 1 = $mac1"
        echo "MAC 2 = $mac2"
        echo "System Name = $name"
    fi
    while read -r IP
    do
        printf '%s\n' \
            "$IP,deviceSerial,$serial" \
            "$IP,sMAC1,$mac1" \
            "$IP,sMAC2,$mac2" \
            "$IP,deviceName,$name" >> ${output_metadata_file}
    done < sourceIPs.txt
done < "$1"

[[ $DEBUG -gt 0 ]] && echo "#-----BEGIN: metadata.conf------"

echo "Creating $output_metadata_conf..."
[[ -f "${output_metadata_conf}" ]] && { echo "${output_metadata_file} already exists, remove it first please"; exit 1; }
cat << EOF > "${output_metadata_conf}"
parser p_add_context_data {
    add-contextual-data(
    selector("\$HOST"),
    database("/etc/syslog-ng/conf.d/${output_metadata_file}"),
    default-selector("unknown"),
    prefix("meta."));
};

rewrite r_add_meta{
   set("STARTMETA:deviceSerial=\"\${meta.deviceSerial}\" sMAC1=\"\${meta.sMAC1}\" sMAC2=\"\${meta.sMAC2}\" deviceName=\"\${meta.deviceName}\"ENDMETA \$MSG", value("MESSAGE"));
};
EOF

echo "Completed, place $output_metadata_file and $output_metadata_conf in /etc/syslog-ng/conf.d and restart syslog-ng"

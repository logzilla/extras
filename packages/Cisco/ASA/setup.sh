#!/usr/bin/env bash

sudo logzilla HIGH_CARDINALITY_TAGS "ASA Buildup Source Real IPs,ASA Buildup Source Mapped IPs,ASA Buildup Destination Real IPs,ASA Buildup Destination Mapped IPs,ASA Teardown Source Real IPs,ASA Teardown Destination Real IPs"
sudo logzilla restart
sudo logzilla dashboards import -I dashboards/cisco-asa-buildup-teardown.dashboard.json
sudo logzilla rules add rules.d/500-cisco-asa-connection-buildup-teardown.yaml -f

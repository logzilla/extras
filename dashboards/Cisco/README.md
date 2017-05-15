# LogZilla Dashboard For Cisco Systems



## Cisco Networks
This dashboard provides an overview for Cisco-based Network Events. Widgets included:

* Top 10 Cisco Devices with Failures
* Cisco Events Per Second
* Failed Events
* Errored Hosts
* Cisco Events Per Day
* Top Cisco Mnemonics
* Duplex Mismatch
* Device Power Events
* Subnet Mismatch
* Last 5 Cisco Mnemonics

## Cisco Most Actionable
This dashboard provides an overview for Cisco's `Most Actionable` network Events. Widgets included:

* Cisco: Spanning Tree BPDU
* Cisco: ASIC Module Error
* Cisco: OSPF Neighbor Change
* Cisco: Non IPSec-encapsulated Crypto
* Cisco: Crypto IKE Message Failure
* Cisco: ASIC Port Error
* Cisco: IPSec Error - Packet Missing from SADB
* Cisco: Crypto Packet Security Association Missing
* Cisco: Crypto Packet failed MAC verification
* Cisco: OSPF process received an invalid packet
* Cisco: Error disabled port has been reenabled
* Cisco: OSPF received LSA with wrong mask
* Cisco: HSRP VIP does not match the standby VIP
* Cisco: Unauthorized connection attempt on a secure port.
* Cisco: OSPF Hello Unidentified Sender
* Cisco: Interface disabled due to misconfiguration
* Cisco: Spanning Tree BPDU received from another bridge


## Cisco Firewalls
This dashboard provides an overview for Cisco-based Firewall Events. Widgets included:

* Last 10 High Severity Cisco Firewall Messages
* Live Stream: OpenSSL Handshake Failures
* Audit Failures
* Live Stream: High Severity Events
* Cisco Firewall Events Per Second
* Cisco Firewall Events Per Day
* 77 Patterns for Most Actionable Firewall Events
 - This widget contains 77 of Cisco's `Most Actionable` Firewall Events.


# Import/Export
Import
---
	~logzilla/src/bin/lz5dashboards import -I mydashboards.json

Export
---
	~logzilla/src/bin/lz5dashboards export -O mydashboards.json


Import/Export from the UI
---

LogZilla version > `5.74` can do the same import and export directly from the UI.


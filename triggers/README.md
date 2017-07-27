# LogZilla Triggers


The default list of Triggers included out of the box contain the following with more added every release (every 2 weeks):

* Cisco: Most Actionable Events (note: this trigger contains almost `1000` of Cisco's top "Actionable" events!)
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
* Windows: Process Started
* Windows: User Logon
* Windows: User Fileshare Accesses
* Windows: New Service Installed
* Windows: New Network Connection Established
* Windows: File Added/Modified/Deleted
* Windows: New Registry Item Added
* Windows: Powershell Execution
* Windows: New Scheduled Task Added
* Windows: New Firewall Rule Added
* Cisco: Duplex Mismatch
* Cisco: DTP Port Channel
* Cisco: Audit Logging
* Cisco: Spanning Tree Root Change

# Format
LogZilla Triggers are stored in standard JSON format. As of `v5.72.1`, triggers can be imported and exported from the command line, for example:

# Import
```
~logzilla/src/bin/lz5triggers import -I mytriggers.json
```

# Export

```
~logzilla/src/bin/lz5triggers export -O mytriggers.json
```

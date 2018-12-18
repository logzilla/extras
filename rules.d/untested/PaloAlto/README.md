# PAN Syslog Events

For organizations using PAN-OS v8.0+, you must use the formats below which can be copied and pasted into your syslog server profile(s) in the PAN-OS dashboard.

# Port and Format

Set the PAN-OS log destination to NEO as any one of these:

* `UDP` port `514`, `BSD` protocol
* `TCP` port `514`, `BSD` protocol
* `TCP` port `601`, `IETF` protocol


# Formats

## Configuration 
 
``` 
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" devTime="$cef-formatted-receive_time" src="$host" VirtualSystem="$vsys" msg="$cmd" usrName="$admin" client="$client" Result="$result" ConfigurationPath="$path" sequence="$seqno" ActionFlags="$actionflags" BeforeChangeDetail="$before-change-detail" AfterChangeDetail="$afterchange-detail" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name"
```

## System

```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" VirtualSystem="$vsys" Filename="$object" Module="$module" sev="$number-of-severity" Severity="$severity" msg="$opaque" sequence="$seqno" ActionFlags="$actionflags" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name"
```

## Threat
```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" src="$src" dst="$dst" srcPostNAT="$natsrc" dstPostNAT="$natdst" RuleName="$rule" usrName="$srcuser" SourceUser="$srcuser" DestinationUser="$dstuser" Application="$app" VirtualSystem="$vsys" SourceZone="$from" DestinationZone="$to" IngressInterface="$inbound_if" EgressInterface="$outbound_if" LogForwardingProfile="$logset" SessionID="$sessionid" RepeatCount="$repeatcnt" srcPort="$sport" dstPort="$dport" srcPostNATPort="$natsport" dstPostNATPort="$natdport" Flags="$flags" proto="$proto" action="$action" Miscellaneous="$misc" ThreatID="$threatid" URLCategory="$category" sev="$number-of-severity" Severity="$severity" Direction="$direction" sequence="$seqno" ActionFlags="$actionflags" SourceLocation="$srcloc" DestinationLocation="$dstloc" ContentType="$contenttype" PCAP_ID="$pcap_id" FileDigest="$filedigest" Cloud="$cloud" URLIndex="$url_idx" RequestMethod="$http_method" Subject="$subject" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" SrcUUID="$src_uuid" DstUUID="$dst_uuid" TunnelID="$tunnelid" MonitorTag="$monitortag" ParentSessionID="$parent_session_id" ParentStartTime="$parent_start_time" TunnelType="$tunnel" ThreatCategory="$thr_category" ContentVer="$contentver"
```

## Traffic

```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" Type="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" src="$src" dst="$dst" srcPostNAT="$natsrc" dstPostNAT="$natdst" RuleName="$rule" usrName="$srcuser" SourceUser="$srcuser" DestinationUser="$dstuser" Application="$app" VirtualSystem="$vsys" SourceZone="$from" DestinationZone="$to" IngressInterface="$inbound_if" EgressInterface="$outbound_if" LogForwardingProfile="$logset" SessionID="$sessionid" RepeatCount="$repeatcnt" srcPort="$sport" dstPort="$dport" srcPostNATPort="$natsport" dstPostNATPort="$natdport" Flags="$flags" proto="$proto" action="$action" totalBytes="$bytes" dstBytes="$bytes_received" srcBytes="$bytes_sent" totalPackets="$packets" StartTime="$start" ElapsedTime="$elapsed" URLCategory="$category" sequence="$seqno" ActionFlags="$actionflags" SourceLocation="$srcloc" DestinationLocation="$dstloc" dstPackets="$pkts_received" srcPackets="$pkts_sent" SessionEndReason="$session_end_reason" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" ActionSource="$action_source" SrcUUID="$src_uuid" DstUUID="$dst_uuid" TunnelID="$tunnelid" MonitorTag="$monitortag" ParentSessionID="$parent_session_id" ParentStartTime="$parent_start_time" TunnelType="$tunnel"
```

## HIP Match
```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" src="$src" dst="$dst" srcPostNAT="$natsrc" dstPostNAT="$natdst" RuleName="$rule" usrName="$srcuser" SourceUser="$srcuser" DestinationUser="$dstuser" Application="$app" VirtualSystem="$vsys" SourceZone="$from" DestinationZone="$to" IngressInterface="$inbound_if" EgressInterface="$outbound_if" LogForwardingProfile="$logset" SessionID="$sessionid" RepeatCount="$repeatcnt" srcPort="$sport" dstPort="$dport" srcPostNATPort="$natsport" dstPostNATPort="$natdport" Flags="$flags" proto="$proto" action="$action" Miscellaneous="$misc" ThreatID="$threatid" URLCategory="$category" sev="$number-of-severity" Severity="$severity" Direction="$direction" sequence="$seqno" ActionFlags="$actionflags" SourceLocation="$srcloc" DestinationLocation="$dstloc" ContentType="$contenttype" PCAP_ID="$pcap_id" FileDigest="$filedigest" Cloud="$cloud" URLIndex="$url_idx" RequestMethod="$http_method" UserAgent="$user_agent" identSrc="$xff" Referer="$referer" Subject="$subject" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" SrcUUID="$src_uuid" DstUUID="$dst_uuid" TunnelID="$tunnelid" MonitorTag="$monitortag" ParentSessionID="$parent_session_id" ParentStartTime="$parent_start_time" TunnelType="$tunnel" ThreatCategory="$thr_category" ContentVer="$contentver"
```

## URL Filtering

```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" src="$src" dst="$dst" srcPostNAT="$natsrc" dstPostNAT="$natdst" RuleName="$rule" usrName="$srcuser" SourceUser="$srcuser" DestinationUser="$dstuser" Application="$app" VirtualSystem="$vsys" SourceZone="$from" DestinationZone="$to" IngressInterface="$inbound_if" EgressInterface="$outbound_if" LogForwardingProfile="$logset" SessionID="$sessionid" RepeatCount="$repeatcnt" srcPort="$sport" dstPort="$dport" srcPostNATPort="$natsport" dstPostNATPort="$natdport" Flags="$flags" proto="$proto" action="$action" Miscellaneous="$misc" ThreatID="$threatid" URLCategory="$category" sev="$number-of-severity" Severity="$severity" Direction="$direction" sequence="$seqno" ActionFlags="$actionflags" SourceLocation="$srcloc" DestinationLocation="$dstloc" ContentType="$contenttype" PCAP_ID="$pcap_id" FileDigest="$filedigest" Cloud="$cloud" URLIndex="$url_idx" RequestMethod="$http_method" Subject="$subject" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" SrcUUID="$src_uuid" DstUUID="$dst_uuid" TunnelID="$tunnelid" MonitorTag="$monitortag" ParentSessionID="$parent_session_id" ParentStartTime="$parent_start_time" TunnelType="$tunnel" ThreatCategory="$thr_category" ContentVer="$contentver"
```

## Data Logs

```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" src="$src" dst="$dst" srcPostNAT="$natsrc" dstPostNAT="$natdst" RuleName="$rule" usrName="$srcuser" SourceUser="$srcuser" DestinationUser="$dstuser" Application="$app" VirtualSystem="$vsys" SourceZone="$from" DestinationZone="$to" IngressInterface="$inbound_if" EgressInterface="$outbound_if" LogForwardingProfile="$logset" SessionID="$sessionid" RepeatCount="$repeatcnt" srcPort="$sport" dstPort="$dport" srcPostNATPort="$natsport" dstPostNATPort="$natdport" Flags="$flags" proto="$proto" action="$action" Miscellaneous="$misc" ThreatID="$threatid" URLCategory="$category" sev="$number-of-severity" Severity="$severity" Direction="$direction" sequence="$seqno" ActionFlags="$actionflags" SourceLocation="$srcloc" DestinationLocation="$dstloc" ContentType="$contenttype" PCAP_ID="$pcap_id" FileDigest="$filedigest" Cloud="$cloud" URLIndex="$url_idx" RequestMethod="$http_method" FileType="$filetype" Sender="$sender" Subject="$subject" Recipient="$recipient" ReportID="$reportid" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" SrcUUID="$src_uuid" DstUUID="$dst_uuid" TunnelID="$tunnelid" MonitorTag="$monitortag" ParentSessionID="$parent_session_id" ParentStartTime="$parent_start_time" TunnelType="$tunnel" ThreatCategory="$thr_category" ContentVer="$contentver"
```

## Wildfire

```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" src="$src" dst="$dst" srcPostNAT="$natsrc" dstPostNAT="$natdst" RuleName="$rule" usrName="$srcuser" SourceUser="$srcuser" DestinationUser="$dstuser" Application="$app" VirtualSystem="$vsys" SourceZone="$from" DestinationZone="$to" IngressInterface="$inbound_if" EgressInterface="$outbound_if" LogForwardingProfile="$logset" SessionID="$sessionid" RepeatCount="$repeatcnt" srcPort="$sport" dstPort="$dport" srcPostNATPort="$natsport" dstPostNATPort="$natdport" Flags="$flags" proto="$proto" action="$action" sequence="$seqno" ActionFlags="$actionflags" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" TunnelID="$tunnelid" MonitorTag="$monitortag" ParentSessionID="$parent_session_id" ParentStartTime="$parent_start_time" TunnelType="$tunnel" totalBytes="$bytes" dstBytes="$bytes_received" srcBytes="$bytes_sent" totalPackets="$packets" dstPackets="$pkts_received" srcPackets="$pkts_sent" MaximumEncapsulation="$max_encap" UnknownProtocol="$unknown_proto" StrictChecking="$strict_check" TunnelFragment="$tunnel_fragment" SessionsCreated="$sessions_created" SessionsClosed="$sessions_closed" SessionEndReason="$session_end_reason" ActionSource="$action_source" startTime="$start" ElapsedTime="$elapsed"
```

## Authentication

```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" ServerProfile="$serverprofile" LogForwardingProfile="$logset" VirtualSystem="$vsys" AuthPolicy="$authpolicy" ClientType="$clienttype" NormalizeUser="$normalize_user" ObjectName="$object" FactorNumber="$factorno" AuthenticationID="$authid" src="$ip" RepeatCount="$repeatcnt" usrName="$user" Vendor="$vendor" msg="$event" sequence="$seqno" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" AdditionalAuthInfo="$desc" ActionFlags="$actionflags"
```

## User-ID
```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" FactorType="$factortype" VirtualSystem="$vsys" DataSourceName="$datasourcename" DataSource="$datasource" DataSourceType="$datasourcetype" FactorNumber="$factorno" VirtualSystemID="$vsys_id" TimeoutThreshold="$timeout" src="$ip" srcPort="$beginport" dstPort="$endport" RepeatCount="$repeatcnt" usrName="$user" sequence="$seqno" EventID="$eventid" FactorCompletionTime="$factorcompletiontime" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" ActionFlags="$actionflags"
```

## Tunnel Inspection
```
logsrc="PaloAlto" ReceiveTime="$receive_time" SerialNumber="$serial" cat="$type" Subtype="$subtype" devTime="$cef-formatted-receive_time" usrName="$srcuser" VirtualSystem="$vsys" identHostName="$machinename" OS="$os" identsrc="$src" HIP="$matchname" RepeatCount="$repeatcnt" HIPType="$matchtype" sequence="$seqno" ActionFlags="$actionflags" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" VirtualSystemID="$vsys_id" srcipv6="$srcipv6" startTime="$cef-formatted-time_generated"
```

## Correlation logs
```
logsrc="PaloAlto" SerialNumber="$serial" cat="$type" devTime="$cef-formatted-receive_time" startTime="$cef-formatted-time_generated" Severity="$severity" VirtualSystem="$vsys" VirtualSystemID="$vsys_id" src="$src" SourceUser="$srcuser" msg="$evidence" DeviceGroupHierarchyL1="$dg_hier_level_1" DeviceGroupHierarchyL2="$dg_hier_level_2" DeviceGroupHierarchyL3="$dg_hier_level_3" DeviceGroupHierarchyL4="$dg_hier_level_4" vSrcName="$vsys_name" DeviceName="$device_name" ObjectName="$object_name" ObjectID="$object_id"
```

# NEO Rule

Confirm that NEO is receiving events from your PAN device and then import the rule file from the NEO server's console:

```
wget https://raw.githubusercontent.com/logzilla/extras/master/rules.d/untested/PaloAlto/200-PaloAlto.json
logzilla rules add 200-PaloAlto.json
logzilla rules reload
```



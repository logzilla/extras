#********************************************************************
#      File:    logzilla.py
#      Author:  LogZilla Corporation (sales@logzilla.net)
#
#      Description:
#       Logzilla eStreamer adapter
#
#*********************************************************************/

# Disable "too many lines"
#pylint: disable=C0302

import socket
import json
import time
import copy
import re

import estreamer.adapters.kvpair
import estreamer.common
import estreamer.definitions as definitions
import estreamer

from estreamer.metadata import View

def __severity( priority, impact ):
    matrix = {
        1: {  # High
            1: '10',
            2: '9',
            3: '7',
            4: '8',
            5: '9'
        },
        2: {  # Medium
            1: '7',
            2: '6',
            3: '4',
            4: '5',
            5: '6'
        },
        3: {  # Low
            1: '3',
            2: '2',
            3: '0',
            4: '1',
            5: '2'
        }
    }

    if priority in matrix and impact in matrix[priority]:
        return matrix[priority][impact]

    return 5



def __ipv4( ipAddress ):
    if ipAddress.startswith('::ffff:'):
        return ipAddress[7:]

    elif ipAddress.find(':') == -1:
        return ipAddress

    return ''



def __ipv6( ipAddress ):
    if ipAddress == '::':
        return ''

    elif ipAddress.startswith('::ffff:'):
        return ''

    elif ipAddress.find(':') > -1:
        return ipAddress

    return ''



MAPPING = {

    # 71
    definitions.RECORD_RNA_CONNECTION_STATISTICS: {
        'sig_id': lambda rec: 'RNA:1003:1',

        'name': lambda rec: 'CONNECTION STATISTICS',

        'severity': lambda rec: 3 if rec['ruleAction'] < 4 else 7,

        'constants': {
            'cs1Label': 'fwPolicy',
            'cs2Label': 'fwRule',
            'cs3Label': 'ingressZone',
            'cs4Label': 'egressZone',
        },

        'lambdas': {
            'src_ip': lambda rec: __ipv4( rec['initiatorIpAddress'] ),
            'dst_ip': lambda rec: __ipv4( rec['responderIpAddress'] ),
        },

        'fields': {
            'deviceId': '',
            'ingressZone': '',
            'egressZone': '',
            'initiatorIpAddress': 'initiator_ip',
            'responderIpAddress': 'responder_ip',
            'originalClientIpAddress': '',
            'policyRevision': '',
            'ruleId': '',
            'tunnelRuleId': '',
            'ruleAction': 'rule_action',
            'ruleReason': 'rule_reason',
            'initiatorPort': 'src_port',
            'responderPort': 'dst_port',
            'tcpFlags': '',
            'protocol': 'proto',
            'netflowSource': '',
            'instanceId': '',
            'connectionCounter': '',
            'initiatorTransmittedPackets': '',
            'responderTransmittedPackets': '',
            'initiatorTransmittedBytes': '',
            'responderTransmittedBytes': '',
            'initiatorPacketsDropped': '',
            'responderPacketsDropped': '',
            'initiatorBytesDropped': '',
            'responderBytesDropped': '',
            'qosAppliedInterface': '',
            'qosRuleId': '',
            'userId': 'source_user_id',
            'applicationId': 'app_proto',
            'urlCategory': 'url_category',
            'urlReputation': 'url_reputation',
            'clientApplicationId': 'request_client_application',
            'webApplicationId': '',
            'clientUrl.data': 'request_url',
            'netbios': '',
            'clientApplicationVersion': '',
            'monitorRules1': '',
            'monitorRules2': '',
            'monitorRules3': '',
            'monitorRules4': '',
            'monitorRules5': '',
            'monitorRules6': '',
            'monitorRules7': '',
            'monitorRules8': '',
            'securityIntelligenceSourceDestination': '',
            'securityIntelligenceLayer': '',
            'fileEventCount': '',
            'intrusionEventCount': '',
            'initiatorCountry': 'initiatorCountry',
            'responderCountry': 'responderCountry',
            'originalClientCountry': '',
            'iocNumber': 'iocNumber',
            'sourceAutonomousSystem': 'sourceAutonomousSystem',
            'destinationAutonomousSystem': 'destinationAutonomousSystem',
            'snmpIn': '',
            'snmpOut': '',
            'sourceTos': '',
            'destinationTos': '',
            'sourceMask': '',
            'destinationMask': '',
            'securityContext': '',
            'vlanId': '',
            'referencedHost.data': 'referencedHost',
            'userAgent.data': 'userAgent',
            'httpReferrer': '',
            'sslCertificateFingerprint': '',
            'sslPolicyId': '',
            'sslRuleId': '',
            'sslCipherSuite': '',
            'sslVersion': '',
            'sslServerCertificateStatus': '',
            'sslActualAction': '',
            'sslExpectedAction': '',
            'sslFlowStatus': '',
            'sslFlowError': '',
            'sslFlowMessages': '',
            'sslFlowFlags': '',
            'sslServerName': '',
            'sslUrlCategory': '',
            'sslSessionId': '',
            'sslSessionIdLength': '',
            'sslTicketId': '',
            'sslTicketIdLength': '',
            'networkAnalysisPolicyRevision': '',
            'endpointProfileId': '',
            'securityGroupId': '',
            'locationIpv6': '',
            'httpResponse': 'httpResponse',
            'dnsQuery': '',
            'dnsRecordType': '',
            'dnsResponseType': '',
            'dnsTtl': '',
            'sinkholeUuid': '',
            'securityIntelligenceList1': '',
            'securityIntelligenceList2': ''
        },

        'viewdata': {
            View.SEC_ZONE_INGRESS: 'cs3',
            View.SEC_ZONE_EGRESS: 'cs4',
            View.CORRELATION_POLICY: 'cs1',
            View.FW_RULE_ACTION: 'act',
            View.FW_RULE_REASON: 'reason',
            View.PROTOCOL: 'proto',
            View.SOURCE_USER: 'suser',
            View.APP_PROTO: 'app_proto',
        },
    },

    # 112
    definitions.RECORD_CORRELATION_EVENT: {
        'sig_id': lambda rec: 'PV:112:{0}:{1}'.format(
            rec['ruleId'],
            rec['policyId']
        ),

        'name': lambda rec: 'POLICY VIOLATION',

        'severity': lambda rec: __severity(
            rec['priority'],
            rec['{0}.{1}'.format( View.OUTPUT_KEY,View.IMPACT )] ),

        'constants': {
            'cs1Label': 'policy',
            'cs2Label': 'policyRule',
            'cs3Label': 'ingressZone',
            'cs4Label': 'egressZone',
            'cn1Label': 'vlan',
            'cn2Label': 'impact',
        },

        'lambdas': {
            'src_ip': lambda rec: __ipv4( rec['sourceIpv6Address'] ),
            'dst_ip': lambda rec: __ipv4( rec['destinationIpv6Address'] ),
        },

        'fields': {
            'deviceId': '',
            'correlationEventSecond': '', # Used to generate rt
            'policyId': '',
            'ruleId': '',
            'priority': '', # Used to generate severity
            'eventDescription.data': 'msg',
            'eventType': '',
            'eventDeviceId': '',
            'signatureId': '',
            'signatureGeneratorId': '',
            'triggerEventSecond': '',
            'triggerEventMicrosecond': '',
            'deviceEventId': '',
            'eventDefinedMask': '',
            'eventImpactFlags': '', # Used to generate severity
            'ipProtocol': 'proto',
            'networkProtocol': 'networkProtocol',
            'sourceIp': 'sourceIp',
            'sourceHostType': 'sourceHostType',
            'sourceVlanId': '',
            'sourceOperatingSystemFingerprintUuid': '',
            'sourceCriticality': '',
            'sourceUserId': 'source_user_id',
            'sourcePort': 'src_port',
            'sourceServerId': '',
            'destinationIp': 'destinationIp',
            'destinationHostType': 'destinationHostType',
            'destinationVlanId': '',
            'destinationOperatingSystemFingerprintUuid': '',
            'destinationCriticality': '',
            'destinationUserId': 'dst_user_id',
            'destinationPort': 'dst_port',
            'destinationServerId': '',
            'blocked': 'act',
            'ingressZoneUuid': '',
            'egressZoneUuid': '',
            'sourceIpv6Address': '',
            'destinationIpv6Address': '',
            'sourceCountry': 'sourceCountry',
            'destinationCountry': 'destinationCountry',
            'securityIntelligenceUuid': '',
            'securityContext': '',
            'sslPolicyId': '',
            'sslRuleId': '',
            'sslActualAction': '',
            'sslFlowStatus': '',
            'sslCertificateFingerprint': '',
        },

        'viewdata': {
            View.CORRELATION_POLICY: 'cs1',
            View.BLOCKED: 'act',
            View.PROTOCOL: 'proto',
            View.APP_PROTO: 'app_proto',
            View.SOURCE_USER: 'suser',
            View.DESTINATION_USER: 'dst_user_id',
            View.SEC_ZONE_INGRESS: 'cs3',
            View.SEC_ZONE_EGRESS: 'cs4',
            View.IMPACT: 'cn2',
        },
    },

    # 125
    definitions.RECORD_MALWARE_EVENT: {
        'sig_id': lambda rec: 'FireAMP:125:1',

        'name': lambda rec: 'FireAMP Event',

        'severity': lambda rec: rec['threatScore'] / 10,

        'constants': {
            'cs1Label': 'policy',
            'cs2Label': 'virusName',
            'cs3Label': 'disposition',
            'cs4Label': 'speroDisposition',
            'cs5Label': 'eventDescription',
        },

        'lambdas': {
            'src_ip': lambda rec: __ipv4( rec['sourceIpAddress'] ),
            'dst_ip': lambda rec: __ipv4( rec['destinationIpAddress'] ),
        },

        'viewdata': {
            View.PROTOCOL: 'proto',
            View.SOURCE_USER: '',
            View.APP_PROTO: 'app_proto',
            View.MALWARE_EVENT_TYPE: 'malware_event_type',
            View.FILE_TYPE: 'fileType',
            View.AGENT_USER: 'dst_user_id',
            View.FILE_POLICY: 'cs1',
            View.DISPOSITION: 'cs3',
            View.RETRO_DISPOSITION: 'cs4',
        },

        'fields': {
            'agentUuid': '',
            'cloudUuid': '',
            'malwareEventTimestamp': '', # Used to generate rt
            'eventTypeId': 'malware_event_type',
            'eventSubtypeId': '',
            'detectorId': '',
            'detectionName.data': 'cs2',
            'user.data': 'source_userID',
            'fileName.data': 'fileName',
            'filePath.data': '',
            'fileShaHash.data': '',
            'fileSize': '',
            'fileType': 'fileType',
            'fileTimestamp': '',
            'parentFileName.data': '',
            'parentShaHash': '',
            'eventDescription.data': 'cs5',
            'deviceId': '',
            'connectionInstance': '',
            'connectionCounter': '',
            'connectionEventTimestamp': '', # Used to generate start
            'sourceIpAddress': 'sourceIpAddress',
            'destinationIpAddress': 'destinationIpAddress',
            'applicationId': 'app_proto',
            'userId': 'dst_user_id',
            'accessControlPolicyUuid': '',
            'disposition': 'cs3',
            'retroDisposition': 'cs4',
            'uri.data': 'request',
            'sourcePort': 'src_port',
            'destinationPort': 'dst_port',
            'sourceCountry': 'sourceCountry',
            'destinationCountry': 'destinationCountry',
            'webApplicationId': 'webApplicationId',
            'clientApplicationId': '',
            'action': 'act',
            'protocol': 'proto',
            'threatScore': 'threatScore',
            'iocNumber': '',
            'securityContext': '',
            'sslCertificateFingerprint': '',
            'sslActualAction': '',
            'sslFlowStatus': '',
            'archiveSha': '',
            'archiveName': '',
            'archiveDepth': '',
            'httpResponse': 'httpResponse',
        },
    },

    # 400
    definitions.RECORD_INTRUSION_EVENT: {
        'sig_id': lambda rec: '[{0}:{1}]'.format(
            rec['generatorId'],
            rec['ruleId']
        ),

        'name': lambda rec: rec['@computed.message'],

        'severity': lambda rec: __severity(
            rec['priorityId'],
            rec['impact'] ),

        'constants': {
            'cs1Label': 'fwPolicy',
            'cs2Label': 'fwRule',
            'cs3Label': 'ingressZone',
            'cs4Label': 'egressZone',
            'cs5Label': 'ipsPolicy',
            'cs6Label': 'ruleId',
            'cn1Label': 'vlan',
            'cn2Label': 'impact',
        },

        'lambdas': {
            'src_ip': lambda rec: __ipv4( rec['sourceIpAddress'] ),
            'dst_ip': lambda rec: __ipv4( rec['destinationIpAddress'] ),
            'request': lambda rec: '',
            'act': lambda rec: ['Alerted', 'Blocked', 'Would Be Blocked'][ rec['blocked'] ]
        },

        'viewdata': {
            View.CLASSIFICATION_DESCRIPTION: 'classification',
            View.IP_PROTOCOL: 'proto',
            View.IDS_POLICY: 'cs5',
            View.SOURCE_USER: 'suser',
            View.APP_PROTO: 'app_proto',
            View.FW_POLICY: 'cs1',
            View.SEC_ZONE_INGRESS: 'cs3',
            View.SEC_ZONE_EGRESS: 'cs4',
            View.IMPACT: 'cn2'
        },

        'fields': {
            'deviceId': '',
            'eventId': '',
            'eventSecond': '', # Used to generate rt
            'eventMicrosecond': '',
            'ruleId': 'cs6', # Used to generate sig_id
            'generatorId': '', # Used to generate sig_id
            'ruleRevision': '',
            'classificationId': '',
            'priorityId': '', # Used to generate severity
            'sourceIpAddress': 'sourceIpAddress',
            'destinationIpAddress': 'destinationIpAddress',
            'sourcePortOrIcmpType': 'src_port',
            'destinationPortOrIcmpType': 'dst_port',
            'ipProtocolId': 'proto',
            'impactFlags': '',
            'impact': '', # Used to generate severity
            'blocked': 'act',
            'mplsLabel': 'mplsLabel',
            'vlanId': 'cn1',
            'pad': '',
            'policyUuid': '',
            'userId': 'source_userID',
            'webApplicationId': 'webApplicationId',
            'clientApplicationId': '',
            'applicationId': 'app_proto',
            'accessControlRuleId': 'cs2',
            'accessControlPolicyUuid': 'cs1',
            'interfaceIngressUuid': '',
            'interfaceEgressUuid': '',
            'securityZoneIngressUuid': 'cs3',
            'securityZoneEgressUuid': 'cs4',
            'connectionTimestamp': '', # Used to generate start
            'connectionInstanceId': '',
            'connectionCounter': '',
            'sourceCountry': 'sourceCountry',
            'destinationCountry': 'destinationCountry',
            'iocNumber': '',
            'securityContext': '',
            'sslCertificateFingerprint': '',
            'sslActualAction': '',
            'sslFlowStatus': '',
            'networkAnalysisPolicyUuid': '',
            'httpResponse': 'httpResponse',
        },
    },

    # 500 (and also 502 - it's copied below)
    definitions.RECORD_FILELOG_EVENT: {
        'sig_id': lambda rec: 'File:500:1',

        'name': lambda rec: '{0}'.format( rec['@computed.recordTypeDescription'] ),

        'severity': lambda rec: rec['threatScore'] / 10,

        'constants': {
            'cs1Label': 'filePolicy',
            'cs2Label': 'disposition',
            'cs3Label': 'speroDisposition',
            'cs4Label': 'virusName',
        },

        'lambdas': {
            'src_ip': lambda rec: __ipv4( rec['sourceIpAddress'] ),
            'dst_ip': lambda rec: __ipv4( rec['destinationIpAddress'] ),
        },

        'viewdata': {
            View.DISPOSITION: 'cs2',
            View.SPERO_DISPOSITION: 'cs3',
            View.FILE_ACTION: 'act',
            View.FILE_TYPE: 'fileType',
            View.APP_PROTO: 'app_proto',
            View.SOURCE_USER: 'suser',
            View.PROTOCOL: 'proto',
            View.FILE_POLICY: 'cs1',
        },

        'fields': {
            'deviceId': '',
            'connectionInstance': '',
            'connectionCounter': '',
            'connectionTimestamp': '', # Used to generate start
            'fileEventTimestamp': '', # Used to generate rt
            'sourceIpAddress': 'sourceIpAddress',
            'destinationIpAddress': 'destinationIpAddress',
            'disposition': 'cs2',
            'speroDisposition': 'cs3',
            'fileStorageStatus': '',
            'fileAnalysisStatus': '',
            'localMalwareAnalysisStatus': '',
            'archiveFileStatus': '',
            'threatScore': '', # Used to generate severity
            'action': 'act',
            'fileTypeId': '',
            'fileName.data': 'fileName',
            'fileSize': '',
            'applicationId': 'app_proto',
            'userId': 'source_userID',
            'uri.data': 'request',
            'signature.data': '',
            'sourcePort': 'src_port',
            'destinationPort': 'dst_port',
            'protocol': 'proto',
            'accessControlPolicyUuid': '',
            'sourceCountry': 'sourceCountry',
            'destinationCountry': 'destinationCountry',
            'webApplicationId': 'webApplicationId',
            'clientApplicationId': '',
            'securityContext': '',
            'sslCertificateFingerprint': '',
            'sslActualAction': '',
            'sslFlowStatus': '',
            'archiveSha': '',
            'archiveName': '',
            'archiveDepth': '',
        },
    },
}

# 502
MAPPING[ definitions.RECORD_FILELOG_MALWARE_EVENT ] = copy.deepcopy(
    MAPPING[ definitions.RECORD_FILELOG_EVENT ])

MAPPING[ definitions.RECORD_FILELOG_MALWARE_EVENT ]['sig_id'] = lambda rec: 'FileMalware:502:1'


hostname = socket.gethostname()
        

def sanitize(value):
    value = str( value )

    # Remove null, returns and linefeeds
    value = value.replace('\0', '')
    value = value.replace('\r', '')
    value = value.replace('\n', '')

    # Escape "=" and "\" with \
    value = value.replace('\\', '\\\\')
    value = value.replace('=', '\\=')

    return value


def convert(record):

    rec_type = record['recordType']
    if not rec_type in MAPPING:
        return {
            'MESSAGE': "Unknown recordType {}".format(rec_type),
            'user_tags': {'record_type': str(rec_type)},
        }

    mapping = MAPPING[rec_type]
    output = {'recordType': str(rec_type)}

    # Do the fields first (mapping)
    for source in mapping['fields']:
        target = mapping['fields'][source]
        if len(target) > 0:
            output[target] = record[source]

    # Now the constants (hard coded values)
    for target in mapping['constants']:
        output[target] = mapping['constants'][target]

    # Lambdas
    for target in mapping['lambdas']:
        function = mapping['lambdas'][target]
        output[target] = function(record)

    # View data last
    for source in mapping['viewdata']:
        key = '{0}.{1}'.format( View.OUTPUT_KEY, source )
        if key in record:
            value = record[key]
            if value is not None:
                target = mapping['viewdata'][source]
                output[target] = value

    keys = output.keys()
    for key in keys:
        if isinstance(output[key], basestring):
            if len(output[key]) == 0:
                del output[key]

        elif output[key] == 0:
            del output[key]

        else:
            output[key] = sanitize(output[key])

    return output

def un_camel_case(k):
#    k = str(k)
    if not isinstance(k, basestring):
        raise Exception("Wrong k: {} (type={})".format(k, type(k)))
    k = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', k)
    k = re.sub('([a-z0-9])([A-Z])', r'\1_\2', k)
    return k.lower()

def dumps( source ):
    
    record = estreamer.common.Flatdict( source )
    record = convert(record)

    now = time.time()

    data = {
        'TS': str(now),
        'HOST': hostname,
        'PRI': 13,
        'PROGRAM': 'eStreamer',
    }

    for k in ('HOST', 'PRI', 'PROGRAM', 'MESSAGE', 'user_tags', 'status'):
        if k in record:
            data[k] = record.pop(k)

    record_lc = dict()
    for k, v in record.iteritems():
        record_lc[un_camel_case(k)] = v


    if 'MESSAGE' not in data:
        data['MESSAGE'] = estreamer.adapters.kvpair.dumps(
            record_lc,
            delimiter=' ',
            quoteSpaces=False,
            sort=True,
        )

    if 'user_tags' not in data:
        data['user_tags'] = record_lc

    return json.dumps(data)

# Cisco Duplex Mismatch Auto-Remediation
- Match on `CDP-4-DUPLEX_MISMATCH`
- SSH to device and check for `duplex half`
- If exists, fix it!
- Reports results to Slack channel

# Script Type: Perl

**Required Modules**

    File::Sync
    Net::DNS::Resolver
    JSON
    HTTP::Request::Common
    LWP::UserAgent
    LWP::Protocol::https

# Script Variables
You will need to obtain your webhook URL from the slack admin interface

Once you have that, modify the script and set the correct webhook url:

    my $posturl = https://hooks.slack.com/services/STRING/STRING/STRING';

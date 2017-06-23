# Cisco Trunk Port
- Match on `DTP-5-NONTRUNKPORTON` or `DTP-5-TRUNKPORTON`
- SSH to device and check trunk status
- Gather more information such as `show interface`
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

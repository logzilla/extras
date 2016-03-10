# lz2Slack
The lz2slack script will allow matched triggers to be sent to a slack.com channel.

# Script type: Perl
# Required modules:
```File::Sync
Net::DNS::Resolver
JSON
HTTP::Request::Common
LWP::UserAgent
LWP::Protocol::https```

# Script variables
You will need to obtain your webhook URL from the slack admin interface

Once you have that, modify the lz5slack.pl script and set the correct webhook url:

```my $posturl = https://hooks.slack.com/services/STRING/STRING/STRING';```

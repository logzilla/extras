#!/usr/bin/env perl
#-----------------------------------------------------------------------------
# Required modules (install using cpanminus):
# cpanm File::Sync Net::DNS::Resolver JSON HTTP::Request::Common LWP::UserAgent LWP::Protocol::https
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# LogZilla (>5.70.3) passes the following environment variables:
#-----------------------------------------------------------------------------
# EVENT_CISCO_MNEMONIC          =   <string>
# EVENT_COUNTER                 =   <integer>
# EVENT_FACILITY                =   <integer>
# EVENT_FIRST_OCCURRENCE        =   <float>
# EVENT_HOST                    =   <string>
# EVENT_ID                      =   <int>
# EVENT_LAST_OCCURRENCE         =   <float>
# EVENT_MESSAGE                 =   <string>
# EVENT_PROGRAM                 =   <string>
# EVENT_SEVERITY                =   <integer>
# EVENT_STATUS                  =   <integer>
# EVENT_TRIGGER_AUTHOR          =   <string>
# EVENT_TRIGGER_AUTHOR_EMAIL    =   <string>
# EVENT_TRIGGER_ID              =   <integer>
# EVENT_USER_TAGS               =   <integer>
# TRIGGER_HITS_COUNT            =   <integer>

#-----------------------------------------------------------------------------
# Change these values to match your setup:
#-----------------------------------------------------------------------------
# You will need to obtain your webhook URL from the slack admin interface
my $posturl = 'https://hooks.slack.com/services/STRING/STRING/STRING';
my $default_channel = '#rawr';
my $slack_user = 'logzilla-bot';

use strict;

use HTTP::Request::Common qw(POST);
use LWP::UserAgent;
use JSON;
use Getopt::Std;

=pod
=head1 lz2slack

Post a LogZila notificaion message to a channel in a slack.com group using the
"Incoming Webhooks" integration.

To configure Slack, you need to enable the Incoming Webhooks
integration. The posturl variable at the top of this script needs to
be set to the token from that integration

=cut


my $msg;
foreach my $key (sort keys(%ENV)) {
  next if $key !~ /^EVENT|^TRIGGER/;
  $msg .= "$key = $ENV{$key}\n";
}
# Testing - output to file to see if the event is being passed
#my $filename = "/tmp/test.txt";
#open(my $fh, '>>', $filename) or die "Could not open file '$filename' $!";
#print $fh "$msg\n";
#close $fh;

my $payload = {
  channel => $default_channel,
  username => $slack_user,
};

$payload->{text} .= ' :crocodile:';
$payload->{attachments} = [
  {
    fallback => "LogZilla Notification: $ENV{'EVENT_MESSAGE'}",
    color => '#D3D3D3',
    fields => [
      {
        title => 'Host',
        value => "$ENV{'EVENT_HOST'}",
        short => 'false',
      },
      # You can also do other fun things...
      {
      title => 'Severity',
      value => "$ENV{'EVENT_SEVERITY'}",
      short => 'false',
      },
      {
      title => 'Facility',
      value => "$ENV{'EVENT_FACILITY'}",
      short => 'false',
      },
      {
      title => 'Program',
      value => "$ENV{'EVENT_PROGRAM'}",
      short => 'false',
      },
      {
      title => 'Count',
      value => "$ENV{'EVENT_COUNTER'}",
      short => 'false',
      },
      {
        title => 'Message',
        value => "$ENV{'EVENT_MESSAGE'}",
        short => 'false',
      },
    ],
  },
];

my $ua = LWP::UserAgent->new;
$ua->timeout(15);

my $req = POST("${posturl}", ['payload' => encode_json($payload)]);


my $resp = $ua->request($req);

if ($resp->is_success) {
  #print $resp->decoded_content;
}
else {
  die $resp->status_line;
}
exit(0);

=pod

=head1 COPYRIGHT

Copyright LogZilla Corporation

=cut

#!/usr/bin/env perl

#-----------------------------------------------------------------------------
# Required OS package (Ubuntu):
# apt install libnet-ssh2-perl
# 
# Required modules (install using cpanminus):
# Net::SSH2::Cisco
# cpan install:
# (from package cpanminus)
# cpanm Net::SSH2::Cisco HTTP::Request::Common LWP::UserAgent JSON
#-----------------------------------------------------------------------------

use Net::SSH2::Cisco;
use HTTP::Request::Common qw(POST);
use LWP::UserAgent;
use JSON;

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
# Testing - disable in production
my $debug = 0;
# Use the section below to output to file for debugging/testing
my $msg;
# Make sure you create the file and chmod 666 first.
my $filename = "/tmp/test.txt";

foreach my $key (sort keys(%ENV)) {
    next if $key !~ /^EVENT|^TRIGGER/;
    $msg .= "$key = $ENV{$key}\n";
}
open(my $fh, '>>', $filename) or die "Could not open file '$filename' $!" if $debug > 0;
print $fh "$msg\n" if $debug > 0;
# End Test
#-----------------------------------------------------------------------------

my $ciscoUsername = "username";
my $ciscoPassword = "password";
printf $fh "Connecting to %s\n", $ENV{'EVENT_HOST'} if $debug > 0;
my $session = Net::SSH2::Cisco->new(host => $ENV{'EVENT_HOST'});
$session->login($ciscoUsername, $ciscoPassword);

# Execute a command
my @output = $session->cmd('show version');
# Below just sends to the /tmp/test.txt file as an example.
print $fh @output if $debug > 0;


#-----------------------------------------------------------------------------
# Let's get that show ver result and put in in a slack channel, shall we?
my $posturl = 'https://hooks.slack.com/services/AAAAAAA/AAAAAAA/CCCC';
my $default_channel = '#vmware-test';
my $slack_user = 'logzilla-bot';

my $payload = {
    username => $slack_user,
    channel => $default_channel,
    icon_url => "http://www.logzilla.net/images/logo_orange_png_cropped_40x40.png",
    attachments => [{
            fallback => "Cisco SSH test",
            title => 'Cisco SSH test',
            color => '#9C1A22',
            pretext => "Incoming Results from SSH Test",
            author_name => "$ENV{'EVENT_TRIGGER_AUTHOR'}",
            author_link => "mailto:$ENV{'EVENT_TRIGGER_AUTHOR_EMAIL'}",
            author_icon => "http://www.logzilla.net/images/log_file_icon_25x25.png",
            fields => [{
                    title => 'Program',
                    value => "$ENV{'EVENT_PROGRAM'}",
                    short => 'true',
                }, {
                    title => 'Severity',
                    value => "$ENV{'EVENT_SEVERITY'}",
                    short => 'true',
                }],
            mrkdwn_in => ["text", "fields"],
            text => "```@output```",
            thumb_url => "http://www.logzilla.net/images/icon_warning_25x25.png",
        }]
};

my $ua = LWP::UserAgent->new;
$ua->timeout(15);

my $req = POST("${posturl}", ['payload' => encode_json($payload)]);
print $fh encode_json($payload) if $debug > 0;


my $resp = $ua->request($req);

if ($resp->is_success) {
    #print $resp->decoded_content;
}
else {
    die $resp->status_line;
}

close $fh if $debug > 0;
$session->close;

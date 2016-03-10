#!/usr/bin/env perl
#-----------------------------------------------------------------------------
# Required modules (install using cpanminus):
# cpanm Net::SNMP
#-----------------------------------------------------------------------------
#-----------------------------------------------------------------------------
# Change these values to match your setup:
#-----------------------------------------------------------------------------
my @trapdests = 'my.server1.com, my.server2.com, 1.2.3.4';
my $port = 161;
my @communities = 'public, private';

#-----------------------------------------------------------------------------
# Currently, LogZilla passes the following:
#-----------------------------------------------------------------------------
# EVENT_COUNTER=<integer>
# EVENT_CISCO_MNEMONIC=<string>
# EVENT_STATUS=<integer>
# EVENT_ID=<integer>
# EVENT_SEVERITY=<integer>
# EVENT_FACILITY=<integer>
# EVENT_HOST=<string>
# EVENT_FIRST_OCCURRENCE=<float>
# EVENT_SNARE_ID=<integer>
# EVENT_PROGRAM=<string>
# EVENT_LAST_OCCURRENCE=<float>
# EVENT_MESSAGE=<string>
# TRIGGER_ID=<integer>

use strict;
use warnings;

use Net::SNMP qw(SNMP_TRAP_PORT OCTET_STRING);
use Getopt::Std;

=pod
=head1 snmpTRAP

Generate an SNMP TRAP to an external SNMP receiver

=cut

my $msg;
foreach my $key (sort keys(%ENV)) {
next if $key !~ /^EVENT|^TRIGGER/;
$msg .= "$key = $ENV{$key} | ";
}
# Testing - output to file to see if the event is being passed
#my $filename = "/tmp/test.txt";
#open(my $fh, '>>', $filename) or die "Could not open file '$filename' $!";
#print $fh "$msg\n";
#close $fh;

# SNMP Forwarding
sub trap {
my( $msg ) = @_;
foreach my $dest (@trapdests) {
foreach my $community (@communities) {
my ( $session, $error ) = Net::SNMP->session(
-hostname => $dest,
-community => $community,
-port => SNMP_TRAP_PORT,
);

if ( !defined($session) ) {
printf ( "SNMP ERROR: %s.", $error );
exit(1);
}

#Note, these are genric OIDs, if you have registered OIDs, feel free to modify
my $result = $session->trap(
-enterprise => '1.3.6.1.4.1.31337',
-generictrap => 6,
-specifictrap => 1,
-varbindlist => [
'1.3.6.1.4.1.31337.1.1', OCTET_STRING, "Trap FWD from LogZilla: $msg"
]
);
if ( !$result ) {
printf ( "SNMP ERROR for $dest/$community: $session->error" );
}
$session->close();
}
}
}
trap( $msg );
exit(0);

=pod

=head1 COPYRIGHT

Copyright LogZilla Corporation

=cut

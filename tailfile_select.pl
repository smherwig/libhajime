#!/usr/bin/env perl

use strict;
use warnings;

use IO::Handle;
use IO::Select;

$|++;   # autoflush stdout

# ./tailfile_select.pl <kadnode-logfile>
# for each IP PORT of a bot in the logfile, uses './hajime_getpeerpubkey
# to try to connect to the bot and perform a key exchange.
#
# Outputs the kadnode-logfile with two extra fields appended to each line:
#
#   <bots_pubkey | '*> <UTC UNIX Epoch time when tried to retrieve key>
#
# Here, the '*' (star) symbol means that we failed to retrieve the bot's public key.
open (my $fh, "-|", "tail -F -n +0 $ARGV[0]") or die "cannot tail -f $ARGV[0]: $!";

my $pool_size = $ARGV[1] || 16;

my $sel = IO::Select->new();
my %fd2line;

sub select_loop {

    my @ready;

    while (@ready = $sel->can_read(0.1)) {
        foreach my $ph (@ready) {
            my @lines;
            while (defined($_ = $ph->getline)) {
                chomp;
                push @lines, $_;
            }

            my $t = time;
            my $fd = fileno $ph;

            if (scalar @lines) {
                if ($lines[0] =~ /pubkey=(\w{64})\s*$/) {
                    printf "%s %s %d\n", $fd2line{$fd}, $1, $t;
                } else {
                    printf "%s * %d\n", $fd2line{$fd}, $t;
                }
            } else {
                printf "%s * %d\n", $fd2line{$fd}, $t;
            }

            #print "<- fd=$fd\n";
            delete $fd2line{$fd};
            $sel->remove($ph);
            $ph->close;
        }
    }
}


while (<$fh>) {
    my $log_line = $_;
    chomp $log_line;

    # limit ourselves to just 16 pipes open at a time
    while ($sel->count > $pool_size) {
        select_loop();            
    }

    if ($log_line =~ /\s+(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\s+(\d+)\s*/) {
        my $ip = $1;
        my $port = $2;
        open(my $ph, '-|', "timeout 5 hajime_getpeerpubkey -t 3 $ip $port") or die "can't create pipe: $!";

        my $fd = fileno $ph;
        #print "-> fd=$fd\n";
        $fd2line{$fd} = $log_line;
        $sel->add($ph);
    }

    select_loop();
}

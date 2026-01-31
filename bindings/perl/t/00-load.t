#!/usr/bin/env perl
#
# 00-load.t - Test module loading and basic sanity
#

use strict;
use warnings;
use Test::More tests => 8;

# Test 1: Module loads
BEGIN {
    use_ok('Net::LPM', qw(LPM_INVALID_NEXT_HOP)) or BAIL_OUT("Cannot load Net::LPM");
}

# Test 2: Module version is defined
ok(defined $Net::LPM::VERSION, 'Module version is defined');
like($Net::LPM::VERSION, qr/^\d+\.\d+\.\d+$/, 'Version format is X.Y.Z');

# Test 3: C library version
my $c_version = Net::LPM->version();
ok(defined $c_version, 'C library version is defined');
like($c_version, qr/\d+\.\d+/, 'C library version looks valid');
diag("C library version: $c_version");

# Test 4: Constants
ok(defined &LPM_INVALID_NEXT_HOP, 'LPM_INVALID_NEXT_HOP constant is defined');
is(LPM_INVALID_NEXT_HOP(), 0xFFFFFFFF, 'LPM_INVALID_NEXT_HOP has expected value');

# Test 5: Can create objects
can_ok('Net::LPM', qw(new_ipv4 new_ipv6 insert delete lookup lookup_batch version));

diag("Module loaded successfully");

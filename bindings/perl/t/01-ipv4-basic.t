#!/usr/bin/env perl
#
# 01-ipv4-basic.t - Test IPv4 basic operations
#

use strict;
use warnings;
use Test::More tests => 32;

use Net::LPM;

# Test table creation
my $table = Net::LPM->new_ipv4();
ok(defined $table, 'IPv4 table created');
isa_ok($table, 'Net::LPM');
ok($table->is_ipv4, 'Table is IPv4');
ok(!$table->is_ipv6, 'Table is not IPv6');

# Test basic insert and lookup
ok($table->insert("192.168.0.0/16", 100), 'Insert 192.168.0.0/16');
is($table->lookup("192.168.1.1"), 100, 'Lookup 192.168.1.1 returns 100');
is($table->lookup("192.168.255.255"), 100, 'Lookup 192.168.255.255 returns 100');
is($table->lookup("192.169.0.1"), undef, 'Lookup 192.169.0.1 returns undef');

# Test multiple prefixes
ok($table->insert("10.0.0.0/8", 200), 'Insert 10.0.0.0/8');
ok($table->insert("172.16.0.0/12", 300), 'Insert 172.16.0.0/12');

is($table->lookup("10.1.2.3"), 200, 'Lookup in 10.0.0.0/8');
is($table->lookup("172.20.0.1"), 300, 'Lookup in 172.16.0.0/12');
is($table->lookup("8.8.8.8"), undef, 'No match for 8.8.8.8');

# Test longest prefix match
ok($table->insert("192.168.1.0/24", 101), 'Insert more specific prefix');
is($table->lookup("192.168.1.1"), 101, 'Longest prefix match returns 101');
is($table->lookup("192.168.2.1"), 100, 'Less specific match returns 100');

# Test even more specific prefix
ok($table->insert("192.168.1.128/25", 102), 'Insert /25 prefix');
is($table->lookup("192.168.1.129"), 102, 'Lookup in /25 returns 102');
is($table->lookup("192.168.1.1"), 101, 'Lookup outside /25 returns 101');

# Test default route
ok($table->insert("0.0.0.0/0", 999), 'Insert default route');
is($table->lookup("8.8.8.8"), 999, 'Previously no-match now hits default');

# Test host routes (/32)
ok($table->insert("1.2.3.4/32", 444), 'Insert host route /32');
is($table->lookup("1.2.3.4"), 444, 'Host route lookup works');
is($table->lookup("1.2.3.5"), 999, 'Adjacent IP uses default');

# Test prefix deletion
ok($table->delete("192.168.1.128/25"), 'Delete /25 prefix');
is($table->lookup("192.168.1.129"), 101, 'After delete, falls back to /24');

# Test delete non-existent
ok(!$table->delete("203.0.113.0/24"), 'Delete non-existent returns false');

# Test boundary addresses
ok($table->insert("255.255.255.0/24", 555), 'Insert high address prefix');
is($table->lookup("255.255.255.255"), 555, 'Lookup 255.255.255.255');

# Test zero next hop
ok($table->insert("100.0.0.0/8", 0), 'Insert with next_hop=0');
is($table->lookup("100.1.1.1"), 0, 'Lookup returns 0 (not undef)');

# Clean up
undef $table;
pass('Table destroyed cleanly');

diag("IPv4 basic operations tests completed");

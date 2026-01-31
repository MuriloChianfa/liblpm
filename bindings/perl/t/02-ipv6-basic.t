#!/usr/bin/env perl
#
# 02-ipv6-basic.t - Test IPv6 basic operations
#

use strict;
use warnings;
use Test::More tests => 28;

use Net::LPM;

# Test table creation
my $table = Net::LPM->new_ipv6();
ok(defined $table, 'IPv6 table created');
isa_ok($table, 'Net::LPM');
ok($table->is_ipv6, 'Table is IPv6');
ok(!$table->is_ipv4, 'Table is not IPv4');

# Test basic insert and lookup
ok($table->insert("2001:db8::/32", 100), 'Insert 2001:db8::/32');
is($table->lookup("2001:db8::1"), 100, 'Lookup 2001:db8::1 returns 100');
is($table->lookup("2001:db8:ffff::1"), 100, 'Lookup 2001:db8:ffff::1 returns 100');
is($table->lookup("2001:db9::1"), undef, 'Lookup 2001:db9::1 returns undef');

# Test multiple prefixes
ok($table->insert("2001:db8:1::/48", 200), 'Insert 2001:db8:1::/48');
ok($table->insert("2001:db8:2::/48", 300), 'Insert 2001:db8:2::/48');

is($table->lookup("2001:db8:1::1"), 200, 'Lookup in 2001:db8:1::/48');
is($table->lookup("2001:db8:2::ffff"), 300, 'Lookup in 2001:db8:2::/48');

# Test longest prefix match
ok($table->insert("2001:db8:1:1::/64", 201), 'Insert more specific /64');
is($table->lookup("2001:db8:1:1::1"), 201, 'Longest prefix match returns 201');
is($table->lookup("2001:db8:1:2::1"), 200, 'Less specific match returns 200');

# Test default route
ok($table->insert("::/0", 999), 'Insert default route');
is($table->lookup("2607:f8b0:4004:800::200e"), 999, 'Google DNS hits default route');

# Test link-local range
ok($table->insert("fe80::/10", 500), 'Insert link-local prefix');
is($table->lookup("fe80::1"), 500, 'Link-local lookup works');

# Test host route (/128)
ok($table->insert("2001:db8:1:1::dead:beef/128", 666), 'Insert host route /128');
is($table->lookup("2001:db8:1:1::dead:beef"), 666, 'Host route lookup works');
is($table->lookup("2001:db8:1:1::dead:beee"), 201, 'Adjacent IP uses /64');

# Test delete
ok($table->delete("2001:db8:1::/48"), 'Delete /48 prefix');
is($table->lookup("2001:db8:1:2::1"), 100, 'After delete, falls back to /32');

# Test delete non-existent (note: C library returns success for this)
ok($table->delete("2001:db9::/32"), 'Delete non-existent completes without error');

# Test zero next hop
ok($table->insert("fc00::/7", 0), 'Insert with next_hop=0');
is($table->lookup("fd00::1"), 0, 'Lookup returns 0 (not undef)');

# Clean up
undef $table;
pass('Table destroyed cleanly');

diag("IPv6 basic operations tests completed");

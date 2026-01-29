#!/usr/bin/env perl
#
# 04-memory.t - Test memory management
#

use strict;
use warnings;
use Test::More tests => 15;

use Net::LPM;

# Test multiple table creation/destruction
{
    my @tables;
    for my $i (1..5) {
        my $t = Net::LPM->new_ipv4();
        ok(defined $t, "Create table $i");
        $t->insert("10.$i.0.0/16", $i * 100);
        push @tables, $t;
    }
    
    # Verify all tables work
    for my $i (1..5) {
        is($tables[$i-1]->lookup("10.$i.1.1"), $i * 100, "Table $i lookup works");
    }
    
    # Tables go out of scope here
}
pass('Multiple tables destroyed cleanly');

# Test explicit destruction
{
    my $table = Net::LPM->new_ipv4();
    $table->insert("192.168.0.0/16", 100);
    is($table->lookup("192.168.1.1"), 100, 'Lookup before undef');
    
    undef $table;
    pass('Explicit undef works');
}

# Test table reuse (same variable)
{
    my $table = Net::LPM->new_ipv4();
    $table->insert("192.168.0.0/16", 100);
    
    # Replace with new table
    $table = Net::LPM->new_ipv6();
    ok($table->is_ipv6, 'Replaced table is IPv6');
    
    $table->insert("2001:db8::/32", 200);
    is($table->lookup("2001:db8::1"), 200, 'New table works');
}

diag("Memory management tests completed");
diag("For leak detection, run: make valgrind");

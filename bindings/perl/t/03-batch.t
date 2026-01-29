#!/usr/bin/env perl
#
# 03-batch.t - Test batch lookup operations
#

use strict;
use warnings;
use Test::More tests => 20;

use Net::LPM;

# IPv4 batch tests
{
    my $table = Net::LPM->new_ipv4();
    
    # Setup routes
    $table->insert("192.168.0.0/16", 100);
    $table->insert("10.0.0.0/8", 200);
    $table->insert("172.16.0.0/12", 300);
    $table->insert("0.0.0.0/0", 999);
    
    # Test basic batch lookup
    my @addrs = qw(192.168.1.1 10.1.1.1 172.20.0.1 8.8.8.8);
    my @results = $table->lookup_batch(\@addrs);
    
    is(scalar @results, 4, 'Batch returns correct number of results');
    is($results[0], 100, 'First result correct');
    is($results[1], 200, 'Second result correct');
    is($results[2], 300, 'Third result correct');
    is($results[3], 999, 'Fourth result (default) correct');
    
    # Test empty batch
    my @empty = $table->lookup_batch([]);
    is(scalar @empty, 0, 'Empty batch returns empty list');
    
    # Test single-element batch
    my @single = $table->lookup_batch(["192.168.1.1"]);
    is(scalar @single, 1, 'Single-element batch returns one result');
    is($single[0], 100, 'Single-element result correct');
    
    # Test larger batch
    my @large_addrs;
    for my $i (0..99) {
        push @large_addrs, "192.168.1.$i";
    }
    my @large_results = $table->lookup_batch(\@large_addrs);
    is(scalar @large_results, 100, 'Large batch returns 100 results');
    is($large_results[0], 100, 'Large batch first result correct');
    is($large_results[99], 100, 'Large batch last result correct');
    
    undef $table;
}

# IPv6 batch tests
{
    my $table = Net::LPM->new_ipv6();
    
    # Setup routes
    $table->insert("2001:db8::/32", 100);
    $table->insert("2001:db8:1::/48", 200);
    $table->insert("fe80::/10", 300);
    $table->insert("::/0", 999);
    
    # Test basic batch lookup
    my @addrs = qw(2001:db8::1 2001:db8:1::1 fe80::1 2607:f8b0::1);
    my @results = $table->lookup_batch(\@addrs);
    
    is(scalar @results, 4, 'IPv6 batch returns correct number of results');
    is($results[0], 100, 'IPv6 first result correct');
    is($results[1], 200, 'IPv6 second result (more specific) correct');
    is($results[2], 300, 'IPv6 third result correct');
    is($results[3], 999, 'IPv6 fourth result (default) correct');
    
    # Test batch with no matches (before default route)
    $table->delete("::/0");
    my @addrs2 = qw(2607:f8b0::1 2607:f8b0::2);
    my @results2 = $table->lookup_batch(\@addrs2);
    is(scalar @results2, 2, 'Batch with no matches returns correct count');
    is($results2[0], undef, 'No match returns undef');
    is($results2[1], undef, 'No match returns undef');
    
    undef $table;
}

pass('Batch tests completed');

diag("Batch lookup tests completed");

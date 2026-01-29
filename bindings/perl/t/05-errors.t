#!/usr/bin/env perl
#
# 05-errors.t - Test error handling
#

use strict;
use warnings;
use Test::More tests => 18;

use Net::LPM;

# Test invalid prefix format
{
    my $table = Net::LPM->new_ipv4();
    
    eval { $table->insert("invalid", 100); };
    like($@, qr/Invalid prefix/, 'Invalid prefix format rejected');
    
    eval { $table->insert("192.168.0.0", 100); };
    like($@, qr/Invalid prefix/, 'Missing /prefix_len rejected');
    
    eval { $table->insert("192.168.0.0/", 100); };
    like($@, qr/Invalid prefix/, 'Empty prefix length rejected');
    
    eval { $table->insert("192.168.0.0/33", 100); };
    like($@, qr/Invalid prefix/, 'Prefix length > 32 rejected for IPv4');
    
    eval { $table->insert("192.168.0.0/-1", 100); };
    like($@, qr/Invalid prefix/, 'Negative prefix length rejected');
    
    undef $table;
}

# Test invalid address format
{
    my $table = Net::LPM->new_ipv4();
    $table->insert("0.0.0.0/0", 999);
    
    eval { $table->lookup("invalid"); };
    like($@, qr/Invalid.*address/, 'Invalid address format rejected');
    
    eval { $table->lookup("256.256.256.256"); };
    like($@, qr/Invalid.*address/, 'Invalid IP octets rejected');
    
    eval { $table->lookup("2001:db8::1"); };
    like($@, qr/Invalid.*address/, 'IPv6 address in IPv4 table rejected');
    
    undef $table;
}

# Test IPv6 errors
{
    my $table = Net::LPM->new_ipv6();
    
    eval { $table->insert("2001:db8::/129", 100); };
    like($@, qr/Invalid prefix/, 'Prefix length > 128 rejected for IPv6');
    
    eval { $table->lookup("192.168.1.1"); };
    like($@, qr/Invalid.*address/, 'IPv4 address in IPv6 table rejected');
    
    eval { $table->lookup("invalid::address"); };
    like($@, qr/Invalid.*address/, 'Invalid IPv6 address rejected');
    
    undef $table;
}

# Test missing arguments
{
    my $table = Net::LPM->new_ipv4();
    
    eval { $table->insert(undef, 100); };
    like($@, qr/Missing prefix/, 'Missing prefix argument detected');
    
    eval { $table->insert("192.168.0.0/16"); };
    like($@, qr/Missing next_hop/, 'Missing next_hop argument detected');
    
    eval { $table->delete(); };
    like($@, qr/Missing prefix/, 'Missing delete prefix detected');
    
    eval { $table->lookup(); };
    like($@, qr/Missing address/, 'Missing lookup address detected');
    
    undef $table;
}

# Test invalid next_hop values
{
    my $table = Net::LPM->new_ipv4();
    
    eval { $table->insert("192.168.0.0/16", -1); };
    like($@, qr/non-negative|integer/, 'Negative next_hop rejected');
    
    eval { $table->insert("192.168.0.0/16", 1.5); };
    like($@, qr/integer/, 'Float next_hop rejected');
    
    undef $table;
}

# Test batch errors
{
    my $table = Net::LPM->new_ipv4();
    $table->insert("0.0.0.0/0", 999);
    
    eval { $table->lookup_batch("not_an_array"); };
    like($@, qr/array reference/, 'Non-array-ref to lookup_batch rejected');
    
    undef $table;
}

diag("Error handling tests completed");

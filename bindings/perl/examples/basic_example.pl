#!/usr/bin/env perl
#
# basic_example.pl - Demonstrates Net::LPM usage for IPv4 and IPv6
#
# This example shows how to:
# - Create IPv4 and IPv6 routing tables
# - Insert routes with different prefix lengths
# - Perform single and batch lookups
# - Handle longest prefix matching
# - Delete routes
#
# Usage: perl basic_example.pl
#

use strict;
use warnings;
use FindBin qw($Bin);
use lib "$Bin/../lib", "$Bin/../blib/lib", "$Bin/../blib/arch";

use Net::LPM;

print "=" x 60, "\n";
print "Net::LPM Basic Example\n";
print "liblpm version: ", Net::LPM->version(), "\n";
print "=" x 60, "\n\n";

# ============================================================================
# IPv4 Example
# ============================================================================

print "IPv4 Routing Table Example\n";
print "-" x 40, "\n\n";

# Create IPv4 routing table
my $ipv4_table = Net::LPM->new_ipv4();

# Add some routes
print "Adding routes:\n";
my @ipv4_routes = (
    ["0.0.0.0/0",       999, "Default route"],
    ["10.0.0.0/8",      100, "Private Class A"],
    ["10.1.0.0/16",     101, "More specific /16"],
    ["10.1.1.0/24",     102, "Even more specific /24"],
    ["192.168.0.0/16",  200, "Private Class C"],
    ["172.16.0.0/12",   300, "Private Class B"],
    ["8.8.8.0/24",      400, "Google DNS subnet"],
);

for my $route (@ipv4_routes) {
    my ($prefix, $nh, $desc) = @$route;
    $ipv4_table->insert($prefix, $nh);
    printf "  %-18s -> %3d  (%s)\n", $prefix, $nh, $desc;
}
print "\n";

# Single lookups demonstrating longest prefix match
print "Single lookups (demonstrating longest prefix match):\n";
my @ipv4_lookups = (
    ["10.1.1.5",      "Matches /24 (102)"],
    ["10.1.2.5",      "Matches /16 (101)"],
    ["10.2.3.4",      "Matches /8 (100)"],
    ["192.168.1.1",   "Matches /16 (200)"],
    ["172.20.0.1",    "Matches /12 (300)"],
    ["8.8.8.8",       "Google DNS (400)"],
    ["1.1.1.1",       "Default route (999)"],
);

for my $lookup (@ipv4_lookups) {
    my ($addr, $desc) = @$lookup;
    my $nh = $ipv4_table->lookup($addr);
    printf "  %-15s -> %s  (%s)\n", $addr, 
           defined($nh) ? $nh : "no match", $desc;
}
print "\n";

# Batch lookup (more efficient for multiple addresses)
print "Batch lookup:\n";
my @batch_addrs = qw(10.1.1.1 10.1.2.2 10.2.3.4 192.168.1.1 8.8.8.8);
my @batch_results = $ipv4_table->lookup_batch(\@batch_addrs);

for my $i (0 .. $#batch_addrs) {
    my $nh = $batch_results[$i];
    printf "  %-15s -> %s\n", $batch_addrs[$i],
           defined($nh) ? $nh : "no match";
}
print "\n";

# Delete a route and see the fallback
print "Deleting 10.1.1.0/24 and looking up 10.1.1.5 again:\n";
$ipv4_table->delete("10.1.1.0/24");
my $nh = $ipv4_table->lookup("10.1.1.5");
printf "  10.1.1.5 now -> %d (fell back to /16)\n\n", $nh;

# Clean up
undef $ipv4_table;

# ============================================================================
# IPv6 Example
# ============================================================================

print "IPv6 Routing Table Example\n";
print "-" x 40, "\n\n";

# Create IPv6 routing table
my $ipv6_table = Net::LPM->new_ipv6();

# Add some routes
print "Adding routes:\n";
my @ipv6_routes = (
    ["::/0",                   999, "Default route"],
    ["2001:db8::/32",          100, "Documentation prefix"],
    ["2001:db8:1::/48",        101, "More specific /48"],
    ["2001:db8:1:1::/64",      102, "Even more specific /64"],
    ["fe80::/10",              200, "Link-local"],
    ["fc00::/7",               300, "Unique local addresses"],
    ["2607:f8b0:4000::/36",    400, "Google prefix"],
);

for my $route (@ipv6_routes) {
    my ($prefix, $nh, $desc) = @$route;
    $ipv6_table->insert($prefix, $nh);
    printf "  %-24s -> %3d  (%s)\n", $prefix, $nh, $desc;
}
print "\n";

# Single lookups
print "Single lookups:\n";
my @ipv6_lookups = (
    ["2001:db8:1:1::1",           "Matches /64 (102)"],
    ["2001:db8:1:2::1",           "Matches /48 (101)"],
    ["2001:db8:ffff::1",          "Matches /32 (100)"],
    ["fe80::1",                   "Link-local (200)"],
    ["fd00::1",                   "Unique local (300)"],
    ["2607:f8b0:4004:800::200e", "Google (400)"],
    ["2620:fe::fe",              "Default route (999)"],
);

for my $lookup (@ipv6_lookups) {
    my ($addr, $desc) = @$lookup;
    my $nh = $ipv6_table->lookup($addr);
    printf "  %-28s -> %s  (%s)\n", $addr,
           defined($nh) ? $nh : "no match", $desc;
}
print "\n";

# Batch lookup
print "Batch lookup:\n";
my @ipv6_batch = qw(2001:db8:1:1::1 2001:db8:1:2::1 fe80::1 2620:fe::fe);
my @ipv6_results = $ipv6_table->lookup_batch(\@ipv6_batch);

for my $i (0 .. $#ipv6_batch) {
    my $nh = $ipv6_results[$i];
    printf "  %-24s -> %s\n", $ipv6_batch[$i],
           defined($nh) ? $nh : "no match";
}
print "\n";

# Clean up
undef $ipv6_table;

# ============================================================================
# Performance tips
# ============================================================================

print "Performance Tips\n";
print "-" x 40, "\n";
print <<'TIPS';

1. Use batch lookups for multiple addresses:
   my @results = $table->lookup_batch(\@addresses);

2. Reuse table objects - avoid frequent create/destroy cycles

3. For highest performance, keep tables in scope during
   lookup-intensive operations

4. The underlying C library uses SIMD optimizations automatically

TIPS

print "=" x 60, "\n";
print "Example completed successfully!\n";
print "=" x 60, "\n";

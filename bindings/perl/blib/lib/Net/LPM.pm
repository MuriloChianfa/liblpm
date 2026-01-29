package Net::LPM;

use strict;
use warnings;
use Carp qw(croak);

our $VERSION = '2.0.0';

# Load XS code first
require XSLoader;
XSLoader::load('Net::LPM', $VERSION);

# Export constants - must be after XSLoader
use Exporter 'import';
our @EXPORT_OK = qw(LPM_INVALID_NEXT_HOP);
our %EXPORT_TAGS = (
    constants => [qw(LPM_INVALID_NEXT_HOP)],
    all       => \@EXPORT_OK,
);

# Constant for invalid next hop (0xFFFFFFFF)
# Define as literal to avoid load-order issues
use constant LPM_INVALID_NEXT_HOP => 0xFFFFFFFF;

=head1 NAME

Net::LPM - High-performance Perl bindings for liblpm Longest Prefix Match library

=head1 VERSION

Version 2.0.0

=head1 SYNOPSIS

    use Net::LPM;
    
    # Create an IPv4 routing table
    my $table = Net::LPM->new_ipv4();
    
    # Insert routes (CIDR notation)
    $table->insert("192.168.0.0/16", 100);
    $table->insert("10.0.0.0/8", 200);
    $table->insert("0.0.0.0/0", 999);  # Default route
    
    # Lookup an address
    my $next_hop = $table->lookup("192.168.1.1");
    if (defined $next_hop) {
        print "Next hop: $next_hop\n";  # Prints: Next hop: 100
    }
    
    # Batch lookup (more efficient for multiple addresses)
    my @addresses = qw(192.168.1.1 10.1.1.1 172.16.0.1);
    my @next_hops = $table->lookup_batch(\@addresses);
    
    # Delete a route
    $table->delete("10.0.0.0/8");
    
    # Table is automatically destroyed when $table goes out of scope

=head1 DESCRIPTION

Net::LPM provides high-performance Perl bindings to the liblpm C library
for Longest Prefix Match (LPM) routing table operations.

LPM is a fundamental operation in IP routing where, given an IP address,
you need to find the most specific (longest) matching prefix in a routing
table. For example, if a table contains both 192.168.0.0/16 and
192.168.1.0/24, a lookup for 192.168.1.1 would return the /24 route.

=head2 Features

=over 4

=item * B<High Performance>: Direct XS bindings to optimized C library

=item * B<Dual Stack>: Full support for both IPv4 and IPv6

=item * B<Multiple Algorithms>: DIR-24-8 for IPv4, Wide 16-bit stride for IPv6

=item * B<Batch Operations>: Efficient batch lookups to reduce overhead

=item * B<SIMD Optimized>: Runtime CPU feature detection for optimal performance

=item * B<Memory Safe>: Automatic cleanup via Perl's reference counting

=back

=head1 CONSTRUCTOR METHODS

=head2 new_ipv4

    my $table = Net::LPM->new_ipv4();

Creates a new IPv4 routing table using the DIR-24-8 algorithm.
This provides optimal lookup performance with typically 1-2 memory
accesses per lookup.

Returns a new Net::LPM object.

Dies on failure (e.g., out of memory).

=head2 new_ipv6

    my $table = Net::LPM->new_ipv6();

Creates a new IPv6 routing table using the Wide 16-bit stride algorithm.
This is optimized for common /48 prefix allocations.

Returns a new Net::LPM object.

Dies on failure.

=cut

sub new_ipv4 {
    my $class = shift;
    my $ptr = _xs_new_ipv4();
    my $self = bless {
        _ptr  => $ptr,
        _type => 'ipv4',
    }, $class;
    return $self;
}

sub new_ipv6 {
    my $class = shift;
    my $ptr = _xs_new_ipv6();
    my $self = bless {
        _ptr  => $ptr,
        _type => 'ipv6',
    }, $class;
    return $self;
}

=head1 INSTANCE METHODS

=head2 insert

    $table->insert($prefix, $next_hop);
    $table->insert("192.168.0.0/16", 100);
    $table->insert("2001:db8::/32", 200);

Inserts a route into the table.

B<Parameters:>

=over 4

=item * C<$prefix> - CIDR notation prefix (e.g., "192.168.0.0/16" or "2001:db8::/32")

=item * C<$next_hop> - Unsigned 32-bit integer representing the next hop

=back

Returns true on success.

Dies on error (invalid prefix format, prefix length out of range, etc.).

=cut

sub insert {
    my ($self, $prefix, $next_hop) = @_;
    
    croak "Missing prefix argument" unless defined $prefix;
    croak "Missing next_hop argument" unless defined $next_hop;
    croak "next_hop must be a non-negative integer" 
        if $next_hop < 0 || $next_hop != int($next_hop);
    croak "next_hop exceeds 32-bit unsigned range"
        if $next_hop > 0xFFFFFFFF;
    
    return _xs_insert($self->{_ptr}, $prefix, $next_hop);
}

=head2 delete

    my $deleted = $table->delete($prefix);
    $table->delete("192.168.0.0/16");

Deletes a route from the table.

B<Parameters:>

=over 4

=item * C<$prefix> - CIDR notation prefix to delete

=back

Returns true if the prefix was found and deleted, false if not found.

Dies on error (invalid prefix format).

=cut

sub delete {
    my ($self, $prefix) = @_;
    
    croak "Missing prefix argument" unless defined $prefix;
    
    return _xs_delete($self->{_ptr}, $prefix);
}

=head2 lookup

    my $next_hop = $table->lookup($address);
    my $nh = $table->lookup("192.168.1.1");
    my $nh = $table->lookup("2001:db8::1");

Looks up a single IP address in the table.

B<Parameters:>

=over 4

=item * C<$address> - IP address to lookup (dotted-quad for IPv4, colon notation for IPv6)

=back

Returns the next hop value if a matching prefix is found, C<undef> otherwise.

Dies on error (invalid address format, wrong IP version).

=cut

sub lookup {
    my ($self, $address) = @_;
    
    croak "Missing address argument" unless defined $address;
    
    return _xs_lookup($self->{_ptr}, $address);
}

=head2 lookup_batch

    my @next_hops = $table->lookup_batch(\@addresses);
    my @results = $table->lookup_batch([qw(192.168.1.1 10.0.0.1 172.16.0.1)]);

Performs batch lookup for multiple addresses. This is more efficient than
calling lookup() multiple times because it amortizes the Perl/C call overhead.

B<Parameters:>

=over 4

=item * C<\@addresses> - Array reference of IP addresses to lookup

=back

Returns a list of next hop values (or C<undef> for addresses with no match),
in the same order as the input addresses.

Dies on error (invalid address, wrong IP version).

=cut

sub lookup_batch {
    my ($self, $addrs_ref) = @_;
    
    croak "lookup_batch requires an array reference" 
        unless ref($addrs_ref) eq 'ARRAY';
    
    return () unless @$addrs_ref;
    
    if ($self->{_type} eq 'ipv6') {
        return _xs_lookup_batch_ipv6($self->{_ptr}, $addrs_ref);
    } else {
        return _xs_lookup_batch_ipv4($self->{_ptr}, $addrs_ref);
    }
}

=head2 is_ipv4

    if ($table->is_ipv4) { ... }

Returns true if this is an IPv4 table.

=cut

sub is_ipv4 {
    my $self = shift;
    return $self->{_type} eq 'ipv4';
}

=head2 is_ipv6

    if ($table->is_ipv6) { ... }

Returns true if this is an IPv6 table.

=cut

sub is_ipv6 {
    my $self = shift;
    return $self->{_type} eq 'ipv6';
}

=head2 print_stats

    $table->print_stats();

Prints internal statistics about the routing table to STDOUT.
Useful for debugging and performance analysis.

=cut

sub print_stats {
    my $self = shift;
    _xs_print_stats($self->{_ptr});
}

=head1 CLASS METHODS

=head2 version

    my $version = Net::LPM->version();

Returns the version string of the underlying liblpm C library.

=cut

sub version {
    return _xs_get_version();
}

=head1 DESTRUCTOR

The destructor is automatically called when the object goes out of scope
or is explicitly undefined. It frees all C resources associated with
the routing table.

    undef $table;  # Explicitly destroy
    # Or just let it go out of scope

=cut

sub DESTROY {
    my $self = shift;
    if ($self->{_ptr}) {
        _xs_free($self->{_ptr});
        $self->{_ptr} = undef;
    }
}

=head1 EXPORTS

The following constants can be exported:

    use Net::LPM qw(LPM_INVALID_NEXT_HOP);
    use Net::LPM qw(:constants);
    use Net::LPM qw(:all);

=head2 LPM_INVALID_NEXT_HOP

The value returned internally when no matching prefix is found.
In the Perl API, this is converted to C<undef>, but the constant
is available if needed for comparison.

=head1 PERFORMANCE

=head2 XS Overhead

While the C library provides nanosecond-scale lookups, the XS binding
adds some overhead (~5-20ns per call). For high-performance applications:

=over 4

=item * Use C<lookup_batch()> instead of multiple C<lookup()> calls

=item * Reuse table objects (avoid create/destroy cycles)

=item * Keep tables in scope during lookup-intensive operations

=back

=head2 Batch Operations

Batch operations amortize the Perl/XS overhead across multiple lookups:

    # Slower: N XS calls
    my @results = map { $table->lookup($_) } @addresses;
    
    # Faster: 1 XS call
    my @results = $table->lookup_batch(\@addresses);

=head1 THREAD SAFETY

B<Net::LPM objects are NOT thread-safe.>

Each table should only be accessed from a single thread. For multi-threaded
applications, either:

=over 4

=item * Use separate tables per thread

=item * Add external synchronization (e.g., using L<threads::shared>)

=back

Read-only lookups can be done concurrently if no modifications occur.

=head1 MEMORY MANAGEMENT

The module uses Perl's reference counting for automatic memory management.
When a Net::LPM object goes out of scope or is undefined, the underlying
C resources are automatically freed.

For explicit control:

    undef $table;  # Immediately free resources

=head1 ERROR HANDLING

Methods die on error with descriptive messages. Use eval or Try::Tiny
for error handling:

    use Try::Tiny;
    
    try {
        $table->insert("invalid-prefix", 100);
    } catch {
        warn "Insert failed: $_";
    };

=head1 SEE ALSO

=over 4

=item * L<https://github.com/MuriloChianfa/liblpm> - liblpm project

=item * L<Net::IP> - Pure Perl IP address manipulation

=item * L<NetAddr::IP> - IP address manipulation with prefix support

=back

=head1 AUTHOR

Murilo Chianfa E<lt>murilo.chianfa@outlook.comE<gt>

=head1 LICENSE

This library is licensed under the Boost Software License 1.0.

=head1 COPYRIGHT

Copyright (c) Murilo Chianfa. All rights reserved.

=cut

1;

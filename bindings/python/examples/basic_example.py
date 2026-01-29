#!/usr/bin/env python3
"""Basic example demonstrating liblpm Python bindings.

This example shows the fundamental operations: creating a table,
inserting routes, looking up addresses, and cleaning up resources.
"""

from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network

from liblpm import LpmTableIPv4, LpmTableIPv6


def ipv4_example():
    """Demonstrate IPv4 routing table operations."""
    print("=" * 60)
    print("IPv4 Routing Table Example")
    print("=" * 60)
    
    # Create an IPv4 routing table using context manager
    with LpmTableIPv4() as table:
        print(f"\nCreated IPv4 table with algorithm: {table.algorithm}")
        
        # Insert some routes
        routes = [
            ('0.0.0.0/0', 1),         # Default route
            ('10.0.0.0/8', 10),        # Private network
            ('192.168.0.0/16', 168),   # Private network
            ('192.168.1.0/24', 1681),  # More specific
            ('8.8.8.0/24', 888),       # Google DNS
        ]
        
        print("\nInserting routes:")
        for prefix, next_hop in routes:
            table.insert(prefix, next_hop)
            print(f"  {prefix} -> {next_hop}")
        
        print(f"\nTable now has {table.num_prefixes} prefixes")
        
        # Look up some addresses
        addresses = [
            '192.168.1.100',  # Should match /24 (most specific)
            '192.168.2.100',  # Should match /16
            '10.0.0.1',       # Should match /8
            '8.8.8.8',        # Should match Google DNS
            '1.1.1.1',        # Should match default route
        ]
        
        print("\nLooking up addresses:")
        for addr_str in addresses:
            addr = IPv4Address(addr_str)
            next_hop = table.lookup(addr)
            print(f"  {addr_str} -> next_hop: {next_hop}")
    
    print("\nTable automatically closed after 'with' block")


def ipv6_example():
    """Demonstrate IPv6 routing table operations."""
    print("\n" + "=" * 60)
    print("IPv6 Routing Table Example")
    print("=" * 60)
    
    with LpmTableIPv6() as table:
        print(f"\nCreated IPv6 table with algorithm: {table.algorithm}")
        
        # Insert some routes
        routes = [
            ('::/0', 1),                    # Default route
            ('2001:db8::/32', 2001),         # Documentation prefix
            ('2001:db8:1::/48', 20011),      # More specific
            ('fd00::/8', 0xfd),              # Unique local
            ('fe80::/10', 0xfe80),           # Link-local
        ]
        
        print("\nInserting routes:")
        for prefix, next_hop in routes:
            table.insert(prefix, next_hop)
            print(f"  {prefix} -> {next_hop}")
        
        print(f"\nTable now has {table.num_prefixes} prefixes")
        
        # Look up some addresses
        addresses = [
            '2001:db8:1::1',    # Should match /48 (most specific)
            '2001:db8:ffff::1', # Should match /32
            'fd00::1',          # Should match unique local
            'fe80::1',          # Should match link-local
            '2607:f8b0::1',     # Should match default route
        ]
        
        print("\nLooking up addresses:")
        for addr_str in addresses:
            addr = IPv6Address(addr_str)
            next_hop = table.lookup(addr)
            print(f"  {addr_str} -> next_hop: {next_hop}")
    
    print("\nTable automatically closed after 'with' block")


def main():
    """Run all examples."""
    print("liblpm Python Bindings - Basic Example")
    print("======================================\n")
    
    ipv4_example()
    ipv6_example()
    
    print("\n" + "=" * 60)
    print("All examples completed successfully!")
    print("=" * 60)


if __name__ == '__main__':
    main()

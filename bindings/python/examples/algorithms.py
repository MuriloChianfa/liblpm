#!/usr/bin/env python3
"""Example demonstrating different LPM algorithms.

liblpm provides multiple algorithms optimized for different use cases:

IPv4:
- DIR24: Uses a 24-bit direct table for fast lookups (recommended)
- Stride8: Standard 8-bit stride trie, memory efficient

IPv6:
- Wide16: 16-bit stride for first level, optimal for /48 prefixes (recommended)
- Stride8: Standard 8-bit stride trie, memory efficient
"""

import time
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network

from liblpm import LpmTableIPv4, LpmTableIPv6, Algorithm


def compare_ipv4_algorithms():
    """Compare IPv4 algorithms."""
    print("IPv4 Algorithm Comparison")
    print("=" * 50)
    
    # Generate test data
    routes = [(f'{i}.{j}.0.0/16', i * 256 + j) 
              for i in range(64) for j in range(64)]
    addresses = [IPv4Address(f'{i}.{j}.1.1') 
                 for i in range(64) for j in range(64)]
    
    for algo in ['dir24', 'stride8']:
        print(f"\n{algo.upper()} Algorithm:")
        print("-" * 30)
        
        with LpmTableIPv4(algo) as table:
            # Insert routes
            start = time.perf_counter()
            for prefix, next_hop in routes:
                table.insert(prefix, next_hop)
            insert_time = time.perf_counter() - start
            
            # Single lookups
            start = time.perf_counter()
            for addr in addresses:
                table.lookup(addr)
            single_time = time.perf_counter() - start
            
            # Batch lookups
            start = time.perf_counter()
            table.lookup_batch(addresses)
            batch_time = time.perf_counter() - start
            
            print(f"  Routes inserted: {table.num_prefixes}")
            print(f"  Nodes used: {table.num_nodes}")
            print(f"  Insert time: {insert_time*1000:.2f}ms")
            print(f"  Single lookup: {single_time*1000:.2f}ms ({len(addresses)/single_time:.0f}/s)")
            print(f"  Batch lookup: {batch_time*1000:.2f}ms ({len(addresses)/batch_time:.0f}/s)")


def compare_ipv6_algorithms():
    """Compare IPv6 algorithms."""
    print("\n" + "=" * 50)
    print("IPv6 Algorithm Comparison")
    print("=" * 50)
    
    # Generate test data
    routes = [(f'2001:db8:{i:x}::/48', i) for i in range(1000)]
    addresses = [IPv6Address(f'2001:db8:{i:x}::1') for i in range(1000)]
    
    for algo in ['wide16', 'stride8']:
        print(f"\n{algo.upper()} Algorithm:")
        print("-" * 30)
        
        with LpmTableIPv6(algo) as table:
            # Insert routes
            start = time.perf_counter()
            for prefix, next_hop in routes:
                table.insert(prefix, next_hop)
            insert_time = time.perf_counter() - start
            
            # Single lookups
            start = time.perf_counter()
            for addr in addresses:
                table.lookup(addr)
            single_time = time.perf_counter() - start
            
            # Batch lookups
            start = time.perf_counter()
            table.lookup_batch(addresses)
            batch_time = time.perf_counter() - start
            
            print(f"  Routes inserted: {table.num_prefixes}")
            print(f"  Nodes used: {table.num_nodes}")
            if algo == 'wide16':
                print(f"  Wide nodes: {table.num_wide_nodes}")
            print(f"  Insert time: {insert_time*1000:.2f}ms")
            print(f"  Single lookup: {single_time*1000:.2f}ms ({len(addresses)/single_time:.0f}/s)")
            print(f"  Batch lookup: {batch_time*1000:.2f}ms ({len(addresses)/batch_time:.0f}/s)")


def using_algorithm_enum():
    """Demonstrate using the Algorithm enum."""
    print("\n" + "=" * 50)
    print("Using Algorithm Enum")
    print("=" * 50)
    
    # Can use enum values
    with LpmTableIPv4(Algorithm.DIR24) as table:
        print(f"Created IPv4 table with: {table.algorithm}")
    
    with LpmTableIPv4(Algorithm.STRIDE8) as table:
        print(f"Created IPv4 table with: {table.algorithm}")
    
    with LpmTableIPv6(Algorithm.WIDE16) as table:
        print(f"Created IPv6 table with: {table.algorithm}")
    
    with LpmTableIPv6(Algorithm.STRIDE8) as table:
        print(f"Created IPv6 table with: {table.algorithm}")


def main():
    """Run algorithm comparison examples."""
    print("liblpm Python Bindings - Algorithm Comparison")
    print()
    
    compare_ipv4_algorithms()
    compare_ipv6_algorithms()
    using_algorithm_enum()
    
    print("\n" + "=" * 50)
    print("Algorithm comparison completed!")
    print()
    print("Recommendations:")
    print("- IPv4: Use DIR24 for best lookup performance")
    print("- IPv6: Use Wide16 for typical /48 prefix allocations")
    print("- Stride8: Use when memory is constrained")


if __name__ == '__main__':
    main()

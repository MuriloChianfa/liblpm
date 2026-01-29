#!/usr/bin/env python3
"""Example demonstrating batch lookup operations.

Batch lookups are more efficient when looking up many addresses,
as they reduce the overhead of crossing the Python/C boundary.
"""

import time
from ipaddress import IPv4Address, IPv4Network

from liblpm import LpmTableIPv4


def generate_test_routes(count: int = 1000):
    """Generate test routes for benchmarking."""
    routes = []
    for i in range(min(count, 256)):
        for j in range(min(count // 256 + 1, 256)):
            prefix = f'{i}.{j}.0.0/16'
            next_hop = i * 256 + j
            routes.append((prefix, next_hop))
            if len(routes) >= count:
                return routes
    return routes


def generate_test_addresses(count: int = 10000):
    """Generate test addresses for lookups."""
    addresses = []
    for i in range(256):
        for j in range(256):
            for k in range(count // 65536 + 1):
                addr = IPv4Address(f'{i}.{j}.{k}.1')
                addresses.append(addr)
                if len(addresses) >= count:
                    return addresses
    return addresses


def compare_single_vs_batch():
    """Compare single lookups vs batch lookups."""
    print("Comparing single lookups vs batch lookups")
    print("-" * 50)
    
    # Create and populate table
    routes = generate_test_routes(1000)
    addresses = generate_test_addresses(10000)
    
    with LpmTableIPv4() as table:
        print(f"Inserting {len(routes)} routes...")
        for prefix, next_hop in routes:
            table.insert(prefix, next_hop)
        
        print(f"Looking up {len(addresses)} addresses...")
        
        # Single lookups
        start = time.perf_counter()
        single_results = []
        for addr in addresses:
            result = table.lookup(addr)
            single_results.append(result)
        single_time = time.perf_counter() - start
        
        # Batch lookup
        start = time.perf_counter()
        batch_results = table.lookup_batch(addresses)
        batch_time = time.perf_counter() - start
        
        # Verify results match
        assert len(single_results) == len(batch_results)
        mismatches = sum(1 for s, b in zip(single_results, batch_results) if s != b)
        
        print(f"\nResults:")
        print(f"  Single lookups: {single_time:.4f}s ({len(addresses)/single_time:.0f} lookups/s)")
        print(f"  Batch lookup:   {batch_time:.4f}s ({len(addresses)/batch_time:.0f} lookups/s)")
        print(f"  Speedup:        {single_time/batch_time:.2f}x")
        print(f"  Results match:  {'Yes' if mismatches == 0 else f'No ({mismatches} mismatches)'}")


def batch_with_mixed_results():
    """Demonstrate batch lookup with mixed hit/miss results."""
    print("\n" + "-" * 50)
    print("Batch lookup with mixed hit/miss results")
    print("-" * 50)
    
    with LpmTableIPv4() as table:
        # Insert a few specific routes
        table.insert('192.168.0.0/16', 100)
        table.insert('10.0.0.0/8', 200)
        
        # Mix of addresses that will/won't match
        addresses = [
            IPv4Address('192.168.1.1'),   # Hit -> 100
            IPv4Address('8.8.8.8'),        # Miss
            IPv4Address('10.0.0.1'),       # Hit -> 200
            IPv4Address('172.16.0.1'),     # Miss
            IPv4Address('192.168.2.1'),   # Hit -> 100
        ]
        
        results = table.lookup_batch(addresses)
        
        print("Batch lookup results:")
        for addr, result in zip(addresses, results):
            status = f"next_hop={result}" if result is not None else "no match"
            print(f"  {addr} -> {status}")


def main():
    """Run batch lookup examples."""
    print("liblpm Python Bindings - Batch Lookup Example")
    print("=" * 50)
    print()
    
    compare_single_vs_batch()
    batch_with_mixed_results()
    
    print("\n" + "=" * 50)
    print("Batch lookup examples completed!")


if __name__ == '__main__':
    main()

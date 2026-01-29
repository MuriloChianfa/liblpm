#!/usr/bin/env python3
"""Benchmark batch lookup performance.

This benchmark measures the performance of batch address lookups
compared to individual lookups. Run with:

    pytest benchmarks/benchmark_batch.py --benchmark-only -v
"""

import pytest
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network

from liblpm import LpmTableIPv4, LpmTableIPv6


# ============================================================================
# Fixtures
# ============================================================================

@pytest.fixture
def ipv4_table():
    """IPv4 table with 1000 prefixes."""
    table = LpmTableIPv4()
    for i in range(32):
        for j in range(32):
            table.insert(f'{i}.{j * 8}.0.0/16', i * 32 + j)
    yield table
    table.close()


@pytest.fixture
def ipv6_table():
    """IPv6 table with 1000 prefixes."""
    table = LpmTableIPv6()
    for i in range(1000):
        table.insert(f'2001:db8:{i:x}::/48', i)
    yield table
    table.close()


@pytest.fixture
def ipv4_addresses_100():
    """100 IPv4 addresses for lookup."""
    return [IPv4Address(f'{i % 32}.{(i // 32) * 8}.{i}.1') for i in range(100)]


@pytest.fixture
def ipv4_addresses_1000():
    """1000 IPv4 addresses for lookup."""
    return [IPv4Address(f'{i % 32}.{(i // 32) * 8}.{i % 256}.1') for i in range(1000)]


@pytest.fixture
def ipv4_addresses_10000():
    """10000 IPv4 addresses for lookup."""
    addrs = []
    for i in range(10000):
        a = i % 32
        b = ((i // 32) % 32) * 8
        c = (i // 1024) % 256
        d = (i % 256)
        addrs.append(IPv4Address(f'{a}.{b}.{c}.{d}'))
    return addrs


@pytest.fixture
def ipv6_addresses_100():
    """100 IPv6 addresses for lookup."""
    return [IPv6Address(f'2001:db8:{i:x}::1') for i in range(100)]


@pytest.fixture
def ipv6_addresses_1000():
    """1000 IPv6 addresses for lookup."""
    return [IPv6Address(f'2001:db8:{i:x}::1') for i in range(1000)]


# ============================================================================
# IPv4 Batch Benchmarks
# ============================================================================

class TestBenchmarkIPv4BatchLookup:
    """Benchmark IPv4 batch lookup operations."""
    
    def test_batch_100(self, benchmark, ipv4_table, ipv4_addresses_100):
        """Benchmark batch lookup of 100 addresses."""
        benchmark(ipv4_table.lookup_batch, ipv4_addresses_100)
    
    def test_batch_1000(self, benchmark, ipv4_table, ipv4_addresses_1000):
        """Benchmark batch lookup of 1000 addresses."""
        benchmark(ipv4_table.lookup_batch, ipv4_addresses_1000)
    
    def test_batch_10000(self, benchmark, ipv4_table, ipv4_addresses_10000):
        """Benchmark batch lookup of 10000 addresses."""
        benchmark(ipv4_table.lookup_batch, ipv4_addresses_10000)


class TestBenchmarkIPv4SingleVsBatch:
    """Compare single lookups vs batch lookups for IPv4."""
    
    def test_single_100(self, benchmark, ipv4_table, ipv4_addresses_100):
        """Benchmark 100 individual lookups."""
        def do_lookups():
            for addr in ipv4_addresses_100:
                ipv4_table.lookup(addr)
        
        benchmark(do_lookups)
    
    def test_single_1000(self, benchmark, ipv4_table, ipv4_addresses_1000):
        """Benchmark 1000 individual lookups."""
        def do_lookups():
            for addr in ipv4_addresses_1000:
                ipv4_table.lookup(addr)
        
        benchmark(do_lookups)


# ============================================================================
# IPv6 Batch Benchmarks
# ============================================================================

class TestBenchmarkIPv6BatchLookup:
    """Benchmark IPv6 batch lookup operations."""
    
    def test_batch_100(self, benchmark, ipv6_table, ipv6_addresses_100):
        """Benchmark batch lookup of 100 addresses."""
        benchmark(ipv6_table.lookup_batch, ipv6_addresses_100)
    
    def test_batch_1000(self, benchmark, ipv6_table, ipv6_addresses_1000):
        """Benchmark batch lookup of 1000 addresses."""
        benchmark(ipv6_table.lookup_batch, ipv6_addresses_1000)


class TestBenchmarkIPv6SingleVsBatch:
    """Compare single lookups vs batch lookups for IPv6."""
    
    def test_single_100(self, benchmark, ipv6_table, ipv6_addresses_100):
        """Benchmark 100 individual lookups."""
        def do_lookups():
            for addr in ipv6_addresses_100:
                ipv6_table.lookup(addr)
        
        benchmark(do_lookups)
    
    def test_single_1000(self, benchmark, ipv6_table, ipv6_addresses_1000):
        """Benchmark 1000 individual lookups."""
        def do_lookups():
            for addr in ipv6_addresses_1000:
                ipv6_table.lookup(addr)
        
        benchmark(do_lookups)


# ============================================================================
# Throughput Tests (for informal comparison)
# ============================================================================

class TestThroughput:
    """Measure throughput in lookups per second."""
    
    @pytest.mark.benchmark(group="throughput")
    def test_ipv4_throughput(self, benchmark, ipv4_table, ipv4_addresses_10000):
        """Measure IPv4 batch lookup throughput."""
        result = benchmark(ipv4_table.lookup_batch, ipv4_addresses_10000)
        
        # Calculate throughput
        if hasattr(benchmark, 'stats'):
            mean_time = benchmark.stats['mean']
            throughput = len(ipv4_addresses_10000) / mean_time
            print(f"\nIPv4 throughput: {throughput:,.0f} lookups/sec")
    
    @pytest.mark.benchmark(group="throughput")
    def test_ipv6_throughput(self, benchmark, ipv6_table, ipv6_addresses_1000):
        """Measure IPv6 batch lookup throughput."""
        result = benchmark(ipv6_table.lookup_batch, ipv6_addresses_1000)
        
        # Calculate throughput
        if hasattr(benchmark, 'stats'):
            mean_time = benchmark.stats['mean']
            throughput = len(ipv6_addresses_1000) / mean_time
            print(f"\nIPv6 throughput: {throughput:,.0f} lookups/sec")

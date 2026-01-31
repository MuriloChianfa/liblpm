#!/usr/bin/env python3
"""Benchmark single lookup performance.

This benchmark measures the performance of single address lookups
using pytest-benchmark. Run with:

    pytest benchmarks/benchmark_single.py --benchmark-only -v
"""

import pytest
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network

from liblpm import LpmTableIPv4, LpmTableIPv6


# ============================================================================
# Fixtures
# ============================================================================

@pytest.fixture
def ipv4_table_small():
    """Small IPv4 table with 100 prefixes."""
    table = LpmTableIPv4()
    for i in range(10):
        for j in range(10):
            table.insert(f'{i}.{j * 25}.0.0/16', i * 10 + j)
    yield table
    table.close()


@pytest.fixture
def ipv4_table_medium():
    """Medium IPv4 table with 1000 prefixes."""
    table = LpmTableIPv4()
    for i in range(32):
        for j in range(32):
            table.insert(f'{i}.{j * 8}.0.0/16', i * 32 + j)
    yield table
    table.close()


@pytest.fixture
def ipv4_table_large():
    """Large IPv4 table with 10000 prefixes."""
    table = LpmTableIPv4()
    for i in range(100):
        for j in range(100):
            table.insert(f'{i}.{j * 2}.0.0/16', i * 100 + j)
    yield table
    table.close()


@pytest.fixture
def ipv6_table_small():
    """Small IPv6 table with 100 prefixes."""
    table = LpmTableIPv6()
    for i in range(100):
        table.insert(f'2001:db8:{i:x}::/48', i)
    yield table
    table.close()


@pytest.fixture
def ipv6_table_medium():
    """Medium IPv6 table with 1000 prefixes."""
    table = LpmTableIPv6()
    for i in range(1000):
        table.insert(f'2001:db8:{i:x}::/48', i)
    yield table
    table.close()


@pytest.fixture
def ipv6_table_large():
    """Large IPv6 table with 10000 prefixes."""
    table = LpmTableIPv6()
    for i in range(10000):
        table.insert(f'2001:{i // 256:x}:{i % 256:x}::/48', i)
    yield table
    table.close()


# ============================================================================
# IPv4 Benchmarks
# ============================================================================

class TestBenchmarkIPv4SingleLookup:
    """Benchmark IPv4 single lookup operations."""
    
    def test_lookup_small_table(self, benchmark, ipv4_table_small):
        """Benchmark lookup in small table."""
        addr = IPv4Address('5.125.0.1')
        benchmark(ipv4_table_small.lookup, addr)
    
    def test_lookup_medium_table(self, benchmark, ipv4_table_medium):
        """Benchmark lookup in medium table."""
        addr = IPv4Address('16.128.0.1')
        benchmark(ipv4_table_medium.lookup, addr)
    
    def test_lookup_large_table(self, benchmark, ipv4_table_large):
        """Benchmark lookup in large table."""
        addr = IPv4Address('50.100.0.1')
        benchmark(ipv4_table_large.lookup, addr)
    
    def test_lookup_string_address(self, benchmark, ipv4_table_medium):
        """Benchmark lookup with string address (includes parsing)."""
        benchmark(ipv4_table_medium.lookup, '16.128.0.1')
    
    def test_lookup_miss(self, benchmark, ipv4_table_medium):
        """Benchmark lookup that doesn't match."""
        addr = IPv4Address('200.200.200.200')
        benchmark(ipv4_table_medium.lookup, addr)


class TestBenchmarkIPv4Insert:
    """Benchmark IPv4 insert operations."""
    
    def test_insert_single(self, benchmark):
        """Benchmark single insert operation."""
        def do_insert():
            with LpmTableIPv4() as table:
                table.insert('192.168.0.0/16', 100)
        
        benchmark(do_insert)
    
    def test_insert_100(self, benchmark):
        """Benchmark inserting 100 prefixes."""
        def do_insert():
            with LpmTableIPv4() as table:
                for i in range(100):
                    table.insert(f'{i}.0.0.0/8', i)
        
        benchmark(do_insert)


# ============================================================================
# IPv6 Benchmarks
# ============================================================================

class TestBenchmarkIPv6SingleLookup:
    """Benchmark IPv6 single lookup operations."""
    
    def test_lookup_small_table(self, benchmark, ipv6_table_small):
        """Benchmark lookup in small table."""
        addr = IPv6Address('2001:db8:32::1')
        benchmark(ipv6_table_small.lookup, addr)
    
    def test_lookup_medium_table(self, benchmark, ipv6_table_medium):
        """Benchmark lookup in medium table."""
        addr = IPv6Address('2001:db8:1f4::1')
        benchmark(ipv6_table_medium.lookup, addr)
    
    def test_lookup_large_table(self, benchmark, ipv6_table_large):
        """Benchmark lookup in large table."""
        addr = IPv6Address('2001:13:88::1')
        benchmark(ipv6_table_large.lookup, addr)
    
    def test_lookup_string_address(self, benchmark, ipv6_table_medium):
        """Benchmark lookup with string address (includes parsing)."""
        benchmark(ipv6_table_medium.lookup, '2001:db8:1f4::1')
    
    def test_lookup_miss(self, benchmark, ipv6_table_medium):
        """Benchmark lookup that doesn't match."""
        addr = IPv6Address('2607:f8b0::1')
        benchmark(ipv6_table_medium.lookup, addr)


class TestBenchmarkIPv6Insert:
    """Benchmark IPv6 insert operations."""
    
    def test_insert_single(self, benchmark):
        """Benchmark single insert operation."""
        def do_insert():
            with LpmTableIPv6() as table:
                table.insert('2001:db8::/32', 100)
        
        benchmark(do_insert)
    
    def test_insert_100(self, benchmark):
        """Benchmark inserting 100 prefixes."""
        def do_insert():
            with LpmTableIPv6() as table:
                for i in range(100):
                    table.insert(f'2001:db8:{i:x}::/48', i)
        
        benchmark(do_insert)


# ============================================================================
# Algorithm Comparison
# ============================================================================

class TestBenchmarkAlgorithmComparison:
    """Compare performance of different algorithms."""
    
    @pytest.fixture
    def ipv4_dir24_table(self):
        """IPv4 table with DIR24 algorithm."""
        table = LpmTableIPv4('dir24')
        for i in range(256):
            table.insert(f'{i}.0.0.0/8', i)
        yield table
        table.close()
    
    @pytest.fixture
    def ipv4_stride8_table(self):
        """IPv4 table with Stride8 algorithm."""
        table = LpmTableIPv4('stride8')
        for i in range(256):
            table.insert(f'{i}.0.0.0/8', i)
        yield table
        table.close()
    
    @pytest.fixture
    def ipv6_wide16_table(self):
        """IPv6 table with Wide16 algorithm."""
        table = LpmTableIPv6('wide16')
        for i in range(256):
            table.insert(f'2001:db8:{i:x}::/48', i)
        yield table
        table.close()
    
    @pytest.fixture
    def ipv6_stride8_table(self):
        """IPv6 table with Stride8 algorithm."""
        table = LpmTableIPv6('stride8')
        for i in range(256):
            table.insert(f'2001:db8:{i:x}::/48', i)
        yield table
        table.close()
    
    def test_ipv4_dir24_lookup(self, benchmark, ipv4_dir24_table):
        """Benchmark DIR24 algorithm lookup."""
        addr = IPv4Address('128.0.0.1')
        benchmark(ipv4_dir24_table.lookup, addr)
    
    def test_ipv4_stride8_lookup(self, benchmark, ipv4_stride8_table):
        """Benchmark Stride8 algorithm lookup."""
        addr = IPv4Address('128.0.0.1')
        benchmark(ipv4_stride8_table.lookup, addr)
    
    def test_ipv6_wide16_lookup(self, benchmark, ipv6_wide16_table):
        """Benchmark Wide16 algorithm lookup."""
        addr = IPv6Address('2001:db8:80::1')
        benchmark(ipv6_wide16_table.lookup, addr)
    
    def test_ipv6_stride8_lookup(self, benchmark, ipv6_stride8_table):
        """Benchmark Stride8 algorithm lookup."""
        addr = IPv6Address('2001:db8:80::1')
        benchmark(ipv6_stride8_table.lookup, addr)

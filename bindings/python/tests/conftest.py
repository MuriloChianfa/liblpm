"""Pytest configuration and fixtures for liblpm tests."""

import pytest
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network


# ============================================================================
# IPv4 Fixtures
# ============================================================================

@pytest.fixture
def ipv4_prefixes():
    """Sample IPv4 prefixes for testing."""
    return [
        (IPv4Network('0.0.0.0/0'), 1),        # Default route
        (IPv4Network('10.0.0.0/8'), 10),       # Class A private
        (IPv4Network('172.16.0.0/12'), 172),   # Class B private
        (IPv4Network('192.168.0.0/16'), 192),  # Class C private
        (IPv4Network('192.168.1.0/24'), 1921), # More specific
        (IPv4Network('8.8.8.0/24'), 888),      # Google DNS
    ]


@pytest.fixture
def ipv4_addresses():
    """Sample IPv4 addresses for testing."""
    return [
        IPv4Address('10.0.0.1'),
        IPv4Address('10.255.255.255'),
        IPv4Address('172.16.0.1'),
        IPv4Address('172.31.255.255'),
        IPv4Address('192.168.0.1'),
        IPv4Address('192.168.1.1'),
        IPv4Address('192.168.2.1'),
        IPv4Address('8.8.8.8'),
        IPv4Address('1.1.1.1'),
    ]


# ============================================================================
# IPv6 Fixtures
# ============================================================================

@pytest.fixture
def ipv6_prefixes():
    """Sample IPv6 prefixes for testing."""
    return [
        (IPv6Network('::/0'), 1),                    # Default route
        (IPv6Network('2001:db8::/32'), 2001),        # Documentation prefix
        (IPv6Network('2001:db8:1::/48'), 20011),     # More specific
        (IPv6Network('2001:db8:1:2::/64'), 200112),  # Even more specific
        (IPv6Network('fd00::/8'), 0xfd),             # Unique local
        (IPv6Network('fe80::/10'), 0xfe80),          # Link-local
    ]


@pytest.fixture
def ipv6_addresses():
    """Sample IPv6 addresses for testing."""
    return [
        IPv6Address('2001:db8::1'),
        IPv6Address('2001:db8:1::1'),
        IPv6Address('2001:db8:1:2::1'),
        IPv6Address('2001:db8:ffff::1'),
        IPv6Address('fd00::1'),
        IPv6Address('fe80::1'),
        IPv6Address('2607:f8b0:4004:800::200e'),  # Google
    ]


# ============================================================================
# Large Dataset Fixtures (for benchmarks)
# ============================================================================

@pytest.fixture
def large_ipv4_prefixes():
    """Generate a large set of IPv4 prefixes for benchmarking."""
    prefixes = []
    # Generate /24 prefixes
    for i in range(256):
        for j in range(256):
            prefix = IPv4Network(f'{i}.{j}.0.0/16')
            prefixes.append((prefix, i * 256 + j))
            if len(prefixes) >= 10000:
                return prefixes
    return prefixes


@pytest.fixture
def large_ipv4_addresses():
    """Generate a large set of IPv4 addresses for benchmarking."""
    addresses = []
    for i in range(256):
        for j in range(256):
            for k in range(4):
                addr = IPv4Address(f'{i}.{j}.{k}.1')
                addresses.append(addr)
                if len(addresses) >= 10000:
                    return addresses
    return addresses


# ============================================================================
# Utility Fixtures
# ============================================================================

@pytest.fixture
def random_seed():
    """Fixed random seed for reproducible tests."""
    return 42

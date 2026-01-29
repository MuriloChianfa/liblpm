"""Tests for batch lookup operations."""

import pytest
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network

from liblpm import LpmTableIPv4, LpmTableIPv6, LpmClosedError


class TestBatchLookupIPv4:
    """Tests for IPv4 batch lookup operations."""
    
    def test_batch_lookup_basic(self):
        """Test basic batch lookup."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            table.insert('10.0.0.0/8', 200)
            
            addrs = [
                IPv4Address('192.168.1.1'),
                IPv4Address('192.168.2.2'),
                IPv4Address('10.0.0.1'),
            ]
            
            results = table.lookup_batch(addrs)
            assert len(results) == 3
            assert results[0] == 100
            assert results[1] == 100
            assert results[2] == 200
    
    def test_batch_lookup_empty(self):
        """Test batch lookup with empty list."""
        with LpmTableIPv4() as table:
            results = table.lookup_batch([])
            assert results == []
    
    def test_batch_lookup_single(self):
        """Test batch lookup with single address."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            
            results = table.lookup_batch([IPv4Address('192.168.1.1')])
            assert len(results) == 1
            assert results[0] == 100
    
    def test_batch_lookup_with_misses(self):
        """Test batch lookup with some addresses not matching."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            
            addrs = [
                IPv4Address('192.168.1.1'),  # Match
                IPv4Address('10.0.0.1'),      # No match
                IPv4Address('192.168.2.2'),  # Match
            ]
            
            results = table.lookup_batch(addrs)
            assert results[0] == 100
            assert results[1] is None
            assert results[2] == 100
    
    def test_batch_lookup_string_addresses(self):
        """Test batch lookup with string addresses."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            
            addrs = ['192.168.1.1', '192.168.2.2']
            results = table.lookup_batch(addrs)
            
            assert results[0] == 100
            assert results[1] == 100
    
    def test_batch_lookup_large(self):
        """Test batch lookup with many addresses."""
        with LpmTableIPv4() as table:
            table.insert('0.0.0.0/0', 1)
            
            # Create 1000 addresses
            addrs = [IPv4Address(f'{i}.{i % 256}.0.1') for i in range(256)]
            
            results = table.lookup_batch(addrs)
            assert len(results) == 256
            assert all(r == 1 for r in results)
    
    def test_batch_lookup_closed_table(self):
        """Test batch lookup on closed table."""
        table = LpmTableIPv4()
        table.insert('192.168.0.0/16', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.lookup_batch([IPv4Address('192.168.1.1')])
    
    def test_batch_lookup_longest_prefix(self):
        """Test batch lookup returns longest prefix match."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 16)
            table.insert('192.168.1.0/24', 24)
            
            addrs = [
                IPv4Address('192.168.1.1'),   # Should match /24
                IPv4Address('192.168.2.1'),   # Should match /16
            ]
            
            results = table.lookup_batch(addrs)
            assert results[0] == 24
            assert results[1] == 16


class TestBatchLookupIPv6:
    """Tests for IPv6 batch lookup operations."""
    
    def test_batch_lookup_basic(self):
        """Test basic batch lookup."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            table.insert('fd00::/8', 200)
            
            addrs = [
                IPv6Address('2001:db8::1'),
                IPv6Address('2001:db8:1::1'),
                IPv6Address('fd00::1'),
            ]
            
            results = table.lookup_batch(addrs)
            assert len(results) == 3
            assert results[0] == 100
            assert results[1] == 100
            assert results[2] == 200
    
    def test_batch_lookup_empty(self):
        """Test batch lookup with empty list."""
        with LpmTableIPv6() as table:
            results = table.lookup_batch([])
            assert results == []
    
    def test_batch_lookup_single(self):
        """Test batch lookup with single address."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            
            results = table.lookup_batch([IPv6Address('2001:db8::1')])
            assert len(results) == 1
            assert results[0] == 100
    
    def test_batch_lookup_with_misses(self):
        """Test batch lookup with some addresses not matching."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            
            addrs = [
                IPv6Address('2001:db8::1'),   # Match
                IPv6Address('2001:db9::1'),   # No match
                IPv6Address('2001:db8:1::1'), # Match
            ]
            
            results = table.lookup_batch(addrs)
            assert results[0] == 100
            assert results[1] is None
            assert results[2] == 100
    
    def test_batch_lookup_string_addresses(self):
        """Test batch lookup with string addresses."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            
            addrs = ['2001:db8::1', '2001:db8:1::1']
            results = table.lookup_batch(addrs)
            
            assert results[0] == 100
            assert results[1] == 100
    
    def test_batch_lookup_large(self):
        """Test batch lookup with many addresses."""
        with LpmTableIPv6() as table:
            table.insert('::/0', 1)
            
            # Create 256 addresses
            addrs = [IPv6Address(f'2001:db8:{i:x}::1') for i in range(256)]
            
            results = table.lookup_batch(addrs)
            assert len(results) == 256
            assert all(r == 1 for r in results)
    
    def test_batch_lookup_closed_table(self):
        """Test batch lookup on closed table."""
        table = LpmTableIPv6()
        table.insert('2001:db8::/32', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.lookup_batch([IPv6Address('2001:db8::1')])
    
    def test_batch_lookup_longest_prefix(self):
        """Test batch lookup returns longest prefix match."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 32)
            table.insert('2001:db8:1::/48', 48)
            
            addrs = [
                IPv6Address('2001:db8:1::1'),    # Should match /48
                IPv6Address('2001:db8:ffff::1'), # Should match /32
            ]
            
            results = table.lookup_batch(addrs)
            assert results[0] == 48
            assert results[1] == 32


class TestBatchLookupPerformance:
    """Performance-related tests for batch lookup."""
    
    def test_batch_vs_single_ipv4(self):
        """Verify batch lookup is available for both single and batch."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            
            addr = IPv4Address('192.168.1.1')
            
            # Single lookup
            single_result = table.lookup(addr)
            
            # Batch lookup (single address)
            batch_result = table.lookup_batch([addr])[0]
            
            assert single_result == batch_result
    
    def test_batch_vs_single_ipv6(self):
        """Verify batch lookup is available for both single and batch."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            
            addr = IPv6Address('2001:db8::1')
            
            # Single lookup
            single_result = table.lookup(addr)
            
            # Batch lookup (single address)
            batch_result = table.lookup_batch([addr])[0]
            
            assert single_result == batch_result

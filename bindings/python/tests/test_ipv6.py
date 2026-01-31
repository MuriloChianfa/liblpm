"""Tests for IPv6 LPM table functionality."""

import pytest
from ipaddress import IPv6Address, IPv6Network

from liblpm import (
    LpmTableIPv6,
    Algorithm,
    LpmClosedError,
    LpmInsertError,
    LpmInvalidPrefixError,
)


class TestLpmTableIPv6Creation:
    """Tests for IPv6 table creation."""
    
    def test_create_default(self):
        """Test creating table with default algorithm."""
        table = LpmTableIPv6()
        assert not table.closed
        assert table.algorithm == 'wide16'
        assert table.num_prefixes == 0
        table.close()
    
    def test_create_wide16(self):
        """Test creating table with Wide16 algorithm."""
        table = LpmTableIPv6(Algorithm.WIDE16)
        assert table.algorithm == 'wide16'
        table.close()
    
    def test_create_stride8(self):
        """Test creating table with Stride8 algorithm."""
        table = LpmTableIPv6(Algorithm.STRIDE8)
        assert table.algorithm == 'stride8'
        table.close()
    
    def test_create_string_algorithm(self):
        """Test creating table with string algorithm name."""
        table = LpmTableIPv6('wide16')
        assert table.algorithm == 'wide16'
        table.close()
        
        table = LpmTableIPv6('stride8')
        assert table.algorithm == 'stride8'
        table.close()
    
    def test_create_invalid_algorithm(self):
        """Test creating table with invalid algorithm."""
        with pytest.raises(ValueError, match="Invalid IPv6 algorithm"):
            LpmTableIPv6('invalid')
        
        with pytest.raises(ValueError, match="Invalid IPv6 algorithm"):
            LpmTableIPv6('dir24')  # dir24 is IPv4 only
    
    def test_context_manager(self):
        """Test context manager protocol."""
        with LpmTableIPv6() as table:
            assert not table.closed
        assert table.closed


class TestLpmTableIPv6Insert:
    """Tests for IPv6 insert operations."""
    
    def test_insert_network(self):
        """Test inserting IPv6Network prefix."""
        with LpmTableIPv6() as table:
            table.insert(IPv6Network('2001:db8::/32'), 100)
            assert table.num_prefixes == 1
    
    def test_insert_string(self):
        """Test inserting string prefix."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            assert table.num_prefixes == 1
    
    def test_insert_default_route(self):
        """Test inserting default route (::/0)."""
        with LpmTableIPv6() as table:
            table.insert(IPv6Network('::/0'), 1)
            assert table.num_prefixes == 1
    
    def test_insert_host_route(self):
        """Test inserting host route (/128)."""
        with LpmTableIPv6() as table:
            table.insert(IPv6Network('2001:db8::1/128'), 100)
            result = table.lookup(IPv6Address('2001:db8::1'))
            assert result == 100
    
    def test_insert_multiple(self, ipv6_prefixes):
        """Test inserting multiple prefixes."""
        with LpmTableIPv6() as table:
            for prefix, next_hop in ipv6_prefixes:
                table.insert(prefix, next_hop)
            assert table.num_prefixes == len(ipv6_prefixes)
    
    def test_insert_link_local(self):
        """Test inserting link-local prefix."""
        with LpmTableIPv6() as table:
            table.insert('fe80::/10', 100)
            result = table.lookup('fe80::1')
            assert result == 100
    
    def test_insert_unique_local(self):
        """Test inserting unique local prefix."""
        with LpmTableIPv6() as table:
            table.insert('fd00::/8', 100)
            result = table.lookup('fd12:3456:789a::1')
            assert result == 100
    
    def test_insert_invalid_next_hop_negative(self):
        """Test inserting with negative next hop."""
        with LpmTableIPv6() as table:
            with pytest.raises(ValueError, match="next_hop must be"):
                table.insert('2001:db8::/32', -1)
    
    def test_insert_invalid_prefix(self):
        """Test inserting invalid prefix string."""
        with LpmTableIPv6() as table:
            with pytest.raises(LpmInvalidPrefixError):
                table.insert('not-a-prefix', 100)
    
    def test_insert_closed_table(self):
        """Test inserting into closed table."""
        table = LpmTableIPv6()
        table.close()
        with pytest.raises(LpmClosedError):
            table.insert('2001:db8::/32', 100)
    
    def test_insert_update(self):
        """Test updating existing prefix."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            result1 = table.lookup('2001:db8::1')
            assert result1 == 100
            
            # Insert same prefix with different next hop
            table.insert('2001:db8::/32', 200)
            result2 = table.lookup('2001:db8::1')
            assert result2 == 200


class TestLpmTableIPv6Lookup:
    """Tests for IPv6 lookup operations."""
    
    def test_lookup_match(self):
        """Test lookup with matching prefix."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            result = table.lookup(IPv6Address('2001:db8::1'))
            assert result == 100
    
    def test_lookup_no_match(self):
        """Test lookup with no matching prefix."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            result = table.lookup(IPv6Address('2001:db9::1'))
            assert result is None
    
    def test_lookup_string(self):
        """Test lookup with string address."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            result = table.lookup('2001:db8::1')
            assert result == 100
    
    def test_lookup_longest_prefix_match(self):
        """Test that longest prefix is matched."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 32)
            table.insert('2001:db8:1::/48', 48)
            table.insert('2001:db8:1:2::/64', 64)
            
            # Should match /48 (not /32)
            result = table.lookup('2001:db8:1::1')
            assert result == 48
            
            # Should match /64 (not /48)
            result = table.lookup('2001:db8:1:2::1')
            assert result == 64
            
            # Should match /32 (not covered by /48 or /64)
            result = table.lookup('2001:db8:ffff::1')
            assert result == 32
    
    def test_lookup_default_route(self):
        """Test lookup with default route."""
        with LpmTableIPv6() as table:
            table.insert('::/0', 1)
            
            # All addresses should match default route
            assert table.lookup('2001:db8::1') == 1
            assert table.lookup('fe80::1') == 1
            assert table.lookup('::1') == 1
    
    def test_lookup_closed_table(self):
        """Test lookup on closed table."""
        table = LpmTableIPv6()
        table.insert('2001:db8::/32', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.lookup('2001:db8::1')
    
    def test_lookup_empty_table(self):
        """Test lookup on empty table."""
        with LpmTableIPv6() as table:
            result = table.lookup('2001:db8::1')
            assert result is None
    
    def test_lookup_ipv4_mapped(self):
        """Test lookup with IPv4-mapped IPv6 address."""
        with LpmTableIPv6() as table:
            # ::ffff:0:0/96 is IPv4-mapped prefix
            table.insert('::ffff:0:0/96', 100)
            result = table.lookup('::ffff:192.168.1.1')
            assert result == 100


class TestLpmTableIPv6Delete:
    """Tests for IPv6 delete operations."""
    
    def test_delete_existing(self):
        """Test deleting existing prefix."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            assert table.lookup('2001:db8::1') == 100
            
            table.delete('2001:db8::/32')
            assert table.lookup('2001:db8::1') is None
    
    def test_delete_string(self):
        """Test deleting with string prefix."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            table.delete(IPv6Network('2001:db8::/32'))
            assert table.lookup('2001:db8::1') is None
    
    def test_delete_closed_table(self):
        """Test deleting from closed table."""
        table = LpmTableIPv6()
        table.insert('2001:db8::/32', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.delete('2001:db8::/32')


class TestLpmTableIPv6Properties:
    """Tests for IPv6 table properties."""
    
    def test_repr(self):
        """Test string representation."""
        with LpmTableIPv6() as table:
            repr_str = repr(table)
            assert 'LpmTableIPv6' in repr_str
            assert 'wide16' in repr_str
            assert 'open' in repr_str
        
        # After close
        repr_str = repr(table)
        assert 'closed' in repr_str
    
    def test_num_nodes(self):
        """Test num_nodes property."""
        with LpmTableIPv6() as table:
            initial_nodes = table.num_nodes
            table.insert('2001:db8::/32', 100)
            # Nodes may increase after insert
            assert table.num_nodes >= initial_nodes
    
    def test_num_wide_nodes(self):
        """Test num_wide_nodes property for wide16 algorithm."""
        with LpmTableIPv6('wide16') as table:
            # Wide16 uses 16-bit stride for first level
            initial_wide = table.num_wide_nodes
            table.insert('2001:db8::/32', 100)
            # Wide nodes should exist
            assert table.num_wide_nodes >= initial_wide


class TestLpmTableIPv6Algorithms:
    """Tests for different IPv6 algorithms."""
    
    @pytest.mark.parametrize("algorithm", ['wide16', 'stride8'])
    def test_algorithm_basic_operations(self, algorithm, ipv6_prefixes):
        """Test basic operations with different algorithms."""
        with LpmTableIPv6(algorithm) as table:
            # Insert prefixes
            for prefix, next_hop in ipv6_prefixes:
                table.insert(prefix, next_hop)
            
            assert table.num_prefixes == len(ipv6_prefixes)
            
            # Verify lookups
            for prefix, expected_hop in ipv6_prefixes:
                # Get an address from the prefix
                addr = prefix.network_address
                result = table.lookup(addr)
                # Should match this or a more specific prefix
                assert result is not None
    
    @pytest.mark.parametrize("algorithm", ['wide16', 'stride8'])
    def test_algorithm_large_prefixes(self, algorithm):
        """Test with various prefix lengths."""
        with LpmTableIPv6(algorithm) as table:
            # Test different prefix lengths
            table.insert('2001::/16', 16)
            table.insert('2001:db8::/32', 32)
            table.insert('2001:db8:1::/48', 48)
            table.insert('2001:db8:1:1::/64', 64)
            
            assert table.lookup('2001::1') == 16
            assert table.lookup('2001:db8::1') == 32
            assert table.lookup('2001:db8:1::1') == 48
            assert table.lookup('2001:db8:1:1::1') == 64

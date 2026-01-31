"""Tests for IPv4 LPM table functionality."""

import pytest
from ipaddress import IPv4Address, IPv4Network

from liblpm import (
    LpmTableIPv4,
    Algorithm,
    LpmClosedError,
    LpmInsertError,
    LpmInvalidPrefixError,
)


class TestLpmTableIPv4Creation:
    """Tests for IPv4 table creation."""
    
    def test_create_default(self):
        """Test creating table with default algorithm."""
        table = LpmTableIPv4()
        assert not table.closed
        assert table.algorithm == 'dir24'
        assert table.num_prefixes == 0
        table.close()
    
    def test_create_dir24(self):
        """Test creating table with DIR24 algorithm."""
        table = LpmTableIPv4(Algorithm.DIR24)
        assert table.algorithm == 'dir24'
        table.close()
    
    def test_create_stride8(self):
        """Test creating table with Stride8 algorithm."""
        table = LpmTableIPv4(Algorithm.STRIDE8)
        assert table.algorithm == 'stride8'
        table.close()
    
    def test_create_string_algorithm(self):
        """Test creating table with string algorithm name."""
        table = LpmTableIPv4('dir24')
        assert table.algorithm == 'dir24'
        table.close()
        
        table = LpmTableIPv4('stride8')
        assert table.algorithm == 'stride8'
        table.close()
    
    def test_create_invalid_algorithm(self):
        """Test creating table with invalid algorithm."""
        with pytest.raises(ValueError, match="Invalid IPv4 algorithm"):
            LpmTableIPv4('invalid')
    
    def test_context_manager(self):
        """Test context manager protocol."""
        with LpmTableIPv4() as table:
            assert not table.closed
        assert table.closed


class TestLpmTableIPv4Insert:
    """Tests for IPv4 insert operations."""
    
    def test_insert_network(self):
        """Test inserting IPv4Network prefix."""
        with LpmTableIPv4() as table:
            table.insert(IPv4Network('192.168.0.0/16'), 100)
            assert table.num_prefixes == 1
    
    def test_insert_string(self):
        """Test inserting string prefix."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            assert table.num_prefixes == 1
    
    def test_insert_default_route(self):
        """Test inserting default route (0.0.0.0/0)."""
        with LpmTableIPv4() as table:
            table.insert(IPv4Network('0.0.0.0/0'), 1)
            assert table.num_prefixes == 1
    
    def test_insert_host_route(self):
        """Test inserting host route (/32)."""
        with LpmTableIPv4() as table:
            table.insert(IPv4Network('192.168.1.1/32'), 100)
            result = table.lookup(IPv4Address('192.168.1.1'))
            assert result == 100
    
    def test_insert_multiple(self, ipv4_prefixes):
        """Test inserting multiple prefixes."""
        with LpmTableIPv4() as table:
            for prefix, next_hop in ipv4_prefixes:
                table.insert(prefix, next_hop)
            assert table.num_prefixes == len(ipv4_prefixes)
    
    def test_insert_invalid_next_hop_negative(self):
        """Test inserting with negative next hop."""
        with LpmTableIPv4() as table:
            with pytest.raises(ValueError, match="next_hop must be"):
                table.insert('192.168.0.0/16', -1)
    
    def test_insert_invalid_next_hop_too_large(self):
        """Test inserting with next hop > 32 bits."""
        with LpmTableIPv4() as table:
            with pytest.raises(ValueError, match="next_hop must be"):
                table.insert('192.168.0.0/16', 0x100000000)
    
    def test_insert_invalid_prefix(self):
        """Test inserting invalid prefix string."""
        with LpmTableIPv4() as table:
            with pytest.raises(LpmInvalidPrefixError):
                table.insert('not-a-prefix', 100)
    
    def test_insert_closed_table(self):
        """Test inserting into closed table."""
        table = LpmTableIPv4()
        table.close()
        with pytest.raises(LpmClosedError):
            table.insert('192.168.0.0/16', 100)
    
    def test_insert_update(self):
        """Test updating existing prefix."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            result1 = table.lookup('192.168.1.1')
            assert result1 == 100
            
            # Insert same prefix with different next hop
            table.insert('192.168.0.0/16', 200)
            result2 = table.lookup('192.168.1.1')
            assert result2 == 200


class TestLpmTableIPv4Lookup:
    """Tests for IPv4 lookup operations."""
    
    def test_lookup_match(self):
        """Test lookup with matching prefix."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            result = table.lookup(IPv4Address('192.168.1.1'))
            assert result == 100
    
    def test_lookup_no_match(self):
        """Test lookup with no matching prefix."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            result = table.lookup(IPv4Address('10.0.0.1'))
            assert result is None
    
    def test_lookup_string(self):
        """Test lookup with string address."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            result = table.lookup('192.168.1.1')
            assert result == 100
    
    def test_lookup_longest_prefix_match(self):
        """Test that longest prefix is matched."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 16)
            table.insert('192.168.1.0/24', 24)
            table.insert('192.168.1.128/25', 25)
            
            # Should match /24 (not /16)
            result = table.lookup('192.168.1.1')
            assert result == 24
            
            # Should match /25 (not /24)
            result = table.lookup('192.168.1.129')
            assert result == 25
            
            # Should match /16 (not covered by /24 or /25)
            result = table.lookup('192.168.2.1')
            assert result == 16
    
    def test_lookup_default_route(self):
        """Test lookup with default route."""
        with LpmTableIPv4() as table:
            table.insert('0.0.0.0/0', 1)
            
            # All addresses should match default route
            assert table.lookup('1.1.1.1') == 1
            assert table.lookup('8.8.8.8') == 1
            assert table.lookup('192.168.1.1') == 1
    
    def test_lookup_closed_table(self):
        """Test lookup on closed table."""
        table = LpmTableIPv4()
        table.insert('192.168.0.0/16', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.lookup('192.168.1.1')
    
    def test_lookup_empty_table(self):
        """Test lookup on empty table."""
        with LpmTableIPv4() as table:
            result = table.lookup('192.168.1.1')
            assert result is None


class TestLpmTableIPv4Delete:
    """Tests for IPv4 delete operations."""
    
    def test_delete_existing(self):
        """Test deleting existing prefix."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            assert table.lookup('192.168.1.1') == 100
            
            table.delete('192.168.0.0/16')
            assert table.lookup('192.168.1.1') is None
    
    def test_delete_string(self):
        """Test deleting with string prefix."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            table.delete(IPv4Network('192.168.0.0/16'))
            assert table.lookup('192.168.1.1') is None
    
    def test_delete_closed_table(self):
        """Test deleting from closed table."""
        table = LpmTableIPv4()
        table.insert('192.168.0.0/16', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.delete('192.168.0.0/16')


class TestLpmTableIPv4Properties:
    """Tests for IPv4 table properties."""
    
    def test_repr(self):
        """Test string representation."""
        with LpmTableIPv4() as table:
            repr_str = repr(table)
            assert 'LpmTableIPv4' in repr_str
            assert 'dir24' in repr_str
            assert 'open' in repr_str
        
        # After close
        repr_str = repr(table)
        assert 'closed' in repr_str
    
    def test_num_nodes(self):
        """Test num_nodes property."""
        with LpmTableIPv4() as table:
            initial_nodes = table.num_nodes
            table.insert('192.168.0.0/16', 100)
            # Nodes may increase after insert
            assert table.num_nodes >= initial_nodes


class TestLpmTableIPv4Algorithms:
    """Tests for different IPv4 algorithms."""
    
    @pytest.mark.parametrize("algorithm", ['dir24', 'stride8'])
    def test_algorithm_basic_operations(self, algorithm, ipv4_prefixes):
        """Test basic operations with different algorithms."""
        with LpmTableIPv4(algorithm) as table:
            # Insert prefixes
            for prefix, next_hop in ipv4_prefixes:
                table.insert(prefix, next_hop)
            
            assert table.num_prefixes == len(ipv4_prefixes)
            
            # Verify lookups
            for prefix, expected_hop in ipv4_prefixes:
                # Get an address from the prefix
                addr = prefix.network_address
                result = table.lookup(addr)
                # Should match this or a more specific prefix
                assert result is not None

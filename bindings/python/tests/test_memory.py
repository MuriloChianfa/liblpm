"""Tests for memory management and resource cleanup."""

import gc
import pytest
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network

from liblpm import LpmTableIPv4, LpmTableIPv6, LpmClosedError


class TestMemoryManagementIPv4:
    """Tests for IPv4 table memory management."""
    
    def test_explicit_close(self):
        """Test explicit close frees resources."""
        table = LpmTableIPv4()
        table.insert('192.168.0.0/16', 100)
        
        assert not table.closed
        table.close()
        assert table.closed
        
        # Second close should be safe
        table.close()
        assert table.closed
    
    def test_context_manager_cleanup(self):
        """Test context manager properly cleans up."""
        with LpmTableIPv4() as table:
            table.insert('192.168.0.0/16', 100)
            assert not table.closed
        
        assert table.closed
    
    def test_context_manager_exception(self):
        """Test context manager cleans up even on exception."""
        table = None
        try:
            with LpmTableIPv4() as table:
                table.insert('192.168.0.0/16', 100)
                raise ValueError("Test exception")
        except ValueError:
            pass
        
        assert table is not None
        assert table.closed
    
    def test_garbage_collection(self):
        """Test that garbage collection frees resources."""
        table = LpmTableIPv4()
        table.insert('192.168.0.0/16', 100)
        
        # Delete reference and force GC
        del table
        gc.collect()
        
        # No way to verify C memory is freed, but we can verify
        # no exceptions are raised during GC
    
    def test_multiple_tables(self):
        """Test creating multiple tables doesn't leak memory."""
        for _ in range(100):
            with LpmTableIPv4() as table:
                table.insert('192.168.0.0/16', 100)
        
        gc.collect()
    
    def test_operations_after_close(self):
        """Test all operations fail after close."""
        table = LpmTableIPv4()
        table.insert('192.168.0.0/16', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.insert('10.0.0.0/8', 200)
        
        with pytest.raises(LpmClosedError):
            table.lookup(IPv4Address('192.168.1.1'))
        
        with pytest.raises(LpmClosedError):
            table.delete('192.168.0.0/16')
        
        with pytest.raises(LpmClosedError):
            table.lookup_batch([IPv4Address('192.168.1.1')])


class TestMemoryManagementIPv6:
    """Tests for IPv6 table memory management."""
    
    def test_explicit_close(self):
        """Test explicit close frees resources."""
        table = LpmTableIPv6()
        table.insert('2001:db8::/32', 100)
        
        assert not table.closed
        table.close()
        assert table.closed
        
        # Second close should be safe
        table.close()
        assert table.closed
    
    def test_context_manager_cleanup(self):
        """Test context manager properly cleans up."""
        with LpmTableIPv6() as table:
            table.insert('2001:db8::/32', 100)
            assert not table.closed
        
        assert table.closed
    
    def test_context_manager_exception(self):
        """Test context manager cleans up even on exception."""
        table = None
        try:
            with LpmTableIPv6() as table:
                table.insert('2001:db8::/32', 100)
                raise ValueError("Test exception")
        except ValueError:
            pass
        
        assert table is not None
        assert table.closed
    
    def test_garbage_collection(self):
        """Test that garbage collection frees resources."""
        table = LpmTableIPv6()
        table.insert('2001:db8::/32', 100)
        
        # Delete reference and force GC
        del table
        gc.collect()
    
    def test_multiple_tables(self):
        """Test creating multiple tables doesn't leak memory."""
        for _ in range(100):
            with LpmTableIPv6() as table:
                table.insert('2001:db8::/32', 100)
        
        gc.collect()
    
    def test_operations_after_close(self):
        """Test all operations fail after close."""
        table = LpmTableIPv6()
        table.insert('2001:db8::/32', 100)
        table.close()
        
        with pytest.raises(LpmClosedError):
            table.insert('fd00::/8', 200)
        
        with pytest.raises(LpmClosedError):
            table.lookup(IPv6Address('2001:db8::1'))
        
        with pytest.raises(LpmClosedError):
            table.delete('2001:db8::/32')
        
        with pytest.raises(LpmClosedError):
            table.lookup_batch([IPv6Address('2001:db8::1')])


class TestLargeScaleOperations:
    """Tests for large-scale operations to catch memory issues."""
    
    def test_many_inserts_ipv4(self):
        """Test inserting many IPv4 prefixes."""
        with LpmTableIPv4() as table:
            # Insert 1000 prefixes
            for i in range(256):
                for j in range(4):
                    prefix = f'{i}.{j * 64}.0.0/16'
                    table.insert(prefix, i * 4 + j)
            
            assert table.num_prefixes >= 1000
    
    def test_many_inserts_ipv6(self):
        """Test inserting many IPv6 prefixes."""
        with LpmTableIPv6() as table:
            # Insert 1000 prefixes
            for i in range(1000):
                prefix = f'2001:db8:{i:x}::/48'
                table.insert(prefix, i)
            
            assert table.num_prefixes >= 1000
    
    def test_repeated_insert_delete_ipv4(self):
        """Test repeated insert/delete cycles."""
        with LpmTableIPv4() as table:
            for _ in range(100):
                table.insert('192.168.0.0/16', 100)
                table.delete('192.168.0.0/16')
            
            # Table should be empty
            assert table.lookup('192.168.1.1') is None
    
    def test_repeated_insert_delete_ipv6(self):
        """Test repeated insert/delete cycles."""
        with LpmTableIPv6() as table:
            for _ in range(100):
                table.insert('2001:db8::/32', 100)
                table.delete('2001:db8::/32')
            
            # Table should be empty
            assert table.lookup('2001:db8::1') is None

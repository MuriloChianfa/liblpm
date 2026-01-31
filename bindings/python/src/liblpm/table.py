"""High-level Pythonic API for liblpm.

This module provides user-friendly wrapper classes that integrate with
Python's standard library types (ipaddress module) and idioms (context
managers, iterators).

Example:
    >>> from liblpm import LpmTableIPv4
    >>> from ipaddress import IPv4Address, IPv4Network
    >>> 
    >>> with LpmTableIPv4() as table:
    ...     table.insert(IPv4Network('192.168.0.0/16'), 100)
    ...     table.insert(IPv4Network('10.0.0.0/8'), 200)
    ...     
    ...     next_hop = table.lookup(IPv4Address('192.168.1.1'))
    ...     print(f"Next hop: {next_hop}")
    Next hop: 100
"""

from __future__ import annotations

import array
import struct
from enum import Enum
from ipaddress import (
    IPv4Address,
    IPv4Network,
    IPv6Address,
    IPv6Network,
)
from typing import (
    List,
    Optional,
    Sequence,
    Union,
)

from ._liblpm import _LpmTrieIPv4, _LpmTrieIPv6, INVALID_NEXT_HOP, get_version
from .exceptions import (
    LpmClosedError,
    LpmDeleteError,
    LpmInsertError,
    LpmInvalidPrefixError,
)


__all__ = [
    "LpmTableIPv4",
    "LpmTableIPv6",
    "Algorithm",
    "get_version",
]


class Algorithm(str, Enum):
    """Available LPM algorithms.
    
    Attributes:
        DIR24: IPv4 DIR-24-8 algorithm (recommended for IPv4)
        WIDE16: IPv6 wide 16-bit stride algorithm (recommended for IPv6)
        STRIDE8: 8-bit stride algorithm (works for both IPv4 and IPv6)
    """
    
    DIR24 = "dir24"
    WIDE16 = "wide16"
    STRIDE8 = "stride8"


class LpmTableIPv4:
    """IPv4 Longest Prefix Match routing table.
    
    This class provides a Pythonic interface to the liblpm C library for
    IPv4 routing table operations. It integrates with Python's ipaddress
    module and supports the context manager protocol for automatic
    resource cleanup.
    
    The default algorithm is DIR-24-8, which provides optimal IPv4
    lookup performance with 1-2 memory accesses per lookup.
    
    Args:
        algorithm: The algorithm to use. Either Algorithm.DIR24 (default),
            Algorithm.STRIDE8, or a string ('dir24', 'stride8').
    
    Example:
        >>> with LpmTableIPv4() as table:
        ...     # Insert routes
        ...     table.insert(IPv4Network('192.168.0.0/16'), 100)
        ...     table.insert(IPv4Network('192.168.1.0/24'), 101)
        ...     
        ...     # Lookup - returns most specific match
        ...     nh = table.lookup(IPv4Address('192.168.1.1'))
        ...     assert nh == 101  # /24 is more specific than /16
        
        >>> # Or without context manager
        >>> table = LpmTableIPv4()
        >>> try:
        ...     table.insert(IPv4Network('10.0.0.0/8'), 200)
        ...     nh = table.lookup(IPv4Address('10.1.2.3'))
        ... finally:
        ...     table.close()
    
    Note:
        The table is not thread-safe. For concurrent access, use external
        synchronization (e.g., threading.RLock).
    """
    
    __slots__ = ('_trie', '_closed')
    
    def __init__(self, algorithm: Union[Algorithm, str] = Algorithm.DIR24):
        """Initialize a new IPv4 routing table."""
        if isinstance(algorithm, Algorithm):
            algo_str = algorithm.value
        else:
            algo_str = str(algorithm)
        
        if algo_str not in ('dir24', 'stride8'):
            raise ValueError(
                f"Invalid IPv4 algorithm: {algo_str}. "
                f"Must be 'dir24' or 'stride8'."
            )
        
        self._trie = _LpmTrieIPv4(algo_str)
        self._closed = False
    
    def __enter__(self) -> LpmTableIPv4:
        """Enter context manager."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Exit context manager and clean up resources."""
        self.close()
        return False
    
    def __del__(self):
        """Destructor - ensure resources are cleaned up."""
        if hasattr(self, '_trie') and not self._closed:
            self.close()
    
    def insert(
        self,
        prefix: Union[IPv4Network, str],
        next_hop: int
    ) -> None:
        """Insert a route into the table.
        
        Args:
            prefix: The network prefix (IPv4Network or CIDR string)
            next_hop: The next hop value (32-bit unsigned integer)
        
        Raises:
            LpmClosedError: If the table has been closed
            LpmInsertError: If the insert operation fails
            LpmInvalidPrefixError: If the prefix is invalid
            ValueError: If next_hop is out of range
        
        Example:
            >>> table.insert(IPv4Network('192.168.0.0/16'), 100)
            >>> table.insert('10.0.0.0/8', 200)  # String also works
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        # Validate next_hop
        if not (0 <= next_hop <= 0xFFFFFFFF):
            raise ValueError(
                f"next_hop must be 0-4294967295, got {next_hop}"
            )
        
        # Convert string to IPv4Network if needed
        if isinstance(prefix, str):
            try:
                prefix = IPv4Network(prefix, strict=False)
            except ValueError as e:
                raise LpmInvalidPrefixError(
                    prefix=prefix,
                    reason=str(e)
                ) from e
        
        # Validate prefix type
        if not isinstance(prefix, IPv4Network):
            raise LpmInvalidPrefixError(
                reason=f"Expected IPv4Network, got {type(prefix).__name__}"
            )
        
        # Get prefix bytes and length
        prefix_bytes = prefix.network_address.packed
        prefix_len = prefix.prefixlen
        
        # Call C library
        result = self._trie.add(prefix_bytes, prefix_len, next_hop)
        if result != 0:
            raise LpmInsertError(
                prefix=str(prefix),
                next_hop=next_hop
            )
    
    def delete(self, prefix: Union[IPv4Network, str]) -> None:
        """Delete a route from the table.
        
        Args:
            prefix: The network prefix to delete (IPv4Network or CIDR string)
        
        Raises:
            LpmClosedError: If the table has been closed
            LpmDeleteError: If the delete operation fails
            LpmInvalidPrefixError: If the prefix is invalid
        
        Example:
            >>> table.delete(IPv4Network('192.168.0.0/16'))
            >>> table.delete('10.0.0.0/8')  # String also works
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        # Convert string to IPv4Network if needed
        if isinstance(prefix, str):
            try:
                prefix = IPv4Network(prefix, strict=False)
            except ValueError as e:
                raise LpmInvalidPrefixError(
                    prefix=prefix,
                    reason=str(e)
                ) from e
        
        # Validate prefix type
        if not isinstance(prefix, IPv4Network):
            raise LpmInvalidPrefixError(
                reason=f"Expected IPv4Network, got {type(prefix).__name__}"
            )
        
        # Get prefix bytes and length
        prefix_bytes = prefix.network_address.packed
        prefix_len = prefix.prefixlen
        
        # Call C library
        result = self._trie.delete(prefix_bytes, prefix_len)
        if result != 0:
            raise LpmDeleteError(prefix=str(prefix))
    
    def lookup(self, addr: Union[IPv4Address, str]) -> Optional[int]:
        """Look up an address in the table.
        
        Performs a longest prefix match lookup for the given address
        and returns the associated next hop value.
        
        Args:
            addr: The IPv4 address to look up (IPv4Address or string)
        
        Returns:
            The next hop value for the longest matching prefix, or None
            if no matching prefix is found.
        
        Raises:
            LpmClosedError: If the table has been closed
        
        Example:
            >>> table.insert(IPv4Network('192.168.0.0/16'), 100)
            >>> table.lookup(IPv4Address('192.168.1.1'))
            100
            >>> table.lookup(IPv4Address('8.8.8.8'))
            None
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        # Convert string to IPv4Address if needed
        if isinstance(addr, str):
            addr = IPv4Address(addr)
        
        # Convert to 32-bit integer (big-endian/network byte order)
        addr_int = int.from_bytes(addr.packed, 'big')
        
        # Perform lookup
        result = self._trie.lookup(addr_int)
        
        # Convert INVALID_NEXT_HOP to None
        return result if result != INVALID_NEXT_HOP else None
    
    def lookup_batch(
        self,
        addrs: Sequence[Union[IPv4Address, str]]
    ) -> List[Optional[int]]:
        """Look up multiple addresses in a single batch.
        
        This is more efficient than calling lookup() multiple times,
        especially for large batches, as it reduces the overhead of
        crossing the Python/C boundary.
        
        Args:
            addrs: Sequence of IPv4 addresses to look up
        
        Returns:
            List of next hop values (None for addresses with no match)
        
        Raises:
            LpmClosedError: If the table has been closed
        
        Example:
            >>> addrs = [IPv4Address('192.168.1.1'), IPv4Address('10.0.0.1')]
            >>> results = table.lookup_batch(addrs)
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        if not addrs:
            return []
        
        # Convert addresses to uint32 array
        addr_array = array.array('I')  # Unsigned int (4 bytes)
        for addr in addrs:
            if isinstance(addr, str):
                addr = IPv4Address(addr)
            # Convert to big-endian uint32
            addr_int = int.from_bytes(addr.packed, 'big')
            addr_array.append(addr_int)
        
        # Create results array
        result_array = array.array('I', [0] * len(addrs))
        
        # Perform batch lookup (pass memoryview directly, no cast needed)
        self._trie.lookup_batch(
            memoryview(addr_array),
            memoryview(result_array)
        )
        
        # Convert results, replacing INVALID_NEXT_HOP with None
        return [
            r if r != INVALID_NEXT_HOP else None
            for r in result_array
        ]
    
    def close(self) -> None:
        """Close the table and release all resources.
        
        After calling close(), no further operations can be performed
        on the table. It is safe to call close() multiple times.
        
        Note:
            If using the context manager protocol (with statement),
            this is called automatically when exiting the context.
        """
        if not self._closed:
            self._trie.close()
            self._closed = True
    
    @property
    def closed(self) -> bool:
        """Check if the table is closed.
        
        Returns:
            True if the table has been closed, False otherwise.
        """
        return self._closed
    
    @property
    def algorithm(self) -> str:
        """Get the algorithm used by this table.
        
        Returns:
            Algorithm name ('dir24' or 'stride8')
        """
        return self._trie.algorithm
    
    @property
    def num_prefixes(self) -> int:
        """Get the number of prefixes in the table.
        
        Returns:
            Number of prefixes currently stored
        """
        return self._trie.num_prefixes
    
    @property
    def num_nodes(self) -> int:
        """Get the number of trie nodes.
        
        Returns:
            Number of nodes in the underlying trie structure
        """
        return self._trie.num_nodes
    
    def __repr__(self) -> str:
        """Return a string representation of the table."""
        status = "closed" if self._closed else "open"
        return (
            f"LpmTableIPv4(algorithm={self.algorithm!r}, "
            f"prefixes={self.num_prefixes}, status={status!r})"
        )


class LpmTableIPv6:
    """IPv6 Longest Prefix Match routing table.
    
    This class provides a Pythonic interface to the liblpm C library for
    IPv6 routing table operations. It integrates with Python's ipaddress
    module and supports the context manager protocol for automatic
    resource cleanup.
    
    The default algorithm is Wide16, which uses a 16-bit stride for the
    first level and 8-bit strides for remaining levels, optimized for
    common /48 allocations.
    
    Args:
        algorithm: The algorithm to use. Either Algorithm.WIDE16 (default),
            Algorithm.STRIDE8, or a string ('wide16', 'stride8').
    
    Example:
        >>> with LpmTableIPv6() as table:
        ...     table.insert(IPv6Network('2001:db8::/32'), 100)
        ...     table.insert(IPv6Network('2001:db8:1::/48'), 101)
        ...     
        ...     nh = table.lookup(IPv6Address('2001:db8:1::1'))
        ...     assert nh == 101
    
    Note:
        The table is not thread-safe. For concurrent access, use external
        synchronization (e.g., threading.RLock).
    """
    
    __slots__ = ('_trie', '_closed')
    
    def __init__(self, algorithm: Union[Algorithm, str] = Algorithm.WIDE16):
        """Initialize a new IPv6 routing table."""
        if isinstance(algorithm, Algorithm):
            algo_str = algorithm.value
        else:
            algo_str = str(algorithm)
        
        if algo_str not in ('wide16', 'stride8'):
            raise ValueError(
                f"Invalid IPv6 algorithm: {algo_str}. "
                f"Must be 'wide16' or 'stride8'."
            )
        
        self._trie = _LpmTrieIPv6(algo_str)
        self._closed = False
    
    def __enter__(self) -> LpmTableIPv6:
        """Enter context manager."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        """Exit context manager and clean up resources."""
        self.close()
        return False
    
    def __del__(self):
        """Destructor - ensure resources are cleaned up."""
        if hasattr(self, '_trie') and not self._closed:
            self.close()
    
    def insert(
        self,
        prefix: Union[IPv6Network, str],
        next_hop: int
    ) -> None:
        """Insert a route into the table.
        
        Args:
            prefix: The network prefix (IPv6Network or CIDR string)
            next_hop: The next hop value (32-bit unsigned integer)
        
        Raises:
            LpmClosedError: If the table has been closed
            LpmInsertError: If the insert operation fails
            LpmInvalidPrefixError: If the prefix is invalid
            ValueError: If next_hop is out of range
        
        Example:
            >>> table.insert(IPv6Network('2001:db8::/32'), 100)
            >>> table.insert('fd00::/8', 200)  # String also works
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        # Validate next_hop
        if not (0 <= next_hop <= 0xFFFFFFFF):
            raise ValueError(
                f"next_hop must be 0-4294967295, got {next_hop}"
            )
        
        # Convert string to IPv6Network if needed
        if isinstance(prefix, str):
            try:
                prefix = IPv6Network(prefix, strict=False)
            except ValueError as e:
                raise LpmInvalidPrefixError(
                    prefix=prefix,
                    reason=str(e)
                ) from e
        
        # Validate prefix type
        if not isinstance(prefix, IPv6Network):
            raise LpmInvalidPrefixError(
                reason=f"Expected IPv6Network, got {type(prefix).__name__}"
            )
        
        # Get prefix bytes and length
        prefix_bytes = prefix.network_address.packed
        prefix_len = prefix.prefixlen
        
        # Call C library
        result = self._trie.add(prefix_bytes, prefix_len, next_hop)
        if result != 0:
            raise LpmInsertError(
                prefix=str(prefix),
                next_hop=next_hop
            )
    
    def delete(self, prefix: Union[IPv6Network, str]) -> None:
        """Delete a route from the table.
        
        Args:
            prefix: The network prefix to delete (IPv6Network or CIDR string)
        
        Raises:
            LpmClosedError: If the table has been closed
            LpmDeleteError: If the delete operation fails
            LpmInvalidPrefixError: If the prefix is invalid
        
        Example:
            >>> table.delete(IPv6Network('2001:db8::/32'))
            >>> table.delete('fd00::/8')  # String also works
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        # Convert string to IPv6Network if needed
        if isinstance(prefix, str):
            try:
                prefix = IPv6Network(prefix, strict=False)
            except ValueError as e:
                raise LpmInvalidPrefixError(
                    prefix=prefix,
                    reason=str(e)
                ) from e
        
        # Validate prefix type
        if not isinstance(prefix, IPv6Network):
            raise LpmInvalidPrefixError(
                reason=f"Expected IPv6Network, got {type(prefix).__name__}"
            )
        
        # Get prefix bytes and length
        prefix_bytes = prefix.network_address.packed
        prefix_len = prefix.prefixlen
        
        # Call C library
        result = self._trie.delete(prefix_bytes, prefix_len)
        if result != 0:
            raise LpmDeleteError(prefix=str(prefix))
    
    def lookup(self, addr: Union[IPv6Address, str]) -> Optional[int]:
        """Look up an address in the table.
        
        Performs a longest prefix match lookup for the given address
        and returns the associated next hop value.
        
        Args:
            addr: The IPv6 address to look up (IPv6Address or string)
        
        Returns:
            The next hop value for the longest matching prefix, or None
            if no matching prefix is found.
        
        Raises:
            LpmClosedError: If the table has been closed
        
        Example:
            >>> table.insert(IPv6Network('2001:db8::/32'), 100)
            >>> table.lookup(IPv6Address('2001:db8::1'))
            100
            >>> table.lookup(IPv6Address('2001:db9::1'))
            None
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        # Convert string to IPv6Address if needed
        if isinstance(addr, str):
            addr = IPv6Address(addr)
        
        # Get 16-byte packed address
        addr_bytes = addr.packed
        
        # Perform lookup
        result = self._trie.lookup(addr_bytes)
        
        # Convert INVALID_NEXT_HOP to None
        return result if result != INVALID_NEXT_HOP else None
    
    def lookup_batch(
        self,
        addrs: Sequence[Union[IPv6Address, str]]
    ) -> List[Optional[int]]:
        """Look up multiple addresses in a single batch.
        
        This is more efficient than calling lookup() multiple times,
        especially for large batches, as it reduces the overhead of
        crossing the Python/C boundary.
        
        Args:
            addrs: Sequence of IPv6 addresses to look up
        
        Returns:
            List of next hop values (None for addresses with no match)
        
        Raises:
            LpmClosedError: If the table has been closed
        
        Example:
            >>> addrs = [IPv6Address('2001:db8::1'), IPv6Address('fd00::1')]
            >>> results = table.lookup_batch(addrs)
        """
        if self._closed:
            raise LpmClosedError("Table is closed")
        
        if not addrs:
            return []
        
        # Convert addresses to list of bytes
        addr_list = []
        for addr in addrs:
            if isinstance(addr, str):
                addr = IPv6Address(addr)
            addr_list.append(addr.packed)
        
        # Create results array
        result_array = array.array('I', [0] * len(addrs))
        
        # Perform batch lookup (pass memoryview directly, no cast needed)
        self._trie.lookup_batch(
            addr_list,
            memoryview(result_array)
        )
        
        # Convert results, replacing INVALID_NEXT_HOP with None
        return [
            r if r != INVALID_NEXT_HOP else None
            for r in result_array
        ]
    
    def close(self) -> None:
        """Close the table and release all resources.
        
        After calling close(), no further operations can be performed
        on the table. It is safe to call close() multiple times.
        
        Note:
            If using the context manager protocol (with statement),
            this is called automatically when exiting the context.
        """
        if not self._closed:
            self._trie.close()
            self._closed = True
    
    @property
    def closed(self) -> bool:
        """Check if the table is closed.
        
        Returns:
            True if the table has been closed, False otherwise.
        """
        return self._closed
    
    @property
    def algorithm(self) -> str:
        """Get the algorithm used by this table.
        
        Returns:
            Algorithm name ('wide16' or 'stride8')
        """
        return self._trie.algorithm
    
    @property
    def num_prefixes(self) -> int:
        """Get the number of prefixes in the table.
        
        Returns:
            Number of prefixes currently stored
        """
        return self._trie.num_prefixes
    
    @property
    def num_nodes(self) -> int:
        """Get the number of trie nodes.
        
        Returns:
            Number of nodes in the underlying trie structure
        """
        return self._trie.num_nodes
    
    @property
    def num_wide_nodes(self) -> int:
        """Get the number of wide (16-bit stride) nodes.
        
        Returns:
            Number of wide nodes (only for wide16 algorithm)
        """
        return self._trie.num_wide_nodes
    
    def __repr__(self) -> str:
        """Return a string representation of the table."""
        status = "closed" if self._closed else "open"
        return (
            f"LpmTableIPv6(algorithm={self.algorithm!r}, "
            f"prefixes={self.num_prefixes}, status={status!r})"
        )

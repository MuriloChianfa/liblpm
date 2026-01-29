"""liblpm - High-performance Longest Prefix Match library for Python.

This module provides Python bindings for the liblpm C library, enabling
fast IPv4 and IPv6 longest prefix match (LPM) routing table operations.

Example:
    >>> from liblpm import LpmTableIPv4
    >>> from ipaddress import IPv4Address, IPv4Network
    >>> 
    >>> with LpmTableIPv4() as table:
    ...     table.insert(IPv4Network('192.168.0.0/16'), 100)
    ...     next_hop = table.lookup(IPv4Address('192.168.1.1'))
    ...     print(f"Next hop: {next_hop}")
    Next hop: 100

Features:
    - High Performance: Direct C bindings via Cython with minimal overhead
    - IPv4 DIR-24-8: Optimized IPv4 lookups using DPDK-style algorithm
    - IPv6 Wide Stride: Efficient IPv6 lookups with 16-bit first-level stride
    - Batch Operations: Reduced overhead through batch lookup operations
    - Pythonic API: Clean interface using ipaddress module types
    - Type Hints: Full mypy support for IDE integration
    - Context Manager: Automatic resource cleanup with 'with' statement
"""

__version__ = "2.0.0"
__author__ = "Murilo Chianfa"
__email__ = "murilo@made4it.com.br"
__license__ = "BSL-1.0"

from .table import (
    LpmTableIPv4,
    LpmTableIPv6,
    Algorithm,
)
from .exceptions import (
    LpmError,
    LpmInsertError,
    LpmDeleteError,
    LpmClosedError,
    LpmInvalidPrefixError,
)

# Re-export constants
INVALID_NEXT_HOP = 0xFFFFFFFF

__all__ = [
    # Version info
    "__version__",
    "__author__",
    "__email__",
    "__license__",
    # Main classes
    "LpmTableIPv4",
    "LpmTableIPv6",
    "Algorithm",
    # Exceptions
    "LpmError",
    "LpmInsertError",
    "LpmDeleteError",
    "LpmClosedError",
    "LpmInvalidPrefixError",
    # Constants
    "INVALID_NEXT_HOP",
]

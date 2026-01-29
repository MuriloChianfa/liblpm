"""Exception classes for liblpm Python bindings.

This module defines a hierarchy of exceptions that can be raised by
the liblpm library operations.

Exception Hierarchy:
    LpmError (base)
    ├── LpmInsertError - Failed to insert prefix
    ├── LpmDeleteError - Failed to delete prefix
    ├── LpmClosedError - Operation on closed table
    └── LpmInvalidPrefixError - Invalid prefix format
"""

from typing import Optional


class LpmError(Exception):
    """Base exception for all liblpm errors.
    
    All exceptions raised by the liblpm library inherit from this class,
    making it easy to catch any liblpm-related error.
    
    Example:
        >>> try:
        ...     table.insert(invalid_prefix, 100)
        ... except LpmError as e:
        ...     print(f"LPM operation failed: {e}")
    """
    
    def __init__(self, message: str = "An LPM operation failed"):
        self.message = message
        super().__init__(self.message)


class LpmInsertError(LpmError):
    """Exception raised when inserting a prefix into the table fails.
    
    This can occur when:
    - Memory allocation fails during trie expansion
    - The prefix is invalid for the table type (e.g., IPv6 prefix in IPv4 table)
    - Internal trie operation fails
    
    Attributes:
        prefix: The prefix that failed to insert (if available)
        next_hop: The next hop value that was being inserted (if available)
    """
    
    def __init__(
        self,
        message: str = "Failed to insert prefix",
        prefix: Optional[str] = None,
        next_hop: Optional[int] = None
    ):
        self.prefix = prefix
        self.next_hop = next_hop
        
        if prefix is not None:
            message = f"{message}: {prefix}"
        if next_hop is not None:
            message = f"{message} -> {next_hop}"
            
        super().__init__(message)


class LpmDeleteError(LpmError):
    """Exception raised when deleting a prefix from the table fails.
    
    This can occur when:
    - The prefix does not exist in the table
    - The prefix is invalid for the table type
    - Internal trie operation fails
    
    Attributes:
        prefix: The prefix that failed to delete (if available)
    """
    
    def __init__(
        self,
        message: str = "Failed to delete prefix",
        prefix: Optional[str] = None
    ):
        self.prefix = prefix
        
        if prefix is not None:
            message = f"{message}: {prefix}"
            
        super().__init__(message)


class LpmClosedError(LpmError):
    """Exception raised when operating on a closed table.
    
    Once a table is closed (either explicitly via close() or automatically
    when exiting a context manager), no further operations can be performed.
    
    Example:
        >>> table = LpmTableIPv4()
        >>> table.close()
        >>> table.lookup(addr)  # Raises LpmClosedError
    """
    
    def __init__(self, message: str = "Operation on closed table"):
        super().__init__(message)


class LpmInvalidPrefixError(LpmError):
    """Exception raised when a prefix has an invalid format.
    
    This can occur when:
    - The prefix length exceeds the maximum for the address type
      (32 for IPv4, 128 for IPv6)
    - The network address has host bits set
    - The prefix string cannot be parsed
    
    Attributes:
        prefix: The invalid prefix (if available)
        reason: Additional details about why the prefix is invalid
    """
    
    def __init__(
        self,
        message: str = "Invalid prefix",
        prefix: Optional[str] = None,
        reason: Optional[str] = None
    ):
        self.prefix = prefix
        self.reason = reason
        
        if prefix is not None:
            message = f"{message}: {prefix}"
        if reason is not None:
            message = f"{message} ({reason})"
            
        super().__init__(message)


class LpmMemoryError(LpmError):
    """Exception raised when memory allocation fails.
    
    This typically occurs when:
    - The system is out of memory
    - The trie has grown too large
    - Huge page allocation fails
    """
    
    def __init__(self, message: str = "Memory allocation failed"):
        super().__init__(message)

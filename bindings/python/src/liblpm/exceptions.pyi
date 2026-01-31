"""Type stubs for liblpm.exceptions module."""

from typing import Optional

class LpmError(Exception):
    """Base exception for all liblpm errors."""
    
    message: str
    
    def __init__(self, message: str = ...) -> None: ...

class LpmInsertError(LpmError):
    """Exception raised when inserting a prefix into the table fails."""
    
    prefix: Optional[str]
    next_hop: Optional[int]
    
    def __init__(
        self,
        message: str = ...,
        prefix: Optional[str] = None,
        next_hop: Optional[int] = None,
    ) -> None: ...

class LpmDeleteError(LpmError):
    """Exception raised when deleting a prefix from the table fails."""
    
    prefix: Optional[str]
    
    def __init__(
        self,
        message: str = ...,
        prefix: Optional[str] = None,
    ) -> None: ...

class LpmClosedError(LpmError):
    """Exception raised when operating on a closed table."""
    
    def __init__(self, message: str = ...) -> None: ...

class LpmInvalidPrefixError(LpmError):
    """Exception raised when a prefix has an invalid format."""
    
    prefix: Optional[str]
    reason: Optional[str]
    
    def __init__(
        self,
        message: str = ...,
        prefix: Optional[str] = None,
        reason: Optional[str] = None,
    ) -> None: ...

class LpmMemoryError(LpmError):
    """Exception raised when memory allocation fails."""
    
    def __init__(self, message: str = ...) -> None: ...

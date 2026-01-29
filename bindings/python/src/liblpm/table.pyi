"""Type stubs for liblpm.table module."""

from enum import Enum
from ipaddress import IPv4Address, IPv4Network, IPv6Address, IPv6Network
from types import TracebackType
from typing import Literal, Optional, Sequence, Type, Union

class Algorithm(str, Enum):
    """Available LPM algorithms."""
    
    DIR24: str
    WIDE16: str
    STRIDE8: str

class LpmTableIPv4:
    """IPv4 Longest Prefix Match routing table."""
    
    def __init__(
        self, 
        algorithm: Union[Algorithm, Literal['dir24', 'stride8']] = Algorithm.DIR24
    ) -> None: ...
    
    def __enter__(self) -> LpmTableIPv4: ...
    
    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> bool: ...
    
    def insert(
        self,
        prefix: Union[IPv4Network, str],
        next_hop: int,
    ) -> None: ...
    
    def delete(self, prefix: Union[IPv4Network, str]) -> None: ...
    
    def lookup(self, addr: Union[IPv4Address, str]) -> Optional[int]: ...
    
    def lookup_batch(
        self,
        addrs: Sequence[Union[IPv4Address, str]],
    ) -> list[Optional[int]]: ...
    
    def close(self) -> None: ...
    
    @property
    def closed(self) -> bool: ...
    
    @property
    def algorithm(self) -> str: ...
    
    @property
    def num_prefixes(self) -> int: ...
    
    @property
    def num_nodes(self) -> int: ...
    
    def __repr__(self) -> str: ...

class LpmTableIPv6:
    """IPv6 Longest Prefix Match routing table."""
    
    def __init__(
        self,
        algorithm: Union[Algorithm, Literal['wide16', 'stride8']] = Algorithm.WIDE16
    ) -> None: ...
    
    def __enter__(self) -> LpmTableIPv6: ...
    
    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> bool: ...
    
    def insert(
        self,
        prefix: Union[IPv6Network, str],
        next_hop: int,
    ) -> None: ...
    
    def delete(self, prefix: Union[IPv6Network, str]) -> None: ...
    
    def lookup(self, addr: Union[IPv6Address, str]) -> Optional[int]: ...
    
    def lookup_batch(
        self,
        addrs: Sequence[Union[IPv6Address, str]],
    ) -> list[Optional[int]]: ...
    
    def close(self) -> None: ...
    
    @property
    def closed(self) -> bool: ...
    
    @property
    def algorithm(self) -> str: ...
    
    @property
    def num_prefixes(self) -> int: ...
    
    @property
    def num_nodes(self) -> int: ...
    
    @property
    def num_wide_nodes(self) -> int: ...
    
    def __repr__(self) -> str: ...

def get_version() -> str: ...

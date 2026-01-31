"""Type stubs for liblpm package."""

from typing import Literal

from .table import LpmTableIPv4 as LpmTableIPv4
from .table import LpmTableIPv6 as LpmTableIPv6
from .table import Algorithm as Algorithm
from .exceptions import LpmError as LpmError
from .exceptions import LpmInsertError as LpmInsertError
from .exceptions import LpmDeleteError as LpmDeleteError
from .exceptions import LpmClosedError as LpmClosedError
from .exceptions import LpmInvalidPrefixError as LpmInvalidPrefixError

__version__: str
__author__: str
__email__: str
__license__: str

INVALID_NEXT_HOP: Literal[0xFFFFFFFF]

__all__: list[str]

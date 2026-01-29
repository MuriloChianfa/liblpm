# cython: language_level=3
# cython: boundscheck=False
# cython: wraparound=False
# cython: cdivision=True
"""Cython implementation of liblpm Python bindings.

This module provides low-level wrapper classes for the liblpm C library.
These classes handle memory management and provide the bridge between
Python and the C library. The high-level Pythonic API in table.py
builds on top of these classes.

Note:
    This module is not intended for direct use. Use the classes in
    liblpm.table instead, which provide a more Pythonic interface.
"""

from libc.stdlib cimport malloc, free
from libc.string cimport memcpy, memset
from libc.stdint cimport uint8_t, uint32_t, uint64_t
from cpython.mem cimport PyMem_Malloc, PyMem_Free
from cpython.bytes cimport PyBytes_AS_STRING

cimport _liblpm


# Re-export constant for Python access
INVALID_NEXT_HOP = 0xFFFFFFFF


cdef class _LpmTrieIPv4:
    """Low-level wrapper for IPv4 LPM trie.
    
    This class provides direct access to the C library functions with
    minimal overhead. Memory management is handled automatically via
    __dealloc__.
    
    Attributes:
        _trie: Pointer to the underlying C lpm_trie_t structure
        _closed: Boolean indicating if the trie has been closed
        _algorithm: String indicating which algorithm was used
    """
    
    cdef _liblpm.lpm_trie_t* _trie
    cdef bint _closed
    cdef str _algorithm
    
    def __cinit__(self, str algorithm='dir24'):
        """Initialize the IPv4 trie with the specified algorithm.
        
        Args:
            algorithm: 'dir24' (default) or 'stride8'
        """
        self._trie = NULL
        self._closed = False
        self._algorithm = algorithm
        
        if algorithm == 'dir24':
            with nogil:
                self._trie = _liblpm.lpm_create_ipv4_dir24()
        elif algorithm == 'stride8':
            with nogil:
                self._trie = _liblpm.lpm_create_ipv4_8stride()
        else:
            # Default to generic IPv4 (compile-time selected)
            with nogil:
                self._trie = _liblpm.lpm_create_ipv4()
        
        if self._trie is NULL:
            raise MemoryError("Failed to create IPv4 LPM trie")
    
    def __dealloc__(self):
        """Clean up C resources when the object is garbage collected."""
        if self._trie is not NULL and not self._closed:
            _liblpm.lpm_destroy(self._trie)
            self._trie = NULL
    
    cpdef void close(self) except *:
        """Explicitly close the trie and free resources.
        
        After calling close(), no further operations can be performed.
        It is safe to call close() multiple times.
        """
        if not self._closed and self._trie is not NULL:
            _liblpm.lpm_destroy(self._trie)
            self._trie = NULL
            self._closed = True
    
    cpdef int add(self, bytes prefix, uint8_t prefix_len, 
                  uint32_t next_hop) except -1:
        """Add a prefix to the trie.
        
        Args:
            prefix: 4-byte prefix address in network byte order
            prefix_len: Prefix length (0-32)
            next_hop: Next hop value (32-bit unsigned integer)
        
        Returns:
            0 on success, non-zero on failure
        
        Raises:
            RuntimeError: If the trie is closed
        """
        cdef int result
        cdef const uint8_t* prefix_ptr
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        if len(prefix) < 4:
            raise ValueError("IPv4 prefix must be 4 bytes")
        
        prefix_ptr = <const uint8_t*>PyBytes_AS_STRING(prefix)
        
        with nogil:
            result = _liblpm.lpm_add(self._trie, prefix_ptr, 
                                      prefix_len, next_hop)
        
        return result
    
    cpdef int delete(self, bytes prefix, uint8_t prefix_len) except -1:
        """Delete a prefix from the trie.
        
        Args:
            prefix: 4-byte prefix address in network byte order
            prefix_len: Prefix length (0-32)
        
        Returns:
            0 on success, non-zero on failure
        
        Raises:
            RuntimeError: If the trie is closed
        """
        cdef int result
        cdef const uint8_t* prefix_ptr
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        if len(prefix) < 4:
            raise ValueError("IPv4 prefix must be 4 bytes")
        
        prefix_ptr = <const uint8_t*>PyBytes_AS_STRING(prefix)
        
        with nogil:
            result = _liblpm.lpm_delete(self._trie, prefix_ptr, prefix_len)
        
        return result
    
    cpdef uint32_t lookup(self, uint32_t addr):
        """Perform a single IPv4 lookup.
        
        Args:
            addr: IPv4 address as a 32-bit unsigned integer in host byte order
                  (big-endian network representation)
        
        Returns:
            Next hop value, or INVALID_NEXT_HOP (0xFFFFFFFF) if no match
        
        Raises:
            RuntimeError: If the trie is closed
        """
        cdef uint32_t result
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        with nogil:
            result = _liblpm.lpm_lookup_ipv4(self._trie, addr)
        
        return result
    
    cpdef lookup_batch(self, uint32_t[::1] addrs, uint32_t[::1] results):
        """Perform batch IPv4 lookups.
        
        Args:
            addrs: Memory view of uint32 addresses (host byte order)
            results: Memory view to store results (must be same size as addrs)
        
        Raises:
            RuntimeError: If the trie is closed
            ValueError: If results buffer is too small
        """
        cdef size_t count
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        count = len(addrs)
        if len(results) < count:
            raise ValueError("Results buffer too small")
        
        with nogil:
            _liblpm.lpm_lookup_batch_ipv4(self._trie, &addrs[0], 
                                           &results[0], count)
    
    @property
    def closed(self) -> bool:
        """Check if the trie is closed."""
        return self._closed
    
    @property
    def algorithm(self) -> str:
        """Get the algorithm used for this trie."""
        return self._algorithm
    
    @property
    def num_prefixes(self) -> int:
        """Get the number of prefixes in the trie."""
        if self._closed or self._trie is NULL:
            return 0
        return self._trie.num_prefixes
    
    @property
    def num_nodes(self) -> int:
        """Get the number of nodes in the trie."""
        if self._closed or self._trie is NULL:
            return 0
        return self._trie.num_nodes


cdef class _LpmTrieIPv6:
    """Low-level wrapper for IPv6 LPM trie.
    
    This class provides direct access to the C library functions with
    minimal overhead. Memory management is handled automatically via
    __dealloc__.
    
    Attributes:
        _trie: Pointer to the underlying C lpm_trie_t structure
        _closed: Boolean indicating if the trie has been closed
        _algorithm: String indicating which algorithm was used
    """
    
    cdef _liblpm.lpm_trie_t* _trie
    cdef bint _closed
    cdef str _algorithm
    
    def __cinit__(self, str algorithm='wide16'):
        """Initialize the IPv6 trie with the specified algorithm.
        
        Args:
            algorithm: 'wide16' (default) or 'stride8'
        """
        self._trie = NULL
        self._closed = False
        self._algorithm = algorithm
        
        if algorithm == 'wide16':
            with nogil:
                self._trie = _liblpm.lpm_create_ipv6_wide16()
        elif algorithm == 'stride8':
            with nogil:
                self._trie = _liblpm.lpm_create_ipv6_8stride()
        else:
            # Default to generic IPv6 (compile-time selected)
            with nogil:
                self._trie = _liblpm.lpm_create_ipv6()
        
        if self._trie is NULL:
            raise MemoryError("Failed to create IPv6 LPM trie")
    
    def __dealloc__(self):
        """Clean up C resources when the object is garbage collected."""
        if self._trie is not NULL and not self._closed:
            _liblpm.lpm_destroy(self._trie)
            self._trie = NULL
    
    cpdef void close(self) except *:
        """Explicitly close the trie and free resources.
        
        After calling close(), no further operations can be performed.
        It is safe to call close() multiple times.
        """
        if not self._closed and self._trie is not NULL:
            _liblpm.lpm_destroy(self._trie)
            self._trie = NULL
            self._closed = True
    
    cpdef int add(self, bytes prefix, uint8_t prefix_len,
                  uint32_t next_hop) except -1:
        """Add a prefix to the trie.
        
        Args:
            prefix: 16-byte prefix address in network byte order
            prefix_len: Prefix length (0-128)
            next_hop: Next hop value (32-bit unsigned integer)
        
        Returns:
            0 on success, non-zero on failure
        
        Raises:
            RuntimeError: If the trie is closed
        """
        cdef int result
        cdef const uint8_t* prefix_ptr
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        if len(prefix) < 16:
            raise ValueError("IPv6 prefix must be 16 bytes")
        
        prefix_ptr = <const uint8_t*>PyBytes_AS_STRING(prefix)
        
        with nogil:
            result = _liblpm.lpm_add(self._trie, prefix_ptr,
                                      prefix_len, next_hop)
        
        return result
    
    cpdef int delete(self, bytes prefix, uint8_t prefix_len) except -1:
        """Delete a prefix from the trie.
        
        Args:
            prefix: 16-byte prefix address in network byte order
            prefix_len: Prefix length (0-128)
        
        Returns:
            0 on success, non-zero on failure
        
        Raises:
            RuntimeError: If the trie is closed
        """
        cdef int result
        cdef const uint8_t* prefix_ptr
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        if len(prefix) < 16:
            raise ValueError("IPv6 prefix must be 16 bytes")
        
        prefix_ptr = <const uint8_t*>PyBytes_AS_STRING(prefix)
        
        with nogil:
            result = _liblpm.lpm_delete(self._trie, prefix_ptr, prefix_len)
        
        return result
    
    cpdef uint32_t lookup(self, bytes addr):
        """Perform a single IPv6 lookup.
        
        Args:
            addr: 16-byte IPv6 address in network byte order
        
        Returns:
            Next hop value, or INVALID_NEXT_HOP (0xFFFFFFFF) if no match
        
        Raises:
            RuntimeError: If the trie is closed
        """
        cdef uint32_t result
        cdef const uint8_t* addr_ptr
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        if len(addr) < 16:
            raise ValueError("IPv6 address must be 16 bytes")
        
        addr_ptr = <const uint8_t*>PyBytes_AS_STRING(addr)
        
        with nogil:
            result = _liblpm.lpm_lookup_ipv6(self._trie, addr_ptr)
        
        return result
    
    cpdef lookup_batch(self, list addrs, uint32_t[::1] results):
        """Perform batch IPv6 lookups.
        
        Args:
            addrs: List of 16-byte address buffers
            results: Memory view to store results (must be same size as addrs)
        
        Raises:
            RuntimeError: If the trie is closed
            ValueError: If results buffer is too small
        """
        cdef size_t count, i
        cdef uint8_t* batch_buffer
        cdef const uint8_t* addr_ptr
        cdef bytes addr
        
        if self._closed or self._trie is NULL:
            raise RuntimeError("Trie is closed")
        
        count = len(addrs)
        if len(results) < count:
            raise ValueError("Results buffer too small")
        
        if count == 0:
            return
        
        # Allocate contiguous buffer for batch operation
        batch_buffer = <uint8_t*>malloc(count * 16)
        if batch_buffer is NULL:
            raise MemoryError("Failed to allocate batch buffer")
        
        try:
            # Copy addresses to contiguous buffer
            for i in range(count):
                addr = addrs[i]
                if len(addr) < 16:
                    raise ValueError(f"Address {i} must be 16 bytes")
                addr_ptr = <const uint8_t*>PyBytes_AS_STRING(addr)
                memcpy(&batch_buffer[i * 16], addr_ptr, 16)
            
            # Perform batch lookup
            with nogil:
                _liblpm.lpm_lookup_batch_ipv6(
                    self._trie,
                    <const uint8_t (*)[16]>batch_buffer,
                    &results[0],
                    count
                )
        finally:
            free(batch_buffer)
    
    @property
    def closed(self) -> bool:
        """Check if the trie is closed."""
        return self._closed
    
    @property
    def algorithm(self) -> str:
        """Get the algorithm used for this trie."""
        return self._algorithm
    
    @property
    def num_prefixes(self) -> int:
        """Get the number of prefixes in the trie."""
        if self._closed or self._trie is NULL:
            return 0
        return self._trie.num_prefixes
    
    @property
    def num_nodes(self) -> int:
        """Get the number of nodes in the trie."""
        if self._closed or self._trie is NULL:
            return 0
        return self._trie.num_nodes
    
    @property
    def num_wide_nodes(self) -> int:
        """Get the number of wide (16-bit stride) nodes in the trie."""
        if self._closed or self._trie is NULL:
            return 0
        return self._trie.num_wide_nodes


def get_version() -> str:
    """Get the liblpm library version string.
    
    Returns:
        Version string (e.g., "2.0.0")
    """
    cdef const char* version
    version = _liblpm.lpm_get_version()
    if version is NULL:
        return "unknown"
    return version.decode('utf-8')

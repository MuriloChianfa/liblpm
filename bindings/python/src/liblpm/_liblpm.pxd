# cython: language_level=3
"""Cython declarations for liblpm C library.

This file contains the external C declarations for interfacing with
the liblpm library. These declarations mirror the public API defined
in lpm.h.
"""

from libc.stdint cimport uint8_t, uint32_t, uint64_t, int32_t
from libc.stddef cimport size_t


cdef extern from "lpm.h" nogil:
    # ========================================================================
    # Constants
    # ========================================================================
    
    cdef uint32_t LPM_INVALID_NEXT_HOP
    cdef uint8_t LPM_IPV4_MAX_DEPTH
    cdef uint8_t LPM_IPV6_MAX_DEPTH
    
    # ========================================================================
    # Opaque Types
    # ========================================================================
    
    # Main trie structure (opaque to Python)
    ctypedef struct lpm_trie_t:
        # We expose some fields for statistics (readonly)
        uint64_t num_prefixes
        uint64_t num_nodes
        uint64_t num_wide_nodes
        uint64_t cache_hits
        uint64_t cache_misses
        uint8_t max_depth
        bint has_default_route
    
    # ========================================================================
    # Generic API (dispatches to compile-time selected algorithm)
    # ========================================================================
    
    # Create/Destroy
    lpm_trie_t* lpm_create_ipv4()
    lpm_trie_t* lpm_create_ipv6()
    void lpm_destroy(lpm_trie_t* trie)
    
    # Add/Delete (generic)
    int lpm_add(lpm_trie_t* trie, const uint8_t* prefix, 
                uint8_t prefix_len, uint32_t next_hop)
    int lpm_delete(lpm_trie_t* trie, const uint8_t* prefix, 
                   uint8_t prefix_len)
    
    # Lookup (generic)
    uint32_t lpm_lookup_ipv4(const lpm_trie_t* trie, uint32_t addr)
    uint32_t lpm_lookup_ipv6(const lpm_trie_t* trie, const uint8_t* addr)
    
    # Batch lookup (generic)
    void lpm_lookup_batch_ipv4(const lpm_trie_t* trie, const uint32_t* addrs,
                               uint32_t* next_hops, size_t count)
    void lpm_lookup_batch_ipv6(const lpm_trie_t* trie, const uint8_t (*addrs)[16],
                               uint32_t* next_hops, size_t count)
    
    # ========================================================================
    # Algorithm-Specific API: IPv4 DIR-24-8
    # ========================================================================
    
    lpm_trie_t* lpm_create_ipv4_dir24()
    int lpm_add_ipv4_dir24(lpm_trie_t* trie, const uint8_t* prefix,
                           uint8_t prefix_len, uint32_t next_hop)
    int lpm_delete_ipv4_dir24(lpm_trie_t* trie, const uint8_t* prefix,
                              uint8_t prefix_len)
    uint32_t lpm_lookup_ipv4_dir24(const lpm_trie_t* trie, uint32_t addr)
    uint32_t lpm_lookup_ipv4_dir24_bytes(const lpm_trie_t* trie, 
                                          const uint8_t* addr)
    void lpm_lookup_batch_ipv4_dir24(const lpm_trie_t* trie, 
                                      const uint32_t* addrs,
                                      uint32_t* next_hops, size_t count)
    
    # ========================================================================
    # Algorithm-Specific API: IPv4 8-bit Stride
    # ========================================================================
    
    lpm_trie_t* lpm_create_ipv4_8stride()
    int lpm_add_ipv4_8stride(lpm_trie_t* trie, const uint8_t* prefix,
                             uint8_t prefix_len, uint32_t next_hop)
    int lpm_delete_ipv4_8stride(lpm_trie_t* trie, const uint8_t* prefix,
                                uint8_t prefix_len)
    uint32_t lpm_lookup_ipv4_8stride(const lpm_trie_t* trie, uint32_t addr)
    uint32_t lpm_lookup_ipv4_8stride_bytes(const lpm_trie_t* trie,
                                            const uint8_t* addr)
    void lpm_lookup_batch_ipv4_8stride(const lpm_trie_t* trie,
                                        const uint32_t* addrs,
                                        uint32_t* next_hops, size_t count)
    
    # ========================================================================
    # Algorithm-Specific API: IPv6 Wide 16-bit Stride
    # ========================================================================
    
    lpm_trie_t* lpm_create_ipv6_wide16()
    int lpm_add_ipv6_wide16(lpm_trie_t* trie, const uint8_t* prefix,
                            uint8_t prefix_len, uint32_t next_hop)
    int lpm_delete_ipv6_wide16(lpm_trie_t* trie, const uint8_t* prefix,
                               uint8_t prefix_len)
    uint32_t lpm_lookup_ipv6_wide16(const lpm_trie_t* trie, const uint8_t* addr)
    void lpm_lookup_batch_ipv6_wide16(const lpm_trie_t* trie,
                                       const uint8_t (*addrs)[16],
                                       uint32_t* next_hops, size_t count)
    
    # ========================================================================
    # Algorithm-Specific API: IPv6 8-bit Stride
    # ========================================================================
    
    lpm_trie_t* lpm_create_ipv6_8stride()
    int lpm_add_ipv6_8stride(lpm_trie_t* trie, const uint8_t* prefix,
                             uint8_t prefix_len, uint32_t next_hop)
    int lpm_delete_ipv6_8stride(lpm_trie_t* trie, const uint8_t* prefix,
                                uint8_t prefix_len)
    uint32_t lpm_lookup_ipv6_8stride(const lpm_trie_t* trie, const uint8_t* addr)
    void lpm_lookup_batch_ipv6_8stride(const lpm_trie_t* trie,
                                        const uint8_t (*addrs)[16],
                                        uint32_t* next_hops, size_t count)
    
    # ========================================================================
    # Legacy API (for backwards compatibility)
    # ========================================================================
    
    lpm_trie_t* lpm_create(uint8_t max_depth)
    uint32_t lpm_lookup(const lpm_trie_t* trie, const uint8_t* addr)
    void lpm_lookup_batch(const lpm_trie_t* trie, const uint8_t** addrs,
                          uint32_t* next_hops, size_t count)
    
    # ========================================================================
    # Utility Functions
    # ========================================================================
    
    const char* lpm_get_version()
    void lpm_print_stats(const lpm_trie_t* trie)

/*
 * liblpm API Dispatch Layer
 *
 * Provides generic API functions that dispatch to compile-time selected algorithms.
 * Algorithm selection is controlled by CMake flags:
 * - LPM_IPV4_DEFAULT: "dir24" (default) or "stride8"
 * - LPM_IPV6_DEFAULT: "wide16" (default) or "stride8"
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../include/lpm.h"
#include "../include/internal.h"

/* ============================================================================
 * Generic IPv4 API - Compile-time dispatch
 * ============================================================================ */

lpm_trie_t *lpm_create_ipv4(void)
{
#if defined(LPM_IPV4_DEFAULT_stride8)
    return lpm_create_ipv4_8stride();
#else
    /* Default: DIR-24-8 */
    return lpm_create_ipv4_dir24();
#endif
}

uint32_t lpm_lookup_ipv4(const lpm_trie_t *trie, uint32_t addr)
{
    if (!trie) return LPM_INVALID_NEXT_HOP;
    
    /* Runtime dispatch based on trie configuration */
    if (trie->use_ipv4_dir24 && trie->dir24_table) {
        return lpm_lookup_ipv4_dir24(trie, addr);
    }
    return lpm_lookup_ipv4_8stride(trie, addr);
}

void lpm_lookup_batch_ipv4(const lpm_trie_t *trie, const uint32_t *addrs,
                           uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    
    /* Runtime dispatch based on trie configuration */
    if (trie->use_ipv4_dir24 && trie->dir24_table) {
        lpm_lookup_batch_ipv4_dir24(trie, addrs, next_hops, count);
        return;
    }
    lpm_lookup_batch_ipv4_8stride(trie, addrs, next_hops, count);
}

/* ============================================================================
 * Generic IPv6 API - Compile-time dispatch
 * ============================================================================ */

lpm_trie_t *lpm_create_ipv6(void)
{
#if defined(LPM_IPV6_DEFAULT_stride8)
    return lpm_create_ipv6_8stride();
#else
    /* Default: Wide 16-bit stride */
    return lpm_create_ipv6_wide16();
#endif
}

uint32_t lpm_lookup_ipv6(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr) return LPM_INVALID_NEXT_HOP;
    
    /* Runtime dispatch based on trie configuration */
    if (trie->use_ipv6_wide_stride && trie->wide_nodes_pool) {
        return lpm_lookup_ipv6_wide16(trie, addr);
    }
    return lpm_lookup_ipv6_8stride(trie, addr);
}

void lpm_lookup_batch_ipv6(const lpm_trie_t *trie, const uint8_t (*addrs)[16],
                           uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    
    /* Runtime dispatch based on trie configuration */
    if (trie->use_ipv6_wide_stride && trie->wide_nodes_pool) {
        lpm_lookup_batch_ipv6_wide16(trie, addrs, next_hops, count);
        return;
    }
    lpm_lookup_batch_ipv6_8stride(trie, addrs, next_hops, count);
}

/* ============================================================================
 * Generic Add/Delete - Runtime dispatch based on trie type
 * ============================================================================ */

int lpm_add(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie || !prefix || prefix_len > trie->max_depth) return -1;
    
    /* IPv4 dispatch */
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        if (trie->use_ipv4_dir24 && trie->dir24_table) {
            return lpm_add_ipv4_dir24(trie, prefix, prefix_len, next_hop);
        }
        return lpm_add_ipv4_8stride(trie, prefix, prefix_len, next_hop);
    }
    
    /* IPv6 dispatch */
    if (trie->max_depth == LPM_IPV6_MAX_DEPTH) {
        if (trie->use_ipv6_wide_stride && trie->wide_nodes_pool) {
            return lpm_add_ipv6_wide16(trie, prefix, prefix_len, next_hop);
        }
        return lpm_add_ipv6_8stride(trie, prefix, prefix_len, next_hop);
    }
    
    return -1;
}

int lpm_delete(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len)
{
    if (!trie || !prefix || prefix_len > trie->max_depth) return -1;
    
    /* IPv4 dispatch */
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        if (trie->use_ipv4_dir24 && trie->dir24_table) {
            return lpm_delete_ipv4_dir24(trie, prefix, prefix_len);
        }
        return lpm_delete_ipv4_8stride(trie, prefix, prefix_len);
    }
    
    /* IPv6 dispatch */
    if (trie->max_depth == LPM_IPV6_MAX_DEPTH) {
        if (trie->use_ipv6_wide_stride && trie->wide_nodes_pool) {
            return lpm_delete_ipv6_wide16(trie, prefix, prefix_len);
        }
        return lpm_delete_ipv6_8stride(trie, prefix, prefix_len);
    }
    
    return -1;
}

/* ============================================================================
 * Generic lookup by address pointer
 * ============================================================================ */

uint32_t lpm_lookup(const lpm_trie_t *trie, const uint8_t *addr)
{
    if (!trie || !addr) return LPM_INVALID_NEXT_HOP;
    
    /* IPv4 */
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        if (trie->use_ipv4_dir24 && trie->dir24_table) {
            return lpm_lookup_ipv4_dir24_bytes(trie, addr);
        }
        return lpm_lookup_ipv4_8stride_bytes(trie, addr);
    }
    
    /* IPv6 */
    if (trie->max_depth == LPM_IPV6_MAX_DEPTH) {
        if (trie->use_ipv6_wide_stride && trie->wide_nodes_pool) {
            return lpm_lookup_ipv6_wide16(trie, addr);
        }
        return lpm_lookup_ipv6_8stride(trie, addr);
    }
    
    return LPM_INVALID_NEXT_HOP;
}

/* Batch lookup with pointer array */
void lpm_lookup_batch(const lpm_trie_t *trie, const uint8_t **addrs,
                      uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    
    /* IPv4 */
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        if (trie->use_ipv4_dir24 && trie->dir24_table) {
            lpm_lookup_batch_ipv4_dir24_ptrs(trie, addrs, next_hops, count);
            return;
        }
        lpm_lookup_batch_ipv4_8stride_bytes(trie, addrs, next_hops, count);
        return;
    }
    
    /* IPv6 - convert to proper format and dispatch */
    if (trie->max_depth == LPM_IPV6_MAX_DEPTH) {
        for (size_t i = 0; i < count; i++) {
            if (trie->use_ipv6_wide_stride && trie->wide_nodes_pool) {
                next_hops[i] = lpm_lookup_ipv6_wide16(trie, addrs[i]);
            } else {
                next_hops[i] = lpm_lookup_ipv6_8stride(trie, addrs[i]);
            }
        }
    }
}

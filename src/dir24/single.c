/*
 * IPv4 DIR-24-8 Algorithm - Single Lookup
 * Optimized single lookup implementation
 */

#include <stdint.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../../include/lpm.h"
#include "../../include/internal.h"

/* ============================================================================
 * Inline Lookup Implementation
 * Key optimizations:
 * - Single branch for common case (most routes are /8-/24)
 * - Branchless extended flag check
 * - Compiler hints for branch prediction
 * ============================================================================ */

__attribute__((hot, always_inline, flatten))
static inline uint32_t dir24_lookup_inline(const struct lpm_dir24_entry * restrict dir24,
                                           const struct lpm_tbl8_entry * restrict tbl8,
                                           const uint8_t * restrict addr)
{
    /* First lookup: 24-bit direct table - most common path */
    uint32_t dir24_idx = ((uint32_t)addr[0] << 16) | ((uint32_t)addr[1] << 8) | addr[2];
    uint32_t data = dir24[dir24_idx].data;
    
    /* Fast path: most routes are /8-/24 and don't need tbl8 */
    if (__builtin_expect(!(data & LPM_DIR24_EXT_FLAG), 1)) {
        /* Check valid flag and return next_hop or invalid */
        return (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
    }
    
    /* Slow path: extended to tbl8 for /25-/32 routes */
    uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
    uint32_t tbl8_idx = (tbl8_group << 8) | addr[3];
    uint32_t tbl8_data = tbl8[tbl8_idx].data;
    
    return (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
}

/* ============================================================================
 * Public Lookup Functions
 * ============================================================================ */

/* Lookup with byte array input */
uint32_t lpm_lookup_ipv4_dir24_bytes(const lpm_trie_t *trie, const uint8_t addr[4])
{
    if (!trie || !addr || !trie->use_ipv4_dir24 || !trie->dir24_table) {
        return LPM_INVALID_NEXT_HOP;
    }
    
    uint32_t result = dir24_lookup_inline(trie->dir24_table, trie->tbl8_groups, addr);
    
    /* Return default route if no match and default exists */
    if (result == LPM_INVALID_NEXT_HOP && trie->has_default_route) {
        return trie->default_next_hop;
    }
    
    return result;
}

/* Lookup with uint32_t input */
uint32_t lpm_lookup_ipv4_dir24(const lpm_trie_t *trie, uint32_t addr)
{
    if (!trie || !trie->use_ipv4_dir24 || !trie->dir24_table) {
        return LPM_INVALID_NEXT_HOP;
    }
    
    /* Convert to byte array - network byte order */
    uint8_t bytes[4] = {
        (addr >> 24) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    uint32_t result = dir24_lookup_inline(trie->dir24_table, trie->tbl8_groups, bytes);
    
    if (result == LPM_INVALID_NEXT_HOP && trie->has_default_route) {
        return trie->default_next_hop;
    }
    
    return result;
}

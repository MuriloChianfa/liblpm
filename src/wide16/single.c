/*
 * IPv6 Wide 16-bit Stride Algorithm - Single Lookup
 * Optimized single lookup for wide stride trie
 */

#include <stdint.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../../include/lpm.h"
#include "../../include/internal.h"

/* ============================================================================
 * Wide Stride Lookup Implementation
 * ============================================================================ */

__attribute__((hot))
static uint32_t lookup_wide16_internal(const lpm_trie_t *trie, const uint8_t *addr)
{
    uint32_t best_next_hop = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    uint32_t node_idx = trie->root_idx;
    uint8_t level = 0;
    
    /* First 3 levels: 16-bit wide stride (48 bits total) */
    bool is_wide_node = true;
    for (level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS && level < 3; level++) {
        if (!is_wide_node && node_idx == LPM_INVALID_INDEX) break;
        if (!is_wide_node) break;  /* Transitioned to 8-bit nodes */
        
        struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
        
        /* Extract 16-bit index from address */
        uint16_t index = ((uint16_t)addr[level * 2] << 8) | addr[level * 2 + 1];
        
        struct lpm_entry *entry = &wide_node->entries[index];
        
        /* Update best match if this entry has a valid next hop */
        if (entry->child_and_valid & LPM_VALID_FLAG) {
            best_next_hop = entry->next_hop;
        }
        
        /* Get child node and check its type */
        uint32_t cv = entry->child_and_valid;
        uint32_t child_idx = cv & LPM_CHILD_MASK;
        bool has_child = (child_idx != 0) || (cv & LPM_WIDE_NODE_FLAG);
        if (!has_child) {
            return best_next_hop;
        }
        
        is_wide_node = (cv & LPM_WIDE_NODE_FLAG) != 0;
        node_idx = child_idx;
    }
    
    /* Remaining levels: 8-bit stride (bytes 2-15 after first 16-bit level, 112 bits) */
    for (uint8_t byte_idx = 2; byte_idx < 16; byte_idx++) {
        if (node_idx == LPM_INVALID_INDEX) break;
        
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        uint8_t index = addr[byte_idx];
        
        struct lpm_entry *entry = &node->entries[index];
        
        if (entry->child_and_valid & LPM_VALID_FLAG) {
            best_next_hop = entry->next_hop;
        }
        
        uint32_t child_idx = entry->child_and_valid & LPM_CHILD_MASK;
        if (child_idx == LPM_INVALID_INDEX) {
            return best_next_hop;
        }
        
        node_idx = child_idx;
    }
    
    return best_next_hop;
}

/* ============================================================================
 * SIMD Variants (same algorithm, different compilation targets)
 * ============================================================================ */

__attribute__((hot))
uint32_t lpm_lookup_ipv6_wide16_scalar(const lpm_trie_t *trie, const uint8_t *addr)
{
    return lookup_wide16_internal(trie, addr);
}

__attribute__((hot))
uint32_t lpm_lookup_ipv6_wide16_sse2(const lpm_trie_t *trie, const uint8_t *addr)
{
    return lookup_wide16_internal(trie, addr);
}

__attribute__((hot, target("avx2")))
uint32_t lpm_lookup_ipv6_wide16_avx2(const lpm_trie_t *trie, const uint8_t *addr)
{
    return lookup_wide16_internal(trie, addr);
}

__attribute__((hot, target("avx512f")))
uint32_t lpm_lookup_ipv6_wide16_avx512(const lpm_trie_t *trie, const uint8_t *addr)
{
    return lookup_wide16_internal(trie, addr);
}

/* ============================================================================
 * ifunc Resolver
 * ============================================================================ */

typedef uint32_t (*lpm_wide16_single_func_t)(const lpm_trie_t *, const uint8_t *);

static lpm_wide16_single_func_t lpm_wide16_single_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_ipv6_wide16_avx512;
    case SIMD_AVX2:
    case SIMD_AVX:
        return lpm_lookup_ipv6_wide16_avx2;
    case SIMD_SSE4_2:
    case SIMD_SSE2:
        return lpm_lookup_ipv6_wide16_sse2;
    case SIMD_SCALAR:
    default:
        return lpm_lookup_ipv6_wide16_scalar;
    }
}

/* Internal ifunc-dispatched lookup */
static uint32_t lpm_lookup_ipv6_wide16_internal(const lpm_trie_t *trie, const uint8_t *addr)
    __attribute__((ifunc("lpm_wide16_single_resolver")));

/* Public API */
uint32_t lpm_lookup_ipv6_wide16(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr || !trie->use_ipv6_wide_stride) {
        return LPM_INVALID_NEXT_HOP;
    }
    return lpm_lookup_ipv6_wide16_internal(trie, addr);
}

/*
 * IPv6 Wide 16-bit Stride Algorithm - Batch Lookup
 * SIMD-optimized batch lookup with ifunc dispatch
 *
 * NEW IMPLEMENTATION - This was missing from the original codebase!
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../../include/lpm.h"
#include "../../include/internal.h"

/* Forward declaration for single lookup */
extern uint32_t lpm_lookup_ipv6_wide16(const lpm_trie_t *trie, const uint8_t addr[16]);

/* ============================================================================
 * Wide Stride Lookup Helper (inline for batch use)
 * ============================================================================ */

__attribute__((hot, always_inline))
static inline uint32_t lookup_wide16_single(const lpm_trie_t *trie, const uint8_t *addr)
{
    uint32_t best_next_hop = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    uint32_t node_idx = trie->root_idx;
    
    /* First level: 16-bit wide stride */
    bool is_wide_node = true;
    for (uint8_t level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS && is_wide_node; level++) {
        struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
        uint16_t index = ((uint16_t)addr[level * 2] << 8) | addr[level * 2 + 1];
        
        struct lpm_entry *entry = &wide_node->entries[index];
        
        if (entry->child_and_valid & LPM_VALID_FLAG) {
            best_next_hop = entry->next_hop;
        }
        
        uint32_t cv = entry->child_and_valid;
        uint32_t child_idx = cv & LPM_CHILD_MASK;
        bool has_child = (child_idx != 0) || (cv & LPM_WIDE_NODE_FLAG);
        if (!has_child) {
            return best_next_hop;
        }
        
        is_wide_node = (cv & LPM_WIDE_NODE_FLAG) != 0;
        node_idx = child_idx;
    }
    
    /* Remaining levels: 8-bit stride */
    for (uint8_t byte_idx = 2; byte_idx < 16; byte_idx++) {
        if (node_idx == LPM_INVALID_INDEX) break;
        
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        struct lpm_entry *entry = &node->entries[addr[byte_idx]];
        
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
 * Scalar Batch Implementation
 * ============================================================================ */

__attribute__((hot))
void lpm_lookup_batch_ipv6_wide16_scalar(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        next_hops[i] = lookup_wide16_single(trie, addrs[i]);
    }
}

/* ============================================================================
 * SSE2 Batch - Process 4 in parallel with interleaved access
 * ============================================================================ */

__attribute__((hot))
void lpm_lookup_batch_ipv6_wide16_sse2(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count)
{
    const struct lpm_node_16 * restrict wide_pool = trie->wide_nodes_pool;
    const struct lpm_node * restrict node_pool = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    
    /* Process 4 in parallel */
    for (; i + 3 < count; i += 4) {
        uint32_t n[4], r[4];
        bool wide[4];
        const uint8_t *a[4];
        
        for (int j = 0; j < 4; j++) {
            n[j] = root;
            r[j] = def;
            wide[j] = true;
            a[j] = addrs[i + j];
        }
        
        /* Wide stride levels (16-bit) */
        for (uint8_t level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS; level++) {
            for (int j = 0; j < 4; j++) {
                if (wide[j]) {
                    struct lpm_node_16 *wn = &((struct lpm_node_16 *)wide_pool)[n[j]];
                    uint16_t idx = ((uint16_t)a[j][level * 2] << 8) | a[j][level * 2 + 1];
                    
                    _mm_prefetch((const char*)&wn->entries[idx], _MM_HINT_T0);
                }
            }
            
            for (int j = 0; j < 4; j++) {
                if (wide[j]) {
                    struct lpm_node_16 *wn = &((struct lpm_node_16 *)wide_pool)[n[j]];
                    uint16_t idx = ((uint16_t)a[j][level * 2] << 8) | a[j][level * 2 + 1];
                    struct lpm_entry *e = &wn->entries[idx];
                    
                    if (e->child_and_valid & LPM_VALID_FLAG) {
                        r[j] = e->next_hop;
                    }
                    
                    uint32_t cv = e->child_and_valid;
                    uint32_t child = cv & LPM_CHILD_MASK;
                    bool has_child = (child != 0) || (cv & LPM_WIDE_NODE_FLAG);
                    
                    if (has_child) {
                        wide[j] = (cv & LPM_WIDE_NODE_FLAG) != 0;
                        n[j] = child;
                    } else {
                        n[j] = LPM_INVALID_INDEX;
                        wide[j] = false;
                    }
                }
            }
        }
        
        /* 8-bit stride levels */
        for (uint8_t byte_idx = 2; byte_idx < 16; byte_idx++) {
            bool any_active = false;
            
            for (int j = 0; j < 4; j++) {
                if (n[j] != LPM_INVALID_INDEX && !wide[j]) {
                    struct lpm_node *node = &((struct lpm_node *)node_pool)[n[j]];
                    _mm_prefetch((const char*)&node->entries[a[j][byte_idx]], _MM_HINT_T0);
                    any_active = true;
                }
            }
            
            if (!any_active) break;
            
            for (int j = 0; j < 4; j++) {
                if (n[j] != LPM_INVALID_INDEX && !wide[j]) {
                    struct lpm_node *node = &((struct lpm_node *)node_pool)[n[j]];
                    struct lpm_entry *e = &node->entries[a[j][byte_idx]];
                    
                    if (e->child_and_valid & LPM_VALID_FLAG) {
                        r[j] = e->next_hop;
                    }
                    
                    n[j] = e->child_and_valid & LPM_CHILD_MASK;
                }
            }
        }
        
        for (int j = 0; j < 4; j++) {
            next_hops[i + j] = r[j];
        }
    }
    
    /* Remainder */
    for (; i < count; i++) {
        next_hops[i] = lookup_wide16_single(trie, addrs[i]);
    }
}

/* ============================================================================
 * AVX2 Batch - Process 8 in parallel
 * ============================================================================ */

__attribute__((hot, target("avx2")))
void lpm_lookup_batch_ipv6_wide16_avx2(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count)
{
    const struct lpm_node_16 * restrict wide_pool = trie->wide_nodes_pool;
    const struct lpm_node * restrict node_pool = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    
    /* Process 8 in parallel */
    for (; i + 7 < count; i += 8) {
        uint32_t n[8], r[8];
        bool wide[8];
        const uint8_t *a[8];
        
        for (int j = 0; j < 8; j++) {
            n[j] = root;
            r[j] = def;
            wide[j] = true;
            a[j] = addrs[i + j];
        }
        
        /* Wide stride levels */
        for (uint8_t level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS; level++) {
            for (int j = 0; j < 8; j++) {
                if (wide[j]) {
                    struct lpm_node_16 *wn = &((struct lpm_node_16 *)wide_pool)[n[j]];
                    uint16_t idx = ((uint16_t)a[j][level * 2] << 8) | a[j][level * 2 + 1];
                    _mm_prefetch((const char*)&wn->entries[idx], _MM_HINT_T0);
                }
            }
            
            for (int j = 0; j < 8; j++) {
                if (wide[j]) {
                    struct lpm_node_16 *wn = &((struct lpm_node_16 *)wide_pool)[n[j]];
                    uint16_t idx = ((uint16_t)a[j][level * 2] << 8) | a[j][level * 2 + 1];
                    struct lpm_entry *e = &wn->entries[idx];
                    
                    if (e->child_and_valid & LPM_VALID_FLAG) {
                        r[j] = e->next_hop;
                    }
                    
                    uint32_t cv = e->child_and_valid;
                    uint32_t child = cv & LPM_CHILD_MASK;
                    bool has_child = (child != 0) || (cv & LPM_WIDE_NODE_FLAG);
                    
                    if (has_child) {
                        wide[j] = (cv & LPM_WIDE_NODE_FLAG) != 0;
                        n[j] = child;
                    } else {
                        n[j] = LPM_INVALID_INDEX;
                        wide[j] = false;
                    }
                }
            }
        }
        
        /* 8-bit stride levels */
        for (uint8_t byte_idx = 2; byte_idx < 16; byte_idx++) {
            int active = 0;
            
            for (int j = 0; j < 8; j++) {
                if (n[j] != LPM_INVALID_INDEX && !wide[j]) {
                    struct lpm_node *node = &((struct lpm_node *)node_pool)[n[j]];
                    _mm_prefetch((const char*)&node->entries[a[j][byte_idx]], _MM_HINT_T0);
                    active++;
                }
            }
            
            if (!active) break;
            
            for (int j = 0; j < 8; j++) {
                if (n[j] != LPM_INVALID_INDEX && !wide[j]) {
                    struct lpm_node *node = &((struct lpm_node *)node_pool)[n[j]];
                    struct lpm_entry *e = &node->entries[a[j][byte_idx]];
                    
                    if (e->child_and_valid & LPM_VALID_FLAG) {
                        r[j] = e->next_hop;
                    }
                    
                    n[j] = e->child_and_valid & LPM_CHILD_MASK;
                }
            }
        }
        
        for (int j = 0; j < 8; j++) {
            next_hops[i + j] = r[j];
        }
    }
    
    /* Remainder */
    for (; i < count; i++) {
        next_hops[i] = lookup_wide16_single(trie, addrs[i]);
    }
}

/* ============================================================================
 * AVX512 Batch - Process 16 in parallel
 * ============================================================================ */

__attribute__((hot, target("avx512f")))
void lpm_lookup_batch_ipv6_wide16_avx512(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count)
{
    const struct lpm_node_16 * restrict wide_pool = trie->wide_nodes_pool;
    const struct lpm_node * restrict node_pool = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    
    /* Process 16 in parallel */
    for (; i + 15 < count; i += 16) {
        uint32_t n[16], r[16];
        bool wide[16];
        const uint8_t *a[16];
        
        for (int j = 0; j < 16; j++) {
            n[j] = root;
            r[j] = def;
            wide[j] = true;
            a[j] = addrs[i + j];
        }
        
        /* Wide stride levels */
        for (uint8_t level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS; level++) {
            for (int j = 0; j < 16; j++) {
                if (wide[j]) {
                    struct lpm_node_16 *wn = &((struct lpm_node_16 *)wide_pool)[n[j]];
                    uint16_t idx = ((uint16_t)a[j][level * 2] << 8) | a[j][level * 2 + 1];
                    _mm_prefetch((const char*)&wn->entries[idx], _MM_HINT_T0);
                }
            }
            
            for (int j = 0; j < 16; j++) {
                if (wide[j]) {
                    struct lpm_node_16 *wn = &((struct lpm_node_16 *)wide_pool)[n[j]];
                    uint16_t idx = ((uint16_t)a[j][level * 2] << 8) | a[j][level * 2 + 1];
                    struct lpm_entry *e = &wn->entries[idx];
                    
                    if (e->child_and_valid & LPM_VALID_FLAG) {
                        r[j] = e->next_hop;
                    }
                    
                    uint32_t cv = e->child_and_valid;
                    uint32_t child = cv & LPM_CHILD_MASK;
                    bool has_child = (child != 0) || (cv & LPM_WIDE_NODE_FLAG);
                    
                    if (has_child) {
                        wide[j] = (cv & LPM_WIDE_NODE_FLAG) != 0;
                        n[j] = child;
                    } else {
                        n[j] = LPM_INVALID_INDEX;
                        wide[j] = false;
                    }
                }
            }
        }
        
        /* 8-bit stride levels */
        for (uint8_t byte_idx = 2; byte_idx < 16; byte_idx++) {
            int active = 0;
            
            for (int j = 0; j < 16; j++) {
                if (n[j] != LPM_INVALID_INDEX && !wide[j]) {
                    struct lpm_node *node = &((struct lpm_node *)node_pool)[n[j]];
                    _mm_prefetch((const char*)&node->entries[a[j][byte_idx]], _MM_HINT_T0);
                    active++;
                }
            }
            
            if (!active) break;
            
            for (int j = 0; j < 16; j++) {
                if (n[j] != LPM_INVALID_INDEX && !wide[j]) {
                    struct lpm_node *node = &((struct lpm_node *)node_pool)[n[j]];
                    struct lpm_entry *e = &node->entries[a[j][byte_idx]];
                    
                    if (e->child_and_valid & LPM_VALID_FLAG) {
                        r[j] = e->next_hop;
                    }
                    
                    n[j] = e->child_and_valid & LPM_CHILD_MASK;
                }
            }
        }
        
        for (int j = 0; j < 16; j++) {
            next_hops[i + j] = r[j];
        }
    }
    
    /* Remainder with AVX2 */
    if (i + 7 < count) {
        lpm_lookup_batch_ipv6_wide16_avx2(trie, &addrs[i], &next_hops[i], count - i);
        return;
    }
    
    /* Scalar remainder */
    for (; i < count; i++) {
        next_hops[i] = lookup_wide16_single(trie, addrs[i]);
    }
}

/* ============================================================================
 * ifunc Resolver
 * ============================================================================ */

typedef void (*lpm_wide16_batch_func_t)(const lpm_trie_t *, const uint8_t **, uint32_t *, size_t);

EXPLICIT_RUNTIME_RESOLVER(lpm_wide16_batch_resolver)
{
    simd_level_t level = LPM_DETECT_SIMD();
    
    switch (level) {
    case SIMD_AVX512F:
        return (void*)lpm_lookup_batch_ipv6_wide16_avx512;
    case SIMD_AVX2:
    case SIMD_AVX:
        return (void*)lpm_lookup_batch_ipv6_wide16_avx2;
    case SIMD_SSE4_2:
    case SIMD_SSE2:
        return (void*)lpm_lookup_batch_ipv6_wide16_sse2;
    case SIMD_SCALAR:
    default:
        return (void*)lpm_lookup_batch_ipv6_wide16_scalar;
    }
}

/* Internal ifunc-dispatched batch lookup for pointer array */
static void lpm_lookup_batch_ipv6_wide16_internal(const lpm_trie_t *trie, const uint8_t **addrs,
                                                   uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_wide16_batch_resolver")));

/* Public API for 2D array */
void lpm_lookup_batch_ipv6_wide16(const lpm_trie_t *trie, const uint8_t (*addrs)[16],
                                   uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    if (!trie->use_ipv6_wide_stride) return;
    
    const uint8_t *stack_ptrs[256];
    const uint8_t **ptrs;
    
    if (count <= 256) {
        ptrs = stack_ptrs;
    } else {
        ptrs = (const uint8_t**)malloc(count * sizeof(uint8_t*));
        if (!ptrs) return;
    }
    
    for (size_t i = 0; i < count; i++) {
        ptrs[i] = addrs[i];
    }
    
    lpm_lookup_batch_ipv6_wide16_internal(trie, ptrs, next_hops, count);
    
    if (count > 256) {
        free(ptrs);
    }
}

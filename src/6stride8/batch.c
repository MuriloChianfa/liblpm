/*
 * IPv6 8-bit Stride Algorithm - Batch Lookup
 * SIMD-optimized batch lookup with ifunc dispatch
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
extern uint32_t lpm_lookup_ipv6_8stride(const lpm_trie_t *trie, const uint8_t addr[16]);

/* ============================================================================
 * Scalar Batch Implementation
 * ============================================================================ */

__attribute__((hot))
void lpm_lookup_batch_ipv6_8stride_scalar(const lpm_trie_t *trie, const uint8_t **addrs,
                                           uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    for (size_t i = 0; i < count; i++) {
        const uint8_t *addr = addrs[i];
        uint32_t N = root;
        uint32_t R = LPM_INVALID_NEXT_HOP;
        
        for (uint8_t d = 0; d < 16 && N; d++) {
            const struct lpm_entry *e = &P[N].entries[addr[d]];
            uint32_t cv = e->child_and_valid;
            if (cv & LPM_VALID_FLAG) R = e->next_hop;
            N = cv & LPM_CHILD_MASK;
        }
        
        next_hops[i] = (R != LPM_INVALID_NEXT_HOP) ? R : def;
    }
}

/* ============================================================================
 * SSE2 Batch - Process 4 in parallel with interleaved access
 * ============================================================================ */

__attribute__((hot))
void lpm_lookup_batch_ipv6_8stride_sse2(const lpm_trie_t *trie, const uint8_t **addrs,
                                         uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        uint32_t n0 = root, n1 = root, n2 = root, n3 = root;
        uint32_t r0 = LPM_INVALID_NEXT_HOP, r1 = LPM_INVALID_NEXT_HOP;
        uint32_t r2 = LPM_INVALID_NEXT_HOP, r3 = LPM_INVALID_NEXT_HOP;
        const uint8_t *a0 = addrs[i], *a1 = addrs[i+1], *a2 = addrs[i+2], *a3 = addrs[i+3];
        
        for (uint8_t d = 0; d < 16; d++) {
            /* Prefetch next level for all 4 */
            if (n0) _mm_prefetch((const char*)&P[n0].entries[a0[d]], _MM_HINT_T0);
            if (n1) _mm_prefetch((const char*)&P[n1].entries[a1[d]], _MM_HINT_T0);
            if (n2) _mm_prefetch((const char*)&P[n2].entries[a2[d]], _MM_HINT_T0);
            if (n3) _mm_prefetch((const char*)&P[n3].entries[a3[d]], _MM_HINT_T0);
            
            if (n0) {
                const struct lpm_entry *e = &P[n0].entries[a0[d]];
                uint32_t cv = e->child_and_valid;
                if (cv & LPM_VALID_FLAG) r0 = e->next_hop;
                n0 = cv & LPM_CHILD_MASK;
            }
            if (n1) {
                const struct lpm_entry *e = &P[n1].entries[a1[d]];
                uint32_t cv = e->child_and_valid;
                if (cv & LPM_VALID_FLAG) r1 = e->next_hop;
                n1 = cv & LPM_CHILD_MASK;
            }
            if (n2) {
                const struct lpm_entry *e = &P[n2].entries[a2[d]];
                uint32_t cv = e->child_and_valid;
                if (cv & LPM_VALID_FLAG) r2 = e->next_hop;
                n2 = cv & LPM_CHILD_MASK;
            }
            if (n3) {
                const struct lpm_entry *e = &P[n3].entries[a3[d]];
                uint32_t cv = e->child_and_valid;
                if (cv & LPM_VALID_FLAG) r3 = e->next_hop;
                n3 = cv & LPM_CHILD_MASK;
            }
            
            if (!n0 && !n1 && !n2 && !n3) break;
        }
        
        next_hops[i]   = (r0 != LPM_INVALID_NEXT_HOP) ? r0 : def;
        next_hops[i+1] = (r1 != LPM_INVALID_NEXT_HOP) ? r1 : def;
        next_hops[i+2] = (r2 != LPM_INVALID_NEXT_HOP) ? r2 : def;
        next_hops[i+3] = (r3 != LPM_INVALID_NEXT_HOP) ? r3 : def;
    }
    
    /* Remainder */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_ipv6_8stride(trie, addrs[i]);
    }
}

/* ============================================================================
 * SSE4.2 Batch
 * ============================================================================ */

__attribute__((hot, target("sse4.2")))
void lpm_lookup_batch_ipv6_8stride_sse42(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count)
{
    lpm_lookup_batch_ipv6_8stride_sse2(trie, addrs, next_hops, count);
}

/* ============================================================================
 * AVX Batch - Process 8 in parallel
 * ============================================================================ */

__attribute__((hot, target("avx")))
void lpm_lookup_batch_ipv6_8stride_avx(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    for (; i + 7 < count; i += 8) {
        uint32_t n[8], r[8];
        const uint8_t *a[8];
        
        for (int j = 0; j < 8; j++) {
            n[j] = root;
            r[j] = LPM_INVALID_NEXT_HOP;
            a[j] = addrs[i+j];
        }
        
        for (uint8_t d = 0; d < 16; d++) {
            bool active = false;
            for (int j = 0; j < 8; j++) {
                if (n[j]) {
                    _mm_prefetch((const char*)&P[n[j]].entries[a[j][d]], _MM_HINT_T0);
                    active = true;
                }
            }
            
            for (int j = 0; j < 8; j++) {
                if (n[j]) {
                    const struct lpm_entry *e = &P[n[j]].entries[a[j][d]];
                    uint32_t cv = e->child_and_valid;
                    if (cv & LPM_VALID_FLAG) r[j] = e->next_hop;
                    n[j] = cv & LPM_CHILD_MASK;
                }
            }
            if (!active) break;
        }
        
        for (int j = 0; j < 8; j++) {
            next_hops[i+j] = (r[j] != LPM_INVALID_NEXT_HOP) ? r[j] : def;
        }
    }
    
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_ipv6_8stride(trie, addrs[i]);
    }
}

/* ============================================================================
 * AVX2 Batch - Process 8 in parallel with better throughput
 * ============================================================================ */

__attribute__((hot, target("avx2")))
void lpm_lookup_batch_ipv6_8stride_avx2(const lpm_trie_t *trie, const uint8_t **addrs,
                                         uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    for (; i + 7 < count; i += 8) {
        uint32_t n[8], r[8];
        const uint8_t *a[8];
        
        for (int j = 0; j < 8; j++) {
            n[j] = root;
            r[j] = LPM_INVALID_NEXT_HOP;
            a[j] = addrs[i+j];
        }
        
        for (uint8_t d = 0; d < 16; d++) {
            for (int j = 0; j < 8; j++) {
                if (n[j]) _mm_prefetch((const char*)&P[n[j]].entries[a[j][d]], _MM_HINT_T0);
            }
            
            int active = 0;
            for (int j = 0; j < 8; j++) {
                if (n[j]) {
                    const struct lpm_entry *e = &P[n[j]].entries[a[j][d]];
                    uint32_t cv = e->child_and_valid;
                    if (cv & LPM_VALID_FLAG) r[j] = e->next_hop;
                    n[j] = cv & LPM_CHILD_MASK;
                    if (n[j]) active++;
                }
            }
            if (!active) break;
        }
        
        for (int j = 0; j < 8; j++) {
            next_hops[i+j] = (r[j] != LPM_INVALID_NEXT_HOP) ? r[j] : def;
        }
    }
    
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_ipv6_8stride(trie, addrs[i]);
    }
}

/* ============================================================================
 * AVX512 Batch - Process 16 in parallel
 * ============================================================================ */

__attribute__((hot, target("avx512f")))
void lpm_lookup_batch_ipv6_8stride_avx512(const lpm_trie_t *trie, const uint8_t **addrs,
                                           uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    size_t i = 0;
    for (; i + 15 < count; i += 16) {
        uint32_t n[16], r[16];
        const uint8_t *a[16];
        
        for (int j = 0; j < 16; j++) {
            n[j] = root;
            r[j] = LPM_INVALID_NEXT_HOP;
            a[j] = addrs[i+j];
        }
        
        for (uint8_t d = 0; d < 16; d++) {
            for (int j = 0; j < 16; j++) {
                if (n[j]) _mm_prefetch((const char*)&P[n[j]].entries[a[j][d]], _MM_HINT_T0);
            }
            
            int active = 0;
            for (int j = 0; j < 16; j++) {
                if (n[j]) {
                    const struct lpm_entry *e = &P[n[j]].entries[a[j][d]];
                    uint32_t cv = e->child_and_valid;
                    if (cv & LPM_VALID_FLAG) r[j] = e->next_hop;
                    n[j] = cv & LPM_CHILD_MASK;
                    if (n[j]) active++;
                }
            }
            if (!active) break;
        }
        
        for (int j = 0; j < 16; j++) {
            next_hops[i+j] = (r[j] != LPM_INVALID_NEXT_HOP) ? r[j] : def;
        }
    }
    
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_ipv6_8stride(trie, addrs[i]);
    }
}

/* ============================================================================
 * ifunc Resolver
 * ============================================================================ */

typedef void (*lpm_ipv6_8stride_batch_func_t)(const lpm_trie_t *, const uint8_t **, uint32_t *, size_t);

static lpm_ipv6_8stride_batch_func_t lpm_ipv6_8stride_batch_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_batch_ipv6_8stride_avx512;
    case SIMD_AVX2:
        return lpm_lookup_batch_ipv6_8stride_avx2;
    case SIMD_AVX:
        return lpm_lookup_batch_ipv6_8stride_avx;
    case SIMD_SSE4_2:
        return lpm_lookup_batch_ipv6_8stride_sse42;
    case SIMD_SSE2:
        return lpm_lookup_batch_ipv6_8stride_sse2;
    case SIMD_SCALAR:
    default:
        return lpm_lookup_batch_ipv6_8stride_scalar;
    }
}

/* Internal ifunc-dispatched batch lookup for pointer array */
static void lpm_lookup_batch_ipv6_8stride_internal(const lpm_trie_t *trie, const uint8_t **addrs,
                                                    uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_ipv6_8stride_batch_resolver")));

/* Public API for 2D array */
void lpm_lookup_batch_ipv6_8stride(const lpm_trie_t *trie, const uint8_t (*addrs)[16],
                                    uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    
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
    
    lpm_lookup_batch_ipv6_8stride_internal(trie, ptrs, next_hops, count);
    
    if (count > 256) {
        free(ptrs);
    }
}

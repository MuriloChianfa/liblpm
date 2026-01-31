/*
 * IPv4 8-bit Stride Algorithm - Single Lookup
 * SIMD-optimized single lookup with ifunc dispatch
 */

#include <stdint.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../../include/lpm.h"
#include "../../include/internal.h"

/* ============================================================================
 * IPv4 Lookup - Unrolled 4 levels with prefetch
 * ============================================================================ */

__attribute__((hot, always_inline))
static inline uint32_t lookup_ipv4_unrolled(const lpm_node_t * restrict P,
                                            uint32_t N, const uint8_t *addr)
{
    uint32_t R = LPM_INVALID_NEXT_HOP;
    const struct lpm_entry *e;
    uint32_t cv;
    
    /* Level 0 */
    e = &P[N].entries[addr[0]]; cv = e->child_and_valid;
    R = (cv & LPM_VALID_FLAG) ? e->next_hop : R;
    N = cv & LPM_CHILD_MASK;
    if (!N) return R;
    __builtin_prefetch(&P[N].entries[addr[1]], 0, 3);
    
    /* Level 1 */
    e = &P[N].entries[addr[1]]; cv = e->child_and_valid;
    R = (cv & LPM_VALID_FLAG) ? e->next_hop : R;
    N = cv & LPM_CHILD_MASK;
    if (!N) return R;
    __builtin_prefetch(&P[N].entries[addr[2]], 0, 3);
    
    /* Level 2 */
    e = &P[N].entries[addr[2]]; cv = e->child_and_valid;
    R = (cv & LPM_VALID_FLAG) ? e->next_hop : R;
    N = cv & LPM_CHILD_MASK;
    if (!N) return R;
    __builtin_prefetch(&P[N].entries[addr[3]], 0, 3);
    
    /* Level 3 */
    e = &P[N].entries[addr[3]]; cv = e->child_and_valid;
    if (cv & LPM_VALID_FLAG) R = e->next_hop;
    
    return R;
}

/* ============================================================================
 * SIMD Variants
 * ============================================================================ */

__attribute__((hot))
uint32_t lpm_lookup_ipv4_8stride_scalar(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv4_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot))
uint32_t lpm_lookup_ipv4_8stride_sse2(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv4_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("sse4.2")))
uint32_t lpm_lookup_ipv4_8stride_sse42(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv4_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx")))
uint32_t lpm_lookup_ipv4_8stride_avx(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv4_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx2")))
uint32_t lpm_lookup_ipv4_8stride_avx2(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv4_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx512f")))
uint32_t lpm_lookup_ipv4_8stride_avx512(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv4_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

/* ============================================================================
 * ifunc Resolver
 * ============================================================================ */

typedef uint32_t (*lpm_ipv4_8stride_single_func_t)(const lpm_trie_t *, const uint8_t *);

EXPLICIT_RUNTIME_RESOLVER(lpm_ipv4_8stride_single_resolver)
{
    simd_level_t level = LPM_DETECT_SIMD();
    
    switch (level) {
    case SIMD_AVX512F:
        return (void*)lpm_lookup_ipv4_8stride_avx512;
    case SIMD_AVX2:
        return (void*)lpm_lookup_ipv4_8stride_avx2;
    case SIMD_AVX:
        return (void*)lpm_lookup_ipv4_8stride_avx;
    case SIMD_SSE4_2:
        return (void*)lpm_lookup_ipv4_8stride_sse42;
    case SIMD_SSE2:
        return (void*)lpm_lookup_ipv4_8stride_sse2;
    case SIMD_SCALAR:
    default:
        return (void*)lpm_lookup_ipv4_8stride_scalar;
    }
}

/* ifunc-dispatched lookup for byte array input */
uint32_t lpm_lookup_ipv4_8stride_bytes(const lpm_trie_t *trie, const uint8_t *addr)
    __attribute__((ifunc("lpm_ipv4_8stride_single_resolver")));

/* Public API wrapper for uint32_t address */
uint32_t lpm_lookup_ipv4_8stride(const lpm_trie_t *trie, uint32_t addr)
{
    if (!trie || trie->max_depth != LPM_IPV4_MAX_DEPTH) return LPM_INVALID_NEXT_HOP;
    
    uint8_t bytes[4] = {
        (addr >> 24) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    return lpm_lookup_ipv4_8stride_bytes(trie, bytes);
}

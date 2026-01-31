/*
 * IPv6 8-bit Stride Algorithm - Single Lookup
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
 * IPv6 Lookup - Unrolled 16 levels, branchless
 * ============================================================================ */

__attribute__((hot, always_inline))
static inline uint32_t lookup_ipv6_unrolled(const lpm_node_t * restrict P,
                                            uint32_t N, const uint8_t *addr)
{
    uint32_t R = LPM_INVALID_NEXT_HOP;
    
#define S(i) do { \
    const struct lpm_entry *e = &P[N].entries[addr[i]]; \
    uint32_t cv = e->child_and_valid; \
    R = (cv & LPM_VALID_FLAG) ? e->next_hop : R; \
    N = cv & LPM_CHILD_MASK; \
    if (!N) return R; \
} while(0)
    
    S(0); S(1); S(2); S(3);
    S(4); S(5); S(6); S(7);
    S(8); S(9); S(10); S(11);
    S(12); S(13); S(14);
    
    /* Last byte */
    const struct lpm_entry *e = &P[N].entries[addr[15]];
    uint32_t cv = e->child_and_valid;
    if (cv & LPM_VALID_FLAG) R = e->next_hop;
    
#undef S
    return R;
}

/* ============================================================================
 * SIMD Variants
 * ============================================================================ */

__attribute__((hot))
uint32_t lpm_lookup_ipv6_8stride_scalar(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv6_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot))
uint32_t lpm_lookup_ipv6_8stride_sse2(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv6_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("sse4.2")))
uint32_t lpm_lookup_ipv6_8stride_sse42(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv6_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx")))
uint32_t lpm_lookup_ipv6_8stride_avx(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv6_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx2")))
uint32_t lpm_lookup_ipv6_8stride_avx2(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv6_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx512f")))
uint32_t lpm_lookup_ipv6_8stride_avx512(const lpm_trie_t *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    
    uint32_t R = lookup_ipv6_unrolled(P, N, addr);
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

/* ============================================================================
 * ifunc Resolver
 * ============================================================================ */

typedef uint32_t (*lpm_ipv6_8stride_single_func_t)(const lpm_trie_t *, const uint8_t *);

EXPLICIT_RUNTIME_RESOLVER(lpm_ipv6_8stride_single_resolver)
{
    simd_level_t level = LPM_DETECT_SIMD();
    
    switch (level) {
    case SIMD_AVX512F:
        return (void*)lpm_lookup_ipv6_8stride_avx512;
    case SIMD_AVX2:
        return (void*)lpm_lookup_ipv6_8stride_avx2;
    case SIMD_AVX:
        return (void*)lpm_lookup_ipv6_8stride_avx;
    case SIMD_SSE4_2:
        return (void*)lpm_lookup_ipv6_8stride_sse42;
    case SIMD_SSE2:
        return (void*)lpm_lookup_ipv6_8stride_sse2;
    case SIMD_SCALAR:
    default:
        return (void*)lpm_lookup_ipv6_8stride_scalar;
    }
}

/* Internal ifunc-dispatched lookup */
static uint32_t lpm_lookup_ipv6_8stride_internal(const lpm_trie_t *trie, const uint8_t *addr)
    __attribute__((ifunc("lpm_ipv6_8stride_single_resolver")));

/* Public API */
uint32_t lpm_lookup_ipv6_8stride(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr || trie->max_depth != LPM_IPV6_MAX_DEPTH) return LPM_INVALID_NEXT_HOP;
    return lpm_lookup_ipv6_8stride_internal(trie, addr);
}

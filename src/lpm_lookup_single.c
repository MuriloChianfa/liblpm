#include <stdint.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../include/lpm.h"
#include <dynemit/core.h>

/*
 * ============================================================================
 * SIMD-Optimized Lookup Functions - Interleaved Layout
 * 
 * struct lpm_entry { child_and_valid, next_hop } = 8 bytes
 * Single access to entry gets both fields in one cache line
 * ============================================================================
 */

/* One lookup step macro - zero function call overhead */
#define DO_STEP(P, N, R, addr, i) do { \
    const struct lpm_entry *_e = &(P)[(N)].entries[(addr)[(i)]]; \
    uint32_t _cv = _e->child_and_valid; \
    if (_cv & LPM_VALID_FLAG) (R) = _e->next_hop; \
    (N) = _cv & LPM_CHILD_MASK; \
} while(0)

/* IPv6 single lookup - branchless, no software prefetch overhead */
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

/* IPv4 single lookup - with prefetch (helps for short 4-level traversal) */
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

__attribute__((hot))
uint32_t lpm_lookup_single_sse2(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    uint32_t R;
    
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        R = lookup_ipv4_unrolled(P, N, addr);
    } else {
        R = lookup_ipv6_unrolled(P, N, addr);
    }
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

/* 
 * True software pipelined batch - interleave memory accesses across addresses
 * While one lookup waits for memory, work on another
 */
__attribute__((hot))
void lpm_lookup_batch_sse2(const struct lpm_trie *trie, const uint8_t **addrs,
                          uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint8_t max = trie->max_depth >> 3;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Process 4 in parallel with interleaved memory accesses */
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        uint32_t n0 = root, n1 = root, n2 = root, n3 = root;
        uint32_t r0 = LPM_INVALID_NEXT_HOP, r1 = LPM_INVALID_NEXT_HOP;
        uint32_t r2 = LPM_INVALID_NEXT_HOP, r3 = LPM_INVALID_NEXT_HOP;
        const uint8_t *a0 = addrs[i], *a1 = addrs[i+1], *a2 = addrs[i+2], *a3 = addrs[i+3];
        
        for (uint8_t d = 0; d < max; d++) {
            /* Prefetch next level for all 4 */
            if (n0) _mm_prefetch((const char*)&P[n0].entries[a0[d]], _MM_HINT_T0);
            if (n1) _mm_prefetch((const char*)&P[n1].entries[a1[d]], _MM_HINT_T0);
            if (n2) _mm_prefetch((const char*)&P[n2].entries[a2[d]], _MM_HINT_T0);
            if (n3) _mm_prefetch((const char*)&P[n3].entries[a3[d]], _MM_HINT_T0);
            
            /* Process all 4 - interleaved memory access pattern */
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
            
            /* Early exit if all done */
            if (!n0 && !n1 && !n2 && !n3) break;
        }
        
        next_hops[i]   = (r0 != LPM_INVALID_NEXT_HOP) ? r0 : def;
        next_hops[i+1] = (r1 != LPM_INVALID_NEXT_HOP) ? r1 : def;
        next_hops[i+2] = (r2 != LPM_INVALID_NEXT_HOP) ? r2 : def;
        next_hops[i+3] = (r3 != LPM_INVALID_NEXT_HOP) ? r3 : def;
    }
    
    /* Remainder */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_single_sse2(trie, addrs[i]);
    }
}

/* SSE4.2 versions - same as SSE2 but compiled with SSE4.2 target */
__attribute__((hot, target("sse4.2")))
uint32_t lpm_lookup_single_sse42(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    uint32_t R;
    
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        R = lookup_ipv4_unrolled(P, N, addr);
    } else {
        R = lookup_ipv6_unrolled(P, N, addr);
    }
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("sse4.2")))
void lpm_lookup_batch_sse42(const struct lpm_trie *trie, const uint8_t **addrs,
                           uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint8_t max = trie->max_depth >> 3;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Process 4 in parallel with interleaved memory accesses */
    size_t i = 0;
    for (; i + 3 < count; i += 4) {
        uint32_t n0 = root, n1 = root, n2 = root, n3 = root;
        uint32_t r0 = LPM_INVALID_NEXT_HOP, r1 = LPM_INVALID_NEXT_HOP;
        uint32_t r2 = LPM_INVALID_NEXT_HOP, r3 = LPM_INVALID_NEXT_HOP;
        const uint8_t *a0 = addrs[i], *a1 = addrs[i+1], *a2 = addrs[i+2], *a3 = addrs[i+3];
        
        for (uint8_t d = 0; d < max; d++) {
            /* Prefetch next level for all 4 */
            if (n0) _mm_prefetch((const char*)&P[n0].entries[a0[d]], _MM_HINT_T0);
            if (n1) _mm_prefetch((const char*)&P[n1].entries[a1[d]], _MM_HINT_T0);
            if (n2) _mm_prefetch((const char*)&P[n2].entries[a2[d]], _MM_HINT_T0);
            if (n3) _mm_prefetch((const char*)&P[n3].entries[a3[d]], _MM_HINT_T0);
            
            /* Process all 4 - interleaved memory access pattern */
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
            
            /* Early exit if all done */
            if (!n0 && !n1 && !n2 && !n3) break;
        }
        
        next_hops[i]   = (r0 != LPM_INVALID_NEXT_HOP) ? r0 : def;
        next_hops[i+1] = (r1 != LPM_INVALID_NEXT_HOP) ? r1 : def;
        next_hops[i+2] = (r2 != LPM_INVALID_NEXT_HOP) ? r2 : def;
        next_hops[i+3] = (r3 != LPM_INVALID_NEXT_HOP) ? r3 : def;
    }
    
    /* Remainder */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_single_sse42(trie, addrs[i]);
    }
}

/* AVX versions - same as AVX2 but compiled with AVX-only target */
__attribute__((hot, target("avx")))
uint32_t lpm_lookup_single_avx(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    uint32_t R;
    
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        R = lookup_ipv4_unrolled(P, N, addr);
    } else {
        R = lookup_ipv6_unrolled(P, N, addr);
    }
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

__attribute__((hot, target("avx")))
void lpm_lookup_batch_avx(const struct lpm_trie *trie, const uint8_t **addrs,
                         uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint8_t max = trie->max_depth >> 3;
    const uint32_t def = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Process 8 in parallel - AVX allows better throughput */
    size_t i = 0;
    for (; i + 7 < count; i += 8) {
        uint32_t n[8], r[8];
        const uint8_t *a[8];
        
        for (int j = 0; j < 8; j++) {
            n[j] = root;
            r[j] = LPM_INVALID_NEXT_HOP;
            a[j] = addrs[i+j];
        }
        
        for (uint8_t d = 0; d < max; d++) {
            /* Prefetch and process all 8 */
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
        next_hops[i] = lpm_lookup_single_avx(trie, addrs[i]);
    }
}

__attribute__((hot, target("avx2")))
uint32_t lpm_lookup_single_avx2(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    uint32_t R;
    
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        R = lookup_ipv4_unrolled(P, N, addr);
    } else {
        R = lookup_ipv6_unrolled(P, N, addr);
    }
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

/* 
 * True software pipelined batch - 8 addresses interleaved
 */
__attribute__((hot, target("avx2")))
void lpm_lookup_batch_avx2(const struct lpm_trie *trie, const uint8_t **addrs,
                          uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint8_t max = trie->max_depth >> 3;
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
        
        for (uint8_t d = 0; d < max; d++) {
            /* Prefetch all */
            for (int j = 0; j < 8; j++) {
                if (n[j]) _mm_prefetch((const char*)&P[n[j]].entries[a[j][d]], _MM_HINT_T0);
            }
            
            /* Process all 8 interleaved */
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
        next_hops[i] = lpm_lookup_single_avx2(trie, addrs[i]);
    }
}

__attribute__((hot, target("avx512f")))
uint32_t lpm_lookup_single_avx512(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    uint32_t R;
    
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        R = lookup_ipv4_unrolled(P, N, addr);
    } else {
        R = lookup_ipv6_unrolled(P, N, addr);
    }
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

/* 
 * True software pipelined batch - 16 addresses interleaved 
 */
__attribute__((hot, target("avx512f")))
void lpm_lookup_batch_avx512(const struct lpm_trie *trie, const uint8_t **addrs,
                            uint32_t *next_hops, size_t count)
{
    const lpm_node_t * restrict P = trie->node_pool;
    const uint32_t root = trie->root_idx;
    const uint8_t max = trie->max_depth >> 3;
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
        
        for (uint8_t d = 0; d < max; d++) {
            /* Prefetch all */
            for (int j = 0; j < 16; j++) {
                if (n[j]) _mm_prefetch((const char*)&P[n[j]].entries[a[j][d]], _MM_HINT_T0);
            }
            
            /* Process all 16 interleaved */
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
        next_hops[i] = lpm_lookup_single_avx512(trie, addrs[i]);
    }
}

/* Scalar fallback for single lookup */
__attribute__((hot))
static uint32_t lpm_lookup_single_scalar(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t * restrict P = trie->node_pool;
    uint32_t N = trie->root_idx;
    uint32_t R;
    
    if (trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        R = lookup_ipv4_unrolled(P, N, addr);
    } else {
        R = lookup_ipv6_unrolled(P, N, addr);
    }
    
    return (R != LPM_INVALID_NEXT_HOP) ? R :
           (trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP);
}

/* Scalar fallback for batch lookup */
__attribute__((hot))
static void lpm_lookup_batch_scalar(const struct lpm_trie *trie, const uint8_t **addrs,
                                    uint32_t *next_hops, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        next_hops[i] = lpm_lookup_single_scalar(trie, addrs[i]);
    }
}

/* ============================================================================
 * ifunc Resolvers - Called once at program load time
 * ============================================================================ */

typedef uint32_t (*lpm_single_func_t)(const struct lpm_trie *, const uint8_t *);
typedef void (*lpm_batch_func_t)(const struct lpm_trie *, const uint8_t **, uint32_t *, size_t);

static lpm_single_func_t lpm_single_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_single_avx512;
    case SIMD_AVX2:
        return lpm_lookup_single_avx2;
    case SIMD_AVX:
        return lpm_lookup_single_avx;
    case SIMD_SSE4_2:
        return lpm_lookup_single_sse42;
    case SIMD_SSE2:
        return lpm_lookup_single_sse2;
    case SIMD_SCALAR:
    default:
        return lpm_lookup_single_scalar;
    }
}

static lpm_batch_func_t lpm_batch_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_batch_avx512;
    case SIMD_AVX2:
        return lpm_lookup_batch_avx2;
    case SIMD_AVX:
        return lpm_lookup_batch_avx;
    case SIMD_SSE4_2:
        return lpm_lookup_batch_sse42;
    case SIMD_SSE2:
        return lpm_lookup_batch_sse2;
    case SIMD_SCALAR:
    default:
        return lpm_lookup_batch_scalar;
    }
}

/* 
 * Internal single lookup with ifunc dispatch
 * Zero-overhead dispatch resolved at program load time
 */
uint32_t lpm_lookup_single_simd(const struct lpm_trie *trie, const uint8_t *addr)
    __attribute__((ifunc("lpm_single_resolver")));

/* 
 * Internal batch lookup with ifunc dispatch
 * Zero-overhead dispatch resolved at program load time
 */
void lpm_lookup_batch_simd(const struct lpm_trie *trie, const uint8_t **addrs,
                           uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_batch_resolver")));

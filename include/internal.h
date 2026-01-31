/*
 * liblpm Internal Header
 * Shared declarations for algorithm implementations
 */
#ifndef _LPM_INTERNAL_H_
#define _LPM_INTERNAL_H_

#include "lpm.h"
#include <dynemit/core.h>
#include <dynemit/err.h>

/* Conditional SIMD detection based on build configuration */
#ifdef LPM_TS_RESOLVERS
    /* Thread-safe version for dlopen() contexts, plugins */
    #define LPM_DETECT_SIMD() detect_simd_level_ts()
#else
    /* Direct version for standard C programs (no atomic overhead) */
    #define LPM_DETECT_SIMD() detect_simd_level()
#endif

/* Algorithm-specific headers */
#include "algo/4stride8.h"
#include "algo/6stride8.h"
#include "algo/dir24.h"
#include "algo/wide16.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Internal Node Pool Management
 * ============================================================================ */

/* Allocate a new 8-bit stride node from the pool */
uint32_t node_alloc(lpm_trie_t *trie);

/* Allocate a new 16-bit wide stride node from the pool */
uint32_t wide_node_alloc(lpm_trie_t *trie);

/* ============================================================================
 * Algorithm Type Enumeration
 * ============================================================================ */

typedef enum {
    LPM_ALGO_STRIDE8 = 0,     /* 8-bit stride (generic) */
    LPM_ALGO_DIR24 = 1,       /* DIR-24-8 (IPv4 only) */
    LPM_ALGO_WIDE16 = 2,      /* 16-bit wide stride (IPv6 only) */
} lpm_algo_t;

/* ============================================================================
 * Shared Utility Functions
 * ============================================================================ */

/* Cache management */
void lpm_cache_invalidate(lpm_trie_t *trie);

/* Fast inline hash for hot cache */
static inline uint64_t lpm_fast_hash(const uint8_t *addr, uint8_t len)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (uint8_t i = 0; i < len; i++) {
        h ^= addr[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* Cache probe */
static inline uint32_t lpm_cache_probe(const lpm_trie_t *trie, const uint8_t *addr,
                                        uint8_t len, bool *hit)
{
    if (!trie->hot_cache) {
        *hit = false;
        return 0;
    }

    uint64_t h = lpm_fast_hash(addr, len);
    uint32_t idx = h & (LPM_HOT_CACHE_SIZE - 1);
    const struct lpm_cache_entry *e = &trie->hot_cache[idx];
    *hit = (e->addr_hash == h);
    return e->next_hop;
}

/* Cache store */
static inline void lpm_cache_store(lpm_trie_t *trie, const uint8_t *addr,
                                   uint8_t len, uint32_t next_hop)
{
    if (!trie->hot_cache) return;
    uint64_t h = lpm_fast_hash(addr, len);
    uint32_t idx = h & (LPM_HOT_CACHE_SIZE - 1);
    trie->hot_cache[idx].addr_hash = h;
    trie->hot_cache[idx].next_hop = next_hop;
}

#ifdef __cplusplus
}
#endif

#endif /* _LPM_INTERNAL_H_ */

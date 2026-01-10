/*
 * IPv4 DIR-24-8 Algorithm
 * Uses 24-8 stride pattern for IPv4 with compact 4-byte entries
 * First 24 bits: Single table lookup (16.7M entries, 64MB)
 * Last 8 bits: 8-bit table for longer routes (256 entries per group)
 * Total: 1-2 memory accesses for any lookup!
 *
 * SIMD optimizations using GNU ifunc for zero-overhead dispatch:
 * - AVX2: Process 8 lookups in parallel using gather instructions
 * - AVX512: Process 16 lookups in parallel using gather instructions
 * 
 * Dispatch is resolved once at program load time via ifunc, eliminating
 * runtime branch overhead in the hot path.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../include/lpm.h"
#include <dynemit/core.h>

/* Default number of tbl8 groups to allocate */
#define LPM_TBL8_DEFAULT_GROUPS 256
#define LPM_TBL8_GROUP_ENTRIES 256

/* 
 * Lookup function with DIR-24-8 format
 * Key optimizations:
 * - Single branch for common case (most routes are /8-/24)
 * - Branchless extended flag check
 * - Compiler hints for branch prediction
 */
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

/* Helper for tbl8 lookup from group index and last byte */
__attribute__((hot, always_inline))
static inline uint32_t tbl8_lookup_inline(const struct lpm_tbl8_entry * restrict tbl8,
                                          uint32_t tbl8_group, uint8_t last_byte)
{
    uint32_t tbl8_idx = (tbl8_group << 8) | last_byte;
    uint32_t tbl8_data = tbl8[tbl8_idx].data;
    return (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
}

uint32_t lpm_lookup_ipv4_dir24(const lpm_trie_t *trie, const uint8_t addr[4])
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

/* Allocate a new tbl8 group */
static int32_t tbl8_group_alloc(lpm_trie_t *trie)
{
    if (trie->tbl8_groups_used >= trie->tbl8_num_groups) {
        /* Need to grow the tbl8 array */
        uint32_t new_groups = trie->tbl8_num_groups * 2;
        struct lpm_tbl8_entry *new_tbl8 = realloc(trie->tbl8_groups, 
                                                   new_groups * LPM_TBL8_GROUP_ENTRIES * sizeof(struct lpm_tbl8_entry));
        if (!new_tbl8) return -1;
        
        /* Initialize new groups */
        memset(&new_tbl8[trie->tbl8_num_groups * LPM_TBL8_GROUP_ENTRIES], 0,
               (new_groups - trie->tbl8_num_groups) * LPM_TBL8_GROUP_ENTRIES * sizeof(struct lpm_tbl8_entry));
        
        trie->tbl8_groups = new_tbl8;
        trie->tbl8_num_groups = new_groups;
    }
    
    return trie->tbl8_groups_used++;
}

/* Add prefix with DIR-24-8 support (compact format) */
int lpm_add_ipv4_dir24(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie || !prefix || prefix_len > 32 || !trie->dir24_table) return -1;
    if (next_hop & 0xC0000000) return -1;  /* Next hop must fit in 30 bits */
    
    /* Handle default route */
    if (prefix_len == 0) {
        trie->has_default_route = true;
        trie->default_next_hop = next_hop;
        trie->num_prefixes++;
        return 0;
    }
    
    /* Routes up to /24: Store directly in DIR24 table */
    if (prefix_len <= 24) {
        uint32_t base_idx = ((uint32_t)prefix[0] << 16) | ((uint32_t)prefix[1] << 8);
        if (prefix_len >= 16) {
            base_idx |= prefix[2];
        }
        
        /* Mask off bits beyond prefix length */
        if (prefix_len < 24) {
            uint32_t mask = ~((1U << (24 - prefix_len)) - 1);
            base_idx &= mask;
        }
        
        /* Calculate how many entries to fill (prefix expansion) */
        uint32_t count = 1U << (24 - prefix_len);
        
        /* Fill all matching entries */
        for (uint32_t i = 0; i < count; i++) {
            uint32_t idx = base_idx + i;
            struct lpm_dir24_entry *entry = &trie->dir24_table[idx];
            
            /* Only overwrite if this is a more specific prefix or not extended */
            if (!(entry->data & LPM_DIR24_EXT_FLAG)) {
                entry->data = LPM_DIR24_VALID_FLAG | next_hop;
            }
        }
        
        trie->num_prefixes++;
        return 0;
    }
    
    /* Routes longer than /24: Need tbl8 */
    uint32_t dir24_idx = ((uint32_t)prefix[0] << 16) | ((uint32_t)prefix[1] << 8) | prefix[2];
    struct lpm_dir24_entry *dir_entry = &trie->dir24_table[dir24_idx];
    
    uint32_t tbl8_group;
    
    /* Check if we need to allocate a tbl8 group */
    if (!(dir_entry->data & LPM_DIR24_EXT_FLAG)) {
        /* Allocate new tbl8 group */
        int32_t new_group = tbl8_group_alloc(trie);
        if (new_group < 0) return -1;
        tbl8_group = new_group;
        
        /* If dir24 entry had a valid next hop, copy it to all tbl8 entries */
        struct lpm_tbl8_entry *tbl8 = &trie->tbl8_groups[tbl8_group * LPM_TBL8_GROUP_ENTRIES];
        if (dir_entry->data & LPM_DIR24_VALID_FLAG) {
            uint32_t parent_nh = dir_entry->data & LPM_DIR24_NH_MASK;
            for (int i = 0; i < LPM_TBL8_GROUP_ENTRIES; i++) {
                tbl8[i].data = LPM_DIR24_VALID_FLAG | parent_nh;
            }
        }
        
        /* Update dir24 entry to point to tbl8 group */
        dir_entry->data = LPM_DIR24_VALID_FLAG | LPM_DIR24_EXT_FLAG | tbl8_group;
    } else {
        tbl8_group = dir_entry->data & LPM_DIR24_NH_MASK;
    }
    
    /* Add route to tbl8 */
    struct lpm_tbl8_entry *tbl8 = &trie->tbl8_groups[tbl8_group * LPM_TBL8_GROUP_ENTRIES];
    uint8_t last_byte = prefix[3];
    uint8_t remaining_bits = prefix_len - 24;  /* 1-8 bits */
    
    if (remaining_bits == 8) {
        /* Exact /32 route */
        tbl8[last_byte].data = LPM_DIR24_VALID_FLAG | next_hop;
    } else {
        /* Partial last byte - expand to all matching entries */
        uint8_t mask = ~((1 << (8 - remaining_bits)) - 1);
        uint8_t base = last_byte & mask;
        uint32_t count = 1 << (8 - remaining_bits);
        
        for (uint32_t i = 0; i < count; i++) {
            uint8_t idx = base + i;
            tbl8[idx].data = LPM_DIR24_VALID_FLAG | next_hop;
        }
    }
    
    trie->num_prefixes++;
    return 0;
}

/* 
 * Scalar batch lookup for DIR-24-8 with software prefetching
 * Works with pointer array (const uint8_t **)
 */
__attribute__((hot))
static void lpm_lookup_batch_ipv4_dir24_scalar_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                                     uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->default_next_hop;
    const bool has_default = trie->has_default_route;
    
    for (size_t i = 0; i < count; i++) {
        /* Prefetch next entry */
        if (i + 8 < count) {
            const uint8_t *pf = addrs[i + 8];
            uint32_t pf_idx = ((uint32_t)pf[0] << 16) | ((uint32_t)pf[1] << 8) | pf[2];
            __builtin_prefetch(&dir24[pf_idx], 0, 0);
        }
        
        uint32_t r = dir24_lookup_inline(dir24, tbl8, addrs[i]);
        next_hops[i] = (r == LPM_INVALID_NEXT_HOP && has_default) ? default_nh : r;
    }
}

/*
 * SSE4.2 batch lookup - optimized scalar with SSE4.2 target
 * Works with pointer array (const uint8_t **)
 */
__attribute__((hot, target("sse4.2")))
static void lpm_lookup_batch_ipv4_dir24_sse42_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                                     uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->default_next_hop;
    const bool has_default = trie->has_default_route;
    
    for (size_t i = 0; i < count; i++) {
        /* Prefetch next entry */
        if (i + 8 < count) {
            const uint8_t *pf = addrs[i + 8];
            uint32_t pf_idx = ((uint32_t)pf[0] << 16) | ((uint32_t)pf[1] << 8) | pf[2];
            __builtin_prefetch(&dir24[pf_idx], 0, 0);
        }
        
        uint32_t r = dir24_lookup_inline(dir24, tbl8, addrs[i]);
        next_hops[i] = (r == LPM_INVALID_NEXT_HOP && has_default) ? default_nh : r;
    }
}

/*
 * AVX batch lookup - optimized scalar with AVX target
 * Works with pointer array (const uint8_t **)
 * Note: Pure AVX doesn't have gather (that's AVX2), so we use optimized scalar
 */
__attribute__((hot, target("avx")))
static void lpm_lookup_batch_ipv4_dir24_avx_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                                  uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->default_next_hop;
    const bool has_default = trie->has_default_route;
    
    for (size_t i = 0; i < count; i++) {
        /* Prefetch next entry */
        if (i + 8 < count) {
            const uint8_t *pf = addrs[i + 8];
            uint32_t pf_idx = ((uint32_t)pf[0] << 16) | ((uint32_t)pf[1] << 8) | pf[2];
            __builtin_prefetch(&dir24[pf_idx], 0, 0);
        }
        
        uint32_t r = dir24_lookup_inline(dir24, tbl8, addrs[i]);
        next_hops[i] = (r == LPM_INVALID_NEXT_HOP && has_default) ? default_nh : r;
    }
}

/*
 * AVX2 batch lookup using gather instructions - process 8 lookups in parallel!
 * Works with pointer array (const uint8_t **)
 */
__attribute__((hot, target("avx2")))
static void lpm_lookup_batch_ipv4_dir24_avx2_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                                   uint32_t *next_hops, size_t count)
{
    const uint32_t * restrict dir24 = (const uint32_t *)trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Constants for masking */
    const __m256i valid_mask = _mm256_set1_epi32(LPM_DIR24_VALID_FLAG);
    const __m256i ext_mask = _mm256_set1_epi32(LPM_DIR24_EXT_FLAG);
    const __m256i nh_mask = _mm256_set1_epi32(LPM_DIR24_NH_MASK);
    const __m256i invalid_nh_vec = _mm256_set1_epi32(LPM_INVALID_NEXT_HOP);
    const __m256i default_vec = _mm256_set1_epi32(default_nh);
    
    size_t i = 0;
    
    /* Process 8 at a time using AVX2 gather */
    for (; i + 8 <= count; i += 8) {
        /* Load addresses and compute indices */
        const uint8_t *a0 = addrs[i+0], *a1 = addrs[i+1], *a2 = addrs[i+2], *a3 = addrs[i+3];
        const uint8_t *a4 = addrs[i+4], *a5 = addrs[i+5], *a6 = addrs[i+6], *a7 = addrs[i+7];
        
        /* Compute 8 DIR24 indices: (addr[0] << 16) | (addr[1] << 8) | addr[2] */
        __m256i indices = _mm256_set_epi32(
            ((uint32_t)a7[0] << 16) | ((uint32_t)a7[1] << 8) | a7[2],
            ((uint32_t)a6[0] << 16) | ((uint32_t)a6[1] << 8) | a6[2],
            ((uint32_t)a5[0] << 16) | ((uint32_t)a5[1] << 8) | a5[2],
            ((uint32_t)a4[0] << 16) | ((uint32_t)a4[1] << 8) | a4[2],
            ((uint32_t)a3[0] << 16) | ((uint32_t)a3[1] << 8) | a3[2],
            ((uint32_t)a2[0] << 16) | ((uint32_t)a2[1] << 8) | a2[2],
            ((uint32_t)a1[0] << 16) | ((uint32_t)a1[1] << 8) | a1[2],
            ((uint32_t)a0[0] << 16) | ((uint32_t)a0[1] << 8) | a0[2]
        );
        
        /* Prefetch next batch of address pointers */
        if (i + 16 <= count) {
            _mm_prefetch(&addrs[i + 8], _MM_HINT_T0);
        }
        
        /* GATHER: Fetch 8 DIR24 entries in ONE instruction! */
        __m256i data = _mm256_i32gather_epi32((const int *)dir24, indices, 4);
        
        /* Check which entries need tbl8 lookup (extended flag set) */
        __m256i is_extended = _mm256_and_si256(data, ext_mask);
        int ext_bits = _mm256_movemask_ps(_mm256_castsi256_ps(
            _mm256_slli_epi32(is_extended, 1))); /* Move bit 30 to bit 31 for movemask */
        
        /* Extract next hops for non-extended entries */
        __m256i results = _mm256_and_si256(data, nh_mask);
        
        /* Check valid flag */
        __m256i is_valid = _mm256_and_si256(data, valid_mask);
        __m256i valid_results = _mm256_blendv_epi8(invalid_nh_vec, results,
            _mm256_cmpeq_epi32(is_valid, valid_mask));
        
        /* Handle extended entries (tbl8 lookups) - scalar fallback for these */
        if (ext_bits) {
            uint32_t temp_results[8] __attribute__((aligned(32)));
            uint32_t temp_data[8] __attribute__((aligned(32)));
            _mm256_store_si256((__m256i *)temp_results, valid_results);
            _mm256_store_si256((__m256i *)temp_data, data);
            
            const uint8_t *addr_arr[8] = {a0, a1, a2, a3, a4, a5, a6, a7};
            for (int j = 0; j < 8; j++) {
                if (ext_bits & (1 << j)) {
                    uint32_t d = temp_data[j];
                    uint32_t tbl8_group = d & LPM_DIR24_NH_MASK;
                    uint32_t tbl8_idx = (tbl8_group << 8) | addr_arr[j][3];
                    uint32_t tbl8_data = tbl8[tbl8_idx].data;
                    temp_results[j] = (tbl8_data & LPM_DIR24_VALID_FLAG) ? 
                                      (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
                }
            }
            valid_results = _mm256_load_si256((const __m256i *)temp_results);
        }
        
        /* Apply default route where needed */
        __m256i need_default = _mm256_cmpeq_epi32(valid_results, invalid_nh_vec);
        valid_results = _mm256_blendv_epi8(valid_results, default_vec, need_default);
        
        /* Store results */
        _mm256_storeu_si256((__m256i *)&next_hops[i], valid_results);
    }
    
    /* Handle remaining elements with scalar code */
    const struct lpm_dir24_entry *dir24_e = trie->dir24_table;
    for (; i < count; i++) {
        uint32_t r = dir24_lookup_inline(dir24_e, tbl8, addrs[i]);
        next_hops[i] = (r == LPM_INVALID_NEXT_HOP) ? default_nh : r;
    }
}

/*
 * AVX512 batch lookup using gather instructions - process 16 lookups in parallel!
 * Works with pointer array (const uint8_t **)
 */
__attribute__((hot, target("avx512f")))
static void lpm_lookup_batch_ipv4_dir24_avx512_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                                     uint32_t *next_hops, size_t count)
{
    const uint32_t * restrict dir24 = (const uint32_t *)trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Constants */
    const __m512i valid_mask = _mm512_set1_epi32(LPM_DIR24_VALID_FLAG);
    const __m512i ext_mask = _mm512_set1_epi32(LPM_DIR24_EXT_FLAG);
    const __m512i nh_mask = _mm512_set1_epi32(LPM_DIR24_NH_MASK);
    const __m512i invalid_nh_vec = _mm512_set1_epi32(LPM_INVALID_NEXT_HOP);
    const __m512i default_vec = _mm512_set1_epi32(default_nh);
    
    size_t i = 0;
    
    /* Process 16 at a time using AVX512 gather */
    for (; i + 16 <= count; i += 16) {
        /* Load addresses */
        const uint8_t *a[16];
        for (int j = 0; j < 16; j++) a[j] = addrs[i + j];
        
        /* Compute 16 DIR24 indices */
        __m512i indices = _mm512_set_epi32(
            ((uint32_t)a[15][0] << 16) | ((uint32_t)a[15][1] << 8) | a[15][2],
            ((uint32_t)a[14][0] << 16) | ((uint32_t)a[14][1] << 8) | a[14][2],
            ((uint32_t)a[13][0] << 16) | ((uint32_t)a[13][1] << 8) | a[13][2],
            ((uint32_t)a[12][0] << 16) | ((uint32_t)a[12][1] << 8) | a[12][2],
            ((uint32_t)a[11][0] << 16) | ((uint32_t)a[11][1] << 8) | a[11][2],
            ((uint32_t)a[10][0] << 16) | ((uint32_t)a[10][1] << 8) | a[10][2],
            ((uint32_t)a[9][0] << 16) | ((uint32_t)a[9][1] << 8) | a[9][2],
            ((uint32_t)a[8][0] << 16) | ((uint32_t)a[8][1] << 8) | a[8][2],
            ((uint32_t)a[7][0] << 16) | ((uint32_t)a[7][1] << 8) | a[7][2],
            ((uint32_t)a[6][0] << 16) | ((uint32_t)a[6][1] << 8) | a[6][2],
            ((uint32_t)a[5][0] << 16) | ((uint32_t)a[5][1] << 8) | a[5][2],
            ((uint32_t)a[4][0] << 16) | ((uint32_t)a[4][1] << 8) | a[4][2],
            ((uint32_t)a[3][0] << 16) | ((uint32_t)a[3][1] << 8) | a[3][2],
            ((uint32_t)a[2][0] << 16) | ((uint32_t)a[2][1] << 8) | a[2][2],
            ((uint32_t)a[1][0] << 16) | ((uint32_t)a[1][1] << 8) | a[1][2],
            ((uint32_t)a[0][0] << 16) | ((uint32_t)a[0][1] << 8) | a[0][2]
        );
        
        /* Prefetch next batch */
        if (i + 32 <= count) {
            _mm_prefetch(&addrs[i + 16], _MM_HINT_T0);
        }
        
        /* GATHER: Fetch 16 DIR24 entries in ONE instruction! */
        __m512i data = _mm512_i32gather_epi32(indices, dir24, 4);
        
        /* Check which entries need tbl8 lookup */
        __mmask16 ext_bits = _mm512_test_epi32_mask(data, ext_mask);
        
        /* Extract next hops */
        __m512i results = _mm512_and_si512(data, nh_mask);
        
        /* Check valid flag and set invalid where not valid */
        __mmask16 valid_bits = _mm512_test_epi32_mask(data, valid_mask);
        results = _mm512_mask_blend_epi32(valid_bits, invalid_nh_vec, results);
        
        /* Handle extended entries (tbl8 lookups) */
        if (ext_bits) {
            uint32_t temp_results[16] __attribute__((aligned(64)));
            uint32_t temp_data[16] __attribute__((aligned(64)));
            _mm512_store_si512(temp_results, results);
            _mm512_store_si512(temp_data, data);
            
            for (int j = 0; j < 16; j++) {
                if (ext_bits & (1 << j)) {
                    uint32_t d = temp_data[j];
                    uint32_t tbl8_group = d & LPM_DIR24_NH_MASK;
                    uint32_t tbl8_idx = (tbl8_group << 8) | a[j][3];
                    uint32_t tbl8_data = tbl8[tbl8_idx].data;
                    temp_results[j] = (tbl8_data & LPM_DIR24_VALID_FLAG) ? 
                                      (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
                }
            }
            results = _mm512_load_si512(temp_results);
        }
        
        /* Apply default route where needed */
        __mmask16 need_default = _mm512_cmpeq_epi32_mask(results, invalid_nh_vec);
        results = _mm512_mask_blend_epi32(need_default, results, default_vec);
        
        /* Store results */
        _mm512_storeu_si512(&next_hops[i], results);
    }
    
    /* Handle remaining with AVX2 */
    if (i + 8 <= count) {
        lpm_lookup_batch_ipv4_dir24_avx2_ptrs(trie, &addrs[i], &next_hops[i], count - i);
        return;
    }
    
    /* Scalar remainder */
    const struct lpm_dir24_entry *dir24_e = trie->dir24_table;
    for (; i < count; i++) {
        uint32_t r = dir24_lookup_inline(dir24_e, tbl8, addrs[i]);
        next_hops[i] = (r == LPM_INVALID_NEXT_HOP) ? default_nh : r;
    }
}

/* 
 * ifunc resolver for pointer-based batch lookup
 * Called once at program load time - zero runtime dispatch overhead
 */
typedef void (*lpm_batch_ptrs_func_t)(const lpm_trie_t *, const uint8_t **, uint32_t *, size_t);

static lpm_batch_ptrs_func_t lpm_batch_ptrs_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_batch_ipv4_dir24_avx512_ptrs;
    case SIMD_AVX2:
        return lpm_lookup_batch_ipv4_dir24_avx2_ptrs;
    case SIMD_AVX:
        return lpm_lookup_batch_ipv4_dir24_avx_ptrs;
    case SIMD_SSE4_2:
        return lpm_lookup_batch_ipv4_dir24_sse42_ptrs;
    case SIMD_SSE2:
    case SIMD_SCALAR:
    default:
        return lpm_lookup_batch_ipv4_dir24_scalar_ptrs;
    }
}

/* 
 * Batch lookup for pointer arrays (const uint8_t **)
 * Uses ifunc for zero-overhead dispatch resolved at program load time
 */
void lpm_lookup_batch_ipv4_dir24_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                       uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_batch_ptrs_resolver")));

/* 
 * Batch lookup for contiguous arrays
 */
__attribute__((hot))
void lpm_lookup_batch_ipv4_dir24(const lpm_trie_t *trie, const uint8_t (*addrs)[4],
                                  uint32_t *next_hops, size_t count)
{
    /* Contiguous arrays can be cast to pointer arrays for the optimized path */
    lpm_lookup_batch_ipv4_dir24_ptrs(trie, (const uint8_t **)&addrs[0], next_hops, count);
}

/* ============================================================================
 * PURE SIMD BATCH LOOKUP FOR uint32_t IPs
 * 
 * These functions work directly with uint32_t IPs in big-endian (network) byte
 * order, eliminating all pointer dereference overhead.
 * and allows pure SIMD index computation.
 * ============================================================================ */

/*
 * Scalar batch lookup for uint32_t IPs
 */
__attribute__((hot))
static void lpm_lookup_batch_u32_scalar(const lpm_trie_t *trie, const uint32_t *ips,
                                         uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    for (size_t i = 0; i < count; i++) {
        /* IPs are in big-endian (network byte order)
         * IP = 0xC0A80001 means 192.168.0.1
         * DIR24 index = top 24 bits = IP >> 8 */
        uint32_t ip = ips[i];
        uint32_t dir24_idx = ip >> 8;
        uint32_t data = dir24[dir24_idx].data;
        
        uint32_t result;
        if (!(data & LPM_DIR24_EXT_FLAG)) {
            result = (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        } else {
            /* tbl8 lookup: last byte of IP */
            uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
            uint32_t tbl8_idx = (tbl8_group << 8) | (ip & 0xFF);
            uint32_t tbl8_data = tbl8[tbl8_idx].data;
            result = (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        }
        
        next_hops[i] = (result == LPM_INVALID_NEXT_HOP) ? default_nh : result;
    }
}

/*
 * SSE4.2 batch lookup for uint32_t IPs - optimized scalar with SSE4.2 target
 */
__attribute__((hot, target("sse4.2")))
static void lpm_lookup_batch_u32_sse42(const lpm_trie_t *trie, const uint32_t *ips,
                                         uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    for (size_t i = 0; i < count; i++) {
        /* IPs are in big-endian (network byte order)
         * IP = 0xC0A80001 means 192.168.0.1
         * DIR24 index = top 24 bits = IP >> 8 */
        uint32_t ip = ips[i];
        uint32_t dir24_idx = ip >> 8;
        uint32_t data = dir24[dir24_idx].data;
        
        uint32_t result;
        if (!(data & LPM_DIR24_EXT_FLAG)) {
            result = (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        } else {
            /* tbl8 lookup: last byte of IP */
            uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
            uint32_t tbl8_idx = (tbl8_group << 8) | (ip & 0xFF);
            uint32_t tbl8_data = tbl8[tbl8_idx].data;
            result = (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        }
        
        next_hops[i] = (result == LPM_INVALID_NEXT_HOP) ? default_nh : result;
    }
}

/*
 * AVX batch lookup for uint32_t IPs
 * Pure AVX doesn't have gather (that's AVX2), so optimized scalar with prefetching
 */
__attribute__((hot, target("avx")))
static void lpm_lookup_batch_u32_avx(const lpm_trie_t *trie, const uint32_t *ips,
                                       uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    for (size_t i = 0; i < count; i++) {
        /* Prefetch next entry */
        if (i + 8 < count) {
            uint32_t pf_idx = ips[i + 8] >> 8;
            __builtin_prefetch(&dir24[pf_idx], 0, 0);
        }
        
        uint32_t ip = ips[i];
        uint32_t dir24_idx = ip >> 8;
        uint32_t data = dir24[dir24_idx].data;
        
        uint32_t result;
        if (!(data & LPM_DIR24_EXT_FLAG)) {
            result = (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        } else {
            uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
            uint32_t tbl8_idx = (tbl8_group << 8) | (ip & 0xFF);
            uint32_t tbl8_data = tbl8[tbl8_idx].data;
            result = (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        }
        
        next_hops[i] = (result == LPM_INVALID_NEXT_HOP) ? default_nh : result;
    }
}

/*
 * AVX2 pure SIMD batch lookup for uint32_t IPs
 * Processes 8 IPs in parallel with gather instructions
 */
__attribute__((hot, target("avx2")))
static void lpm_lookup_batch_u32_avx2(const lpm_trie_t *trie, const uint32_t *ips,
                                       uint32_t *next_hops, size_t count)
{
    const uint32_t * restrict dir24 = (const uint32_t *)trie->dir24_table;
    const uint32_t * restrict tbl8 = (const uint32_t *)trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Constants */
    const __m256i valid_mask = _mm256_set1_epi32(LPM_DIR24_VALID_FLAG);
    const __m256i ext_mask = _mm256_set1_epi32(LPM_DIR24_EXT_FLAG);
    const __m256i nh_mask = _mm256_set1_epi32(LPM_DIR24_NH_MASK);
    const __m256i invalid_nh_vec = _mm256_set1_epi32(LPM_INVALID_NEXT_HOP);
    const __m256i default_vec = _mm256_set1_epi32(default_nh);
    const __m256i byte_mask = _mm256_set1_epi32(0xFF);
    
    size_t i = 0;
    
    for (; i + 8 <= count; i += 8) {
        /* Load 8 IPs directly - pure SIMD! */
        __m256i ips_vec = _mm256_loadu_si256((const __m256i *)&ips[i]);
        
        /* Prefetch next batch */
        if (i + 16 <= count) {
            _mm_prefetch(&ips[i + 8], _MM_HINT_T0);
        }
        
        /* Compute DIR24 indices: top 24 bits = IP >> 8 */
        __m256i indices = _mm256_srli_epi32(ips_vec, 8);
        
        /* GATHER: Fetch 8 DIR24 entries */
        __m256i data = _mm256_i32gather_epi32((const int *)dir24, indices, 4);
        
        /* Extract next hops and check validity */
        __m256i results = _mm256_and_si256(data, nh_mask);
        __m256i is_valid = _mm256_and_si256(data, valid_mask);
        results = _mm256_blendv_epi8(invalid_nh_vec, results,
            _mm256_cmpeq_epi32(is_valid, valid_mask));
        
        /* Check for extended entries */
        __m256i is_extended = _mm256_and_si256(data, ext_mask);
        int ext_bits = _mm256_movemask_ps(_mm256_castsi256_ps(
            _mm256_slli_epi32(is_extended, 1)));
        
        /* Handle tbl8 lookups if any extended entries */
        if (ext_bits) {
            /* Compute tbl8 indices: (tbl8_group << 8) | (ip & 0xFF) */
            __m256i tbl8_groups = _mm256_and_si256(data, nh_mask);
            __m256i last_bytes = _mm256_and_si256(ips_vec, byte_mask);
            __m256i tbl8_indices = _mm256_or_si256(
                _mm256_slli_epi32(tbl8_groups, 8), last_bytes);
            
            /* Gather tbl8 entries for ALL (we'll blend later) */
            __m256i tbl8_data = _mm256_i32gather_epi32((const int *)tbl8, tbl8_indices, 4);
            
            /* Extract tbl8 results */
            __m256i tbl8_results = _mm256_and_si256(tbl8_data, nh_mask);
            __m256i tbl8_valid = _mm256_and_si256(tbl8_data, valid_mask);
            tbl8_results = _mm256_blendv_epi8(invalid_nh_vec, tbl8_results,
                _mm256_cmpeq_epi32(tbl8_valid, valid_mask));
            
            /* Blend: use tbl8_results where extended, otherwise keep dir24 results */
            __m256i ext_blend_mask = _mm256_cmpeq_epi32(is_extended, ext_mask);
            results = _mm256_blendv_epi8(results, tbl8_results, ext_blend_mask);
        }
        
        /* Apply default route */
        __m256i need_default = _mm256_cmpeq_epi32(results, invalid_nh_vec);
        results = _mm256_blendv_epi8(results, default_vec, need_default);
        
        /* Store results */
        _mm256_storeu_si256((__m256i *)&next_hops[i], results);
    }
    
    /* Scalar remainder */
    for (; i < count; i++) {
        uint32_t ip = ips[i];
        uint32_t dir24_idx = ip >> 8;
        uint32_t data = ((const uint32_t *)trie->dir24_table)[dir24_idx];
        
        uint32_t result;
        if (!(data & LPM_DIR24_EXT_FLAG)) {
            result = (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        } else {
            uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
            uint32_t tbl8_idx = (tbl8_group << 8) | (ip & 0xFF);
            uint32_t tbl8_data = tbl8[tbl8_idx];
            result = (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        }
        next_hops[i] = (result == LPM_INVALID_NEXT_HOP) ? default_nh : result;
    }
}

/*
 * AVX512 pure SIMD batch lookup for uint32_t IPs
 * Processes 16 IPs in parallel with gather instructions
 */
__attribute__((hot, target("avx512f")))
static void lpm_lookup_batch_u32_avx512(const lpm_trie_t *trie, const uint32_t *ips,
                                         uint32_t *next_hops, size_t count)
{
    const uint32_t * restrict dir24 = (const uint32_t *)trie->dir24_table;
    const uint32_t * restrict tbl8 = (const uint32_t *)trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    /* Constants */
    const __m512i valid_mask = _mm512_set1_epi32(LPM_DIR24_VALID_FLAG);
    const __m512i ext_mask = _mm512_set1_epi32(LPM_DIR24_EXT_FLAG);
    const __m512i nh_mask = _mm512_set1_epi32(LPM_DIR24_NH_MASK);
    const __m512i invalid_nh_vec = _mm512_set1_epi32(LPM_INVALID_NEXT_HOP);
    const __m512i default_vec = _mm512_set1_epi32(default_nh);
    const __m512i byte_mask = _mm512_set1_epi32(0xFF);
    
    size_t i = 0;
    
    for (; i + 16 <= count; i += 16) {
        /* Load 16 IPs directly into AVX512 register - PURE SIMD! */
        __m512i ips_vec = _mm512_loadu_si512(&ips[i]);
        
        /* Prefetch next batch of IPs */
        if (i + 32 <= count) {
            _mm_prefetch(&ips[i + 16], _MM_HINT_T0);
        }
        
        /* Compute DIR24 indices: top 24 bits = IP >> 8 
         * This single SIMD operation replaces all the pointer dereferences! */
        __m512i indices = _mm512_srli_epi32(ips_vec, 8);
        
        /* GATHER: Fetch 16 DIR24 entries in ONE instruction! */
        __m512i data = _mm512_i32gather_epi32(indices, dir24, 4);
        
        /* Extract next hops */
        __m512i results = _mm512_and_si512(data, nh_mask);
        
        /* Check validity and set invalid where not valid */
        __mmask16 valid_bits = _mm512_test_epi32_mask(data, valid_mask);
        results = _mm512_mask_blend_epi32(valid_bits, invalid_nh_vec, results);
        
        /* Check for extended entries (need tbl8 lookup) */
        __mmask16 ext_bits = _mm512_test_epi32_mask(data, ext_mask);
        
        /* Handle tbl8 lookups with SIMD - no scalar fallback! */
        if (ext_bits) {
            /* Compute tbl8 indices: (tbl8_group << 8) | (ip & 0xFF) */
            __m512i tbl8_groups = _mm512_and_si512(data, nh_mask);
            __m512i last_bytes = _mm512_and_si512(ips_vec, byte_mask);
            __m512i tbl8_indices = _mm512_or_si512(
                _mm512_slli_epi32(tbl8_groups, 8), last_bytes);
            
            /* Masked gather: only fetch tbl8 entries where needed */
            __m512i tbl8_data = _mm512_mask_i32gather_epi32(
                results, ext_bits, tbl8_indices, tbl8, 4);
            
            /* Extract tbl8 next hops */
            __m512i tbl8_results = _mm512_and_si512(tbl8_data, nh_mask);
            
            /* Check tbl8 validity */
            __mmask16 tbl8_valid = _mm512_mask_test_epi32_mask(ext_bits, tbl8_data, valid_mask);
            
            /* Update results: use tbl8 result where valid, else invalid */
            results = _mm512_mask_blend_epi32(tbl8_valid, 
                _mm512_mask_blend_epi32(ext_bits, results, invalid_nh_vec),
                tbl8_results);
        }
        
        /* Apply default route where result is invalid */
        __mmask16 need_default = _mm512_cmpeq_epi32_mask(results, invalid_nh_vec);
        results = _mm512_mask_blend_epi32(need_default, results, default_vec);
        
        /* Store 16 results */
        _mm512_storeu_si512(&next_hops[i], results);
    }
    
    /* Handle remaining with AVX2 */
    if (i + 8 <= count) {
        lpm_lookup_batch_u32_avx2(trie, &ips[i], &next_hops[i], count - i);
        return;
    }
    
    /* Scalar remainder */
    for (; i < count; i++) {
        uint32_t ip = ips[i];
        uint32_t dir24_idx = ip >> 8;
        uint32_t data = dir24[dir24_idx];
        
        uint32_t result;
        if (!(data & LPM_DIR24_EXT_FLAG)) {
            result = (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        } else {
            uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
            uint32_t tbl8_idx = (tbl8_group << 8) | (ip & 0xFF);
            uint32_t tbl8_data = tbl8[tbl8_idx];
            result = (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
        }
        next_hops[i] = (result == LPM_INVALID_NEXT_HOP) ? default_nh : result;
    }
}

/*
 * ifunc resolver for uint32_t IP batch lookup
 * Called once at program load time - zero runtime dispatch overhead
 */
typedef void (*lpm_batch_u32_func_t)(const lpm_trie_t *, const uint32_t *, uint32_t *, size_t);

static lpm_batch_u32_func_t lpm_batch_u32_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_batch_u32_avx512;
    case SIMD_AVX2:
        return lpm_lookup_batch_u32_avx2;
    case SIMD_AVX:
        return lpm_lookup_batch_u32_avx;
    case SIMD_SSE4_2:
        return lpm_lookup_batch_u32_sse42;
    case SIMD_SSE2:
    case SIMD_SCALAR:
    default:
        return lpm_lookup_batch_u32_scalar;
    }
}

/*
 * Main dispatcher for uint32_t IP batch lookup
 * Uses ifunc for zero-overhead dispatch resolved at program load time
 */
void lpm_lookup_batch_ipv4_dir24_u32(const lpm_trie_t *trie, const uint32_t *ips,
                                      uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_batch_u32_resolver")));

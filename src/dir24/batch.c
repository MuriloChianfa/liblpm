/*
 * IPv4 DIR-24-8 Algorithm - Batch Lookup
 * SIMD-optimized batch lookup with ifunc dispatch
 *
 * SIMD optimizations using GNU ifunc for zero-overhead dispatch:
 * - AVX2: Process 8 lookups in parallel using gather instructions
 * - AVX512: Process 16 lookups in parallel using gather instructions
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../../include/lpm.h"
#include "../../include/internal.h"

#define LPM_TBL8_GROUP_ENTRIES 256

/* ============================================================================
 * Inline Lookup Helper
 * ============================================================================ */

__attribute__((hot, always_inline, flatten))
static inline uint32_t dir24_lookup_inline(const struct lpm_dir24_entry * restrict dir24,
                                           const struct lpm_tbl8_entry * restrict tbl8,
                                           const uint8_t * restrict addr)
{
    uint32_t dir24_idx = ((uint32_t)addr[0] << 16) | ((uint32_t)addr[1] << 8) | addr[2];
    uint32_t data = dir24[dir24_idx].data;
    
    if (__builtin_expect(!(data & LPM_DIR24_EXT_FLAG), 1)) {
        return (data & LPM_DIR24_VALID_FLAG) ? (data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
    }
    
    uint32_t tbl8_group = data & LPM_DIR24_NH_MASK;
    uint32_t tbl8_idx = (tbl8_group << 8) | addr[3];
    uint32_t tbl8_data = tbl8[tbl8_idx].data;
    
    return (tbl8_data & LPM_DIR24_VALID_FLAG) ? (tbl8_data & LPM_DIR24_NH_MASK) : LPM_INVALID_NEXT_HOP;
}

/* ============================================================================
 * Scalar Batch Implementation
 * ============================================================================ */

__attribute__((hot))
void lpm_lookup_batch_ipv4_dir24_scalar(const lpm_trie_t *trie, const uint32_t *ips,
                                         uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    for (size_t i = 0; i < count; i++) {
        /* IPs are in host byte order (big-endian for network)
         * IP = 0xC0A80001 means 192.168.0.1
         * DIR24 index = top 24 bits = IP >> 8 */
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

/* ============================================================================
 * SSE4.2 Batch - Optimized scalar with prefetch
 * ============================================================================ */

__attribute__((hot, target("sse4.2")))
void lpm_lookup_batch_ipv4_dir24_sse42(const lpm_trie_t *trie, const uint32_t *ips,
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

/* ============================================================================
 * AVX Batch - Same as SSE4.2 (no gather in AVX)
 * ============================================================================ */

__attribute__((hot, target("avx")))
void lpm_lookup_batch_ipv4_dir24_avx(const lpm_trie_t *trie, const uint32_t *ips,
                                      uint32_t *next_hops, size_t count)
{
    lpm_lookup_batch_ipv4_dir24_sse42(trie, ips, next_hops, count);
}

/* ============================================================================
 * AVX2 Batch - Process 8 lookups in parallel with gather
 * ============================================================================ */

__attribute__((hot, target("avx2")))
void lpm_lookup_batch_ipv4_dir24_avx2(const lpm_trie_t *trie, const uint32_t *ips,
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
        /* Load 8 IPs directly */
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

/* ============================================================================
 * AVX512 Batch - Process 16 lookups in parallel with gather
 * ============================================================================ */

__attribute__((hot, target("avx512f")))
void lpm_lookup_batch_ipv4_dir24_avx512(const lpm_trie_t *trie, const uint32_t *ips,
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
        /* Load 16 IPs directly into AVX512 register */
        __m512i ips_vec = _mm512_loadu_si512(&ips[i]);
        
        /* Prefetch next batch of IPs */
        if (i + 32 <= count) {
            _mm_prefetch(&ips[i + 16], _MM_HINT_T0);
        }
        
        /* Compute DIR24 indices: top 24 bits = IP >> 8 */
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
        
        /* Handle tbl8 lookups with SIMD */
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
            
            /* Update results */
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
        lpm_lookup_batch_ipv4_dir24_avx2(trie, &ips[i], &next_hops[i], count - i);
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

/* ============================================================================
 * ifunc Resolver
 * ============================================================================ */

typedef void (*lpm_dir24_batch_func_t)(const lpm_trie_t *, const uint32_t *, uint32_t *, size_t);

static lpm_dir24_batch_func_t lpm_dir24_batch_resolver(void)
{
    simd_level_t level = detect_simd_level();
    
    switch (level) {
    case SIMD_AVX512F:
        return lpm_lookup_batch_ipv4_dir24_avx512;
    case SIMD_AVX2:
        return lpm_lookup_batch_ipv4_dir24_avx2;
    case SIMD_AVX:
        return lpm_lookup_batch_ipv4_dir24_avx;
    case SIMD_SSE4_2:
        return lpm_lookup_batch_ipv4_dir24_sse42;
    case SIMD_SSE2:
    case SIMD_SCALAR:
    default:
        return lpm_lookup_batch_ipv4_dir24_scalar;
    }
}

/* Main batch lookup for uint32_t IPs */
void lpm_lookup_batch_ipv4_dir24(const lpm_trie_t *trie, const uint32_t *addrs,
                                  uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_dir24_batch_resolver")));

/* ============================================================================
 * Pointer-based Batch Lookup (for compatibility)
 * ============================================================================ */

__attribute__((hot))
static void lpm_lookup_batch_dir24_ptrs_scalar(const lpm_trie_t *trie, const uint8_t **addrs,
                                                uint32_t *next_hops, size_t count)
{
    const struct lpm_dir24_entry * restrict dir24 = trie->dir24_table;
    const struct lpm_tbl8_entry * restrict tbl8 = trie->tbl8_groups;
    const uint32_t default_nh = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t r = dir24_lookup_inline(dir24, tbl8, addrs[i]);
        next_hops[i] = (r == LPM_INVALID_NEXT_HOP) ? default_nh : r;
    }
}

typedef void (*lpm_dir24_ptrs_func_t)(const lpm_trie_t *, const uint8_t **, uint32_t *, size_t);

static lpm_dir24_ptrs_func_t lpm_dir24_ptrs_resolver(void)
{
    /* For pointer-based, we use scalar for now */
    return lpm_lookup_batch_dir24_ptrs_scalar;
}

void lpm_lookup_batch_ipv4_dir24_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                       uint32_t *next_hops, size_t count)
    __attribute__((ifunc("lpm_dir24_ptrs_resolver")));

/* Batch lookup for byte array input */
void lpm_lookup_batch_ipv4_dir24_bytes(const lpm_trie_t *trie, const uint8_t (*addrs)[4],
                                        uint32_t *next_hops, size_t count)
{
    lpm_lookup_batch_ipv4_dir24_ptrs(trie, (const uint8_t **)&addrs[0], next_hops, count);
}

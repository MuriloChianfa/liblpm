#include <string.h>
#include <immintrin.h>
#include "../include/lpm.h"

/* Branchless bitmap check */
uint32_t lpm_bitmap_get_branchless(const uint32_t *bitmap, uint8_t index)
{
    uint32_t word = index >> 5;  /* index / 32 */
    uint32_t bit = index & 31;   /* index % 32 */
    return (bitmap[word] >> bit) & 1;
}

/* Branchless next hop selection */
static inline uint32_t lpm_select_next_hop(uint32_t current, uint32_t candidate, uint32_t valid)
{
    /* If valid is 1, return candidate; if valid is 0, return current */
    return (candidate & -valid) | (current & ~(-valid));
}

/* Optimized generic single lookup with aggressive prefetching */
uint32_t lpm_lookup_single_optimized(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    /* Prefetch the address data */
    __builtin_prefetch(addr, 0, 3);
    
    /* Unroll the first few iterations for better performance */
    if (node && depth < trie->max_depth) {
        uint8_t index = addr[0];
        
        /* Prefetch the child node early */
        __builtin_prefetch(node->children[index], 0, 3);
        __builtin_prefetch(&node->valid_bitmap[index >> 5], 0, 3);
        __builtin_prefetch(&node->prefix_info[index], 0, 3);
        
        /* Branchless next hop update */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    /* Continue with remaining lookups */
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;  /* depth / 8 */
        uint8_t index = addr[byte_index];
        
        /* Double prefetch - current access and potential next */
        if (node->children[index]) {
            __builtin_prefetch(node->children[index], 0, 3);
            __builtin_prefetch(&node->children[index]->valid_bitmap[0], 0, 3);
            __builtin_prefetch(&node->children[index]->prefix_info[0], 0, 3);
        }
        
        /* Branchless next hop update */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return next_hop;
}

#ifdef LPM_HAVE_SSE2
/* SSE2 optimized lookup - uses movemask for bitmap checks */
uint32_t lpm_lookup_single_sse2(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch next level */
        if (node->children[index]) {
            __builtin_prefetch(node->children[index], 0, 3);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_T0);
        }
        
        /* Check validity using bit manipulation */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        /* Conditional move using SSE2 */
        if (valid && node->prefix_info[index]) {
            uint32_t new_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
            __m128i current_hop = _mm_set1_epi32(next_hop);
            __m128i candidate_hop = _mm_set1_epi32(new_hop);
            __m128i mask = _mm_set1_epi32(-1);  /* Always update when valid */
            __m128i result = _mm_or_si128(
                _mm_and_si128(candidate_hop, mask),
                _mm_andnot_si128(mask, current_hop)
            );
            next_hop = _mm_cvtsi128_si32(result);
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return next_hop;
}
#endif

#ifdef LPM_HAVE_AVX2
/* AVX2 optimized lookup - uses advanced bit manipulation */
uint32_t lpm_lookup_single_avx2(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    /* Prefetch multiple cache lines ahead */
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
    _mm_prefetch((const char*)(addr + 8), _MM_HINT_T1);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Aggressive prefetching for next two levels */
        if (node->children[index]) {
            _mm_prefetch((const char*)node->children[index], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->prefix_info[0], _MM_HINT_T0);
            
            /* Prefetch potential grandchild */
            if (depth + LPM_STRIDE_BITS < trie->max_depth && byte_index + 1 < 16) {
                uint8_t next_index = addr[byte_index + 1];
                if (node->children[index]->children[next_index]) {
                    _mm_prefetch((const char*)node->children[index]->children[next_index], _MM_HINT_T1);
                }
            }
        }
        
        /* Branchless validity check and update */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        /* AVX2 conditional move */
        if (valid && node->prefix_info[index]) {
            uint32_t new_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
            __m256i current_hop = _mm256_set1_epi32(next_hop);
            __m256i candidate_hop = _mm256_set1_epi32(new_hop);
            __m256i mask = _mm256_set1_epi32(-1);  /* Always update when valid */
            __m256i result = _mm256_blendv_epi8(current_hop, candidate_hop, mask);
            next_hop = _mm256_extract_epi32(result, 0);
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return next_hop;
}
#endif

#ifdef LPM_HAVE_AVX512F
/* AVX512 optimized lookup - uses mask registers for branchless operations */
uint32_t lpm_lookup_single_avx512(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    /* Prefetch with AVX512 prefetch instructions */
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch with non-temporal hint for streaming access */
        if (node->children[index]) {
            _mm_prefetch((const char*)node->children[index], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_NTA);
        }
        
        /* Check validity */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        /* AVX512 mask operation for branchless update */
        if (valid && node->prefix_info[index]) {
            uint32_t new_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
            __mmask8 k = _cvtu32_mask8(1);  /* Single bit mask */
            __m512i current_hop = _mm512_set1_epi32(next_hop);
            __m512i candidate_hop = _mm512_set1_epi32(new_hop);
            __m512i result = _mm512_mask_blend_epi32(k, current_hop, candidate_hop);
            next_hop = _mm512_cvtsi512_si32(result);
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return next_hop;
}
#endif

/* IPv4 specific optimized lookup */
uint32_t lpm_lookup_ipv4_optimized(const struct lpm_trie *trie, uint32_t addr)
{
    /* Convert to big-endian byte array on stack (likely in cache) */
    uint8_t addr_bytes[4] __attribute__((aligned(16)));
    addr_bytes[0] = (addr >> 24) & 0xFF;
    addr_bytes[1] = (addr >> 16) & 0xFF;
    addr_bytes[2] = (addr >> 8) & 0xFF;
    addr_bytes[3] = addr & 0xFF;
    
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    
    /* Unrolled loop for IPv4 (max 4 iterations) */
    
    /* Level 0 */
    if (node) {
        uint8_t index = addr_bytes[0];
        __builtin_prefetch(node->children[index], 0, 3);
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        node = node->children[index];
    }
    
    /* Level 1 */
    if (node) {
        uint8_t index = addr_bytes[1];
        __builtin_prefetch(node->children[index], 0, 3);
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        node = node->children[index];
    }
    
    /* Level 2 */
    if (node) {
        uint8_t index = addr_bytes[2];
        __builtin_prefetch(node->children[index], 0, 3);
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        node = node->children[index];
    }
    
    /* Level 3 */
    if (node) {
        uint8_t index = addr_bytes[3];
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
    }
    
    return next_hop;
}

/* IPv6 specific optimized lookup with SIMD address loading */
uint32_t lpm_lookup_ipv6_optimized(const struct lpm_trie *trie, const uint8_t addr[16])
{
#ifdef LPM_HAVE_SSE2
    /* Load IPv6 address into SSE register for faster access */
    __m128i addr_vec = _mm_loadu_si128((const __m128i*)addr);
    uint8_t addr_bytes[16] __attribute__((aligned(16)));
    _mm_store_si128((__m128i*)addr_bytes, addr_vec);
    
    /* Prefetch entire address */
    _mm_prefetch((const char*)addr_bytes, _MM_HINT_T0);
#else
    const uint8_t *addr_bytes = addr;
#endif
    
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    /* Unroll first few iterations */
    for (int i = 0; i < 4 && node && depth < LPM_IPV6_MAX_DEPTH; i++) {
        uint8_t index = addr_bytes[i];
        
        /* Prefetch next two levels */
        if (node->children[index]) {
            __builtin_prefetch(node->children[index], 0, 3);
            if (i + 1 < 16 && node->children[index]) {
                uint8_t next_index = addr_bytes[i + 1];
                __builtin_prefetch(&node->children[index]->children[next_index], 0, 2);
            }
        }
        
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    /* Continue with remaining bytes */
    while (node && depth < LPM_IPV6_MAX_DEPTH) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr_bytes[byte_index];
        
        if (node->children[index]) {
            __builtin_prefetch(node->children[index], 0, 3);
        }
        
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return next_hop;
}

/* Function to select the best single lookup implementation based on CPU features */
void lpm_select_single_lookup_function(lpm_trie_t *trie)
{
#ifdef LPM_HAVE_AVX512F
    if (trie->cpu_features & LPM_CPU_AVX512F) {
        trie->lookup_single = lpm_lookup_single_avx512;
        return;
    }
#endif

#ifdef LPM_HAVE_AVX2
    if (trie->cpu_features & LPM_CPU_AVX2) {
        trie->lookup_single = lpm_lookup_single_avx2;
        return;
    }
#endif

#ifdef LPM_HAVE_SSE2
    if (trie->cpu_features & LPM_CPU_SSE2) {
        trie->lookup_single = lpm_lookup_single_sse2;
        return;
    }
#endif

    /* Default to optimized generic implementation */
    trie->lookup_single = lpm_lookup_single_optimized;
} 
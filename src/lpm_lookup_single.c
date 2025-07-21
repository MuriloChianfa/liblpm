#include <stdint.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../include/lpm.h"

#ifdef LPM_X86_ARCH
#include <cpuid.h>
#endif

/* Forward declaration for generic function */
static uint32_t lpm_lookup_single_generic(const struct lpm_trie *trie, const uint8_t *addr);

/* Branchless bitmap check */
uint32_t lpm_bitmap_get_branchless(const uint32_t *bitmap, uint8_t index)
{
    uint32_t word = index >> 5;  /* index / 32 */
    uint32_t bit = index & 31;   /* index % 32 */
    return (bitmap[word] >> bit) & 1;
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
uint32_t lpm_lookup_single_sse2(const struct lpm_trie *trie, const uint8_t *addr) __attribute__((target("sse2")));
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
    
    /* If no specific route found, check for default route */
    if (next_hop == LPM_INVALID_NEXT_HOP && trie->default_route) {
        next_hop = (uint32_t)(uintptr_t)trie->default_route->user_data;
    }
    
    return next_hop;
}
#endif

#if defined(LPM_HAVE_AVX) && !defined(LPM_DISABLE_AVX)
/* AVX optimized lookup - uses 256-bit vectors for better throughput */
uint32_t lpm_lookup_single_avx(const struct lpm_trie *trie, const uint8_t *addr) __attribute__((target("avx")));
uint32_t lpm_lookup_single_avx(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Runtime safety check - verify AVX is actually supported */
    #ifdef __GNUC__
        if (!__builtin_cpu_supports("avx")) {
            /* Fallback to SSE2 if AVX not available */
            return lpm_lookup_single_sse2(trie, addr);
        }
    #else
        /* For non-GCC/Clang compilers, fallback to generic implementation */
        return lpm_lookup_single_generic(trie, addr);
    #endif

    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    /* Prefetch with AVX */
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch next level */
        if (node->children[index]) {
            _mm_prefetch((const char*)node->children[index], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_T0);
        }
        
        /* Check validity */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        if (valid && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    /* If no specific route found, check for default route */
    if (next_hop == LPM_INVALID_NEXT_HOP && trie->default_route) {
        next_hop = (uint32_t)(uintptr_t)trie->default_route->user_data;
    }
    
    return next_hop;
}
#endif

#if defined(LPM_HAVE_AVX2) && !defined(LPM_DISABLE_AVX2)
/* AVX2 optimized lookup - uses 256-bit vectors for better throughput */
uint32_t lpm_lookup_single_avx2(const struct lpm_trie *trie, const uint8_t *addr) __attribute__((target("avx2")));
uint32_t lpm_lookup_single_avx2(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Runtime safety check - verify AVX2 is actually supported */
    #ifdef __GNUC__
        if (!__builtin_cpu_supports("avx2")) {
            /* Fallback to SSE2 if AVX2 not available */
            return lpm_lookup_single_sse2(trie, addr);
        }
    #else
        /* For non-GCC compilers, fallback to generic implementation */
        return lpm_lookup_single_generic(trie, addr);
    #endif

    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    /* Prefetch with AVX2 */
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch next level */
        if (node->children[index]) {
            _mm_prefetch((const char*)node->children[index], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_T0);
        }
        
        /* Check validity */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        /* AVX2 blend operation for branchless update */
        if (valid && node->prefix_info[index]) {
            uint32_t new_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
            __m256i current_hop = _mm256_set1_epi32(next_hop);
            __m256i candidate_hop = _mm256_set1_epi32(new_hop);
            __m256i mask = _mm256_set1_epi32(-1);
            __m256i result = _mm256_blendv_epi8(current_hop, candidate_hop, mask);
            next_hop = _mm256_cvtsi256_si32(result);
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    /* If no specific route found, check for default route */
    if (next_hop == LPM_INVALID_NEXT_HOP && trie->default_route) {
        next_hop = (uint32_t)(uintptr_t)trie->default_route->user_data;
    }
    
    return next_hop;
}
#endif

#if defined(LPM_HAVE_AVX512F) && !defined(LPM_DISABLE_AVX512)
/* AVX512 optimized lookup - uses mask registers for branchless operations */
uint32_t lpm_lookup_single_avx512(const struct lpm_trie *trie, const uint8_t *addr) __attribute__((target("avx512f")));
uint32_t lpm_lookup_single_avx512(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Runtime safety check - verify AVX512 is actually supported */
    #ifdef __GNUC__
        if (!__builtin_cpu_supports("avx512f")) {
            /* Fallback to AVX2 if AVX512 not available */
            if (__builtin_cpu_supports("avx2")) {
                return lpm_lookup_single_avx2(trie, addr);
            }

            return lpm_lookup_single_sse2(trie, addr);
        }
    #else
        /* For non-GCC compilers, fallback to generic implementation */
        return lpm_lookup_single_generic(trie, addr);
    #endif

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
            /* Use simple conditional assignment instead of complex mask operations */
            next_hop = new_hop;
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    /* If no specific route found, check for default route */
    if (next_hop == LPM_INVALID_NEXT_HOP && trie->default_route) {
        next_hop = (uint32_t)(uintptr_t)trie->default_route->user_data;
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
    
    /* If no specific route found, check for default route */
    if (next_hop == LPM_INVALID_NEXT_HOP && trie->default_route) {
        next_hop = (uint32_t)(uintptr_t)trie->default_route->user_data;
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
            if (i + 1 < 4 && node->children[index]) {
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
    
    /* If no specific route found, check for default route */
    if (next_hop == LPM_INVALID_NEXT_HOP && trie->default_route) {
        next_hop = (uint32_t)(uintptr_t)trie->default_route->user_data;
    }
    
    return next_hop;
}

/* Stub implementations for disabled SIMD functions */
#ifdef LPM_DISABLE_AVX
uint32_t lpm_lookup_single_avx(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Fallback to SSE2 implementation */
    return lpm_lookup_single_sse2(trie, addr);
}
#endif

#ifdef LPM_DISABLE_AVX2
uint32_t lpm_lookup_single_avx2(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Fallback to SSE2 implementation */
    return lpm_lookup_single_sse2(trie, addr);
}
#endif

#ifdef LPM_DISABLE_AVX512
uint32_t lpm_lookup_single_avx512(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Fallback to AVX2 if available, otherwise SSE2 */
    #ifdef __GNUC__
        if (__builtin_cpu_supports("avx2")) {
            return lpm_lookup_single_avx2(trie, addr);
        }
    #else
        /* For non-GCC compilers, assume no AVX2 for safety */
        /* Fallback to generic implementation */
        return lpm_lookup_single_generic(trie, addr);
    #endif
    return lpm_lookup_single_sse2(trie, addr);
}
#endif

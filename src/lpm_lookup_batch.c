#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../include/lpm.h"

/* Forward declaration for generic function */
static void lpm_lookup_batch_generic(const struct lpm_trie *trie, const uint8_t **addrs, 
                                    uint32_t *next_hops, size_t count);

#ifdef LPM_X86_ARCH
#include <cpuid.h>
#endif

/* External function pointers for SIMD implementations */
extern uint32_t (*lpm_lookup_single_func)(const struct lpm_trie *trie, const uint8_t *addr);
extern void (*lpm_lookup_batch_func)(const struct lpm_trie *trie, const uint8_t **addrs, 
                                    uint32_t *next_hops, size_t count);

/* Helper macros for SIMD gather operations */
#if defined(LPM_HAVE_AVX2) && !defined(LPM_DISABLE_AVX2)
#define GATHER_INDICES_AVX2(base, indices, scale) \
    _mm256_i32gather_epi64((const long long*)(base), indices, scale)
#endif

#if defined(LPM_HAVE_AVX512F) && !defined(LPM_DISABLE_AVX512)
#define GATHER_INDICES_AVX512(base, indices, mask, scale) \
    _mm512_mask_i64gather_epi64(_mm512_setzero_si512(), mask, indices, (const long long*)(base), scale)
#endif

/* Generic batch lookup - baseline implementation */
void lpm_lookup_batch_generic(const struct lpm_trie *trie, const uint8_t **addrs, 
                             uint32_t *next_hops, size_t count)
{
    /* Process addresses one by one */
    for (size_t i = 0; i < count; i++) {
        next_hops[i] = lpm_lookup_single_func(trie, addrs[i]);
    }
}

#ifdef LPM_HAVE_SSE2
/* SSE2 batch lookup - process 4 addresses at a time */
void lpm_lookup_batch_sse2(const struct lpm_trie *trie, const uint8_t **addrs, 
                          uint32_t *next_hops, size_t count) __attribute__((target("sse2")));
void lpm_lookup_batch_sse2(const struct lpm_trie *trie, const uint8_t **addrs, 
                          uint32_t *next_hops, size_t count)
{
    size_t i = 0;
    
    /* Process 4 addresses at a time */
    for (; i + 3 < count; i += 4) {
        const lpm_node_t *nodes[4];
        for (int j = 0; j < 4; j++) {
            nodes[j] = trie->root;
        }

        __m128i next_hop_vec = _mm_set1_epi32(LPM_INVALID_NEXT_HOP);
        uint8_t depth = 0;
        
        /* Prefetch all 4 addresses */
        for (int j = 0; j < 4; j++) {
            _mm_prefetch((const char*)addrs[i+j], _MM_HINT_T0);
        }
        
        while (depth < trie->max_depth) {
            uint8_t byte_index = depth >> 3;
            
            /* Load indices from all 4 addresses */
            __m128i indices = _mm_setr_epi32(
                addrs[i][byte_index],
                addrs[i+1][byte_index],
                addrs[i+2][byte_index],
                addrs[i+3][byte_index]
            );
            
            /* Extract indices */
            uint32_t idx[4];
            _mm_storeu_si128((__m128i*)idx, indices);
            
            __m128i valid_mask = _mm_setzero_si128();
            __m128i new_hops = _mm_setzero_si128();
            
            /* Process each node */
            for (int j = 0; j < 4; j++) {
                if (nodes[j]) {
                    /* Prefetch child node */
                    if (nodes[j]->children[idx[j]]) {
                        _mm_prefetch((const char*)nodes[j]->children[idx[j]], _MM_HINT_T0);
                    }
                    
                    /* Check validity and get next hop */
                    uint32_t word = idx[j] >> 5;
                    uint32_t bit = idx[j] & 31;
                    uint32_t valid = (nodes[j]->valid_bitmap[word] >> bit) & 1;
                    
                    if (valid && nodes[j]->prefix_info[idx[j]]) {
                        uint32_t next_hop = (uint32_t)(uintptr_t)nodes[j]->prefix_info[idx[j]]->user_data;

                        /* Use conditional operations instead of insert */
                        __m128i hop_val = _mm_set1_epi32(next_hop);

                        /* Create position-specific masks */
                        __m128i pos_mask;
                        switch (j) {
                            case 0: pos_mask = _mm_setr_epi32(-1, 0, 0, 0); break;
                            case 1: pos_mask = _mm_setr_epi32(0, -1, 0, 0); break;
                            case 2: pos_mask = _mm_setr_epi32(0, 0, -1, 0); break;
                            case 3: pos_mask = _mm_setr_epi32(0, 0, 0, -1); break;
                            default: pos_mask = _mm_setzero_si128(); break;
                        }

                        valid_mask = _mm_or_si128(valid_mask, pos_mask);
                        new_hops = _mm_or_si128(new_hops, _mm_and_si128(hop_val, pos_mask));
                    }
                    
                    nodes[j] = nodes[j]->children[idx[j]];
                } else {
                    nodes[j] = NULL;
                }
            }
            
            /* Update next hops using blend */
            next_hop_vec = _mm_or_si128(
                _mm_and_si128(new_hops, valid_mask),
                _mm_andnot_si128(valid_mask, next_hop_vec)
            );
            
            depth += LPM_STRIDE_BITS;
            
            /* Check if all nodes are NULL */
            if (!nodes[0] && !nodes[1] && !nodes[2] && !nodes[3]) {
                break;
            }
        }
        
        /* Store results */
        _mm_storeu_si128((__m128i*)&next_hops[i], next_hop_vec);
        
        /* Apply default route to any addresses that didn't find a match */
        for (int j = 0; j < 4; j++) {
            if (next_hops[i + j] == LPM_INVALID_NEXT_HOP && trie->default_route) {
                next_hops[i + j] = (uint32_t)(uintptr_t)trie->default_route->user_data;
            }
        }
    }
    
    /* Process remaining addresses */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_single_func(trie, addrs[i]);
    }
}
#endif

#if defined(LPM_HAVE_AVX) && !defined(LPM_DISABLE_AVX)
/* AVX batch lookup - process 8 addresses at a time */
void lpm_lookup_batch_avx(const struct lpm_trie *trie, const uint8_t **addrs, 
                         uint32_t *next_hops, size_t count) __attribute__((target("avx")));
void lpm_lookup_batch_avx(const struct lpm_trie *trie, const uint8_t **addrs, 
                         uint32_t *next_hops, size_t count)
{
    /* Runtime safety check - verify AVX is actually supported */
    #if defined(__GNUC__) && !defined(__clang__)
        if (!__builtin_cpu_supports("avx")) {
            /* Fallback to generic implementation */
            lpm_lookup_batch_generic(trie, addrs, next_hops, count);
            return;
        }
    #else
        /* For non-GCC compilers (including Apple Clang), fallback to generic implementation */
        lpm_lookup_batch_generic(trie, addrs, next_hops, count);
        return;
    #endif

    size_t i = 0;

    /* Process 8 addresses at a time */
    for (; i + 7 < count; i += 8) {
        const lpm_node_t *nodes[8];
        for (int j = 0; j < 8; j++) {
            nodes[j] = trie->root;
        }

        __m256i next_hop_vec = _mm256_set1_epi32(LPM_INVALID_NEXT_HOP);
        uint8_t depth = 0;
        
        /* Prefetch all 8 addresses */
        for (int j = 0; j < 8; j++) {
            _mm_prefetch((const char*)addrs[i+j], _MM_HINT_T0);
        }
        
        while (depth < trie->max_depth) {
            uint8_t byte_index = depth >> 3;
            
            /* Load indices from all 8 addresses */
            __m256i indices = _mm256_setr_epi32(
                addrs[i][byte_index],
                addrs[i+1][byte_index],
                addrs[i+2][byte_index],
                addrs[i+3][byte_index],
                addrs[i+4][byte_index],
                addrs[i+5][byte_index],
                addrs[i+6][byte_index],
                addrs[i+7][byte_index]
            );
            
            /* Extract indices */
            uint32_t idx[8];
            _mm256_storeu_si256((__m256i*)idx, indices);
            
            __m256i valid_mask = _mm256_setzero_si256();
            __m256i new_hops = _mm256_setzero_si256();
            int active_nodes = 0;
            
            /* Process each node */
            for (int j = 0; j < 8; j++) {
                if (nodes[j]) {
                    /* Prefetch child node */
                    if (nodes[j]->children[idx[j]]) {
                        _mm_prefetch((const char*)nodes[j]->children[idx[j]], _MM_HINT_T0);
                    }
                    
                    /* Check validity */
                    uint32_t word = idx[j] >> 5;
                    uint32_t bit = idx[j] & 31;
                    uint32_t valid = (nodes[j]->valid_bitmap[word] >> bit) & 1;
                    
                    if (valid && nodes[j]->prefix_info[idx[j]]) {
                        uint32_t next_hop = (uint32_t)(uintptr_t)nodes[j]->prefix_info[idx[j]]->user_data;

                        /* Use conditional operations instead of insert */
                        __m256i hop_val = _mm256_set1_epi32(next_hop);

                        /* Create position-specific masks */
                        __m256i pos_mask;
                        switch (j) {
                            case 0: pos_mask = _mm256_setr_epi32(-1, 0, 0, 0, 0, 0, 0, 0); break;
                            case 1: pos_mask = _mm256_setr_epi32(0, -1, 0, 0, 0, 0, 0, 0); break;
                            case 2: pos_mask = _mm256_setr_epi32(0, 0, -1, 0, 0, 0, 0, 0); break;
                            case 3: pos_mask = _mm256_setr_epi32(0, 0, 0, -1, 0, 0, 0, 0); break;
                            case 4: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, -1, 0, 0, 0); break;
                            case 5: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, -1, 0, 0); break;
                            case 6: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, -1, 0); break;
                            case 7: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, 0, -1); break;
                            default: pos_mask = _mm256_setzero_si256(); break;
                        }

                        /* Use simple scalar operations for AVX compatibility */
                        uint32_t valid_vals[8], new_vals[8], hop_vals[8], mask_vals[8];
                        _mm256_storeu_si256((__m256i*)valid_vals, valid_mask);
                        _mm256_storeu_si256((__m256i*)new_vals, new_hops);  
                        _mm256_storeu_si256((__m256i*)hop_vals, hop_val);
                        _mm256_storeu_si256((__m256i*)mask_vals, pos_mask);
                        
                        for (int k = 0; k < 8; k++) {
                            valid_vals[k] |= mask_vals[k];
                            new_vals[k] |= (hop_vals[k] & mask_vals[k]);
                        }
                        
                        valid_mask = _mm256_loadu_si256((__m256i*)valid_vals);
                        new_hops = _mm256_loadu_si256((__m256i*)new_vals);
                    }
                    
                    nodes[j] = nodes[j]->children[idx[j]];
                    if (nodes[j]) active_nodes++;
                }
            }
            
            uint32_t current_hops[8];
            uint32_t new_hops_array[8];
            uint32_t valid_array[8];
            
            _mm256_storeu_si256((__m256i*)current_hops, next_hop_vec);
            _mm256_storeu_si256((__m256i*)new_hops_array, new_hops);
            _mm256_storeu_si256((__m256i*)valid_array, valid_mask);
            
            for (int k = 0; k < 8; k++) {
                if (valid_array[k]) {
                    current_hops[k] = new_hops_array[k];
                }
            }
            
            next_hop_vec = _mm256_loadu_si256((__m256i*)current_hops);
            
            /* If no active nodes, we can stop */
            if (active_nodes == 0) {
                break;
            }
            
            depth += LPM_STRIDE_BITS;
        }
        
        /* Store results */
        uint32_t results[8];
        _mm256_storeu_si256((__m256i*)results, next_hop_vec);
        
        for (int j = 0; j < 8; j++) {
            next_hops[i + j] = results[j];
        }
    }
    
    /* Handle remaining addresses */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_single_func(trie, addrs[i]);
    }
}
#endif

#if defined(LPM_HAVE_AVX2) && !defined(LPM_DISABLE_AVX2)
/* AVX2 batch lookup - process 8 addresses at a time */
void lpm_lookup_batch_avx2(const struct lpm_trie *trie, const uint8_t **addrs, 
                          uint32_t *next_hops, size_t count) __attribute__((target("avx2")));
void lpm_lookup_batch_avx2(const struct lpm_trie *trie, const uint8_t **addrs, 
                          uint32_t *next_hops, size_t count)
{
    /* Runtime safety check - verify AVX2 is actually supported */
    #if defined(__GNUC__) && !defined(__clang__)
        if (!__builtin_cpu_supports("avx2")) {
            /* Fallback to generic implementation */
            lpm_lookup_batch_generic(trie, addrs, next_hops, count);
            return;
        }
    #else
        /* For non-GCC compilers (including Apple Clang), fallback to generic implementation */
        lpm_lookup_batch_generic(trie, addrs, next_hops, count);
        return;
    #endif

    size_t i = 0;

    /* Process 8 addresses at a time */
    for (; i + 7 < count; i += 8) {
        const lpm_node_t *nodes[8];
        for (int j = 0; j < 8; j++) {
            nodes[j] = trie->root;
        }

        __m256i next_hop_vec = _mm256_set1_epi32(LPM_INVALID_NEXT_HOP);
        uint8_t depth = 0;
        
        /* Prefetch all 8 addresses */
        for (int j = 0; j < 8; j++) {
            _mm_prefetch((const char*)addrs[i+j], _MM_HINT_T0);
        }
        
        while (depth < trie->max_depth) {
            uint8_t byte_index = depth >> 3;
            
            /* Load indices from all 8 addresses */
            __m256i indices = _mm256_setr_epi32(
                addrs[i][byte_index],
                addrs[i+1][byte_index],
                addrs[i+2][byte_index],
                addrs[i+3][byte_index],
                addrs[i+4][byte_index],
                addrs[i+5][byte_index],
                addrs[i+6][byte_index],
                addrs[i+7][byte_index]
            );
            
            /* Extract indices */
            uint32_t idx[8];
            _mm256_storeu_si256((__m256i*)idx, indices);
            
            __m256i valid_mask = _mm256_setzero_si256();
            __m256i new_hops = _mm256_setzero_si256();
            int active_nodes = 0;
            
            /* Process each node */
            for (int j = 0; j < 8; j++) {
                if (nodes[j]) {
                    /* Prefetch child node */
                    if (nodes[j]->children[idx[j]]) {
                        _mm_prefetch((const char*)nodes[j]->children[idx[j]], _MM_HINT_T0);
                    }
                    
                    /* Check validity */
                    uint32_t word = idx[j] >> 5;
                    uint32_t bit = idx[j] & 31;
                    uint32_t valid = (nodes[j]->valid_bitmap[word] >> bit) & 1;
                    
                    if (valid && nodes[j]->prefix_info[idx[j]]) {
                        uint32_t next_hop = (uint32_t)(uintptr_t)nodes[j]->prefix_info[idx[j]]->user_data;

                        /* Use conditional operations instead of insert */
                        __m256i hop_val = _mm256_set1_epi32(next_hop);

                        /* Create position-specific masks */
                        __m256i pos_mask;
                        switch (j) {
                            case 0: pos_mask = _mm256_setr_epi32(-1, 0, 0, 0, 0, 0, 0, 0); break;
                            case 1: pos_mask = _mm256_setr_epi32(0, -1, 0, 0, 0, 0, 0, 0); break;
                            case 2: pos_mask = _mm256_setr_epi32(0, 0, -1, 0, 0, 0, 0, 0); break;
                            case 3: pos_mask = _mm256_setr_epi32(0, 0, 0, -1, 0, 0, 0, 0); break;
                            case 4: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, -1, 0, 0, 0); break;
                            case 5: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, -1, 0, 0); break;
                            case 6: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, -1, 0); break;
                            case 7: pos_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, 0, -1); break;
                            default: pos_mask = _mm256_setzero_si256(); break;
                        }

                        valid_mask = _mm256_or_si256(valid_mask, pos_mask);
                        new_hops = _mm256_or_si256(new_hops, _mm256_and_si256(hop_val, pos_mask));
                    }
                    
                    nodes[j] = nodes[j]->children[idx[j]];
                    if (nodes[j]) active_nodes++;
                }
            }
            
            /* Update next hops using AVX2 blend */
            next_hop_vec = _mm256_blendv_epi8(next_hop_vec, new_hops, valid_mask);
            
            depth += LPM_STRIDE_BITS;
            
            /* Early exit if no active nodes */
            if (active_nodes == 0) {
                break;
            }
        }
        
        /* Store results */
        _mm256_storeu_si256((__m256i*)&next_hops[i], next_hop_vec);
        
        /* Apply default route to any addresses that didn't find a match */
        for (int j = 0; j < 8; j++) {
            if (next_hops[i + j] == LPM_INVALID_NEXT_HOP && trie->default_route) {
                next_hops[i + j] = (uint32_t)(uintptr_t)trie->default_route->user_data;
            }
        }
    }
    
    /* Process remaining addresses */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_single_func(trie, addrs[i]);
    }
}
#endif

#if defined(LPM_HAVE_AVX512F) && !defined(LPM_DISABLE_AVX512)
/* AVX512 batch lookup - process 16 addresses at a time */
void lpm_lookup_batch_avx512(const struct lpm_trie *trie, const uint8_t **addrs, 
                            uint32_t *next_hops, size_t count) __attribute__((target("avx512f")));
void lpm_lookup_batch_avx512(const struct lpm_trie *trie, const uint8_t **addrs, 
                            uint32_t *next_hops, size_t count)
{
    /* Runtime safety check - verify AVX512 is actually supported */
    #if defined(__GNUC__) && !defined(__clang__)
        if (!__builtin_cpu_supports("avx512f")) {
            /* Fallback to generic implementation */
            lpm_lookup_batch_generic(trie, addrs, next_hops, count);
            return;
        }
    #else
        /* For non-GCC compilers (including Apple Clang), fallback to generic implementation */
        lpm_lookup_batch_generic(trie, addrs, next_hops, count);
        return;
    #endif

    size_t i = 0;
    
    /* Process 16 addresses at a time */
    for (; i + 15 < count; i += 16) {
        const lpm_node_t *nodes[16];
        for (int j = 0; j < 16; j++) {
            nodes[j] = trie->root;
        }
        
        __m512i next_hop_vec = _mm512_set1_epi32(LPM_INVALID_NEXT_HOP);
        uint8_t depth = 0;
        
        /* Prefetch all 16 addresses with streaming hint */
        for (int j = 0; j < 16; j++) {
            _mm_prefetch((const char*)addrs[i+j], _MM_HINT_T0);
        }
        
        while (depth < trie->max_depth) {
            uint8_t byte_index = depth >> 3;
            
            /* Load indices from all 16 addresses */
            uint32_t idx[16];
            for (int j = 0; j < 16; j++) {
                idx[j] = addrs[i+j][byte_index];
            }
            
            __mmask16 valid_mask = 0;
            __m512i new_hops = _mm512_setzero_si512();
            __mmask16 active_mask = 0;
            
            /* Process each node */
            for (int j = 0; j < 16; j++) {
                if (nodes[j]) {
                    /* Prefetch child node */
                    if (nodes[j]->children[idx[j]]) {
                        _mm_prefetch((const char*)nodes[j]->children[idx[j]], _MM_HINT_T0);
                    }
                    
                    /* Check validity */
                    uint32_t word = idx[j] >> 5;
                    uint32_t bit = idx[j] & 31;
                    uint32_t valid = (nodes[j]->valid_bitmap[word] >> bit) & 1;
                    
                    if (valid && nodes[j]->prefix_info[idx[j]]) {
                        valid_mask |= (1 << j);
                        uint32_t next_hop = (uint32_t)(uintptr_t)nodes[j]->prefix_info[idx[j]]->user_data;
                        /* Use simple conditional assignment instead of complex mask operations */
                        /* Store the next hop in a temporary array for later processing */
                        uint32_t temp_hops[16];
                        _mm512_storeu_si512((__m512i*)temp_hops, new_hops);
                        temp_hops[j] = next_hop;
                        new_hops = _mm512_loadu_si512((__m512i*)temp_hops);
                    }
                    
                    nodes[j] = nodes[j]->children[idx[j]];
                    if (nodes[j]) active_mask |= (1 << j);
                }
            }
            
            /* Update next hops using AVX512 mask operations */
            next_hop_vec = _mm512_mask_blend_epi32(valid_mask, next_hop_vec, new_hops);
            
            depth += LPM_STRIDE_BITS;
            
            /* Early exit if no active nodes */
            if (active_mask == 0) {
                break;
            }
        }
        
        /* Store results */
        _mm512_storeu_si512((__m512i*)&next_hops[i], next_hop_vec);
        
        /* Apply default route to any addresses that didn't find a match */
        for (int j = 0; j < 16; j++) {
            if (next_hops[i + j] == LPM_INVALID_NEXT_HOP && trie->default_route) {
                next_hops[i + j] = (uint32_t)(uintptr_t)trie->default_route->user_data;
            }
        }
    }
    
    /* Process remaining addresses */
    for (; i < count; i++) {
        next_hops[i] = lpm_lookup_single_func(trie, addrs[i]);
    }
}
#endif

/* Batch lookup for IPv4 addresses */
void lpm_lookup_batch_ipv4(const lpm_trie_t *trie, const uint32_t *addrs, 
                          uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) {
        return;
    }
    
    /* Convert IPv4 addresses to byte arrays and perform batch lookup */
    const uint8_t **addr_ptrs = (const uint8_t**)malloc(count * sizeof(uint8_t*));

    /* Align size to 64 bytes */
    size_t aligned_size = ((count * 4) + 63) & ~63;
    uint8_t *addr_bytes = (uint8_t*)aligned_alloc(64, aligned_size);
    
    if (!addr_ptrs || !addr_bytes) {
        free(addr_ptrs);
        free(addr_bytes);
        return;
    }
    
    /* Convert addresses */
    for (size_t i = 0; i < count; i++) {
        uint8_t *bytes = &addr_bytes[i * 4];
        uint32_t addr = addrs[i];
        /* Convert from network byte order (big-endian) to byte array */
        bytes[0] = (addr >> 24) & 0xFF;
        bytes[1] = (addr >> 16) & 0xFF;
        bytes[2] = (addr >> 8) & 0xFF;
        bytes[3] = addr & 0xFF;
        addr_ptrs[i] = bytes;
    }
    
    /* Perform batch lookup */
    lpm_lookup_batch_func(trie, addr_ptrs, next_hops, count);
    
    free(addr_ptrs);
    free(addr_bytes);
}

/* Batch lookup for IPv6 addresses */
void lpm_lookup_batch_ipv6(const lpm_trie_t *trie, const uint8_t (*addrs)[16], 
                          uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) {
        return;
    }
    
    /* Create array of pointers */
    const uint8_t **addr_ptrs = (const uint8_t**)malloc(count * sizeof(uint8_t*));
    if (!addr_ptrs) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        addr_ptrs[i] = addrs[i];
    }
    
    /* Perform batch lookup */
    lpm_lookup_batch_func(trie, addr_ptrs, next_hops, count);
    
    free(addr_ptrs);
}

/* Stub implementations for disabled SIMD functions */
#ifdef LPM_DISABLE_AVX
void lpm_lookup_batch_avx(const struct lpm_trie *trie, const uint8_t **addrs, 
                         uint32_t *next_hops, size_t count)
{
    /* Fallback to generic implementation */
    lpm_lookup_batch_generic(trie, addrs, next_hops, count);
}
#endif

#ifdef LPM_DISABLE_AVX2
void lpm_lookup_batch_avx2(const struct lpm_trie *trie, const uint8_t **addrs, 
                          uint32_t *next_hops, size_t count)
{
    /* Fallback to generic implementation */
    lpm_lookup_batch_generic(trie, addrs, next_hops, count);
}
#endif

#ifdef LPM_DISABLE_AVX512
void lpm_lookup_batch_avx512(const struct lpm_trie *trie, const uint8_t **addrs, 
                            uint32_t *next_hops, size_t count)
{
    /* Fallback to generic implementation */
    lpm_lookup_batch_generic(trie, addrs, next_hops, count);
}
#endif



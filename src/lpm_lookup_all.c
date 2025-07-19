#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../include/lpm.h"

/* External function to get branchless bitmap check */
extern uint32_t lpm_bitmap_get_branchless(const uint32_t *bitmap, uint8_t index);

/* Optimized lookup all with prefetching */
lpm_result_t* lpm_lookup_all_optimized(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Create result structure */
    lpm_result_t *result = lpm_result_create(LPM_MAX_RESULTS);
    if (!result) {
        return NULL;
    }
    
    const lpm_node_t *node = trie->root;
    uint8_t depth = 0;
    
    /* Prefetch the address data */
    __builtin_prefetch(addr, 0, 3);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch next level */
        if (node->children[index]) {
            __builtin_prefetch(node->children[index], 0, 3);
            __builtin_prefetch(&node->children[index]->valid_bitmap[0], 0, 3);
            __builtin_prefetch(&node->children[index]->prefix_info[0], 0, 3);
        }
        
        /* Check if we have a valid prefix at this level */
        if (lpm_bitmap_get_branchless(node->valid_bitmap, index) && node->prefix_info[index]) {
            /* Add this prefix to results */
            if (lpm_result_add(result, node->prefix_info[index]) < 0) {
                /* Failed to add - probably out of memory */
                lpm_result_destroy(result);
                return NULL;
            }
        }
        
        /* Move to next level */
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return result;
}

#ifdef LPM_HAVE_SSE2
/* SSE2 optimized lookup all */
lpm_result_t* lpm_lookup_all_sse2(const struct lpm_trie *trie, const uint8_t *addr)
{
    lpm_result_t *result = lpm_result_create(LPM_MAX_RESULTS);
    if (!result) {
        return NULL;
    }
    
    const lpm_node_t *node = trie->root;
    uint8_t depth = 0;
    
    /* Prefetch with SSE */
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch next level with SSE */
        if (node->children[index]) {
            _mm_prefetch((const char*)node->children[index], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_T0);
        }
        
        /* Check validity using SSE for alignment */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        if (valid && node->prefix_info[index]) {
            if (lpm_result_add(result, node->prefix_info[index]) < 0) {
                lpm_result_destroy(result);
                return NULL;
            }
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return result;
}
#endif

#ifdef LPM_HAVE_AVX2
/* AVX2 optimized lookup all */
lpm_result_t* lpm_lookup_all_avx2(const struct lpm_trie *trie, const uint8_t *addr)
{
    lpm_result_t *result = lpm_result_create(LPM_MAX_RESULTS);
    if (!result) {
        return NULL;
    }
    
    const lpm_node_t *node = trie->root;
    uint8_t depth = 0;
    
    /* Prefetch multiple cache lines */
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
        
        /* Check validity */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        if (valid && node->prefix_info[index]) {
            if (lpm_result_add(result, node->prefix_info[index]) < 0) {
                lpm_result_destroy(result);
                return NULL;
            }
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return result;
}
#endif

#ifdef LPM_HAVE_AVX512F
/* AVX512 optimized lookup all */
lpm_result_t* lpm_lookup_all_avx512(const struct lpm_trie *trie, const uint8_t *addr)
{
    lpm_result_t *result = lpm_result_create(LPM_MAX_RESULTS);
    if (!result) {
        return NULL;
    }
    
    const lpm_node_t *node = trie->root;
    uint8_t depth = 0;
    
    /* Prefetch with AVX512 */
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;
        uint8_t index = addr[byte_index];
        
        /* Prefetch with non-temporal hint */
        if (node->children[index]) {
            _mm_prefetch((const char*)node->children[index], _MM_HINT_T0);
            _mm_prefetch((const char*)&node->children[index]->valid_bitmap[0], _MM_HINT_NTA);
        }
        
        /* Check validity */
        uint32_t valid = lpm_bitmap_get_branchless(node->valid_bitmap, index);
        
        if (valid && node->prefix_info[index]) {
            if (lpm_result_add(result, node->prefix_info[index]) < 0) {
                lpm_result_destroy(result);
                return NULL;
            }
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return result;
}
#endif

/* Batch lookup all - process multiple addresses */
void lpm_lookup_all_batch_generic(const struct lpm_trie *trie, const uint8_t **addrs, 
                                 struct lpm_result **results, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        results[i] = trie->lookup_all(trie, addrs[i]);
    }
}

#ifdef LPM_HAVE_SSE2
/* SSE2 batch lookup all - process 4 addresses at a time */
void lpm_lookup_all_batch_sse2(const struct lpm_trie *trie, const uint8_t **addrs, 
                               struct lpm_result **results, size_t count)
{
    size_t i = 0;
    
    /* Process 4 addresses at a time */
    for (; i + 3 < count; i += 4) {
        /* Prefetch all 4 addresses */
        _mm_prefetch((const char*)addrs[i], _MM_HINT_T0);
        _mm_prefetch((const char*)addrs[i+1], _MM_HINT_T0);
        _mm_prefetch((const char*)addrs[i+2], _MM_HINT_T0);
        _mm_prefetch((const char*)addrs[i+3], _MM_HINT_T0);
        
        /* Process each address */
        for (int j = 0; j < 4; j++) {
            results[i + j] = trie->lookup_all(trie, addrs[i + j]);
        }
    }
    
    /* Process remaining addresses */
    for (; i < count; i++) {
        results[i] = trie->lookup_all(trie, addrs[i]);
    }
}
#endif

/* Function to select the best lookup all implementation based on CPU features */
void lpm_select_lookup_all_function(lpm_trie_t *trie)
{
#ifdef LPM_HAVE_AVX512F
    if (trie->cpu_features & LPM_CPU_AVX512F) {
        trie->lookup_all = lpm_lookup_all_avx512;
        return;
    }
#endif

#ifdef LPM_HAVE_AVX2
    if (trie->cpu_features & LPM_CPU_AVX2) {
        trie->lookup_all = lpm_lookup_all_avx2;
        return;
    }
#endif

#ifdef LPM_HAVE_SSE2
    if (trie->cpu_features & LPM_CPU_SSE2) {
        trie->lookup_all = lpm_lookup_all_sse2;
        return;
    }
#endif

    /* Default to optimized generic implementation */
    trie->lookup_all = lpm_lookup_all_optimized;
}

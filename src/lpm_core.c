#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#ifdef LPM_X86_ARCH
#include <cpuid.h>
#endif
#include "../include/lpm.h"

/* Version information */
static const char *lpm_version = "liblpm 1.0.0";

/* Forward declarations for lookup functions */
static uint32_t lpm_lookup_single_generic(const struct lpm_trie *trie, const uint8_t *addr);
static void lpm_lookup_batch_generic(const struct lpm_trie *trie, const uint8_t **addrs, 
                                    uint32_t *next_hops, size_t count);
static struct lpm_result* lpm_lookup_all_generic(const struct lpm_trie *trie, const uint8_t *addr);

/* Forward declarations for optimized function selectors */
extern void lpm_select_single_lookup_function(lpm_trie_t *trie);
extern void lpm_select_batch_lookup_function(lpm_trie_t *trie);
extern void lpm_select_lookup_all_function(lpm_trie_t *trie);

/* CPU feature detection using CPUID */
uint32_t lpm_detect_cpu_features(void)
{
    uint32_t features = 0;
    
#ifdef LPM_X86_ARCH
    unsigned int eax, ebx, ecx, edx;
    
    /* Check for CPUID support */
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (edx & (1 << 25)) features |= LPM_CPU_SSE;
        if (edx & (1 << 26)) features |= LPM_CPU_SSE2;
        if (ecx & (1 << 0))  features |= LPM_CPU_SSE3;
        if (ecx & (1 << 19)) features |= LPM_CPU_SSE4_1;
        if (ecx & (1 << 20)) features |= LPM_CPU_SSE4_2;
        if (ecx & (1 << 28)) features |= LPM_CPU_AVX;
    }
    
    /* Check extended features */
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1U << 5))  features |= LPM_CPU_AVX2;
        if (ebx & (1U << 16)) features |= LPM_CPU_AVX512F;
        if (ebx & (1U << 17)) features |= LPM_CPU_AVX512DQ;
        if (ebx & (1U << 30)) features |= LPM_CPU_AVX512BW;
        if (ebx & (1U << 31)) features |= LPM_CPU_AVX512VL;
    }

    /* Additional safety checks for AVX2 and AVX512 */
    /* Check if OS supports AVX2 (XSAVE and YMM state) */
    if (features & LPM_CPU_AVX2) {
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            if (!(ecx & (1 << 27))) { /* XSAVE not supported */
                features &= ~LPM_CPU_AVX2;
            }
        }
    }

    /* Check if OS supports AVX512 (XSAVE and ZMM state) */
    if (features & LPM_CPU_AVX512F) {
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            if (!(ecx & (1 << 27))) { /* XSAVE not supported */
                features &= ~(LPM_CPU_AVX512F | LPM_CPU_AVX512VL | LPM_CPU_AVX512DQ | LPM_CPU_AVX512BW);
            }
        }
    }
#endif

    return features;
}

/* Allocate and initialize a new node */
static lpm_node_t *lpm_node_alloc(lpm_trie_t *trie, uint8_t depth)
{
    lpm_node_t *node = (lpm_node_t *)trie->alloc(sizeof(lpm_node_t));
    if (!node) {
        return NULL;
    }
    
    /* Initialize node - use memset for cache-aligned structure */
    memset(node, 0, sizeof(lpm_node_t));
    node->depth = depth;
    
    /* Initialize all next hops as invalid */
    for (int i = 0; i < LPM_STRIDE_SIZE; i++) {
        node->next_hops[i] = LPM_INVALID_NEXT_HOP;
    }
    
    trie->num_nodes++;
    return node;
}

/* Default memory allocator */
static void *default_alloc(size_t size)
{
    /* Ensure size is a multiple of alignment for aligned_alloc */
    size_t aligned_size = (size + LPM_CACHE_LINE_SIZE - 1) & ~(LPM_CACHE_LINE_SIZE - 1);
    return aligned_alloc(LPM_CACHE_LINE_SIZE, aligned_size);
}

/* Default memory deallocator */
static void default_free(void *ptr)
{
    free(ptr);
}

/* Create LPM trie with custom allocators */
lpm_trie_t *lpm_create_custom(uint8_t max_depth, 
                              void *(*alloc_fn)(size_t), 
                              void (*free_fn)(void *))
{
    /* Validate parameters */
    if (max_depth != LPM_IPV4_MAX_DEPTH && max_depth != LPM_IPV6_MAX_DEPTH) {
        return NULL;
    }
    
    /* Allocate trie structure */
    lpm_trie_t *trie = (lpm_trie_t *)alloc_fn(sizeof(lpm_trie_t));
    if (!trie) {
        return NULL;
    }
    
    /* Initialize trie */
    memset(trie, 0, sizeof(lpm_trie_t));
    trie->max_depth = max_depth;
    trie->stride_bits = LPM_STRIDE_BITS;
    trie->alloc = alloc_fn;
    trie->free = free_fn;
    
    /* Detect CPU features */
    trie->cpu_features = lpm_detect_cpu_features();
    
    /* Set function pointers based on CPU features */
    trie->lookup_single = lpm_lookup_single_generic;
    trie->lookup_batch = lpm_lookup_batch_generic;
    trie->lookup_all = lpm_lookup_all_generic;
    
    /* Select optimized implementations based on detected CPU features */
    lpm_select_single_lookup_function(trie);
    lpm_select_batch_lookup_function(trie);
    lpm_select_lookup_all_function(trie);
    
    /* Allocate root node */
    trie->root = lpm_node_alloc(trie, 0);
    if (!trie->root) {
        free_fn(trie);
        return NULL;
    }
    
    return trie;
}

/* Create LPM trie with default allocators */
lpm_trie_t *lpm_create(uint8_t max_depth)
{
    return lpm_create_custom(max_depth, default_alloc, default_free);
}

/* Recursively free nodes */
static void lpm_node_free_recursive(lpm_trie_t *trie, lpm_node_t *node)
{
    if (!node) {
        return;
    }
    
    /* Free all children */
    for (int i = 0; i < LPM_STRIDE_SIZE; i++) {
        if (node->children[i]) {
            lpm_node_free_recursive(trie, node->children[i]);
        }
    }
    
    /* Free prefix structures - need to track unique ones to avoid double-free */
    lpm_prefix_t *freed_prefixes[LPM_STRIDE_SIZE] = {NULL};
    int freed_count = 0;
    
    for (int i = 0; i < LPM_STRIDE_SIZE; i++) {
        if (node->prefix_info[i]) {
            /* Check if we've already freed this prefix to avoid double-free */
            bool already_freed = false;
            for (int j = 0; j < freed_count; j++) {
                if (freed_prefixes[j] == node->prefix_info[i]) {
                    already_freed = true;
                    break;
                }
            }
            
            if (!already_freed) {
                trie->free(node->prefix_info[i]);
                freed_prefixes[freed_count++] = node->prefix_info[i];
            }
        }
    }
    
    /* Free this node */
    trie->free(node);
}

/* Destroy LPM trie */
void lpm_destroy(lpm_trie_t *trie)
{
    if (!trie) {
        return;
    }
    
    /* Free all nodes */
    lpm_node_free_recursive(trie, trie->root);
    
    /* Free trie structure */
    trie->free(trie);
}

/* Check if a bit is set in the bitmap */
static inline bool lpm_bitmap_get(const uint32_t *bitmap, uint8_t index)
{
    uint32_t word = index / 32;
    uint32_t bit = index % 32;
    return (bitmap[word] >> bit) & 1;
}

/* Set a bit in the bitmap */
static inline void lpm_bitmap_set(uint32_t *bitmap, uint8_t index)
{
    uint32_t word = index / 32;
    uint32_t bit = index % 32;
    bitmap[word] |= (1U << bit);
}

/* Add a prefix to the trie with user data */
int lpm_add_prefix(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, void *user_data)
{
    if (!trie || !prefix || prefix_len > trie->max_depth) {
        return -1;
    }
    
    lpm_node_t *node = trie->root;
    uint8_t depth = 0;
    
    /* Traverse/create nodes for complete strides */
    while (depth + LPM_STRIDE_BITS < prefix_len) {
        uint8_t byte_index = depth >> 3;  /* depth / 8 */
        uint8_t index = prefix[byte_index];
        
        /* Create child node if it doesn't exist */
        if (!node->children[index]) {
            node->children[index] = lpm_node_alloc(trie, depth + LPM_STRIDE_BITS);
            if (!node->children[index]) {
                return -1;  /* Out of memory */
            }
        }
        
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    /* Create prefix info structure */
    lpm_prefix_t *pinfo = (lpm_prefix_t *)trie->alloc(sizeof(lpm_prefix_t));
    if (!pinfo) {
        return -1;
    }
    
    /* Copy prefix data */
    memset(pinfo, 0, sizeof(lpm_prefix_t));
    memcpy(pinfo->prefix, prefix, (prefix_len + 7) / 8);
    pinfo->prefix_len = prefix_len;
    pinfo->user_data = user_data;
    
    /* Handle remaining bits (partial stride) */
    if (depth < prefix_len) {
        uint8_t remaining_bits = prefix_len - depth;
        uint8_t byte_index = depth / 8;
        uint8_t bit_offset = depth % 8;
        
        /* Extract the remaining bits */
        uint8_t index = prefix[byte_index] >> (8 - remaining_bits - bit_offset);
        index &= (1 << remaining_bits) - 1;
        
        if (remaining_bits == LPM_STRIDE_BITS && prefix_len == depth + LPM_STRIDE_BITS) {
            /* Exactly one full byte remaining - this is an exact match prefix */
            index = prefix[byte_index];
            node->prefix_info[index] = pinfo;
            lpm_bitmap_set(node->valid_bitmap, index);
        } else {
            /* Partial byte OR not the last byte - expand to all possible completions */
            uint32_t num_entries = 1U << (LPM_STRIDE_BITS - remaining_bits);
            uint32_t base_index = index << (LPM_STRIDE_BITS - remaining_bits);
            
            if (remaining_bits == LPM_STRIDE_BITS) {
                /* Full byte but not the last - set all 256 entries */
                for (int i = 0; i < LPM_STRIDE_SIZE; i++) {
                    node->prefix_info[i] = pinfo;
                    lpm_bitmap_set(node->valid_bitmap, i);
                }
            } else {
                /* Partial byte - expand based on the prefix bits */
                for (uint32_t i = 0; i < num_entries; i++) {
                    node->prefix_info[base_index + i] = pinfo;
                    lpm_bitmap_set(node->valid_bitmap, base_index + i);
                }
            }
        }
    } else {
        /* Exact stride boundary - set all entries */
        for (int i = 0; i < LPM_STRIDE_SIZE; i++) {
            /* TODO: Handle memory management properly - for now, just overwrite */
            node->prefix_info[i] = pinfo;
            lpm_bitmap_set(node->valid_bitmap, i);
        }
    }
    
    trie->num_prefixes++;
    return 0;
}

/* Backward compatibility - add with next hop */
int lpm_add(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    /* For backward compatibility, just call lpm_add_prefix */
    return lpm_add_prefix(trie, prefix, prefix_len, (void*)(uintptr_t)next_hop);
}

/* Generic single lookup implementation */
static uint32_t lpm_lookup_single_generic(const struct lpm_trie *trie, const uint8_t *addr)
{
    const lpm_node_t *node = trie->root;
    uint32_t next_hop = LPM_INVALID_NEXT_HOP;
    uint8_t depth = 0;
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;  /* depth / 8 */
        uint8_t index = addr[byte_index];
        
        /* Check if we have a valid next hop at this level */
        if (lpm_bitmap_get(node->valid_bitmap, index) && node->prefix_info[index]) {
            next_hop = (uint32_t)(uintptr_t)node->prefix_info[index]->user_data;
        }
        
        /* Prefetch next node for better performance */
        if (node->children[index]) {
            __builtin_prefetch(node->children[index], 0, 3);
        }
        
        /* Move to next level */
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return next_hop;
}

/* Generic batch lookup implementation */
static void lpm_lookup_batch_generic(const struct lpm_trie *trie, const uint8_t **addrs, 
                                    uint32_t *next_hops, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        next_hops[i] = lpm_lookup_single_generic(trie, addrs[i]);
    }
}

/* Generic lookup all implementation */
static lpm_result_t* lpm_lookup_all_generic(const struct lpm_trie *trie, const uint8_t *addr)
{
    /* Create result structure */
    lpm_result_t *result = lpm_result_create(LPM_MAX_RESULTS);
    if (!result) {
        return NULL;
    }
    
    const lpm_node_t *node = trie->root;
    uint8_t depth = 0;
    
    while (node && depth < trie->max_depth) {
        uint8_t byte_index = depth >> 3;  /* depth / 8 */
        uint8_t index = addr[byte_index];
        
        /* Check if we have a valid prefix at this level */
        if (lpm_bitmap_get(node->valid_bitmap, index) && node->prefix_info[index]) {
            /* Add this prefix to results */
            lpm_result_add(result, node->prefix_info[index]);
        }
        
        /* Move to next level */
        node = node->children[index];
        depth += LPM_STRIDE_BITS;
    }
    
    return result;
}

/* Result management functions */
lpm_result_t *lpm_result_create(uint32_t capacity)
{
    lpm_result_t *result = (lpm_result_t *)malloc(sizeof(lpm_result_t));
    if (!result) {
        return NULL;
    }
    
    /* Handle zero capacity case - return NULL as expected by tests */
    if (capacity == 0) {
        free(result);
        return NULL;
    }
    
    result->prefixes = (lpm_prefix_t *)malloc(capacity * sizeof(lpm_prefix_t));
    if (!result->prefixes) {
        free(result);
        return NULL;
    }
    
    result->count = 0;
    result->capacity = capacity;
    return result;
}

void lpm_result_destroy(lpm_result_t *result)
{
    if (result) {
        if (result->prefixes) {
            free(result->prefixes);
        }
        free(result);
    }
}

void lpm_result_clear(lpm_result_t *result)
{
    if (result) {
        result->count = 0;
    }
}

int lpm_result_add(lpm_result_t *result, const lpm_prefix_t *prefix)
{
    if (!result || !prefix) {
        return -1;
    }
    
    if (result->count >= result->capacity) {
        /* Expand capacity */
        uint32_t new_capacity = result->capacity * 2;
        lpm_prefix_t *new_prefixes = (lpm_prefix_t *)realloc(result->prefixes, 
                                                              new_capacity * sizeof(lpm_prefix_t));
        if (!new_prefixes) {
            return -1;
        }
        result->prefixes = new_prefixes;
        result->capacity = new_capacity;
    }
    
    /* Copy prefix */
    memcpy(&result->prefixes[result->count], prefix, sizeof(lpm_prefix_t));
    result->count++;
    return 0;
}

/* Public lookup functions */
uint32_t lpm_lookup(const lpm_trie_t *trie, const uint8_t *addr)
{
    if (!trie || !addr) {
        return LPM_INVALID_NEXT_HOP;
    }
    
    return trie->lookup_single(trie, addr);
}

uint32_t lpm_lookup_ipv4(const lpm_trie_t *trie, uint32_t addr)
{
    if (!trie || trie->max_depth != LPM_IPV4_MAX_DEPTH) {
        return LPM_INVALID_NEXT_HOP;
    }
    
    /* Convert to big-endian byte array */
    uint8_t addr_bytes[4];
    addr_bytes[0] = (addr >> 24) & 0xFF;
    addr_bytes[1] = (addr >> 16) & 0xFF;
    addr_bytes[2] = (addr >> 8) & 0xFF;
    addr_bytes[3] = addr & 0xFF;
    
    return trie->lookup_single(trie, addr_bytes);
}

uint32_t lpm_lookup_ipv6(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr || trie->max_depth != LPM_IPV6_MAX_DEPTH) {
        return LPM_INVALID_NEXT_HOP;
    }
    
    return trie->lookup_single(trie, addr);
}

void lpm_lookup_batch(const lpm_trie_t *trie, const uint8_t **addrs, 
                      uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) {
        return;
    }
    
    trie->lookup_batch(trie, addrs, next_hops, count);
}

/* Public lookup all functions */
lpm_result_t *lpm_lookup_all(const lpm_trie_t *trie, const uint8_t *addr)
{
    if (!trie || !addr) {
        return NULL;
    }
    
    return trie->lookup_all(trie, addr);
}

lpm_result_t *lpm_lookup_all_ipv4(const lpm_trie_t *trie, uint32_t addr)
{
    if (!trie || trie->max_depth != LPM_IPV4_MAX_DEPTH) {
        return NULL;
    }
    
    /* Convert to big-endian byte array */
    uint8_t addr_bytes[4];
    addr_bytes[0] = (addr >> 24) & 0xFF;
    addr_bytes[1] = (addr >> 16) & 0xFF;
    addr_bytes[2] = (addr >> 8) & 0xFF;
    addr_bytes[3] = addr & 0xFF;
    
    return trie->lookup_all(trie, addr_bytes);
}

lpm_result_t *lpm_lookup_all_ipv6(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr || trie->max_depth != LPM_IPV6_MAX_DEPTH) {
        return NULL;
    }
    
    return trie->lookup_all(trie, addr);
}

void lpm_lookup_all_batch(const lpm_trie_t *trie, const uint8_t **addrs, 
                         lpm_result_t **results, size_t count)
{
    if (!trie || !addrs || !results || count == 0) {
        return;
    }
    
    /* For now, use simple loop - can be optimized later */
    for (size_t i = 0; i < count; i++) {
        results[i] = trie->lookup_all(trie, addrs[i]);
    }
}

/* Utility functions */
const char *lpm_get_version(void)
{
    return lpm_version;
}

void lpm_print_stats(const lpm_trie_t *trie)
{
    if (!trie) {
        return;
    }
    
    printf("LPM Trie Statistics:\n");
    printf("  Max depth: %u\n", trie->max_depth);
    printf("  Stride bits: %u\n", trie->stride_bits);
#ifdef LPM_X86_ARCH
    printf("  Number of prefixes: %lu\n", trie->num_prefixes);
    printf("  Number of nodes: %lu\n", trie->num_nodes);
#else
    printf("  Number of prefixes: %llu\n", trie->num_prefixes);
    printf("  Number of nodes: %llu\n", trie->num_nodes);
#endif
    printf("  CPU features: 0x%08x\n", trie->cpu_features);
    
    /* Print detected features */
    printf("  Detected features:");
    if (trie->cpu_features & LPM_CPU_SSE) printf(" SSE");
    if (trie->cpu_features & LPM_CPU_SSE2) printf(" SSE2");
    if (trie->cpu_features & LPM_CPU_SSE3) printf(" SSE3");
    if (trie->cpu_features & LPM_CPU_SSE4_1) printf(" SSE4.1");
    if (trie->cpu_features & LPM_CPU_SSE4_2) printf(" SSE4.2");
    if (trie->cpu_features & LPM_CPU_AVX) printf(" AVX");
    if (trie->cpu_features & LPM_CPU_AVX2) printf(" AVX2");
    if (trie->cpu_features & LPM_CPU_AVX512F) printf(" AVX512F");
    if (trie->cpu_features & LPM_CPU_AVX512VL) printf(" AVX512VL");
    if (trie->cpu_features & LPM_CPU_AVX512DQ) printf(" AVX512DQ");
    if (trie->cpu_features & LPM_CPU_AVX512BW) printf(" AVX512BW");
    printf("\n");
}

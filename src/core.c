/*
 * liblpm Core - Shared Memory Management and Trie Operations
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#ifdef LPM_X86_ARCH
#include <cpuid.h>
#include <immintrin.h>
#endif
#include "../include/lpm.h"
#include "../include/internal.h"

static const char *lpm_version = "liblpm 2.0.0";

/* ============================================================================
 * Node Pool Management
 * ============================================================================ */

static int pool_grow(lpm_trie_t *trie)
{
    uint32_t new_cap = trie->pool_capacity * LPM_POOL_GROWTH_FACTOR;
    size_t new_size = new_cap * sizeof(struct lpm_node);
    
    struct lpm_node *new_pool = (struct lpm_node *)malloc(new_size);
    if (!new_pool) return -1;
    
    memcpy(new_pool, trie->node_pool, trie->pool_used * sizeof(struct lpm_node));
    free(trie->node_pool);
    
    trie->node_pool = new_pool;
    trie->pool_capacity = new_cap;
    return 0;
}

static int wide_pool_grow(lpm_trie_t *trie)
{
    uint32_t new_cap = trie->wide_pool_capacity ? trie->wide_pool_capacity * 2 : 16;
    size_t new_size = new_cap * sizeof(struct lpm_node_16);
    
    struct lpm_node_16 *new_pool = (struct lpm_node_16 *)malloc(new_size);
    if (!new_pool) return -1;
    
    if (trie->wide_nodes_pool) {
        memcpy(new_pool, trie->wide_nodes_pool, trie->wide_pool_used * sizeof(struct lpm_node_16));
        free(trie->wide_nodes_pool);
    }
    
    trie->wide_nodes_pool = new_pool;
    trie->wide_pool_capacity = new_cap;
    return 0;
}

uint32_t node_alloc(lpm_trie_t *trie)
{
    if (trie->pool_used >= trie->pool_capacity) {
        if (pool_grow(trie) != 0) return LPM_INVALID_INDEX;
    }
    
    uint32_t idx = trie->pool_used++;
    
    /* For DIR-24-8, skip index 0 as it's reserved as invalid */
    if (trie->use_ipv4_dir24 && idx == 0 && trie->pool_used < trie->pool_capacity) {
        idx = trie->pool_used++;
    }
    
    struct lpm_node *n = &((struct lpm_node *)trie->node_pool)[idx];
    memset(n, 0, sizeof(struct lpm_node));
    
    for (int i = 0; i < LPM_STRIDE_SIZE_8; i++) {
        n->entries[i].next_hop = LPM_INVALID_NEXT_HOP;
    }
    
    trie->num_nodes++;
    return idx;
}

uint32_t wide_node_alloc(lpm_trie_t *trie)
{
    if (trie->wide_pool_used >= trie->wide_pool_capacity) {
        if (wide_pool_grow(trie) != 0) return LPM_INVALID_INDEX;
    }
    
    uint32_t idx = trie->wide_pool_used++;
    struct lpm_node_16 *n = &((struct lpm_node_16 *)trie->wide_nodes_pool)[idx];
    memset(n, 0, sizeof(struct lpm_node_16));
    
    for (int i = 0; i < LPM_STRIDE_SIZE_16; i++) {
        n->entries[i].next_hop = LPM_INVALID_NEXT_HOP;
    }
    
    trie->num_wide_nodes++;
    
    /* Note: Index 0 is valid for wide nodes, unlike regular nodes */
    return idx;
}

/* ============================================================================
 * Cache Management
 * ============================================================================ */

void lpm_cache_invalidate(lpm_trie_t *trie)
{
    if (trie && trie->hot_cache) {
        memset(trie->hot_cache, 0, LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry));
    }
}

/* ============================================================================
 * Trie Destruction
 * ============================================================================ */

void lpm_destroy(lpm_trie_t *trie)
{
    if (!trie) return;
    if (trie->node_pool) free(trie->node_pool);
    if (trie->wide_nodes_pool) free(trie->wide_nodes_pool);
    if (trie->dir24_table) free(trie->dir24_table);
    if (trie->tbl8_groups) free(trie->tbl8_groups);
    if (trie->direct_table) free(trie->direct_table);
    if (trie->hot_cache) free(trie->hot_cache);
    free(trie);
}

/* ============================================================================
 * lpm_create with depth parameter
 * ============================================================================ */

lpm_trie_t *lpm_create(uint8_t max_depth)
{
    if (max_depth == LPM_IPV4_MAX_DEPTH) {
#if defined(LPM_IPV4_DEFAULT_dir24)
        return lpm_create_ipv4_dir24();
#else
        return lpm_create_ipv4_8stride();
#endif
    } else if (max_depth == LPM_IPV6_MAX_DEPTH) {
#if defined(LPM_IPV6_DEFAULT_wide16)
        return lpm_create_ipv6_wide16();
#else
        return lpm_create_ipv6_8stride();
#endif
    }
    return NULL;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *lpm_get_version(void) { return lpm_version; }

void lpm_print_stats(const lpm_trie_t *trie)
{
    if (!trie) return;
    
    size_t node_size = sizeof(lpm_node_t);
    size_t pool_mem = trie->pool_capacity * node_size;
    size_t used_mem = trie->pool_used * node_size;
    
    printf("LPM Trie Statistics:\n");
    printf("  Version: %s\n", lpm_version);
    printf("  Max depth: %u bits\n", trie->max_depth);
    
    if (trie->use_ipv4_dir24) {
        printf("  Algorithm: DIR-24-8\n");
        printf("  Prefixes: %llu\n", (unsigned long long)trie->num_prefixes);
        printf("  TBL8 groups: %u / %u\n", trie->tbl8_groups_used, trie->tbl8_num_groups);
        
        size_t dir24_mem = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry);
        size_t tbl8_mem = trie->tbl8_groups_used * 256 * sizeof(struct lpm_tbl8_entry);
        printf("  Memory: DIR24=%.2f MB, TBL8=%.2f MB, Total=%.2f MB\n",
               dir24_mem / (1024.0 * 1024.0),
               tbl8_mem / (1024.0 * 1024.0),
               (dir24_mem + tbl8_mem) / (1024.0 * 1024.0));
    } else if (trie->use_ipv6_wide_stride) {
        printf("  Algorithm: Wide 16-bit stride (IPv6)\n");
        printf("  Prefixes: %llu\n", (unsigned long long)trie->num_prefixes);
        printf("  8-bit nodes: %llu\n", (unsigned long long)trie->num_nodes);
        printf("  16-bit nodes: %llu\n", (unsigned long long)trie->num_wide_nodes);
        
        size_t wide_mem = trie->num_wide_nodes * sizeof(struct lpm_node_16);
        printf("  Total memory: %.2f MB (8-bit: %.2f MB, 16-bit: %.2f MB)\n", 
               (used_mem + wide_mem) / (1024.0 * 1024.0),
               used_mem / (1024.0 * 1024.0),
               wide_mem / (1024.0 * 1024.0));
    } else {
        printf("  Algorithm: 8-bit stride\n");
        printf("  Prefixes: %llu\n", (unsigned long long)trie->num_prefixes);
        printf("  Nodes: %llu\n", (unsigned long long)trie->num_nodes);
        printf("  Node size: %zu bytes\n", node_size);
        printf("  Pool: %.2f MB allocated, %.2f MB used\n", 
               pool_mem / (1024.0 * 1024.0), used_mem / (1024.0 * 1024.0));
    }
    printf("  Huge pages: %s\n", trie->use_huge_pages ? "enabled" : "disabled");
    printf("  Direct table: %s\n", trie->direct_table ? "enabled (256KB)" : "disabled");
    printf("  Hot cache: %s", trie->hot_cache ? "enabled" : "disabled");
    if (trie->hot_cache) {
        printf(" (hits: %llu, misses: %llu, ratio: %.1f%%)\n",
               (unsigned long long)trie->cache_hits,
               (unsigned long long)trie->cache_misses,
               trie->cache_hits + trie->cache_misses > 0 ?
               100.0 * trie->cache_hits / (trie->cache_hits + trie->cache_misses) : 0);
    } else {
        printf("\n");
    }
    simd_level_t level = detect_simd_level();
    printf("  SIMD level: %s (via ifunc dispatch)\n", simd_level_name(level));
}

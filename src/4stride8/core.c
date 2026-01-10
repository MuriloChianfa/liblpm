/*
 * IPv4 8-bit Stride Algorithm - Core Functions
 * Create, Add, Delete operations for IPv4 with 8-bit stride trie
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../include/lpm.h"
#include "../../include/internal.h"

/* ============================================================================
 * Trie Creation
 * ============================================================================ */

lpm_trie_t *lpm_create_ipv4_8stride(void)
{
    lpm_trie_t *t = (lpm_trie_t *)aligned_alloc(LPM_CACHE_LINE_SIZE, sizeof(lpm_trie_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(lpm_trie_t));
    
    t->max_depth = LPM_IPV4_MAX_DEPTH;
    t->use_huge_pages = false;
    t->default_next_hop = LPM_INVALID_NEXT_HOP;
    t->use_ipv6_wide_stride = false;
    t->use_ipv4_dir24 = false;
    
    /* Allocate node pool */
    size_t pool_size = LPM_INITIAL_POOL_SIZE * sizeof(struct lpm_node);
    t->node_pool = malloc(pool_size);
    if (!t->node_pool) {
        free(t);
        return NULL;
    }
    t->pool_capacity = LPM_INITIAL_POOL_SIZE;
    t->pool_used = 1;
    memset(t->node_pool, 0, sizeof(struct lpm_node));
    
    /* Allocate root node */
    t->root_idx = node_alloc(t);
    if (t->root_idx == LPM_INVALID_INDEX) {
        free(t->node_pool);
        free(t);
        return NULL;
    }
    
    /* Direct table for IPv4 (16-bit prefix optimization) */
    size_t dt_size = LPM_DIRECT_SIZE * sizeof(struct lpm_direct_entry);
    t->direct_table = (struct lpm_direct_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, dt_size);
    if (t->direct_table) {
        for (uint32_t i = 0; i < LPM_DIRECT_SIZE; i++) {
            t->direct_table[i].next_hop = LPM_INVALID_NEXT_HOP;
            t->direct_table[i].node_idx = t->root_idx;
            t->direct_table[i].prefix_len = 0;
        }
    }
    
    /* Hot cache */
    size_t cache_size = LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry);
    t->hot_cache = (struct lpm_cache_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, cache_size);
    if (t->hot_cache) {
        memset(t->hot_cache, 0, cache_size);
    }
    
    return t;
}

/* ============================================================================
 * Direct Table Update (for prefixes <= 16 bits)
 * ============================================================================ */

static void direct_table_update(lpm_trie_t *trie, const uint8_t *prefix,
                                uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie->direct_table || trie->max_depth != LPM_IPV4_MAX_DEPTH) return;
    if (prefix_len > 16) return;
    
    uint32_t base = ((uint32_t)prefix[0] << 8) | prefix[1];
    
    if (prefix_len < 16) {
        base &= ~((1U << (16 - prefix_len)) - 1);
    }
    
    uint32_t count = 1U << (16 - prefix_len);
    
    for (uint32_t i = 0; i < count; i++) {
        struct lpm_direct_entry *e = &trie->direct_table[base + i];
        if (e->prefix_len <= prefix_len) {
            e->next_hop = next_hop;
            e->prefix_len = prefix_len;
        }
    }
}

/* ============================================================================
 * Add Prefix
 * ============================================================================ */

int lpm_add_ipv4_8stride(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie || !prefix || prefix_len > LPM_IPV4_MAX_DEPTH) return -1;
    
    /* Invalidate cache */
    if (trie->hot_cache) {
        memset(trie->hot_cache, 0, LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry));
    }
    
    if (prefix_len == 0) {
        trie->default_next_hop = next_hop;
        trie->has_default_route = true;
        trie->num_prefixes++;
        return 0;
    }
    
    uint32_t node_idx = trie->root_idx;
    uint8_t depth = 0;
    
    /* Traverse/create nodes for complete bytes */
    while (depth + 8 <= prefix_len) {
        uint8_t byte_idx = depth >> 3;
        uint8_t index = prefix[byte_idx];
        
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        uint32_t cv = node->entries[index].child_and_valid;
        uint32_t child_idx = cv & LPM_CHILD_MASK;
        
        if (depth + 8 == prefix_len) {
            /* Set next_hop at this entry */
            node->entries[index].child_and_valid = (cv & LPM_CHILD_MASK) | LPM_VALID_FLAG;
            node->entries[index].next_hop = next_hop;
            
            if (prefix_len <= 16) {
                direct_table_update(trie, prefix, prefix_len, next_hop);
            }
            
            trie->num_prefixes++;
            return 0;
        }
        
        if (child_idx == LPM_INVALID_INDEX) {
            child_idx = node_alloc(trie);
            if (child_idx == LPM_INVALID_INDEX) return -1;
            node = &((struct lpm_node *)trie->node_pool)[node_idx];
            node->entries[index].child_and_valid =
                (node->entries[index].child_and_valid & LPM_VALID_FLAG) | child_idx;
        }
        
        node_idx = child_idx;
        depth += 8;
    }
    
    /* Handle partial byte */
    if (depth < prefix_len) {
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        uint8_t remaining = prefix_len - depth;
        uint8_t byte_idx = depth >> 3;
        uint8_t prefix_byte = prefix[byte_idx];
        
        uint8_t mask = ~((1 << (8 - remaining)) - 1);
        uint8_t base = prefix_byte & mask;
        uint32_t count = 1U << (8 - remaining);
        
        for (uint32_t i = 0; i < count; i++) {
            uint8_t idx = base + i;
            node->entries[idx].child_and_valid |= LPM_VALID_FLAG;
            node->entries[idx].next_hop = next_hop;
        }
        
        if (prefix_len <= 16) {
            direct_table_update(trie, prefix, prefix_len, next_hop);
        }
    }
    
    trie->num_prefixes++;
    return 0;
}

/* ============================================================================
 * Delete Prefix
 * ============================================================================ */

int lpm_delete_ipv4_8stride(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len)
{
    if (!trie || !prefix || prefix_len > LPM_IPV4_MAX_DEPTH) return -1;
    
    if (trie->hot_cache) {
        memset(trie->hot_cache, 0, LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry));
    }
    
    if (prefix_len == 0) {
        trie->has_default_route = false;
        trie->default_next_hop = LPM_INVALID_NEXT_HOP;
        if (trie->num_prefixes > 0) trie->num_prefixes--;
        return 0;
    }
    
    uint32_t node_idx = trie->root_idx;
    uint8_t depth = 0;
    
    while (depth + 8 <= prefix_len) {
        uint8_t byte_idx = depth >> 3;
        uint8_t index = prefix[byte_idx];
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        
        if (depth + 8 == prefix_len) {
            node->entries[index].child_and_valid &= ~LPM_VALID_FLAG;
            node->entries[index].next_hop = LPM_INVALID_NEXT_HOP;
            if (trie->num_prefixes > 0) trie->num_prefixes--;
            return 0;
        }
        
        uint32_t child_idx = node->entries[index].child_and_valid & LPM_CHILD_MASK;
        if (child_idx == LPM_INVALID_INDEX) return -1;
        
        node_idx = child_idx;
        depth += 8;
    }
    
    if (depth < prefix_len) {
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        uint8_t remaining = prefix_len - depth;
        uint8_t byte_idx = depth >> 3;
        uint8_t prefix_byte = prefix[byte_idx];
        
        uint8_t mask = ~((1 << (8 - remaining)) - 1);
        uint8_t base = prefix_byte & mask;
        uint32_t count = 1U << (8 - remaining);
        
        for (uint32_t i = 0; i < count; i++) {
            uint8_t idx = base + i;
            node->entries[idx].child_and_valid &= ~LPM_VALID_FLAG;
            node->entries[idx].next_hop = LPM_INVALID_NEXT_HOP;
        }
    }
    
    if (trie->num_prefixes > 0) trie->num_prefixes--;
    return 0;
}

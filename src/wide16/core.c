/*
 * IPv6 Wide 16-bit Stride Algorithm - Core Functions
 * Create, Add, Delete operations for IPv6 with wide stride (16-16-16-8-8-8...)
 *
 * Uses 16-16-16-8-8-8... stride pattern for IPv6:
 * - First 48 bits: 3 levels of 16-bit stride (covers common /48 allocations)
 * - Remaining 80 bits: 10 levels of 8-bit stride
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

lpm_trie_t *lpm_create_ipv6_wide16(void)
{
    lpm_trie_t *t = (lpm_trie_t *)aligned_alloc(LPM_CACHE_LINE_SIZE, sizeof(lpm_trie_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(lpm_trie_t));
    
    t->max_depth = LPM_IPV6_MAX_DEPTH;
    t->use_huge_pages = false;
    t->default_next_hop = LPM_INVALID_NEXT_HOP;
    t->use_ipv6_wide_stride = true;  /* Enable wide stride */
    t->use_ipv4_dir24 = false;
    
    /* Allocate regular node pool (for 8-bit stride levels) */
    size_t pool_size = LPM_INITIAL_POOL_SIZE * sizeof(struct lpm_node);
    t->node_pool = malloc(pool_size);
    if (!t->node_pool) {
        free(t);
        return NULL;
    }
    t->pool_capacity = LPM_INITIAL_POOL_SIZE;
    t->pool_used = 1;
    memset(t->node_pool, 0, sizeof(struct lpm_node));
    
    /* Allocate wide stride pool (for 16-bit stride levels) */
    size_t wide_pool_size = 16 * sizeof(struct lpm_node_16);
    t->wide_nodes_pool = malloc(wide_pool_size);
    if (!t->wide_nodes_pool) {
        free(t->node_pool);
        free(t);
        return NULL;
    }
    t->wide_pool_capacity = 16;
    t->wide_pool_used = 0;
    
    /* Allocate root wide node */
    t->root_idx = wide_node_alloc(t);
    /* Note: For wide nodes, index 0 is valid */
    
    /* Hot cache */
    size_t cache_size = LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry);
    t->hot_cache = (struct lpm_cache_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, cache_size);
    if (t->hot_cache) {
        memset(t->hot_cache, 0, cache_size);
    }
    
    return t;
}

/* ============================================================================
 * Add Prefix
 * ============================================================================ */

int lpm_add_ipv6_wide16(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie || !prefix || prefix_len > 128) return -1;
    
    /* Handle default route */
    if (prefix_len == 0) {
        trie->has_default_route = true;
        trie->default_next_hop = next_hop;
        trie->num_prefixes++;
        return 0;
    }
    
    uint32_t node_idx = trie->root_idx;
    uint8_t depth = 0;
    bool in_wide_levels = true;
    
    /* First 3 levels: 16-bit wide stride */
    for (uint8_t level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS && depth < prefix_len; level++) {
        uint16_t stride_bits = 16;
        
        if (depth + stride_bits > prefix_len) {
            /* Partial level - need to expand to all matching entries */
            struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
            uint8_t remaining = prefix_len - depth;
            
            /* Extract base index */
            uint16_t base_index = ((uint16_t)prefix[level * 2] << 8) | prefix[level * 2 + 1];
            base_index &= ~((1 << (16 - remaining)) - 1);
            
            /* Expand to all matching entries */
            uint32_t count = 1 << (16 - remaining);
            for (uint32_t i = 0; i < count; i++) {
                uint16_t idx = base_index + i;
                wide_node->entries[idx].child_and_valid |= LPM_VALID_FLAG;
                wide_node->entries[idx].next_hop = next_hop;
            }
            
            trie->num_prefixes++;
            return 0;
        }
        
        /* Full 16-bit level */
        struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
        uint16_t index = ((uint16_t)prefix[level * 2] << 8) | prefix[level * 2 + 1];
        
        if (depth + stride_bits == prefix_len) {
            /* Terminal node at this level */
            wide_node->entries[index].child_and_valid |= LPM_VALID_FLAG;
            wide_node->entries[index].next_hop = next_hop;
            trie->num_prefixes++;
            return 0;
        }
        
        /* Need to go deeper */
        uint32_t child_idx = wide_node->entries[index].child_and_valid & LPM_CHILD_MASK;
        if (child_idx == LPM_INVALID_INDEX) {
            /* Allocate next level */
            uint32_t flags = (wide_node->entries[index].child_and_valid & LPM_VALID_FLAG);
            
            if (level + 1 < LPM_IPV6_WIDE_STRIDE_LEVELS) {
                /* Next level is also wide (16-bit) */
                child_idx = wide_node_alloc(trie);
                flags |= LPM_WIDE_NODE_FLAG;  /* Mark as wide node */
            } else {
                /* Transition to 8-bit nodes */
                child_idx = node_alloc(trie);
                in_wide_levels = false;
            }
            
            if (child_idx == LPM_INVALID_INDEX) return -1;
            
            /* Refresh pointer after potential realloc */
            wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
            wide_node->entries[index].child_and_valid = flags | child_idx;
        } else {
            /* Check if this is transitioning to regular nodes */
            if (level + 1 >= LPM_IPV6_WIDE_STRIDE_LEVELS) {
                in_wide_levels = false;
            }
        }
        
        node_idx = child_idx;
        depth += stride_bits;
    }
    
    /* Remaining levels: 8-bit stride */
    while (depth < prefix_len) {
        uint8_t byte_idx = depth >> 3;
        uint8_t index = prefix[byte_idx];
        
        if (depth + 8 > prefix_len) {
            /* Partial byte */
            struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
            uint8_t remaining = prefix_len - depth;
            uint8_t mask = ~((1 << (8 - remaining)) - 1);
            uint8_t base = index & mask;
            uint32_t count = 1 << (8 - remaining);
            
            for (uint32_t i = 0; i < count; i++) {
                uint8_t idx = base + i;
                node->entries[idx].child_and_valid |= LPM_VALID_FLAG;
                node->entries[idx].next_hop = next_hop;
            }
            
            trie->num_prefixes++;
            return 0;
        }
        
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        
        if (depth + 8 == prefix_len) {
            node->entries[index].child_and_valid |= LPM_VALID_FLAG;
            node->entries[index].next_hop = next_hop;
            trie->num_prefixes++;
            return 0;
        }
        
        uint32_t child_idx = node->entries[index].child_and_valid & LPM_CHILD_MASK;
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
    
    trie->num_prefixes++;
    return 0;
}

/* ============================================================================
 * Delete Prefix
 * ============================================================================ */

int lpm_delete_ipv6_wide16(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len)
{
    if (!trie || !prefix || prefix_len > 128) return -1;
    
    /* Handle default route */
    if (prefix_len == 0) {
        trie->has_default_route = false;
        trie->default_next_hop = LPM_INVALID_NEXT_HOP;
        if (trie->num_prefixes > 0) trie->num_prefixes--;
        return 0;
    }
    
    uint32_t node_idx = trie->root_idx;
    uint8_t depth = 0;
    
    /* First levels: 16-bit wide stride */
    for (uint8_t level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS && depth < prefix_len; level++) {
        uint16_t stride_bits = 16;
        
        if (depth + stride_bits > prefix_len) {
            /* Partial level */
            struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
            uint8_t remaining = prefix_len - depth;
            
            uint16_t base_index = ((uint16_t)prefix[level * 2] << 8) | prefix[level * 2 + 1];
            base_index &= ~((1 << (16 - remaining)) - 1);
            
            uint32_t count = 1 << (16 - remaining);
            for (uint32_t i = 0; i < count; i++) {
                uint16_t idx = base_index + i;
                wide_node->entries[idx].child_and_valid &= ~LPM_VALID_FLAG;
                wide_node->entries[idx].next_hop = LPM_INVALID_NEXT_HOP;
            }
            
            if (trie->num_prefixes > 0) trie->num_prefixes--;
            return 0;
        }
        
        struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
        uint16_t index = ((uint16_t)prefix[level * 2] << 8) | prefix[level * 2 + 1];
        
        if (depth + stride_bits == prefix_len) {
            wide_node->entries[index].child_and_valid &= ~LPM_VALID_FLAG;
            wide_node->entries[index].next_hop = LPM_INVALID_NEXT_HOP;
            if (trie->num_prefixes > 0) trie->num_prefixes--;
            return 0;
        }
        
        uint32_t child_idx = wide_node->entries[index].child_and_valid & LPM_CHILD_MASK;
        bool has_child = (child_idx != 0) || (wide_node->entries[index].child_and_valid & LPM_WIDE_NODE_FLAG);
        if (!has_child) {
            return -1;  /* Prefix not found */
        }
        
        node_idx = child_idx;
        depth += stride_bits;
    }
    
    /* Remaining levels: 8-bit stride */
    while (depth < prefix_len) {
        uint8_t byte_idx = depth >> 3;
        uint8_t index = prefix[byte_idx];
        
        if (depth + 8 > prefix_len) {
            struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
            uint8_t remaining = prefix_len - depth;
            uint8_t mask = ~((1 << (8 - remaining)) - 1);
            uint8_t base = index & mask;
            uint32_t count = 1 << (8 - remaining);
            
            for (uint32_t i = 0; i < count; i++) {
                uint8_t idx = base + i;
                node->entries[idx].child_and_valid &= ~LPM_VALID_FLAG;
                node->entries[idx].next_hop = LPM_INVALID_NEXT_HOP;
            }
            
            if (trie->num_prefixes > 0) trie->num_prefixes--;
            return 0;
        }
        
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        
        if (depth + 8 == prefix_len) {
            node->entries[index].child_and_valid &= ~LPM_VALID_FLAG;
            node->entries[index].next_hop = LPM_INVALID_NEXT_HOP;
            if (trie->num_prefixes > 0) trie->num_prefixes--;
            return 0;
        }
        
        uint32_t child_idx = node->entries[index].child_and_valid & LPM_CHILD_MASK;
        if (child_idx == LPM_INVALID_INDEX) {
            return -1;  /* Prefix not found */
        }
        
        node_idx = child_idx;
        depth += 8;
    }
    
    if (trie->num_prefixes > 0) trie->num_prefixes--;
    return 0;
}

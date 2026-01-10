/*
 * IPv6 Wide Stride Optimization
 * Uses 16-16-16-8-8-8... stride pattern for IPv6
 * First 48 bits: 3 levels of 16-bit stride (covers common /48 allocations)
 * Remaining 80 bits: 10 levels of 8-bit stride
 * Total: 13 levels instead of 16 (18.75% reduction)
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/lpm.h"

/* Forward declarations of allocation functions */
extern uint32_t node_alloc(lpm_trie_t *trie);
extern uint32_t wide_node_alloc(lpm_trie_t *trie);

/* Optimized IPv6 lookup with wide stride */
uint32_t lpm_lookup_ipv6_wide(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr || !trie->use_ipv6_wide_stride) {
        return LPM_INVALID_NEXT_HOP;
    }
    
    uint32_t best_next_hop = trie->has_default_route ? trie->default_next_hop : LPM_INVALID_NEXT_HOP;
    uint32_t node_idx = trie->root_idx;
    uint8_t level = 0;
    
    /* First 3 levels: 16-bit wide stride (48 bits total) */
    bool is_wide_node = true;
    for (level = 0; level < LPM_IPV6_WIDE_STRIDE_LEVELS && level < 3; level++) {
        /* Note: For wide nodes, index 0 is valid (root), so we can't use the same check as regular nodes */
        if (!is_wide_node && node_idx == LPM_INVALID_INDEX) break;
        
        if (!is_wide_node) break;  /* Transitioned to 8-bit nodes */
        
        struct lpm_node_16 *wide_node = &((struct lpm_node_16 *)trie->wide_nodes_pool)[node_idx];
        
        /* Extract 16-bit index from address */
        uint16_t index = ((uint16_t)addr[level * 2] << 8) | addr[level * 2 + 1];
        
        struct lpm_entry *entry = &wide_node->entries[index];
        
        /* Update best match if this entry has a valid next hop */
        if (entry->child_and_valid & LPM_VALID_FLAG) {
            best_next_hop = entry->next_hop;
        }
        
        /* Get child node and check its type */
        uint32_t cv = entry->child_and_valid;
        uint32_t child_idx = cv & LPM_CHILD_MASK;
        /* Check if there's no child: child_idx is 0 AND no wide node flag
         * (Wide nodes can use index 0, but they'll have WIDE_NODE_FLAG set) */
        bool has_child = (child_idx != 0) || (cv & LPM_WIDE_NODE_FLAG);
        if (!has_child) {
            return best_next_hop;
        }
        
        is_wide_node = (cv & LPM_WIDE_NODE_FLAG) != 0;
        node_idx = child_idx;
    }
    
    /* Remaining levels: 8-bit stride (bytes 2-15 after first 16-bit level, 112 bits) */
    for (uint8_t byte_idx = 2; byte_idx < 16; byte_idx++) {
        if (node_idx == LPM_INVALID_INDEX) break;
        
        struct lpm_node *node = &((struct lpm_node *)trie->node_pool)[node_idx];
        uint8_t index = addr[byte_idx];
        
        struct lpm_entry *entry = &node->entries[index];
        
        if (entry->child_and_valid & LPM_VALID_FLAG) {
            best_next_hop = entry->next_hop;
        }
        
        uint32_t child_idx = entry->child_and_valid & LPM_CHILD_MASK;
        if (child_idx == LPM_INVALID_INDEX) {
            return best_next_hop;
        }
        
        node_idx = child_idx;
    }
    
    return best_next_hop;
}

/* Add prefix with wide stride support */
int lpm_add_ipv6_wide(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
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
                /* No WIDE_NODE_FLAG means regular 8-bit node */
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


/*
 * IPv4 DIR-24-8 Algorithm - Core Functions
 * Create, Add, Delete operations for IPv4 with DIR-24-8 table
 *
 * Uses 24-8 stride pattern for IPv4 with compact 4-byte entries:
 * - First 24 bits: Single table lookup (16.7M entries, 64MB)
 * - Last 8 bits: 8-bit table for longer routes (256 entries per group)
 * - Total: 1-2 memory accesses for any lookup!
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../include/lpm.h"
#include "../../include/internal.h"

/* Default number of tbl8 groups to allocate */
#define LPM_TBL8_DEFAULT_GROUPS 256
#define LPM_TBL8_GROUP_ENTRIES 256

/* ============================================================================
 * Trie Creation
 * ============================================================================ */

lpm_trie_t *lpm_create_ipv4_dir24(void)
{
    lpm_trie_t *t = (lpm_trie_t *)aligned_alloc(LPM_CACHE_LINE_SIZE, sizeof(lpm_trie_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(lpm_trie_t));
    
    t->max_depth = LPM_IPV4_MAX_DEPTH;
    t->use_huge_pages = false;
    t->default_next_hop = LPM_INVALID_NEXT_HOP;
    t->use_ipv6_wide_stride = false;
    t->use_ipv4_dir24 = true;  /* Enable DIR-24-8 */
    
    /* Allocate DIR-24 table (24-bit direct lookup table) - 64MB with 4-byte entries */
    size_t dir24_size = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry);
    t->dir24_table = (struct lpm_dir24_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, dir24_size);
    if (!t->dir24_table) {
        free(t);
        return NULL;
    }
    
    /* Initialize all DIR24 entries to invalid (0) */
    memset(t->dir24_table, 0, dir24_size);
    
    /* Allocate tbl8 groups (for /25-/32 prefixes) */
    t->tbl8_num_groups = LPM_TBL8_DEFAULT_GROUPS;
    t->tbl8_groups_used = 0;
    size_t tbl8_size = t->tbl8_num_groups * LPM_TBL8_GROUP_ENTRIES * sizeof(struct lpm_tbl8_entry);
    t->tbl8_groups = (struct lpm_tbl8_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, tbl8_size);
    if (!t->tbl8_groups) {
        free(t->dir24_table);
        free(t);
        return NULL;
    }
    memset(t->tbl8_groups, 0, tbl8_size);
    
    /* Allocate hot cache */
    size_t cache_size = LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry);
    t->hot_cache = (struct lpm_cache_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, cache_size);
    if (t->hot_cache) {
        memset(t->hot_cache, 0, cache_size);
    }
    
    return t;
}

/* ============================================================================
 * TBL8 Group Allocation
 * ============================================================================ */

static int32_t tbl8_group_alloc(lpm_trie_t *trie)
{
    if (trie->tbl8_groups_used >= trie->tbl8_num_groups) {
        /* Need to grow the tbl8 array */
        uint32_t new_groups = trie->tbl8_num_groups * 2;
        struct lpm_tbl8_entry *new_tbl8 = realloc(trie->tbl8_groups,
            new_groups * LPM_TBL8_GROUP_ENTRIES * sizeof(struct lpm_tbl8_entry));
        if (!new_tbl8) return -1;
        
        /* Initialize new groups */
        memset(&new_tbl8[trie->tbl8_num_groups * LPM_TBL8_GROUP_ENTRIES], 0,
               (new_groups - trie->tbl8_num_groups) * LPM_TBL8_GROUP_ENTRIES * sizeof(struct lpm_tbl8_entry));
        
        trie->tbl8_groups = new_tbl8;
        trie->tbl8_num_groups = new_groups;
    }
    
    return trie->tbl8_groups_used++;
}

/* ============================================================================
 * Add Prefix
 * ============================================================================ */

int lpm_add_ipv4_dir24(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie || !prefix || prefix_len > 32 || !trie->dir24_table) return -1;
    if (next_hop & 0xC0000000) return -1;  /* Next hop must fit in 30 bits */
    
    /* Handle default route */
    if (prefix_len == 0) {
        trie->has_default_route = true;
        trie->default_next_hop = next_hop;
        trie->num_prefixes++;
        return 0;
    }
    
    /* Routes up to /24: Store directly in DIR24 table */
    if (prefix_len <= 24) {
        uint32_t base_idx = ((uint32_t)prefix[0] << 16) | ((uint32_t)prefix[1] << 8);
        if (prefix_len >= 16) {
            base_idx |= prefix[2];
        }
        
        /* Mask off bits beyond prefix length */
        if (prefix_len < 24) {
            uint32_t mask = ~((1U << (24 - prefix_len)) - 1);
            base_idx &= mask;
        }
        
        /* Calculate how many entries to fill (prefix expansion) */
        uint32_t count = 1U << (24 - prefix_len);
        
        /* Fill all matching entries */
        for (uint32_t i = 0; i < count; i++) {
            uint32_t idx = base_idx + i;
            struct lpm_dir24_entry *entry = &trie->dir24_table[idx];
            
            /* Only overwrite if this is a more specific prefix or not extended */
            if (!(entry->data & LPM_DIR24_EXT_FLAG)) {
                entry->data = LPM_DIR24_VALID_FLAG | next_hop;
            }
        }
        
        trie->num_prefixes++;
        return 0;
    }
    
    /* Routes longer than /24: Need tbl8 */
    uint32_t dir24_idx = ((uint32_t)prefix[0] << 16) | ((uint32_t)prefix[1] << 8) | prefix[2];
    struct lpm_dir24_entry *dir_entry = &trie->dir24_table[dir24_idx];
    
    uint32_t tbl8_group;
    
    /* Check if we need to allocate a tbl8 group */
    if (!(dir_entry->data & LPM_DIR24_EXT_FLAG)) {
        /* Allocate new tbl8 group */
        int32_t new_group = tbl8_group_alloc(trie);
        if (new_group < 0) return -1;
        tbl8_group = new_group;
        
        /* If dir24 entry had a valid next hop, copy it to all tbl8 entries */
        struct lpm_tbl8_entry *tbl8 = &trie->tbl8_groups[tbl8_group * LPM_TBL8_GROUP_ENTRIES];
        if (dir_entry->data & LPM_DIR24_VALID_FLAG) {
            uint32_t parent_nh = dir_entry->data & LPM_DIR24_NH_MASK;
            for (int i = 0; i < LPM_TBL8_GROUP_ENTRIES; i++) {
                tbl8[i].data = LPM_DIR24_VALID_FLAG | parent_nh;
            }
        }
        
        /* Update dir24 entry to point to tbl8 group */
        dir_entry->data = LPM_DIR24_VALID_FLAG | LPM_DIR24_EXT_FLAG | tbl8_group;
    } else {
        tbl8_group = dir_entry->data & LPM_DIR24_NH_MASK;
    }
    
    /* Add route to tbl8 */
    struct lpm_tbl8_entry *tbl8 = &trie->tbl8_groups[tbl8_group * LPM_TBL8_GROUP_ENTRIES];
    uint8_t last_byte = prefix[3];
    uint8_t remaining_bits = prefix_len - 24;  /* 1-8 bits */
    
    if (remaining_bits == 8) {
        /* Exact /32 route */
        tbl8[last_byte].data = LPM_DIR24_VALID_FLAG | next_hop;
    } else {
        /* Partial last byte - expand to all matching entries */
        uint8_t mask = ~((1 << (8 - remaining_bits)) - 1);
        uint8_t base = last_byte & mask;
        uint32_t count = 1 << (8 - remaining_bits);
        
        for (uint32_t i = 0; i < count; i++) {
            uint8_t idx = base + i;
            tbl8[idx].data = LPM_DIR24_VALID_FLAG | next_hop;
        }
    }
    
    trie->num_prefixes++;
    return 0;
}

/* ============================================================================
 * Delete Prefix
 * ============================================================================ */

int lpm_delete_ipv4_dir24(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len)
{
    if (!trie || !prefix || prefix_len > 32 || !trie->dir24_table) return -1;
    
    /* Handle default route */
    if (prefix_len == 0) {
        trie->has_default_route = false;
        trie->default_next_hop = LPM_INVALID_NEXT_HOP;
        if (trie->num_prefixes > 0) trie->num_prefixes--;
        return 0;
    }
    
    /* Routes up to /24: Delete from DIR24 table */
    if (prefix_len <= 24) {
        uint32_t base_idx = ((uint32_t)prefix[0] << 16) | ((uint32_t)prefix[1] << 8);
        if (prefix_len >= 16) {
            base_idx |= prefix[2];
        }
        
        /* Mask off bits beyond prefix length */
        if (prefix_len < 24) {
            uint32_t mask = ~((1U << (24 - prefix_len)) - 1);
            base_idx &= mask;
        }
        
        /* Calculate how many entries to clear (prefix expansion) */
        uint32_t count = 1U << (24 - prefix_len);
        
        /* Clear all matching entries */
        for (uint32_t i = 0; i < count; i++) {
            uint32_t idx = base_idx + i;
            struct lpm_dir24_entry *entry = &trie->dir24_table[idx];
            
            /* Only clear if not extended to tbl8 */
            if (!(entry->data & LPM_DIR24_EXT_FLAG)) {
                entry->data = 0;  /* Clear both valid flag and next hop */
            }
        }
        
        if (trie->num_prefixes > 0) trie->num_prefixes--;
        return 0;
    }
    
    /* Routes longer than /24: Delete from tbl8 */
    uint32_t dir24_idx = ((uint32_t)prefix[0] << 16) | ((uint32_t)prefix[1] << 8) | prefix[2];
    struct lpm_dir24_entry *dir_entry = &trie->dir24_table[dir24_idx];
    
    /* Check if this entry uses tbl8 */
    if (!(dir_entry->data & LPM_DIR24_EXT_FLAG)) {
        /* No tbl8 group - just clear the dir24 entry */
        dir_entry->data = 0;
        if (trie->num_prefixes > 0) trie->num_prefixes--;
        return 0;
    }
    
    uint32_t tbl8_group = dir_entry->data & LPM_DIR24_NH_MASK;
    struct lpm_tbl8_entry *tbl8 = &trie->tbl8_groups[tbl8_group * LPM_TBL8_GROUP_ENTRIES];
    
    uint8_t last_byte = prefix[3];
    uint8_t remaining_bits = prefix_len - 24;  /* 1-8 bits */
    
    if (remaining_bits == 8) {
        /* Exact /32 route */
        tbl8[last_byte].data = 0;
    } else {
        /* Partial last byte - clear all matching entries */
        uint8_t mask = ~((1 << (8 - remaining_bits)) - 1);
        uint8_t base = last_byte & mask;
        uint32_t count = 1 << (8 - remaining_bits);
        
        for (uint32_t i = 0; i < count; i++) {
            uint8_t idx = base + i;
            tbl8[idx].data = 0;
        }
    }
    
    if (trie->num_prefixes > 0) trie->num_prefixes--;
    return 0;
}

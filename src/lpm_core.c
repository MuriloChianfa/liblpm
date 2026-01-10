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
#include <dynemit/core.h>

static const char *lpm_version = "liblpm 2.0.0";

/* ============================================================================
 * External ifunc-dispatched functions
 * These use GNU ifunc for zero-overhead dispatch resolved at program load time
 * ============================================================================ */

extern uint32_t lpm_lookup_single_simd(const struct lpm_trie *trie, const uint8_t *addr);
extern void lpm_lookup_batch_simd(const struct lpm_trie *trie, const uint8_t **addrs, 
                                  uint32_t *next_hops, size_t count);

/* ============================================================================
 * Hot Cache - Ultra Fast Hash Lookup
 * ============================================================================ */

static inline uint64_t fast_hash(const uint8_t *addr, uint8_t len)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (uint8_t i = 0; i < len; i++) {
        h ^= addr[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static inline uint32_t cache_probe(const lpm_trie_t *trie, const uint8_t *addr, 
                                   uint8_t len, bool *hit)
{
    if (!trie->hot_cache) {
        *hit = false;
        return 0;
    }
    uint64_t h = fast_hash(addr, len);
    uint32_t idx = h & (LPM_HOT_CACHE_SIZE - 1);
    const struct lpm_cache_entry *e = &trie->hot_cache[idx];
    *hit = (e->addr_hash == h);
    return e->next_hop;
}

static inline void cache_store(lpm_trie_t *trie, const uint8_t *addr, 
                              uint8_t len, uint32_t next_hop)
{
    if (!trie->hot_cache) return;
    uint64_t h = fast_hash(addr, len);
    uint32_t idx = h & (LPM_HOT_CACHE_SIZE - 1);
    trie->hot_cache[idx].addr_hash = h;
    trie->hot_cache[idx].next_hop = next_hop;
}

/* ============================================================================
 * Direct Table - Instant 16-bit Lookup for IPv4
 * ============================================================================ */

static void direct_table_update(lpm_trie_t *trie, const uint8_t *prefix, 
                               uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie->direct_table || trie->max_depth != LPM_IPV4_MAX_DEPTH) return;
    if (prefix_len > 16) return;  /* Only handle prefixes <= 16 bits */
    
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
            /* Keep node_idx pointing to root - we still need to traverse for longer prefixes */
        }
    }
}

/* ============================================================================
 * Memory Allocation with Huge Pages
 * ============================================================================ */

static void *alloc_huge(size_t size, size_t *actual)
{
    size_t aligned = (size + LPM_HUGE_PAGE_SIZE - 1) & ~(LPM_HUGE_PAGE_SIZE - 1);
    
#ifdef __linux__
    void *p = mmap(NULL, aligned, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (p != MAP_FAILED) {
        *actual = aligned;
        return p;
    }
    
    p = mmap(NULL, aligned, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != MAP_FAILED) {
        madvise(p, aligned, MADV_HUGEPAGE);
        *actual = aligned;
        return p;
    }
#endif
    
    void *p2 = aligned_alloc(LPM_CACHE_LINE_SIZE, aligned);
    if (p2) *actual = aligned;
    return p2;
}

static void free_huge(void *p, size_t size)
{
#ifdef __linux__
    if (p) munmap(p, size);
#else
    free(p);
#endif
}

/* ============================================================================
 * Node Pool
 * ============================================================================ */

static int pool_grow(lpm_trie_t *trie)
{
    uint32_t new_cap = trie->pool_capacity * LPM_POOL_GROWTH_FACTOR;
    size_t new_size = new_cap * sizeof(struct lpm_node);
    
    /* Always use malloc/realloc for simplicity and compatibility */
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
 * Trie Creation/Destruction
 * ============================================================================ */

lpm_trie_t *lpm_create(uint8_t max_depth)
{
    if (max_depth != LPM_IPV4_MAX_DEPTH && max_depth != LPM_IPV6_MAX_DEPTH) {
        return NULL;
    }
    
    lpm_trie_t *t = (lpm_trie_t *)aligned_alloc(LPM_CACHE_LINE_SIZE, sizeof(lpm_trie_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(lpm_trie_t));
    
    t->max_depth = max_depth;
    t->use_huge_pages = false;  /* Disable for stability */
    t->default_next_hop = LPM_INVALID_NEXT_HOP;
    
    /* Enable optimizations based on IP version */
    t->use_ipv6_wide_stride = (max_depth == LPM_IPV6_MAX_DEPTH);
    t->use_ipv4_dir24 = false;  /* Disabled: 8-bit stride is faster for diverse data */
    
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
    
    /* For IPv6, allocate wide stride pool and use wide root */
    if (t->use_ipv6_wide_stride) {
        size_t wide_pool_size = 16 * sizeof(struct lpm_node_16);
        t->wide_nodes_pool = malloc(wide_pool_size);
        if (!t->wide_nodes_pool) {
            free(t->node_pool);
            free(t);
            return NULL;
        }
        t->wide_pool_capacity = 16;
        t->wide_pool_used = 0;
        
        t->root_idx = wide_node_alloc(t);
    } else if (t->use_ipv4_dir24) {
        /* DIR-24-8: root is implicit in DIR24 table, but we still allocate root node for consistency */
        t->root_idx = 0;  /* DIR-24-8 doesn't need a traditional root */
    } else {
        t->root_idx = node_alloc(t);
    }
    
    /* Note: For wide nodes, index 0 is valid, so we only check for creation failure differently */
    if (!t->use_ipv6_wide_stride && !t->use_ipv4_dir24 && t->root_idx == LPM_INVALID_INDEX) {
        if (t->wide_nodes_pool) free(t->wide_nodes_pool);
        free(t->node_pool);
        free(t);
        return NULL;
    }
    
    /* DIR-24-8 table for IPv4 - Note: use lpm_create_ipv4_dir24() instead for DIR-24-8 */
    /* This code path is kept for potential future use but currently use_ipv4_dir24 is false */
    if (max_depth == LPM_IPV4_MAX_DEPTH && t->use_ipv4_dir24) {
        size_t dir24_size = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry);
        printf("Allocating DIR-24-8 table: %.2f MB\n", dir24_size / (1024.0 * 1024.0));
        
        t->dir24_table = (struct lpm_dir24_entry *)malloc(dir24_size);
        if (!t->dir24_table) {
            printf("Warning: Failed to allocate DIR-24-8 table, falling back to standard\n");
            t->use_ipv4_dir24 = false;
        } else {
            /* Initialize all entries to 0 (invalid) */
            memset(t->dir24_table, 0, dir24_size);
        }
    }
    
    /* Legacy direct table for IPv4 (if DIR-24-8 not used) */
    if (max_depth == LPM_IPV4_MAX_DEPTH && !t->use_ipv4_dir24) {
        size_t dt_size = LPM_DIRECT_SIZE * sizeof(struct lpm_direct_entry);
        t->direct_table = (struct lpm_direct_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, dt_size);
        if (t->direct_table) {
            for (uint32_t i = 0; i < LPM_DIRECT_SIZE; i++) {
                t->direct_table[i].next_hop = LPM_INVALID_NEXT_HOP;
                t->direct_table[i].node_idx = t->root_idx;
                t->direct_table[i].prefix_len = 0;
            }
        }
    }
    
    /* Hot cache */
    size_t cache_size = LPM_HOT_CACHE_SIZE * sizeof(struct lpm_cache_entry);
    t->hot_cache = (struct lpm_cache_entry *)aligned_alloc(LPM_CACHE_LINE_SIZE, cache_size);
    if (t->hot_cache) {
        memset(t->hot_cache, 0, cache_size);
    }
    
    /* Note: Function dispatch is handled by ifunc - no setup needed */
    
    return t;
}

/* Default tbl8 groups for DIR-24-8 */
#define LPM_TBL8_DEFAULT_GROUPS 256
#define LPM_TBL8_GROUP_ENTRIES 256

/* Create IPv4 trie with DIR-24-8 */
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
    
    /* Note: Function dispatch is handled by ifunc - no setup needed */
    
    return t;
}

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
 * Add/Delete Prefixes
 * ============================================================================ */

/* Forward declare IPv6 wide stride functions */
extern int lpm_add_ipv6_wide(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
extern uint32_t lpm_lookup_ipv6_wide(const lpm_trie_t *trie, const uint8_t addr[16]);

/* Forward declare IPv4 DIR-24-8 functions */
extern int lpm_add_ipv4_dir24(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
extern uint32_t lpm_lookup_ipv4_dir24(const lpm_trie_t *trie, const uint8_t addr[4]);
extern void lpm_lookup_batch_ipv4_dir24(const lpm_trie_t *trie, const uint8_t (*addrs)[4],
                                         uint32_t *next_hops, size_t count);
extern void lpm_lookup_batch_ipv4_dir24_ptrs(const lpm_trie_t *trie, const uint8_t **addrs,
                                              uint32_t *next_hops, size_t count);

int lpm_add(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop)
{
    if (!trie || !prefix || prefix_len > trie->max_depth) return -1;
    
    /* Use DIR-24-8 for IPv4 */
    if (trie->use_ipv4_dir24 && trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        return lpm_add_ipv4_dir24(trie, prefix, prefix_len, next_hop);
    }
    
    /* Use wide stride for IPv6 */
    if (trie->use_ipv6_wide_stride && trie->max_depth == LPM_IPV6_MAX_DEPTH) {
        return lpm_add_ipv6_wide(trie, prefix, prefix_len, next_hop);
    }
    
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
            
            /* Update direct table for short prefixes */
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
        
        /* Update direct table for short prefixes */
        if (prefix_len <= 16) {
            direct_table_update(trie, prefix, prefix_len, next_hop);
        }
    }
    
    trie->num_prefixes++;
    return 0;
}

int lpm_delete(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len)
{
    if (!trie || !prefix || prefix_len > trie->max_depth) return -1;
    
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

/* ============================================================================
 * Public API
 * ============================================================================ */

uint32_t lpm_lookup(const lpm_trie_t *trie, const uint8_t *addr)
{
    if (!trie || !addr) return LPM_INVALID_NEXT_HOP;
    
    /* Use DIR-24-8 for IPv4 */
    if (trie->use_ipv4_dir24 && trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        return lpm_lookup_ipv4_dir24(trie, addr);
    }
    
    /* Use wide stride for IPv6 */
    if (trie->use_ipv6_wide_stride && trie->max_depth == LPM_IPV6_MAX_DEPTH) {
        return lpm_lookup_ipv6_wide(trie, addr);
    }
    
    return lpm_lookup_single_simd(trie, addr);
}

uint32_t lpm_lookup_ipv4(const lpm_trie_t *trie, uint32_t addr)
{
    if (!trie || trie->max_depth != LPM_IPV4_MAX_DEPTH) return LPM_INVALID_NEXT_HOP;
    
    uint8_t bytes[4] = {
        (addr >> 24) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    return lpm_lookup_single_simd(trie, bytes);
}

uint32_t lpm_lookup_ipv6(const lpm_trie_t *trie, const uint8_t addr[16])
{
    if (!trie || !addr || trie->max_depth != LPM_IPV6_MAX_DEPTH) return LPM_INVALID_NEXT_HOP;
    
    /* Use wide stride lookup for IPv6 */
    if (trie->use_ipv6_wide_stride) {
        return lpm_lookup_ipv6_wide(trie, addr);
    }
    
    return lpm_lookup_single_simd(trie, addr);
}

void lpm_lookup_batch(const lpm_trie_t *trie, const uint8_t **addrs, 
                      uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || !count) return;
    
    /* Use SIMD-optimized DIR-24-8 batch for IPv4 when enabled */
    if (trie->use_ipv4_dir24 && trie->max_depth == LPM_IPV4_MAX_DEPTH) {
        lpm_lookup_batch_ipv4_dir24_ptrs(trie, addrs, next_hops, count);
        return;
    }
    
    lpm_lookup_batch_simd(trie, addrs, next_hops, count);
}

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
        size_t tbl8_mem = trie->tbl8_groups_used * LPM_TBL8_GROUP_ENTRIES * sizeof(struct lpm_tbl8_entry);
        printf("  Memory: DIR24=%.2f MB, TBL8=%.2f MB, Total=%.2f MB\n",
               dir24_mem / (1024.0 * 1024.0),
               tbl8_mem / (1024.0 * 1024.0),
               (dir24_mem + tbl8_mem) / (1024.0 * 1024.0));
    } else if (trie->use_ipv6_wide_stride) {
        printf("  Stride: 16-16-16-8-8-8... (IPv6 wide stride optimization)\n");
        printf("  Prefixes: %llu\n", (unsigned long long)trie->num_prefixes);
        printf("  8-bit nodes: %llu\n", (unsigned long long)trie->num_nodes);
        printf("  16-bit nodes: %llu\n", (unsigned long long)trie->num_wide_nodes);
        
        size_t wide_mem = trie->num_wide_nodes * sizeof(struct lpm_node_16);
        printf("  Total memory: %.2f MB (8-bit: %.2f MB, 16-bit: %.2f MB)\n", 
               (used_mem + wide_mem) / (1024.0 * 1024.0),
               used_mem / (1024.0 * 1024.0),
               wide_mem / (1024.0 * 1024.0));
    } else {
        printf("  Stride: 8-bit (256 entries/node)\n");
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

#ifndef _LPM_H_
#define _LPM_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Architecture detection - x86 only */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define LPM_X86_ARCH 1
#endif

/* 
 * Note: SIMD feature detection is handled at runtime via libdynemit.
 * All SIMD variants (SSE2, AVX2, AVX512) are compiled and the optimal
 * implementation is selected at program load time using GNU ifunc.
 */

/* ============================================================================
 * SPEED-OPTIMIZED CONFIGURATION
 * ============================================================================ */

#define LPM_CACHE_LINE_SIZE 64
#define LPM_ALIGN_CACHE __attribute__((aligned(LPM_CACHE_LINE_SIZE)))

/* 8-bit stride for MAXIMUM SPEED (256 entries/node) */
#define LPM_STRIDE_BITS_8 8
#define LPM_STRIDE_SIZE_8 256

/* 16-bit stride for IPv6 optimization (65536 entries/node) */
#define LPM_STRIDE_BITS_16 16
#define LPM_STRIDE_SIZE_16 65536

/* IPv6 Variable Stride Configuration: 16-8-8-8...
 * First 16 bits: 1 level of 16-bit stride (512KB)
 * Remaining 112 bits: 14 levels of 8-bit stride
 * Total: 15 levels instead of 16 (6.25% reduction)
 * Memory: ~100MB instead of 8GB!
 */
#define LPM_IPV6_WIDE_STRIDE_LEVELS 1

/* IPv4 DIR-24-8 Configuration
 * First 24 bits: Single 24-bit lookup (16.7M entries)
 * Last 8 bits: 8-bit lookup if needed (256 entries)
 * Total: 2 levels maximum, 1 level for most routes!
 */
#define LPM_IPV4_DIR24_BITS 24
#define LPM_IPV4_DIR24_SIZE (1 << LPM_IPV4_DIR24_BITS)  /* 16,777,216 entries */

/* Legacy compatibility */
#define LPM_STRIDE_BITS LPM_STRIDE_BITS_8
#define LPM_STRIDE_SIZE LPM_STRIDE_SIZE_8

#define LPM_INVALID_NEXT_HOP ((uint32_t)-1)
#define LPM_INVALID_INDEX 0

/* Validity flag in high bit of child index */
#define LPM_VALID_FLAG      (1U << 31)
#define LPM_WIDE_NODE_FLAG  (1U << 30)  /* Indicates 16-bit node (bit 30) */
#define LPM_CHILD_MASK      0x3FFFFFFF  /* Lower 30 bits for node index */

#define LPM_IPV4_MAX_DEPTH 32
#define LPM_IPV6_MAX_DEPTH 128

#define LPM_INITIAL_POOL_SIZE 4096
#define LPM_POOL_GROWTH_FACTOR 2

/* Direct table: instant lookup for first 16 bits */
#define LPM_DIRECT_BITS 16
#define LPM_DIRECT_SIZE (1 << LPM_DIRECT_BITS)

/* Hot cache for repeated lookups */
#define LPM_HOT_CACHE_SIZE 8192

/* Huge pages */
#define LPM_HUGE_PAGE_SIZE (2 * 1024 * 1024)

/* ============================================================================
 * Data Structures - Optimized for Speed
 * ============================================================================ */

typedef struct lpm_trie lpm_trie_t;
typedef struct lpm_node lpm_node_t;

/* Interleaved entry: child + next_hop together for cache locality */
struct lpm_entry {
    uint32_t child_and_valid;  /* bit 31 = valid, bits 0-30 = child idx */
    uint32_t next_hop;
} __attribute__((packed));

/* 8-bit stride node: 256 entries * 8 bytes = 2048 bytes */
struct lpm_node_8 {
    struct lpm_entry entries[LPM_STRIDE_SIZE_8];
} LPM_ALIGN_CACHE;

/* 16-bit stride node: 65536 entries * 8 bytes = 512 KB */
struct lpm_node_16 {
    struct lpm_entry entries[LPM_STRIDE_SIZE_16];
} LPM_ALIGN_CACHE;

/* 24-bit DIR table for IPv4: 16.7M entries * 4 bytes = 64 MB
 * Format: bit 31 = valid, bit 30 = tbl8 extended flag, bits 0-29 = next_hop or tbl8_idx
 * This compact 4-byte format matches DPDK's design for optimal cache efficiency.
 */
struct lpm_dir24_entry {
    uint32_t data;  /* All info packed in one 32-bit word */
} __attribute__((packed));

/* TBL8 entry: 256 entries per group, each 4 bytes */
struct lpm_tbl8_entry {
    uint32_t data;  /* bit 31 = valid, bits 0-30 = next_hop */
} __attribute__((packed));

/* DIR24 entry flags */
#define LPM_DIR24_VALID_FLAG    (1U << 31)
#define LPM_DIR24_EXT_FLAG      (1U << 30)  /* Extended to tbl8 */
#define LPM_DIR24_NH_MASK       0x3FFFFFFF  /* Lower 30 bits for next_hop/tbl8_idx */

/* Legacy compatibility - keep original struct as lpm_node */
struct lpm_node {
    struct lpm_entry entries[LPM_STRIDE_SIZE_8];
} LPM_ALIGN_CACHE;

/* Direct table entry */
struct lpm_direct_entry {
    uint32_t next_hop;
    uint32_t node_idx;
    uint8_t prefix_len;
    uint8_t _pad[3];
};

/* Hot cache entry */
struct lpm_cache_entry {
    uint64_t addr_hash;
    uint32_t next_hop;
    uint32_t _pad;
};

/* Main trie structure */
struct lpm_trie {
    void *node_pool;  /* Changed to void* to support mixed node types */
    uint32_t pool_capacity;
    uint32_t pool_used;
    
    struct lpm_direct_entry *direct_table;
    struct lpm_cache_entry *hot_cache;
    
    /* IPv6 wide stride nodes (16-bit) - separate pool */
    void *wide_nodes_pool;
    uint32_t wide_pool_capacity;
    uint32_t wide_pool_used;
    
    /* IPv4 DIR-24-8 table (compact 4-byte entries) */
    struct lpm_dir24_entry *dir24_table;
    struct lpm_tbl8_entry *tbl8_groups;  /* Array of 256-entry groups */
    uint32_t tbl8_num_groups;
    uint32_t tbl8_groups_used;
    
    uint32_t root_idx;
    
    uint64_t num_prefixes;
    uint64_t num_nodes;
    uint64_t num_wide_nodes;
    uint64_t cache_hits;
    uint64_t cache_misses;
    
    uint8_t max_depth;
    bool has_default_route;
    bool use_huge_pages;
    bool use_ipv6_wide_stride;  /* Enable 16-bit stride for IPv6 */
    bool use_ipv4_dir24;         /* Enable DIR-24-8 for IPv4 */
    
    uint32_t default_next_hop;
    
    void *huge_page_base;
    size_t huge_page_size;
    
    /* Note: Function pointers removed - using ifunc for dispatch */
} LPM_ALIGN_CACHE;

/* ============================================================================
 * API
 * ============================================================================ */

lpm_trie_t *lpm_create(uint8_t max_depth);
lpm_trie_t *lpm_create_ipv4_dir24(void);  /* Create IPv4 trie with DIR-24-8 optimization */
void lpm_destroy(lpm_trie_t *trie);

int lpm_add(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
int lpm_delete(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len);

uint32_t lpm_lookup(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv4(const lpm_trie_t *trie, uint32_t addr);
uint32_t lpm_lookup_ipv6(const lpm_trie_t *trie, const uint8_t addr[16]);

void lpm_lookup_batch(const lpm_trie_t *trie, const uint8_t **addrs, 
                      uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4(const lpm_trie_t *trie, const uint32_t *addrs, 
                           uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6(const lpm_trie_t *trie, const uint8_t (*addrs)[16], 
                           uint32_t *next_hops, size_t count);

const char *lpm_get_version(void);
void lpm_print_stats(const lpm_trie_t *trie);

#ifdef __cplusplus
}
#endif
#endif /* _LPM_H_ */

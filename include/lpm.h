#ifndef _LPM_H_
#define _LPM_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CPU feature detection macros */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define LPM_X86_ARCH 1
    
    #ifdef __SSE__
        #define LPM_HAVE_SSE 1
    #endif
    #ifdef __SSE2__
        #define LPM_HAVE_SSE2 1
    #endif
    #ifdef __SSE3__
        #define LPM_HAVE_SSE3 1
    #endif
    #ifdef __SSE4_1__
        #define LPM_HAVE_SSE4_1 1
    #endif
    #ifdef __SSE4_2__
        #define LPM_HAVE_SSE4_2 1
    #endif
    #ifdef __AVX__
        #define LPM_HAVE_AVX 1
    #endif
    #ifdef __AVX2__
        #define LPM_HAVE_AVX2 1
    #endif
    #ifdef __AVX512F__
        #define LPM_HAVE_AVX512F 1
    #endif
    #ifdef __AVX512VL__
        #define LPM_HAVE_AVX512VL 1
    #endif
    #ifdef __AVX512DQ__
        #define LPM_HAVE_AVX512DQ 1
    #endif
    #ifdef __AVX512BW__
        #define LPM_HAVE_AVX512BW 1
    #endif
#endif

/* Alignment macros */
#define LPM_CACHE_LINE_SIZE 64
#define LPM_ALIGN_CACHE __attribute__((aligned(LPM_CACHE_LINE_SIZE)))

/* Constants */
#define LPM_STRIDE_BITS 8
#define LPM_STRIDE_SIZE (1 << LPM_STRIDE_BITS)
#define LPM_INVALID_NEXT_HOP ((uint32_t)-1)
#define LPM_MAX_RESULTS 128  /* Maximum number of matched prefixes to return */

/* IPv4/IPv6 constants */
#define LPM_IPV4_MAX_DEPTH 32
#define LPM_IPV6_MAX_DEPTH 128

/* Batch sizes for vectorized lookups */
#define LPM_BATCH_SIZE_SSE 4
#define LPM_BATCH_SIZE_AVX 8
#define LPM_BATCH_SIZE_AVX512 16

/* Forward declarations */
typedef struct lpm_trie lpm_trie_t;
typedef struct lpm_node lpm_node_t;
typedef struct lpm_prefix lpm_prefix_t;
typedef struct lpm_result lpm_result_t;

/* Prefix information structure */
struct lpm_prefix {
    uint8_t prefix[16];      /* Prefix bytes (supports up to IPv6) */
    uint8_t prefix_len;      /* Prefix length in bits */
    uint8_t _pad[3];         /* Padding for alignment */
    void *user_data;         /* Optional user data associated with prefix */
} __attribute__((packed));

/* Result structure for matched prefixes */
struct lpm_result {
    struct lpm_prefix *prefixes;  /* Array of matched prefixes */
    uint32_t count;               /* Number of matched prefixes */
    uint32_t capacity;            /* Capacity of the array */
};

/* LPM node structure - optimized for 8-bit stride */
struct lpm_node {
    /* Child pointers array - indexed by next 8 bits */
    struct lpm_node *children[LPM_STRIDE_SIZE] LPM_ALIGN_CACHE;
    
    /* Next hop values for prefixes ending at this node */
    uint32_t next_hops[LPM_STRIDE_SIZE];
    /* Prefix information for each entry */
    struct lpm_prefix *prefix_info[LPM_STRIDE_SIZE];
    
    /* Valid prefix bitmap - indicates which entries have valid prefixes */
    uint32_t valid_bitmap[LPM_STRIDE_SIZE / 32];
    
    /* Depth of this node in the trie */
    uint8_t depth;
    
    /* Padding for cache alignment */
    uint8_t _pad[7];
} LPM_ALIGN_CACHE;

/* Main LPM trie structure */
struct lpm_trie {
    /* Root node of the trie */
    struct lpm_node *root;
    
    /* Memory allocator function pointers */
    void *(*alloc)(size_t size);
    void (*free)(void *ptr);
    
    /* Statistics */
    uint64_t num_prefixes;
    uint64_t num_nodes;
    
    /* Configuration */
    uint8_t max_depth;      /* 32 for IPv4, 128 for IPv6 */
    uint8_t stride_bits;    /* Usually 8 */
    
    /* CPU features detected at runtime */
    uint32_t cpu_features;
    
    /* Function pointers for optimized lookup paths */
    uint32_t (*lookup_single)(const struct lpm_trie *trie, const uint8_t *addr);
    void (*lookup_batch)(const struct lpm_trie *trie, const uint8_t **addrs, 
                        uint32_t *next_hops, size_t count);
    struct lpm_result* (*lookup_all)(const struct lpm_trie *trie, const uint8_t *addr);
    void (*lookup_all_batch)(const struct lpm_trie *trie, const uint8_t **addrs, 
                            struct lpm_result **results, size_t count);
    
    /* Default route (0.0.0.0/0) - stored separately to avoid conflicts */
    struct lpm_prefix *default_route;
} LPM_ALIGN_CACHE;

/* CPU feature flags */
enum lpm_cpu_features {
    LPM_CPU_SSE    = 1 << 0,
    LPM_CPU_SSE2   = 1 << 1,
    LPM_CPU_SSE3   = 1 << 2,
    LPM_CPU_SSE4_1 = 1 << 3,
    LPM_CPU_SSE4_2 = 1 << 4,
    LPM_CPU_AVX    = 1 << 5,
    LPM_CPU_AVX2   = 1 << 6,
    LPM_CPU_AVX512F = 1 << 7,
    LPM_CPU_AVX512VL = 1 << 8,
    LPM_CPU_AVX512DQ = 1 << 9,
    LPM_CPU_AVX512BW = 1 << 10,
};

/* API Functions */

/* Initialize and destroy LPM trie */
lpm_trie_t *lpm_create(uint8_t max_depth);
lpm_trie_t *lpm_create_custom(uint8_t max_depth, 
                              void *(*alloc_fn)(size_t), 
                              void (*free_fn)(void *));
void lpm_destroy(lpm_trie_t *trie);

/* Add/delete prefixes */
int lpm_add(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
int lpm_add_prefix(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, void *user_data);
int lpm_delete(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len);

/* Single lookup functions */
/* Single lookup functions - returns longest match only */
uint32_t lpm_lookup(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv4(const lpm_trie_t *trie, uint32_t addr);
uint32_t lpm_lookup_ipv6(const lpm_trie_t *trie, const uint8_t addr[16]);

/* Lookup all matching prefixes */
lpm_result_t *lpm_lookup_all(const lpm_trie_t *trie, const uint8_t *addr);
lpm_result_t *lpm_lookup_all_ipv4(const lpm_trie_t *trie, uint32_t addr);
lpm_result_t *lpm_lookup_all_ipv6(const lpm_trie_t *trie, const uint8_t addr[16]);

/* Batch lookup functions */
void lpm_lookup_batch(const lpm_trie_t *trie, const uint8_t **addrs, 
                      uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv4(const lpm_trie_t *trie, const uint32_t *addrs, 
                           uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6(const lpm_trie_t *trie, const uint8_t (*addrs)[16], 
                           uint32_t *next_hops, size_t count);

/* Batch lookup for all matching prefixes */
void lpm_lookup_all_batch(const lpm_trie_t *trie, const uint8_t **addrs, 
                         lpm_result_t **results, size_t count);

/* Result management */
lpm_result_t *lpm_result_create(uint32_t capacity);
void lpm_result_destroy(lpm_result_t *result);
void lpm_result_clear(lpm_result_t *result);
int lpm_result_add(lpm_result_t *result, const lpm_prefix_t *prefix);

/* Utility functions */
const char *lpm_get_version(void);
void lpm_print_stats(const lpm_trie_t *trie);

#ifdef __cplusplus
}
#endif
#endif /* _LPM_H_ */

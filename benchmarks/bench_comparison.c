#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdbool.h>

/* liblpm Header */
#include "../include/lpm.h"

/* DPDK Headers - conditional */
#ifdef HAVE_DPDK
#include <rte_eal.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_errno.h>
#include <rte_version.h>
#endif

#define MILLION 1000000
#define NUM_PREFIXES 10000
#define NUM_LOOKUPS 1000000
#define BATCH_SIZE 256

/* DPDK Configuration */
#ifdef HAVE_DPDK
#define DPDK_LPM_MAX_RULES     100000
#define DPDK_LPM_TBL8_NUM_GROUPS  256
#define DPDK_LPM6_MAX_RULES    100000
#define DPDK_LPM6_NUMBER_TBL8S 65536
#endif

/* Colors for output */
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

/* Test results structure */
typedef struct {
    double lookups_per_sec;
    double ns_per_lookup;
    size_t memory_bytes;
    const char *library_name;
} benchmark_result_t;

/* Generate random IPv4 address */
static void generate_random_ipv4(uint8_t addr[4])
{
    for (int i = 0; i < 4; i++) {
        addr[i] = rand() % 256;
    }
}

/* Generate random IPv6 address */
static void generate_random_ipv6(uint8_t addr[16])
{
    for (int i = 0; i < 16; i++) {
        addr[i] = rand() % 256;
    }
}

/* Convert uint8_t[4] to uint32_t in network byte order */
static inline uint32_t ipv4_to_uint32(const uint8_t addr[4])
{
    return (addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3];
}

/* Measure time difference in microseconds */
static double time_diff_us(const struct timespec *start, const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000.0 + 
           (end->tv_nsec - start->tv_nsec) / 1000.0;
}

#ifdef HAVE_DPDK
/* Print comparison results for two implementations (used for IPv6 with DPDK) */
static void print_comparison(const char *test_name, 
                            const benchmark_result_t *liblpm_result,
                            const benchmark_result_t *dpdk_result)
{
    printf("\n%s=== %s ===%s\n", COLOR_BOLD, test_name, COLOR_RESET);
    printf("\n%-15s | %-15s | %-12s | %-12s\n", 
           "Library", "Lookups/sec", "ns/lookup", "Memory (MB)");
    printf("----------------|-----------------|--------------|-------------\n");
    
    /* liblpm results */
    printf("%-15s | %s%9.2f M%s     | %s%8.2f%s     | %8.2f\n",
           liblpm_result->library_name,
           COLOR_CYAN, liblpm_result->lookups_per_sec / MILLION, COLOR_RESET,
           COLOR_CYAN, liblpm_result->ns_per_lookup, COLOR_RESET,
           liblpm_result->memory_bytes / (1024.0 * 1024.0));
    
    /* DPDK results */
    printf("%-15s | %s%9.2f M%s     | %s%8.2f%s     | %8.2f\n",
           dpdk_result->library_name,
           COLOR_CYAN, dpdk_result->lookups_per_sec / MILLION, COLOR_RESET,
           COLOR_CYAN, dpdk_result->ns_per_lookup, COLOR_RESET,
           dpdk_result->memory_bytes / (1024.0 * 1024.0));
    
    /* Determine winner */
    double speedup = liblpm_result->lookups_per_sec / dpdk_result->lookups_per_sec;
    const char *winner;
    double percent;
    const char *color;
    
    if (speedup > 1.05) {
        winner = liblpm_result->library_name;
        percent = (speedup - 1.0) * 100.0;
        color = COLOR_GREEN;
    } else if (speedup < 0.95) {
        winner = dpdk_result->library_name;
        percent = (1.0/speedup - 1.0) * 100.0;
        color = COLOR_GREEN;
    } else {
        printf("\n%sPerformance: Comparable (within 5%%)%s\n", COLOR_YELLOW, COLOR_RESET);
        return;
    }
    
    printf("\n%sWinner: %s (%.1f%% faster)%s\n", color, winner, percent, COLOR_RESET);
}
#endif /* HAVE_DPDK */

/* Print comparison results for multiple implementations (2 or 3) */
static void print_comparison_multi(const char *test_name,
                                   const benchmark_result_t *results,
                                   size_t num_results)
{
    printf("\n%s=== %s ===%s\n", COLOR_BOLD, test_name, COLOR_RESET);
    printf("\n%-15s | %-15s | %-12s | %-12s\n", 
           "Library", "Lookups/sec", "ns/lookup", "Memory (MB)");
    printf("----------------|-----------------|--------------|-------------\n");
    
    /* Print all results */
    for (size_t i = 0; i < num_results; i++) {
        printf("%-15s | %s%9.2f M%s     | %s%8.2f%s     | %8.2f\n",
               results[i].library_name,
               COLOR_CYAN, results[i].lookups_per_sec / MILLION, COLOR_RESET,
               COLOR_CYAN, results[i].ns_per_lookup, COLOR_RESET,
               results[i].memory_bytes / (1024.0 * 1024.0));
    }
    
    /* Find the best performer */
    double best_perf = results[0].lookups_per_sec;
    const char *winner = results[0].library_name;
    
    for (size_t i = 1; i < num_results; i++) {
        if (results[i].lookups_per_sec > best_perf) {
            best_perf = results[i].lookups_per_sec;
            winner = results[i].library_name;
        }
    }
    
    /* Calculate speedup factors */
    printf("\n%sBest Performer: %s%s\n", COLOR_GREEN, winner, COLOR_RESET);
    if (num_results > 1) {
        printf("Speedup factors:\n");
        for (size_t i = 0; i < num_results; i++) {
            printf("  vs %-13s %.2fx\n", results[i].library_name, 
                   best_perf / results[i].lookups_per_sec);
        }
    }
}

/* ============================================================================
 * IPv4 Benchmarks
 * ============================================================================ */

static benchmark_result_t benchmark_ipv4_liblpm_pure_single(void)
{
    benchmark_result_t result = {0};
    result.library_name = "liblpm Pure";
    
    /* Use pure LPM trie (standard multibit trie) for comparison */
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25); // 8 to 32
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses */
    uint8_t (*test_addrs)[4] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        generate_random_ipv4(test_addrs[i]);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 1000; i++) {
        lpm_lookup(trie, test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        volatile uint32_t result_nh = lpm_lookup(trie, test_addrs[i]);
        (void)result_nh;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    result.lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    
    /* Memory: 8-bit stride nodes (2KB each) */
    result.memory_bytes = trie->num_nodes * sizeof(lpm_node_t);
    
    free(test_addrs);
    lpm_destroy(trie);
    
    return result;
}

static benchmark_result_t benchmark_ipv4_liblpm_single(void)
{
    benchmark_result_t result = {0};
    result.library_name = "liblpm DIR24";
    
    /* Use DIR-24-8 optimized trie for fair comparison with DPDK LPM */
    lpm_trie_t *trie = lpm_create_ipv4_dir24();
    assert(trie != NULL);
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25); // 8 to 32
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses */
    uint8_t (*test_addrs)[4] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        generate_random_ipv4(test_addrs[i]);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 1000; i++) {
        lpm_lookup(trie, test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        volatile uint32_t result_nh = lpm_lookup(trie, test_addrs[i]);
        (void)result_nh;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    result.lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    
    /* Memory: DIR24 table (16.7M * 4 bytes = 64MB) + tbl8 groups */
    size_t dir24_mem = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry);
    size_t tbl8_mem = trie->tbl8_groups_used * 256 * sizeof(struct lpm_tbl8_entry);
    result.memory_bytes = dir24_mem + tbl8_mem;
    
    free(test_addrs);
    lpm_destroy(trie);
    
    return result;
}

#ifdef HAVE_DPDK
static benchmark_result_t benchmark_ipv4_dpdk_single(void)
{
    benchmark_result_t result = {0};
    result.library_name = "DPDK LPM";
    
    struct rte_lpm_config config = {
        .max_rules = DPDK_LPM_MAX_RULES,
        .number_tbl8s = DPDK_LPM_TBL8_NUM_GROUPS,
        .flags = 0
    };
    
    struct rte_lpm *lpm = rte_lpm_create("IPv4_LPM", SOCKET_ID_ANY, &config);
    if (lpm == NULL) {
        fprintf(stderr, "Error: Failed to create DPDK LPM: %s\n", rte_strerror(rte_errno));
        result.lookups_per_sec = 0;
        return result;
    }
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25);
        uint32_t ip = ipv4_to_uint32(prefix);
        uint32_t next_hop = i % 256; // DPDK uses 1-byte next hop
        
        int ret = rte_lpm_add(lpm, ip, prefix_len, next_hop);
        /* Silently ignore failures (typically out of tbl8 space) */
        (void)ret;
    }
    
    /* Generate test addresses */
    uint32_t *test_addrs = malloc(NUM_LOOKUPS * sizeof(uint32_t));
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        uint8_t addr[4];
        generate_random_ipv4(addr);
        test_addrs[i] = ipv4_to_uint32(addr);
    }
    
    /* Warm up cache */
    uint32_t next_hop;
    for (int i = 0; i < 1000; i++) {
        rte_lpm_lookup(lpm, test_addrs[i], &next_hop);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        volatile int ret = rte_lpm_lookup(lpm, test_addrs[i], &next_hop);
        (void)ret;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    result.lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    
    /* Estimate memory usage (DPDK doesn't provide direct API) */
    /* tbl24: 2^24 entries * 4 bytes + tbl8: 256 * 256 entries * 4 bytes */
    result.memory_bytes = (1 << 24) * 4 + DPDK_LPM_TBL8_NUM_GROUPS * 256 * 4;
    
    free(test_addrs);
    rte_lpm_free(lpm);
    
    return result;
}
#endif /* HAVE_DPDK */

static benchmark_result_t benchmark_ipv4_liblpm_pure_batch(void)
{
    benchmark_result_t result = {0};
    result.library_name = "liblpm";
    
    /* Use pure LPM trie (standard multibit trie) for comparison */
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25);
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses as uint32_t for fair comparison */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint32_t *test_addrs = malloc(num_batches * BATCH_SIZE * sizeof(uint32_t));
    uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        uint8_t addr[4];
        generate_random_ipv4(addr);
        /* Store as big-endian (network byte order) */
        test_addrs[i] = ipv4_to_uint32(addr);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 10; i++) {
        lpm_lookup_batch_ipv4(trie, test_addrs, next_hops, BATCH_SIZE);
    }
    
    /* Benchmark - using uint32_t API directly */
    struct timespec start, end;
    volatile uint32_t checksum = 0;  /* Prevent optimization */
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        /* Direct uint32_t batch lookup - no pointer array overhead! */
        lpm_lookup_batch_ipv4(trie, &test_addrs[batch * BATCH_SIZE], 
                              next_hops, BATCH_SIZE);
        checksum += next_hops[0];
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    (void)checksum;  /* Prevent warning */
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    result.lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    
    /* Memory: 8-bit stride nodes (2KB each) */
    result.memory_bytes = trie->num_nodes * sizeof(lpm_node_t);
    
    free(test_addrs);
    free(next_hops);
    lpm_destroy(trie);
    
    return result;
}

static benchmark_result_t benchmark_ipv4_liblpm_batch(void)
{
    benchmark_result_t result = {0};
    result.library_name = "liblpm DIR24";
    
    /* Use DIR-24-8 optimized trie for fair comparison with DPDK LPM */
    lpm_trie_t *trie = lpm_create_ipv4_dir24();
    assert(trie != NULL);
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25);
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses as uint32_t for fair comparison */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint32_t *test_addrs = malloc(num_batches * BATCH_SIZE * sizeof(uint32_t));
    uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        uint8_t addr[4];
        generate_random_ipv4(addr);
        /* Store as big-endian (network byte order) */
        test_addrs[i] = ipv4_to_uint32(addr);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 10; i++) {
        lpm_lookup_batch_ipv4(trie, test_addrs, next_hops, BATCH_SIZE);
    }
    
    /* Benchmark - using uint32_t API directly */
    struct timespec start, end;
    volatile uint32_t checksum = 0;  /* Prevent optimization */
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        /* Direct uint32_t batch lookup - no pointer array overhead! */
        lpm_lookup_batch_ipv4(trie, &test_addrs[batch * BATCH_SIZE], 
                              next_hops, BATCH_SIZE);
        checksum += next_hops[0];
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    (void)checksum;  /* Prevent warning */
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    result.lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    
    /* Memory: DIR24 table (16.7M * 4 bytes = 64MB) + tbl8 groups */
    size_t dir24_mem = LPM_IPV4_DIR24_SIZE * sizeof(struct lpm_dir24_entry);
    size_t tbl8_mem = trie->tbl8_groups_used * 256 * sizeof(struct lpm_tbl8_entry);
    result.memory_bytes = dir24_mem + tbl8_mem;
    
    free(test_addrs);
    free(next_hops);
    lpm_destroy(trie);
    
    return result;
}

#ifdef HAVE_DPDK
static benchmark_result_t benchmark_ipv4_dpdk_batch(void)
{
    benchmark_result_t result = {0};
    result.library_name = "DPDK LPM";
    
    struct rte_lpm_config config = {
        .max_rules = DPDK_LPM_MAX_RULES,
        .number_tbl8s = DPDK_LPM_TBL8_NUM_GROUPS,
        .flags = 0
    };
    
    struct rte_lpm *lpm = rte_lpm_create("IPv4_LPM_Batch", SOCKET_ID_ANY, &config);
    if (lpm == NULL) {
        fprintf(stderr, "Error: Failed to create DPDK LPM\n");
        result.lookups_per_sec = 0;
        return result;
    }
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25);
        uint32_t ip = ipv4_to_uint32(prefix);
        uint32_t next_hop = i % 256;
        rte_lpm_add(lpm, ip, prefix_len, next_hop);
    }
    
    /* Generate test addresses */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint32_t *test_addrs = malloc(num_batches * BATCH_SIZE * sizeof(uint32_t));
    uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        uint8_t addr[4];
        generate_random_ipv4(addr);
        test_addrs[i] = ipv4_to_uint32(addr);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 10; i++) {
        rte_lpm_lookup_bulk(lpm, test_addrs, next_hops, BATCH_SIZE);
    }
    
    /* Benchmark */
    struct timespec start, end;
    volatile uint32_t checksum = 0;  /* Prevent optimization */
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        rte_lpm_lookup_bulk(lpm, &test_addrs[batch * BATCH_SIZE], 
                           next_hops, BATCH_SIZE);
        /* Prevent compiler from optimizing away the loop */
        checksum += next_hops[0];
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    (void)checksum;  /* Use the checksum to prevent warning */
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    result.lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    result.memory_bytes = (1 << 24) * 4 + DPDK_LPM_TBL8_NUM_GROUPS * 256 * 4;
    
    free(test_addrs);
    free(next_hops);
    rte_lpm_free(lpm);
    
    return result;
}
#endif /* HAVE_DPDK */

/* ============================================================================
 * IPv6 Benchmarks
 * ============================================================================ */

static benchmark_result_t benchmark_ipv6_liblpm_single(void)
{
    benchmark_result_t result = {0};
    result.library_name = "liblpm";
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[16];
        generate_random_ipv6(prefix);
        uint8_t prefix_len = 8 + (rand() % 121); // 8 to 128
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses */
    uint8_t (*test_addrs)[16] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        generate_random_ipv6(test_addrs[i]);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 1000; i++) {
        lpm_lookup_ipv6(trie, test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        volatile uint32_t result_nh = lpm_lookup_ipv6(trie, test_addrs[i]);
        (void)result_nh;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    result.lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    result.memory_bytes = trie->num_nodes * sizeof(lpm_node_t);
    
    free(test_addrs);
    lpm_destroy(trie);
    
    return result;
}

#ifdef HAVE_DPDK
static benchmark_result_t benchmark_ipv6_dpdk_single(void)
{
    benchmark_result_t result = {0};
    result.library_name = "DPDK LPM6";
    
    struct rte_lpm6_config config = {
        .max_rules = DPDK_LPM6_MAX_RULES,
        .number_tbl8s = DPDK_LPM6_NUMBER_TBL8S,
        .flags = 0
    };
    
    struct rte_lpm6 *lpm6 = rte_lpm6_create("IPv6_LPM", SOCKET_ID_ANY, &config);
    if (lpm6 == NULL) {
        fprintf(stderr, "Error: Failed to create DPDK LPM6: %s\n", rte_strerror(rte_errno));
        result.lookups_per_sec = 0;
        return result;
    }
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[16];
        generate_random_ipv6(prefix);
        uint8_t prefix_len = 8 + (rand() % 121);
        uint32_t next_hop = i;
        
        int ret = rte_lpm6_add(lpm6, (const struct rte_ipv6_addr *)prefix, prefix_len, next_hop);
        /* Silently ignore failures (typically out of tbl8 space) */
        (void)ret;
    }
    
    /* Generate test addresses */
    uint8_t (*test_addrs)[16] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        generate_random_ipv6(test_addrs[i]);
    }
    
    /* Warm up cache */
    uint32_t next_hop;
    for (int i = 0; i < 1000; i++) {
        rte_lpm6_lookup(lpm6, (const struct rte_ipv6_addr *)test_addrs[i], &next_hop);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        volatile int ret = rte_lpm6_lookup(lpm6, (const struct rte_ipv6_addr *)test_addrs[i], &next_hop);
        (void)ret;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    result.lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    
    /* Estimate LPM6 memory (complex structure, rough estimate) */
    result.memory_bytes = DPDK_LPM6_NUMBER_TBL8S * 256 * 8;
    
    free(test_addrs);
    rte_lpm6_free(lpm6);
    
    return result;
}
#endif /* HAVE_DPDK */

static benchmark_result_t benchmark_ipv6_liblpm_batch(void)
{
    benchmark_result_t result = {0};
    result.library_name = "liblpm";
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[16];
        generate_random_ipv6(prefix);
        uint8_t prefix_len = 8 + (rand() % 121);
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint8_t (*test_addrs)[16] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
    uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        generate_random_ipv6(test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        lpm_lookup_batch_ipv6(trie, &test_addrs[batch * BATCH_SIZE], next_hops, BATCH_SIZE);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    result.lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    result.memory_bytes = trie->num_nodes * sizeof(lpm_node_t);
    
    free(test_addrs);
    free(next_hops);
    lpm_destroy(trie);
    
    return result;
}

#ifdef HAVE_DPDK
static benchmark_result_t benchmark_ipv6_dpdk_batch(void)
{
    benchmark_result_t result = {0};
    result.library_name = "DPDK LPM6";
    
    struct rte_lpm6_config config = {
        .max_rules = DPDK_LPM6_MAX_RULES,
        .number_tbl8s = DPDK_LPM6_NUMBER_TBL8S,
        .flags = 0
    };
    
    struct rte_lpm6 *lpm6 = rte_lpm6_create("IPv6_LPM_Batch", SOCKET_ID_ANY, &config);
    if (lpm6 == NULL) {
        fprintf(stderr, "Error: Failed to create DPDK LPM6\n");
        result.lookups_per_sec = 0;
        return result;
    }
    
    /* Add random prefixes */
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[16];
        generate_random_ipv6(prefix);
        uint8_t prefix_len = 8 + (rand() % 121);
        uint32_t next_hop = i;
        rte_lpm6_add(lpm6, (const struct rte_ipv6_addr *)prefix, prefix_len, next_hop);
    }
    
    /* Generate test addresses */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint8_t (*test_addrs)[16] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
    uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        generate_random_ipv6(test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        rte_lpm6_lookup_bulk_func(lpm6, 
                                  (struct rte_ipv6_addr *)&test_addrs[batch * BATCH_SIZE],
                                  (int32_t *)next_hops, BATCH_SIZE);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    result.lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    result.ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    result.memory_bytes = DPDK_LPM6_NUMBER_TBL8S * 256 * 8;
    
    free(test_addrs);
    free(next_hops);
    rte_lpm6_free(lpm6);
    
    return result;
}
#endif /* HAVE_DPDK */

/* ============================================================================
 * Main
 * ============================================================================ */

/* Flag to track if DPDK is available at runtime */
#ifdef HAVE_DPDK
static int dpdk_available = 0;
#endif

int main(int argc, char **argv)
{
    /* Force output to be line-buffered for better crash debugging */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    
    printf("bench_comparison: Starting benchmark...\n");
    fflush(stdout);
    
    (void)argc; /* Suppress unused parameter warning */
    
#ifdef HAVE_DPDK
    /* Initialize DPDK EAL (Environment Abstraction Layer) */
    char *dpdk_argv[] = {
        argv[0],
        "-l", "0",           /* Use logical core 0 */
        "--proc-type", "primary",
        "--log-level", "error",
        "--no-pci",          /* Don't need PCI devices */
        "--no-huge",         /* Don't require hugepages */
        "--in-memory",       /* Don't require shared memory files */
        "-m", "256",         /* Use 256MB of memory (reduced) */
        "--",
        NULL
    };
    int dpdk_argc = sizeof(dpdk_argv) / sizeof(dpdk_argv[0]) - 1;
    
    printf("Initializing DPDK EAL (version %d.%d)...\n", RTE_VER_YEAR, RTE_VER_MONTH);
    fflush(stdout);
    
    int ret = rte_eal_init(dpdk_argc, dpdk_argv);
    if (ret < 0) {
        fprintf(stderr, "Warning: DPDK EAL initialization failed: %s\n", rte_strerror(rte_errno));
        fprintf(stderr, "Continuing without DPDK comparison...\n");
        fflush(stderr);
        dpdk_available = 0;
    } else {
        printf("DPDK EAL initialized successfully.\n");
        fflush(stdout);
        dpdk_available = 1;
    }
#endif
    
    printf("\n%s==========================================%s\n", COLOR_BOLD, COLOR_RESET);
#ifdef HAVE_DPDK
    printf("%s  LPM Benchmark (with DPDK)%s\n", COLOR_BOLD, COLOR_RESET);
#else
    printf("%s  LPM Benchmark%s\n", COLOR_BOLD, COLOR_RESET);
#endif
    printf("%s==========================================%s\n", COLOR_BOLD, COLOR_RESET);
#ifdef HAVE_DPDK
    printf("DPDK Version: %d.%d.%d\n", RTE_VER_YEAR, RTE_VER_MONTH, RTE_VER_MINOR);
#endif
    printf("liblpm Version: %s\n", lpm_get_version());
    printf("\n");
    printf("Test Configuration:\n");
    printf("  Prefixes: %d\n", NUM_PREFIXES);
    printf("  Lookups: %d\n", NUM_LOOKUPS);
    printf("  Batch size: %d\n", BATCH_SIZE);
    printf("\n");
    
    /* Seed random number generator with fixed value for reproducibility */
    srand(42);
    
    /* Run IPv4 single lookup benchmark */
    printf("%sRunning IPv4 Single Lookup Benchmark...%s\n", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
    srand(42);
    benchmark_result_t ipv4_liblpm_pure_single = benchmark_ipv4_liblpm_pure_single();
    srand(42);
    benchmark_result_t ipv4_liblpm_single = benchmark_ipv4_liblpm_single();
    
#ifdef HAVE_DPDK
    if (dpdk_available) {
        srand(42);
        benchmark_result_t ipv4_dpdk_single = benchmark_ipv4_dpdk_single();
        benchmark_result_t ipv4_single_results[] = {
            ipv4_liblpm_pure_single, ipv4_liblpm_single, ipv4_dpdk_single
        };
        print_comparison_multi("IPv4 Single Lookup Comparison", ipv4_single_results, 3);
    } else {
        benchmark_result_t ipv4_single_results[] = {
            ipv4_liblpm_pure_single, ipv4_liblpm_single
        };
        print_comparison_multi("IPv4 Single Lookup Comparison", ipv4_single_results, 2);
    }
#else
    benchmark_result_t ipv4_single_results[] = {
        ipv4_liblpm_pure_single, ipv4_liblpm_single
    };
    print_comparison_multi("IPv4 Single Lookup Comparison", ipv4_single_results, 2);
#endif
    
    /* Run IPv4 batch lookup benchmark */
    printf("\n%sRunning IPv4 Batch Lookup Benchmark...%s\n", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
    srand(42);
    benchmark_result_t ipv4_liblpm_pure_batch = benchmark_ipv4_liblpm_pure_batch();
    srand(42);
    benchmark_result_t ipv4_liblpm_batch = benchmark_ipv4_liblpm_batch();
    
#ifdef HAVE_DPDK
    if (dpdk_available) {
        srand(42);
        benchmark_result_t ipv4_dpdk_batch = benchmark_ipv4_dpdk_batch();
        benchmark_result_t ipv4_batch_results[] = {
            ipv4_liblpm_pure_batch, ipv4_liblpm_batch, ipv4_dpdk_batch
        };
        print_comparison_multi("IPv4 Batch Lookup Comparison", ipv4_batch_results, 3);
    } else {
        benchmark_result_t ipv4_batch_results[] = {
            ipv4_liblpm_pure_batch, ipv4_liblpm_batch
        };
        print_comparison_multi("IPv4 Batch Lookup Comparison", ipv4_batch_results, 2);
    }
#else
    benchmark_result_t ipv4_batch_results[] = {
        ipv4_liblpm_pure_batch, ipv4_liblpm_batch
    };
    print_comparison_multi("IPv4 Batch Lookup Comparison", ipv4_batch_results, 2);
#endif
    
    /* Run IPv6 single lookup benchmark */
    printf("\n%sRunning IPv6 Single Lookup Benchmark...%s\n", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
    srand(42);
    benchmark_result_t ipv6_liblpm_single = benchmark_ipv6_liblpm_single();
    
#ifdef HAVE_DPDK
    if (dpdk_available) {
        srand(42);
        benchmark_result_t ipv6_dpdk_single = benchmark_ipv6_dpdk_single();
        print_comparison("IPv6 Single Lookup Comparison", &ipv6_liblpm_single, &ipv6_dpdk_single);
    } else {
        benchmark_result_t ipv6_single_results[] = { ipv6_liblpm_single };
        print_comparison_multi("IPv6 Single Lookup", ipv6_single_results, 1);
    }
#else
    benchmark_result_t ipv6_single_results[] = { ipv6_liblpm_single };
    print_comparison_multi("IPv6 Single Lookup", ipv6_single_results, 1);
#endif
    
    /* Run IPv6 batch lookup benchmark */
    printf("\n%sRunning IPv6 Batch Lookup Benchmark...%s\n", COLOR_CYAN, COLOR_RESET);
    fflush(stdout);
    srand(42);
    benchmark_result_t ipv6_liblpm_batch = benchmark_ipv6_liblpm_batch();
    
#ifdef HAVE_DPDK
    if (dpdk_available) {
        srand(42);
        benchmark_result_t ipv6_dpdk_batch = benchmark_ipv6_dpdk_batch();
        print_comparison("IPv6 Batch Lookup Comparison", &ipv6_liblpm_batch, &ipv6_dpdk_batch);
    } else {
        benchmark_result_t ipv6_batch_results[] = { ipv6_liblpm_batch };
        print_comparison_multi("IPv6 Batch Lookup", ipv6_batch_results, 1);
    }
#else
    benchmark_result_t ipv6_batch_results[] = { ipv6_liblpm_batch };
    print_comparison_multi("IPv6 Batch Lookup", ipv6_batch_results, 1);
#endif
    
    /* Summary */
    printf("\n%s==========================================%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s  Benchmark Summary%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s==========================================%s\n", COLOR_BOLD, COLOR_RESET);
#ifdef HAVE_DPDK
    if (dpdk_available) {
        printf("\nDPDK comparison enabled.\n");
        printf("For detailed methodology and interpretation,\n");
        printf("see docs/DPDK_COMPARISON.md\n");
    } else {
        printf("\nDPDK was compiled but initialization failed.\n");
        printf("Showing liblpm algorithms only.\n");
        printf("This can happen in containerized environments.\n");
    }
#else
    printf("\nDPDK not available - showing liblpm algorithms only.\n");
    printf("To enable DPDK comparison, install DPDK and rebuild with:\n");
    printf("  cmake -DWITH_DPDK_BENCHMARK=ON ..\n");
#endif
    printf("\n");
    
#ifdef HAVE_DPDK
    /* Cleanup DPDK */
    if (dpdk_available) {
        rte_eal_cleanup();
    }
#endif
    
    return 0;
}


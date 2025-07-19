#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "../include/lpm.h"

#define MILLION 1000000
#define NUM_PREFIXES 10000
#define NUM_LOOKUPS 1000000
#define BATCH_SIZE 256

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

/* Measure time difference in microseconds */
static double time_diff_us(const struct timespec *start, const struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000.0 + 
           (end->tv_nsec - start->tv_nsec) / 1000.0;
}

static void benchmark_ipv4_single_lookup(void)
{
    printf("\n=== IPv4 Single Lookup Benchmark ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    printf("Adding %d random prefixes...\n", NUM_PREFIXES);
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
        volatile uint32_t result = lpm_lookup(trie, test_addrs[i]);
        (void)result; // Prevent optimization
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    double ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    
    printf("Single lookup performance:\n");
    printf("  Total lookups: %d\n", NUM_LOOKUPS);
    printf("  Total time: %.2f ms\n", elapsed_us / 1000);
    printf("  Lookups/sec: %.2f million\n", lookups_per_sec / MILLION);
    printf("  Time per lookup: %.2f ns\n", ns_per_lookup);
    
    lpm_print_stats(trie);
    free(test_addrs);
    lpm_destroy(trie);
}

static void benchmark_ipv4_batch_lookup(void)
{
    printf("\n=== IPv4 Batch Lookup Benchmark ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    printf("Adding %d random prefixes...\n", NUM_PREFIXES);
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        uint8_t prefix_len = 8 + (rand() % 25);
        lpm_add(trie, prefix, prefix_len, i);
    }
    
    /* Generate test addresses */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint8_t (*test_addrs)[4] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
    const uint8_t **addr_ptrs = malloc(BATCH_SIZE * sizeof(uint8_t*));
    uint32_t *next_hops = malloc(BATCH_SIZE * sizeof(uint32_t));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        generate_random_ipv4(test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        /* Setup pointers for this batch */
        for (int i = 0; i < BATCH_SIZE; i++) {
            addr_ptrs[i] = test_addrs[batch * BATCH_SIZE + i];
        }
        
        lpm_lookup_batch(trie, addr_ptrs, next_hops, BATCH_SIZE);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    double lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    double ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    
    printf("Batch lookup performance (batch size %d):\n", BATCH_SIZE);
    printf("  Total lookups: %.0f\n", total_lookups);
    printf("  Total time: %.2f ms\n", elapsed_us / 1000);
    printf("  Lookups/sec: %.2f million\n", lookups_per_sec / MILLION);
    printf("  Time per lookup: %.2f ns\n", ns_per_lookup);
    
    free(test_addrs);
    free(addr_ptrs);
    free(next_hops);
    lpm_destroy(trie);
}

static void benchmark_ipv6_single_lookup(void)
{
    printf("\n=== IPv6 Single Lookup Benchmark ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    printf("Adding %d random prefixes...\n", NUM_PREFIXES);
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
        volatile uint32_t result = lpm_lookup_ipv6(trie, test_addrs[i]);
        (void)result;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    double ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    
    printf("Single lookup performance:\n");
    printf("  Total lookups: %d\n", NUM_LOOKUPS);
    printf("  Total time: %.2f ms\n", elapsed_us / 1000);
    printf("  Lookups/sec: %.2f million\n", lookups_per_sec / MILLION);
    printf("  Time per lookup: %.2f ns\n", ns_per_lookup);
    
    lpm_print_stats(trie);
    free(test_addrs);
    lpm_destroy(trie);
}

static void benchmark_memory_usage(void)
{
    printf("\n=== Memory Usage Analysis ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    
    /* Add prefixes and track node count */
    printf("Prefixes | Nodes | Bytes/Prefix | Total Memory (MB)\n");
    printf("---------|-------|--------------|------------------\n");
    
    for (int count = 1000; count <= 100000; count *= 10) {
        /* Clear trie */
        lpm_destroy(trie);
        trie = lpm_create(LPM_IPV4_MAX_DEPTH);
        
        /* Add random prefixes */
        for (int i = 0; i < count; i++) {
            uint8_t prefix[4];
            generate_random_ipv4(prefix);
            uint8_t prefix_len = 8 + (rand() % 25);
            lpm_add(trie, prefix, prefix_len, i);
        }
        
        uint64_t num_nodes = trie->num_nodes;
        size_t node_size = sizeof(lpm_node_t);
        size_t total_memory = num_nodes * node_size;
        double bytes_per_prefix = (double)total_memory / count;
        double total_mb = total_memory / (1024.0 * 1024.0);
        
        printf("%8d | %5lu | %12.1f | %17.2f\n", 
               count, num_nodes, bytes_per_prefix, total_mb);
    }
    
    lpm_destroy(trie);
}

int main(void)
{
    printf("=== LPM Library Performance Benchmark ===\n");
    printf("Library version: %s\n", lpm_get_version());
    
    /* Seed random number generator */
    srand(time(NULL));
    
    /* Show CPU features */
    uint32_t features = lpm_detect_cpu_features();
    printf("\nCPU features enabled: 0x%08x\n", features);
    
    /* Run benchmarks */
    benchmark_ipv4_single_lookup();
    benchmark_ipv4_batch_lookup();
    benchmark_ipv6_single_lookup();
    benchmark_memory_usage();
    
    printf("\nBenchmark complete!\n");
    return 0;
}

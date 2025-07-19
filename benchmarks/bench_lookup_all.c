#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "../include/lpm.h"

#define MILLION 1000000
#define NUM_PREFIXES 10000
#define NUM_LOOKUPS 100000
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
static double time_diff_us(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec - start->tv_sec) * 1000000.0 + 
           (end->tv_nsec - start->tv_nsec) / 1000.0;
}

static void benchmark_ipv4_lookup_all(void)
{
    printf("\n=== IPv4 lookup_all Benchmark ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes with varying lengths to create overlaps */
    printf("Adding %d random prefixes with overlaps...\n", NUM_PREFIXES);
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        
        /* Add multiple prefix lengths for overlap */
        if (i % 4 == 0) {
            lpm_add_prefix(trie, prefix, 8, (void*)(uintptr_t)(i*10));
        }
        if (i % 3 == 0) {
            lpm_add_prefix(trie, prefix, 16, (void*)(uintptr_t)(i*10+1));
        }
        if (i % 2 == 0) {
            lpm_add_prefix(trie, prefix, 24, (void*)(uintptr_t)(i*10+2));
        }
        lpm_add_prefix(trie, prefix, 16 + (rand() % 17), (void*)(uintptr_t)(i*10+3));
    }
    
    /* Generate test addresses */
    uint8_t (*test_addrs)[4] = malloc(NUM_LOOKUPS * sizeof(*test_addrs));
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        generate_random_ipv4(test_addrs[i]);
    }
    
    /* Warm up cache */
    for (int i = 0; i < 1000; i++) {
        lpm_result_t *result = lpm_lookup_all(trie, test_addrs[i]);
        lpm_result_destroy(result);
    }
    
    /* Benchmark */
    struct timespec start, end;
    uint64_t total_matches = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        lpm_result_t *result = lpm_lookup_all(trie, test_addrs[i]);
        total_matches += result->count;
        lpm_result_destroy(result);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double lookups_per_sec = (NUM_LOOKUPS / elapsed_us) * MILLION;
    double ns_per_lookup = (elapsed_us * 1000) / NUM_LOOKUPS;
    double avg_matches = (double)total_matches / NUM_LOOKUPS;
    
    printf("lookup_all performance:\n");
    printf("  Total lookups: %d\n", NUM_LOOKUPS);
    printf("  Total time: %.2f ms\n", elapsed_us / 1000);
    printf("  Lookups/sec: %.2f million\n", lookups_per_sec / MILLION);
    printf("  Time per lookup: %.2f ns\n", ns_per_lookup);
    printf("  Average matches per lookup: %.2f\n", avg_matches);
    printf("  Total matches found: %lu\n", total_matches);
    
    lpm_print_stats(trie);
    free(test_addrs);
    lpm_destroy(trie);
}

static void benchmark_ipv4_batch_lookup_all(void)
{
    printf("\n=== IPv4 Batch lookup_all Benchmark ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add prefixes with overlaps */
    printf("Adding %d random prefixes with overlaps...\n", NUM_PREFIXES);
    for (int i = 0; i < NUM_PREFIXES; i++) {
        uint8_t prefix[4];
        generate_random_ipv4(prefix);
        
        /* Create more overlaps */
        lpm_add_prefix(trie, prefix, 8 + (rand() % 25), (void*)(uintptr_t)i);
        if (i % 2 == 0) {
            prefix[3] = 0;
            lpm_add_prefix(trie, prefix, 24, (void*)(uintptr_t)(i+NUM_PREFIXES));
        }
    }
    
    /* Generate test addresses */
    int num_batches = NUM_LOOKUPS / BATCH_SIZE;
    uint8_t (*test_addrs)[4] = malloc(num_batches * BATCH_SIZE * sizeof(*test_addrs));
    const uint8_t **addr_ptrs = malloc(BATCH_SIZE * sizeof(uint8_t*));
    lpm_result_t **results = malloc(BATCH_SIZE * sizeof(lpm_result_t*));
    
    for (int i = 0; i < num_batches * BATCH_SIZE; i++) {
        generate_random_ipv4(test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    uint64_t total_matches = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int batch = 0; batch < num_batches; batch++) {
        /* Setup pointers for this batch */
        for (int i = 0; i < BATCH_SIZE; i++) {
            addr_ptrs[i] = test_addrs[batch * BATCH_SIZE + i];
        }
        
        lpm_lookup_all_batch(trie, addr_ptrs, results, BATCH_SIZE);
        
        /* Count matches and free results */
        for (int i = 0; i < BATCH_SIZE; i++) {
            total_matches += results[i]->count;
            lpm_result_destroy(results[i]);
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double total_lookups = num_batches * BATCH_SIZE;
    double lookups_per_sec = (total_lookups / elapsed_us) * MILLION;
    double ns_per_lookup = (elapsed_us * 1000) / total_lookups;
    double avg_matches = (double)total_matches / total_lookups;
    
    printf("Batch lookup_all performance (batch size %d):\n", BATCH_SIZE);
    printf("  Total lookups: %.0f\n", total_lookups);
    printf("  Total time: %.2f ms\n", elapsed_us / 1000);
    printf("  Lookups/sec: %.2f million\n", lookups_per_sec / MILLION);
    printf("  Time per lookup: %.2f ns\n", ns_per_lookup);
    printf("  Average matches per lookup: %.2f\n", avg_matches);
    
    free(test_addrs);
    free(addr_ptrs);
    free(results);
    lpm_destroy(trie);
}

static void benchmark_ipv6_lookup_all(void)
{
    printf("\n=== IPv6 lookup_all Benchmark ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add random prefixes */
    printf("Adding %d random IPv6 prefixes...\n", NUM_PREFIXES/2); // Fewer for IPv6
    for (int i = 0; i < NUM_PREFIXES/2; i++) {
        uint8_t prefix[16];
        generate_random_ipv6(prefix);
        
        /* Add with different prefix lengths for overlaps */
        lpm_add_prefix(trie, prefix, 16 + (rand() % 49), (void*)(uintptr_t)(i*10));
        if (i % 3 == 0) {
            lpm_add_prefix(trie, prefix, 32, (void*)(uintptr_t)(i*10+1));
        }
        if (i % 5 == 0) {
            lpm_add_prefix(trie, prefix, 64, (void*)(uintptr_t)(i*10+2));
        }
    }
    
    /* Generate test addresses */
    uint8_t (*test_addrs)[16] = malloc(NUM_LOOKUPS/2 * sizeof(*test_addrs));
    for (int i = 0; i < NUM_LOOKUPS/2; i++) {
        generate_random_ipv6(test_addrs[i]);
    }
    
    /* Benchmark */
    struct timespec start, end;
    uint64_t total_matches = 0;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_LOOKUPS/2; i++) {
        lpm_result_t *result = lpm_lookup_all_ipv6(trie, test_addrs[i]);
        total_matches += result->count;
        lpm_result_destroy(result);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_us = time_diff_us(&start, &end);
    double lookups_per_sec = ((NUM_LOOKUPS/2) / elapsed_us) * MILLION;
    double ns_per_lookup = (elapsed_us * 1000) / (NUM_LOOKUPS/2);
    double avg_matches = (double)total_matches / (NUM_LOOKUPS/2);
    
    printf("IPv6 lookup_all performance:\n");
    printf("  Total lookups: %d\n", NUM_LOOKUPS/2);
    printf("  Total time: %.2f ms\n", elapsed_us / 1000);
    printf("  Lookups/sec: %.2f million\n", lookups_per_sec / MILLION);
    printf("  Time per lookup: %.2f ns\n", ns_per_lookup);
    printf("  Average matches per lookup: %.2f\n", avg_matches);
    
    lpm_print_stats(trie);
    free(test_addrs);
    lpm_destroy(trie);
}

static void benchmark_overlapping_prefixes(void)
{
    printf("\n=== Overlapping Prefixes Stress Test ===\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Create deeply nested overlapping prefixes */
    printf("Creating deeply nested overlapping prefixes...\n");
    uint8_t base_prefix[4] = {10, 0, 0, 0};
    
    /* Add prefixes at every possible length */
    for (int len = 8; len <= 32; len++) {
        lpm_add_prefix(trie, base_prefix, len, (void*)(uintptr_t)len);
    }
    
    /* Add more overlapping prefixes */
    for (int i = 0; i < 256; i++) {
        base_prefix[1] = i;
        for (int len = 16; len <= 24; len += 8) {
            lpm_add_prefix(trie, base_prefix, len, (void*)(uintptr_t)(1000 + i*10 + len));
        }
    }
    
    /* Test address that should match many prefixes */
    uint8_t test_addr[4] = {10, 128, 64, 32};
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    lpm_result_t *result = lpm_lookup_all(trie, test_addr);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_ns = time_diff_us(&start, &end) * 1000;
    
    printf("Deep overlap test results:\n");
    printf("  Test address: 10.128.64.32\n");
    printf("  Matched prefixes: %u\n", result->count);
    printf("  Lookup time: %.2f ns\n", elapsed_ns);
    
    /* Display some matches */
    printf("  First few matches:\n");
    for (uint32_t i = 0; i < result->count && i < 5; i++) {
        printf("    - Prefix length %u, data=%lu\n", 
               result->prefixes[i].prefix_len, 
               (uintptr_t)result->prefixes[i].user_data);
    }
    
    lpm_result_destroy(result);
    lpm_print_stats(trie);
    lpm_destroy(trie);
}

int main(void)
{
    printf("=== LPM lookup_all Performance Benchmark ===\n");
    printf("Library version: %s\n", lpm_get_version());
    
    /* Seed random number generator */
    srand(time(NULL));
    
    /* Show CPU features */
    uint32_t features = lpm_detect_cpu_features();
    printf("\nCPU features enabled: 0x%08x\n", features);
    
    /* Run benchmarks */
    benchmark_ipv4_lookup_all();
    benchmark_ipv4_batch_lookup_all();
    benchmark_ipv6_lookup_all();
    benchmark_overlapping_prefixes();
    
    printf("\nBenchmark complete!\n");
    return 0;
} 
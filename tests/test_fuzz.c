#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "../include/lpm.h"

/* Fuzzing test configuration */
#define MAX_PREFIXES 1000
#define MAX_LOOKUPS 500
#define MAX_PREFIX_LEN 32  /* For IPv4 testing */

/* Fuzzing test data structures */
typedef struct {
    uint8_t prefix[16];
    uint8_t prefix_len;
    uint32_t next_hop;
} fuzz_prefix_t;

typedef struct {
    uint8_t addr[16];
    uint8_t addr_len;
} fuzz_lookup_t;

/* Test case generator */
static void generate_random_prefix(fuzz_prefix_t *prefix, uint8_t max_len) {
    prefix->prefix_len = (rand() % max_len) + 1;
    
    /* Generate random prefix bytes */
    for (int i = 0; i < 16; i++) {
        prefix->prefix[i] = rand() & 0xFF;
    }
    
    /* Clear unused bits based on prefix length */
    int bytes_needed = (prefix->prefix_len + 7) / 8;
    for (int i = bytes_needed; i < 16; i++) {
        prefix->prefix[i] = 0;
    }
    
    /* Clear unused bits in the last byte */
    if (prefix->prefix_len % 8 != 0) {
        uint8_t mask = 0xFF << (8 - (prefix->prefix_len % 8));
        prefix->prefix[bytes_needed - 1] &= mask;
    }
    
    prefix->next_hop = rand() & 0xFFFF;
}

static void generate_random_address(fuzz_lookup_t *lookup, uint8_t max_len) {
    lookup->addr_len = max_len;
    
    /* Generate random address bytes */
    for (int i = 0; i < 16; i++) {
        lookup->addr[i] = rand() & 0xFF;
    }
}

/* Test memory exhaustion scenarios */
static void test_memory_exhaustion(void) {
    printf("Testing memory exhaustion scenarios...\n");
    
    /* Create many tries to test memory management */
    lpm_trie_t *tries[10];
    for (int i = 0; i < 10; i++) {
        tries[i] = lpm_create(LPM_IPV4_MAX_DEPTH);
        assert(tries[i] != NULL);
        
        /* Add some prefixes */
        for (int j = 0; j < 100; j++) {
            fuzz_prefix_t prefix;
            generate_random_prefix(&prefix, LPM_IPV4_MAX_DEPTH);
            int ret = lpm_add(tries[i], prefix.prefix, prefix.prefix_len, prefix.next_hop);
            assert(ret == 0);
        }
    }
    
    /* Clean up */
    for (int i = 0; i < 10; i++) {
        lpm_destroy(tries[i]);
    }
    
    printf("Memory exhaustion test passed\n");
}

/* Test bitmap operations edge cases */
static void test_bitmap_edge_cases(void) {
    printf("Testing bitmap edge cases...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Test prefixes at stride boundaries */
    const uint8_t prefix1[] = {0x00, 0x00, 0x00, 0x00};
    const uint8_t prefix2[] = {0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t prefix3[] = {0x80, 0x00, 0x00, 0x00};
    
    assert(lpm_add(trie, prefix1, 8, 1) == 0);
    assert(lpm_add(trie, prefix2, 16, 2) == 0);
    assert(lpm_add(trie, prefix3, 24, 3) == 0);
    
    /* Test lookups */
    const uint8_t addr1[] = {0x00, 0x00, 0x00, 0x00};
    const uint8_t addr2[] = {0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t addr3[] = {0x80, 0x00, 0x00, 0x00};
    
    uint32_t result1 = lpm_lookup(trie, addr1);
    uint32_t result2 = lpm_lookup(trie, addr2);
    uint32_t result3 = lpm_lookup(trie, addr3);
    
    assert(result1 == 1);
    assert(result2 == 2);
    assert(result3 == 3);
    
    lpm_destroy(trie);
    printf("Bitmap edge cases test passed\n");
}

/* Test overlapping prefixes */
static void test_overlapping_prefixes(void) {
    printf("Testing overlapping prefixes...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add overlapping prefixes */
    const uint8_t prefix1[] = {192, 168, 0, 0};
    const uint8_t prefix2[] = {192, 168, 1, 0};
    const uint8_t prefix3[] = {192, 168, 0, 1};
    
    assert(lpm_add(trie, prefix1, 16, 1) == 0);
    assert(lpm_add(trie, prefix2, 24, 2) == 0);
    assert(lpm_add(trie, prefix3, 24, 3) == 0);
    
    /* Test lookups with overlapping prefixes */
    const uint8_t addr1[] = {192, 168, 0, 1};
    const uint8_t addr2[] = {192, 168, 1, 1};
    const uint8_t addr3[] = {192, 168, 0, 2};
    
    uint32_t result1 = lpm_lookup(trie, addr1);
    uint32_t result2 = lpm_lookup(trie, addr2);
    uint32_t result3 = lpm_lookup(trie, addr3);
    
    /* Should match the most specific prefix */
    assert(result1 == 3);  /* 192.168.0.1/24 */
    assert(result2 == 2);  /* 192.168.1.1/24 */
    assert(result3 == 3);  /* 192.168.0.2/24 */
    
    lpm_destroy(trie);
    printf("Overlapping prefixes test passed\n");
}

/* Test batch operations */
static void test_batch_operations_fuzz(void) {
    printf("Testing batch operations fuzzing...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add some prefixes */
    for (int i = 0; i < 50; i++) {
        fuzz_prefix_t prefix;
        generate_random_prefix(&prefix, LPM_IPV4_MAX_DEPTH);
        int ret = lpm_add(trie, prefix.prefix, prefix.prefix_len, prefix.next_hop);
        assert(ret == 0);
    }
    
    /* Generate batch lookup addresses */
    const uint8_t *addrs[100];
    uint8_t addr_buffers[100][16];
    uint32_t next_hops[100];
    
    for (int i = 0; i < 100; i++) {
        fuzz_lookup_t lookup;
        generate_random_address(&lookup, LPM_IPV4_MAX_DEPTH);
        memcpy(addr_buffers[i], lookup.addr, 16);
        addrs[i] = addr_buffers[i];
    }
    
    /* Perform batch lookup */
    lpm_lookup_batch(trie, addrs, next_hops, 100);
    
    /* Verify results are consistent with individual lookups */
    for (int i = 0; i < 100; i++) {
        uint32_t individual_result = lpm_lookup(trie, addrs[i]);
        assert(individual_result == next_hops[i]);
    }
    
    lpm_destroy(trie);
    printf("Batch operations fuzzing test passed\n");
}

/* Test IPv6 functionality */
static void test_ipv6_fuzz(void) {
    printf("Testing IPv6 fuzzing...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add IPv6 prefixes */
    for (int i = 0; i < 50; i++) {
        fuzz_prefix_t prefix;
        generate_random_prefix(&prefix, LPM_IPV6_MAX_DEPTH);
        int ret = lpm_add(trie, prefix.prefix, prefix.prefix_len, prefix.next_hop);
        assert(ret == 0);
    }
    
    /* Perform IPv6 lookups */
    for (int i = 0; i < 100; i++) {
        fuzz_lookup_t lookup;
        generate_random_address(&lookup, LPM_IPV6_MAX_DEPTH);
        uint32_t result = lpm_lookup(trie, lookup.addr);
        /* Result can be valid or invalid, just ensure no crash */
        (void)result;
    }
    
    lpm_destroy(trie);
    printf("IPv6 fuzzing test passed\n");
}

/* Test error conditions */
static void test_error_conditions_fuzz(void) {
    printf("Testing error conditions fuzzing...\n");
    
    /* Test NULL parameters */
    assert(lpm_create(0) == NULL);  /* Invalid max_depth */
    assert(lpm_create(33) == NULL); /* Invalid max_depth for IPv4 */
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Test invalid prefix operations */
    assert(lpm_add(trie, NULL, 8, 1) == -1);
    assert(lpm_add(trie, (uint8_t[]){192, 168, 0, 0}, 33, 1) == -1);
    
    /* Test invalid lookup operations */
    assert(lpm_lookup(NULL, (uint8_t[]){192, 168, 0, 1}) == LPM_INVALID_NEXT_HOP);
    assert(lpm_lookup(trie, NULL) == LPM_INVALID_NEXT_HOP);
    
    lpm_destroy(trie);
    printf("Error conditions fuzzing test passed\n");
}

/* Test default route handling */
static void test_default_route_fuzz(void) {
    printf("Testing default route fuzzing...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add default route */
    const uint8_t default_prefix[] = {0, 0, 0, 0};
    assert(lpm_add(trie, default_prefix, 0, 999) == 0);
    
    /* Add some specific prefixes */
    const uint8_t prefix1[] = {10, 0, 0, 0};
    const uint8_t prefix2[] = {192, 168, 0, 0};
    assert(lpm_add(trie, prefix1, 8, 100) == 0);
    assert(lpm_add(trie, prefix2, 16, 200) == 0);
    
    /* Test lookups */
    const uint8_t addr1[] = {10, 1, 2, 3};
    const uint8_t addr2[] = {192, 168, 1, 1};
    const uint8_t addr3[] = {8, 8, 8, 8};
    
    assert(lpm_lookup(trie, addr1) == 100);
    assert(lpm_lookup(trie, addr2) == 200);
    assert(lpm_lookup(trie, addr3) == 999);  /* Default route */
    
    lpm_destroy(trie);
    printf("Default route fuzzing test passed\n");
}

/* Test large scale operations */
static void test_large_scale_fuzz(void) {
    printf("Testing large scale fuzzing...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add many prefixes */
    for (int i = 0; i < MAX_PREFIXES; i++) {
        fuzz_prefix_t prefix;
        generate_random_prefix(&prefix, LPM_IPV4_MAX_DEPTH);
        int ret = lpm_add(trie, prefix.prefix, prefix.prefix_len, prefix.next_hop);
        assert(ret == 0);
    }
    
    /* Perform many lookups */
    for (int i = 0; i < MAX_LOOKUPS; i++) {
        fuzz_lookup_t lookup;
        generate_random_address(&lookup, LPM_IPV4_MAX_DEPTH);
        uint32_t result = lpm_lookup(trie, lookup.addr);
        /* Result can be valid or invalid, just ensure no crash */
        (void)result;
    }
    
    lpm_print_stats(trie);
    lpm_destroy(trie);
    printf("Large scale fuzzing test passed\n");
}

/* Main fuzzing test */
int main(int argc, char *argv[]) {
    printf("Starting LPM fuzzing tests...\n");
    printf("Library version: %s\n\n", lpm_get_version());
    
    /* Seed random number generator */
    srand(42);
    
    /* Run all fuzzing tests */
    test_memory_exhaustion();
    test_bitmap_edge_cases();
    test_overlapping_prefixes();
    test_batch_operations_fuzz();
    test_ipv6_fuzz();
    test_error_conditions_fuzz();
    test_default_route_fuzz();
    test_large_scale_fuzz();
    
    printf("\nAll fuzzing tests passed!\n");
    return 0;
}

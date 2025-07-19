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

/* Custom allocator for testing memory management */
static void *test_alloc(size_t size) {
    return malloc(size);
}

static void test_free(void *ptr) {
    free(ptr);
}

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

/* Test lookup_all functionality */
static void test_lookup_all_fuzz(void) {
    printf("Testing lookup_all fuzzing...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add multiple prefixes that could match the same address */
    const uint8_t prefix1[] = {192, 168, 0, 0};
    const uint8_t prefix2[] = {192, 168, 0, 0};
    const uint8_t prefix3[] = {192, 168, 0, 0};
    
    assert(lpm_add_prefix(trie, prefix1, 8, (void*)1) == 0);
    assert(lpm_add_prefix(trie, prefix2, 16, (void*)2) == 0);
    assert(lpm_add_prefix(trie, prefix3, 24, (void*)3) == 0);
    
    /* Test lookup_all */
    const uint8_t addr[] = {192, 168, 0, 1};
    lpm_result_t *result = lpm_lookup_all(trie, addr);
    assert(result != NULL);
    assert(result->count == 3);
    
    /* Verify all prefixes are returned */
    bool found1 = false, found2 = false, found3 = false;
    for (uint32_t i = 0; i < result->count; i++) {
        uintptr_t user_data = (uintptr_t)result->prefixes[i].user_data;
        if (user_data == 1) found1 = true;
        if (user_data == 2) found2 = true;
        if (user_data == 3) found3 = true;
    }
    assert(found1 && found2 && found3);
    
    lpm_result_destroy(result);
    lpm_destroy(trie);
    printf("Lookup_all fuzzing test passed\n");
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

/* Test custom allocators */
static void test_custom_allocators_fuzz(void) {
    printf("Testing custom allocators fuzzing...\n");
    
    lpm_trie_t *trie = lpm_create_custom(LPM_IPV4_MAX_DEPTH, test_alloc, test_free);
    assert(trie != NULL);
    
    /* Add many prefixes to test memory management */
    for (int i = 0; i < 200; i++) {
        fuzz_prefix_t prefix;
        generate_random_prefix(&prefix, LPM_IPV4_MAX_DEPTH);
        int ret = lpm_add(trie, prefix.prefix, prefix.prefix_len, prefix.next_hop);
        assert(ret == 0);
    }
    
    /* Perform lookups */
    for (int i = 0; i < 100; i++) {
        fuzz_lookup_t lookup;
        generate_random_address(&lookup, LPM_IPV4_MAX_DEPTH);
        uint32_t result = lpm_lookup(trie, lookup.addr);
        /* Result can be valid or invalid, just ensure no crash */
        (void)result;
    }
    
    lpm_destroy(trie);
    printf("Custom allocators fuzzing test passed\n");
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

/* Test result management */
static void test_result_management_fuzz(void) {
    printf("Testing result management fuzzing...\n");
    
    /* Test result creation and destruction */
    for (int i = 0; i < 100; i++) {
        uint32_t capacity = (rand() % 100) + 1;
        lpm_result_t *result = lpm_result_create(capacity);
        assert(result != NULL);
        assert(result->capacity >= capacity);
        assert(result->count == 0);
        
        /* Add some prefixes */
        for (int j = 0; j < capacity + 10; j++) {
            lpm_prefix_t prefix;
            generate_random_prefix((fuzz_prefix_t*)&prefix, LPM_IPV4_MAX_DEPTH);
            int ret = lpm_result_add(result, &prefix);
            assert(ret == 0);
        }
        
        /* Test clear */
        lpm_result_clear(result);
        assert(result->count == 0);
        
        lpm_result_destroy(result);
    }
    
    printf("Result management fuzzing test passed\n");
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
    
    /* Test invalid result operations */
    assert(lpm_result_add(NULL, NULL) == -1);
    assert(lpm_result_create(0) == NULL);
    
    lpm_destroy(trie);
    printf("Error conditions fuzzing test passed\n");
}

/* Main fuzzing test */
int main(int argc, char *argv[]) {
    printf("Starting LPM fuzzing tests...\n");
    
    /* Seed random number generator */
    srand(42);
    
    /* Run all fuzzing tests */
    test_memory_exhaustion();
    test_bitmap_edge_cases();
    test_overlapping_prefixes();
    test_lookup_all_fuzz();
    test_batch_operations_fuzz();
    test_custom_allocators_fuzz();
    test_ipv6_fuzz();
    test_result_management_fuzz();
    test_error_conditions_fuzz();
    
    printf("All fuzzing tests passed!\n");
    return 0;
}

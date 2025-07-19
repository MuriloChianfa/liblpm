#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "../include/lpm.h"

/* Advanced fuzzing test that can read from files for AFL-style fuzzing */

/* Parse fuzz input from file */
static int parse_fuzz_input(const uint8_t *data, size_t size, 
                           lpm_trie_t **trie, uint8_t **addrs, 
                           uint32_t **expected_results, size_t *num_lookups) {
    if (size < 8) return -1;  /* Need at least header */
    
    /* Parse header */
    uint32_t num_prefixes = *(uint32_t*)data;
    uint32_t num_lookups_val = *((uint32_t*)data + 1);
    
    if (num_prefixes > 1000 || num_lookups_val > 1000) return -1;
    
    size_t offset = 8;
    
    /* Create trie */
    *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    if (!*trie) return -1;
    
    /* Parse prefixes */
    for (uint32_t i = 0; i < num_prefixes; i++) {
        if (offset + 6 > size) {
            lpm_destroy(*trie);
            return -1;
        }
        
        uint8_t prefix_len = data[offset];
        uint32_t next_hop = *(uint32_t*)(data + offset + 1);
        uint8_t prefix[16] = {0};
        
        /* Validate prefix_len to prevent buffer overflow */
        if (prefix_len > 128) {  /* Maximum reasonable prefix length */
            lpm_destroy(*trie);
            return -1;
        }
        
        offset += 5;
        if (offset + (prefix_len + 7) / 8 > size) {
            lpm_destroy(*trie);
            return -1;
        }
        
        /* Ensure we don't copy more than 16 bytes */
        size_t bytes_to_copy = (prefix_len + 7) / 8;
        if (bytes_to_copy > 16) {
            bytes_to_copy = 16;
        }
        
        memcpy(prefix, data + offset, bytes_to_copy);
        offset += (prefix_len + 7) / 8;
        
        if (lpm_add(*trie, prefix, prefix_len, next_hop) != 0) {
            lpm_destroy(*trie);
            return -1;
        }
    }
    
    /* Allocate arrays for lookups */
    *addrs = malloc(num_lookups_val * 16);
    *expected_results = malloc(num_lookups_val * sizeof(uint32_t));
    if (!*addrs || !*expected_results) {
        free(*addrs);
        free(*expected_results);
        lpm_destroy(*trie);
        return -1;
    }
    
    /* Parse lookups */
    for (uint32_t i = 0; i < num_lookups_val; i++) {
        if (offset + 20 > size) {
            free(*addrs);
            free(*expected_results);
            lpm_destroy(*trie);
            return -1;
        }
        
        memcpy(*addrs + i * 16, data + offset, 16);
        (*expected_results)[i] = *(uint32_t*)(data + offset + 16);
        offset += 20;
    }
    
    *num_lookups = num_lookups_val;
    return 0;
}

/* Test trie consistency */
static int test_trie_consistency(const lpm_trie_t *trie, const uint8_t *addrs, 
                                const uint32_t *expected_results, size_t num_lookups) {
    for (size_t i = 0; i < num_lookups; i++) {
        uint32_t result = lpm_lookup(trie, addrs + i * 16);
        
        /* For fuzzing, we don't assert exact matches since input is random */
        /* Just ensure no crashes and results are reasonable */
        if (result != LPM_INVALID_NEXT_HOP && result > 0xFFFF) {
            return -1;  /* Invalid next hop value */
        }
    }
    return 0;
}

/* Test memory safety with random operations */
static int test_memory_safety(const uint8_t *data, size_t size) {
    lpm_trie_t *trie = NULL;
    uint8_t *addrs = NULL;
    uint32_t *expected_results = NULL;
    size_t num_lookups = 0;
    
    int ret = parse_fuzz_input(data, size, &trie, &addrs, &expected_results, &num_lookups);
    if (ret != 0) return ret;
    
    /* Test basic operations */
    ret = test_trie_consistency(trie, addrs, expected_results, num_lookups);
    
    /* Test lookup_all and batch operations */
    if (num_lookups > 0) {
        lpm_result_t *result = lpm_lookup_all(trie, addrs);
        if (result) {
            lpm_result_destroy(result);
        }

        const uint8_t **addr_ptrs = malloc(num_lookups * sizeof(uint8_t*));
        uint32_t *batch_results = malloc(num_lookups * sizeof(uint32_t));
        
        if (addr_ptrs && batch_results) {
            for (size_t i = 0; i < num_lookups; i++) {
                addr_ptrs[i] = addrs + i * 16;
            }
            
            lpm_lookup_batch(trie, addr_ptrs, batch_results, num_lookups);
            
            free(addr_ptrs);
            free(batch_results);
        }
    }
    
    /* Cleanup */
    if (addrs) free(addrs);
    if (expected_results) free(expected_results);
    if (trie) lpm_destroy(trie);
    
    return ret;
}

/* Test edge cases with specific patterns */
static int test_edge_cases(void) {
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    if (!trie) return -1;
    
    /* Test 1: Empty trie */
    const uint8_t addr[] = {192, 168, 0, 1};
    uint32_t result = lpm_lookup(trie, addr);
    assert(result == LPM_INVALID_NEXT_HOP);
    
    /* Test 2: Single prefix */
    const uint8_t prefix[] = {192, 168, 0, 0};
    assert(lpm_add(trie, prefix, 16, 1) == 0);
    result = lpm_lookup(trie, addr);
    assert(result == 1);
    
    /* Test 3: Overlapping prefixes */
    const uint8_t prefix2[] = {192, 168, 0, 0};
    assert(lpm_add(trie, prefix2, 24, 2) == 0);
    result = lpm_lookup(trie, addr);
    assert(result == 2);  /* More specific match */
    
    /* Test 4: Exact match */
    const uint8_t addr2[] = {192, 168, 0, 0};
    result = lpm_lookup(trie, addr2);
    assert(result == 2);
    
    /* Test 5: No match */
    const uint8_t addr3[] = {10, 0, 0, 1};
    result = lpm_lookup(trie, addr3);
    assert(result == LPM_INVALID_NEXT_HOP);
    
    lpm_destroy(trie);
    return 0;
}

/* Test IPv6 edge cases */
static int test_ipv6_edge_cases(void) {
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    if (!trie) return -1;
    
    /* Test IPv6 prefix */
    const uint8_t prefix[16] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t addr[16] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    
    assert(lpm_add(trie, prefix, 64, 1) == 0);
    uint32_t result = lpm_lookup(trie, addr);
    assert(result == 1);
    
    lpm_destroy(trie);
    return 0;
}

/* Test custom allocators with edge cases */
static int test_custom_allocator_edge_cases(void) {
    /* Test with custom allocator */
    lpm_trie_t *trie = lpm_create_custom(LPM_IPV4_MAX_DEPTH, malloc, free);
    if (!trie) return -1;
    
    /* Add many prefixes to test memory management */
    for (int i = 0; i < 100; i++) {
        const uint8_t prefix[4] = {192, 168, i, 0};
        assert(lpm_add(trie, prefix, 24, i) == 0);
    }
    
    /* Test lookups */
    for (int i = 0; i < 100; i++) {
        const uint8_t addr[4] = {192, 168, i, 1};
        uint32_t result = lpm_lookup(trie, addr);
        assert(result == i);
    }
    
    lpm_destroy(trie);
    return 0;
}

/* Main function for AFL-style fuzzing */
int main(int argc, const char *argv[]) {
    /* Test edge cases first */
    if (test_edge_cases() != 0) {
        fprintf(stderr, "Edge cases test failed\n");
        return 1;
    }
    
    if (test_ipv6_edge_cases() != 0) {
        fprintf(stderr, "IPv6 edge cases test failed\n");
        return 1;
    }
    
    if (test_custom_allocator_edge_cases() != 0) {
        fprintf(stderr, "Custom allocator edge cases test failed\n");
        return 1;
    }
    
    /* If no file input, run basic tests */
    if (argc < 2) {
        printf("Running basic fuzzing tests...\n");
        
        /* Generate some random test data */
        uint8_t test_data[1000];
        size_t data_size = 0;
        
        /* Header */
        *(uint32_t*)test_data = 10;  /* 10 prefixes */
        *((uint32_t*)test_data + 1) = 5;  /* 5 lookups */
        data_size = 8;
        
        /* Add some prefixes */
        for (int i = 0; i < 10; i++) {
            test_data[data_size++] = 24;  /* prefix length in bits */
            *(uint32_t*)(test_data + data_size) = i;  /* next hop */
            data_size += 4;
            
            const uint8_t prefix[4] = {192, 168, i, 0};
            memcpy(test_data + data_size, prefix, 3);  /* Only 3 bytes for /24 */
            data_size += 3;
        }
        
        /* Add some lookups */
        for (int i = 0; i < 5; i++) {
            const uint8_t addr[16] = {192, 168, i, 1};
            memcpy(test_data + data_size, addr, 16);
            data_size += 16;
            *(uint32_t*)(test_data + data_size) = i;
            data_size += 4;
        }
        
        if (test_memory_safety(test_data, data_size) != 0) {
            fprintf(stderr, "Memory safety test failed\n");
            return 1;
        }
        
        printf("Basic fuzzing tests passed\n");
        return 0;
    }
    
    /* Read input file for AFL-style fuzzing */
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Failed to open input file\n");
        return 1;
    }
    
    /* Read file content */
    uint8_t data[10000];
    size_t size = fread(data, 1, sizeof(data), f);
    fclose(f);
    
    if (size == 0) {
        fprintf(stderr, "Empty input file\n");
        return 1;
    }
    
    /* Test with file input */
    if (test_memory_safety(data, size) != 0) {
        fprintf(stderr, "Memory safety test failed with file input\n");
        return 1;
    }
    
    return 0;
}

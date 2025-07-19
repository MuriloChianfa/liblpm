#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "../include/lpm.h"

/* Test lookup all functionality */
static void test_lookup_all_basic(void)
{
    printf("Testing lookup_all basic functionality...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    printf("CPU features: 0x%08x\n", trie->cpu_features);
    printf("lookup_all function: %p\n", trie->lookup_all);
    
    /* Add overlapping prefixes */
    uint8_t prefix1[4] = {10, 0, 0, 0};       // 10.0.0.0/8
    uint8_t prefix2[4] = {10, 1, 0, 0};       // 10.1.0.0/16
    uint8_t prefix3[4] = {10, 1, 2, 0};       // 10.1.2.0/24
    uint8_t prefix4[4] = {10, 1, 2, 3};       // 10.1.2.3/32
    
    printf("Adding prefixes...\n");
    assert(lpm_add_prefix(trie, prefix1, 8, (void*)100) == 0);
    assert(lpm_add_prefix(trie, prefix2, 16, (void*)200) == 0);
    assert(lpm_add_prefix(trie, prefix3, 24, (void*)300) == 0);
    assert(lpm_add_prefix(trie, prefix4, 32, (void*)400) == 0);
    
    /* Debug: Check what's in the root node */
    printf("Root node valid_bitmap[0] = 0x%08x\n", trie->root->valid_bitmap[0]);
    printf("Root node prefix_info[10] = %p\n", trie->root->prefix_info[10]);
    if (trie->root->prefix_info[10]) {
        printf("  prefix_len = %u, user_data = %lu\n", 
               trie->root->prefix_info[10]->prefix_len, 
               (uintptr_t)trie->root->prefix_info[10]->user_data);
    }
    
    /* Test lookups - should return all matching prefixes */
    uint8_t test1[4] = {10, 1, 2, 3};         // Should match all 4 prefixes
    uint8_t test2[4] = {10, 1, 2, 4};         // Should match 3 prefixes (not /32)
    uint8_t test3[4] = {10, 1, 3, 1};         // Should match 2 prefixes (/8, /16)
    uint8_t test4[4] = {10, 2, 0, 0};         // Should match 1 prefix (/8)
    uint8_t test5[4] = {192, 168, 1, 1};      // Should match 0 prefixes
    
    /* Test 10.1.2.3 - should match all 4 */
    printf("Testing 10.1.2.3...\n");
    lpm_result_t *result = lpm_lookup_all(trie, test1);
    assert(result != NULL);
    printf("  10.1.2.3 matched %u prefixes (expected 4)\n", result->count);
    
    for (uint32_t i = 0; i < result->count; i++) {
        printf("    Prefix %u: length=%u, user_data=%lu\n", 
               i, result->prefixes[i].prefix_len, (uintptr_t)result->prefixes[i].user_data);
    }
    
    if (result->count != 4) {
        printf("ERROR: Expected 4 prefixes, got %u\n", result->count);
        lpm_result_destroy(result);
        lpm_destroy(trie);
        return;
    }
    
    lpm_result_destroy(result);
    
    /* Test 10.1.2.4 - should match 3 */
    printf("Testing 10.1.2.4...\n");
    result = lpm_lookup_all(trie, test2);
    assert(result != NULL);
    printf("  10.1.2.4 matched %u prefixes (expected 3)\n", result->count);
    lpm_result_destroy(result);
    
    /* Test 10.1.3.1 - should match 2 */
    printf("Testing 10.1.3.1...\n");
    result = lpm_lookup_all(trie, test3);
    assert(result != NULL);
    printf("  10.1.3.1 matched %u prefixes (expected 2)\n", result->count);
    lpm_result_destroy(result);
    
    /* Test 10.2.0.0 - should match 1 */
    printf("Testing 10.2.0.0...\n");
    result = lpm_lookup_all(trie, test4);
    assert(result != NULL);
    printf("  10.2.0.0 matched %u prefixes (expected 1)\n", result->count);
    lpm_result_destroy(result);
    
    /* Test 192.168.1.1 - should match 0 */
    printf("Testing 192.168.1.1...\n");
    result = lpm_lookup_all(trie, test5);
    assert(result != NULL);
    printf("  192.168.1.1 matched %u prefixes (expected 0)\n", result->count);
    lpm_result_destroy(result);
    
    lpm_destroy(trie);
    printf("lookup_all basic tests passed!\n\n");
}

static void test_lookup_all_ipv6(void)
{
    printf("Testing IPv6 lookup_all functionality...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add overlapping IPv6 prefixes */
    uint8_t prefix1[16] = {0x20, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 2001::/16
    uint8_t prefix2[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 2001:db8::/32
    uint8_t prefix3[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; // 2001:db8:0:1::/64
    
    assert(lpm_add_prefix(trie, prefix1, 16, (void*)100) == 0);
    assert(lpm_add_prefix(trie, prefix2, 32, (void*)200) == 0);
    assert(lpm_add_prefix(trie, prefix3, 64, (void*)300) == 0);
    
    /* Test address that matches all 3 prefixes */
    uint8_t test[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};
    
    lpm_result_t *result = lpm_lookup_all_ipv6(trie, test);
    assert(result != NULL);
    printf("  IPv6 address matched %u prefixes (expected 3)\n", result->count);
    assert(result->count == 3);
    
    for (uint32_t i = 0; i < result->count; i++) {
        printf("    Prefix %u: length=%u, user_data=%lu\n", 
               i, result->prefixes[i].prefix_len, (uintptr_t)result->prefixes[i].user_data);
    }
    
    lpm_result_destroy(result);
    lpm_destroy(trie);
    printf("IPv6 lookup_all tests passed!\n\n");
}

static void test_lookup_all_batch(void)
{
    printf("Testing batch lookup_all functionality...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add some prefixes */
    uint8_t prefix1[4] = {10, 0, 0, 0};
    uint8_t prefix2[4] = {10, 1, 0, 0};
    uint8_t prefix3[4] = {192, 168, 0, 0};
    
    assert(lpm_add_prefix(trie, prefix1, 8, (void*)100) == 0);
    assert(lpm_add_prefix(trie, prefix2, 16, (void*)200) == 0);
    assert(lpm_add_prefix(trie, prefix3, 16, (void*)300) == 0);
    
    /* Prepare batch of addresses */
    const int batch_size = 4;
    uint8_t addrs[4][4] = {
        {10, 0, 0, 1},      // Matches prefix1
        {10, 1, 0, 1},      // Matches prefix1 and prefix2
        {192, 168, 1, 1},   // Matches prefix3
        {172, 16, 0, 1}     // Matches nothing
    };
    
    const uint8_t *addr_ptrs[4];
    for (int i = 0; i < batch_size; i++) {
        addr_ptrs[i] = addrs[i];
    }
    
    lpm_result_t *results[4];
    lpm_lookup_all_batch(trie, addr_ptrs, results, batch_size);
    
    /* Verify results */
    printf("  10.0.0.1 matched %u prefixes (expected 1)\n", results[0]->count);
    printf("  10.1.0.1 matched %u prefixes (expected 2)\n", results[1]->count);
    printf("  192.168.1.1 matched %u prefixes (expected 1)\n", results[2]->count);
    printf("  172.16.0.1 matched %u prefixes (expected 0)\n", results[3]->count);
    assert(results[0]->count == 1);  // 10.0.0.1 matches /8
    assert(results[1]->count == 2);  // 10.1.0.1 matches /8 and /16
    assert(results[2]->count == 1);  // 192.168.1.1 matches /16
    assert(results[3]->count == 0);  // 172.16.0.1 matches nothing
    
    /* Clean up results */
    for (int i = 0; i < batch_size; i++) {
        lpm_result_destroy(results[i]);
    }
    
    lpm_destroy(trie);
    printf("Batch lookup_all tests passed!\n\n");
}

static void test_large_scale(void)
{
    printf("Testing large scale lookup_all...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add many overlapping prefixes */
    for (int i = 0; i < 100; i++) {
        uint8_t prefix[4] = {10, (uint8_t)(i / 10), (uint8_t)(i % 10), 0};
        
        /* Add multiple prefix lengths for each base */
        lpm_add_prefix(trie, prefix, 8, (void*)(uintptr_t)(i * 10 + 1));
        lpm_add_prefix(trie, prefix, 16, (void*)(uintptr_t)(i * 10 + 2));
        lpm_add_prefix(trie, prefix, 24, (void*)(uintptr_t)(i * 10 + 3));
    }
    
    /* Test lookup on an address with many matches */
    uint8_t test[4] = {10, 5, 5, 1};
    lpm_result_t *result = lpm_lookup_all(trie, test);
    assert(result != NULL);
    
    printf("  Address 10.5.5.1 matched %u prefixes\n", result->count);
    assert(result->count >= 3);  // Should match at least /8, /16, /24
    
    lpm_result_destroy(result);
    lpm_print_stats(trie);
    lpm_destroy(trie);
    printf("Large scale tests passed!\n\n");
}

int main(void)
{
    printf("=== LPM lookup_all Test Suite ===\n");
    printf("Library version: %s\n\n", lpm_get_version());
    
    test_lookup_all_basic();
    test_lookup_all_ipv6();
    test_lookup_all_batch();
    test_large_scale();
    
    printf("All lookup_all tests passed successfully!\n");
    return 0;
} 
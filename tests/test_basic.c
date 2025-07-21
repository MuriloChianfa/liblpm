#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "../include/lpm.h"

/* Enable verbose debugging only when DEBUG_TESTS is defined */
#ifdef DEBUG_TESTS
#define DEBUG_VERBOSE 1
#else
#define DEBUG_VERBOSE 0
#endif

#if DEBUG_VERBOSE
#define DEBUG_PRINTF(...) printf("[DEBUG] " __VA_ARGS__)
#else
#define DEBUG_PRINTF(...) do {} while(0)
#endif

/* Helper function to validate trie state */
static void validate_trie_state(lpm_trie_t *trie, const char *context) {
    DEBUG_PRINTF("=== Validating trie state: %s ===\n", context);
    if (!trie) {
        printf("ERROR: Trie is NULL in context: %s\n", context);
        abort();
    }
    DEBUG_PRINTF("Trie pointer: %p\n", (void*)trie);
    printf("Trie stats in context '%s':\n", context);
    lpm_print_stats(trie);
    DEBUG_PRINTF("=== End validation: %s ===\n", context);
}

/* Helper function to validate add operation */
static int debug_lpm_add(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop, const char *desc) {
    DEBUG_PRINTF("=== Adding prefix: %s ===\n", desc);
    DEBUG_PRINTF("Prefix: %u.%u.%u.%u/%u, Next hop: %u\n", 
                prefix[0], prefix[1], prefix[2], prefix[3], prefix_len, next_hop);
    
    validate_trie_state(trie, "before add");
    
    int result = lpm_add(trie, prefix, prefix_len, next_hop);
    DEBUG_PRINTF("lpm_add returned: %d\n", result);
    
    validate_trie_state(trie, "after add");
    
    if (result != 0) {
        printf("ERROR: lpm_add failed with result %d for %s\n", result, desc);
    }
    
    DEBUG_PRINTF("=== End adding prefix: %s ===\n", desc);
    return result;
}

/* Helper function to validate lookup operation */
static uint32_t debug_lpm_lookup(lpm_trie_t *trie, const uint8_t *addr, const char *desc) {
    DEBUG_PRINTF("=== Looking up: %s ===\n", desc);
    DEBUG_PRINTF("Address: %u.%u.%u.%u\n", addr[0], addr[1], addr[2], addr[3]);
    
    uint32_t result = lpm_lookup(trie, addr);
    DEBUG_PRINTF("Lookup result: %u\n", result);
    
    DEBUG_PRINTF("=== End lookup: %s ===\n", desc);
    return result;
}

/* Test helper functions */
static void test_ipv4_basic(void)
{
    printf("Testing IPv4 basic functionality...\n");
    
    DEBUG_PRINTF("Creating trie with max depth %d\n", LPM_IPV4_MAX_DEPTH);
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    if (!trie) {
        printf("CRITICAL ERROR: Failed to create trie\n");
        abort();
    }
    
    validate_trie_state(trie, "initial creation");
    
    /* Add some test prefixes */
    const uint8_t prefix1[4] = {192, 168, 0, 0};    // 192.168.0.0/16
    const uint8_t prefix2[4] = {192, 168, 1, 0};    // 192.168.1.0/24
    const uint8_t prefix3[4] = {10, 0, 0, 0};       // 10.0.0.0/8
    const uint8_t prefix4[4] = {172, 16, 0, 0};     // 172.16.0.0/12
    
    assert(debug_lpm_add(trie, prefix1, 16, 100, "192.168.0.0/16 -> 100") == 0);
    assert(debug_lpm_add(trie, prefix2, 24, 200, "192.168.1.0/24 -> 200") == 0);
    assert(debug_lpm_add(trie, prefix3, 8, 300, "10.0.0.0/8 -> 300") == 0);
    assert(debug_lpm_add(trie, prefix4, 12, 400, "172.16.0.0/12 -> 400") == 0);
    
    /* Test lookups */
    const uint8_t test1[4] = {192, 168, 1, 1};      // Should match 192.168.1.0/24 -> 200
    const uint8_t test2[4] = {192, 168, 2, 1};      // Should match 192.168.0.0/16 -> 100
    const uint8_t test3[4] = {10, 1, 2, 3};         // Should match 10.0.0.0/8 -> 300
    const uint8_t test4[4] = {172, 16, 5, 10};      // Should match 172.16.0.0/12 -> 400
    const uint8_t test5[4] = {8, 8, 8, 8};          // Should not match -> INVALID
    
    assert(debug_lpm_lookup(trie, test1, "192.168.1.1 -> expect 200") == 200);
    assert(debug_lpm_lookup(trie, test2, "192.168.2.1 -> expect 100") == 100);
    assert(debug_lpm_lookup(trie, test3, "10.1.2.3 -> expect 300") == 300);
    assert(debug_lpm_lookup(trie, test4, "172.16.5.10 -> expect 400") == 400);
    assert(debug_lpm_lookup(trie, test5, "8.8.8.8 -> expect INVALID") == LPM_INVALID_NEXT_HOP);
    
    /* Test IPv4 specific function */
    uint32_t addr1 = htonl((192 << 24) | (168 << 16) | (1 << 8) | 1);
    DEBUG_PRINTF("Testing lpm_lookup_ipv4 with address 0x%08x\n", ntohl(addr1));
    uint32_t ipv4_result = lpm_lookup_ipv4(trie, ntohl(addr1));
    DEBUG_PRINTF("lpm_lookup_ipv4 result: %u (expected: 200)\n", ipv4_result);
    assert(ipv4_result == 200);
    
    validate_trie_state(trie, "before destruction");
    lpm_destroy(trie);
    printf("IPv4 basic tests passed!\n\n");
}

static void test_ipv6_basic(void)
{
    printf("Testing IPv6 basic functionality...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV6_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add some test prefixes */
    const uint8_t prefix1[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 2001:db8::/32
    const uint8_t prefix2[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}; // 2001:db8:0:1::/64
    const uint8_t prefix3[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};       // fe80::/10
    
    assert(lpm_add(trie, prefix1, 32, 100) == 0);
    assert(lpm_add(trie, prefix2, 64, 200) == 0);
    assert(lpm_add(trie, prefix3, 10, 300) == 0);
    
    /* Test lookups */
    const uint8_t test1[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1}; // Should match /64 -> 200
    const uint8_t test2[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1}; // Should match /32 -> 100
    const uint8_t test3[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};       // Should match /10 -> 300
    const uint8_t test4[16] = {0x30, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};       // Should not match
    
    assert(lpm_lookup_ipv6(trie, test1) == 200);
    assert(lpm_lookup_ipv6(trie, test2) == 100);
    assert(lpm_lookup_ipv6(trie, test3) == 300);
    assert(lpm_lookup_ipv6(trie, test4) == LPM_INVALID_NEXT_HOP);
    
    lpm_print_stats(trie);
    lpm_destroy(trie);
    printf("IPv6 basic tests passed!\n\n");
}

static void test_batch_lookup(void)
{
    printf("Testing batch lookup functionality...\n");
    
    DEBUG_PRINTF("Creating trie for batch lookup test\n");
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    if (!trie) {
        printf("CRITICAL ERROR: Failed to create trie for batch lookup\n");
        abort();
    }
    
    validate_trie_state(trie, "batch test initial");
    
    /* Add prefixes */
    const uint8_t prefix1[4] = {10, 0, 0, 0};
    const uint8_t prefix2[4] = {10, 1, 0, 0};
    const uint8_t prefix3[4] = {10, 2, 0, 0};
    
    assert(debug_lpm_add(trie, prefix1, 8, 100, "10.0.0.0/8 -> 100") == 0);
    assert(debug_lpm_add(trie, prefix2, 16, 200, "10.1.0.0/16 -> 200") == 0);
    assert(debug_lpm_add(trie, prefix3, 16, 300, "10.2.0.0/16 -> 300") == 0);
    
    /* Debug: Print trie statistics after adding prefixes */
    printf("Debug: After adding prefixes:\n");
    lpm_print_stats(trie);
    
    /* Test individual lookups first */
    const uint8_t test_addr1[4] = {10, 0, 0, 1};
    const uint8_t test_addr2[4] = {10, 1, 0, 1};
    const uint8_t test_addr3[4] = {10, 2, 0, 1};
    
    printf("Debug: Individual lookup results:\n");
    printf("  10.0.0.1 -> %u\n", debug_lpm_lookup(trie, test_addr1, "10.0.0.1"));
    printf("  10.1.0.1 -> %u\n", debug_lpm_lookup(trie, test_addr2, "10.1.0.1"));
    printf("  10.2.0.1 -> %u\n", debug_lpm_lookup(trie, test_addr3, "10.2.0.1"));
    
    /* Prepare batch of addresses */
    const int batch_size = 8;
    const uint8_t addrs[8][4] = {
        {10, 0, 0, 1},    // -> 100
        {10, 1, 0, 1},    // -> 200
        {10, 2, 0, 1},    // -> 300
        {10, 3, 0, 1},    // -> 100
        {11, 0, 0, 1},    // -> INVALID
        {10, 1, 1, 1},    // -> 200
        {10, 2, 2, 2},    // -> 300
        {192, 168, 1, 1}  // -> INVALID
    };
    
    const uint8_t *addr_ptrs[8];
    for (int i = 0; i < batch_size; i++) {
        addr_ptrs[i] = addrs[i];
    }
    
    uint32_t next_hops[8];
    const uint32_t expected[8] = {100, 200, 300, 100, LPM_INVALID_NEXT_HOP, 200, 300, LPM_INVALID_NEXT_HOP};
    
    lpm_lookup_batch(trie, addr_ptrs, next_hops, batch_size);
    
    /* Print debug info */
    printf("Debug: Batch lookup results:\n");
    for (int i = 0; i < batch_size; i++) {
        printf("  [%d] expected %u, got %u\n", i, expected[i], next_hops[i]);
    }
    
    /* Verify results */
    for (int i = 0; i < batch_size; i++) {
        if (next_hops[i] != expected[i]) {
            printf("Batch lookup failed at index %d: expected %u, got %u\n", 
                   i, expected[i], next_hops[i]);
            assert(0);
        }
    }
    
    /* Test IPv4 batch lookup */
    const uint32_t ipv4_addrs[4] = {
        (10 << 24) | 1,              // 10.0.0.1
        (10 << 24) | (1 << 16) | 1,  // 10.1.0.1
        (10 << 24) | (2 << 16) | 1,  // 10.2.0.1
        (11 << 24) | 1               // 11.0.0.1
    };
    
    uint32_t ipv4_next_hops[4];
    lpm_lookup_batch_ipv4(trie, ipv4_addrs, ipv4_next_hops, 4);
    
    assert(ipv4_next_hops[0] == 100);
    assert(ipv4_next_hops[1] == 200);
    assert(ipv4_next_hops[2] == 300);
    assert(ipv4_next_hops[3] == LPM_INVALID_NEXT_HOP);
    
    lpm_destroy(trie);
    printf("Batch lookup tests passed!\n\n");
}

static void test_overlapping_prefixes(void)
{
    printf("Testing overlapping prefixes (longest match)...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add overlapping prefixes */
    const uint8_t prefix1[4] = {10, 0, 0, 0};       // 10.0.0.0/8
    const uint8_t prefix2[4] = {10, 1, 0, 0};       // 10.1.0.0/16
    const uint8_t prefix3[4] = {10, 1, 2, 0};       // 10.1.2.0/24
    const uint8_t prefix4[4] = {10, 1, 2, 3};       // 10.1.2.3/32
    
    assert(lpm_add(trie, prefix1, 8, 100) == 0);
    assert(lpm_add(trie, prefix2, 16, 200) == 0);
    assert(lpm_add(trie, prefix3, 24, 300) == 0);
    assert(lpm_add(trie, prefix4, 32, 400) == 0);
    
    /* Test longest prefix matching */
    const uint8_t test1[4] = {10, 1, 2, 3};         // Should match /32 -> 400
    const uint8_t test2[4] = {10, 1, 2, 4};         // Should match /24 -> 300
    const uint8_t test3[4] = {10, 1, 3, 1};         // Should match /16 -> 200
    const uint8_t test4[4] = {10, 2, 0, 0};         // Should match /8 -> 100
    
    assert(lpm_lookup(trie, test1) == 400);
    assert(lpm_lookup(trie, test2) == 300);
    assert(lpm_lookup(trie, test3) == 200);
    assert(lpm_lookup(trie, test4) == 100);
    
    lpm_destroy(trie);
    printf("Overlapping prefix tests passed!\n\n");
}

static void test_default_route(void)
{
    printf("Testing default route (0.0.0.0/0) handling...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add default route and specific prefixes */
    const uint8_t default_prefix[4] = {0, 0, 0, 0};  // 0.0.0.0/0 (default route)
    const uint8_t prefix1[4] = {10, 0, 0, 0};        // 10.0.0.0/8
    const uint8_t prefix2[4] = {192, 168, 0, 0};     // 192.168.0.0/16
    const uint8_t prefix3[4] = {172, 16, 0, 0};      // 172.16.0.0/12
    
    /* Test adding default route first */
    assert(lpm_add(trie, default_prefix, 0, 999) == 0);
    assert(lpm_add(trie, prefix1, 8, 100) == 0);
    assert(lpm_add(trie, prefix2, 16, 200) == 0);
    assert(lpm_add(trie, prefix3, 12, 300) == 0);
    
    /* Test lookups - specific prefixes should take precedence over default */
    const uint8_t test1[4] = {10, 1, 2, 3};         // Should match 10.0.0.0/8 -> 100
    const uint8_t test2[4] = {192, 168, 1, 1};      // Should match 192.168.0.0/16 -> 200
    const uint8_t test3[4] = {172, 16, 5, 10};      // Should match 172.16.0.0/12 -> 300
    const uint8_t test4[4] = {8, 8, 8, 8};          // Should match 0.0.0.0/0 -> 999
    const uint8_t test5[4] = {1, 1, 1, 1};          // Should match 0.0.0.0/0 -> 999
    
    /* Test individual lookups */
    uint32_t lookup1 = lpm_lookup(trie, test1);
    uint32_t lookup2 = lpm_lookup(trie, test2);
    uint32_t lookup3 = lpm_lookup(trie, test3);
    uint32_t lookup4 = lpm_lookup(trie, test4);
    uint32_t lookup5 = lpm_lookup(trie, test5);
    
    printf("Default route test results: %u, %u, %u, %u, %u\n", 
           lookup1, lookup2, lookup3, lookup4, lookup5);
    
    printf("DEBUG: About to check lookup1 == 100 (lookup1 = %u)\n", lookup1);
    
    /* Test with simple variable comparison */
    uint32_t expected = 100;
    uint32_t actual = lookup1;
    printf("DEBUG: expected = %u, actual = %u\n", expected, actual);
    
    if (actual != expected) {
        printf("ERROR: Assertion failed - expected %u, got %u\n", expected, actual);
        abort();
    }
    
    assert(lookup1 == 100);
    
    assert(lookup2 == 200);
    assert(lookup3 == 300);
    assert(lookup4 == 999);
    assert(lookup5 == 999);
    
    lpm_destroy(trie);
    
    /* Test adding default route last */
    trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    assert(lpm_add(trie, prefix1, 8, 100) == 0);
    assert(lpm_add(trie, prefix2, 16, 200) == 0);
    assert(lpm_add(trie, prefix3, 12, 300) == 0);
    assert(lpm_add(trie, default_prefix, 0, 999) == 0);
    
    /* Test lookups again */
    assert(lpm_lookup(trie, test1) == 100);
    assert(lpm_lookup(trie, test2) == 200);
    assert(lpm_lookup(trie, test3) == 300);
    assert(lpm_lookup(trie, test4) == 999);
    assert(lpm_lookup(trie, test5) == 999);
    
    lpm_destroy(trie);
    printf("Default route tests passed!\n\n");
}



int main(void)
{
    printf("=== LPM Library Test Suite ===\n");
    printf("Library version: %s\n\n", lpm_get_version());
    
    test_ipv4_basic();
    test_ipv6_basic();
    test_batch_lookup();
    test_overlapping_prefixes();
    test_default_route();
    
    printf("All tests passed successfully!\n");
    return 0;
}

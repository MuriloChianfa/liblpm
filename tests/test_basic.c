#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "../include/lpm.h"

/* Test helper functions */
static void test_ipv4_basic(void)
{
    printf("Testing IPv4 basic functionality...\n");
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add some test prefixes */
    const uint8_t prefix1[4] = {192, 168, 0, 0};    // 192.168.0.0/16
    const uint8_t prefix2[4] = {192, 168, 1, 0};    // 192.168.1.0/24
    const uint8_t prefix3[4] = {10, 0, 0, 0};       // 10.0.0.0/8
    const uint8_t prefix4[4] = {172, 16, 0, 0};     // 172.16.0.0/12
    
    assert(lpm_add(trie, prefix1, 16, 100) == 0);
    assert(lpm_add(trie, prefix2, 24, 200) == 0);
    assert(lpm_add(trie, prefix3, 8, 300) == 0);
    assert(lpm_add(trie, prefix4, 12, 400) == 0);
    
    /* Test lookups */
    const uint8_t test1[4] = {192, 168, 1, 1};      // Should match 192.168.1.0/24 -> 200
    const uint8_t test2[4] = {192, 168, 2, 1};      // Should match 192.168.0.0/16 -> 100
    const uint8_t test3[4] = {10, 1, 2, 3};         // Should match 10.0.0.0/8 -> 300
    const uint8_t test4[4] = {172, 16, 5, 10};      // Should match 172.16.0.0/12 -> 400
    const uint8_t test5[4] = {8, 8, 8, 8};          // Should not match -> INVALID
    
    assert(lpm_lookup(trie, test1) == 200);
    assert(lpm_lookup(trie, test2) == 100);
    assert(lpm_lookup(trie, test3) == 300);
    assert(lpm_lookup(trie, test4) == 400);
    assert(lpm_lookup(trie, test5) == LPM_INVALID_NEXT_HOP);
    
    /* Test IPv4 specific function */
    uint32_t addr1 = htonl((192 << 24) | (168 << 16) | (1 << 8) | 1);
    assert(lpm_lookup_ipv4(trie, ntohl(addr1)) == 200);
    
    lpm_print_stats(trie);
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
    
    lpm_trie_t *trie = lpm_create(LPM_IPV4_MAX_DEPTH);
    assert(trie != NULL);
    
    /* Add prefixes */
    const uint8_t prefix1[4] = {10, 0, 0, 0};
    const uint8_t prefix2[4] = {10, 1, 0, 0};
    const uint8_t prefix3[4] = {10, 2, 0, 0};
    
    assert(lpm_add(trie, prefix1, 8, 100) == 0);
    assert(lpm_add(trie, prefix2, 16, 200) == 0);
    assert(lpm_add(trie, prefix3, 16, 300) == 0);
    
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

static void test_cpu_features(void)
{
    printf("Testing CPU feature detection...\n");
    
    uint32_t features = lpm_detect_cpu_features();
    printf("Detected CPU features: 0x%08x\n", features);
    
    if (features & LPM_CPU_SSE) printf("  - SSE supported\n");
    if (features & LPM_CPU_SSE2) printf("  - SSE2 supported\n");
    if (features & LPM_CPU_SSE3) printf("  - SSE3 supported\n");
    if (features & LPM_CPU_SSE4_1) printf("  - SSE4.1 supported\n");
    if (features & LPM_CPU_SSE4_2) printf("  - SSE4.2 supported\n");
    if (features & LPM_CPU_AVX) printf("  - AVX supported\n");
    if (features & LPM_CPU_AVX2) printf("  - AVX2 supported\n");
    if (features & LPM_CPU_AVX512F) printf("  - AVX512F supported\n");
    if (features & LPM_CPU_AVX512VL) printf("  - AVX512VL supported\n");
    if (features & LPM_CPU_AVX512DQ) printf("  - AVX512DQ supported\n");
    if (features & LPM_CPU_AVX512BW) printf("  - AVX512BW supported\n");
    
    printf("\n");
}

int main(void)
{
    printf("=== LPM Library Test Suite ===\n");
    printf("Library version: %s\n\n", lpm_get_version());
    
    test_cpu_features();
    test_ipv4_basic();
    test_ipv6_basic();
    test_batch_lookup();
    test_overlapping_prefixes();
    
    printf("All tests passed successfully!\n");
    return 0;
} 
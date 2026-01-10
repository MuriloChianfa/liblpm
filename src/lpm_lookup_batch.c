#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef LPM_X86_ARCH
#include <immintrin.h>
#endif
#include "../include/lpm.h"

/* External ifunc-dispatched batch lookup (defined in lpm_lookup_single.c) */
extern void lpm_lookup_batch_simd(const struct lpm_trie *trie, const uint8_t **addrs, 
                                  uint32_t *next_hops, size_t count);

/* Forward declaration for DIR-24-8 lookup */
extern uint32_t lpm_lookup_ipv4_dir24(const lpm_trie_t *trie, const uint8_t addr[4]);

/* Forward declaration for pure SIMD uint32_t batch lookup (ifunc in lpm_ipv4_dir24.c) */
extern void lpm_lookup_batch_ipv4_dir24_u32(const lpm_trie_t *trie, const uint32_t *ips,
                                            uint32_t *next_hops, size_t count);

void lpm_lookup_batch_ipv4(const lpm_trie_t *trie, const uint32_t *addrs, 
                          uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    
    /* Use SIMD-optimized DIR-24-8 when enabled */
    if (trie->use_ipv4_dir24 && trie->dir24_table) {
        lpm_lookup_batch_ipv4_dir24_u32(trie, addrs, next_hops, count);
        return;
    }
    
    /* Standard path - stack allocation for small batches, heap for large */
    uint8_t stack_bytes[256 * 4];
    const uint8_t *stack_ptrs[256];
    
    uint8_t *bytes;
    const uint8_t **ptrs;
    
    if (count <= 256) {
        bytes = stack_bytes;
        ptrs = stack_ptrs;
    } else {
        bytes = (uint8_t*)malloc(count * 4);
        ptrs = (const uint8_t**)malloc(count * sizeof(uint8_t*));
        if (!bytes || !ptrs) {
            free(bytes);
            free(ptrs);
            return;
        }
    }
    
    /* Convert to byte arrays */
    for (size_t i = 0; i < count; i++) {
        uint8_t *b = &bytes[i * 4];
        uint32_t a = addrs[i];
        b[0] = (a >> 24) & 0xFF;
        b[1] = (a >> 16) & 0xFF;
        b[2] = (a >> 8) & 0xFF;
        b[3] = a & 0xFF;
        ptrs[i] = b;
    }
    
    lpm_lookup_batch_simd(trie, ptrs, next_hops, count);
    
    if (count > 256) {
        free(bytes);
        free(ptrs);
    }
}

void lpm_lookup_batch_ipv6(const lpm_trie_t *trie, const uint8_t (*addrs)[16], 
                          uint32_t *next_hops, size_t count)
{
    if (!trie || !addrs || !next_hops || count == 0) return;
    
    const uint8_t *stack_ptrs[256];
    const uint8_t **ptrs;
    
    if (count <= 256) {
        ptrs = stack_ptrs;
    } else {
        ptrs = (const uint8_t**)malloc(count * sizeof(uint8_t*));
        if (!ptrs) return;
    }
    
    for (size_t i = 0; i < count; i++) {
        ptrs[i] = addrs[i];
    }
    
    lpm_lookup_batch_simd(trie, ptrs, next_hops, count);
    
    if (count > 256) {
        free(ptrs);
    }
}

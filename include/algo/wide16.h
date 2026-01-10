/*
 * liblpm - IPv6 Wide 16-bit Stride Algorithm
 * Internal declarations
 */
#ifndef _LPM_ALGO_WIDE16_H_
#define _LPM_ALGO_WIDE16_H_

#include "../lpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * IPv6 Wide 16-bit Stride Algorithm (wide16)
 * ============================================================================ */

/* Creation and management */
lpm_trie_t *lpm_create_ipv6_wide16(void);
int lpm_add_ipv6_wide16(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len, uint32_t next_hop);
int lpm_delete_ipv6_wide16(lpm_trie_t *trie, const uint8_t *prefix, uint8_t prefix_len);

/* Single lookup */
uint32_t lpm_lookup_ipv6_wide16(const lpm_trie_t *trie, const uint8_t addr[16]);

/* Batch lookup - ifunc dispatched */
void lpm_lookup_batch_ipv6_wide16(const lpm_trie_t *trie, const uint8_t (*addrs)[16],
                                   uint32_t *next_hops, size_t count);

/* Internal SIMD variants */
uint32_t lpm_lookup_ipv6_wide16_scalar(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_wide16_sse2(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_wide16_avx2(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_wide16_avx512(const lpm_trie_t *trie, const uint8_t *addr);

void lpm_lookup_batch_ipv6_wide16_scalar(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_wide16_sse2(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_wide16_avx2(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_wide16_avx512(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* _LPM_ALGO_WIDE16_H_ */

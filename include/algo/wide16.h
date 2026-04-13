/*
 * liblpm - IPv6 Wide 16-bit Stride Algorithm
 * Internal declarations
 */
#ifndef LPM_ALGO_WIDE16_H_
#define LPM_ALGO_WIDE16_H_

#include "../lpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Internal SIMD variants (used by ifunc resolver)
 * Public API functions are declared in lpm.h
 * ============================================================================ */

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

#endif /* LPM_ALGO_WIDE16_H_ */

/*
 * liblpm - IPv6 8-bit Stride Algorithm (6stride8)
 * Internal declarations
 */
#ifndef LPM_ALGO_6STRIDE8_H_
#define LPM_ALGO_6STRIDE8_H_

#include "../lpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Internal SIMD variants (used by ifunc resolver)
 * Public API functions are declared in lpm.h
 * ============================================================================ */

uint32_t lpm_lookup_ipv6_8stride_scalar(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_sse2(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_sse42(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_avx(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_avx2(const lpm_trie_t *trie, const uint8_t *addr);
uint32_t lpm_lookup_ipv6_8stride_avx512(const lpm_trie_t *trie, const uint8_t *addr);

void lpm_lookup_batch_ipv6_8stride_scalar(const lpm_trie_t *trie, const uint8_t **addrs,
                                           uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_sse2(const lpm_trie_t *trie, const uint8_t **addrs,
                                         uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_sse42(const lpm_trie_t *trie, const uint8_t **addrs,
                                          uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_avx(const lpm_trie_t *trie, const uint8_t **addrs,
                                        uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_avx2(const lpm_trie_t *trie, const uint8_t **addrs,
                                         uint32_t *next_hops, size_t count);
void lpm_lookup_batch_ipv6_8stride_avx512(const lpm_trie_t *trie, const uint8_t **addrs,
                                           uint32_t *next_hops, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* LPM_ALGO_6STRIDE8_H_ */
